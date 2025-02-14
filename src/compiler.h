#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#define VM_ARG_STACK_SIZE 1024

typedef struct {
    BlockChain* code;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMExecutionEngineRef engine;

    //pthread_t thread;
    //atomic_bool is_running;
} Exec;

typedef bool (*BlockCompileFunc)(Exec* exec, int argc, LLVMValueRef* argv);

Exec exec_new(void);
void exec_free(Exec* exec);
void exec_copy_code(Vm* vm, Exec* exec, BlockChain* code);
//bool exec_run_chain(Exec* exec, BlockChain* chain, int argc, Data* argv, Data* return_val);
bool exec_start(Vm* vm, Exec* exec);
bool exec_stop(Vm* vm, Exec* exec);
bool exec_join(Vm* vm, Exec* exec, size_t* return_code);
bool exec_try_join(Vm* vm, Exec* exec, size_t* return_code);
void exec_thread_exit(void* thread_exec);

#endif // COMPILER_H
