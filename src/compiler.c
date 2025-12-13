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
#include "std.h"

#include <sys/stat.h>
#include <llvm-c/Analysis.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

#ifndef _WIN32
#include <glob.h>
#endif

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

#ifdef _WIN32
#define THREAD_EXIT(exec) do { exec_thread_exit(exec); pthread_exit((void*)0); } while(0)
#define TARGET_TRIPLE "x86_64-w64-windows-gnu"
#else
#define THREAD_EXIT(exec) pthread_exit((void*)0)
#define TARGET_TRIPLE "x86_64-pc-linux-gnu"
#endif

static bool compile_program(Exec* exec);
static bool run_program(Exec* exec);
static bool build_program(Exec* exec);
static void free_defined_functions(Exec* exec);

Exec exec_new(CompilerMode mode) {
    Exec exec = (Exec) {
        .code = NULL,
        .thread = (pthread_t) {0},
        .running_state = EXEC_STATE_NOT_RUNNING,
        .current_error_block = NULL,
        .current_mode = mode,
    };
    exec.current_error[0] = 0;
    return exec;
}

void exec_free(Exec* exec) {
    (void) exec;
}

void exec_thread_exit(void* thread_exec) {
    Exec* exec = thread_exec;
    
    switch (exec->current_state) {
    case STATE_NONE:
        break;
    case STATE_COMPILE:
        LLVMDisposeModule(exec->module);
        LLVMDisposeBuilder(exec->builder);
        vector_free(exec->gc_dirty_funcs);
        vector_free(exec->compile_func_list);
        vector_free(exec->global_variables);
        free_defined_functions(exec);
        break;
    case STATE_PRE_EXEC:
        LLVMDisposeModule(exec->module);
        vector_free(exec->compile_func_list);
        break;
    case STATE_EXEC:
        gc_free(&exec->gc);
        LLVMDisposeExecutionEngine(exec->engine);
        break;
    }

    exec->running_state = EXEC_STATE_DONE;
}

void* exec_thread_entry(void* thread_exec) {
    Exec* exec = thread_exec;
    exec->running_state = EXEC_STATE_RUNNING;
    exec->current_state = STATE_NONE;
    pthread_cleanup_push(exec_thread_exit, thread_exec);

    if (!compile_program(exec)) THREAD_EXIT(exec);

    if (exec->current_mode == COMPILER_MODE_JIT) {
        if (!run_program(exec)) THREAD_EXIT(exec);
    } else {
        if (!build_program(exec)) THREAD_EXIT(exec);
    }

    pthread_cleanup_pop(1);
    return (void*)1;
}

bool exec_start(Vm* vm, Exec* exec) {
    if (vm->is_running) return false;
    if (exec->running_state != EXEC_STATE_NOT_RUNNING) return false;

    exec->running_state = EXEC_STATE_STARTING;
    if (pthread_create(&exec->thread, NULL, exec_thread_entry, exec)) {
        exec->running_state = EXEC_STATE_NOT_RUNNING;
        return false;
    }
    vm->is_running = true;

    return true;
}

bool exec_stop(Vm* vm, Exec* exec) {
    if (!vm->is_running) return false;
    if (exec->running_state != EXEC_STATE_RUNNING) return false;
    if (pthread_cancel(exec->thread)) return false;
    return true;
}

void exec_copy_code(Vm* vm, Exec* exec, BlockChain* code) {
    if (vm->is_running) return;
    if (exec->running_state != EXEC_STATE_NOT_RUNNING) return;
    exec->code = code;
}

bool exec_join(Vm* vm, Exec* exec, size_t* return_code) {
    (void) exec;
    if (!vm->is_running) return false;
    if (exec->running_state == EXEC_STATE_NOT_RUNNING) return false;

    void* return_val;
    if (pthread_join(exec->thread, &return_val)) return false;
    vm->is_running = false;
    exec->running_state = EXEC_STATE_NOT_RUNNING;
    *return_code = (size_t)return_val;
    return true;
}

bool exec_try_join(Vm* vm, Exec* exec, size_t* return_code) {
    (void) exec;
    if (!vm->is_running) return false;
    if (exec->running_state != EXEC_STATE_DONE) return false;

    void* return_val;
    if (pthread_join(exec->thread, &return_val)) return false;
    vm->is_running = false;
    exec->running_state = EXEC_STATE_NOT_RUNNING;
    *return_code = (size_t)return_val;
    return true;
}

void exec_set_error(Exec* exec, Block* block, const char* fmt, ...) {
    exec->current_error_block = block;
    va_list va;
    va_start(va, fmt);
    vsnprintf(exec->current_error, MAX_ERROR_LEN, fmt, va);
    va_end(va);
    TraceLog(LOG_ERROR, "[EXEC] %s", exec->current_error);
}

static bool control_stack_push(Exec* exec, Block* block) {
    if (exec->control_stack_len >= VM_CONTROL_STACK_SIZE) {
        exec_set_error(exec, block, "Chain stack overflow");
        return false;
    }
    exec->control_stack[exec->control_stack_len++] = block;
    return true;
}

static Block* control_stack_pop(Exec* exec) {
    if (exec->control_stack_len == 0) {
        exec_set_error(exec, NULL, "Chain stack underflow");
        return NULL;
    }
    return exec->control_stack[--exec->control_stack_len];
}

void global_variable_add(Exec* exec, Variable variable) {
    vector_add(&exec->global_variables, variable);
}

