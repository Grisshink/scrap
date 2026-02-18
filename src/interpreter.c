// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-2026 Grisshink
// 
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "scrap.h"
#include "ast.h"
#include "vec.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libintl.h>

void arg_stack_push_arg(Compiler* compiler, AnyValue data);
void arg_stack_undo_args(Compiler* compiler, size_t count);
void variable_stack_pop_layer(Compiler* compiler);
void variable_stack_cleanup(Compiler* compiler);
void chain_stack_push(Compiler* compiler, ChainStackData data);
void chain_stack_pop(Compiler* compiler);
bool compiler_evaluate_block(Compiler* compiler, Block* block, AnyValue* block_return, ControlState control_state, AnyValue control_arg);

void define_function(Compiler* compiler, Blockdef* blockdef, BlockChain* chain) {
    DefineFunction* func = vector_add_dst(&compiler->defined_functions);
    func->blockdef = blockdef;
    func->run_chain = chain;
    func->args = vector_create();

    int arg_ind = 0;
    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;

        DefineArgument* arg = vector_add_dst(&func->args);
        arg->blockdef = blockdef->inputs[i].data.arg.blockdef;
        arg->arg_ind = arg_ind++;
    }
}

Compiler compiler_new(Thread* thread) {
    Compiler compiler = (Compiler) {
        .code = NULL,
        .arg_stack_len = 0,
        .control_stack_len = 0,
        .thread = thread,
        .current_error_block = NULL,
    };
    compiler.current_error[0] = 0;
    return compiler;
}

void compiler_free(Compiler* compiler) {
    (void) compiler;
}

bool compiler_run(void* e) {
    Compiler* compiler = e;
    compiler->arg_stack_len = 0;
    compiler->control_stack_len = 0;
    compiler->chain_stack_len = 0;
    compiler->running_chain = NULL;
    compiler->defined_functions = vector_create();
    compiler->gc = gc_new(MIN_MEMORY_LIMIT, MAX_MEMORY_LIMIT);

    SetRandomSeed(time(NULL));

    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        Block* block = &compiler->code[i].blocks[0];
        if (strcmp(block->blockdef->id, "define_block")) continue;

        for (size_t j = 0; j < vector_size(block->arguments); j++) {
            if (block->arguments[j].type != ARGUMENT_BLOCKDEF) continue;
            define_function(compiler, block->arguments[j].data.blockdef, &compiler->code[i]);
        }
    }

    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        Block* block = &compiler->code[i].blocks[0];
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;
        bool cont = false;
        for (size_t j = 0; j < vector_size(block->arguments); j++) {
            if (block->arguments[j].type == ARGUMENT_BLOCKDEF) {
                cont = true;
                break;
            }
        }
        if (cont) continue;
        AnyValue bin;
        if (!compiler_evaluate_chain(compiler, &compiler->code[i], -1, NULL, &bin)) {
            compiler->running_chain = NULL;
            return false;
        }
        compiler->running_chain = NULL;
    }

    return true;
}

void compiler_cleanup(void* e) {
    Compiler* compiler = e;
    for (size_t i = 0; i < vector_size(compiler->defined_functions); i++) {
        vector_free(compiler->defined_functions[i].args);
    }
    vector_free(compiler->defined_functions);
    variable_stack_cleanup(compiler);
    arg_stack_undo_args(compiler, compiler->arg_stack_len);
    gc_free(&compiler->gc);
}

void compiler_set_error(Compiler* compiler, Block* block, const char* fmt, ...) {
    compiler->current_error_block = block;
    va_list va;
    va_start(va, fmt);
    vsnprintf(compiler->current_error, MAX_ERROR_LEN, fmt, va);
    va_end(va);
    scrap_log(LOG_ERROR, "[EXEC] %s", compiler->current_error);
}

bool compiler_evaluate_argument(Compiler* compiler, Argument* arg, AnyValue* return_val) {
    switch (arg->type) {
    case ARGUMENT_TEXT:
    case ARGUMENT_CONST_STRING:
        *return_val = DATA_LITERAL(arg->data.text);
        return true;
    case ARGUMENT_BLOCK:
        if (!compiler_evaluate_block(compiler, &arg->data.block, return_val, CONTROL_STATE_NORMAL, (AnyValue) {0})) {
            return false;
        }
        return true;
    case ARGUMENT_BLOCKDEF:
        return true;
    case ARGUMENT_COLOR:
        *return_val = DATA_COLOR(CONVERT_COLOR(arg->data.color, StdColor));
        return true;
    }
    return false;
}

