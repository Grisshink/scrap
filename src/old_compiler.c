// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-2026 Grisshink
// 
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

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
#include <libintl.h>

#ifndef _WIN32
#include <glob.h>
#endif

#ifdef _WIN32
#define TARGET_TRIPLE "x86_64-w64-windows-gnu"
#else
#define TARGET_TRIPLE "x86_64-pc-linux-gnu"
#endif

static bool compile_program(Compiler* compiler);
static bool run_program(Compiler* compiler);
static bool build_program(Compiler* compiler);
static void free_defined_functions(Compiler* compiler);

Compiler compiler_new(Thread* thread, CompilerMode mode) {
    Compiler compiler = (Compiler) {
        .code = NULL,
        .thread = thread,
        .current_error_block = NULL,
        .current_mode = mode,
    };
    compiler.current_error[0] = 0;
    return compiler;
}

void compiler_free(Compiler* compiler) {
    (void) compiler;
}

void compiler_cleanup(void* e) {
    Compiler* compiler = e;
    
    switch (compiler->current_state) {
    case STATE_NONE:
        break;
    case STATE_COMPILE:
        LLVMDisposeModule(compiler->module);
        LLVMDisposeBuilder(compiler->builder);
        vector_free(compiler->gc_dirty_funcs);
        vector_free(compiler->compile_func_list);
        vector_free(compiler->global_variables);
        free_defined_functions(compiler);
        break;
    case STATE_PRE_EXEC:
        LLVMDisposeModule(compiler->module);
        vector_free(compiler->compile_func_list);
        break;
    case STATE_EXEC:
        gc_free(&compiler->gc);
        LLVMDisposeExecutionEngine(compiler->engine);
        break;
    }
}

bool compiler_run(void* e) {
    Compiler* compiler = e;
    compiler->current_state = STATE_NONE;

    if (!compile_program(compiler)) return false;

    if (compiler->current_mode == COMPILER_MODE_JIT) {
        if (!run_program(compiler)) return false;
    } else {
        if (!build_program(compiler)) return false;
    }

    return true;
}

static void compiler_handle_running_state(Compiler* compiler) {
    if (compiler->thread->state != THREAD_STATE_STOPPING) return;
    longjmp(compiler->run_jump_buf, 1);
}

void compiler_set_error(Compiler* compiler, Block* block, const char* fmt, ...) {
    compiler->current_error_block = block;
    va_list va;
    va_start(va, fmt);
    vsnprintf(compiler->current_error, MAX_ERROR_LEN, fmt, va);
    va_end(va);
    scrap_log(LOG_ERROR, "[EXEC] %s", compiler->current_error);
}

static bool control_stack_push(Compiler* compiler, Block* block) {
    if (compiler->control_stack_len >= VM_CONTROL_STACK_SIZE) {
        compiler_set_error(compiler, block, gettext("Control stack overflow"));
        return false;
    }
    compiler->control_stack[compiler->control_stack_len++] = block;
    return true;
}

static Block* control_stack_pop(Compiler* compiler) {
    if (compiler->control_stack_len == 0) {
        compiler_set_error(compiler, NULL, gettext("Control stack underflow"));
        return NULL;
    }
    return compiler->control_stack[--compiler->control_stack_len];
}

void global_variable_add(Compiler* compiler, Variable variable) {
    vector_add(&compiler->global_variables, variable);
}

bool variable_stack_push(Compiler* compiler, Block* block, Variable variable) {
    if (compiler->variable_stack_len >= VM_CONTROL_STACK_SIZE) {
        compiler_set_error(compiler, block, gettext("Variable stack overflow"));
        return false;
    }
    compiler->variable_stack[compiler->variable_stack_len++] = variable;
    return true;
}

Variable* variable_get(Compiler* compiler, const char* var_name) {
    for (ssize_t i = compiler->variable_stack_len - 1; i >= 0; i--) {
        if (!strcmp(var_name, compiler->variable_stack[i].name)) return &compiler->variable_stack[i];
    }
    for (ssize_t i = vector_size(compiler->global_variables) - 1; i >= 0; i--) {
        if (!strcmp(var_name, compiler->global_variables[i].name)) return &compiler->global_variables[i];
    }
    return NULL;
}

static bool variable_stack_frame_push(Compiler* compiler) {
    if (compiler->variable_stack_frames_len >= VM_CONTROL_STACK_SIZE) {
        compiler_set_error(compiler, NULL, gettext("Variable stack overflow"));
        return false;
    }
    VariableStackFrame frame;
    frame.base_size = compiler->variable_stack_len;

    frame.base_stack = build_call(compiler, "llvm.stacksave.p0");

    compiler->variable_stack_frames[compiler->variable_stack_frames_len++] = frame;
    return true;
}

static bool variable_stack_frame_pop(Compiler* compiler) {
    if (compiler->variable_stack_frames_len == 0) {
        compiler_set_error(compiler, NULL, gettext("Variable stack underflow"));
        return false;
    }
    VariableStackFrame frame = compiler->variable_stack_frames[--compiler->variable_stack_frames_len];

    build_call(compiler, "llvm.stackrestore.p0", frame.base_stack);

    compiler->variable_stack_len = frame.base_size;
    return true;
}