bool variable_stack_push(Exec* exec, Block* block, Variable variable) {
    if (exec->variable_stack_len >= VM_CONTROL_STACK_SIZE) {
        exec_set_error(exec, block, "Variable stack overflow");
        return false;
    }
    exec->variable_stack[exec->variable_stack_len++] = variable;
    return true;
}

Variable* variable_get(Exec* exec, const char* var_name) {
    for (ssize_t i = exec->variable_stack_len - 1; i >= 0; i--) {
        if (!strcmp(var_name, exec->variable_stack[i].name)) return &exec->variable_stack[i];
    }
    for (ssize_t i = vector_size(exec->global_variables) - 1; i >= 0; i--) {
        if (!strcmp(var_name, exec->global_variables[i].name)) return &exec->global_variables[i];
    }
    return NULL;
}

static bool variable_stack_frame_push(Exec* exec) {
    if (exec->variable_stack_frames_len >= VM_CONTROL_STACK_SIZE) {
        exec_set_error(exec, NULL, "Variable stack overflow");
        return false;
    }
    VariableStackFrame frame;
    frame.base_size = exec->variable_stack_len;

    frame.base_stack = build_call(exec, "llvm.stacksave.p0");

    exec->variable_stack_frames[exec->variable_stack_frames_len++] = frame;
    return true;
}

static bool variable_stack_frame_pop(Exec* exec) {
    if (exec->variable_stack_frames_len == 0) {
        exec_set_error(exec, NULL, "Variable stack underflow");
        return false;
    }
    VariableStackFrame frame = exec->variable_stack_frames[--exec->variable_stack_frames_len];

    build_call(exec, "llvm.stackrestore.p0", frame.base_stack);

    exec->variable_stack_len = frame.base_size;
    return true;
}

static bool evaluate_block(Exec* exec, Block* block, FuncArg* return_val, ControlState control_state, FuncArg input_val) {
    if (!block->blockdef) {
        exec_set_error(exec, block, "Tried to compile block without definition");
        return false;
    }
    if (!block->blockdef->func) {
        exec_set_error(exec, block, "Tried to compile block \"%s\" without implementation", block->blockdef->id);
        return false;
    }

    BlockCompileFunc compile_block = block->blockdef->func;
    FuncArg* args = vector_create();
    FuncArg* arg;

    if (control_state == CONTROL_STATE_BEGIN) {
        LLVMBasicBlockRef current = LLVMGetInsertBlock(exec->builder);
        LLVMBasicBlockRef control_block = LLVMInsertBasicBlock(current, "control_block");
        LLVMMoveBasicBlockAfter(control_block, current);

        LLVMBuildBr(exec->builder, control_block);
        LLVMPositionBuilderAtEnd(exec->builder, control_block);

        variable_stack_frame_push(exec);
    } else if (control_state == CONTROL_STATE_END) {
        if (exec->current_mode == COMPILER_MODE_JIT) build_call(exec, "test_cancel");
        variable_stack_frame_pop(exec);
    }

    if (block->blockdef->type == BLOCKTYPE_CONTROLEND && control_state == CONTROL_STATE_BEGIN) {
        vector_add(&args, input_val);
    }

    if (control_state != CONTROL_STATE_END) {
        for (size_t i = 0; i < vector_size(block->arguments); i++) {
            FuncArg block_return;
            switch (block->arguments[i].type) {
            case ARGUMENT_TEXT:
            case ARGUMENT_CONST_STRING:
                arg = vector_add_dst(&args);
                arg->type = DATA_TYPE_LITERAL;
                arg->data.str = block->arguments[i].data.text;
                break;
            case ARGUMENT_BLOCK:
                if (!evaluate_block(exec, &block->arguments[i].data.block, &block_return, CONTROL_STATE_NORMAL, DATA_NOTHING)) {
                    TraceLog(LOG_ERROR, "[LLVM] While compiling block id: \"%s\" (argument #%d) (at block %p)", block->blockdef->id, i + 1, block);
                    vector_free(args);
                    return false;
                }
                vector_add(&args, block_return);
                break;
            case ARGUMENT_BLOCKDEF:
                arg = vector_add_dst(&args);
                arg->type = DATA_TYPE_BLOCKDEF;
                arg->data.blockdef = block->arguments[i].data.blockdef;
                break;
            }
        }
    }

    if (control_state == CONTROL_STATE_BEGIN) {
        control_data_stack_push_data(exec->gc_dirty, bool);
    }

    if (!compile_block(exec, block, vector_size(args), args, return_val, control_state)) {
        vector_free(args);
        TraceLog(LOG_ERROR, "[LLVM] Got error while compiling block id: \"%s\" (at block %p)", block->blockdef->id, block);
        return false;
    }

    if (control_state == CONTROL_STATE_END) {
        control_data_stack_pop_data(exec->gc_dirty, bool);
    }

    if (!block->parent && exec->gc_dirty) {
        build_call(exec, "gc_flush", CONST_GC);
        exec->gc_dirty = false;
    }

    vector_free(args);
    return true;
}

