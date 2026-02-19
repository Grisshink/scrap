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

#define SCRAP_IR_IMPLEMENTATION
#include "scrap_ir.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libintl.h>

Compiler compiler_new(Thread* thread) {
    Compiler compiler = (Compiler) {
        .code = NULL,
        .thread = thread,
        .current_error_block = NULL,
    };
    compiler.current_error[0] = 0;
    return compiler;
}

void compiler_free(Compiler* compiler) {
    (void) compiler;
}

bool print_str(IrExec* exec) {
    IrList* list = exec_pop_list(exec);
    if (!list) return false;
    for (size_t i = 0; i < list->size; i++) {
        IrValue c = list->items[i];
        switch (c.type) {
        case IR_TYPE_INT: printf("%lc", c.as.int_val); break;
        case IR_TYPE_BYTE: printf("%c", c.as.byte_val); break;
        default: printf("?"); break;
        }
    }
    printf("\n");
    return true;
}

IrRunFunction resolve_function(IrExec* exec, const char* hint) {
    (void) exec;
    TraceLog(LOG_INFO, "[EXEC] Resolve: %s", hint);
    if (!strcmp(hint, "print_str")) {
        return print_str;
    }
    return NULL;
}

bool compiler_run(void* e) {
    Compiler* compiler = e;
    compiler->bytecode = bytecode_new("main");

    bytecode_push_label(&compiler->bytecode, "entry");

    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        Block* block = &compiler->code[i].blocks[0];
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;
        if (!strcmp(block->blockdef->id, "define_block")) continue;

        if (!compiler_evaluate_chain(compiler, &compiler->code[i])) {
            return false;
        }
    }

    bytecode_push_op(&compiler->bytecode, IR_RET);
    bytecode_print(&compiler->bytecode);

    compiler->exec = exec_new(1024 * 1024); // 1 MB
    if (compiler->exec.last_error[0] != 0) {
        compiler_set_error(compiler, NULL, "Exec create error: %s", compiler->exec.last_error);
        exec_free(&compiler->exec);
        return false;
    }
    exec_set_run_function_resolver(&compiler->exec, resolve_function);
    exec_add_bytecode(&compiler->exec, compiler->bytecode);
    compiler->exec_running = true;

    if (!exec_run(&compiler->exec, "main", "entry")) {
        compiler_set_error(compiler, NULL, "Runtime error: %s", compiler->exec.last_error);
        return false;
    }

    return true;
}

void compiler_cleanup(void* e) {
    Compiler* compiler = e;
    if (compiler->exec_running) {
        exec_free(&compiler->exec);
    } else {
        bytecode_free(&compiler->bytecode);
    }
}

void compiler_set_error(Compiler* compiler, Block* block, const char* fmt, ...) {
    compiler->current_error_block = block;
    va_list va;
    va_start(va, fmt);
    vsnprintf(compiler->current_error, MAX_ERROR_LEN, fmt, va);
    va_end(va);
    scrap_log(LOG_ERROR, "[COMPILER] %s", compiler->current_error);
}

bool compiler_evaluate_argument(Compiler* compiler, Argument* arg, AnyValue* return_val) {
    switch (arg->type) {
    case ARGUMENT_TEXT:
    case ARGUMENT_CONST_STRING:
        *return_val = DATA_LITERAL(arg->data.text);
        return true;
    case ARGUMENT_BLOCK:
        if (!compiler_evaluate_block(compiler, &arg->data.block, return_val)) return false;
        return true;
    case ARGUMENT_BLOCKDEF:
        compiler_set_error(compiler, NULL, gettext("Tried to evaluate blockdef"));
        return false;
    case ARGUMENT_COLOR:
        bytecode_push_op_int(&compiler->bytecode, IR_PUSHI, *(int*)&arg->data.color);
        *return_val = DATA_COLOR;
        return true;
    }
    return false;
}

bool compiler_evaluate_block(Compiler* compiler, Block* block, AnyValue* block_return) {
    if (!block->blockdef) {
        compiler_set_error(compiler, block, gettext("Tried to execute block without definition"));
        return false;
    }
    if (!block->blockdef->func) {
        compiler_set_error(compiler, block, gettext("Tried to execute block \"%s\" without implementation"), block->blockdef->id);
        return false;
    }

    BlockFunc execute_block = block->blockdef->func;

    *block_return = execute_block(compiler, block);
    if (block_return->type == DATA_TYPE_UNKNOWN) {
        scrap_log(LOG_ERROR, "[COMPILER] Error from block id: \"%s\" (at block %p)", block->blockdef->id, &block);
        return false;
    }

    return true;
}

bool compiler_evaluate_chain(Compiler* compiler, BlockChain* chain) {
    AnyValue block_return;
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        thread_handle_stopping_state(compiler->thread);
        if (!compiler_evaluate_block(compiler, &chain->blocks[i], &block_return)) return false;
    }
    return true;
}