static bool evaluate_block(Compiler* compiler, Block* block, FuncArg* return_val, ControlState control_state, FuncArg input_val) {
    if (!block->blockdef) {
        compiler_set_error(compiler, block, gettext("Tried to compile block without definition"));
        return false;
    }
    if (!block->blockdef->func) {
        compiler_set_error(compiler, block, gettext("Tried to compile block \"%s\" without implementation"), block->blockdef->id);
        return false;
    }

    BlockCompileFunc compile_block = block->blockdef->func;
    FuncArg* args = vector_create();
    FuncArg* arg;

    if (control_state == CONTROL_STATE_BEGIN) {
        LLVMBasicBlockRef current = LLVMGetInsertBlock(compiler->builder);
        LLVMBasicBlockRef control_block = LLVMInsertBasicBlock(current, "control_block");
        LLVMMoveBasicBlockAfter(control_block, current);

        LLVMBuildBr(compiler->builder, control_block);
        LLVMPositionBuilderAtEnd(compiler->builder, control_block);

        variable_stack_frame_push(compiler);
    } else if (control_state == CONTROL_STATE_END) {
        if (compiler->current_mode == COMPILER_MODE_JIT) build_call(compiler, "test_cancel", CONST_EXEC);
        variable_stack_frame_pop(compiler);
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
                if (!evaluate_block(compiler, &block->arguments[i].data.block, &block_return, CONTROL_STATE_NORMAL, DATA_NOTHING)) {
                    scrap_log(LOG_ERROR, "While compiling block id: \"%s\" (argument #%d) (at block %p)", block->blockdef->id, i + 1, block);
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
            case ARGUMENT_COLOR:
                arg = vector_add_dst(&args);
                arg->type = DATA_TYPE_COLOR;
                arg->data.value = CONST_INTEGER(*(int*)&block->arguments[i].data.color);
                break;
            }
        }
    }

    if (control_state == CONTROL_STATE_BEGIN) {
        control_data_stack_push_data(compiler->gc_dirty, bool);
    }

    if (!compile_block(compiler, block, vector_size(args), args, return_val, control_state)) {
        vector_free(args);
        scrap_log(LOG_ERROR, "Got error while compiling block id: \"%s\" (at block %p)", block->blockdef->id, block);
        return false;
    }

    if (control_state == CONTROL_STATE_END) {
        control_data_stack_pop_data(compiler->gc_dirty, bool);
    }

    if (!block->parent && compiler->gc_dirty) {
        build_call(compiler, "gc_flush", CONST_GC);
        compiler->gc_dirty = false;
    }

    vector_free(args);
    return true;
}

static bool evaluate_chain(Compiler* compiler, BlockChain* chain) {
    if (vector_size(chain->blocks) == 0 || chain->blocks[0].blockdef->type != BLOCKTYPE_HAT) return true;

    compiler->variable_stack_len = 0;
    compiler->variable_stack_frames_len = 0;

    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        FuncArg block_return;
        Block* compiler_block = &chain->blocks[i];
        ControlState control_state = chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROL ? CONTROL_STATE_BEGIN : CONTROL_STATE_NORMAL;

        if (chain->blocks[i].blockdef->type == BLOCKTYPE_END || chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROLEND) {
            compiler_block = control_stack_pop(compiler);
            if (!compiler_block) return false;
            control_state = CONTROL_STATE_END;
        }

        if (!evaluate_block(compiler, compiler_block, &block_return, control_state, DATA_NOTHING)) return false;

        if (chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROLEND) {
            FuncArg bin;
            control_state = CONTROL_STATE_BEGIN;
            if (!evaluate_block(compiler, &chain->blocks[i], &bin, control_state, block_return)) return false;
        }

        if (chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROL || chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROLEND) {
            if (!control_stack_push(compiler, &chain->blocks[i])) return false;
        }
    }

    return true;
}

DefineArgument* get_custom_argument(Compiler* compiler, Blockdef* blockdef, DefineFunction** func) {
    for (size_t i = 0; i < vector_size(compiler->defined_functions); i++) {
        for (size_t j = 0; j < vector_size(compiler->defined_functions[i].args); j++) {
            if (compiler->defined_functions[i].args[j].blockdef == blockdef) {
                *func = &compiler->defined_functions[i];
                return &compiler->defined_functions[i].args[j];
            }
        }
    }
    return NULL;
}

static void vector_add_str(char** vec, const char* str) {
    for (const char* str_val = str; *str_val; str_val++) vector_add(vec, *str_val);
}

