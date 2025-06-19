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
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <math.h>

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

// Should be enough memory for now
#define MEMORY_LIMIT 4194304 // 4 MB

static bool compile_program(Exec* exec);
static bool run_program(Exec* exec);

Exec exec_new(void) {
    Exec exec = (Exec) {
        .code = NULL,
        .thread = (pthread_t) {0},
        .is_running = false,
    };
    return exec;
}

void exec_free(Exec* exec) {
    (void) exec;
}

void exec_thread_exit(void* thread_exec) {
    Exec* exec = thread_exec;
    exec->is_running = false;
}

void* exec_thread_entry(void* thread_exec) {
    Exec* exec = thread_exec;
    exec->is_running = true;
    pthread_cleanup_push(exec_thread_exit, thread_exec);

    if (!compile_program(exec)) {
        exec->is_running = false;
        exec_thread_exit(thread_exec);
        return (void*)0;
    }

    if (!run_program(exec)) {
        exec->is_running = false;
        exec_thread_exit(thread_exec);
        return (void*)0;
    }

    pthread_cleanup_pop(1);
    return (void*)1;
}

bool exec_start(Vm* vm, Exec* exec) {
    if (vm->is_running) return false;
    if (exec->is_running) return false;

    if (pthread_create(&exec->thread, NULL, exec_thread_entry, exec)) return false;
    exec->is_running = true;

    vm->is_running = true;
    return true;
}

bool exec_stop(Vm* vm, Exec* exec) {
    if (!vm->is_running) return false;
    if (!exec->is_running) return false;
    if (pthread_cancel(exec->thread)) return false;
    return true;
}

void exec_copy_code(Vm* vm, Exec* exec, BlockChain* code) {
    if (vm->is_running) return;
    exec->code = code;
}

bool exec_join(Vm* vm, Exec* exec, size_t* return_code) {
    (void) exec;
    if (!vm->is_running) return false;
    if (!exec->is_running) return false;

    void* return_val;
    if (pthread_join(exec->thread, &return_val)) return false;
    vm->is_running = false;
    *return_code = (size_t)return_val;
    return true;
}

bool exec_try_join(Vm* vm, Exec* exec, size_t* return_code) {
    (void) exec;
    if (!vm->is_running) return false;
    if (exec->is_running) return false;

    void* return_val;
    if (pthread_join(exec->thread, &return_val)) return false;
    vm->is_running = false;
    *return_code = (size_t)return_val;
    return true;
}

static bool control_stack_push(Exec* exec, Block* block) {
    if (exec->control_stack_len >= VM_CONTROL_STACK_SIZE) {
        TraceLog(LOG_ERROR, "[LLVM] Chain stack overflow");
        return false;
    }
    exec->control_stack[exec->control_stack_len++] = block;
    return true;
}

static Block* control_stack_pop(Exec* exec) {
    if (exec->control_stack_len == 0) {
        TraceLog(LOG_ERROR, "[LLVM] Chain stack underflow");
        return NULL;
    }
    return exec->control_stack[--exec->control_stack_len];
}

bool variable_stack_push(Exec* exec, Variable variable) {
    if (exec->variable_stack_len >= VM_CONTROL_STACK_SIZE) {
        TraceLog(LOG_ERROR, "[LLVM] Variable stack overflow");
        return false;
    }
    exec->variable_stack[exec->variable_stack_len++] = variable;
    return true;
}

Variable* variable_stack_get(Exec* exec, const char* var_name) {
    for (ssize_t i = exec->variable_stack_len - 1; i >= 0; i--) {
        if (!strcmp(var_name, exec->variable_stack[i].name)) return &exec->variable_stack[i];
    }
    return NULL;
}

static bool variable_stack_frame_push(Exec* exec) {
    if (exec->variable_stack_frames_len >= VM_CONTROL_STACK_SIZE) {
        TraceLog(LOG_ERROR, "[LLVM] Variable stack overflow");
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
        TraceLog(LOG_ERROR, "[LLVM] Variable stack underflow");
        return false;
    }
    VariableStackFrame frame = exec->variable_stack_frames[--exec->variable_stack_frames_len];

    build_call(exec, "llvm.stackrestore.p0", frame.base_stack);

    exec->variable_stack_len = frame.base_size;
    return true;
}