static bool evaluate_chain(Exec* exec, BlockChain* chain) {
    if (vector_size(chain->blocks) == 0 || chain->blocks[0].blockdef->type != BLOCKTYPE_HAT) return true;

    exec->variable_stack_len = 0;
    exec->variable_stack_frames_len = 0;

    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        FuncArg block_return;
        Block* exec_block = &chain->blocks[i];
        ControlState control_state = chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROL ? CONTROL_STATE_BEGIN : CONTROL_STATE_NORMAL;

        if (chain->blocks[i].blockdef->type == BLOCKTYPE_END || chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROLEND) {
            exec_block = control_stack_pop(exec);
            if (!exec_block) return false;
            control_state = CONTROL_STATE_END;
        }

        if (!evaluate_block(exec, exec_block, &block_return, control_state, DATA_NOTHING)) return false;

        if (chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROLEND) {
            FuncArg bin;
            control_state = CONTROL_STATE_BEGIN;
            if (!evaluate_block(exec, &chain->blocks[i], &bin, control_state, block_return)) return false;
        }

        if (chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROL || chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROLEND) {
            if (!control_stack_push(exec, &chain->blocks[i])) return false;
        }
    }

    return true;
}

DefineArgument* get_custom_argument(Exec* exec, Blockdef* blockdef, DefineFunction** func) {
    for (size_t i = 0; i < vector_size(exec->defined_functions); i++) {
        for (size_t j = 0; j < vector_size(exec->defined_functions[i].args); j++) {
            if (exec->defined_functions[i].args[j].blockdef == blockdef) {
                *func = &exec->defined_functions[i];
                return &exec->defined_functions[i].args[j];
            }
        }
    }
    return NULL;
}

static void vector_add_str(char** vec, const char* str) {
    for (const char* str_val = str; *str_val; str_val++) vector_add(vec, *str_val);
}

DefineFunction* define_function(Exec* exec, Blockdef* blockdef) {
    for (size_t i = 0; i < vector_size(exec->defined_functions); i++) {
        if (exec->defined_functions[i].blockdef == blockdef) {
            return &exec->defined_functions[i];
        }
    }

    LLVMTypeRef func_params[32];
    Blockdef* func_params_blockdefs[32];
    unsigned int func_params_count = 0;

    char* func_name = vector_create();
    vector_add_str(&func_name, blockdef->id);
    vector_add(&func_name, ' ');

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        switch (blockdef->inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            vector_add_str(&func_name, blockdef->inputs[i].data.text);
            vector_add(&func_name, ' ');
            break;
        case INPUT_BLOCKDEF_EDITOR:
        case INPUT_DROPDOWN:
            vector_add_str(&func_name, "[] ");
            break;
        case INPUT_IMAGE_DISPLAY:
            vector_add_str(&func_name, "img ");
            break;
        case INPUT_ARGUMENT:
            func_params[func_params_count] = LLVMPointerType(LLVMInt8Type(), 0);
            func_params_blockdefs[func_params_count] = blockdef->inputs[i].data.arg.blockdef;
            func_params_count++;
            vector_add_str(&func_name, "[] ");
            break;
        }
    }
    func_name[vector_size(func_name) - 1] = 0;

    LLVMTypeRef func_type = LLVMFunctionType(LLVMPointerType(LLVMInt8Type(), 0), func_params, func_params_count, false);
    LLVMValueRef func = LLVMAddFunction(exec->module, func_name, func_type);

    vector_free(func_name);

    DefineFunction* define = vector_add_dst(&exec->defined_functions);
    define->blockdef = blockdef;
    define->func = func;
    define->args = vector_create();

    LLVMValueRef func_params_values[32];
    LLVMGetParams(func, func_params_values);

    for (unsigned int i = 0; i < func_params_count; i++) {
        DefineArgument* arg = vector_add_dst(&define->args);
        arg->blockdef = func_params_blockdefs[i];
        arg->arg = func_params_values[i];
    }

    return define;
}

LLVMValueRef build_gc_root_begin(Exec* exec, Block* block) {
    if (exec->gc_block_stack_len >= VM_CONTROL_STACK_SIZE) {
        exec_set_error(exec, block, "Gc stack overflow");
        return NULL;
    }

    LLVMValueRef root_begin = build_call(exec, "gc_root_begin", CONST_GC);
    GcBlock gc_block = (GcBlock) {
        .root_begin = root_begin,
        .required = false,
    };
    exec->gc_block_stack[exec->gc_block_stack_len++] = gc_block;

    return root_begin;
}

LLVMValueRef build_gc_root_end(Exec* exec, Block* block) {
    if (exec->gc_block_stack_len == 0) {
        exec_set_error(exec, block, "Gc stack underflow");
        return NULL;
    }

    GcBlock gc_block = exec->gc_block_stack[--exec->gc_block_stack_len];
    if (!gc_block.required) {
        LLVMInstructionEraseFromParent(gc_block.root_begin);
        return (LLVMValueRef)-1;
    }

    return build_call(exec, "gc_root_end", CONST_GC);
}

static LLVMValueRef get_function(Exec* exec, const char* func_name) {
    LLVMValueRef func = LLVMGetNamedFunction(exec->module, func_name);
    if (func) return func;
    for (size_t i = 0; i < vector_size(exec->compile_func_list); i++) {
        if (!strcmp(exec->compile_func_list[i].name, func_name)) {
            func = LLVMAddFunction(exec->module, func_name, exec->compile_func_list[i].type);
            if (exec->compile_func_list[i].dynamic) vector_add(&exec->gc_dirty_funcs, func);
            return func;
        }
    }
    exec_set_error(exec, NULL, "[LLVM] Function with name \"%s\" does not exist", func_name);
    return NULL;
}