DefineFunction* define_function(Compiler* compiler, Blockdef* blockdef) {
    for (size_t i = 0; i < vector_size(compiler->defined_functions); i++) {
        if (compiler->defined_functions[i].blockdef == blockdef) {
            return &compiler->defined_functions[i];
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
        case INPUT_COLOR:
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
    LLVMValueRef func = LLVMAddFunction(compiler->module, func_name, func_type);

    vector_free(func_name);

    DefineFunction* define = vector_add_dst(&compiler->defined_functions);
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

LLVMValueRef build_gc_root_begin(Compiler* compiler, Block* block) {
    if (compiler->gc_block_stack_len >= VM_CONTROL_STACK_SIZE) {
        compiler_set_error(compiler, block, "Gc stack overflow");
        return NULL;
    }

    LLVMValueRef root_begin = build_call(compiler, "gc_root_begin", CONST_GC);
    GcBlock gc_block = (GcBlock) {
        .root_begin = root_begin,
        .required = false,
    };
    compiler->gc_block_stack[compiler->gc_block_stack_len++] = gc_block;

    return root_begin;
}

LLVMValueRef build_gc_root_end(Compiler* compiler, Block* block) {
    if (compiler->gc_block_stack_len == 0) {
        compiler_set_error(compiler, block, "Gc stack underflow");
        return NULL;
    }

    GcBlock gc_block = compiler->gc_block_stack[--compiler->gc_block_stack_len];
    if (!gc_block.required) {
        LLVMInstructionEraseFromParent(gc_block.root_begin);
        return (LLVMValueRef)-1;
    }

    return build_call(compiler, "gc_root_end", CONST_GC);
}

static LLVMValueRef get_function(Compiler* compiler, const char* func_name) {
    LLVMValueRef func = LLVMGetNamedFunction(compiler->module, func_name);
    if (func) return func;
    for (size_t i = 0; i < vector_size(compiler->compile_func_list); i++) {
        if (!strcmp(compiler->compile_func_list[i].name, func_name)) {
            func = LLVMAddFunction(compiler->module, func_name, compiler->compile_func_list[i].type);
            if (compiler->compile_func_list[i].dynamic) vector_add(&compiler->gc_dirty_funcs, func);
            return func;
        }
    }
    compiler_set_error(compiler, NULL, gettext("Function with name \"%s\" does not exist"), func_name);
    return NULL;
}

static LLVMValueRef build_call_va(Compiler* compiler, const char* func_name, LLVMValueRef func, LLVMTypeRef func_type, size_t func_param_count, va_list va) {
    for (size_t i = 0; i < vector_size(compiler->gc_dirty_funcs); i++) {
        if (func != compiler->gc_dirty_funcs[i]) continue;
        compiler->gc_dirty = true;
        if (compiler->gc_block_stack_len > 0) {
            compiler->gc_block_stack[compiler->gc_block_stack_len - 1].required = true;
        }
    }

    // Should be enough for all functions
    assert(func_param_count <= 32);
    LLVMValueRef func_param_list[32];

    for (unsigned int i = 0; i < func_param_count; i++) {
        func_param_list[i] = va_arg(va, LLVMValueRef);
    }

    if (LLVMGetTypeKind(LLVMGetReturnType(func_type)) == LLVMVoidTypeKind) {
        return LLVMBuildCall2(compiler->builder, func_type, func, func_param_list, func_param_count, "");
    } else {
        return LLVMBuildCall2(compiler->builder, func_type, func, func_param_list, func_param_count, func_name);
    }
}

LLVMValueRef build_call_count(Compiler* compiler, const char* func_name, size_t func_param_count, ...) {
    LLVMValueRef func = get_function(compiler, func_name);
    LLVMTypeRef func_type = LLVMGlobalGetValueType(func);
    LLVMValueRef out;

    va_list va;
    va_start(va, func_param_count);
    out = build_call_va(compiler, func_name, func, func_type, func_param_count, va);
    va_end(va);

    return out;
}

LLVMValueRef build_call(Compiler* compiler, const char* func_name, ...) {
    LLVMValueRef func = get_function(compiler, func_name);
    LLVMTypeRef func_type = LLVMGlobalGetValueType(func);
    unsigned int func_param_count = LLVMCountParamTypes(func_type);
    LLVMValueRef out;

    va_list va;
    va_start(va, func_name);
    out = build_call_va(compiler, func_name, func, func_type, func_param_count, va);
    va_end(va);

    return out;
}

// Dynamic means the func calls gc_malloc at some point. This is needed for gc.root_temp_chunks cleanup
static void add_function(Compiler* compiler, const char* name, LLVMTypeRef return_type, LLVMTypeRef* params, size_t params_len, void* func, bool dynamic, bool variadic) {
    CompileFunction* comp_func = vector_add_dst(&compiler->compile_func_list);
    comp_func->func = func;
    comp_func->name = name;
    comp_func->type = LLVMFunctionType(return_type, params, params_len, variadic);
    comp_func->dynamic = dynamic;
}

static LLVMValueRef register_globals(Compiler* compiler) {
    LLVMTypeRef print_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_term_print_str", LLVMInt32Type(), print_func_params, ARRLEN(print_func_params), std_term_print_str, false, false);

    LLVMTypeRef print_integer_func_params[] = { LLVMInt32Type() };
    add_function(compiler, "std_term_print_integer", LLVMInt32Type(), print_integer_func_params, ARRLEN(print_integer_func_params), std_term_print_integer, false, false);

    LLVMTypeRef print_float_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "std_term_print_float", LLVMInt32Type(), print_float_func_params, ARRLEN(print_float_func_params), std_term_print_float, false, false);

    LLVMTypeRef print_bool_func_params[] = { LLVMInt1Type() };
    add_function(compiler, "std_term_print_bool", LLVMInt32Type(), print_bool_func_params, ARRLEN(print_bool_func_params), std_term_print_bool, false, false);

    LLVMTypeRef print_list_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_term_print_list", LLVMInt32Type(), print_list_func_params, ARRLEN(print_list_func_params), std_term_print_list, false, false);

    LLVMTypeRef print_color_func_params[] = { LLVMInt32Type() };
    add_function(compiler, "std_term_print_color", LLVMInt32Type(), print_color_func_params, ARRLEN(print_color_func_params), std_term_print_color, false, false);

    LLVMTypeRef print_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_term_print_any", LLVMInt32Type(), print_any_func_params, ARRLEN(print_any_func_params), std_term_print_any, false, false);

    LLVMTypeRef string_literal_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type() };
    add_function(compiler, "std_string_from_literal", LLVMPointerType(LLVMInt8Type(), 0), string_literal_func_params, ARRLEN(string_literal_func_params), std_string_from_literal, true, false);

    LLVMTypeRef string_integer_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(compiler, "std_string_from_integer", LLVMPointerType(LLVMInt8Type(), 0), string_integer_func_params, ARRLEN(string_integer_func_params), std_string_from_integer, true, false);

    LLVMTypeRef string_bool_func_params[] = { LLVMInt64Type(), LLVMInt1Type() };
    add_function(compiler, "std_string_from_bool", LLVMPointerType(LLVMInt8Type(), 0), string_bool_func_params, ARRLEN(string_bool_func_params), std_string_from_bool, true, false);

    LLVMTypeRef string_float_func_params[] = { LLVMInt64Type(), LLVMDoubleType() };
    add_function(compiler, "std_string_from_float", LLVMPointerType(LLVMInt8Type(), 0), string_float_func_params, ARRLEN(string_float_func_params), std_string_from_float, true, false);

    LLVMTypeRef string_color_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(compiler, "std_string_from_color", LLVMPointerType(LLVMInt8Type(), 0), string_color_func_params, ARRLEN(string_color_func_params), std_string_from_color, true, false);

    LLVMTypeRef string_any_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_string_from_any", LLVMPointerType(LLVMInt8Type(), 0), string_any_func_params, ARRLEN(string_any_func_params), std_string_from_any, true, false);

    LLVMTypeRef string_get_data_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_string_get_data", LLVMPointerType(LLVMInt8Type(), 0), string_get_data_func_params, ARRLEN(string_get_data_func_params), std_string_get_data, false, false);

    LLVMTypeRef integer_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_integer_from_any", LLVMInt32Type(), integer_any_func_params, ARRLEN(integer_any_func_params), std_integer_from_any, false, false);

    LLVMTypeRef float_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_float_from_any", LLVMDoubleType(), float_any_func_params, ARRLEN(float_any_func_params), std_float_from_any, false, false);

    LLVMTypeRef bool_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_bool_from_any", LLVMInt1Type(), bool_any_func_params, ARRLEN(bool_any_func_params), std_bool_from_any, false, false);

    LLVMTypeRef color_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_color_from_any", LLVMInt32Type(), color_any_func_params, ARRLEN(color_any_func_params), std_color_from_any, false, false);

    LLVMTypeRef parse_color_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_parse_color", LLVMInt32Type(), parse_color_func_params, ARRLEN(parse_color_func_params), std_parse_color, false, false);

    LLVMTypeRef list_any_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_list_from_any", LLVMPointerType(LLVMInt8Type(), 0), list_any_func_params, ARRLEN(list_any_func_params), std_list_from_any, true, false);

    LLVMTypeRef any_cast_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(compiler, "std_any_from_value", LLVMPointerType(LLVMInt8Type(), 0), any_cast_func_params, ARRLEN(any_cast_func_params), std_any_from_value, true, true);

    LLVMTypeRef string_length_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_string_length", LLVMInt32Type(), string_length_func_params, ARRLEN(string_length_func_params), std_string_length, false, false);

    LLVMTypeRef string_join_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_string_join", LLVMPointerType(LLVMInt8Type(), 0), string_join_func_params, ARRLEN(string_join_func_params), std_string_join, true, false);

    LLVMTypeRef string_ord_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_string_ord", LLVMInt32Type(), string_ord_func_params, ARRLEN(string_ord_func_params), std_string_ord, false, false);

    LLVMTypeRef string_chr_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(compiler, "std_string_chr", LLVMPointerType(LLVMInt8Type(), 0), string_chr_func_params, ARRLEN(string_chr_func_params), std_string_chr, true, false);

    LLVMTypeRef string_letter_in_func_params[] = { LLVMInt64Type(), LLVMInt32Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_string_letter_in", LLVMPointerType(LLVMInt8Type(), 0), string_letter_in_func_params, ARRLEN(string_letter_in_func_params), std_string_letter_in, true, false);

    LLVMTypeRef string_substring_func_params[] = { LLVMInt64Type(), LLVMInt32Type(), LLVMInt32Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_string_substring", LLVMPointerType(LLVMInt8Type(), 0), string_substring_func_params, ARRLEN(string_substring_func_params), std_string_substring, true, false);

    LLVMTypeRef string_eq_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_string_is_eq", LLVMInt1Type(), string_eq_func_params, ARRLEN(string_eq_func_params), std_string_is_eq, false, false);

    LLVMTypeRef any_eq_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_any_is_eq", LLVMInt1Type(), any_eq_func_params, ARRLEN(any_eq_func_params), std_any_is_eq, false, false);

    LLVMTypeRef sleep_func_params[] = { LLVMInt32Type() };
    add_function(compiler, "std_sleep", LLVMInt32Type(), sleep_func_params, ARRLEN(sleep_func_params), std_sleep, false, false);

    LLVMTypeRef random_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    add_function(compiler, "std_get_random", LLVMInt32Type(), random_func_params, ARRLEN(random_func_params), std_get_random, false, false);

    LLVMTypeRef set_seed_func_params[] = { LLVMInt32Type() };
    add_function(compiler, "std_set_random_seed", LLVMVoidType(), set_seed_func_params, ARRLEN(set_seed_func_params), std_set_random_seed, false, false);

    LLVMTypeRef atoi_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "atoi", LLVMInt32Type(), atoi_func_params, ARRLEN(atoi_func_params), atoi, false, false);

    LLVMTypeRef atof_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "atof", LLVMDoubleType(), atof_func_params, ARRLEN(atof_func_params), atof, false, false);

    LLVMTypeRef int_pow_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    add_function(compiler, "std_int_pow", LLVMInt32Type(), int_pow_func_params, ARRLEN(int_pow_func_params), std_int_pow, false, false);

    LLVMTypeRef time_func_params[] = { LLVMPointerType(LLVMVoidType(), 0) };
    add_function(compiler, "time", LLVMInt32Type(), time_func_params, ARRLEN(time_func_params), time, false, false);

    LLVMTypeRef sin_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "sin", LLVMDoubleType(), sin_func_params, ARRLEN(sin_func_params), sin, false, false);

    LLVMTypeRef cos_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "cos", LLVMDoubleType(), cos_func_params, ARRLEN(cos_func_params), cos, false, false);

    LLVMTypeRef tan_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "tan", LLVMDoubleType(), tan_func_params, ARRLEN(tan_func_params), tan, false, false);

    LLVMTypeRef asin_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "asin", LLVMDoubleType(), asin_func_params, ARRLEN(asin_func_params), asin, false, false);

    LLVMTypeRef acos_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "acos", LLVMDoubleType(), acos_func_params, ARRLEN(acos_func_params), acos, false, false);

    LLVMTypeRef atan_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "atan", LLVMDoubleType(), atan_func_params, ARRLEN(atan_func_params), atan, false, false);

    LLVMTypeRef sqrt_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "sqrt", LLVMDoubleType(), sqrt_func_params, ARRLEN(sqrt_func_params), sqrt, false, false);

    LLVMTypeRef round_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "round", LLVMDoubleType(), round_func_params, ARRLEN(round_func_params), round, false, false);

    LLVMTypeRef floor_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "floor", LLVMDoubleType(), floor_func_params, ARRLEN(floor_func_params), floor, false, false);

    LLVMTypeRef pow_func_params[] = { LLVMDoubleType(), LLVMDoubleType() };
    add_function(compiler, "pow", LLVMDoubleType(), pow_func_params, ARRLEN(pow_func_params), pow, false, false);
    
    LLVMTypeRef get_char_func_params[] = { LLVMInt64Type() };
    add_function(compiler, "std_term_get_char", LLVMPointerType(LLVMInt8Type(), 0), get_char_func_params, ARRLEN(get_char_func_params), std_term_get_char, true, false);

    LLVMTypeRef get_input_func_params[] = { LLVMInt64Type() };
    add_function(compiler, "std_term_get_input", LLVMPointerType(LLVMInt8Type(), 0), get_input_func_params, ARRLEN(get_input_func_params), std_term_get_input, true, false);

    LLVMTypeRef set_clear_color_func_params[] = { LLVMInt32Type() };
    add_function(compiler, "std_term_set_clear_color", LLVMVoidType(), set_clear_color_func_params, ARRLEN(set_clear_color_func_params), std_term_set_clear_color, false, false);

    LLVMTypeRef set_fg_color_func_params[] = { LLVMInt32Type() };
    add_function(compiler, "std_term_set_fg_color", LLVMVoidType(), set_fg_color_func_params, ARRLEN(set_fg_color_func_params), std_term_set_fg_color, false, false);

    LLVMTypeRef set_bg_color_func_params[] = { LLVMInt32Type() };
    add_function(compiler, "std_term_set_bg_color", LLVMVoidType(), set_bg_color_func_params, ARRLEN(set_bg_color_func_params), std_term_set_bg_color, false, false);

    LLVMTypeRef set_cursor_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    add_function(compiler, "std_term_set_cursor", LLVMVoidType(), set_cursor_func_params, ARRLEN(set_cursor_func_params), std_term_set_cursor, false, false);

    add_function(compiler, "std_term_cursor_x", LLVMInt32Type(), NULL, 0, std_term_cursor_x, false, false);
    add_function(compiler, "std_term_cursor_y", LLVMInt32Type(), NULL, 0, std_term_cursor_y, false, false);
    add_function(compiler, "std_term_cursor_max_x", LLVMInt32Type(), NULL, 0, std_term_cursor_max_x, false, false);
    add_function(compiler, "std_term_cursor_max_y", LLVMInt32Type(), NULL, 0, std_term_cursor_max_y, false, false);

    add_function(compiler, "std_term_clear", LLVMVoidType(), NULL, 0, std_term_clear, false, false);

    LLVMTypeRef list_new_func_params[] = { LLVMInt64Type() };
    add_function(compiler, "std_list_new", LLVMPointerType(LLVMInt8Type(), 0), list_new_func_params, ARRLEN(list_new_func_params), std_list_new, true, false);

    LLVMTypeRef list_add_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type() };
    add_function(compiler, "std_list_add", LLVMVoidType(), list_add_func_params, ARRLEN(list_add_func_params), std_list_add, true, true);

    LLVMTypeRef list_get_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type() };
    add_function(compiler, "std_list_get", LLVMPointerType(LLVMInt8Type(), 0), list_get_func_params, ARRLEN(list_get_func_params), std_list_get, true, false);

    LLVMTypeRef list_set_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type(), LLVMInt32Type() };
    add_function(compiler, "std_list_set", LLVMPointerType(LLVMInt8Type(), 0), list_set_func_params, ARRLEN(list_set_func_params), std_list_set, false, true);

    LLVMTypeRef list_length_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "std_list_length", LLVMInt32Type(), list_length_func_params, ARRLEN(list_length_func_params), std_list_length, false, false);

    LLVMTypeRef ceil_func_params[] = { LLVMDoubleType() };
    add_function(compiler, "ceil", LLVMDoubleType(), ceil_func_params, ARRLEN(ceil_func_params), ceil, false, false);

    LLVMTypeRef test_cancel_func_params[] = { LLVMInt64Type() };
    add_function(compiler, "test_cancel", LLVMVoidType(), test_cancel_func_params, ARRLEN(test_cancel_func_params), compiler_handle_running_state, false, false);

    LLVMTypeRef stack_save_func_type = LLVMFunctionType(LLVMPointerType(LLVMVoidType(), 0), NULL, 0, false);
    LLVMAddFunction(compiler->module, "llvm.stacksave.p0", stack_save_func_type);

    LLVMTypeRef stack_restore_func_params[] = { LLVMPointerType(LLVMVoidType(), 0) };
    LLVMTypeRef stack_restore_func_type = LLVMFunctionType(LLVMVoidType(), stack_restore_func_params, ARRLEN(stack_restore_func_params), false);
    LLVMAddFunction(compiler->module, "llvm.stackrestore.p0", stack_restore_func_type);

    LLVMTypeRef gc_root_begin_func_params[] = { LLVMInt64Type() };
    add_function(compiler, "gc_root_begin", LLVMVoidType(), gc_root_begin_func_params, ARRLEN(gc_root_begin_func_params), gc_root_begin, false, false);

    LLVMTypeRef gc_root_end_func_params[] = { LLVMInt64Type() };
    add_function(compiler, "gc_root_end", LLVMVoidType(), gc_root_end_func_params, ARRLEN(gc_root_end_func_params), gc_root_end, false, false);
    
    LLVMTypeRef gc_flush_func_params[] = { LLVMInt64Type() };
    add_function(compiler, "gc_flush", LLVMVoidType(), gc_flush_func_params, ARRLEN(gc_flush_func_params), gc_flush, false, false);

    LLVMTypeRef gc_add_root_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "gc_add_root", LLVMVoidType(), gc_add_root_func_params, ARRLEN(gc_add_root_func_params), gc_add_root, false, false);

    LLVMTypeRef gc_add_temp_root_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(compiler, "gc_add_temp_root", LLVMVoidType(), gc_add_temp_root_func_params, ARRLEN(gc_add_temp_root_func_params), gc_add_temp_root, false, false);

    LLVMTypeRef gc_collect_func_params[] = { LLVMInt64Type() };
    add_function(compiler, "gc_collect", LLVMVoidType(), gc_collect_func_params, ARRLEN(gc_collect_func_params), gc_collect, false, false);

    LLVMTypeRef gc_root_save_func_params[] = { LLVMInt64Type() };
    add_function(compiler, "gc_root_save", LLVMVoidType(), gc_root_save_func_params, ARRLEN(gc_root_save_func_params), gc_root_save, false, false);

    LLVMTypeRef gc_root_restore_func_params[] = { LLVMInt64Type() };
    add_function(compiler, "gc_root_restore", LLVMVoidType(), gc_root_restore_func_params, ARRLEN(gc_root_restore_func_params), gc_root_restore, false, false);

    LLVMAddGlobal(compiler->module, LLVMInt64Type(), "gc");

    LLVMTypeRef main_func_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, false);
    LLVMValueRef main_func = LLVMAddFunction(compiler->module, MAIN_NAME, main_func_type);

    return main_func;
}

