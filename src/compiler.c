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

#include "scrap.h"
#include "term.h"
#include "vec.h"

#include <llvm-c/Analysis.h>
#include <assert.h>

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

static bool compile_program(Exec* exec);
static bool run_program(Exec* exec);

Exec exec_new(void) {
    Exec exec = (Exec) {
        .code = NULL,
        //.thread = (pthread_t) {0},
        //.is_running = false,
    };
    return exec;
}

void exec_free(Exec* exec) {
    (void) exec;
}

bool exec_start(Vm* vm, Exec* exec) {
    if (vm->is_running) return false;
    //if (exec->is_running) return false;

    //if (pthread_create(&exec->thread, NULL, exec_thread_entry, exec)) return false;
    //exec->is_running = true;

    if (!compile_program(exec)) return false;
    if (!run_program(exec)) return false;

    vm->is_running = true;
    return true;
}

bool exec_stop(Vm* vm, Exec* exec) {
    (void) exec;
    if (!vm->is_running) return false;
    //if (!exec->is_running) return false;
    //if (pthread_cancel(exec->thread)) return false;
    return true;
}

void exec_copy_code(Vm* vm, Exec* exec, BlockChain* code) {
    if (vm->is_running) return;
    exec->code = code;
}

bool exec_join(Vm* vm, Exec* exec, size_t* return_code) {
    (void) exec;
    if (!vm->is_running) return false;
    //if (!exec->is_running) return false;
    
    //void* return_val;
    //if (pthread_join(exec->thread, &return_val)) return false;
    vm->is_running = false;
    //*return_code = (size_t)return_val;
    *return_code = 1;
    return true;
}

bool exec_try_join(Vm* vm, Exec* exec, size_t* return_code) {
    (void) exec;
    if (!vm->is_running) return false;
    // if (exec->is_running) return false;

    // void* return_val;
    // if (pthread_join(exec->thread, &return_val)) return false;
    vm->is_running = false;
    // *return_code = (size_t)return_val;
    *return_code = 1;
    return true;
}

static bool control_stack_push(Exec* exec, Block* block) {
    if (exec->control_stack_len >= VM_CONTROL_STACK_SIZE) {
        TraceLog(LOG_ERROR, "[VM] Chain stack overflow");
        return false;
    }
    exec->control_stack[exec->control_stack_len++] = block;
    return true;
}

static Block* control_stack_pop(Exec* exec) {
    if (exec->control_stack_len == 0) {
        TraceLog(LOG_ERROR, "[VM] Chain stack underflow");
        return NULL;
    }
    return exec->control_stack[--exec->control_stack_len];
}

static bool evaluate_block(Exec* exec, Block* block, LLVMValueRef* return_val, bool end_block) {
    if (!block->blockdef) {
        TraceLog(LOG_ERROR, "[LLVM] Tried to compile block without definition!");
        return false;
    }
    if (!block->blockdef->func) {
        TraceLog(LOG_ERROR, "[LLVM] Tried to compile block without implementation!");
        TraceLog(LOG_ERROR, "[LLVM] Relevant block id: %s", block->blockdef->id);
        return false;
    }

    BlockCompileFunc compile_block = block->blockdef->func;
    FuncArg* args = vector_create();
    FuncArg* arg;

    if (block->blockdef->type == BLOCKTYPE_CONTROL) {
        arg = vector_add_dst(&args);
        arg->type = FUNC_ARG_CONTROL;
        arg->data.control = end_block ? CONTROL_END : CONTROL_BEGIN;
    }

    if (block->blockdef->type != BLOCKTYPE_CONTROL || !end_block) {
        for (size_t i = 0; i < vector_size(block->arguments); i++) {
            LLVMValueRef block_return;
            switch (block->arguments[i].type) {
            case ARGUMENT_TEXT:
            case ARGUMENT_CONST_STRING:
                arg = vector_add_dst(&args);
                arg->type = FUNC_ARG_STRING;
                arg->data.str = block->arguments[i].data.text;
                break;
            case ARGUMENT_BLOCK:
                if (!evaluate_block(exec, &block->arguments[i].data.block, &block_return, false)) {
                    TraceLog(LOG_ERROR, "[LLVM] While compiling block id: \"%s\" (argument #%d) (at block %p)", block->blockdef->id, i + 1, block);
                    vector_free(args);
                    return false;       
                }
                arg = vector_add_dst(&args);
                arg->type = FUNC_ARG_VALUE;
                arg->data.value = block_return;
                break;
            case ARGUMENT_BLOCKDEF:
                assert(false && "Unimplemented compile blockdef argument");
                break;
            }
        }
    }

    if (!compile_block(exec, vector_size(args), args, return_val)) {
        vector_free(args);
        TraceLog(LOG_ERROR, "[LLVM] Got error while compiling block id: \"%s\" (at block %p)", block->blockdef->id, block);
        return false;
    }

    vector_free(args);
    return true;
}

static bool evaluate_chain(Exec* exec, BlockChain* chain) {
    if (vector_size(chain->blocks) == 0 || chain->blocks[0].blockdef->type != BLOCKTYPE_HAT) return true;

    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        Block* exec_block = &chain->blocks[i];
        bool is_end = false;

        if (chain->blocks[i].blockdef->type == BLOCKTYPE_END) {
            exec_block = control_stack_pop(exec);
            if (!exec_block) return false;
            is_end = true;
        }

        LLVMValueRef bin;
        if (!evaluate_block(exec, exec_block, &bin, is_end)) return false;

        if (chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROL) {
            if (!control_stack_push(exec, &chain->blocks[i])) return false;
        }
    }

    return true;
}