static LLVMValueRef build_call_va(Exec* exec, const char* func_name, LLVMValueRef func, LLVMTypeRef func_type, size_t func_param_count, va_list va) {
    for (size_t i = 0; i < vector_size(exec->gc_dirty_funcs); i++) {
        if (func != exec->gc_dirty_funcs[i]) continue;
        exec->gc_dirty = true;
        if (exec->gc_block_stack_len > 0) {
            exec->gc_block_stack[exec->gc_block_stack_len - 1].required = true;
        }
    }

    // Should be enough for all functions
    assert(func_param_count <= 32);
    LLVMValueRef func_param_list[32];

    for (unsigned int i = 0; i < func_param_count; i++) {
        func_param_list[i] = va_arg(va, LLVMValueRef);
    }

    if (LLVMGetTypeKind(LLVMGetReturnType(func_type)) == LLVMVoidTypeKind) {
        return LLVMBuildCall2(exec->builder, func_type, func, func_param_list, func_param_count, "");
    } else {
        return LLVMBuildCall2(exec->builder, func_type, func, func_param_list, func_param_count, func_name);
    }
}

LLVMValueRef build_call_count(Exec* exec, const char* func_name, size_t func_param_count, ...) {
    LLVMValueRef func = get_function(exec, func_name);
    LLVMTypeRef func_type = LLVMGlobalGetValueType(func);
    LLVMValueRef out;

    va_list va;
    va_start(va, func_param_count);
    out = build_call_va(exec, func_name, func, func_type, func_param_count, va);
    va_end(va);

    return out;
}

LLVMValueRef build_call(Exec* exec, const char* func_name, ...) {
    LLVMValueRef func = get_function(exec, func_name);
    LLVMTypeRef func_type = LLVMGlobalGetValueType(func);
    unsigned int func_param_count = LLVMCountParamTypes(func_type);
    LLVMValueRef out;

    va_list va;
    va_start(va, func_name);
    out = build_call_va(exec, func_name, func, func_type, func_param_count, va);
    va_end(va);

    return out;
}

// Dynamic means the func calls gc_malloc at some point. This is needed for gc.root_temp_chunks cleanup
static void add_function(Exec* exec, const char* name, LLVMTypeRef return_type, LLVMTypeRef* params, size_t params_len, void* func, bool dynamic, bool variadic) {
    CompileFunction* comp_func = vector_add_dst(&exec->compile_func_list);
    comp_func->func = func;
    comp_func->name = name;
    comp_func->type = LLVMFunctionType(return_type, params, params_len, variadic);
    comp_func->dynamic = dynamic;
}

