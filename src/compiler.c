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
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

// Should be enough memory for now
#define MEMORY_LIMIT 4194304 // 4 MB

static bool compile_program(Exec* exec);
static bool run_program(Exec* exec);
static void free_defined_functions(Exec* exec);

Exec exec_new(void) {
    Exec exec = (Exec) {
        .code = NULL,
        .thread = (pthread_t) {0},
        .running_state = EXEC_STATE_NOT_RUNNING,
        .current_error_block = NULL,
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
        gc_free(&exec->gc);
        vector_free(exec->gc_dirty_funcs);
        vector_free(exec->compile_func_list);
        vector_free(exec->global_variables);
        free_defined_functions(exec);
        break;
    case STATE_PRE_EXEC:
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

    if (!compile_program(exec)) {
        pthread_exit((void*)0);
    }

    if (!run_program(exec)) {
        pthread_exit((void*)0);
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

static bool evaluate_block(Exec* exec, Block* block, FuncArg* return_val, bool end_block, FuncArg input_val) {
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
                arg = vector_add_dst(&args);
                arg->type = FUNC_ARG_BLOCKDEF;
                arg->data.blockdef = block->arguments[i].data.blockdef;
                break;
            }
        }
    }

    if (!compile_block(exec, block, vector_size(args), args, return_val)) {
        vector_free(args);
        TraceLog(LOG_ERROR, "[LLVM] Got error while compiling block id: \"%s\" (at block %p)", block->blockdef->id, block);
        return false;
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
    for (const char* str_val = str; *str_val; str_val++) vector_add(vec, *str_val == ' ' ? '_' : *str_val);
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
    vector_add(&func_name, '_');

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        switch (blockdef->inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            vector_add_str(&func_name, blockdef->inputs[i].data.text);
            vector_add(&func_name, '_');
            break;
        case INPUT_BLOCKDEF_EDITOR:
        case INPUT_DROPDOWN:
            vector_add_str(&func_name, "arg_");
            break;
        case INPUT_IMAGE_DISPLAY:
            vector_add_str(&func_name, "img_");
            break;
        case INPUT_ARGUMENT:
            func_params[func_params_count] = LLVMPointerType(LLVMInt8Type(), 0);
            func_params_blockdefs[func_params_count] = blockdef->inputs[i].data.arg.blockdef;
            func_params_count++;
            vector_add_str(&func_name, "arg_");
            break;
        }
    }
    vector_add(&func_name, 0);

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

static List* list_new(Gc* gc) {
    List* list = gc_malloc(gc, sizeof(List), FUNC_ARG_LIST);
    list->size = 0;
    list->capacity = 0;
    list->values = NULL;
    return list;
}

static AnyValueData get_any_data(FuncArgType data_type, va_list va) {
    AnyValueData data;

    switch (data_type) {
    case FUNC_ARG_BOOL:
    case FUNC_ARG_INT:
        data.int_val = va_arg(va, int);
        break;
    case FUNC_ARG_DOUBLE:
        data.double_val = va_arg(va, double);
        break;
    case FUNC_ARG_STRING_REF:
    case FUNC_ARG_STRING_LITERAL:
        data.str_val = va_arg(va, char*);
        break;
    case FUNC_ARG_LIST:
        data.list_val = va_arg(va, List*);
        break;
    default:
        break;
    }

    return data;
}

static void list_add(Gc* gc, List* list, FuncArgType data_type, ...) {
    AnyValueData data;

    va_list va;
    va_start(va, data_type);
    data = get_any_data(data_type, va);
    va_end(va);

    AnyValue value = (AnyValue) {
        .type = data_type,
        .data = data,
    };
    
    if (!list->values) {
        list->values = gc_malloc(gc, sizeof(AnyValue), 0);
        list->capacity = 1;
    }

    if (list->size >= list->capacity) {
        AnyValue* new_list = gc_malloc(gc, sizeof(AnyValue) * list->size * 2, 0);
        memcpy(new_list, list->values, sizeof(AnyValue) * list->size);
        list->values = new_list;
        list->capacity = list->size * 2;
    }

    list->values[list->size++] = value;
}

static void list_set(List* list, int index, FuncArgType data_type, ...) {
    if (index >= list->size || index < 0) return;

    AnyValueData data;

    va_list va;
    va_start(va, data_type);
    data = get_any_data(data_type, va);
    va_end(va);

    AnyValue value = (AnyValue) {
        .type = data_type,
        .data = data,
    };

    list->values[index] = value;
}

static AnyValue* list_get(Gc* gc, List* list, int index) {
    AnyValue* out = gc_malloc(gc, sizeof(AnyValue), FUNC_ARG_ANY);
    *out = (AnyValue) { .type = FUNC_ARG_NOTHING };

    if (index >= list->size || index < 0) return out;

    *out = list->values[index];
    return out;
}

static int list_length(List* list) {
    return list->size;
}

static AnyValue* any_from_value(Gc* gc, FuncArgType data_type, ...) {
    AnyValueData data;

    va_list va;
    va_start(va, data_type);
    data = get_any_data(data_type, va);
    va_end(va);

    AnyValue* value = gc_malloc(gc, sizeof(AnyValue), FUNC_ARG_ANY);
    *value = (AnyValue) {
        .type = data_type,
        .data = data,
    };
    return value;
}

static char* string_from_literal(Gc* gc, const char* literal, unsigned int size) {
    StringHeader* out_str = gc_malloc(gc, sizeof(StringHeader) + size + 1, FUNC_ARG_STRING_REF); // Don't forget null terminator. It is not included in size
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
    
    StringHeader* out_str = gc_malloc(gc, sizeof(StringHeader) + left_header->size + right_header->size + 1, FUNC_ARG_STRING_REF);
    memcpy(out_str->str, left_header->str, left_header->size);
    memcpy(out_str->str + left_header->size, right_header->str, right_header->size);
    out_str->size = left_header->size + right_header->size;
    out_str->capacity = out_str->size;
    out_str->str[out_str->size] = 0;
    return out_str->str;
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

static char* string_from_any(Gc* gc, AnyValue* value) {
    if (!value) return string_from_literal(gc, "", 0);

    switch (value->type) {
    case FUNC_ARG_INT:
        return string_from_int(gc, value->data.int_val);
    case FUNC_ARG_DOUBLE:
        return string_from_double(gc, value->data.double_val);
    case FUNC_ARG_STRING_LITERAL:
        return string_from_literal(gc, value->data.str_val, strlen(value->data.str_val));
    case FUNC_ARG_STRING_REF:
        return value->data.str_val;
    case FUNC_ARG_BOOL:
        return string_from_bool(gc, value->data.int_val);
    case FUNC_ARG_LIST: ;
        char str[32];
        int size = snprintf(str, 32, "*LIST (%zu)*", value->data.list_val->size);
        return string_from_literal(gc, str, size);
    default:
        return string_from_literal(gc, "", 0);
    }
}

static int int_from_any(AnyValue* value) {
    if (!value) return 0;

    switch (value->type) {
    case FUNC_ARG_BOOL:
    case FUNC_ARG_INT:
        return value->data.int_val;
    case FUNC_ARG_DOUBLE:
        return (int)value->data.double_val;
    case FUNC_ARG_STRING_REF:
    case FUNC_ARG_STRING_LITERAL:
        return atoi(value->data.str_val);
    default:
        return 0;
    }
}

static int double_from_any(AnyValue* value) {
    if (!value) return 0;

    switch (value->type) {
    case FUNC_ARG_BOOL:
    case FUNC_ARG_INT:
        return (double)value->data.int_val;
    case FUNC_ARG_DOUBLE:
        return value->data.double_val;
    case FUNC_ARG_STRING_REF:
    case FUNC_ARG_STRING_LITERAL:
        return atof(value->data.str_val);
    default:
        return 0;
    }
}

static int bool_from_any(AnyValue* value) {
    if (!value) return 0;

    switch (value->type) {
    case FUNC_ARG_BOOL:
    case FUNC_ARG_INT:
        return value->data.int_val != 0;
    case FUNC_ARG_DOUBLE:
        return value->data.double_val != 0.0;
    case FUNC_ARG_STRING_REF:
    case FUNC_ARG_STRING_LITERAL:
        return *value->data.str_val != 0;
    default:
        return 0;
    }
}

static List* list_from_any(Gc* gc, AnyValue* value) {
    if (!value) return 0;

    switch (value->type) {
    case FUNC_ARG_LIST:
        return value->data.list_val;
    default:
        return list_new(gc);
    }
}

static bool any_is_eq(AnyValue* left, AnyValue* right) {
    if (left->type != right->type) return false;

    switch (left->type) {
    case FUNC_ARG_NOTHING:
        return true;
    case FUNC_ARG_STRING_LITERAL:
        return !strcmp(left->data.str_val, right->data.str_val);
    case FUNC_ARG_STRING_REF:
        return string_is_eq(left->data.str_val, right->data.str_val);
    case FUNC_ARG_INT:
    case FUNC_ARG_BOOL:
        return left->data.int_val == right->data.int_val;
    case FUNC_ARG_DOUBLE:
        return left->data.double_val == right->data.double_val;
    case FUNC_ARG_LIST:
        return left->data.list_val == right->data.list_val;
    default:
        TraceLog(LOG_WARNING, "[EXEC] Comparison against unknown types in any_is_eq");
        return false;
    }
}

static int sleep_us(int usecs) {
    if (usecs < 0) return 0;

    struct timespec sleep_time = {0};
    sleep_time.tv_sec = usecs / 1000000;
    sleep_time.tv_nsec = (usecs % 1000000) * 1000;

    if (nanosleep(&sleep_time, &sleep_time) == -1) return 0;
    return usecs;
}

static int get_random(int min, int max) {
    if (min > max) {
        return GetRandomValue(max, min);
    } else {
        return GetRandomValue(min, max);
    }
}

static char* term_get_char(Gc* gc) {
    char input[10];
    input[0] = term_input_get_char();
    int mb_size = leading_ones(input[0]);

    if (mb_size == 0) mb_size = 1;
    for (int i = 1; i < mb_size && i < 10; i++) input[i] = term_input_get_char();
    input[mb_size] = 0;

    return string_from_literal(gc, input, mb_size);
}

static void term_set_cursor(int x, int y) {
    pthread_mutex_lock(&term.lock);
    x = CLAMP(x, 0, term.char_w - 1);
    y = CLAMP(y, 0, term.char_h - 1);
    term.cursor_pos = x + y * term.char_w;
    pthread_mutex_unlock(&term.lock);
}

static int term_cursor_x(void) {
    pthread_mutex_lock(&term.lock);
    int cur_x = 0;
    if (term.char_w != 0) cur_x = term.cursor_pos % term.char_w;
    pthread_mutex_unlock(&term.lock);
    return cur_x;
}

static int term_cursor_y(void) {
    pthread_mutex_lock(&term.lock);
    int cur_y = 0;
    if (term.char_w != 0) cur_y = term.cursor_pos / term.char_w;
    pthread_mutex_unlock(&term.lock);
    return cur_y;
}

static int term_cursor_max_x(void) {
    pthread_mutex_lock(&term.lock);
    int cur_max_x = term.char_w;
    pthread_mutex_unlock(&term.lock);
    return cur_max_x;
}

static int term_cursor_max_y(void) {
    pthread_mutex_lock(&term.lock);
    int cur_max_y = term.char_h;
    pthread_mutex_unlock(&term.lock);
    return cur_max_y;
}

static char* term_get_input(Gc* gc) {
    char input_char = 0;
    char* out_string = NULL;

    while (input_char != '\n') {
        char input[256];
        int i = 0;
        for (; i < 255 && input_char != '\n'; i++) input[i] = (input_char = term_input_get_char());
        if (input[i - 1] == '\n') input[i - 1] = 0;
        input[i] = 0;

        if (!out_string) {
            out_string = string_from_literal(gc, input, i);
        } else {
            out_string = string_join(gc, out_string, string_from_literal(gc, input, i));
        }
    }

    return out_string;
}

int term_print_list(List* list) {
    char converted[32];
    snprintf(converted, 32, "*LIST (%zu)*", list->size);
    return term_print_str(converted);
}

int term_print_any(AnyValue* any) {
    if (!any) return 0;

    switch (any->type) {
    case FUNC_ARG_STRING_REF:
    case FUNC_ARG_STRING_LITERAL:
        return term_print_str(any->data.str_val);
    case FUNC_ARG_NOTHING:
        return 0;
    case FUNC_ARG_INT:
        return term_print_int(any->data.int_val);
    case FUNC_ARG_BOOL:
        return term_print_bool(any->data.int_val);
    case FUNC_ARG_DOUBLE:
        return term_print_double(any->data.double_val);
    case FUNC_ARG_LIST:
        return term_print_list(any->data.list_val);
    default:
        TraceLog(LOG_WARNING, "[EXEC] Got unknown type in term_print_any");
        return 0;
    }
}

LLVMValueRef build_gc_root_begin(Exec* exec) {
    return build_call(exec, "gc_root_begin", CONST_GC);
}

LLVMValueRef build_gc_root_end(Exec* exec) {
    return build_call(exec, "gc_root_end", CONST_GC);
}

static LLVMValueRef build_call_va(Exec* exec, const char* func_name, LLVMValueRef func, LLVMTypeRef func_type, size_t func_param_count, va_list va) {
    for (size_t i = 0; i < vector_size(exec->gc_dirty_funcs); i++) {
        if (func != exec->gc_dirty_funcs[i]) continue;
        exec->gc_dirty = true;
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
    LLVMValueRef func = LLVMGetNamedFunction(exec->module, func_name);
    LLVMTypeRef func_type = LLVMGlobalGetValueType(func);
    LLVMValueRef out;

    va_list va;
    va_start(va, func_param_count);
    out = build_call_va(exec, func_name, func, func_type, func_param_count, va);
    va_end(va);

    return out;
}

LLVMValueRef build_call(Exec* exec, const char* func_name, ...) {
    LLVMValueRef func = LLVMGetNamedFunction(exec->module, func_name);
    LLVMTypeRef func_type = LLVMGlobalGetValueType(func);
    unsigned int func_param_count = LLVMCountParamTypes(func_type);
    LLVMValueRef out;

    va_list va;
    va_start(va, func_name);
    out = build_call_va(exec, func_name, func, func_type, func_param_count, va);
    va_end(va);

    return out;
}

static unsigned int string_length(char* str) {
    StringHeader* header = ((StringHeader*)str) - 1;
    return header->size;
}

// Dynamic means the func calls gc_malloc at some point. This is needed for gc.temp_roots cleanup
static LLVMValueRef add_function(Exec* exec, const char* name, LLVMTypeRef return_type, LLVMTypeRef* params, size_t params_len, void* func, bool dynamic, bool variadic) {
    CompileFunction* comp_func = vector_add_dst(&exec->compile_func_list);
    comp_func->func = func;
    comp_func->name = name;

    LLVMTypeRef func_type = LLVMFunctionType(return_type, params, params_len, variadic);
    LLVMValueRef func_value = LLVMAddFunction(exec->module, name, func_type);

    if (dynamic) vector_add(&exec->gc_dirty_funcs, func_value);
    return func_value;
}

static LLVMValueRef register_globals(Exec* exec) {
    LLVMTypeRef print_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "term_print_str", LLVMInt32Type(), print_func_params, ARRLEN(print_func_params), term_print_str, false, false);

    LLVMTypeRef print_int_func_params[] = { LLVMInt32Type() };
    add_function(exec, "term_print_int", LLVMInt32Type(), print_int_func_params, ARRLEN(print_int_func_params), term_print_int, false, false);

    LLVMTypeRef print_double_func_params[] = { LLVMDoubleType() };
    add_function(exec, "term_print_double", LLVMInt32Type(), print_double_func_params, ARRLEN(print_double_func_params), term_print_double, false, false);

    LLVMTypeRef print_bool_func_params[] = { LLVMInt1Type() };
    add_function(exec, "term_print_bool", LLVMInt32Type(), print_bool_func_params, ARRLEN(print_bool_func_params), term_print_bool, false, false);

    LLVMTypeRef print_list_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "term_print_list", LLVMInt32Type(), print_list_func_params, ARRLEN(print_list_func_params), term_print_list, false, false);

    LLVMTypeRef print_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "term_print_any", LLVMInt32Type(), print_any_func_params, ARRLEN(print_any_func_params), term_print_any, false, false);

    LLVMTypeRef string_literal_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type() };
    add_function(exec, "string_from_literal", LLVMPointerType(LLVMInt8Type(), 0), string_literal_func_params, ARRLEN(string_literal_func_params), string_from_literal, true, false);

    LLVMTypeRef string_int_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(exec, "string_from_int", LLVMPointerType(LLVMInt8Type(), 0), string_int_func_params, ARRLEN(string_int_func_params), string_from_int, true, false);

    LLVMTypeRef string_bool_func_params[] = { LLVMInt64Type(), LLVMInt1Type() };
    add_function(exec, "string_from_bool", LLVMPointerType(LLVMInt8Type(), 0), string_bool_func_params, ARRLEN(string_bool_func_params), string_from_bool, true, false);

    LLVMTypeRef string_double_func_params[] = { LLVMInt64Type(), LLVMDoubleType() };
    add_function(exec, "string_from_double", LLVMPointerType(LLVMInt8Type(), 0), string_double_func_params, ARRLEN(string_double_func_params), string_from_double, true, false);

    LLVMTypeRef string_any_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_from_any", LLVMPointerType(LLVMInt8Type(), 0), string_any_func_params, ARRLEN(string_any_func_params), string_from_any, true, false);

    LLVMTypeRef int_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "int_from_any", LLVMInt32Type(), int_any_func_params, ARRLEN(int_any_func_params), int_from_any, false, false);

    LLVMTypeRef double_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "double_from_any", LLVMDoubleType(), double_any_func_params, ARRLEN(double_any_func_params), double_from_any, false, false);

    LLVMTypeRef bool_any_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "bool_from_any", LLVMInt1Type(), bool_any_func_params, ARRLEN(bool_any_func_params), bool_from_any, false, false);

    LLVMTypeRef list_any_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "list_from_any", LLVMPointerType(LLVMInt8Type(), 0), list_any_func_params, ARRLEN(list_any_func_params), list_from_any, true, false);

    LLVMTypeRef any_cast_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(exec, "any_from_value", LLVMPointerType(LLVMInt8Type(), 0), any_cast_func_params, ARRLEN(any_cast_func_params), any_from_value, true, true);

    LLVMTypeRef string_length_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_length", LLVMInt32Type(), string_length_func_params, ARRLEN(string_length_func_params), string_length, false, false);

    LLVMTypeRef string_join_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_join", LLVMPointerType(LLVMInt8Type(), 0), string_join_func_params, ARRLEN(string_join_func_params), string_join, true, false);

    LLVMTypeRef string_ord_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_ord", LLVMInt32Type(), string_ord_func_params, ARRLEN(string_ord_func_params), string_ord, false, false);

    LLVMTypeRef string_chr_func_params[] = { LLVMInt64Type(), LLVMInt32Type() };
    add_function(exec, "string_chr", LLVMPointerType(LLVMInt8Type(), 0), string_chr_func_params, ARRLEN(string_chr_func_params), string_chr, false, false);

    LLVMTypeRef string_letter_in_func_params[] = { LLVMInt64Type(), LLVMInt32Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_letter_in", LLVMPointerType(LLVMInt8Type(), 0), string_letter_in_func_params, ARRLEN(string_letter_in_func_params), string_letter_in, true, false);

    LLVMTypeRef string_substring_func_params[] = { LLVMInt64Type(), LLVMInt32Type(), LLVMInt32Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_substring", LLVMPointerType(LLVMInt8Type(), 0), string_substring_func_params, ARRLEN(string_substring_func_params), string_substring, true, false);

    LLVMTypeRef string_eq_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "string_is_eq", LLVMInt1Type(), string_eq_func_params, ARRLEN(string_eq_func_params), string_is_eq, false, false);

    LLVMTypeRef any_eq_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "any_is_eq", LLVMInt1Type(), any_eq_func_params, ARRLEN(any_eq_func_params), any_is_eq, false, false);

    LLVMTypeRef sleep_func_params[] = { LLVMInt32Type() };
    add_function(exec, "sleep", LLVMInt32Type(), sleep_func_params, ARRLEN(sleep_func_params), sleep_us, false, false);

    LLVMTypeRef random_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    add_function(exec, "random", LLVMInt32Type(), random_func_params, ARRLEN(random_func_params), get_random, false, false);

    LLVMTypeRef atoi_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "atoi", LLVMInt32Type(), atoi_func_params, ARRLEN(atoi_func_params), atoi, false, false);

    LLVMTypeRef atof_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "atof", LLVMDoubleType(), atof_func_params, ARRLEN(atof_func_params), atof, false, false);

    LLVMTypeRef int_pow_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    add_function(exec, "int_pow", LLVMInt32Type(), int_pow_func_params, ARRLEN(int_pow_func_params), int_pow, false, false);

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
    add_function(exec, "get_char", LLVMPointerType(LLVMInt8Type(), 0), get_char_func_params, ARRLEN(get_char_func_params), term_get_char, true, false);

    LLVMTypeRef get_input_func_params[] = { LLVMInt64Type() };
    add_function(exec, "get_input", LLVMPointerType(LLVMInt8Type(), 0), get_input_func_params, ARRLEN(get_input_func_params), term_get_input, true, false);

    LLVMTypeRef set_clear_color_func_params[] = { LLVMInt32Type() };
    add_function(exec, "set_clear_color", LLVMVoidType(), set_clear_color_func_params, ARRLEN(set_clear_color_func_params), term_set_clear_color, false, false);

    LLVMTypeRef set_fg_color_func_params[] = { LLVMInt32Type() };
    add_function(exec, "set_fg_color", LLVMVoidType(), set_fg_color_func_params, ARRLEN(set_fg_color_func_params), term_set_fg_color, false, false);

    LLVMTypeRef set_bg_color_func_params[] = { LLVMInt32Type() };
    add_function(exec, "set_bg_color", LLVMVoidType(), set_bg_color_func_params, ARRLEN(set_bg_color_func_params), term_set_bg_color, false, false);

    LLVMTypeRef set_cursor_func_params[] = { LLVMInt32Type(), LLVMInt32Type() };
    add_function(exec, "set_cursor", LLVMVoidType(), set_cursor_func_params, ARRLEN(set_cursor_func_params), term_set_cursor, false, false);

    add_function(exec, "cursor_x", LLVMInt32Type(), NULL, 0, term_cursor_x, false, false);
    add_function(exec, "cursor_y", LLVMInt32Type(), NULL, 0, term_cursor_y, false, false);
    add_function(exec, "cursor_max_x", LLVMInt32Type(), NULL, 0, term_cursor_max_x, false, false);
    add_function(exec, "cursor_max_y", LLVMInt32Type(), NULL, 0, term_cursor_max_y, false, false);

    add_function(exec, "term_clear", LLVMVoidType(), NULL, 0, term_clear, false, false);

    LLVMTypeRef list_new_func_params[] = { LLVMInt64Type() };
    add_function(exec, "list_new", LLVMPointerType(LLVMInt8Type(), 0), list_new_func_params, ARRLEN(list_new_func_params), list_new, true, false);

    LLVMTypeRef list_add_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type() };
    add_function(exec, "list_add", LLVMVoidType(), list_add_func_params, ARRLEN(list_add_func_params), list_add, true, true);

    LLVMTypeRef list_get_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type() };
    add_function(exec, "list_get", LLVMPointerType(LLVMInt8Type(), 0), list_get_func_params, ARRLEN(list_get_func_params), list_get, true, false);

    LLVMTypeRef list_set_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type(), LLVMInt32Type() };
    add_function(exec, "list_set", LLVMPointerType(LLVMInt8Type(), 0), list_set_func_params, ARRLEN(list_set_func_params), list_set, false, true);

    LLVMTypeRef list_length_func_params[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "list_length", LLVMInt32Type(), list_length_func_params, ARRLEN(list_length_func_params), list_length, false, false);

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

    LLVMTypeRef gc_collect_func_params[] = { LLVMInt64Type() };
    add_function(exec, "gc_collect", LLVMVoidType(), gc_collect_func_params, ARRLEN(gc_collect_func_params), gc_collect, false, false);

    LLVMTypeRef gc_add_str_root_func_params[] = { LLVMInt64Type(), LLVMPointerType(LLVMInt8Type(), 0) };
    add_function(exec, "gc_add_str_root", LLVMVoidType(), gc_add_str_root_func_params, ARRLEN(gc_add_str_root_func_params), gc_add_str_root, false, false);

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
    exec->control_stack_len = 0;
    exec->control_data_stack_len = 0;
    exec->variable_stack_len = 0;
    exec->variable_stack_frames_len = 0;
    exec->gc = gc_new(MEMORY_LIMIT);
    exec->gc_dirty = false;
    exec->gc_dirty_funcs = vector_create();
    exec->defined_functions = vector_create();
    exec->current_state = STATE_COMPILE;

    exec->module = LLVMModuleCreateWithName("scrap_module");

    LLVMValueRef main_func = register_globals(exec);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(main_func, "entry");

    exec->builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(exec->builder, entry);

    build_gc_root_begin(exec);

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        if (strcmp(exec->code[i].blocks[0].blockdef->id, "on_start")) continue;
        if (!evaluate_chain(exec, &exec->code[i])) {
            return false;
        }
    }

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        if (!strcmp(exec->code[i].blocks[0].blockdef->id, "on_start")) continue;
        if (!evaluate_chain(exec, &exec->code[i])) {
            return false;
        }
    }

    LLVMBasicBlockRef last_block = LLVMGetLastBasicBlock(main_func);
    LLVMPositionBuilderAtEnd(exec->builder, last_block);

    build_gc_root_end(exec);
    LLVMBuildRetVoid(exec->builder);

    for (size_t i = 0; i < vector_size(exec->defined_functions); i++) {
        last_block = LLVMGetLastBasicBlock(exec->defined_functions[i].func);
        LLVMPositionBuilderAtEnd(exec->builder, last_block);
        build_gc_root_end(exec);
        LLVMValueRef val = build_call_count(exec, "any_from_value", 2, CONST_GC, CONST_INTEGER(FUNC_ARG_NOTHING));
        LLVMBuildRet(exec->builder, val);
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

static bool run_program(Exec* exec) {
    exec->current_state = STATE_PRE_EXEC;

    if (LLVMInitializeNativeTarget()) {
        exec_set_error(exec, NULL, "[LLVM] Native target initialization failed");
        LLVMDisposeModule(exec->module);
        gc_free(&exec->gc);
        vector_free(exec->compile_func_list);
        return false;
    }
    if (LLVMInitializeNativeAsmParser()) {
        exec_set_error(exec, NULL, "[LLVM] Native asm parser initialization failed");
        LLVMDisposeModule(exec->module);
        gc_free(&exec->gc);
        vector_free(exec->compile_func_list);
        return false;
    }
    if (LLVMInitializeNativeAsmPrinter()) {
        exec_set_error(exec, NULL, "[LLVM] Native asm printer initialization failed");
        LLVMDisposeModule(exec->module);
        gc_free(&exec->gc);
        vector_free(exec->compile_func_list);
        return false;
    }
    LLVMLinkInMCJIT();

    char *error = NULL;
    if (LLVMCreateExecutionEngineForModule(&exec->engine, exec->module, &error)) {
        exec_set_error(exec, NULL, "[LLVM] Failed to create execution engine: %s", error);
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

    exec->current_state = STATE_EXEC;

    LLVMGenericValueRef val = LLVMRunFunction(exec->engine, LLVMGetNamedFunction(exec->module, "llvm_main"), 0, NULL);
    LLVMDisposeGenericValue(val);

    return true;
}
