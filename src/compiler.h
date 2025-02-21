#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

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

typedef struct {
    LLVMValueRef ptr;
    LLVMTypeRef type;
    const char* name;
} Variable;

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

    size_t variable_stack_frames[VM_VARIABLE_STACK_SIZE];
    size_t variable_stack_frames_len;

    //pthread_t thread;
    //atomic_bool is_running;
} Exec;

typedef enum {
    CONTROL_BEGIN,
    CONTROL_END,
} FuncArgControlType;

typedef enum {
    FUNC_ARG_STRING,
    FUNC_ARG_VALUE,
    FUNC_ARG_CONTROL,
} FuncArgType;

typedef struct {
    FuncArgControlType type;
    LLVMBasicBlockRef block;
} ControlData;

typedef union {
    LLVMValueRef value;
    ControlData control;
    const char* str;
} FuncArgData;

typedef struct {
    FuncArgType type;
    FuncArgData data;
} FuncArg;

typedef bool (*BlockCompileFunc)(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val);

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

#endif // COMPILER_H