static LLVMValueRef register_globals(Exec* exec) {
    LLVMTypeRef print_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_term_print_str", LLVMInt32Type(), print_func_params, ARRLEN(print_func_params), std_term_print_str, false, false);

    LLVMTypeRef print_integer_func_params[] = { LLVMInt32Type() };
    add_function(exec, "std_term_print_integer", LLVMInt32Type(), print_integer_func_params, ARRLEN(print_integer_func_params), std_term_print_integer, false, false);

    LLVMTypeRef print_float_func_params[] = { LLVMDoubleType() };
    add_function(exec, "std_term_print_float", LLVMInt32Type(), print_float_func_params, ARRLEN(print_float_func_params), std_term_print_float, false, false);

    LLVMTypeRef print_bool_func_params[] = { LLVMInt1Type() };
    add_function(exec, "std_term_print_bool", LLVMInt32Type(), print_bool_func_params, ARRLEN(print_bool_func_params), std_term_print_bool, false, false);

    LLVMTypeRef print_list_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_term_print_list", LLVMInt32Type(), print_list_func_params, ARRLEN(print_list_func_params), std_term_print_list, false, false);

    LLVMTypeRef print_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_term_print_any", LLVMInt32Type(), print_any_func_params, ARRLEN(print_any_func_params), std_term_print_any, false, false);

    LLVMTypeRef string_literal_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type() };
    add_function(exec, "std_string_from_literal", LLVMPointerType(LLVMInt8Type(), 0), string_literal_func_params, ARRLEN(string_literal_func_params), std_string_from_literal, true, false);

    LLVMTypeRef string_integer_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(exec, "std_string_from_integer", LLVMPointerType(LLVMInt8Type(), 0), string_integer_func_params, ARRLEN(string_integer_func_params), std_string_from_integer, true, false);

    LLVMTypeRef string_bool_func_params[] = { LLVMInt64Type(), LLVMInt1Type() };
    add_function(exec, "std_string_from_bool", LLVMPointerType(LLVMInt8Type(), 0), string_bool_func_params, ARRLEN(string_bool_func_params), std_string_from_bool, true, false);

    LLVMTypeRef string_float_func_params[] = { LLVMInt64Type(), LLVMDoubleType() };
    add_function(exec, "std_string_from_float", LLVMPointerType(LLVMInt8Type(), 0), string_float_func_params, ARRLEN(string_float_func_params), std_string_from_float, true, false);

    LLVMTypeRef string_any_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_string_from_any", LLVMPointerType(LLVMInt8Type(), 0), string_any_func_params, ARRLEN(string_any_func_params), std_string_from_any, true, false);

    LLVMTypeRef string_get_data_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_string_get_data", LLVMPointerType(LLVMInt8Type(), 0), string_get_data_func_params, ARRLEN(string_get_data_func_params), std_string_get_data, false, false);

    LLVMTypeRef integer_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_integer_from_any", LLVMInt32Type(), integer_any_func_params, ARRLEN(integer_any_func_params), std_integer_from_any, false, false);

    LLVMTypeRef float_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_float_from_any", LLVMDoubleType(), float_any_func_params, ARRLEN(float_any_func_params), std_float_from_any, false, false);

    LLVMTypeRef bool_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_bool_from_any", LLVMInt1Type(), bool_any_func_params, ARRLEN(bool_any_func_params), std_bool_from_any, false, false);

    LLVMTypeRef list_any_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_list_from_any", LLVMPointerType(LLVMInt8Type(), 0), list_any_func_params, ARRLEN(list_any_func_params), std_list_from_any, true, false);

    LLVMTypeRef any_cast_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(exec, "std_any_from_value", LLVMPointerType(LLVMInt8Type(), 0), any_cast_func_params, ARRLEN(any_cast_func_params), std_any_from_value, true, true);

    LLVMTypeRef string_length_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_string_length", LLVMInt32Type(), string_length_func_params, ARRLEN(string_length_func_params), std_string_length, false, false);

    LLVMTypeRef string_join_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_string_join", LLVMPointerType(LLVMInt8Type(), 0), string_join_func_params, ARRLEN(string_join_func_params), std_string_join, true, false);

    LLVMTypeRef string_ord_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_string_ord", LLVMInt32Type(), string_ord_func_params, ARRLEN(string_ord_func_params), std_string_ord, false, false);

    LLVMTypeRef string_chr_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(exec, "std_string_chr", LLVMPointerType(LLVMInt8Type(), 0), string_chr_func_params, ARRLEN(string_chr_func_params), std_string_chr, true, false);

    LLVMTypeRef string_letter_in_func_params[] = { LLVMInt64Type(), LLVMInt32Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_string_letter_in", LLVMPointerType(LLVMInt8Type(), 0), string_letter_in_func_params, ARRLEN(string_letter_in_func_params), std_string_letter_in, true, false);

    LLVMTypeRef string_substring_func_params[] = { LLVMInt64Type(), LLVMInt32Type(), LLVMInt32Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_string_substring", LLVMPointerType(LLVMInt8Type(), 0), string_substring_func_params, ARRLEN(string_substring_func_params), std_string_substring, true, false);

    LLVMTypeRef string_eq_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_string_is_eq", LLVMInt1Type(), string_eq_func_params, ARRLEN(string_eq_func_params), std_string_is_eq, false, false);

    LLVMTypeRef any_eq_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_any_is_eq", LLVMInt1Type(), any_eq_func_params, ARRLEN(any_eq_func_params), std_any_is_eq, false, false);

    LLVMTypeRef sleep_func_params[] = { LLVMInt32Type() };
    add_function(exec, "std_sleep", LLVMInt32Type(), sleep_func_params, ARRLEN(sleep_func_params), std_sleep, false, false);

    LLVMTypeRef random_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    add_function(exec, "std_get_random", LLVMInt32Type(), random_func_params, ARRLEN(random_func_params), std_get_random, false, false);

    LLVMTypeRef atoi_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "atoi", LLVMInt32Type(), atoi_func_params, ARRLEN(atoi_func_params), atoi, false, false);

    LLVMTypeRef atof_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "atof", LLVMDoubleType(), atof_func_params, ARRLEN(atof_func_params), atof, false, false);

    LLVMTypeRef int_pow_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    add_function(exec, "std_int_pow", LLVMInt32Type(), int_pow_func_params, ARRLEN(int_pow_func_params), std_int_pow, false, false);

    LLVMTypeRef time_func_params[] = { LLVMPointerType(LLVMVoidType(), 0) };
    add_function(exec, "time", LLVMInt32Type(), time_func_params, ARRLEN(time_func_params), time, false, false);

    LLVMTypeRef sin_func_params[] = { LLVMDoubleType() };
    add_function(exec, "sin", LLVMDoubleType(), sin_func_params, ARRLEN(sin_func_params), sin, false, false);

    LLVMTypeRef cos_func_params[] = { LLVMDoubleType() };
    add_function(exec, "cos", LLVMDoubleType(), cos_func_params, ARRLEN(cos_func_params), cos, false, false);

    LLVMTypeRef tan_func_params[] = { LLVMDoubleType() };
    add_function(exec, "tan", LLVMDoubleType(), tan_func_params, ARRLEN(tan_func_params), tan, false, false);

    LLVMTypeRef asin_func_params[] = { LLVMDoubleType() };
    add_function(exec, "asin", LLVMDoubleType(), asin_func_params, ARRLEN(asin_func_params), asin, false, false);

    LLVMTypeRef acos_func_params[] = { LLVMDoubleType() };
    add_function(exec, "acos", LLVMDoubleType(), acos_func_params, ARRLEN(acos_func_params), acos, false, false);

    LLVMTypeRef atan_func_params[] = { LLVMDoubleType() };
    add_function(exec, "atan", LLVMDoubleType(), atan_func_params, ARRLEN(atan_func_params), atan, false, false);

    LLVMTypeRef sqrt_func_params[] = { LLVMDoubleType() };
    add_function(exec, "sqrt", LLVMDoubleType(), sqrt_func_params, ARRLEN(sqrt_func_params), sqrt, false, false);

    LLVMTypeRef round_func_params[] = { LLVMDoubleType() };
    add_function(exec, "round", LLVMDoubleType(), round_func_params, ARRLEN(round_func_params), round, false, false);

    LLVMTypeRef floor_func_params[] = { LLVMDoubleType() };
    add_function(exec, "floor", LLVMDoubleType(), floor_func_params, ARRLEN(floor_func_params), floor, false, false);
    
    LLVMTypeRef get_char_func_params[] = { LLVMInt64Type() };
    add_function(exec, "std_term_get_char", LLVMPointerType(LLVMInt8Type(), 0), get_char_func_params, ARRLEN(get_char_func_params), std_term_get_char, true, false);

    LLVMTypeRef get_input_func_params[] = { LLVMInt64Type() };
    add_function(exec, "std_term_get_input", LLVMPointerType(LLVMInt8Type(), 0), get_input_func_params, ARRLEN(get_input_func_params), std_term_get_input, true, false);

    LLVMTypeRef set_clear_color_func_params[] = { LLVMInt32Type() };
    add_function(exec, "std_term_set_clear_color", LLVMVoidType(), set_clear_color_func_params, ARRLEN(set_clear_color_func_params), std_term_set_clear_color, false, false);

    LLVMTypeRef set_fg_color_func_params[] = { LLVMInt32Type() };
    add_function(exec, "std_term_set_fg_color", LLVMVoidType(), set_fg_color_func_params, ARRLEN(set_fg_color_func_params), std_term_set_fg_color, false, false);

    LLVMTypeRef set_bg_color_func_params[] = { LLVMInt32Type() };
    add_function(exec, "std_term_set_bg_color", LLVMVoidType(), set_bg_color_func_params, ARRLEN(set_bg_color_func_params), std_term_set_bg_color, false, false);

    LLVMTypeRef set_cursor_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    add_function(exec, "std_term_set_cursor", LLVMVoidType(), set_cursor_func_params, ARRLEN(set_cursor_func_params), std_term_set_cursor, false, false);

    add_function(exec, "std_term_cursor_x", LLVMInt32Type(), NULL, 0, std_term_cursor_x, false, false);
    add_function(exec, "std_term_cursor_y", LLVMInt32Type(), NULL, 0, std_term_cursor_y, false, false);
    add_function(exec, "std_term_cursor_max_x", LLVMInt32Type(), NULL, 0, std_term_cursor_max_x, false, false);
    add_function(exec, "std_term_cursor_max_y", LLVMInt32Type(), NULL, 0, std_term_cursor_max_y, false, false);

    add_function(exec, "std_term_clear", LLVMVoidType(), NULL, 0, std_term_clear, false, false);

    LLVMTypeRef list_new_func_params[] = { LLVMInt64Type() };
    add_function(exec, "std_list_new", LLVMPointerType(LLVMInt8Type(), 0), list_new_func_params, ARRLEN(list_new_func_params), std_list_new, true, false);

    LLVMTypeRef list_add_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type() };
    add_function(exec, "std_list_add", LLVMVoidType(), list_add_func_params, ARRLEN(list_add_func_params), std_list_add, true, true);

    LLVMTypeRef list_get_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type() };
    add_function(exec, "std_list_get", LLVMPointerType(LLVMInt8Type(), 0), list_get_func_params, ARRLEN(list_get_func_params), std_list_get, true, false);

    LLVMTypeRef list_set_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type(), LLVMInt32Type() };
    add_function(exec, "std_list_set", LLVMPointerType(LLVMInt8Type(), 0), list_set_func_params, ARRLEN(list_set_func_params), std_list_set, false, true);

    LLVMTypeRef list_length_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "std_list_length", LLVMInt32Type(), list_length_func_params, ARRLEN(list_length_func_params), std_list_length, false, false);

    LLVMTypeRef ceil_func_params[] = { LLVMDoubleType() };
    add_function(exec, "ceil", LLVMDoubleType(), ceil_func_params, ARRLEN(ceil_func_params), ceil, false, false);

    add_function(exec, "test_cancel", LLVMVoidType(), NULL, 0, pthread_testcancel, false, false);

    LLVMTypeRef stack_save_func_type = LLVMFunctionType(LLVMPointerType(LLVMVoidType(), 0), NULL, 0, false);
    LLVMAddFunction(exec->module, "llvm.stacksave.p0", stack_save_func_type);

    LLVMTypeRef stack_restore_func_params[] = { LLVMPointerType(LLVMVoidType(), 0) };
    LLVMTypeRef stack_restore_func_type = LLVMFunctionType(LLVMVoidType(), stack_restore_func_params, ARRLEN(stack_restore_func_params), false);
    LLVMAddFunction(exec->module, "llvm.stackrestore.p0", stack_restore_func_type);

    LLVMTypeRef gc_root_begin_func_params[] = { LLVMInt64Type() };
    add_function(exec, "gc_root_begin", LLVMVoidType(), gc_root_begin_func_params, ARRLEN(gc_root_begin_func_params), gc_root_begin, false, false);

    LLVMTypeRef gc_root_end_func_params[] = { LLVMInt64Type() };
    add_function(exec, "gc_root_end", LLVMVoidType(), gc_root_end_func_params, ARRLEN(gc_root_end_func_params), gc_root_end, false, false);
    
    LLVMTypeRef gc_flush_func_params[] = { LLVMInt64Type() };
    add_function(exec, "gc_flush", LLVMVoidType(), gc_flush_func_params, ARRLEN(gc_flush_func_params), gc_flush, false, false);

    LLVMTypeRef gc_add_root_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "gc_add_root", LLVMVoidType(), gc_add_root_func_params, ARRLEN(gc_add_root_func_params), gc_add_root, false, false);

    LLVMTypeRef gc_add_temp_root_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "gc_add_temp_root", LLVMVoidType(), gc_add_temp_root_func_params, ARRLEN(gc_add_temp_root_func_params), gc_add_temp_root, false, false);

    LLVMTypeRef gc_collect_func_params[] = { LLVMInt64Type() };
    add_function(exec, "gc_collect", LLVMVoidType(), gc_collect_func_params, ARRLEN(gc_collect_func_params), gc_collect, false, false);

    LLVMTypeRef gc_root_save_func_params[] = { LLVMInt64Type() };
    add_function(exec, "gc_root_save", LLVMVoidType(), gc_root_save_func_params, ARRLEN(gc_root_save_func_params), gc_root_save, false, false);

    LLVMTypeRef gc_root_restore_func_params[] = { LLVMInt64Type() };
    add_function(exec, "gc_root_restore", LLVMVoidType(), gc_root_restore_func_params, ARRLEN(gc_root_restore_func_params), gc_root_restore, false, false);

    LLVMAddGlobal(exec->module, LLVMInt64Type(), "gc");

    LLVMTypeRef main_func_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, false);
    LLVMValueRef main_func = LLVMAddFunction(exec->module, MAIN_NAME, main_func_type);

    return main_func;
}

