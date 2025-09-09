// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-2025 Grisshink
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "raylib.h"
#include "ast.h"
#include "std.h"

#include <pthread.h>
#include <stdatomic.h>

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

    pthread_t thread;
    atomic_int running_state;
    BlockChain* running_chain;

    Gc gc;
};

#ifdef _WIN32
// Winpthreads for some reason does not trigger cleanup functions, so we are explicitly doing cleanup here
#define PTHREAD_FAIL(exec) \
    exec_thread_exit(exec); \
    pthread_exit((void*)0);
#else
#define PTHREAD_FAIL(exec) pthread_exit((void*)0);
#endif

#define control_stack_push_data(data, type) \
    if (exec->control_stack_len + sizeof(type) > VM_CONTROL_STACK_SIZE) { \
        TraceLog(LOG_ERROR, "[VM] Control stack overflow"); \
        PTHREAD_FAIL(exec); \
    } \
    *(type *)(exec->control_stack + exec->control_stack_len) = (data); \
    exec->control_stack_len += sizeof(type);

#define control_stack_pop_data(data, type) \
    if (sizeof(type) > exec->control_stack_len) { \
        TraceLog(LOG_ERROR, "[VM] Control stack underflow"); \
        PTHREAD_FAIL(exec); \
    } \
    exec->control_stack_len -= sizeof(type); \
    data = *(type*)(exec->control_stack + exec->control_stack_len);

#define DATA_NOTHING (AnyValue) { \
    .type = DATA_TYPE_NOTHING, \
    .data = (AnyValueData) {0}, \
}

#define DATA_INTEGER(val) (AnyValue) { \
    .type = DATA_TYPE_INT, \
    .data = (AnyValueData) { .int_val = (val) }, \
}

#define DATA_DOUBLE(val) (AnyValue) { \
    .type = DATA_TYPE_DOUBLE, \
    .data = (AnyValueData) { .double_val = (val) }, \
}

#define DATA_BOOL(val) (AnyValue) { \
    .type = DATA_TYPE_BOOL, \
    .data = (AnyValueData) { .int_val = (val) }, \
}

#define DATA_STRING_LITERAL(val) (AnyValue) { \
    .type = DATA_TYPE_STRING_LITERAL, \
    .data = (AnyValueData) { .str_val = (val) }, \
}

#define DATA_STRING_REF(val) (AnyValue) { \
    .type = DATA_TYPE_STRING_REF, \
    .data = (AnyValueData) { .str_val = (val) }, \
}

#define DATA_LIST(val) (AnyValue) { \
    .type = DATA_TYPE_LIST, \
    .data = (AnyValueData) { .list_val = (val) }, \
}

Exec exec_new(void);
void exec_free(Exec* exec);
void exec_copy_code(Vm* vm, Exec* exec, BlockChain* code);
bool exec_run_chain(Exec* exec, BlockChain* chain, int argc, AnyValue* argv, AnyValue* return_val);
bool exec_start(Vm* vm, Exec* exec);
bool exec_stop(Vm* vm, Exec* exec);
bool exec_join(Vm* vm, Exec* exec, size_t* return_code);
bool exec_try_join(Vm* vm, Exec* exec, size_t* return_code);
void exec_set_skip_block(Exec* exec);
void exec_thread_exit(void* thread_exec);
void exec_set_error(Exec* exec, Block* block, const char* fmt, ...);

bool evaluate_argument(Exec* exec, Argument* arg, AnyValue* return_val);

void variable_stack_push_var(Exec* exec, const char* name, AnyValue data);
Variable* variable_stack_get_variable(Exec* exec, const char* name);

int data_to_int(AnyValue arg);
int data_to_bool(AnyValue arg);
char* data_to_any_string(Exec* exec, AnyValue arg);
double data_to_double(AnyValue arg);

#endif // INTERPRETER_H
