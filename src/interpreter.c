// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-2025 Grisshink
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

void arg_stack_push_arg(Exec* exec, AnyValue data);
void arg_stack_undo_args(Exec* exec, size_t count);
void variable_stack_pop_layer(Exec* exec);
void variable_stack_cleanup(Exec* exec);
void chain_stack_push(Exec* exec, ChainStackData data);
void chain_stack_pop(Exec* exec);
bool exec_block(Exec* exec, Block* block, AnyValue* block_return, ControlState control_state, AnyValue control_arg);

int data_to_integer(AnyValue arg) {
    switch (arg.type) {
    case DATA_TYPE_BOOL:
    case DATA_TYPE_INTEGER:
        return arg.data.integer_val;
    case DATA_TYPE_FLOAT:
        return (int)arg.data.float_val;
    case DATA_TYPE_LITERAL:
        return atoi(arg.data.literal_val);
    case DATA_TYPE_STRING:
        return atoi(arg.data.str_val->str);
    default:
        return 0;
    }
}

double data_to_float(AnyValue arg) {
    switch (arg.type) {
    case DATA_TYPE_BOOL:
    case DATA_TYPE_INTEGER:
        return (double)arg.data.integer_val;
    case DATA_TYPE_FLOAT:
        return arg.data.float_val;
    case DATA_TYPE_LITERAL:
        return atof(arg.data.literal_val);
    case DATA_TYPE_STRING:
        return atof(arg.data.str_val->str);
    default:
        return 0.0;
    }
}

int data_to_bool(AnyValue arg) {
    switch (arg.type) {
    case DATA_TYPE_BOOL:
    case DATA_TYPE_INTEGER:
        return arg.data.integer_val != 0;
    case DATA_TYPE_FLOAT:
        return arg.data.float_val != 0.0;
    case DATA_TYPE_LITERAL:
        return *arg.data.literal_val != 0;
    case DATA_TYPE_STRING:
        return *arg.data.str_val->str != 0;
    case DATA_TYPE_LIST:
        return arg.data.list_val->size != 0;
    default:
        return 0;
    }
}

char* data_to_any_string(Exec* exec, AnyValue arg) {
    if (arg.type == DATA_TYPE_LITERAL) return arg.data.literal_val;
    return std_string_from_any(&exec->gc, &arg)->str;
}

void define_function(Exec* exec, Blockdef* blockdef, BlockChain* chain) {
    DefineFunction* func = vector_add_dst(&exec->defined_functions);
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

Exec exec_new(Thread* thread) {
    Exec exec = (Exec) {
        .code = NULL,
        .arg_stack_len = 0,
        .control_stack_len = 0,
        .thread = thread,
        .current_error_block = NULL,
    };
    exec.current_error[0] = 0;
    return exec;
}

void exec_free(Exec* exec) {
    (void) exec;
}

bool exec_run(void* e) {
    Exec* exec = e;
    exec->arg_stack_len = 0;
    exec->control_stack_len = 0;
    exec->chain_stack_len = 0;
    exec->running_chain = NULL;
    exec->defined_functions = vector_create();
    exec->gc = gc_new(MIN_MEMORY_LIMIT, MAX_MEMORY_LIMIT);

    SetRandomSeed(time(NULL));

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        Block* block = &exec->code[i].blocks[0];
        if (strcmp(block->blockdef->id, "define_block")) continue;

        for (size_t j = 0; j < vector_size(block->arguments); j++) {
            if (block->arguments[j].type != ARGUMENT_BLOCKDEF) continue;
            define_function(exec, block->arguments[j].data.blockdef, &exec->code[i]);
        }
    }

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        Block* block = &exec->code[i].blocks[0];
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
        if (!exec_run_chain(exec, &exec->code[i], -1, NULL, &bin)) {
            exec->running_chain = NULL;
            return false;
        }
        exec->running_chain = NULL;
    }

    return true;
}

void exec_cleanup(void* e) {
    Exec* exec = e;
    for (size_t i = 0; i < vector_size(exec->defined_functions); i++) {
        vector_free(exec->defined_functions[i].args);
    }
    vector_free(exec->defined_functions);
    variable_stack_cleanup(exec);
    arg_stack_undo_args(exec, exec->arg_stack_len);
    gc_free(&exec->gc);
}

void exec_set_error(Exec* exec, Block* block, const char* fmt, ...) {
    exec->current_error_block = block;
    va_list va;
    va_start(va, fmt);
    vsnprintf(exec->current_error, MAX_ERROR_LEN, fmt, va);
    va_end(va);
    TraceLog(LOG_ERROR, "[EXEC] %s", exec->current_error);
}

