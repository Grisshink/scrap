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

#ifndef COMPILER_H
#define COMPILER_H

#include "gc.h"
#include "ast.h"

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <stdatomic.h>
#include <pthread.h>

#define VM_ARG_STACK_SIZE 1024
#define VM_CONTROL_STACK_SIZE 1024
#define VM_CONTROL_DATA_STACK_SIZE 32768
#define VM_VARIABLE_STACK_SIZE 1024

#define control_data_stack_push_data(data, type) \
    if (exec->control_data_stack_len + sizeof(type) > VM_CONTROL_DATA_STACK_SIZE) { \
        TraceLog(LOG_ERROR, "[LLVM] Control stack overflow"); \
        return false; \
    } \
    *(type *)(exec->control_data_stack + exec->control_data_stack_len) = (data); \
    exec->control_data_stack_len += sizeof(type);

#define control_data_stack_pop_data(data, type) \
    if (sizeof(type) > exec->control_data_stack_len) { \
        TraceLog(LOG_ERROR, "[LLVM] Control stack underflow"); \
        return false; \
    } \
    exec->control_data_stack_len -= sizeof(type); \
    data = *(type*)(exec->control_data_stack + exec->control_data_stack_len);

typedef enum {
    CONTROL_STATE_NORMAL = 0,
    CONTROL_STATE_BEGIN,
    CONTROL_STATE_END,
} ControlState;

typedef union {
    LLVMValueRef value;
    const char* str;
    Blockdef* blockdef;
} FuncArgData;

typedef struct {
    DataType type;
    FuncArgData data;
} FuncArg;

typedef struct {
    FuncArg value;
    LLVMTypeRef type;
    const char* name;
} Variable;

typedef struct {
    size_t base_size;
    LLVMValueRef base_stack;
} VariableStackFrame;

typedef struct {
    const char* name;
    void* func;
    LLVMTypeRef type;
    bool dynamic;
} CompileFunction;

typedef struct {
    Blockdef* blockdef;
    LLVMValueRef arg;
} DefineArgument;

typedef struct {
    Blockdef* blockdef;
    LLVMValueRef func;
    DefineArgument* args;
} DefineFunction;

typedef enum {
    STATE_NONE,
    STATE_COMPILE,
    STATE_PRE_EXEC,
    STATE_EXEC,
} CompilerState;

typedef enum {
    COMPILER_MODE_JIT = 0,
    COMPILER_MODE_BUILD,
} CompilerMode;

typedef struct {
    LLVMValueRef root_begin;
    bool required;
} GcBlock;

typedef struct {
    BlockChain* code;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMExecutionEngineRef engine;

    Block* control_stack[VM_CONTROL_STACK_SIZE];
    size_t control_stack_len;

    GcBlock gc_block_stack[VM_CONTROL_STACK_SIZE];
    size_t gc_block_stack_len;

    unsigned char control_data_stack[VM_CONTROL_DATA_STACK_SIZE];
    size_t control_data_stack_len;

    Variable variable_stack[VM_VARIABLE_STACK_SIZE];
    size_t variable_stack_len;

    Variable* global_variables;

    VariableStackFrame variable_stack_frames[VM_VARIABLE_STACK_SIZE];
    size_t variable_stack_frames_len;

    CompileFunction* compile_func_list;
    DefineFunction* defined_functions;

    Gc gc;
    LLVMValueRef gc_value;

    CompilerState current_state;

    char current_error[MAX_ERROR_LEN];
    Block* current_error_block;

    // Needed for compiler to determine if some block uses gc_malloc so we could call gc_flush afterwards
    bool gc_dirty;
    LLVMValueRef* gc_dirty_funcs;

    pthread_t thread;
    CompilerMode current_mode;
    atomic_int running_state;
} Exec;

#define MAIN_NAME "llvm_main"

#define CONST_NOTHING LLVMConstPointerNull(LLVMVoidType())
#define CONST_INTEGER(val) LLVMConstInt(LLVMInt32Type(), val, true)
#define CONST_BOOLEAN(val) LLVMConstInt(LLVMInt1Type(), val, false)
#define CONST_DOUBLE(val) LLVMConstReal(LLVMDoubleType(), val)
#define CONST_STRING_LITERAL(val) LLVMBuildGlobalStringPtr(exec->builder, val, "")
#define CONST_GC exec->gc_value
#define CONST_EXEC LLVMConstInt(LLVMInt64Type(), (unsigned long long)exec, false)

#define _DATA(t, val) (FuncArg) { \
    .type = t, \
    .data = (FuncArgData) { \
        .value = val, \
    }, \
}

#define DATA_BOOLEAN(val) _DATA(DATA_TYPE_BOOL, val)
#define DATA_STRING_REF(val) _DATA(DATA_TYPE_STRING_REF, val)
#define DATA_INTEGER(val) _DATA(DATA_TYPE_INT, val)
#define DATA_DOUBLE(val) _DATA(DATA_TYPE_DOUBLE, val)
#define DATA_LIST(val) _DATA(DATA_TYPE_LIST, val)
#define DATA_ANY(val) _DATA(DATA_TYPE_ANY, val)
#define DATA_UNKNOWN _DATA(DATA_TYPE_UNKNOWN, NULL)
#define DATA_NOTHING _DATA(DATA_TYPE_NOTHING, CONST_NOTHING)

typedef bool (*BlockCompileFunc)(Exec* exec, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state);

Exec exec_new(CompilerMode mode);
void exec_free(Exec* exec);
void exec_copy_code(Vm* vm, Exec* exec, BlockChain* code);
//bool exec_run_chain(Exec* exec, BlockChain* chain, int argc, Data* argv, Data* return_val);
bool exec_start(Vm* vm, Exec* exec);
bool exec_stop(Vm* vm, Exec* exec);
bool exec_join(Vm* vm, Exec* exec, size_t* return_code);
bool exec_try_join(Vm* vm, Exec* exec, size_t* return_code);
void exec_thread_exit(void* thread_exec);
void exec_set_error(Exec* exec, Block* block, const char* fmt, ...);

bool variable_stack_push(Exec* exec, Block* block, Variable variable);
Variable* variable_get(Exec* exec, const char* var_name);
void global_variable_add(Exec* exec, Variable variable);

LLVMValueRef build_gc_root_begin(Exec* exec, Block* block);
LLVMValueRef build_gc_root_end(Exec* exec, Block* block);
LLVMValueRef build_call(Exec* exec, const char* func_name, ...);
LLVMValueRef build_call_count(Exec* exec, const char* func_name, size_t func_param_count, ...);

DefineFunction* define_function(Exec* exec, Blockdef* blockdef);
DefineArgument* get_custom_argument(Exec* exec, Blockdef* blockdef, DefineFunction** func);

#endif // COMPILER_H