static bool evaluate_block(Exec* exec, Block* block, FuncArg* return_val, bool end_block, FuncArg input_val) {
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

    if (block->blockdef->type == BLOCKTYPE_CONTROL || block->blockdef->type == BLOCKTYPE_CONTROLEND) {
        LLVMBasicBlockRef control_block = NULL;
        if (!end_block) {
            LLVMBasicBlockRef current = LLVMGetInsertBlock(exec->builder);
            control_block = LLVMInsertBasicBlock(current, "control_block");
            LLVMMoveBasicBlockAfter(control_block, current);

            LLVMBuildBr(exec->builder, control_block);
            LLVMPositionBuilderAtEnd(exec->builder, control_block);

            variable_stack_frame_push(exec);
        } else {
            build_call(exec, "test_cancel");
            variable_stack_frame_pop(exec);
        }

        arg = vector_add_dst(&args);
        arg->type = FUNC_ARG_CONTROL;
        arg->data.control = (ControlData) {
            .type = end_block ? CONTROL_END : CONTROL_BEGIN,
            .block = control_block,
        };
    }

    if (block->blockdef->type == BLOCKTYPE_CONTROLEND && !end_block) vector_add(&args, input_val);

    if ((block->blockdef->type != BLOCKTYPE_CONTROL && block->blockdef->type != BLOCKTYPE_CONTROLEND) || !end_block) {
        for (size_t i = 0; i < vector_size(block->arguments); i++) {
            FuncArg block_return;
            switch (block->arguments[i].type) {
            case ARGUMENT_TEXT:
            case ARGUMENT_CONST_STRING:
                arg = vector_add_dst(&args);
                arg->type = FUNC_ARG_STRING_LITERAL;
                arg->data.str = block->arguments[i].data.text;
                break;
            case ARGUMENT_BLOCK:
                if (!evaluate_block(exec, &block->arguments[i].data.block, &block_return, false, DATA_NOTHING)) {
                    TraceLog(LOG_ERROR, "[LLVM] While compiling block id: \"%s\" (argument #%d) (at block %p)", block->blockdef->id, i + 1, block);
                    vector_free(args);
                    return false;
                }
                vector_add(&args, block_return);
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
        FuncArg block_return;
        Block* exec_block = &chain->blocks[i];
        bool is_end = false;

        if (chain->blocks[i].blockdef->type == BLOCKTYPE_END || chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROLEND) {
            exec_block = control_stack_pop(exec);
            if (!exec_block) return false;
            is_end = true;
        }

        if (!evaluate_block(exec, exec_block, &block_return, is_end, DATA_NOTHING)) return false;
        if (chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROLEND) {
            FuncArg bin;
            if (!evaluate_block(exec, &chain->blocks[i], &bin, false, block_return)) return false;
        }

        if (chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROL || chain->blocks[i].blockdef->type == BLOCKTYPE_CONTROLEND) {
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

typedef struct {
    unsigned int size;
    unsigned int capacity;
    char str[];
} StringHeader;

static char* string_from_literal(Gc* gc, const char* literal, unsigned int size) {
    StringHeader* out_str = gc_malloc(gc, sizeof(StringHeader) + size + 1); // Don't forget null terminator. It is not included in size
    memcpy(out_str->str, literal, size);
    out_str->size = size;
    out_str->capacity = size;
    out_str->str[size] = 0;
    return out_str->str;
}

static char* string_letter_in(Gc* gc, int target, char* input_str) {
    int pos = 0;
    if (target <= 0) return string_from_literal(gc, "", 0);
    for (char* str = input_str; *str; str++) {
        // Increment pos only on the beginning of multibyte char
        if ((*str & 0x80) == 0 || (*str & 0x40) != 0) pos++;

        if (pos == target) {
            int codepoint_size;
            GetCodepoint(str, &codepoint_size);
            return string_from_literal(gc, str, codepoint_size);
        }
    }

    return string_from_literal(gc, "", 0);
}

static char* string_substring(Gc* gc, int begin, int end, char* input_str) {
    if (begin <= 0) begin = 1;
    if (end <= 0) return string_from_literal(gc, "", 0);
    if (begin > end) return string_from_literal(gc, "", 0);

    char* substr_start = NULL;
    int substr_len = 0;

    int pos = 0;
    for (char* str = input_str; *str; str++) {
        // Increment pos only on the beginning of multibyte char
        if ((*str & 0x80) == 0 || (*str & 0x40) != 0) pos++;
        if (substr_start) substr_len++;

        if (pos == begin && !substr_start) {
            substr_start = str;
            substr_len = 1;
        }
        if (pos == end) {
            if (!substr_start) return string_from_literal(gc, "", 0);
            int codepoint_size;
            GetCodepoint(str, &codepoint_size);
            substr_len += codepoint_size - 1;

            return string_from_literal(gc, substr_start, substr_len);
        }
    }

    if (substr_start) return string_from_literal(gc, substr_start, substr_len);
    return string_from_literal(gc, "", 0);
}

static char* string_join(Gc* gc, char* left, char* right) {
    StringHeader* left_header = ((StringHeader*)left) - 1;
    StringHeader* right_header = ((StringHeader*)right) - 1;
    
    StringHeader* out_str = gc_malloc(gc, sizeof(StringHeader) + left_header->size + right_header->size + 1);
    memcpy(out_str->str, left_header->str, left_header->size);
    memcpy(out_str->str + left_header->size, right_header->str, right_header->size);
    out_str->size = left_header->size + right_header->size;
    out_str->capacity = out_str->size;
    out_str->str[out_str->size] = 0;
    return out_str->str;
}

static char* string_from_int(Gc* gc, int value) {
    char str[20];
    unsigned int len = snprintf(str, 20, "%d", value);
    return string_from_literal(gc, str, len);
}

static char* string_from_bool(Gc* gc, bool value) {
    return value ? string_from_literal(gc, "true", 4) : string_from_literal(gc, "false", 5);
}

static char* string_from_double(Gc* gc, double value) {
    char str[20];
    unsigned int len = snprintf(str, 20, "%f", value);
    return string_from_literal(gc, str, len);
}

static bool string_is_eq(char* left, char* right) {
    StringHeader* left_header = ((StringHeader*)left) - 1;
    StringHeader* right_header = ((StringHeader*)right) - 1;

    if (left_header->size != right_header->size) return false;
    for (unsigned int i = 0; i < left_header->size; i++) {
        if (left_header->str[i] != right_header->str[i]) return false;
    }
    return true;
}

static char* string_chr(Gc* gc, int value) {
    int text_size;
    const char* text = CodepointToUTF8(value, &text_size);
    return string_from_literal(gc, text, text_size);
}

static int string_ord(char* str) {
    int codepoint_size;
    int codepoint = GetCodepoint(str, &codepoint_size);
    (void) codepoint_size;
    return codepoint;
}

static int sleep_us(int usecs) {
    if (usecs < 0) return 0;

    struct timespec sleep_time = {0};
    sleep_time.tv_sec = usecs / 1000000;
    sleep_time.tv_nsec = (usecs % 1000000) * 1000;

    if (nanosleep(&sleep_time, &sleep_time) == -1) return 0;
    return usecs;
}

LLVMValueRef build_gc_root_begin(Exec* exec) {
    return build_call(exec, "gc_root_begin", CONST_GC);
}

LLVMValueRef build_gc_root_end(Exec* exec) {
    return build_call(exec, "gc_root_end", CONST_GC);
}

LLVMValueRef build_call(Exec* exec, const char* func_name, ...) {
    LLVMValueRef func = LLVMGetNamedFunction(exec->module, func_name);
    LLVMTypeRef func_type = LLVMGlobalGetValueType(func);
    unsigned int func_param_count = LLVMCountParamTypes(func_type);

    // Should be enough for all functions
    assert(func_param_count <= 32);
    LLVMValueRef func_param_list[32];

    va_list va;
    va_start(va, func_name);
    for (unsigned int i = 0; i < func_param_count; i++) {
        func_param_list[i] = va_arg(va, LLVMValueRef);
    }
    va_end(va);

    if (LLVMGetTypeKind(LLVMGetReturnType(func_type)) == LLVMVoidTypeKind) {
        return LLVMBuildCall2(exec->builder, func_type, func, func_param_list, func_param_count, "");
    } else {
        return LLVMBuildCall2(exec->builder, func_type, func, func_param_list, func_param_count, func_name);
    }
}

static unsigned int string_length(char* str) {
    StringHeader* header = ((StringHeader*)str) - 1;
    return header->size;
}

static LLVMValueRef add_function(Exec* exec, const char* name, LLVMTypeRef return_type, LLVMTypeRef* params, size_t params_len, void* func) {
    CompileFunction* comp_func = vector_add_dst(&exec->compile_func_list);
    comp_func->func = func;
    comp_func->name = name;

    LLVMTypeRef func_type = LLVMFunctionType(return_type, params, params_len, false);
    return LLVMAddFunction(exec->module, name, func_type);
}

static LLVMBasicBlockRef register_globals(Exec* exec) {
    LLVMTypeRef print_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "term_print_str", LLVMInt32Type(), print_func_params, ARRLEN(print_func_params), term_print_str);

    LLVMTypeRef print_int_func_params[] = { LLVMInt32Type() };
    add_function(exec, "term_print_int", LLVMInt32Type(), print_int_func_params, ARRLEN(print_int_func_params), term_print_int);

    LLVMTypeRef print_double_func_params[] = { LLVMDoubleType() };
    add_function(exec, "term_print_double", LLVMInt32Type(), print_double_func_params, ARRLEN(print_double_func_params), term_print_double);

    LLVMTypeRef print_bool_func_params[] = { LLVMInt1Type() };
    add_function(exec, "term_print_bool", LLVMInt32Type(), print_bool_func_params, ARRLEN(print_bool_func_params), term_print_bool);

    LLVMTypeRef string_literal_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type() };
    add_function(exec, "string_from_literal", LLVMPointerType(LLVMInt8Type(), 0), string_literal_func_params, ARRLEN(string_literal_func_params), string_from_literal);

    LLVMTypeRef string_int_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(exec, "string_from_int", LLVMPointerType(LLVMInt8Type(), 0), string_int_func_params, ARRLEN(string_int_func_params), string_from_int);

    LLVMTypeRef string_bool_func_params[] = { LLVMInt64Type(), LLVMInt1Type() };
    add_function(exec, "string_from_bool", LLVMPointerType(LLVMInt8Type(), 0), string_bool_func_params, ARRLEN(string_bool_func_params), string_from_bool);

    LLVMTypeRef string_double_func_params[] = { LLVMInt64Type(), LLVMDoubleType() };
    add_function(exec, "string_from_double", LLVMPointerType(LLVMInt8Type(), 0), string_double_func_params, ARRLEN(string_double_func_params), string_from_double);

    LLVMTypeRef string_length_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_length", LLVMInt32Type(), string_length_func_params, ARRLEN(string_length_func_params), string_length);

    LLVMTypeRef string_join_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_join", LLVMPointerType(LLVMInt8Type(), 0), string_join_func_params, ARRLEN(string_join_func_params), string_join);

    LLVMTypeRef string_ord_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_ord", LLVMInt32Type(), string_ord_func_params, ARRLEN(string_ord_func_params), string_ord);

    LLVMTypeRef string_chr_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(exec, "string_chr", LLVMPointerType(LLVMInt8Type(), 0), string_chr_func_params, ARRLEN(string_chr_func_params), string_chr);

    LLVMTypeRef string_letter_in_func_params[] = { LLVMInt64Type(), LLVMInt32Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_letter_in", LLVMPointerType(LLVMInt8Type(), 0), string_letter_in_func_params, ARRLEN(string_letter_in_func_params), string_letter_in);

    LLVMTypeRef string_substring_func_params[] = { LLVMInt64Type(), LLVMInt32Type(), LLVMInt32Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_substring", LLVMPointerType(LLVMInt8Type(), 0), string_substring_func_params, ARRLEN(string_substring_func_params), string_substring);

    LLVMTypeRef string_eq_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_is_eq", LLVMInt1Type(), string_eq_func_params, ARRLEN(string_eq_func_params), string_is_eq);

    LLVMTypeRef sleep_func_params[] = { LLVMInt32Type() };
    add_function(exec, "sleep", LLVMInt32Type(), sleep_func_params, ARRLEN(sleep_func_params), sleep_us);

    LLVMTypeRef atoi_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "atoi", LLVMInt32Type(), atoi_func_params, ARRLEN(atoi_func_params), atoi);

    LLVMTypeRef atof_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "atof", LLVMDoubleType(), atof_func_params, ARRLEN(atof_func_params), atof);

    LLVMTypeRef int_pow_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    add_function(exec, "int_pow", LLVMInt32Type(), int_pow_func_params, ARRLEN(int_pow_func_params), int_pow);

    LLVMTypeRef time_func_params[] = { LLVMPointerType(LLVMVoidType(), 0) };
    add_function(exec, "time", LLVMInt32Type(), time_func_params, ARRLEN(time_func_params), time);

    LLVMTypeRef sin_func_params[] = { LLVMDoubleType() };
    add_function(exec, "sin", LLVMDoubleType(), sin_func_params, ARRLEN(sin_func_params), sin);

    LLVMTypeRef cos_func_params[] = { LLVMDoubleType() };
    add_function(exec, "cos", LLVMDoubleType(), cos_func_params, ARRLEN(cos_func_params), cos);

    LLVMTypeRef tan_func_params[] = { LLVMDoubleType() };
    add_function(exec, "tan", LLVMDoubleType(), tan_func_params, ARRLEN(tan_func_params), tan);

    LLVMTypeRef asin_func_params[] = { LLVMDoubleType() };
    add_function(exec, "asin", LLVMDoubleType(), asin_func_params, ARRLEN(asin_func_params), asin);

    LLVMTypeRef acos_func_params[] = { LLVMDoubleType() };
    add_function(exec, "acos", LLVMDoubleType(), acos_func_params, ARRLEN(acos_func_params), acos);

    LLVMTypeRef atan_func_params[] = { LLVMDoubleType() };
    add_function(exec, "atan", LLVMDoubleType(), atan_func_params, ARRLEN(atan_func_params), atan);

    LLVMTypeRef sqrt_func_params[] = { LLVMDoubleType() };
    add_function(exec, "sqrt", LLVMDoubleType(), sqrt_func_params, ARRLEN(sqrt_func_params), sqrt);

    LLVMTypeRef round_func_params[] = { LLVMDoubleType() };
    add_function(exec, "round", LLVMDoubleType(), round_func_params, ARRLEN(round_func_params), round);

    LLVMTypeRef floor_func_params[] = { LLVMDoubleType() };
    add_function(exec, "floor", LLVMDoubleType(), floor_func_params, ARRLEN(floor_func_params), floor);

    LLVMTypeRef ceil_func_params[] = { LLVMDoubleType() };
    add_function(exec, "ceil", LLVMDoubleType(), ceil_func_params, ARRLEN(ceil_func_params), ceil);

    add_function(exec, "test_cancel", LLVMVoidType(), NULL, 0, pthread_testcancel);

    LLVMTypeRef stack_save_func_type = LLVMFunctionType(LLVMPointerType(LLVMVoidType(), 0), NULL, 0, false);
    LLVMAddFunction(exec->module, "llvm.stacksave.p0", stack_save_func_type);

    LLVMTypeRef stack_restore_func_params[] = { LLVMPointerType(LLVMVoidType(), 0) };
    LLVMTypeRef stack_restore_func_type = LLVMFunctionType(LLVMVoidType(), stack_restore_func_params, ARRLEN(stack_restore_func_params), false);
    LLVMAddFunction(exec->module, "llvm.stackrestore.p0", stack_restore_func_type);

    LLVMTypeRef gc_root_begin_func_params[] = { LLVMInt64Type() };
    add_function(exec, "gc_root_begin", LLVMVoidType(), gc_root_begin_func_params, ARRLEN(gc_root_begin_func_params), gc_root_begin);

    LLVMTypeRef gc_root_end_func_params[] = { LLVMInt64Type() };
    add_function(exec, "gc_root_end", LLVMVoidType(), gc_root_end_func_params, ARRLEN(gc_root_end_func_params), gc_root_end);

    LLVMTypeRef main_func_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, false);
    LLVMValueRef main_func = LLVMAddFunction(exec->module, "llvm_main", main_func_type);

    return LLVMAppendBasicBlock(main_func, "entry");
}

static bool compile_program(Exec* exec) {
    exec->compile_func_list = vector_create();
    exec->control_stack_len = 0;
    exec->control_data_stack_len = 0;
    exec->variable_stack_len = 0;
    exec->variable_stack_frames_len = 0;
    exec->gc = gc_new(MEMORY_LIMIT);

    exec->module = LLVMModuleCreateWithName("scrap_module");

    LLVMBasicBlockRef entry = register_globals(exec);

    exec->builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(exec->builder, entry);

    build_gc_root_begin(exec);

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        if (!evaluate_chain(exec, &exec->code[i])) {
            LLVMDisposeModule(exec->module);
            LLVMDisposeBuilder(exec->builder);
            gc_free(&exec->gc);
            vector_free(exec->compile_func_list);
            return false;
        }
    }

    build_gc_root_end(exec);

    LLVMBuildRetVoid(exec->builder);
    LLVMDisposeBuilder(exec->builder);

    char *error = NULL;
    if (LLVMVerifyModule(exec->module, LLVMPrintMessageAction, &error)) {
        TraceLog(LOG_ERROR, "[LLVM] Failed to build module!");
        LLVMDisposeMessage(error);
        LLVMDisposeModule(exec->module);
        gc_free(&exec->gc);
        vector_free(exec->compile_func_list);
        return false;
    }
    LLVMDisposeMessage(error);

    LLVMDumpModule(exec->module);

    return true;
}

static bool run_program(Exec* exec) {
    if (LLVMInitializeNativeTarget()) {
        TraceLog(LOG_ERROR, "[LLVM] Native target initialization failed!");
        LLVMDisposeModule(exec->module);
        gc_free(&exec->gc);
        vector_free(exec->compile_func_list);
        return false;
    }
    if (LLVMInitializeNativeAsmParser()) {
        TraceLog(LOG_ERROR, "[LLVM] Native asm parser initialization failed!");
        LLVMDisposeModule(exec->module);
        gc_free(&exec->gc);
        vector_free(exec->compile_func_list);
        return false;
    }
    if (LLVMInitializeNativeAsmPrinter()) {
        TraceLog(LOG_ERROR, "[LLVM] Native asm printer initialization failed!");
        LLVMDisposeModule(exec->module);
        gc_free(&exec->gc);
        vector_free(exec->compile_func_list);
        return false;
    }
    LLVMLinkInMCJIT();

    char *error = NULL;
    if (LLVMCreateExecutionEngineForModule(&exec->engine, exec->module, &error)) {
        TraceLog(LOG_ERROR, "[LLVM] Failed to create execution engine!");
        TraceLog(LOG_ERROR, "[LLVM] Error: %s", error);
        LLVMDisposeMessage(error);
        LLVMDisposeModule(exec->module);
        gc_free(&exec->gc);
        vector_free(exec->compile_func_list);
        return false;
    }

    for (size_t i = 0; i < vector_size(exec->compile_func_list); i++) {
        LLVMAddGlobalMapping(exec->engine, LLVMGetNamedFunction(exec->module, exec->compile_func_list[i].name), exec->compile_func_list[i].func);
    }

    vector_free(exec->compile_func_list);

    LLVMGenericValueRef val = LLVMRunFunction(exec->engine, LLVMGetNamedFunction(exec->module, "llvm_main"), 0, NULL);
    LLVMDisposeGenericValue(val);

    gc_free(&exec->gc);

    LLVMDisposeExecutionEngine(exec->engine);
    return true;
}