static void free_defined_functions(Exec* exec) {
    for (size_t i = 0; i < vector_size(exec->defined_functions); i++) {
        vector_free(exec->defined_functions[i].args);
    }
    vector_free(exec->defined_functions);
}

static bool compile_program(Exec* exec) {
    exec->compile_func_list = vector_create();
    exec->global_variables = vector_create();
    exec->gc_block_stack_len = 0;
    exec->control_stack_len = 0;
    exec->control_data_stack_len = 0;
    exec->variable_stack_len = 0;
    exec->variable_stack_frames_len = 0;
    exec->gc_dirty = false;
    exec->gc_dirty_funcs = vector_create();
    exec->defined_functions = vector_create();
    exec->current_state = STATE_COMPILE;

    exec->module = LLVMModuleCreateWithName("scrap_module");
    LLVMSetTarget(exec->module, TARGET_TRIPLE);

    LLVMValueRef main_func = register_globals(exec);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(main_func, "entry");

    exec->builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(exec->builder, entry);

    exec->gc_value = LLVMBuildLoad2(exec->builder, LLVMInt64Type(), LLVMGetNamedGlobal(exec->module, "gc"), "get_gc");

    if (!build_gc_root_begin(exec, NULL)) return false;

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        if (strcmp(exec->code[i].blocks[0].blockdef->id, "on_start")) continue;
        if (!evaluate_chain(exec, &exec->code[i])) {
            return false;
        }
    }

    if (!build_gc_root_end(exec, NULL)) return false;
    LLVMBuildRetVoid(exec->builder);

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        if (!strcmp(exec->code[i].blocks[0].blockdef->id, "on_start")) continue;
        if (!evaluate_chain(exec, &exec->code[i])) {
            return false;
        }

        if (vector_size(exec->code[i].blocks) != 0 && exec->code[i].blocks[0].blockdef->type == BLOCKTYPE_HAT) {
            if (!build_gc_root_end(exec, NULL)) return false;
            build_call(exec, "gc_root_restore", CONST_GC);
            LLVMValueRef val = build_call_count(exec, "std_any_from_value", 2, CONST_GC, CONST_INTEGER(DATA_TYPE_NOTHING));
            LLVMBuildRet(exec->builder, val);
        }
    }

    char *error = NULL;
    if (LLVMVerifyModule(exec->module, LLVMReturnStatusAction , &error)) {
        exec_set_error(exec, NULL, "Failed to build module: %s", error);
        return false;
    }
    LLVMDisposeMessage(error);

    LLVMDumpModule(exec->module);

    LLVMDisposeBuilder(exec->builder);
    vector_free(exec->gc_dirty_funcs);
    free_defined_functions(exec);

    return true;
}

