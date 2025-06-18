#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"
#include "gc.h"

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <stdatomic.h>

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
    CONTROL_BEGIN,
    CONTROL_END,
} FuncArgControlType;

typedef struct {
    FuncArgControlType type;
    LLVMBasicBlockRef block;
} ControlData;

typedef enum {
    FUNC_ARG_UNKNOWN = 0,
    FUNC_ARG_NOTHING,
    FUNC_ARG_INT,
    FUNC_ARG_DOUBLE,
    FUNC_ARG_STRING_LITERAL, // Literal string, stored in global memory
    FUNC_ARG_STRING_REF, // Pointer to a string type, managed by the current memory allocator (GC)
    FUNC_ARG_BOOL,
    // FUNC_ARG_LIST,
    FUNC_ARG_CONTROL,
} FuncArgType;

typedef union {
    LLVMValueRef value;
    ControlData control;
    const char* str;
} FuncArgData;

typedef struct {
    FuncArgType type;
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
    BlockChain* code;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMExecutionEngineRef engine;

    Block* control_stack[VM_CONTROL_STACK_SIZE];
    size_t control_stack_len;

    unsigned char control_data_stack[VM_CONTROL_DATA_STACK_SIZE];
    size_t control_data_stack_len;

    Variable variable_stack[VM_VARIABLE_STACK_SIZE];
    size_t variable_stack_len;

    VariableStackFrame variable_stack_frames[VM_VARIABLE_STACK_SIZE];
    size_t variable_stack_frames_len;

    Gc gc;

    pthread_t thread;
    atomic_bool is_running;
} Exec;

#define CONST_NOTHING LLVMConstPointerNull(LLVMVoidType())
#define CONST_INTEGER(val) LLVMConstInt(LLVMInt32Type(), val, true)
#define CONST_BOOLEAN(val) LLVMConstInt(LLVMInt1Type(), val, false)
#define CONST_DOUBLE(val) LLVMConstReal(LLVMDoubleType(), val)
#define CONST_STRING_LITERAL(val) LLVMBuildGlobalStringPtr(exec->builder, val, "")
#define CONST_GC LLVMConstInt(LLVMInt64Type(), (unsigned long long)&exec->gc, false)

#define _DATA(t, val) (FuncArg) { \
    .type = t, \
    .data = (FuncArgData) { \
        .value = val, \
    }, \
}

#define DATA_BOOLEAN(val) _DATA(FUNC_ARG_BOOL, val)
#define DATA_STRING_REF(val) _DATA(FUNC_ARG_STRING_REF, val)
#define DATA_INTEGER(val) _DATA(FUNC_ARG_INT, val)
#define DATA_DOUBLE(val) _DATA(FUNC_ARG_DOUBLE, val)
#define DATA_UNKNOWN _DATA(FUNC_ARG_UNKNOWN, NULL)
#define DATA_NOTHING _DATA(FUNC_ARG_NOTHING, CONST_NOTHING)

typedef bool (*BlockCompileFunc)(Exec* exec, int argc, FuncArg* argv, FuncArg* return_val);

Exec exec_new(void);
void exec_free(Exec* exec);
void exec_copy_code(Vm* vm, Exec* exec, BlockChain* code);
//bool exec_run_chain(Exec* exec, BlockChain* chain, int argc, Data* argv, Data* return_val);
bool exec_start(Vm* vm, Exec* exec);
bool exec_stop(Vm* vm, Exec* exec);
bool exec_join(Vm* vm, Exec* exec, size_t* return_code);
bool exec_try_join(Vm* vm, Exec* exec, size_t* return_code);
void exec_thread_exit(void* thread_exec);

bool variable_stack_push(Exec* exec, Variable variable);
Variable* variable_stack_get(Exec* exec, const char* var_name);

LLVMValueRef build_gc_root_begin(Exec* exec);
LLVMValueRef build_gc_root_end(Exec* exec);
LLVMValueRef build_call(Exec* exec, const char* func_name, ...);

#endif // COMPILER_H
