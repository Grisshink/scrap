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
typedef enum DataType DataType;
typedef enum DataStorageType DataStorageType;
typedef struct DataList DataList;
typedef union DataContents DataContents;
typedef struct DataStorage DataStorage;
typedef struct Data Data;

typedef Data (*BlockFunc)(Exec* exec, int argc, Data* argv);

enum DataControlArgType {
    CONTROL_ARG_BEGIN,
    CONTROL_ARG_END,
};

struct DataList {
    Data* items;
    size_t len; // Length is NOT in bytes, if you want length in bytes, use data.storage.storage_len
};

union DataContents {
    int int_arg;
    double double_arg;
    const char* str_arg;
    DataList list_arg;
    DataControlArgType control_arg;
    const void* custom_arg;
    BlockChain* chain_arg;
};

enum DataStorageType {
    // Data that is contained within arg or lives for the entire lifetime of exec
    DATA_STORAGE_STATIC,
    // Data that is allocated on heap and should be cleaned up by exec.
    // Exec usually cleans up allocated data right after the block execution
    DATA_STORAGE_MANAGED,
    // Data that is allocated on heap and should be cleaned up manually.
    // Exec may free this memory for you if it's necessary
    DATA_STORAGE_UNMANAGED,
};

struct DataStorage {
    DataStorageType type;
    size_t storage_len; // Length is in bytes, so to make copy function work correctly. Only applicable if you don't use DATA_STORAGE_STATIC
};

enum DataType {
    DATA_NOTHING = 0,
    DATA_INT,
    DATA_DOUBLE,
    DATA_STR,
    DATA_BOOL,
    DATA_LIST,
    DATA_CONTROL,
    DATA_OMIT_ARGS, // Marker for vm used in C-blocks that do not require argument recomputation
    DATA_CHAIN,
};

struct Data {
    DataType type;
    DataStorage storage;
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

    pthread_t thread;
    atomic_int running_state;
    BlockChain* running_chain;
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
    .type = DATA_NOTHING, \
    .storage = (DataStorage) { \
        .type = DATA_STORAGE_STATIC, \
        .storage_len = 0, \
    }, \
    .data = (DataContents) {0}, \
}

#define RETURN_OMIT_ARGS return (Data) { \
    .type = DATA_OMIT_ARGS, \
    .storage = (DataStorage) { \
        .type = DATA_STORAGE_STATIC, \
        .storage_len = 0, \
    }, \
    .data = (DataContents) {0}, \
}

#define RETURN_INT(val) return (Data) { \
    .type = DATA_INT, \
    .storage = (DataStorage) { \
        .type = DATA_STORAGE_STATIC, \
        .storage_len = 0, \
    }, \
    .data = (DataContents) { \
        .int_arg = (val) \
    }, \
}

#define RETURN_DOUBLE(val) return (Data) { \
    .type = DATA_DOUBLE, \
    .storage = (DataStorage) { \
        .type = DATA_STORAGE_STATIC, \
        .storage_len = 0, \
    }, \
    .data = (DataContents) { \
        .double_arg = (val) \
    }, \
}

#define RETURN_BOOL(val) return (Data) { \
    .type = DATA_BOOL, \
    .storage = (DataStorage) { \
        .type = DATA_STORAGE_STATIC, \
        .storage_len = 0, \
    }, \
    .data = (DataContents) { \
        .int_arg = (val) \
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

bool variable_stack_push_var(Exec* exec, const char* name, Data data);
Variable* variable_stack_get_variable(Exec* exec, const char* name);

int data_to_int(Data arg);
int data_to_bool(Data arg);
const char* data_to_str(Data arg);
double data_to_double(Data arg);
Data data_copy(Data arg);
void data_free(Data data);

#endif // INTERPRETER_H
