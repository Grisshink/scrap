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

typedef enum DataControlArgType DataControlArgType;
typedef enum DataStorageType DataStorageType;
typedef struct DataList DataList;
typedef union DataContents DataContents;
typedef struct DataStorage DataStorage;
typedef struct Data Data;

typedef enum {
    CONTROL_STATE_NORMAL = 0,
    CONTROL_STATE_BEGIN,
    CONTROL_STATE_END,
} ControlState;

typedef Data (*BlockFunc)(Exec* exec, Block* block, int argc, Data* argv, ControlState control_state);

union DataContents {
    int int_arg;
    double double_arg;
    char* str_arg;
    List* list_arg;
    AnyValue* any_arg;
    void* custom_arg;
};

struct Data {
    DataType type;
    DataContents data;
};

struct Variable {
    const char* name;
    Data value;
    size_t chain_layer;
    int layer;
};

struct ChainStackData {
    bool skip_block;
    int layer;
    size_t running_ind;
    int custom_argc;
    Data* custom_argv;
    bool is_returning;
    Data return_arg;
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

    Data arg_stack[VM_ARG_STACK_SIZE];
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

#define RETURN_NOTHING return (Data) { \
    .type = DATA_TYPE_NOTHING, \
    .data = (DataContents) {0}, \
}

#define RETURN_INT(val) return (Data) { \
    .type = DATA_TYPE_INT, \
    .data = (DataContents) { \
        .int_arg = (val) \
    }, \
}

#define RETURN_DOUBLE(val) return (Data) { \
    .type = DATA_TYPE_DOUBLE, \
    .data = (DataContents) { \
        .double_arg = (val) \
    }, \
}

#define RETURN_BOOL(val) return (Data) { \
    .type = DATA_TYPE_BOOL, \
    .data = (DataContents) { \
        .int_arg = (val) \
    }, \
}

#define RETURN_STRING_LITERAL(val) return (Data) { \
    .type = DATA_TYPE_STRING_LITERAL, \
    .data = (DataContents) { \
        .str_arg = (val) \
    }, \
}

#define RETURN_STRING_REF(val) return (Data) { \
    .type = DATA_TYPE_STRING_REF, \
    .data = (DataContents) { \
        .str_arg = (val) \
    }, \
}

#define RETURN_LIST(val) return (Data) { \
    .type = DATA_TYPE_LIST, \
    .data = (DataContents) { \
        .list_arg = (val) \
    }, \
}

Exec exec_new(void);
void exec_free(Exec* exec);
void exec_copy_code(Vm* vm, Exec* exec, BlockChain* code);
bool exec_run_chain(Exec* exec, BlockChain* chain, int argc, Data* argv, Data* return_val);
bool exec_start(Vm* vm, Exec* exec);
bool exec_stop(Vm* vm, Exec* exec);
bool exec_join(Vm* vm, Exec* exec, size_t* return_code);
bool exec_try_join(Vm* vm, Exec* exec, size_t* return_code);
void exec_set_skip_block(Exec* exec);
void exec_thread_exit(void* thread_exec);

Data evaluate_argument(Exec* exec, Argument* arg);

void variable_stack_push_var(Exec* exec, const char* name, Data data);
Variable* variable_stack_get_variable(Exec* exec, const char* name);

int data_to_int(Data arg);
int data_to_bool(Data arg);
char* data_to_string_ref(Exec* exec, Data arg);
char* data_to_any_string(Exec* exec, Data arg);
double data_to_double(Data arg);

#endif // INTERPRETER_H