bool evaluate_argument(Exec* exec, Argument* arg, AnyValue* return_val) {
    switch (arg->type) {
    case ARGUMENT_TEXT:
    case ARGUMENT_CONST_STRING:
        *return_val = DATA_LITERAL(arg->data.text);
        return true;
    case ARGUMENT_BLOCK:
        if (!exec_block(exec, &arg->data.block, return_val, CONTROL_STATE_NORMAL, (AnyValue) {0})) {
            return false;
        }
        return true;
    case ARGUMENT_BLOCKDEF:
        return true;
    }
    return false;
}

bool exec_block(Exec* exec, Block* block, AnyValue* block_return, ControlState control_state, AnyValue control_arg) {
    if (!block->blockdef) {
        exec_set_error(exec, block, gettext("Tried to execute block without definition"));
        return false;
    }
    if (!block->blockdef->func) {
        exec_set_error(exec, block, gettext("Tried to execute block \"%s\" without implementation"), block->blockdef->id);
        return false;
    }

    BlockFunc execute_block = block->blockdef->func;

    int stack_begin = exec->arg_stack_len;

    if (block->blockdef->type == BLOCKTYPE_CONTROLEND && control_state == CONTROL_STATE_BEGIN) {
        arg_stack_push_arg(exec, control_arg);
    }

    size_t last_temps = vector_size(exec->gc.root_temp_chunks);

    if (control_state != CONTROL_STATE_END) {
        for (vec_size_t i = 0; i < vector_size(block->arguments); i++) {
            AnyValue arg;
            if (!evaluate_argument(exec, &block->arguments[i], &arg)) {
                TraceLog(LOG_ERROR, "[VM] From block id: \"%s\" (at block %p)", block->blockdef->id, &block);
                return false;
            }
            arg_stack_push_arg(exec, arg);
        }
    }

    if (!execute_block(exec, block, exec->arg_stack_len - stack_begin, exec->arg_stack + stack_begin, block_return, control_state)) {
        TraceLog(LOG_ERROR, "[VM] Error from block id: \"%s\" (at block %p)", block->blockdef->id, &block);
        return false;
    }

    arg_stack_undo_args(exec, exec->arg_stack_len - stack_begin);

    if (!block->parent && vector_size(exec->gc.root_temp_chunks) > last_temps) gc_flush(&exec->gc);

    return true;
}

#define BLOCKDEF chain->blocks[i].blockdef
bool exec_run_chain(Exec* exec, BlockChain* chain, int argc, AnyValue* argv, AnyValue* return_val) {
    int skip_layer = -1;
    size_t base_len = exec->control_stack_len;
    chain_stack_push(exec, (ChainStackData) {
        .skip_block = false,
        .layer = 0,
        .running_ind = 0,
        .custom_argc = argc,
        .custom_argv = argv,
        .is_returning = false,
        .return_arg = (AnyValue) {0},
    });

    gc_root_begin(&exec->gc);
    gc_root_save(&exec->gc);

    exec->running_chain = chain;
    AnyValue block_return;
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        thread_handle_stopping_state(exec->thread);

        size_t block_ind = i;
        ChainStackData* chain_data = &exec->chain_stack[exec->chain_stack_len - 1];
        chain_data->running_ind = i;
        ControlState control_state = BLOCKDEF->type == BLOCKTYPE_CONTROL ? CONTROL_STATE_BEGIN : CONTROL_STATE_NORMAL;
        if (chain_data->is_returning) break;

        if (BLOCKDEF->type == BLOCKTYPE_END || BLOCKDEF->type == BLOCKTYPE_CONTROLEND) {
            if (BLOCKDEF->type == BLOCKTYPE_CONTROLEND && chain_data->layer == 0) continue;
            variable_stack_pop_layer(exec);
            chain_data->layer--;
            control_stack_pop_data(block_ind, size_t)
            control_stack_pop_data(block_return, AnyValue)
            gc_root_end(&exec->gc);
            control_state = CONTROL_STATE_END;
            if (chain_data->skip_block && skip_layer == chain_data->layer) {
                chain_data->skip_block = false;
                skip_layer = -1;
            }
        }
        
        if (!chain_data->skip_block) {
            if (!exec_block(exec, &chain->blocks[block_ind], &block_return, control_state, (AnyValue){0})) {
                chain_stack_pop(exec);
                return false;
            }
            exec->running_chain = chain;
            if (chain_data->running_ind != i) i = chain_data->running_ind;
        }

        if (BLOCKDEF->type == BLOCKTYPE_CONTROLEND && block_ind != i) {
            control_state = CONTROL_STATE_BEGIN;
            if (!exec_block(exec, &chain->blocks[i], &block_return, control_state, block_return)) {
                chain_stack_pop(exec);
                return false;
            }
            if (chain_data->running_ind != i) i = chain_data->running_ind;
        }

        if (BLOCKDEF->type == BLOCKTYPE_CONTROL || BLOCKDEF->type == BLOCKTYPE_CONTROLEND) {
            control_stack_push_data(block_return, AnyValue)
            control_stack_push_data(i, size_t)
            gc_root_begin(&exec->gc);
            if (chain_data->skip_block && skip_layer == -1) skip_layer = chain_data->layer;
            chain_data->layer++;
        }
    }
    gc_root_restore(&exec->gc);
    gc_root_end(&exec->gc);

    *return_val = exec->chain_stack[exec->chain_stack_len - 1].return_arg;
    while (exec->chain_stack[exec->chain_stack_len - 1].layer >= 0) {
        variable_stack_pop_layer(exec);
        exec->chain_stack[exec->chain_stack_len - 1].layer--;
    }
    exec->control_stack_len = base_len;
    chain_stack_pop(exec);
    return true;
}
#undef BLOCKDEF