static void vector_append(char** vec, char* str) {
    if (vector_size(*vec) > 0 && (*vec)[vector_size(*vec) - 1] == 0) vector_pop(*vec);
    for (char* s = str; *s; s++) vector_add(vec, *s);
    vector_add(vec, 0);
}

static bool file_exists(char* path) {
    struct stat s;
    if (stat(path, &s)) return false;
    return S_ISREG(s.st_mode);
}

static char* find_path_glob(char* search_path, int file_len) {
#ifdef _WIN32
	return NULL;
#else
	glob_t glob_buf;
	if (glob(search_path, 0, NULL, &glob_buf)) return NULL;

    char* path = glob_buf.gl_pathv[0];
    size_t len = strlen(path);

    path[len - file_len] = 0;

    char* out = vector_create();
    vector_append(&out, path);
    globfree(&glob_buf);
    return out;
#endif
}

static char* find_crt(void) {
    char* out;
    if (file_exists("/usr/lib/crt1.o")) {
        out = vector_create();
        vector_append(&out, "/usr/lib/");
        return out;
    }
    if (file_exists("/usr/lib64/crt1.o")) {
        out = vector_create();
        vector_append(&out, "/usr/lib64/");
        return out;
    }

    out = find_path_glob("/usr/lib/x86_64*linux*/crt1.o", sizeof("crt1.o") - 1);
    if (out) return out;
    return find_path_glob("/usr/lib64/x86_64*linux*/crt1.o", sizeof("crt1.o") - 1);
}

static char* find_crt_begin(void) {
    char* out = find_path_glob("/usr/lib/gcc/x86_64*linux*/*/crtbegin.o", sizeof("crtbegin.o") - 1);
    if (out) return out;
    return find_path_glob("/usr/lib64/gcc/x86_64*linux*/*/crtbegin.o", sizeof("crtbegin.o") - 1);
}

