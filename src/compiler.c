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
#include <wchar.h>

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
        case IR_TYPE_INT: printf("%lc", (wint_t)c.as.int_val); break;
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

    compiler->const_pool = constant_pool_new();

    compiler->bytecode = bytecode_new("main", compiler->const_pool);
    compiler->exec_running = false;

    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        assert(!CHAIN_EMPTY(compiler->code[i].chain));
        Block* block = compiler->code[i].chain->start;
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;

        CompilerValue value = compiler_evaluate_block(compiler, block);
        if (value.type == DATA_TYPE_UNKNOWN) return false;
    }

    bytecode_push_label(&compiler->bytecode, "entry");

    bytecode_push_op_int(&compiler->bytecode, IR_PUSHI, GetRandomValue(0, 4096));
    bytecode_push_op_float(&compiler->bytecode, IR_PUSHF, (double)GetRandomValue(0, 4096)/10.0);

    IrBytecode other_bc = bytecode_new(NULL, compiler->const_pool);

    ConstId other = bytecode_push_label(&other_bc, "other");
    bytecode_push_op_int(&other_bc, IR_PUSHI, GetRandomValue(0, 4096));

    bytecode_push_op_label(&compiler->bytecode, IR_IF, other);

    bytecode_print(&compiler->bytecode);
    bytecode_print(&other_bc);

    bytecode_join(&compiler->bytecode, &other_bc);

    bytecode_print(&compiler->bytecode);

    // compiler->exec = exec_new(1024 * 1024); // 1 MB
    // if (compiler->exec.last_error[0] != 0) {
    //     compiler_set_error(compiler, "Exec create error: %s", compiler->exec.last_error);
    //     exec_free(&compiler->exec);
    //     return false;
    // }
    // exec_set_run_function_resolver(&compiler->exec, resolve_function);
    // exec_add_bytecode(&compiler->exec, compiler->bytecode);
    // compiler->exec_running = true;

    // if (!exec_run(&compiler->exec, "main", "entry")) {
    //     compiler_set_error(compiler, "Runtime error: %s", compiler->exec.last_error);
    //     return false;
    // }

    return true;
}

void compiler_cleanup(void* e) {
    Compiler* compiler = e;
    if (compiler->exec_running) {
        exec_free(&compiler->exec);
    } else {
        bytecode_free(&compiler->bytecode);
    }

    constant_pool_free(compiler->const_pool);
}

void compiler_set_error(Compiler* compiler, const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vsnprintf(compiler->current_error, MAX_ERROR_LEN, fmt, va);
    va_end(va);
    scrap_log(LOG_ERROR, "[COMPILER] %s", compiler->current_error);
}

CompilerValue compiler_evaluate_argument(Compiler* compiler, Argument* arg) {
    static_assert(ARGUMENT_LAST == 5, "Exhaustive argument type in compiler_evaluate_argument");
    switch (arg->type) {
    case ARGUMENT_TEXT:
    case ARGUMENT_CONST_STRING:
        return DATA_LITERAL(arg->data.text);
    case ARGUMENT_BLOCK:
        return compiler_evaluate_block(compiler, arg->data.block);
    case ARGUMENT_BLOCKDEF:
        compiler_set_error(compiler, gettext("Tried to evaluate blockdef"));
        return DATA_UNKNOWN;
    case ARGUMENT_COLOR:
        return DATA_COLOR(CONVERT_COLOR(arg->data.color, StdColor));
    default:
        assert(false && "Unimplemented argument type in compiler_evaluate_argument");
    }
}

CompilerValue compiler_evaluate_block(Compiler* compiler, Block* block) {
    if (!block->blockdef) {
        compiler_set_error(compiler, gettext("Tried to execute block without definition"));
        return DATA_UNKNOWN;
    }
    if (!block->blockdef->func) {
        compiler_set_error(compiler, gettext("Tried to execute block \"%s\" without implementation"), block->blockdef->id);
        return DATA_UNKNOWN;
    }

    BlockFunc execute_block = block->blockdef->func;
    CompilerValue value = execute_block(compiler, block);
    if (value.type == DATA_TYPE_UNKNOWN) {
        scrap_log(LOG_ERROR, "[COMPILER] Error from block id: \"%s\" (at block %p)", block->blockdef->id, &block);
    }

    return value;
}

CompilerValue compiler_evaluate_chain(Compiler* compiler, Block* chain) {
    for (Block* iter = chain; iter; iter = iter->next) {
        thread_handle_stopping_state(compiler->thread);
        CompilerValue value = compiler_evaluate_block(compiler, iter);
        if (value.type == DATA_TYPE_UNKNOWN) {
            scrap_log(LOG_ERROR, "[COMPILER] From chain: %p", chain);
            return value;
        }
        if (value.type != DATA_TYPE_CHUNK) {
            compiler_set_error(compiler, gettext("Top level blocks should return type %s, but got %s instead"), type_to_str(DATA_TYPE_CHUNK), type_to_str(value.type));
            return DATA_UNKNOWN;
        }
    }
    return EMPTY_CHUNK;
}