void exec_set_skip_block(Exec* exec) {
    exec->chain_stack[exec->chain_stack_len - 1].skip_block = true;
}

Variable* variable_stack_push_var(Exec* exec, const char* name, AnyValue arg) {
    if (exec->variable_stack_len >= VM_VARIABLE_STACK_SIZE) {
        TraceLog(LOG_ERROR, "[VM] Variable stack overflow");
        thread_exit(exec->thread, false);
    }
    if (*name == 0) return NULL;
    Variable var;
    var.name = name;
    var.chunk_header.marked = 0;
    var.chunk_header.data_type = DATA_TYPE_ANY;
    var.value_ptr = &exec->variable_stack[exec->variable_stack_len].value;
    var.value = arg;
    var.chain_layer = exec->chain_stack_len - 1;
    var.layer = exec->chain_stack[var.chain_layer].layer;
    exec->variable_stack[exec->variable_stack_len++] = var;
    return &exec->variable_stack[exec->variable_stack_len - 1];
}

void variable_stack_pop_layer(Exec* exec) {
    size_t count = 0;
    for (int i = exec->variable_stack_len - 1; i >= 0 &&
                                               exec->variable_stack[i].layer == exec->chain_stack[exec->chain_stack_len - 1].layer &&
                                               exec->variable_stack[i].chain_layer == exec->chain_stack_len - 1; i--) {
        count++;
    }
    exec->variable_stack_len -= count;
}

void variable_stack_cleanup(Exec* exec) {
    exec->variable_stack_len = 0;
}

Variable* variable_stack_get_variable(Exec* exec, const char* name) {
    for (int i = exec->variable_stack_len - 1; i >= 0; i--) {
        if (exec->variable_stack[i].chain_layer != exec->chain_stack_len - 1) break;
        if (!strcmp(exec->variable_stack[i].name, name)) return &exec->variable_stack[i];
    }
    if (exec->chain_stack_len > 0) {
        for (size_t i = 0; i < exec->variable_stack_len; i++) {
            if (exec->variable_stack[i].layer != 0 || exec->variable_stack[i].chain_layer != 0) break;
            if (!strcmp(exec->variable_stack[i].name, name)) return &exec->variable_stack[i];
        }
    }
    return NULL;
}

void chain_stack_push(Exec* exec, ChainStackData data) {
    if (exec->chain_stack_len >= VM_CHAIN_STACK_SIZE) {
        TraceLog(LOG_ERROR, "[VM] Chain stack overflow");
        thread_exit(exec->thread, false);
    }
    exec->chain_stack[exec->chain_stack_len++] = data;
}

void chain_stack_pop(Exec* exec) {
    if (exec->chain_stack_len == 0) {
        TraceLog(LOG_ERROR, "[VM] Chain stack underflow");
        thread_exit(exec->thread, false);
    }
    exec->chain_stack_len--;
}

void arg_stack_push_arg(Exec* exec, AnyValue arg) {
    if (exec->arg_stack_len >= VM_ARG_STACK_SIZE) {
        TraceLog(LOG_ERROR, "[VM] Arg stack overflow");
        thread_exit(exec->thread, false);
    }
    exec->arg_stack[exec->arg_stack_len++] = arg;
}

void arg_stack_undo_args(Exec* exec, size_t count) {
    if (count > exec->arg_stack_len) {
        TraceLog(LOG_ERROR, "[VM] Arg stack underflow");
        thread_exit(exec->thread, false);
    }
    exec->arg_stack_len -= count;
}
