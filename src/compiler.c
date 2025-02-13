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

#include "term.h"
#include "compiler.h"

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Analysis.h>

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

bool compile_program(void) {
    LLVMModuleRef mod = LLVMModuleCreateWithName("scrap_module");

    LLVMTypeRef print_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    LLVMTypeRef print_func_type = LLVMFunctionType(LLVMInt32Type(), print_func_params, ARRLEN(print_func_params), 0);
    LLVMValueRef print_func_value = LLVMAddFunction(mod, "term_print_str", print_func_type);

    LLVMTypeRef main_func_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, 0);
    LLVMValueRef main_func = LLVMAddFunction(mod, "llvm_main", main_func_type);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(main_func, "entry");

    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef call_args[] = { LLVMBuildGlobalStringPtr(builder, "Hello from LLVM!", "") };
    LLVMBuildCall2(builder, print_func_type, print_func_value, call_args, ARRLEN(call_args), "");
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);

    char *error = NULL;
    if (LLVMVerifyModule(mod, LLVMPrintMessageAction, &error)) {
        TraceLog(LOG_ERROR, "[LLVM] Failed to build module!");
        LLVMDisposeMessage(error);
        return false;
    }
    LLVMDisposeMessage(error);
    error = NULL;
    
    LLVMDumpModule(mod);

    LLVMExecutionEngineRef engine;
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();
    LLVMLinkInMCJIT();

    if (LLVMCreateExecutionEngineForModule(&engine, mod, &error)) {
        TraceLog(LOG_ERROR, "[LLVM] Failed to create execution engine!");
        if (error) TraceLog(LOG_ERROR, "[LLVM] Error: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }
    LLVMDisposeMessage(error);
    error = NULL;
    
    LLVMAddGlobalMapping(engine, print_func_value, term_print_str);

    LLVMRunFunction(engine, main_func, 0, NULL);

    LLVMDisposeExecutionEngine(engine);
    return true;
}