static void free_defined_functions(Compiler* compiler) {
    for (size_t i = 0; i < vector_size(compiler->defined_functions); i++) {
        vector_free(compiler->defined_functions[i].args);
    }
    vector_free(compiler->defined_functions);
}

static bool compile_program(Compiler* compiler) {
    compiler->compile_func_list = vector_create();
    compiler->global_variables = vector_create();
    compiler->gc_block_stack_len = 0;
    compiler->control_stack_len = 0;
    compiler->control_data_stack_len = 0;
    compiler->variable_stack_len = 0;
    compiler->variable_stack_frames_len = 0;
    compiler->build_random = false;
    compiler->gc_dirty = false;
    compiler->gc_dirty_funcs = vector_create();
    compiler->defined_functions = vector_create();
    compiler->current_state = STATE_COMPILE;

    compiler->module = LLVMModuleCreateWithName("scrap_module");
    LLVMSetTarget(compiler->module, TARGET_TRIPLE);

    LLVMValueRef main_func = register_globals(compiler);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(main_func, "entry");

    compiler->builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(compiler->builder, entry);

    compiler->gc_value = LLVMBuildLoad2(compiler->builder, LLVMInt64Type(), LLVMGetNamedGlobal(compiler->module, "gc"), "get_gc");

    if (!build_gc_root_begin(compiler, NULL)) return false;

    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        if (strcmp(compiler->code[i].blocks[0].blockdef->id, "on_start")) continue;
        if (!evaluate_chain(compiler, &compiler->code[i])) {
            return false;
        }
    }

    if (!build_gc_root_end(compiler, NULL)) return false;
    LLVMBuildRetVoid(compiler->builder);

    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        if (!strcmp(compiler->code[i].blocks[0].blockdef->id, "on_start")) continue;
        if (!evaluate_chain(compiler, &compiler->code[i])) {
            return false;
        }

        if (vector_size(compiler->code[i].blocks) != 0 && compiler->code[i].blocks[0].blockdef->type == BLOCKTYPE_HAT) {
            if (!build_gc_root_end(compiler, NULL)) return false;
            build_call(compiler, "gc_root_restore", CONST_GC);
            LLVMValueRef val = build_call_count(compiler, "std_any_from_value", 2, CONST_GC, CONST_INTEGER(DATA_TYPE_NOTHING));
            LLVMBuildRet(compiler->builder, val);
        }
    }

    if (compiler->build_random) {
        LLVMBasicBlockRef random_block = LLVMInsertBasicBlock(entry, "rand_init");
        LLVMPositionBuilderAtEnd(compiler->builder, random_block);
        LLVMValueRef time_val = build_call(compiler, "time", LLVMConstPointerNull(LLVMPointerType(LLVMVoidType(), 0)));
        build_call(compiler, "std_set_random_seed", time_val);
        LLVMBuildBr(compiler->builder, entry);
    }

    char *error = NULL;
    if (LLVMVerifyModule(compiler->module, LLVMReturnStatusAction , &error)) {
        compiler_set_error(compiler, NULL, gettext("Failed to build module: %s"), error);
        return false;
    }
    LLVMDisposeMessage(error);

    LLVMDumpModule(compiler->module);

    LLVMDisposeBuilder(compiler->builder);
    vector_free(compiler->gc_dirty_funcs);
    vector_free(compiler->global_variables);
    free_defined_functions(compiler);

    return true;
}