static bool build_program(Exec* exec) {
    exec->current_state = STATE_PRE_EXEC;

    if (LLVMInitializeNativeTarget()) {
        exec_set_error(exec, NULL, "[LLVM] Native target initialization failed");
        return false;
    }
    if (LLVMInitializeNativeAsmParser()) {
        exec_set_error(exec, NULL, "[LLVM] Native asm parser initialization failed");
        return false;
    }
    if (LLVMInitializeNativeAsmPrinter()) {
        exec_set_error(exec, NULL, "[LLVM] Native asm printer initialization failed");
        return false;
    }

    char *error = NULL;

    LLVMTargetRef target;

    if (LLVMGetTargetFromTriple(TARGET_TRIPLE, &target, &error)) {
        exec_set_error(exec, NULL, "[LLVM] Failed to get target: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }

    LLVMTargetMachineOptionsRef machine_opts = LLVMCreateTargetMachineOptions();
    LLVMTargetMachineOptionsSetCodeGenOptLevel(machine_opts, LLVMCodeGenLevelDefault);
    LLVMTargetMachineOptionsSetRelocMode(machine_opts, LLVMRelocPIC);

    LLVMTargetMachineRef machine = LLVMCreateTargetMachineWithOptions(target, TARGET_TRIPLE, machine_opts);
    if (!machine) {
        LLVMDisposeTargetMachineOptions(machine_opts);
        exec_set_error(exec, NULL, "[LLVM] Failed to create target machine");
        return false;
    }
    LLVMDisposeTargetMachineOptions(machine_opts);
    
    if (LLVMTargetMachineEmitToFile(machine, exec->module, "output.o", LLVMObjectFile, &error)) {
        exec_set_error(exec, NULL, "[LLVM] Failed to save to file: %s", error);
        LLVMDisposeTargetMachine(machine);
        LLVMDisposeMessage(error);
        return false;
    }
    LLVMDisposeTargetMachine(machine);
    TraceLog(LOG_INFO, "Built object file successfully");

    char link_error[1024];
    char* command = vector_create();
#ifdef _WIN32
    // Command for linking on Windows. This thing requires gcc, which is not ideal :/
    vector_append(&command, "x86_64-w64-mingw32-gcc.exe -static -o a.exe output.o -L. -lscrapstd-win -lm");
#else
    char* crt_dir = find_crt();
    if (!crt_dir) {
        exec_set_error(exec, NULL, "Could not find crt files for linking");
        vector_free(command);
        return false;
    }

    char* crt_begin_dir = find_crt_begin();

    TraceLog(LOG_INFO, "Crt dir: %s", crt_dir);
    if (crt_begin_dir) {
        TraceLog(LOG_INFO, "Crtbegin dir: %s", crt_begin_dir);
    } else {
        TraceLog(LOG_WARNING, "Crtbegin dir is not found!");
    }

    vector_append(&command, "ld ");
    vector_append(&command, "-dynamic-linker /lib64/ld-linux-x86-64.so.2 ");
    vector_append(&command, "-pie ");
    vector_append(&command, "-o a.out ");

    vector_append(&command, (char*)TextFormat("%scrti.o %sScrt1.o %scrtn.o ", crt_dir, crt_dir, crt_dir));
    if (crt_begin_dir) vector_append(&command, (char*)TextFormat("%scrtbeginS.o %scrtendS.o ", crt_begin_dir, crt_begin_dir));

    vector_append(&command, "output.o ");
    vector_append(&command, "-L. -lscrapstd -L/usr/lib -L/lib -lm -lc");

    TraceLog(LOG_INFO, "Full command: \"%s\"", command);
#endif
    
    bool res = spawn_process(command, link_error, 1024);
    if (res) {
        TraceLog(LOG_INFO, "Linked successfully");
    } else {
        exec_set_error(exec, NULL, link_error);
    }

    vector_free(command);
    return res;
}

static bool run_program(Exec* exec) {
    exec->current_state = STATE_PRE_EXEC;

    if (LLVMInitializeNativeTarget()) {
        exec_set_error(exec, NULL, "[LLVM] Native target initialization failed");
        return false;
    }
    if (LLVMInitializeNativeAsmParser()) {
        exec_set_error(exec, NULL, "[LLVM] Native asm parser initialization failed");
        return false;
    }
    if (LLVMInitializeNativeAsmPrinter()) {
        exec_set_error(exec, NULL, "[LLVM] Native asm printer initialization failed");
        return false;
    }
    LLVMLinkInMCJIT();

    char *error = NULL;
    if (LLVMCreateExecutionEngineForModule(&exec->engine, exec->module, &error)) {
        exec_set_error(exec, NULL, "[LLVM] Failed to create execution engine: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }

    for (size_t i = 0; i < vector_size(exec->compile_func_list); i++) {
        LLVMValueRef func = LLVMGetNamedFunction(exec->module, exec->compile_func_list[i].name);
        if (!func) continue;
        LLVMAddGlobalMapping(exec->engine, func, exec->compile_func_list[i].func);
    }

    vector_free(exec->compile_func_list);

    exec->gc = gc_new(MEMORY_LIMIT);
    Gc* gc_ref = &exec->gc;
    LLVMAddGlobalMapping(exec->engine, LLVMGetNamedGlobal(exec->module, "gc"), &gc_ref);

    exec->current_state = STATE_EXEC;
    LLVMGenericValueRef val = LLVMRunFunction(exec->engine, LLVMGetNamedFunction(exec->module, "llvm_main"), 0, NULL);
    LLVMDisposeGenericValue(val);

    return true;
}
