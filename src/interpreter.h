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

#define VM_ARG_STACK_SIZE 1024
#define VM_CONTROL_STACK_SIZE 32768
#define VM_VARIABLE_STACK_SIZE 1024
#define VM_CHAIN_STACK_SIZE 1024

typedef struct Variable Variable;
typedef struct Exec Exec;
typedef struct ChainStackData ChainStackData;

typedef enum {
    CONTROL_STATE_NORMAL = 0,
    CONTROL_STATE_BEGIN,
    CONTROL_STATE_END,
} ControlState;

typedef bool (*BlockFunc)(Exec* exec, Block* block, int argc, AnyValue* argv, AnyValue* return_val, ControlState control_state);

struct Variable {
    const char* name;
    // This is a pretty hacky way to make gc think this area of memory is allocated with
    // gc_malloc even though it is not. The data_type field in header should be set to
    // DATA_TYPE_ANY so that gc could check the potential heap references inside the any
    // value. This essentially allows interpreter to change variable type without
    // invalidating gc root pointers.
    AnyValue* value_ptr;
    GcChunkData chunk_header;
    AnyValue value;

    size_t chain_layer;
    int layer;
};

struct ChainStackData {
    bool skip_block;
    int layer;
    size_t running_ind;
    int custom_argc;
    AnyValue* custom_argv;
    bool is_returning;
    AnyValue return_arg;
};

typedef struct {
    Blockdef* blockdef;
    int arg_ind;
} DefineArgument;

typedef struct {
    Blockdef* blockdef;
    BlockChain* run_chain;
    DefineArgument* args;
} DefineFunction;

struct Exec {
    BlockChain* code;

    AnyValue arg_stack[VM_ARG_STACK_SIZE];
    size_t arg_stack_len;

    unsigned char control_stack[VM_CONTROL_STACK_SIZE];
    size_t control_stack_len;

    Variable variable_stack[VM_VARIABLE_STACK_SIZE];
    size_t variable_stack_len;

    ChainStackData chain_stack[VM_CHAIN_STACK_SIZE];
    size_t chain_stack_len;

    DefineFunction* defined_functions;

    char current_error[MAX_ERROR_LEN];
    Block* current_error_block;

    Thread* thread;
    BlockChain* running_chain;

    Gc gc;
};

#define control_stack_push_data(data, type) do { \
    if (exec->control_stack_len + sizeof(type) > VM_CONTROL_STACK_SIZE) { \
        TraceLog(LOG_ERROR, "[VM] Control stack overflow"); \
        thread_exit(exec->thread, false); \
    } \
    *(type *)(exec->control_stack + exec->control_stack_len) = (data); \
    exec->control_stack_len += sizeof(type); \
} while (0)

#define control_stack_pop_data(data, type) do { \
    if (sizeof(type) > exec->control_stack_len) { \
        TraceLog(LOG_ERROR, "[VM] Control stack underflow"); \
        thread_exit(exec->thread, false); \
    } \
    exec->control_stack_len -= sizeof(type); \
    data = *(type*)(exec->control_stack + exec->control_stack_len); \
} while (0)

#define DATA_NOTHING (AnyValue) { \
    .type = DATA_TYPE_NOTHING, \
    .data = (AnyValueData) {0}, \
}

#define DATA_INTEGER(val) (AnyValue) { \
    .type = DATA_TYPE_INTEGER, \
    .data = (AnyValueData) { .integer_val = (val) }, \
}

#define DATA_FLOAT(val) (AnyValue) { \
    .type = DATA_TYPE_FLOAT, \
    .data = (AnyValueData) { .float_val = (val) }, \
}

#define DATA_BOOL(val) (AnyValue) { \
    .type = DATA_TYPE_BOOL, \
    .data = (AnyValueData) { .integer_val = (val) }, \
}

#define DATA_LITERAL(val) (AnyValue) { \
    .type = DATA_TYPE_LITERAL, \
    .data = (AnyValueData) { .literal_val = (val) }, \
}

#define DATA_STRING(val) (AnyValue) { \
    .type = DATA_TYPE_STRING, \
    .data = (AnyValueData) { .str_val = (val) }, \
}

#define DATA_LIST(val) (AnyValue) { \
    .type = DATA_TYPE_LIST, \
    .data = (AnyValueData) { .list_val = (val) }, \
}

#define DATA_COLOR(val) (AnyValue) { \
    .type = DATA_TYPE_COLOR, \
    .data = (AnyValueData) { .color_val = (val) }, \
}

Exec exec_new(Thread* thread);
bool exec_run(void* e);
void exec_cleanup(void* e);
void exec_free(Exec* exec);
bool exec_run_chain(Exec* exec, BlockChain* chain, int argc, AnyValue* argv, AnyValue* return_val);
void exec_set_skip_block(Exec* exec);
void exec_set_error(Exec* exec, Block* block, const char* fmt, ...);

bool evaluate_argument(Exec* exec, Argument* arg, AnyValue* return_val);

Variable* variable_stack_push_var(Exec* exec, const char* name, AnyValue arg);
Variable* variable_stack_get_variable(Exec* exec, const char* name);

#endif // INTERPRETER_H