static void vector_append(char** vec, const char* str) {
    if (vector_size(*vec) > 0 && (*vec)[vector_size(*vec) - 1] == 0) vector_pop(*vec);
    for (size_t i = 0; str[i]; i++) vector_add(vec, str[i]);
    vector_add(vec, 0);
}

static bool file_exists(char* path) {
    struct stat s;
    if (stat(path, &s)) return false;
    return S_ISREG(s.st_mode);
}

#ifndef _WIN32
static char* find_path_glob(char* search_path, int file_len) {
	glob_t glob_buf;
	if (glob(search_path, 0, NULL, &glob_buf)) return NULL;

    char* path = glob_buf.gl_pathv[0];
    size_t len = strlen(path);

    path[len - file_len] = 0;

    char* out = vector_create();
    vector_append(&out, path);
    globfree(&glob_buf);
    return out;
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
#endif

static bool build_program(Compiler* compiler) {
    compiler->current_state = STATE_PRE_EXEC;

    if (LLVMInitializeNativeTarget()) {
        compiler_set_error(compiler, NULL, "[LLVM] Native target initialization failed");
        return false;
    }
    if (LLVMInitializeNativeAsmParser()) {
        compiler_set_error(compiler, NULL, "[LLVM] Native asm parser initialization failed");
        return false;
    }
    if (LLVMInitializeNativeAsmPrinter()) {
        compiler_set_error(compiler, NULL, "[LLVM] Native asm printer initialization failed");
        return false;
    }

    char *error = NULL;

    LLVMTargetRef target;

    if (LLVMGetTargetFromTriple(TARGET_TRIPLE, &target, &error)) {
        compiler_set_error(compiler, NULL, "[LLVM] Failed to get target: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }

    LLVMTargetMachineOptionsRef machine_opts = LLVMCreateTargetMachineOptions();
    LLVMTargetMachineOptionsSetCodeGenOptLevel(machine_opts, LLVMCodeGenLevelDefault);
    LLVMTargetMachineOptionsSetRelocMode(machine_opts, LLVMRelocPIC);

    LLVMTargetMachineRef machine = LLVMCreateTargetMachineWithOptions(target, TARGET_TRIPLE, machine_opts);
    if (!machine) {
        LLVMDisposeTargetMachineOptions(machine_opts);
        compiler_set_error(compiler, NULL, "[LLVM] Failed to create target machine");
        return false;
    }
    LLVMDisposeTargetMachineOptions(machine_opts);
    
    if (LLVMTargetMachineEmitToFile(machine, compiler->module, "output.o", LLVMObjectFile, &error)) {
        compiler_set_error(compiler, NULL, "[LLVM] Failed to save to file: %s", error);
        LLVMDisposeTargetMachine(machine);
        LLVMDisposeMessage(error);
        return false;
    }
    LLVMDisposeTargetMachine(machine);
    scrap_log(LOG_INFO, "Built object file successfully");

    char link_error[1024];
    char* command = vector_create();
#ifdef _WIN32
    // Command for linking on Windows. This thing requires gcc, which is not ideal :/
    vector_append(&command, TextFormat("x86_64-w64-mingw32-gcc.exe -static -o %s.exe output.o -L. -L%s -lscrapstd-win -lm", project_config.executable_name, GetApplicationDirectory()));
#else
    char* crt_dir = find_crt();
    if (!crt_dir) {
        compiler_set_error(compiler, NULL, "Could not find crt files for linking");
        vector_free(command);
        return false;
    }

    char* crt_begin_dir = find_crt_begin();

    scrap_log(LOG_INFO, "Crt dir: %s", crt_dir);
    if (crt_begin_dir) {
        scrap_log(LOG_INFO, "Crtbegin dir: %s", crt_begin_dir);
    } else {
        scrap_log(LOG_WARNING, "Crtbegin dir is not found!");
    }

    vector_append(&command, TextFormat("%s ", project_config.linker_name));
    vector_append(&command, "-dynamic-linker /lib64/ld-linux-x86-64.so.2 ");
    vector_append(&command, "-pie ");
    vector_append(&command, TextFormat("-o %s ", project_config.executable_name));

    vector_append(&command, TextFormat("%scrti.o %sScrt1.o %scrtn.o ", crt_dir, crt_dir, crt_dir));
    if (crt_begin_dir) vector_append(&command, TextFormat("%scrtbeginS.o %scrtendS.o ", crt_begin_dir, crt_begin_dir));

    vector_append(&command, "output.o ");
    vector_append(&command, TextFormat("-L. -L%s -lscrapstd -L/usr/lib -L/lib -L/usr/local/lib -lm -lc", GetApplicationDirectory()));

    scrap_log(LOG_INFO, "Full command: \"%s\"", command);
#endif
    
    bool res = spawn_process(command, link_error, 1024);
    if (res) {
        scrap_log(LOG_INFO, "Linked successfully");
    } else {
        compiler_set_error(compiler, NULL, link_error);
    }

    vector_free(command);
#ifndef _WIN32
    vector_free(crt_dir);
    if (crt_begin_dir) vector_free(crt_begin_dir);
#endif
    return res;
}

static bool run_program(Compiler* compiler) {
    compiler->current_state = STATE_PRE_EXEC;

    if (LLVMInitializeNativeTarget()) {
        compiler_set_error(compiler, NULL, "[LLVM] Native target initialization failed");
        return false;
    }
    if (LLVMInitializeNativeAsmParser()) {
        compiler_set_error(compiler, NULL, "[LLVM] Native asm parser initialization failed");
        return false;
    }
    if (LLVMInitializeNativeAsmPrinter()) {
        compiler_set_error(compiler, NULL, "[LLVM] Native asm printer initialization failed");
        return false;
    }
    LLVMLinkInMCJIT();

    char *error = NULL;
    if (LLVMCreateExecutionEngineForModule(&compiler->engine, compiler->module, &error)) {
        compiler_set_error(compiler, NULL, "[LLVM] Failed to create execution engine: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }

    for (size_t i = 0; i < vector_size(compiler->compile_func_list); i++) {
        LLVMValueRef func = LLVMGetNamedFunction(compiler->module, compiler->compile_func_list[i].name);
        if (!func) continue;
        LLVMAddGlobalMapping(compiler->engine, func, compiler->compile_func_list[i].func);
    }

    vector_free(compiler->compile_func_list);

    compiler->gc = gc_new(MIN_MEMORY_LIMIT, MAX_MEMORY_LIMIT);
    Gc* gc_ref = &compiler->gc;
    LLVMAddGlobalMapping(compiler->engine, LLVMGetNamedGlobal(compiler->module, "gc"), &gc_ref);

    // For some weird reason calling pthread_exit() inside LLVM results in segfault, so we avoid that by using setjmp. 
    // This unfortunately leaks a little bit of memory inside LLVMRunFunction() though :P
    if (setjmp(compiler->run_jump_buf)) {
        thread_exit(compiler->thread, false);
    } else {
        memcpy(compiler->gc.run_jump_buf, compiler->run_jump_buf, sizeof(compiler->gc.run_jump_buf));
        compiler->current_state = STATE_EXEC;
        LLVMGenericValueRef val = LLVMRunFunction(compiler->engine, LLVMGetNamedFunction(compiler->module, "llvm_main"), 0, NULL);
        LLVMDisposeGenericValue(val);
    }

    return true;
}
