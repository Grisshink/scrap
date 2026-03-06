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
typedef struct CompilerValue CompilerValue;
typedef CompilerValue BlockFunc(Compiler* compiler, Block* block, Block** next_block, Block* prev_block);

typedef struct {
    DataType return_type;
    IrBytecode bc;
} BytecodeChunk;

typedef union {
    char* literal_val;
    int integer_val;
    double float_val;
    IrList* list_val;
    StdColor color_val;
    BytecodeChunk chunk_val;
} CompilerValueData;

struct CompilerValue {
    DataType type;
    CompilerValueData data;
};

struct Compiler {
    RootBlockChain* code;

    BlockChain** chains_to_compile;

    IrBytecodePool* bc_pool;
    IrBytecode bytecode;

    IrExec exec;
    bool exec_running;

    char current_error[MAX_ERROR_LEN];
    Block* current_error_block;
    BlockChain* current_error_blockchain;

    Thread* thread;
};

#define _DATA(_t, ...) (CompilerValue) { \
    .type = (_t), \
    .data = (CompilerValueData) { __VA_ARGS__ }, \
}

#define DATA_UNKNOWN _DATA(DATA_TYPE_UNKNOWN, 0)
#define DATA_NOTHING _DATA(DATA_TYPE_NOTHING, 0)
#define DATA_INTEGER(v) _DATA(DATA_TYPE_INTEGER, .int_val = (v))
#define DATA_FLOAT(v) _DATA(DATA_TYPE_FLOAT, .float_val = (v))
#define DATA_BOOL(v) _DATA(DATA_TYPE_BOOL, .bool_val = (v))
#define DATA_COLOR(v) _DATA(DATA_TYPE_COLOR, .color_val = (v))
#define DATA_LIST(v) _DATA(DATA_TYPE_LIST, .list_val = (v))
#define DATA_LITERAL(v) _DATA(DATA_TYPE_LITERAL, .literal_val = (v))
#define DATA_CHUNK(_t, _bc) _DATA(DATA_TYPE_CHUNK, .chunk_val = (BytecodeChunk) { \
    .return_type = (_t), \
    .bc = (_bc), \
})

#define EMPTY_BYTECODE bytecode_new(NULL, compiler->bc_pool)
#define EMPTY_CHUNK DATA_CHUNK(DATA_TYPE_NULL, EMPTY_BYTECODE)

Compiler compiler_new(Thread* thread);
bool compiler_run(void* e);
void compiler_cleanup(void* e);
void compiler_free(Compiler* compiler);
CompilerValue compiler_evaluate_chain(Compiler* compiler, BlockChain* chain);
CompilerValue compiler_evaluate_block(Compiler* compiler, Block* block, Block** next_block, Block* prev_block);
CompilerValue compiler_evaluate_argument(Compiler* compiler, Argument* arg);
void compiler_set_skip_block(Compiler* compiler);
void compiler_set_error(Compiler* compiler, const char* fmt, ...);

#endif // INTERPRETER_H