static int int_pow(int base, int exp) {
    if (exp == 0) return 1;

    int result = 1;
    while (exp) {
        if (exp & 1) result *= base;
        exp >>= 1;
        base *= base;
    }
    return result;
}

static LLVMBasicBlockRef register_globals(Exec* exec) {
    LLVMTypeRef print_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    LLVMTypeRef print_func_type = LLVMFunctionType(LLVMInt32Type(), print_func_params, ARRLEN(print_func_params), false);
    LLVMAddFunction(exec->module, "term_print_str", print_func_type);

    LLVMTypeRef print_int_func_params[] = { LLVMInt32Type() };
    LLVMTypeRef print_int_func_type = LLVMFunctionType(LLVMInt32Type(), print_int_func_params, ARRLEN(print_int_func_params), false);
    LLVMAddFunction(exec->module, "term_print_int", print_int_func_type);

    LLVMTypeRef print_double_func_params[] = { LLVMDoubleType() };
    LLVMTypeRef print_double_func_type = LLVMFunctionType(LLVMInt32Type(), print_double_func_params, ARRLEN(print_double_func_params), false);
    LLVMAddFunction(exec->module, "term_print_double", print_double_func_type);

    LLVMTypeRef print_bool_func_params[] = { LLVMInt1Type() };
    LLVMTypeRef print_bool_func_type = LLVMFunctionType(LLVMInt32Type(), print_bool_func_params, ARRLEN(print_bool_func_params), false);
    LLVMAddFunction(exec->module, "term_print_bool", print_bool_func_type);

    LLVMTypeRef int_pow_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    LLVMTypeRef int_pow_func_type = LLVMFunctionType(LLVMInt32Type(), int_pow_func_params, ARRLEN(int_pow_func_params), false);
    LLVMAddFunction(exec->module, "int_pow", int_pow_func_type);

    LLVMTypeRef main_func_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, false);
    LLVMValueRef main_func = LLVMAddFunction(exec->module, "llvm_main", main_func_type);

    return LLVMAppendBasicBlock(main_func, "entry");
}

static bool compile_program(Exec* exec) {
    exec->module = LLVMModuleCreateWithName("scrap_module");

    LLVMBasicBlockRef entry = register_globals(exec);

    exec->builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(exec->builder, entry);

    // Call print function
    //LLVMValueRef call_args[] = { LLVMBuildGlobalStringPtr(exec->builder, "Hello from LLVM!", "") };
    //LLVMValueRef print_func_value = LLVMGetNamedFunction(exec->module, "term_print_str");
    //LLVMTypeRef print_func_type = LLVMTypeOf(print_func_value);

    //TraceLog(LOG_INFO, "Value kind: %d", LLVMGetValueKind(print_func_value));
    //TraceLog(LOG_INFO, "Type kind: %d", LLVMGetTypeKind(print_func_type));

    //LLVMBuildCall2(exec->builder, print_func_type, print_func_value, call_args, ARRLEN(call_args), "");
    //LLVMBuildCall2(builder, print_func_type, print_func_value, call_args, ARRLEN(call_args), "");

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        if (!evaluate_chain(exec, &exec->code[i])) {
            LLVMDisposeModule(exec->module);
            LLVMDisposeBuilder(exec->builder);
            return false;
        }
    }

    LLVMBuildRetVoid(exec->builder);
    LLVMDisposeBuilder(exec->builder);

    char *error = NULL;
    if (LLVMVerifyModule(exec->module, LLVMPrintMessageAction, &error)) {
        TraceLog(LOG_ERROR, "[LLVM] Failed to build module!");
        LLVMDisposeMessage(error);
        LLVMDisposeModule(exec->module);
        return false;
    }
    
    LLVMDumpModule(exec->module);

    return true;
}

static bool run_program(Exec* exec) {
    if (LLVMInitializeNativeTarget()) {
        TraceLog(LOG_ERROR, "[LLVM] Native target initialization failed!");
        LLVMDisposeModule(exec->module);
        return false;
    }
    if (LLVMInitializeNativeAsmParser()) {
        TraceLog(LOG_ERROR, "[LLVM] Native asm parser initialization failed!");
        LLVMDisposeModule(exec->module);
        return false;
    }
    if (LLVMInitializeNativeAsmPrinter()) {
        TraceLog(LOG_ERROR, "[LLVM] Native asm printer initialization failed!");
        LLVMDisposeModule(exec->module);
        return false;
    }
    LLVMLinkInMCJIT();

    char *error = NULL;
    if (LLVMCreateExecutionEngineForModule(&exec->engine, exec->module, &error)) {
        TraceLog(LOG_ERROR, "[LLVM] Failed to create execution engine!");
        TraceLog(LOG_ERROR, "[LLVM] Error: %s", error);
        LLVMDisposeMessage(error);
        LLVMDisposeModule(exec->module);
        return false;
    }
    
    LLVMAddGlobalMapping(exec->engine, LLVMGetNamedFunction(exec->module, "term_print_str"), term_print_str);
    LLVMAddGlobalMapping(exec->engine, LLVMGetNamedFunction(exec->module, "term_print_int"), term_print_int);
    LLVMAddGlobalMapping(exec->engine, LLVMGetNamedFunction(exec->module, "term_print_double"), term_print_double);
    LLVMAddGlobalMapping(exec->engine, LLVMGetNamedFunction(exec->module, "term_print_bool"), term_print_bool);
    LLVMAddGlobalMapping(exec->engine, LLVMGetNamedFunction(exec->module, "int_pow"), int_pow);

    LLVMRunFunction(exec->engine, LLVMGetNamedFunction(exec->module, "llvm_main"), 0, NULL);

    LLVMDisposeExecutionEngine(exec->engine);
    return true;
}