bool compiler_evaluate_block(Compiler* compiler, Block* block, AnyValue* block_return, ControlState control_state, AnyValue control_arg) {
    if (!block->blockdef) {
        compiler_set_error(compiler, block, gettext("Tried to execute block without definition"));
        return false;
    }
    if (!block->blockdef->func) {
        compiler_set_error(compiler, block, gettext("Tried to execute block \"%s\" without implementation"), block->blockdef->id);
        return false;
    }

    BlockFunc execute_block = block->blockdef->func;

    int stack_begin = compiler->arg_stack_len;

    if (block->blockdef->type == BLOCKTYPE_CONTROLEND && control_state == CONTROL_STATE_BEGIN) {
        arg_stack_push_arg(compiler, control_arg);
    }

    size_t last_temps = vector_size(compiler->gc.root_temp_chunks);

    if (control_state != CONTROL_STATE_END) {
        for (vec_size_t i = 0; i < vector_size(block->arguments); i++) {
            AnyValue arg;
            if (!compiler_evaluate_argument(compiler, &block->arguments[i], &arg)) {
                scrap_log(LOG_ERROR, "[VM] From block id: \"%s\" (at block %p)", block->blockdef->id, &block);
                return false;
            }
            arg_stack_push_arg(compiler, arg);
        }
    }

    if (!execute_block(compiler, block, compiler->arg_stack_len - stack_begin, compiler->arg_stack + stack_begin, block_return, control_state)) {
        scrap_log(LOG_ERROR, "[VM] Error from block id: \"%s\" (at block %p)", block->blockdef->id, &block);
        return false;
    }

    arg_stack_undo_args(compiler, compiler->arg_stack_len - stack_begin);

    if (!block->parent && vector_size(compiler->gc.root_temp_chunks) > last_temps) gc_flush(&compiler->gc);

    return true;
}

#define BLOCKDEF chain->blocks[i].blockdef
bool compiler_evaluate_chain(Compiler* compiler, BlockChain* chain, int argc, AnyValue* argv, AnyValue* return_val) {
    size_t base_len = compiler->control_stack_len;
    chain_stack_push(compiler, (ChainStackData) {
        .skip_block = false,
        .layer = 0,
        .running_ind = 0,
        .custom_argc = argc,
        .custom_argv = argv,
        .is_returning = false,
        .return_arg = (AnyValue) {0},
    });

    gc_root_begin(&compiler->gc);
    gc_root_save(&compiler->gc);

    compiler->running_chain = chain;
    AnyValue block_return;
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        ChainStackData* chain_data = &compiler->chain_stack[compiler->chain_stack_len - 1];

        if (chain_data->skip_block) {
            int layer = chain_data->layer;
            while (i < vector_size(chain->blocks)) {
                if (BLOCKDEF->type == BLOCKTYPE_END || BLOCKDEF->type == BLOCKTYPE_CONTROLEND) {
                    layer--;
                    if (layer < chain_data->layer) break;
                }
                if (BLOCKDEF->type == BLOCKTYPE_CONTROL || BLOCKDEF->type == BLOCKTYPE_CONTROLEND) {
                    layer++;
                }
                i++;
            }
            chain_data->skip_block = false;
        }

        thread_handle_stopping_state(compiler->thread);

        size_t block_ind = i;
        chain_data->running_ind = i;
        ControlState control_state = BLOCKDEF->type == BLOCKTYPE_CONTROL ? CONTROL_STATE_BEGIN : CONTROL_STATE_NORMAL;
        if (chain_data->is_returning) break;

        if (BLOCKDEF->type == BLOCKTYPE_END || BLOCKDEF->type == BLOCKTYPE_CONTROLEND) {
            if (chain_data->layer == 0) continue;
            variable_stack_pop_layer(compiler);
            chain_data->layer--;
            control_stack_pop_data(block_ind, size_t);
            control_stack_pop_data(block_return, AnyValue);
            gc_root_end(&compiler->gc);
            control_state = CONTROL_STATE_END;
        }
        
        if (!compiler_evaluate_block(compiler, &chain->blocks[block_ind], &block_return, control_state, (AnyValue){0})) {
            chain_stack_pop(compiler);
            return false;
        }
        compiler->running_chain = chain;
        if (chain_data->running_ind != i) i = chain_data->running_ind;

        if (BLOCKDEF->type == BLOCKTYPE_CONTROLEND && block_ind != i) {
            control_state = CONTROL_STATE_BEGIN;
            if (!compiler_evaluate_block(compiler, &chain->blocks[i], &block_return, control_state, block_return)) {
                chain_stack_pop(compiler);
                return false;
            }
            if (chain_data->running_ind != i) i = chain_data->running_ind;
        }

        if (BLOCKDEF->type == BLOCKTYPE_CONTROL || BLOCKDEF->type == BLOCKTYPE_CONTROLEND) {
            control_stack_push_data(block_return, AnyValue);
            control_stack_push_data(i, size_t);
            gc_root_begin(&compiler->gc);
            chain_data->layer++;
        }
    }
    gc_root_restore(&compiler->gc);
    gc_root_end(&compiler->gc);

    *return_val = compiler->chain_stack[compiler->chain_stack_len - 1].return_arg;
    while (compiler->chain_stack[compiler->chain_stack_len - 1].layer >= 0) {
        variable_stack_pop_layer(compiler);
        compiler->chain_stack[compiler->chain_stack_len - 1].layer--;
    }
    compiler->control_stack_len = base_len;
    chain_stack_pop(compiler);
    return true;
}
#undef BLOCKDEF

