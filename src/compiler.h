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

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "raylib.h"
#include "ast.h"
#include "std.h"
#include "thread.h"
#include "scrap_ir.h"

typedef struct Compiler Compiler;
typedef AnyValue (*BlockFunc)(Compiler* compiler, Block* block);

struct Compiler {
    BlockChain* code;

    IrBytecode bytecode;
    IrBytecode scratch_bytecode;
    size_t discard_layer;

    IrExec exec;
    bool exec_running;

    char current_error[MAX_ERROR_LEN];
    Block* current_error_block;

    Thread* thread;
};

#define _DATA_WITH_TYPE(_t) (AnyValue) { \
    .type = (_t), \
    .data = (AnyValueData) {0}, \
}

#define DATA_UNKNOWN (AnyValue) {0}

#define DATA_NOTHING _DATA_WITH_TYPE(DATA_TYPE_NOTHING)
#define DATA_INTEGER _DATA_WITH_TYPE(DATA_TYPE_INTEGER)
#define DATA_FLOAT _DATA_WITH_TYPE(DATA_TYPE_FLOAT)
#define DATA_BOOL _DATA_WITH_TYPE(DATA_TYPE_BOOL)
#define DATA_COLOR _DATA_WITH_TYPE(DATA_TYPE_COLOR)
#define DATA_LIST _DATA_WITH_TYPE(DATA_TYPE_LIST)
#define DATA_STRING _DATA_WITH_TYPE(DATA_TYPE_STRING)

#define DATA_LITERAL(v) (AnyValue) { \
    .type = DATA_TYPE_LITERAL, \
    .data = (AnyValueData) { \
        .literal_val = (v), \
    }, \
}

Compiler compiler_new(Thread* thread);
bool compiler_run(void* e);
void compiler_cleanup(void* e);
void compiler_free(Compiler* compiler);
bool compiler_evaluate_chain(Compiler* compiler, BlockChain* chain);
bool compiler_evaluate_block(Compiler* compiler, Block* block, AnyValue* block_return);
bool compiler_evaluate_argument(Compiler* compiler, Argument* arg, AnyValue* return_val);
void compiler_set_skip_block(Compiler* compiler);
void compiler_set_error(Compiler* compiler, Block* block, const char* fmt, ...);

#endif // INTERPRETER_H