void compiler_set_skip_block(Compiler* compiler) {
    compiler->chain_stack[compiler->chain_stack_len - 1].skip_block = true;
}

Variable* variable_stack_push_var(Compiler* compiler, const char* name, AnyValue arg) {
    if (compiler->variable_stack_len >= VM_VARIABLE_STACK_SIZE) {
        scrap_log(LOG_ERROR, "[VM] Variable stack overflow");
        thread_exit(compiler->thread, false);
    }
    if (*name == 0) return NULL;
    Variable var;
    var.name = name;
    var.chunk_header.marked = 0;
    var.chunk_header.data_type = DATA_TYPE_ANY;
    var.value_ptr = &compiler->variable_stack[compiler->variable_stack_len].value;
    var.value = arg;
    var.chain_layer = compiler->chain_stack_len - 1;
    var.layer = compiler->chain_stack[var.chain_layer].layer;
    compiler->variable_stack[compiler->variable_stack_len++] = var;
    return &compiler->variable_stack[compiler->variable_stack_len - 1];
}

void variable_stack_pop_layer(Compiler* compiler) {
    size_t count = 0;
    for (int i = compiler->variable_stack_len - 1; i >= 0 &&
                                               compiler->variable_stack[i].layer == compiler->chain_stack[compiler->chain_stack_len - 1].layer &&
                                               compiler->variable_stack[i].chain_layer == compiler->chain_stack_len - 1; i--) {
        count++;
    }
    compiler->variable_stack_len -= count;
}

void variable_stack_cleanup(Compiler* compiler) {
    compiler->variable_stack_len = 0;
}

Variable* variable_stack_get_variable(Compiler* compiler, const char* name) {
    for (int i = compiler->variable_stack_len - 1; i >= 0; i--) {
        if (compiler->variable_stack[i].chain_layer != compiler->chain_stack_len - 1) break;
        if (!strcmp(compiler->variable_stack[i].name, name)) return &compiler->variable_stack[i];
    }
    if (compiler->chain_stack_len > 0) {
        for (size_t i = 0; i < compiler->variable_stack_len; i++) {
            if (compiler->variable_stack[i].layer != 0 || compiler->variable_stack[i].chain_layer != 0) break;
            if (!strcmp(compiler->variable_stack[i].name, name)) return &compiler->variable_stack[i];
        }
    }
    return NULL;
}

void chain_stack_push(Compiler* compiler, ChainStackData data) {
    if (compiler->chain_stack_len >= VM_CHAIN_STACK_SIZE) {
        scrap_log(LOG_ERROR, "[VM] Chain stack overflow");
        thread_exit(compiler->thread, false);
    }
    compiler->chain_stack[compiler->chain_stack_len++] = data;
}

void chain_stack_pop(Compiler* compiler) {
    if (compiler->chain_stack_len == 0) {
        scrap_log(LOG_ERROR, "[VM] Chain stack underflow");
        thread_exit(compiler->thread, false);
    }
    compiler->chain_stack_len--;
}

void arg_stack_push_arg(Compiler* compiler, AnyValue arg) {
    if (compiler->arg_stack_len >= VM_ARG_STACK_SIZE) {
        scrap_log(LOG_ERROR, "[VM] Arg stack overflow");
        thread_exit(compiler->thread, false);
    }
    compiler->arg_stack[compiler->arg_stack_len++] = arg;
}

void arg_stack_undo_args(Compiler* compiler, size_t count) {
    if (count > compiler->arg_stack_len) {
        scrap_log(LOG_ERROR, "[VM] Arg stack underflow");
        thread_exit(compiler->thread, false);
    }
    compiler->arg_stack_len -= count;
}
