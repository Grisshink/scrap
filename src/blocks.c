// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-202 Grisshink
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

#include "term.h"
#include "scrap.h"
#include "vec.h"
#include "util.h"
#include "std.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <libintl.h>

#define MATH_LIST_LEN 10
#define TERM_COLOR_LIST_LEN 8

typedef struct {
    char* str;
    size_t len;
    size_t cap;
} String;

typedef double (*MathFunc)(double);

char* block_math_list[MATH_LIST_LEN] = {
    "sqrt", "round", "floor", "ceil",
    "sin", "cos", "tan",
    "asin", "acos", "atan",
};

char** math_list_access(Block* block, size_t* list_len) {
    (void) block;
    *list_len = MATH_LIST_LEN;
    return block_math_list;
}

#ifndef USE_LLVM

static MathFunc block_math_func_list[MATH_LIST_LEN] = {
    sqrt, round, floor, ceil,
    sin, cos, tan,
    asin, acos, atan,
};

#include "std.h"

bool any_to_string(Compiler* compiler, Block* block, AnyValue value) {
    switch (value.type) {
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(DATA_TYPE_STRING));
        return false;
    case DATA_TYPE_STRING: return true;
    case DATA_TYPE_FLOAT: bytecode_push_op(&compiler->bytecode, IR_FTOA); return true;
    case DATA_TYPE_NOTHING: bytecode_push_op(&compiler->bytecode, IR_NTOA); return true;
    case DATA_TYPE_BOOL: bytecode_push_op(&compiler->bytecode, IR_BTOA); return true;
    case DATA_TYPE_LIST: bytecode_push_op(&compiler->bytecode, IR_LTOA); return true;
    case DATA_TYPE_ANY: bytecode_push_op(&compiler->bytecode, IR_TOA); return true;
    case DATA_TYPE_INTEGER:
        bytecode_push_op(&compiler->bytecode, IR_ITOA);
        return true;
    case DATA_TYPE_COLOR:
        bytecode_push_op(&compiler->bytecode, IR_ITOA);
        return true;
    case DATA_TYPE_LITERAL:
        IrList* list = bytecode_const_list_new();
        char* str = value.data.literal_val;
        int char_size;
        while (*str) {
            IrValue val;
            val.type = IR_TYPE_INT;
            val.as.int_val = GetCodepoint(str, &char_size);
            bytecode_const_list_append(list, val);
            str += char_size;
        }
        bytecode_push_op_list(&compiler->bytecode, IR_PUSHL, list);
        return true;
    }
    assert(false && "Unimplemented any_to_string");
}

bool any_to_integer(Compiler* compiler, Block* block, AnyValue value) {
    switch (value.type) {
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_LIST:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(DATA_TYPE_INTEGER));
        return false;
    case DATA_TYPE_STRING: bytecode_push_op(&compiler->bytecode, IR_ATOI); return true;
    case DATA_TYPE_FLOAT: bytecode_push_op(&compiler->bytecode, IR_FTOI); return true;
    case DATA_TYPE_BOOL: bytecode_push_op(&compiler->bytecode, IR_BTOI); return true;
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_ANY: 
        bytecode_push_op(&compiler->bytecode, IR_TOI);
        return true;
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_COLOR:
        return true;
    case DATA_TYPE_LITERAL:
        bytecode_push_op_int(&compiler->bytecode, IR_PUSHI, atol(value.data.literal_val));
        return true;
    }
    assert(false && "Unimplemented any_to_integer");
}

bool any_to_float(Compiler* compiler, Block* block, AnyValue value) {
    switch (value.type) {
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_LIST:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(DATA_TYPE_FLOAT));
        return false;
    case DATA_TYPE_STRING: bytecode_push_op(&compiler->bytecode, IR_ATOF); return true;
    case DATA_TYPE_FLOAT: return true;
    case DATA_TYPE_BOOL: bytecode_push_op(&compiler->bytecode, IR_BTOF); return true;
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_ANY:
        bytecode_push_op(&compiler->bytecode, IR_TOF);
        return true;
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_COLOR:
        bytecode_push_op(&compiler->bytecode, IR_ITOF);
        return true;
    case DATA_TYPE_LITERAL:
        bytecode_push_op_float(&compiler->bytecode, IR_PUSHF, atof(value.data.literal_val));
        return true;
    }
    assert(false && "Unimplemented any_to_float");
}

bool any_to_bool(Compiler* compiler, Block* block, AnyValue value) {
    switch (value.type) {
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_LIST:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(DATA_TYPE_BOOL));
        return false;
    case DATA_TYPE_STRING: bytecode_push_op(&compiler->bytecode, IR_ATOB); return true;
    case DATA_TYPE_FLOAT: bytecode_push_op(&compiler->bytecode, IR_FTOB); return true;
    case DATA_TYPE_BOOL: return true;
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_ANY:
        bytecode_push_op(&compiler->bytecode, IR_TOB);
        return true;
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_COLOR:
        bytecode_push_op(&compiler->bytecode, IR_ITOB);
        return true;
    case DATA_TYPE_LITERAL:
        bytecode_push_op_bool(&compiler->bytecode, IR_PUSHB, *value.data.literal_val);
        return true;
    }
    assert(false && "Unimplemented any_to_bool");
}

bool any_to_color(Compiler* compiler, Block* block, AnyValue value) {
    switch (value.type) {
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_LIST:
    case DATA_TYPE_BOOL:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(DATA_TYPE_COLOR));
        return false;
    case DATA_TYPE_STRING: bytecode_push_op(&compiler->bytecode, IR_ATOI); return true;
    case DATA_TYPE_FLOAT: bytecode_push_op(&compiler->bytecode, IR_FTOI); return true;
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_ANY:
        bytecode_push_op(&compiler->bytecode, IR_TOI);
        return true;
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_COLOR:
        return true;
    case DATA_TYPE_LITERAL:
        bytecode_push_op_int(&compiler->bytecode, IR_PUSHI, atol(value.data.literal_val));
        return true;
    }
    assert(false && "Unimplemented any_to_color");
}

bool any_to_list(Compiler* compiler, Block* block, AnyValue value) {
    switch (value.type) {
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_BOOL:
    case DATA_TYPE_FLOAT: 
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_COLOR:
    case DATA_TYPE_LITERAL:
    case DATA_TYPE_ANY: // TODO: Add tol instruction
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(DATA_TYPE_COLOR));
        return false;
    case DATA_TYPE_LIST:
    case DATA_TYPE_STRING:
        return true;
    }
    assert(false && "Unimplemented any_to_list");
}

bool any_cast(Compiler* compiler, Block* block, AnyValue value, DataType type) {
    if (type == value.type) return true;

    switch (type) {
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_LITERAL: 
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(type));
        return false;
    case DATA_TYPE_BOOL: return any_to_bool(compiler, block, value);
    case DATA_TYPE_FLOAT: return any_to_float(compiler, block, value); 
    case DATA_TYPE_INTEGER: return any_to_integer(compiler, block, value);
    case DATA_TYPE_COLOR: return any_to_color(compiler, block, value);
    case DATA_TYPE_ANY: return true;
    case DATA_TYPE_LIST: return any_to_list(compiler, block, value);
    case DATA_TYPE_STRING: return any_to_string(compiler, block, value);
    }
    assert(false && "Unimplemented any_cast");
}

#define GUARD(_val) if (!_val) return DATA_UNKNOWN
#define EVALUATE(_arg, _val) GUARD(compiler_evaluate_argument(compiler, (_arg), _val))
#define TO_STRING(_val) GUARD(any_to_string(compiler, block, _val))
#define TO_INTEGER(_val) GUARD(any_to_integer(compiler, block, _val))
#define TO_FLOAT(_val) GUARD(any_to_float(compiler, block, _val))
#define TO_BOOL(_val) GUARD(any_to_bool(compiler, block, _val))
#define TO_COLOR(_val) GUARD(any_to_color(compiler, block, _val))
#define TO_LIST(_val) GUARD(any_to_list(compiler, block, _val))
#define CAST(_val, _type) GUARD(any_cast(compiler, block, _val, _type))

AnyValue evaluate_binary(Compiler* compiler, Block* block, DataType literal_cast) {
    AnyValue left, right;

    EVALUATE(&block->arguments[0], &left);
    if (left.type == DATA_TYPE_LITERAL) {
        CAST(left, literal_cast);
        left.type = literal_cast;
    }

    EVALUATE(&block->arguments[1], &right);
    CAST(right, left.type);
    right.type = left.type;

    if (left.type != right.type) {
        compiler_set_error(compiler, block, gettext("Incompatible types %s and %s in binary block"), type_to_str(left.type), type_to_str(right.type));
        return DATA_UNKNOWN;
    }

    switch (left.type) {
    case DATA_TYPE_INTEGER: return DATA_INTEGER;
    case DATA_TYPE_STRING:  return DATA_STRING;
    case DATA_TYPE_LIST:    return DATA_LIST;
    case DATA_TYPE_FLOAT:   return DATA_FLOAT;
    case DATA_TYPE_BOOL:    return DATA_BOOL;
    case DATA_TYPE_COLOR:   return DATA_COLOR;
    case DATA_TYPE_NOTHING: return DATA_NOTHING;
    case DATA_TYPE_LITERAL:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_ANY: 
        compiler_set_error(compiler, block, gettext("Invalid type %s in binary block"), type_to_str(left.type));
        return DATA_UNKNOWN;
    }
    assert(false && "Unimplemented evaluate_binary");
}

AnyValue evaluate_number_operation(Compiler* compiler, Block* block, IrOpcode int_op, IrOpcode float_op) {
    AnyValue val = evaluate_binary(compiler, block, DATA_TYPE_INTEGER);
    if (val.type == DATA_TYPE_UNKNOWN) return val;

    switch (val.type) {
    case DATA_TYPE_INTEGER: bytecode_push_op(&compiler->bytecode, int_op); break;
    case DATA_TYPE_FLOAT:   bytecode_push_op(&compiler->bytecode, float_op); break;
    default:
        compiler_set_error(compiler, block, gettext("Invalid type %s in operator block"), type_to_str(val.type));
        return DATA_UNKNOWN;
    }

    return val;
}

AnyValue block_do_nothing(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_NOTHING;
}

AnyValue block_noop(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_NOTHING;
}

AnyValue block_on_start(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_NOTHING;
}

AnyValue block_define_block(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_loop(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_if(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_else_if(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_else(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_repeat(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_while(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_sleep(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_declare_var(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_get_var(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_set_var(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_create_list(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_list_add(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_list_get(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_list_length(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_list_set(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_print(Compiler* compiler, Block* block) {
    AnyValue print_val;
    EVALUATE(&block->arguments[0], &print_val);
    TO_STRING(print_val);
    bytecode_push_op_func(&compiler->bytecode, IR_RUN, ir_func_by_hint("print_str"));
    return DATA_NOTHING;
}

AnyValue block_println(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_cursor_x(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_cursor_y(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_cursor_max_x(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_cursor_max_y(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_set_cursor(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_set_fg_color(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_set_bg_color(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_reset_color(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_term_clear(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_term_set_clear(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_input(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_get_char(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_random(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_join(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_ord(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_chr(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_letter_in(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_substring(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_length(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_unix_time(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_convert_int(Compiler* compiler, Block* block) {
    AnyValue val;
    EVALUATE(&block->arguments[0], &val);
    TO_INTEGER(val);
    return DATA_INTEGER;
}

AnyValue block_convert_float(Compiler* compiler, Block* block) {
    AnyValue val;
    EVALUATE(&block->arguments[0], &val);
    TO_FLOAT(val);
    return DATA_FLOAT;
}

AnyValue block_convert_str(Compiler* compiler, Block* block) {
    AnyValue val;
    EVALUATE(&block->arguments[0], &val);
    TO_STRING(val);
    return DATA_STRING;
}

AnyValue block_convert_bool(Compiler* compiler, Block* block) {
    AnyValue val;
    EVALUATE(&block->arguments[0], &val);
    TO_BOOL(val);
    return DATA_BOOL;
}

AnyValue block_convert_color(Compiler* compiler, Block* block) {
    AnyValue val;
    EVALUATE(&block->arguments[0], &val);
    TO_COLOR(val);
    return DATA_COLOR;
}

AnyValue block_typeof(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_plus(Compiler* compiler, Block* block) {
    return evaluate_number_operation(compiler, block, IR_ADDI, IR_ADDF);
}

AnyValue block_minus(Compiler* compiler, Block* block) {
    return evaluate_number_operation(compiler, block, IR_SUBI, IR_SUBF);
}

AnyValue block_mult(Compiler* compiler, Block* block) {
    return evaluate_number_operation(compiler, block, IR_MULI, IR_MULF);
}

AnyValue block_div(Compiler* compiler, Block* block) {
    return evaluate_number_operation(compiler, block, IR_DIVI, IR_DIVF);
}

AnyValue block_rem(Compiler* compiler, Block* block) {
    return evaluate_number_operation(compiler, block, IR_MODI, IR_MODF);
}

AnyValue block_pow(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_math(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_pi(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_bit_not(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_bit_and(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_bit_xor(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_bit_or(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_less(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_less_eq(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_more(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_more_eq(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_not(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_and(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_or(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_true(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_false(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_eq(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_not_eq(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_exec_custom(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_custom_arg(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_return(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

AnyValue block_gc_collect(Compiler* compiler, Block* block) {
    (void) compiler;
    (void) block;
    return DATA_UNKNOWN;
}

#else

#define MIN_ARG_COUNT(count) \
    if (argc < count) { \
        scrap_log(LOG_ERROR, "Not enough arguments! Expected: %d or more, Got: %d", count, argc); \
        return false; \
    }

LLVMValueRef arg_to_value(Compiler* compiler, Block* block, FuncArg arg) {
    switch (arg.type) {
    case DATA_TYPE_LITERAL:
        return CONST_STRING_LITERAL(arg.data.str);
    case DATA_TYPE_LIST:
    case DATA_TYPE_STRING:
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_FLOAT:
    case DATA_TYPE_BOOL:
    case DATA_TYPE_COLOR:
    case DATA_TYPE_ANY:
        return arg.data.value;
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
        compiler_set_error(compiler, block, gettext("Cannot represent %s as LLVM value"), type_to_str(arg.type));
        return NULL;
    }
    assert(false && "Unhandled arg_to_value");
}

LLVMValueRef arg_to_bool(Compiler* compiler, Block* block, FuncArg arg) {
    switch (arg.type) {
    case DATA_TYPE_LITERAL:
        return CONST_BOOLEAN(*arg.data.str != 0);
    case DATA_TYPE_STRING: ;
        LLVMValueRef first_char = LLVMBuildLoad2(compiler->builder, LLVMInt8Type(), build_call(compiler, "std_string_get_data", arg.data.value), "bool_cast");
        return LLVMBuildICmp(compiler->builder, LLVMIntNE, first_char, LLVMConstInt(LLVMInt8Type(), 0, true), "bool_cast");
    case DATA_TYPE_LIST:
    case DATA_TYPE_NOTHING:
        return CONST_BOOLEAN(0);
    case DATA_TYPE_INTEGER:
        return LLVMBuildICmp(compiler->builder, LLVMIntNE, arg.data.value, CONST_INTEGER(0), "bool_cast");
    case DATA_TYPE_BOOL:
        return arg.data.value;
    case DATA_TYPE_FLOAT:
        return LLVMBuildFCmp(compiler->builder, LLVMRealONE, arg.data.value, CONST_FLOAT(0.0), "bool_cast");
    case DATA_TYPE_ANY:
        return build_call(compiler, "std_bool_from_any", arg.data.value);
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_COLOR:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(arg.type), type_to_str(DATA_TYPE_BOOL));
        return NULL;
    }
    assert(false && "Unhandled cast to bool");
}

LLVMValueRef arg_to_integer(Compiler* compiler, Block* block, FuncArg arg) {
    switch (arg.type) {
    case DATA_TYPE_LITERAL:
        return CONST_INTEGER(atoi(arg.data.str));
    case DATA_TYPE_STRING:
        return build_call(compiler, "atoi", build_call(compiler, "std_string_get_data", arg.data.value));
    case DATA_TYPE_LIST:
    case DATA_TYPE_NOTHING:
        return CONST_INTEGER(0);
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_COLOR:
        return arg.data.value;
    case DATA_TYPE_BOOL:
        return LLVMBuildZExt(compiler->builder, arg.data.value, LLVMInt32Type(), "int_cast");
    case DATA_TYPE_FLOAT:
        return LLVMBuildFPToSI(compiler->builder, arg.data.value, LLVMInt32Type(), "int_cast");
    case DATA_TYPE_ANY:
        return build_call(compiler, "std_integer_from_any", arg.data.value);
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(arg.type), type_to_str(DATA_TYPE_INTEGER));
        return NULL;
    }
    assert(false && "Unhandled cast to integer");
}

LLVMValueRef arg_to_float(Compiler* compiler, Block* block, FuncArg arg) {
    switch (arg.type) {
    case DATA_TYPE_LITERAL:
        return CONST_FLOAT(atof(arg.data.str));
    case DATA_TYPE_STRING:
        return build_call(compiler, "atof", build_call(compiler, "std_string_get_data", arg.data.value));
    case DATA_TYPE_LIST:
    case DATA_TYPE_NOTHING:
        return CONST_FLOAT(0.0);
    case DATA_TYPE_INTEGER:
        return LLVMBuildSIToFP(compiler->builder, arg.data.value, LLVMDoubleType(), "float_cast");
    case DATA_TYPE_BOOL:
        return LLVMBuildUIToFP(compiler->builder, arg.data.value, LLVMDoubleType(), "float_cast");
    case DATA_TYPE_FLOAT:
        return arg.data.value;
    case DATA_TYPE_ANY:
        return build_call(compiler, "std_float_from_any", arg.data.value);
    case DATA_TYPE_COLOR:
        return LLVMBuildSIToFP(compiler->builder, arg.data.value, LLVMDoubleType(), "float_cast");
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(arg.type), type_to_str(DATA_TYPE_FLOAT));
        return NULL;
    }
    assert(false && "Unhandled cast to float");
}

LLVMValueRef arg_to_any_string(Compiler* compiler, Block* block, FuncArg arg) {
    switch (arg.type) {
    case DATA_TYPE_LITERAL:
        return CONST_STRING_LITERAL(arg.data.str);
    case DATA_TYPE_NOTHING:
        return CONST_STRING_LITERAL("nothing");
    case DATA_TYPE_STRING:
        return arg.data.value;
    case DATA_TYPE_INTEGER:
        return build_call(compiler, "std_string_from_integer", CONST_GC, arg.data.value);
    case DATA_TYPE_BOOL:
        return build_call(compiler, "std_string_from_bool", CONST_GC, arg.data.value);
    case DATA_TYPE_FLOAT:
        return build_call(compiler, "std_string_from_float", CONST_GC, arg.data.value);
    case DATA_TYPE_ANY:
        return build_call(compiler, "std_string_from_any", CONST_GC, arg.data.value);
    case DATA_TYPE_LIST:
        return build_call(compiler, "std_string_from_literal", CONST_GC, CONST_STRING_LITERAL(""), CONST_INTEGER(0));
    case DATA_TYPE_COLOR:
        return build_call(compiler, "std_string_from_color", CONST_GC, arg.data.value);
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into any string"), type_to_str(arg.type));
        return NULL;
    }
    assert(false && "Unhandled cast to any string");
}

LLVMValueRef arg_to_string_ref(Compiler* compiler, Block* block, FuncArg arg) {
    switch (arg.type) {
    case DATA_TYPE_LITERAL:
        return build_call(compiler, "std_string_from_literal", CONST_GC, CONST_STRING_LITERAL(arg.data.str), CONST_INTEGER(strlen(arg.data.str)));
    case DATA_TYPE_NOTHING:
        return build_call(compiler, "std_string_from_literal", CONST_GC, CONST_STRING_LITERAL("nothing"), CONST_INTEGER(sizeof("nothing") - 1));
    case DATA_TYPE_LIST:
        return build_call(compiler, "std_string_from_literal", CONST_GC, CONST_STRING_LITERAL(""), CONST_INTEGER(0));
    case DATA_TYPE_STRING:
        return arg.data.value;
    case DATA_TYPE_INTEGER:
        return build_call(compiler, "std_string_from_integer", CONST_GC, arg.data.value);
    case DATA_TYPE_BOOL:
        return build_call(compiler, "std_string_from_bool", CONST_GC, arg.data.value);
    case DATA_TYPE_FLOAT:
        return build_call(compiler, "std_string_from_float", CONST_GC, arg.data.value);
    case DATA_TYPE_ANY:
        return build_call(compiler, "std_string_from_any", CONST_GC, arg.data.value);
    case DATA_TYPE_COLOR:
        return build_call(compiler, "std_string_from_color", CONST_GC, arg.data.value);
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_BLOCKDEF:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(arg.type), type_to_str(DATA_TYPE_STRING));
        return NULL;
    }
    assert(false && "Unhandled cast to string ref");
}

LLVMValueRef arg_to_color(Compiler* compiler, Block* block, FuncArg arg) {
    switch (arg.type) {
    case DATA_TYPE_LITERAL: ;
        StdColor col = std_parse_color(arg.data.str);
        return CONST_INTEGER(*(int*)&col);
    case DATA_TYPE_STRING:
        return build_call(compiler, "std_parse_color", build_call(compiler, "std_string_get_data", arg.data.value));
    case DATA_TYPE_LIST:
    case DATA_TYPE_NOTHING:
        return CONST_INTEGER(0);
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_COLOR:
        return arg.data.value;
    case DATA_TYPE_BOOL:
        return LLVMBuildSelect(compiler->builder, arg.data.value, CONST_INTEGER(0xffffffff), CONST_INTEGER(0xff000000), "color_cast");
    case DATA_TYPE_FLOAT:
        return LLVMBuildFPToSI(compiler->builder, arg.data.value, LLVMInt32Type(), "color_cast");
    case DATA_TYPE_ANY:
        return build_call(compiler, "std_color_from_any", arg.data.value);
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_UNKNOWN:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(arg.type), type_to_str(DATA_TYPE_COLOR));
        return NULL;
    }
    assert(false && "Unhandled cast to color");
}

LLVMValueRef arg_to_list(Compiler* compiler, Block* block, FuncArg arg) {
    switch (arg.type) {
    case DATA_TYPE_BOOL:
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_STRING:
    case DATA_TYPE_FLOAT:
    case DATA_TYPE_LITERAL:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_COLOR:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(arg.type), type_to_str(DATA_TYPE_LIST));
        return NULL;
    case DATA_TYPE_LIST:
        return arg.data.value;
    case DATA_TYPE_ANY:
        return build_call(compiler, "std_list_from_any", CONST_GC, arg.data.value);
    }
    assert(false && "Unhandled cast to string ref");
}

LLVMValueRef arg_to_any(Compiler* compiler, Block* block, FuncArg arg) {
    switch (arg.type) {
    case DATA_TYPE_NOTHING:
        return build_call_count(compiler, "std_any_from_value", 2, CONST_GC, CONST_INTEGER(arg.type));
    case DATA_TYPE_BOOL:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_STRING:
    case DATA_TYPE_FLOAT:
    case DATA_TYPE_LITERAL:
    case DATA_TYPE_LIST:
    case DATA_TYPE_COLOR:
        return build_call_count(compiler, "std_any_from_value", 3, CONST_GC, CONST_INTEGER(arg.type), arg_to_value(compiler, block, arg));
    case DATA_TYPE_ANY:
        return arg.data.value;
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_BLOCKDEF:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(arg.type), type_to_str(DATA_TYPE_ANY));
        return NULL;
    }
    assert(false && "Unhandled cast to string ref");
}

FuncArg arg_cast(Compiler* compiler, Block* block, FuncArg arg, DataType cast_to_type) {
    switch (cast_to_type) {
    case DATA_TYPE_LITERAL:
        if (arg.type == DATA_TYPE_LITERAL) return arg;
        assert(false && "Attempted to cast LLVM value to string literal");
    case DATA_TYPE_STRING:
        return DATA_STRING(arg_to_string_ref(compiler, block, arg));
    case DATA_TYPE_INTEGER:
        return DATA_INTEGER(arg_to_integer(compiler, block, arg));
    case DATA_TYPE_BOOL:
        return DATA_BOOLEAN(arg_to_bool(compiler, block, arg));
    case DATA_TYPE_FLOAT:
        return DATA_FLOAT(arg_to_float(compiler, block, arg));
    case DATA_TYPE_LIST:
        return DATA_LIST(arg_to_list(compiler, block, arg));
    case DATA_TYPE_ANY:
        return DATA_ANY(arg_to_any(compiler, block, arg));
    case DATA_TYPE_COLOR:
        return DATA_COLOR(arg_to_color(compiler, block, arg));
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_BLOCKDEF:
        compiler_set_error(compiler, block, gettext("Cannot cast type %s into %s"), type_to_str(arg.type), type_to_str(cast_to_type));
        return DATA_UNKNOWN;
    }
    assert(false && "Unhandled cast to value typed");
}

bool block_return(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);

    LLVMBasicBlockRef current = LLVMGetInsertBlock(compiler->builder);
    LLVMBasicBlockRef return_block = LLVMInsertBasicBlock(current, "return");
    LLVMBasicBlockRef return_after_block = LLVMInsertBasicBlock(current, "return_after");

    LLVMMoveBasicBlockAfter(return_after_block, current);
    LLVMMoveBasicBlockAfter(return_block, current);

    LLVMBuildBr(compiler->builder, return_block);

    LLVMPositionBuilderAtEnd(compiler->builder, return_block);

    build_call(compiler, "gc_root_restore", CONST_GC);

    LLVMTypeRef data_type = LLVMTypeOf(argv[0].data.value);
    if (data_type == LLVMPointerType(LLVMInt8Type(), 0)) {
        build_call(compiler, "gc_add_temp_root", CONST_GC, argv[0].data.value);
    }
    
    LLVMValueRef custom_return = arg_to_any(compiler, block, argv[0]);
    if (!custom_return) return false;

    compiler->gc_dirty = false;
    LLVMBuildRet(compiler->builder, custom_return);

    LLVMPositionBuilderAtEnd(compiler->builder, return_after_block);
    
    *return_val = DATA_NOTHING;
    return true;
}

bool block_custom_arg(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) argc;
    (void) argv;
    DefineFunction* func;
    DefineArgument* arg = get_custom_argument(compiler, block->blockdef, &func);
    if (!arg) {
        compiler_set_error(compiler, block, gettext("Could not find function definition for argument"));
        return false;
    }
    
    if (LLVMGetBasicBlockParent(LLVMGetInsertBlock(compiler->builder)) != func->func) {
        compiler_set_error(compiler, block, gettext("Function argument block used outside of function"));
        return false;
    }

    *return_val = DATA_ANY(arg->arg);
    return true;
}

bool block_exec_custom(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    if (argc > 32) {
        compiler_set_error(compiler, block, gettext("Too many parameters passed into function. Got %d/32"), argc);
        return false;
    }
    
    DefineFunction* define = define_function(compiler, block->blockdef);
    
    LLVMTypeRef func_type = LLVMGlobalGetValueType(define->func);
    LLVMValueRef func_param_list[32];

    for (int i = 0; i < argc; i++) {
        func_param_list[i] = arg_to_any(compiler, block, argv[i]);
        if (!func_param_list[i]) return false;
    }

    compiler->gc_dirty = true;
    if (compiler->gc_block_stack_len > 0) {
        compiler->gc_block_stack[compiler->gc_block_stack_len - 1].required = true;
    }
    *return_val = DATA_ANY(LLVMBuildCall2(compiler->builder, func_type, define->func, func_param_list, argc, ""));
    return true;
}

bool block_not_eq(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type == DATA_TYPE_LITERAL && argv[1].type == DATA_TYPE_LITERAL) {
        *return_val = DATA_BOOLEAN(CONST_BOOLEAN(!!strcmp(argv[0].data.str, argv[1].data.str)));
        return true;
    }

    FuncArg left;
    FuncArg right;

    if (argv[0].type == DATA_TYPE_LITERAL) {
        left = arg_cast(compiler, block, argv[0], argv[1].type);
        if (!left.data.value) return false;
        right = argv[1];
    } else if (argv[1].type == DATA_TYPE_LITERAL) {
        left = argv[0];
        right = arg_cast(compiler, block, argv[1], argv[0].type);
        if (!right.data.value) return false;
    } else if (argv[0].type != argv[1].type) {
        *return_val = DATA_BOOLEAN(CONST_BOOLEAN(1));
        return true;
    } else {
        left = argv[0];
        right = argv[1];
    }

    switch (left.type) {
    case DATA_TYPE_NOTHING:
        *return_val = DATA_BOOLEAN(CONST_BOOLEAN(0));
        break;
    case DATA_TYPE_STRING: ;
        LLVMValueRef eq_return = build_call(compiler, "std_string_is_eq", left.data.value, right.data.value);
        *return_val = DATA_BOOLEAN(LLVMBuildXor(compiler->builder, eq_return, CONST_BOOLEAN(1), "string_neq"));
        break;
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_COLOR:
        *return_val = DATA_BOOLEAN(LLVMBuildICmp(compiler->builder, LLVMIntNE, left.data.value, right.data.value, "int_neq"));
        break;
    case DATA_TYPE_BOOL:
        *return_val = DATA_BOOLEAN(LLVMBuildXor(compiler->builder, left.data.value, right.data.value, "bool_neq"));
        break;
    case DATA_TYPE_FLOAT:
        *return_val = DATA_BOOLEAN(LLVMBuildFCmp(compiler->builder, LLVMRealONE, left.data.value, right.data.value, "float_neq"));
        break;
    case DATA_TYPE_LIST:
        // Compare list pointers
        *return_val = DATA_BOOLEAN(LLVMBuildICmp(compiler->builder, LLVMIntNE, left.data.value, right.data.value, "list_neq"));
        break;
    case DATA_TYPE_ANY: ;
        LLVMValueRef eq_any_return = build_call(compiler, "std_any_is_eq", left.data.value, right.data.value);
        *return_val = DATA_BOOLEAN(LLVMBuildXor(compiler->builder, eq_any_return, CONST_BOOLEAN(1), "any_neq"));
        break;
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_LITERAL:
    case DATA_TYPE_BLOCKDEF:
        compiler_set_error(compiler, block, gettext("Cannot compare type %s"), type_to_str(left.type));
        return false;
    }

    return true;
}

bool block_eq(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type == DATA_TYPE_LITERAL && argv[1].type == DATA_TYPE_LITERAL) {
        *return_val = DATA_BOOLEAN(CONST_BOOLEAN(!strcmp(argv[0].data.str, argv[1].data.str)));
        return true;
    }

    FuncArg left;
    FuncArg right;

    if (argv[0].type == DATA_TYPE_LITERAL) {
        left = arg_cast(compiler, block, argv[0], argv[1].type);
        if (!left.data.value) return false;
        right = argv[1];
    } else if (argv[1].type == DATA_TYPE_LITERAL) {
        left = argv[0];
        right = arg_cast(compiler, block, argv[1], argv[0].type);
        if (!right.data.value) return false;
    } else if (argv[0].type != argv[1].type) {
        *return_val = DATA_BOOLEAN(CONST_BOOLEAN(0));
        return true;
    } else {
        left = argv[0];
        right = argv[1];
    }

    switch (left.type) {
    case DATA_TYPE_NOTHING:
        *return_val = DATA_BOOLEAN(CONST_BOOLEAN(1));
        break;
    case DATA_TYPE_STRING:
        *return_val = DATA_BOOLEAN(build_call(compiler, "std_string_is_eq", left.data.value, right.data.value));
        break;
    case DATA_TYPE_BOOL:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_COLOR:
        *return_val = DATA_BOOLEAN(LLVMBuildICmp(compiler->builder, LLVMIntEQ, left.data.value, right.data.value, "int_eq"));
        break;
    case DATA_TYPE_FLOAT:
        *return_val = DATA_BOOLEAN(LLVMBuildFCmp(compiler->builder, LLVMRealOEQ, left.data.value, right.data.value, "float_eq"));
        break;
    case DATA_TYPE_LIST:
        // Compare list pointers
        *return_val = DATA_BOOLEAN(LLVMBuildICmp(compiler->builder, LLVMIntEQ, left.data.value, right.data.value, "list_eq"));
        break;
    case DATA_TYPE_ANY:
        *return_val = DATA_BOOLEAN(build_call(compiler, "std_any_is_eq", left.data.value, right.data.value));
        break;
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_LITERAL:
    case DATA_TYPE_BLOCKDEF:
        compiler_set_error(compiler, block, gettext("Cannot compare type %s"), type_to_str(left.type));
        return false;
    }

    return true;
}

bool block_false(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) compiler;
    (void) argc;
    (void) argv;
    *return_val = DATA_BOOLEAN(CONST_BOOLEAN(0));
    return true;
}

bool block_true(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) compiler;
    (void) argc;
    (void) argv;
    *return_val = DATA_BOOLEAN(CONST_BOOLEAN(1));
    return true;
}

bool block_or(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    LLVMValueRef left = arg_to_bool(compiler, block, argv[0]);
    if (!left) return false;
    LLVMValueRef right = arg_to_bool(compiler, block, argv[1]);
    if (!right) return false;
    *return_val = DATA_BOOLEAN(LLVMBuildOr(compiler->builder, left, right, "bool_or"));
    return true;
}

bool block_and(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    LLVMValueRef left = arg_to_bool(compiler, block, argv[0]);
    if (!left) return false;
    LLVMValueRef right = arg_to_bool(compiler, block, argv[1]);
    if (!right) return false;
    *return_val = DATA_BOOLEAN(LLVMBuildAnd(compiler->builder, left, right, "bool_and"));
    return true;
}

bool block_not(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef value = arg_to_bool(compiler, block, argv[0]);
    if (!value) return false;
    *return_val = DATA_BOOLEAN(LLVMBuildXor(compiler->builder, value, CONST_BOOLEAN(1), "not"));
    return true;
}

bool block_more_eq(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_FLOAT) {
        LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
        if (!left) return false;
        LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_BOOLEAN(LLVMBuildICmp(compiler->builder, LLVMIntSGE, left, right, "int_more_or_eq"));
    } else {
        LLVMValueRef right = arg_to_float(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_BOOLEAN(LLVMBuildFCmp(compiler->builder, LLVMRealOGE, argv[0].data.value, right, "float_more_or_eq"));
    }
    return true;
}

bool block_more(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_FLOAT) {
        LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
        if (!left) return false;
        LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_BOOLEAN(LLVMBuildICmp(compiler->builder, LLVMIntSGT, left, right, "int_more"));
    } else {
        LLVMValueRef right = arg_to_float(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_BOOLEAN(LLVMBuildFCmp(compiler->builder, LLVMRealOGT, argv[0].data.value, right, "float_more"));
    }
    return true;
}

bool block_less_eq(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_FLOAT) {
        LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
        if (!left) return false;
        LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_BOOLEAN(LLVMBuildICmp(compiler->builder, LLVMIntSLE, left, right, "int_less_or_eq"));
    } else {
        LLVMValueRef right = arg_to_float(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_BOOLEAN(LLVMBuildFCmp(compiler->builder, LLVMRealOLE, argv[0].data.value, right, "float_less_or_eq"));
    }
    return true;
}

bool block_less(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_FLOAT) {
        LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
        if (!left) return false;
        LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_BOOLEAN(LLVMBuildICmp(compiler->builder, LLVMIntSLT, left, right, "int_less"));
    } else {
        LLVMValueRef right = arg_to_float(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_BOOLEAN(LLVMBuildFCmp(compiler->builder, LLVMRealOLT, argv[0].data.value, right, "float_less"));
    }
    return true;
}

bool block_bit_or(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
    if (!left) return false;
    LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
    if (!right) return false;
    *return_val = DATA_INTEGER(LLVMBuildOr(compiler->builder, left, right, "or"));
    return true;
}

bool block_bit_xor(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
    if (!left) return false;
    LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
    if (!right) return false;
    *return_val = DATA_INTEGER(LLVMBuildXor(compiler->builder, left, right, "xor"));
    return true;
}

bool block_bit_and(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
    if (!left) return false;
    LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
    if (!right) return false;
    *return_val = DATA_INTEGER(LLVMBuildAnd(compiler->builder, left, right, "and"));
    return true;
}

bool block_bit_not(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef integer_val = arg_to_integer(compiler, block, argv[0]);
    if (!integer_val) return false;

    LLVMValueRef add_op = LLVMBuildAdd(compiler->builder, integer_val, CONST_INTEGER(1), "");
    *return_val = DATA_INTEGER(LLVMBuildNeg(compiler->builder, add_op, "bit_not"));
    return true;
}

bool block_pi(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) compiler;
    (void) argc;
    (void) argv;
    *return_val = DATA_FLOAT(CONST_FLOAT(M_PI));
    return true;
}

bool block_math(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_LITERAL) return false;

    LLVMValueRef value = arg_to_float(compiler, block, argv[1]);
    if (!value) return false;

    for (int i = 0; i < MATH_LIST_LEN; i++) {
        if (strcmp(argv[0].data.str, block_math_list[i])) continue;
        *return_val = DATA_FLOAT(build_call(compiler, argv[0].data.str, value));
        return true;
    }

    return false;
}

bool block_pow(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);

    if (argv[0].type != DATA_TYPE_FLOAT) {
        LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
        if (!left) return false;
        LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
        if (!right) return false;

        *return_val = DATA_INTEGER(build_call(compiler, "std_int_pow", left, right));
    } else {
        LLVMValueRef right = arg_to_float(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_FLOAT(build_call(compiler, "pow", argv[0].data.value, right));
    }

    return true;
}

bool block_rem(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_FLOAT) {
        LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
        if (!left) return false;
        LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
        if (!right) return false;

        if (!LLVMIsConstant(right)) {
            LLVMBasicBlockRef current_branch = LLVMGetInsertBlock(compiler->builder);
            LLVMBasicBlockRef non_zero_branch = LLVMInsertBasicBlock(current_branch, "non_zero_cond");
            LLVMBasicBlockRef zero_branch = LLVMInsertBasicBlock(current_branch, "zero_cond");
            LLVMBasicBlockRef phi_branch = LLVMInsertBasicBlock(current_branch, "cond_after");

            LLVMMoveBasicBlockAfter(phi_branch, current_branch);
            LLVMMoveBasicBlockAfter(zero_branch, current_branch);
            LLVMMoveBasicBlockAfter(non_zero_branch, current_branch);

            LLVMValueRef condition = LLVMBuildICmp(compiler->builder, LLVMIntEQ, right, CONST_INTEGER(0), "");
            LLVMBuildCondBr(compiler->builder, condition, zero_branch, non_zero_branch);

            LLVMPositionBuilderAtEnd(compiler->builder, non_zero_branch);
            LLVMValueRef out = LLVMBuildSRem(compiler->builder, left, right, "");
            LLVMBuildBr(compiler->builder, phi_branch);

            LLVMPositionBuilderAtEnd(compiler->builder, zero_branch);
            LLVMBuildBr(compiler->builder, phi_branch);

            LLVMPositionBuilderAtEnd(compiler->builder, phi_branch);

            *return_val = DATA_INTEGER(LLVMBuildPhi(compiler->builder, LLVMInt32Type(), "div"));

            LLVMValueRef vals[] = { CONST_INTEGER(0), out };
            LLVMBasicBlockRef blocks[] = { zero_branch, non_zero_branch };
            LLVMAddIncoming(return_val->data.value, vals, blocks, ARRLEN(blocks));
        } else {
            *return_val = DATA_INTEGER(LLVMBuildSRem(compiler->builder, left, right, "rem"));
        }
    } else {
        LLVMValueRef right = arg_to_float(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_FLOAT(LLVMBuildFRem(compiler->builder, argv[0].data.value, right, "rem"));
    }

    if (LLVMIsPoison(return_val->data.value)) {
        compiler_set_error(compiler, block, gettext("Division by zero"));
        return false;
    }
    return true;
}

bool block_div(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_FLOAT) {
        LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
        if (!left) return false;
        LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
        if (!right) return false;

        if (!LLVMIsConstant(right)) {
            LLVMBasicBlockRef current_branch = LLVMGetInsertBlock(compiler->builder);
            LLVMBasicBlockRef non_zero_branch = LLVMInsertBasicBlock(current_branch, "non_zero_cond");
            LLVMBasicBlockRef zero_branch = LLVMInsertBasicBlock(current_branch, "zero_cond");
            LLVMBasicBlockRef phi_branch = LLVMInsertBasicBlock(current_branch, "cond_after");

            LLVMMoveBasicBlockAfter(phi_branch, current_branch);
            LLVMMoveBasicBlockAfter(zero_branch, current_branch);
            LLVMMoveBasicBlockAfter(non_zero_branch, current_branch);

            LLVMValueRef condition = LLVMBuildICmp(compiler->builder, LLVMIntEQ, right, CONST_INTEGER(0), "");
            LLVMBuildCondBr(compiler->builder, condition, zero_branch, non_zero_branch);

            LLVMPositionBuilderAtEnd(compiler->builder, non_zero_branch);
            LLVMValueRef out = LLVMBuildSDiv(compiler->builder, left, right, "");
            LLVMBuildBr(compiler->builder, phi_branch);

            LLVMPositionBuilderAtEnd(compiler->builder, zero_branch);
            LLVMBuildBr(compiler->builder, phi_branch);

            LLVMPositionBuilderAtEnd(compiler->builder, phi_branch);

            *return_val = DATA_INTEGER(LLVMBuildPhi(compiler->builder, LLVMInt32Type(), "div"));

            LLVMValueRef vals[] = { CONST_INTEGER(0), out };
            LLVMBasicBlockRef blocks[] = { zero_branch, non_zero_branch };
            LLVMAddIncoming(return_val->data.value, vals, blocks, ARRLEN(blocks));
        } else {
            *return_val = DATA_INTEGER(LLVMBuildSDiv(compiler->builder, left, right, "div"));
        }
    } else {
        LLVMValueRef right = arg_to_float(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_FLOAT(LLVMBuildFDiv(compiler->builder, argv[0].data.value, right, "div"));
    }

    if (LLVMIsPoison(return_val->data.value)) {
        compiler_set_error(compiler, block, gettext("Division by zero"));
        return false;
    }
    return true;
}

bool block_mult(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_FLOAT) {
        LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
        if (!left) return false;
        LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_INTEGER(LLVMBuildMul(compiler->builder, left, right, "mul"));
    } else {
        LLVMValueRef right = arg_to_float(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_FLOAT(LLVMBuildFMul(compiler->builder, argv[0].data.value, right, "mul"));
    }
    return true;
}

bool block_minus(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_FLOAT) {
        LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
        if (!left) return false;
        LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_INTEGER(LLVMBuildSub(compiler->builder, left, right, "sub"));
    } else {
        LLVMValueRef right = arg_to_float(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_FLOAT(LLVMBuildFSub(compiler->builder, argv[0].data.value, right, "sub"));
    }
    return true;
}

bool block_plus(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_FLOAT) {
        LLVMValueRef left = arg_to_integer(compiler, block, argv[0]);
        if (!left) return false;
        LLVMValueRef right = arg_to_integer(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_INTEGER(LLVMBuildAdd(compiler->builder, left, right, "add"));
    } else {
        LLVMValueRef right = arg_to_float(compiler, block, argv[1]);
        if (!right) return false;
        *return_val = DATA_FLOAT(LLVMBuildFAdd(compiler->builder, argv[0].data.value, right, "add"));
    }
    return true;
}

// TODO: Make this block evaluate arguments lazily
bool block_typeof(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) compiler;
    (void) argc;
    *return_val = (FuncArg) {
        .type = DATA_TYPE_LITERAL,
        .data = (FuncArgData) {
            .str = (char*)type_to_str(argv[0].type),
        },
    };
    return true;
}

bool block_convert_color(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef value = arg_to_color(compiler, block, argv[0]);
    if (!value) return false;
    *return_val = DATA_COLOR(value);
    return true;
}

bool block_convert_bool(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef value = arg_to_bool(compiler, block, argv[0]);
    if (!value) return false;
    *return_val = DATA_BOOLEAN(value);
    return true;
}

bool block_convert_str(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef value = arg_to_string_ref(compiler, block, argv[0]);
    if (!value) return false;
    *return_val = DATA_STRING(value);
    return true;
}

bool block_convert_float(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef value = arg_to_float(compiler, block, argv[0]);
    if (!value) return false;
    *return_val = DATA_FLOAT(value);
    return true;
}

bool block_convert_int(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef value = arg_to_integer(compiler, block, argv[0]);
    if (!value) return false;
    *return_val = DATA_INTEGER(value);
    return true;
}

bool block_unix_time(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    *return_val = DATA_INTEGER(build_call(compiler, "time", LLVMConstPointerNull(LLVMPointerType(LLVMVoidType(), 0))));
    return true;
}

bool block_length(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef str = arg_to_string_ref(compiler, block, argv[0]);
    if (!str) return false;

    *return_val = DATA_INTEGER(build_call(compiler, "std_string_length", str));
    return true;
}

bool block_substring(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(3);
    LLVMValueRef begin = arg_to_integer(compiler, block, argv[0]);
    if (!begin) return false;

    LLVMValueRef end = arg_to_integer(compiler, block, argv[1]);
    if (!end) return false;

    LLVMValueRef str = arg_to_string_ref(compiler, block, argv[2]);
    if (!str) return false;

    *return_val = DATA_STRING(build_call(compiler, "std_string_substring", CONST_GC, begin, end, str));
    return true;
}

bool block_letter_in(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    LLVMValueRef target = arg_to_integer(compiler, block, argv[0]);
    if (!target) return false;

    LLVMValueRef str = arg_to_string_ref(compiler, block, argv[1]);
    if (!str) return false;

    *return_val = DATA_STRING(build_call(compiler, "std_string_letter_in", CONST_GC, target, str));
    return true;
}

bool block_chr(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef value = arg_to_integer(compiler, block, argv[0]);
    if (!value) return false;

    *return_val = DATA_STRING(build_call(compiler, "std_string_chr", CONST_GC, value));
    return true;
}

bool block_ord(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef str = arg_to_any_string(compiler, block, argv[0]);
    if (!str) return false;

    *return_val = DATA_INTEGER(build_call(compiler, "std_string_ord", str));
    return true;
}

bool block_join(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    LLVMValueRef left = arg_to_string_ref(compiler, block, argv[0]);
    if (!left) return false;
    LLVMValueRef right = arg_to_string_ref(compiler, block, argv[1]);
    if (!right) return false;

    *return_val = DATA_STRING(build_call(compiler, "std_string_join", CONST_GC, left, right));
    return true;
}

bool block_random(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);

    compiler->build_random = true;

    LLVMValueRef min = arg_to_integer(compiler, block, argv[0]);
    if (!min) return false;
    LLVMValueRef max = arg_to_integer(compiler, block, argv[1]);
    if (!max) return false;

    *return_val = DATA_INTEGER(build_call(compiler, "std_get_random", min, max));
    return true;
}

bool block_get_char(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    *return_val = DATA_STRING(build_call(compiler, "std_term_get_char", CONST_GC));
    return true;
}

bool block_input(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    *return_val = DATA_STRING(build_call(compiler, "std_term_get_input", CONST_GC));
    return true;
}

bool block_term_set_clear(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;

    LLVMValueRef color = arg_to_color(compiler, block, argv[0]);
    if (!color) return false;

    build_call(compiler, "std_term_set_clear_color", color);
    *return_val = DATA_NOTHING;
    return true;
}

bool block_term_clear(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    build_call(compiler, "std_term_clear");
    *return_val = DATA_NOTHING;
    return true;
}

bool block_reset_color(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    // For some reason gcc throws a warning in release mode with constant color struct so it is passed as integer
    build_call(compiler, "std_term_set_fg_color", CONST_INTEGER(0xffffffff));
    build_call(compiler, "std_term_set_bg_color", CONST_INTEGER(*(int*)&(BLACK)));
    *return_val = DATA_NOTHING;
    return true;
}

bool block_set_bg_color(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);

    LLVMValueRef color = arg_to_color(compiler, block, argv[0]);
    if (!color) return false;

    build_call(compiler, "std_term_set_bg_color", color);
    *return_val = DATA_NOTHING;
    return true;
}

bool block_set_fg_color(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);

    LLVMValueRef color = arg_to_color(compiler, block, argv[0]);
    if (!color) return false;

    build_call(compiler, "std_term_set_fg_color", color);
    *return_val = DATA_NOTHING;
    return true;
}

bool block_set_cursor(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    LLVMValueRef x = arg_to_integer(compiler, block, argv[0]);
    if (!x) return false;
    LLVMValueRef y = arg_to_integer(compiler, block, argv[1]);
    if (!y) return false;

    build_call(compiler, "std_term_set_cursor", x, y);
    *return_val = DATA_NOTHING;
    return true;
}

bool block_cursor_max_y(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    *return_val = DATA_INTEGER(build_call(compiler, "std_term_cursor_max_y"));
    return true;
}

bool block_cursor_max_x(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    *return_val = DATA_INTEGER(build_call(compiler, "std_term_cursor_max_x"));
    return true;
}

bool block_cursor_y(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    *return_val = DATA_INTEGER(build_call(compiler, "std_term_cursor_y"));
    return true;
}

bool block_cursor_x(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    *return_val = DATA_INTEGER(build_call(compiler, "std_term_cursor_x"));
    return true;
}

bool block_print(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);

    switch (argv[0].type) {
    case DATA_TYPE_LITERAL:
        *return_val = DATA_INTEGER(*argv[0].data.str
                                   ? build_call(compiler, "std_term_print_str", CONST_STRING_LITERAL(argv[0].data.str))
                                   : CONST_INTEGER(0));
        return true;
    case DATA_TYPE_STRING:
        *return_val = DATA_INTEGER(build_call(compiler, "std_term_print_str", build_call(compiler, "std_string_get_data", argv[0].data.value)));
        return true;
    case DATA_TYPE_NOTHING:
        *return_val = DATA_INTEGER(CONST_INTEGER(0));
        return true;
    case DATA_TYPE_INTEGER:
        *return_val = DATA_INTEGER(build_call(compiler, "std_term_print_integer", argv[0].data.value));
        return true;
    case DATA_TYPE_BOOL:
        *return_val = DATA_INTEGER(build_call(compiler, "std_term_print_bool", argv[0].data.value));
        return true;
    case DATA_TYPE_FLOAT:
        *return_val = DATA_INTEGER(build_call(compiler, "std_term_print_float", argv[0].data.value));
        return true;
    case DATA_TYPE_LIST:
        *return_val = DATA_INTEGER(build_call(compiler, "std_term_print_list", argv[0].data.value));
        return true;
    case DATA_TYPE_ANY:
        *return_val = DATA_INTEGER(build_call(compiler, "std_term_print_any", argv[0].data.value));
        return true;
    case DATA_TYPE_COLOR:
        *return_val = DATA_INTEGER(build_call(compiler, "std_term_print_color", argv[0].data.value));
        return true;
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_BLOCKDEF:
        compiler_set_error(compiler, block, gettext("Invalid type %s in print function"), type_to_str(argv[0].type));
        return false;
    }

    compiler_set_error(compiler, block, gettext("Unhandled type %s in print function"), type_to_str(argv[0].type));
    return false;
}

bool block_println(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    block_print(compiler, block, argc, argv, return_val, control_state);
    build_call(compiler, "std_term_print_str", CONST_STRING_LITERAL("\n"));
    *return_val = DATA_INTEGER(LLVMBuildAdd(compiler->builder, return_val->data.value, CONST_INTEGER(1), "add"));
    return true;
}

bool block_list_length(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    MIN_ARG_COUNT(1);

    LLVMValueRef list = arg_to_list(compiler, block, argv[0]);
    if (!list) return false;

    *return_val = DATA_INTEGER(build_call(compiler, "std_list_length", list));
    return true;
}

bool block_list_set(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    MIN_ARG_COUNT(3);

    LLVMValueRef list = arg_to_list(compiler, block, argv[0]);
    if (!list) return false;
    LLVMValueRef index = arg_to_integer(compiler, block, argv[1]);
    if (!index) return false;

    if (argv[2].type == DATA_TYPE_NOTHING) {
        build_call_count(compiler, "std_list_set", 3, list, index, CONST_INTEGER(argv[2].type));
    } else {
        build_call_count(compiler, "std_list_set", 4, list, index, CONST_INTEGER(argv[2].type), arg_to_value(compiler, block, argv[2]));
    }
    *return_val = DATA_NOTHING;
    return true;
}

bool block_list_get(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);

    LLVMValueRef list = arg_to_list(compiler, block, argv[0]);
    if (!list) return false;
    LLVMValueRef index = arg_to_integer(compiler, block, argv[1]);
    if (!index) return false;

    *return_val = DATA_ANY(build_call(compiler, "std_list_get", CONST_GC, list, index));
    return true;
}

bool block_list_add(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);

    LLVMValueRef list = arg_to_list(compiler, block, argv[0]);
    if (!list) return false;

    if (argv[1].type == DATA_TYPE_NOTHING) {
        build_call_count(compiler, "std_list_add", 3, CONST_GC, list, CONST_INTEGER(argv[1].type));
    } else {
        build_call_count(compiler, "std_list_add", 4, CONST_GC, list, CONST_INTEGER(argv[1].type), arg_to_value(compiler, block, argv[1]));
    }
    *return_val = DATA_NOTHING;
    return true;
}

bool block_create_list(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    *return_val = DATA_LIST(build_call(compiler, "std_list_new", CONST_GC));
    return true;
}

bool block_gc_collect(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    build_call(compiler, "gc_collect", CONST_GC);
    *return_val = DATA_NOTHING;
    return true;
}

bool block_set_var(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);
    if (argv[0].type != DATA_TYPE_LITERAL) {
        compiler_set_error(compiler, block, gettext("Invalid data type %s, expected %s"), type_to_str(argv[0].type), type_to_str(DATA_TYPE_LITERAL));
        return false;
    }

    Variable* var = variable_get(compiler, argv[0].data.str);
    if (!var) {
        compiler_set_error(compiler, block, gettext("Variable with name \"%s\" does not exist in the current scope"), argv[0].data.str);
        return false;
    }

    if (argv[1].type != var->value.type) {
        compiler_set_error(compiler, block, gettext("Assign to variable \"%s\" of type %s with incompatible type %s"), argv[0].data.str, type_to_str(var->value.type), type_to_str(argv[1].type));
        return false;
    }

    if (var->value.type == DATA_TYPE_LITERAL) {
        var->value = argv[1];
        *return_val = argv[1];
        return true;
    }

    LLVMBuildStore(compiler->builder, argv[1].data.value, var->value.data.value);
    *return_val = argv[1];
    return true;
}

bool block_get_var(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    if (argv[0].type != DATA_TYPE_LITERAL) {
        compiler_set_error(compiler, block, gettext("Invalid data type %s, expected %s"), type_to_str(argv[0].type), type_to_str(DATA_TYPE_LITERAL));
        return false;
    }

    Variable* var = variable_get(compiler, argv[0].data.str);
    if (!var) {
        compiler_set_error(compiler, block, gettext("Variable with name \"%s\" does not exist in the current scope"), argv[0].data.str);
        return false;
    }

    if (var->value.type == DATA_TYPE_LITERAL) {
        *return_val = var->value;
        return true;
    }

    *return_val = (FuncArg) {
        .type = var->value.type,
        .data = (FuncArgData) {
            .value = LLVMBuildLoad2(compiler->builder, var->type, var->value.data.value, "get_var"),
        },
    };
    return true;
}

bool block_declare_var(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(2);

    if (block->parent) {
        compiler_set_error(compiler, block, gettext("Variable declarations are not allowed inside an argument"));
        return false;
    }
    
    if (argv[0].type != DATA_TYPE_LITERAL) {
        compiler_set_error(compiler, block, gettext("Invalid data type %s, expected %s"), type_to_str(argv[0].type), type_to_str(DATA_TYPE_LITERAL));
        return false;
    }

    if (*argv[0].data.str == 0) {
        compiler_set_error(compiler, block, gettext("Cannot declare variable with empty name"));
        return false;
    }

    if (argv[1].type == DATA_TYPE_NOTHING) {
        compiler_set_error(compiler, block, gettext("Cannot declare a variable with zero sized type (i.e. Nothing)"));
        return false;
    }

    LLVMValueRef func_current = LLVMGetBasicBlockParent(LLVMGetInsertBlock(compiler->builder));
    LLVMValueRef func_main = LLVMGetNamedFunction(compiler->module, MAIN_NAME);

    if (argv[1].type == DATA_TYPE_LITERAL) {
        Variable var = (Variable) {
            .type = LLVMVoidType(),
            .value = argv[1],
            .name = argv[0].data.str,
        };
        if (compiler->control_stack_len == 0 && func_current == func_main) {
            global_variable_add(compiler, var);
        } else {
            if (!variable_stack_push(compiler, block, var)) return false;
        }
        *return_val = argv[1];
        return true;
    }

    LLVMTypeRef data_type = LLVMTypeOf(argv[1].data.value);

    Variable var = (Variable) {
        .type = data_type,
        .value = (FuncArg) {
            .type = argv[1].type,
            .data = (FuncArgData) {
                .value = NULL,
            },
        },
        .name = argv[0].data.str,
    };

    if (compiler->control_stack_len == 0 && func_current == func_main) {
        var.value.data.value = LLVMAddGlobal(compiler->module, data_type, argv[0].data.str);
        LLVMSetInitializer(var.value.data.value, LLVMConstNull(LLVMTypeOf(argv[1].data.value)));
        global_variable_add(compiler, var);
    } else {
        var.value.data.value = LLVMBuildAlloca(compiler->builder, data_type, argv[0].data.str);
        variable_stack_push(compiler, block, var);
    }

    LLVMBuildStore(compiler->builder, argv[1].data.value, var.value.data.value);
    if (data_type == LLVMPointerType(LLVMInt8Type(), 0)) {
        build_call(compiler, "gc_add_root", CONST_GC, var.value.data.value);
    }
    *return_val = argv[1];
    return true;
}

bool block_sleep(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);
    LLVMValueRef usecs = arg_to_integer(compiler, block, argv[0]);
    if (!usecs) return false;

    *return_val = DATA_INTEGER(build_call(compiler, "std_sleep", usecs));
    return true;
}

bool block_while(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) block;

    if (control_state == CONTROL_STATE_BEGIN) {
        MIN_ARG_COUNT(1);

        LLVMValueRef condition = arg_to_bool(compiler, block, argv[0]);
        if (!condition) return false;

        LLVMBasicBlockRef control_block = LLVMGetInsertBlock(compiler->builder);
        LLVMBasicBlockRef while_body_branch = LLVMInsertBasicBlock(control_block, "while");
        LLVMBasicBlockRef while_end_branch = LLVMInsertBasicBlock(control_block, "while_end");

        LLVMMoveBasicBlockAfter(while_end_branch, control_block);
        LLVMMoveBasicBlockAfter(while_body_branch, control_block);

        LLVMBuildCondBr(compiler->builder, condition, while_body_branch, while_end_branch);

        LLVMPositionBuilderAtEnd(compiler->builder, while_body_branch);
        if (!build_gc_root_begin(compiler, block)) return false;

        control_data_stack_push_data(control_block, LLVMBasicBlockRef);
        control_data_stack_push_data(while_end_branch, LLVMBasicBlockRef);
    } else if (control_state == CONTROL_STATE_END) {
        LLVMBasicBlockRef control_block, while_end_branch;
        control_data_stack_pop_data(while_end_branch, LLVMBasicBlockRef);
        control_data_stack_pop_data(control_block, LLVMBasicBlockRef);

        if (!build_gc_root_end(compiler, block)) return false;
        LLVMBuildBr(compiler->builder, control_block);

        LLVMPositionBuilderAtEnd(compiler->builder, while_end_branch);

        *return_val = DATA_BOOLEAN(CONST_BOOLEAN(1));
    } else {
        compiler_set_error(compiler, block, "Invalid control state");
        return false;
    }

    return true;
}

bool block_repeat(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) block;

    if (control_state == CONTROL_STATE_BEGIN) {
        MIN_ARG_COUNT(1);

        LLVMValueRef counter = arg_to_integer(compiler, block, argv[0]);
        if (!counter) return false;

        LLVMBasicBlockRef current = LLVMGetInsertBlock(compiler->builder);
        LLVMBasicBlockRef repeat_branch = LLVMInsertBasicBlock(current, "repeat");
        LLVMBasicBlockRef repeat_body_branch = LLVMInsertBasicBlock(current, "repeat_body");
        LLVMBasicBlockRef repeat_end_branch = LLVMInsertBasicBlock(current, "repeat_end");

        LLVMMoveBasicBlockAfter(repeat_end_branch, current);
        LLVMMoveBasicBlockAfter(repeat_body_branch, current);
        LLVMMoveBasicBlockAfter(repeat_branch, current);

        LLVMBuildBr(compiler->builder, repeat_branch);
        LLVMPositionBuilderAtEnd(compiler->builder, repeat_branch);

        LLVMValueRef phi_node = LLVMBuildPhi(compiler->builder, LLVMInt32Type(), "repeat_phi");
        LLVMValueRef index = LLVMBuildSub(compiler->builder, phi_node, CONST_INTEGER(1), "repeat_index_sub");
        LLVMValueRef index_test = LLVMBuildICmp(compiler->builder, LLVMIntSLT, index, CONST_INTEGER(0), "repeat_loop_check");
        LLVMBuildCondBr(compiler->builder, index_test, repeat_end_branch, repeat_body_branch);

        LLVMValueRef vals[] = { counter };
        LLVMBasicBlockRef blocks[] = { current };
        LLVMAddIncoming(phi_node, vals, blocks, ARRLEN(blocks));

        LLVMPositionBuilderAtEnd(compiler->builder, repeat_body_branch);
        if (!build_gc_root_begin(compiler, block)) return false;

        control_data_stack_push_data(phi_node, LLVMValueRef);
        control_data_stack_push_data(vals[0], LLVMValueRef);
        control_data_stack_push_data(index, LLVMValueRef);
        control_data_stack_push_data(repeat_branch, LLVMBasicBlockRef);
        control_data_stack_push_data(repeat_end_branch, LLVMBasicBlockRef);
    } else if (control_state == CONTROL_STATE_END) {
        LLVMBasicBlockRef current = LLVMGetInsertBlock(compiler->builder);
        LLVMBasicBlockRef loop_end, loop;
        LLVMValueRef phi_node, index, start_index;
        control_data_stack_pop_data(loop_end, LLVMBasicBlockRef);
        control_data_stack_pop_data(loop, LLVMBasicBlockRef);
        control_data_stack_pop_data(index, LLVMValueRef);
        control_data_stack_pop_data(start_index, LLVMValueRef);
        control_data_stack_pop_data(phi_node, LLVMValueRef);

        if (!build_gc_root_end(compiler, block)) return false;
        LLVMBuildBr(compiler->builder, loop);

        LLVMValueRef vals[] = { index };
        LLVMBasicBlockRef blocks[] = { current };
        LLVMAddIncoming(phi_node, vals, blocks, ARRLEN(blocks));

        LLVMPositionBuilderAtEnd(compiler->builder, loop_end);

        *return_val = DATA_BOOLEAN(LLVMBuildICmp(compiler->builder, LLVMIntSGT, start_index, CONST_INTEGER(0), ""));
    } else {
        compiler_set_error(compiler, block, "Invalid control state");
        return false;
    }

    return true;
}

bool block_else(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) block;

    if (control_state == CONTROL_STATE_BEGIN) {
        MIN_ARG_COUNT(1);

        LLVMValueRef value = arg_to_bool(compiler, block, argv[0]);
        if (!value) return false;

        LLVMBasicBlockRef current_branch = LLVMGetInsertBlock(compiler->builder);
        LLVMBasicBlockRef else_branch = LLVMInsertBasicBlock(current_branch, "else");
        LLVMBasicBlockRef end_branch = LLVMInsertBasicBlock(current_branch, "end_else");

        LLVMMoveBasicBlockAfter(end_branch, current_branch);
        LLVMMoveBasicBlockAfter(else_branch, current_branch);

        LLVMBuildCondBr(compiler->builder, value, end_branch, else_branch);

        LLVMPositionBuilderAtEnd(compiler->builder, else_branch);
        if (!build_gc_root_begin(compiler, block)) return false;

        control_data_stack_push_data(end_branch, LLVMBasicBlockRef);
    } else if (control_state == CONTROL_STATE_END) {
        LLVMBasicBlockRef end_branch;
        control_data_stack_pop_data(end_branch, LLVMBasicBlockRef);

        if (!build_gc_root_end(compiler, block)) return false;
        LLVMBuildBr(compiler->builder, end_branch);
        LLVMPositionBuilderAtEnd(compiler->builder, end_branch);
        *return_val = DATA_BOOLEAN(CONST_BOOLEAN(1));
    } else {
        compiler_set_error(compiler, block, "Invalid control state");
        return false;
    }

    return true;
}

bool block_else_if(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) block;

    if (control_state == CONTROL_STATE_BEGIN) {
        MIN_ARG_COUNT(2);

        LLVMValueRef prev_val = arg_to_bool(compiler, block, argv[0]);
        if (!prev_val) return false;
        LLVMValueRef condition = arg_to_bool(compiler, block, argv[1]);
        if (!condition) return false;

        LLVMBasicBlockRef current_branch = LLVMGetInsertBlock(compiler->builder);
        LLVMBasicBlockRef else_if_check_branch = LLVMInsertBasicBlock(current_branch, "else_if_check");
        LLVMBasicBlockRef else_if_branch = LLVMInsertBasicBlock(current_branch, "else_if");
        LLVMBasicBlockRef else_if_fail_branch = LLVMInsertBasicBlock(current_branch, "else_if_fail");
        LLVMBasicBlockRef end_branch = LLVMInsertBasicBlock(current_branch, "end_else_if");

        LLVMMoveBasicBlockAfter(end_branch, current_branch);
        LLVMMoveBasicBlockAfter(else_if_fail_branch, current_branch);
        LLVMMoveBasicBlockAfter(else_if_branch, current_branch);
        LLVMMoveBasicBlockAfter(else_if_check_branch, current_branch);

        LLVMBuildCondBr(compiler->builder, prev_val, end_branch, else_if_check_branch);
        control_data_stack_push_data(current_branch, LLVMBasicBlockRef);

        LLVMPositionBuilderAtEnd(compiler->builder, else_if_check_branch);
        LLVMBuildCondBr(compiler->builder, condition, else_if_branch, else_if_fail_branch);

        LLVMPositionBuilderAtEnd(compiler->builder, else_if_fail_branch);
        LLVMBuildBr(compiler->builder, end_branch);

        LLVMPositionBuilderAtEnd(compiler->builder, else_if_branch);
        if (!build_gc_root_begin(compiler, block)) return false;

        control_data_stack_push_data(else_if_fail_branch, LLVMBasicBlockRef);
        control_data_stack_push_data(end_branch, LLVMBasicBlockRef);
    } else if (control_state == CONTROL_STATE_END) {
        LLVMBasicBlockRef else_if_branch = LLVMGetInsertBlock(compiler->builder);
        LLVMBasicBlockRef top_branch, fail_branch, end_branch;
        control_data_stack_pop_data(end_branch, LLVMBasicBlockRef);
        control_data_stack_pop_data(fail_branch, LLVMBasicBlockRef);
        control_data_stack_pop_data(top_branch, LLVMBasicBlockRef);

        if (!build_gc_root_end(compiler, block)) return false;
        LLVMBuildBr(compiler->builder, end_branch);

        LLVMPositionBuilderAtEnd(compiler->builder, end_branch);
        *return_val = DATA_BOOLEAN(LLVMBuildPhi(compiler->builder, LLVMInt1Type(), ""));

        LLVMValueRef vals[] = { CONST_BOOLEAN(1), CONST_BOOLEAN(1), CONST_BOOLEAN(0) };
        LLVMBasicBlockRef blocks[] = { top_branch, else_if_branch, fail_branch };
        LLVMAddIncoming(return_val->data.value, vals, blocks, ARRLEN(blocks));
    } else {
        compiler_set_error(compiler, block, "Invalid control state");
        return false;
    }

    return true;
}

bool block_if(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) block;

    if (control_state == CONTROL_STATE_BEGIN) {
        MIN_ARG_COUNT(1);

        LLVMValueRef condition = arg_to_bool(compiler, block, argv[0]);
        if (!condition) return false;

        LLVMBasicBlockRef current_branch = LLVMGetInsertBlock(compiler->builder);
        LLVMBasicBlockRef then_branch = LLVMInsertBasicBlock(current_branch, "if_cond");
        // This is needed for a phi block to determine if this condition has failed. The result of this phi node is then passed into a C-end block
        LLVMBasicBlockRef fail_branch = LLVMInsertBasicBlock(current_branch, "if_fail");
        LLVMBasicBlockRef end_branch = LLVMInsertBasicBlock(current_branch, "end_if");

        LLVMMoveBasicBlockAfter(end_branch, current_branch);
        LLVMMoveBasicBlockAfter(fail_branch, current_branch);
        LLVMMoveBasicBlockAfter(then_branch, current_branch);

        LLVMBuildCondBr(compiler->builder, condition, then_branch, fail_branch);

        LLVMPositionBuilderAtEnd(compiler->builder, fail_branch);
        LLVMBuildBr(compiler->builder, end_branch);

        LLVMPositionBuilderAtEnd(compiler->builder, then_branch);
        if (!build_gc_root_begin(compiler, block)) return false;

        control_data_stack_push_data(fail_branch, LLVMBasicBlockRef);
        control_data_stack_push_data(end_branch, LLVMBasicBlockRef);
    } else if (control_state == CONTROL_STATE_END) {
        LLVMBasicBlockRef then_branch = LLVMGetInsertBlock(compiler->builder);
        LLVMBasicBlockRef fail_branch, end_branch;
        control_data_stack_pop_data(end_branch, LLVMBasicBlockRef);
        control_data_stack_pop_data(fail_branch, LLVMBasicBlockRef);

        if (!build_gc_root_end(compiler, block)) return false;
        LLVMBuildBr(compiler->builder, end_branch);

        LLVMPositionBuilderAtEnd(compiler->builder, end_branch);
        *return_val = DATA_BOOLEAN(LLVMBuildPhi(compiler->builder, LLVMInt1Type(), ""));

        LLVMValueRef vals[] = { CONST_BOOLEAN(1), CONST_BOOLEAN(0) };
        LLVMBasicBlockRef blocks[] = { then_branch, fail_branch };
        LLVMAddIncoming(return_val->data.value, vals, blocks, ARRLEN(blocks));
    } else {
        compiler_set_error(compiler, block, "Invalid control state");
        return false;
    }

    return true;
}

bool block_loop(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) block;
    (void) argc;
    (void) argv;

    if (control_state == CONTROL_STATE_BEGIN) {
        LLVMBasicBlockRef current = LLVMGetInsertBlock(compiler->builder);
        LLVMBasicBlockRef loop = LLVMInsertBasicBlock(current, "loop");
        LLVMBasicBlockRef loop_end = LLVMInsertBasicBlock(current, "loop_end");

        LLVMMoveBasicBlockAfter(loop_end, current);
        LLVMMoveBasicBlockAfter(loop, current);

        LLVMBuildBr(compiler->builder, loop);
        LLVMPositionBuilderAtEnd(compiler->builder, loop);
        if (!build_gc_root_begin(compiler, block)) return false;

        control_data_stack_push_data(loop, LLVMBasicBlockRef);
        control_data_stack_push_data(loop_end, LLVMBasicBlockRef);
    } else if (control_state == CONTROL_STATE_END) {
        LLVMBasicBlockRef loop;
        LLVMBasicBlockRef loop_end;
        control_data_stack_pop_data(loop_end, LLVMBasicBlockRef);
        control_data_stack_pop_data(loop, LLVMBasicBlockRef);

        if (!build_gc_root_end(compiler, block)) return false;
        LLVMBuildBr(compiler->builder, loop);
        LLVMPositionBuilderAtEnd(compiler->builder, loop_end);
        *return_val = DATA_BOOLEAN(CONST_BOOLEAN(0));
    } else {
        compiler_set_error(compiler, block, "Invalid control state");
        return false;
    }

    return true;
}

bool block_do_nothing(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) block;
    (void) argc;
    (void) argv;

    if (control_state == CONTROL_STATE_BEGIN) {
        if (!build_gc_root_begin(compiler, block)) return false;
    } else if (control_state == CONTROL_STATE_END) {
        if (!build_gc_root_end(compiler, block)) return false;
    } else {
        compiler_set_error(compiler, block, "Invalid control state");
        return false;
    }

    *return_val = DATA_NOTHING;
    return true;
}

bool block_noop(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) compiler;
    (void) argc;
    (void) argv;
    *return_val = DATA_NOTHING;
    return true;
}

bool block_define_block(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    MIN_ARG_COUNT(1);

    if (argv[0].type != DATA_TYPE_BLOCKDEF) {
        compiler_set_error(compiler, block, gettext("Invalid data type %s, expected %s"), type_to_str(argv[0].type), type_to_str(DATA_TYPE_BLOCKDEF));
        return false;
    }

    DefineFunction* define = define_function(compiler, argv[0].data.blockdef);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(define->func, "entry");
    LLVMPositionBuilderAtEnd(compiler->builder, entry);

    compiler->gc_value = LLVMBuildLoad2(compiler->builder, LLVMInt64Type(), LLVMGetNamedGlobal(compiler->module, "gc"), "get_gc");

    build_call(compiler, "gc_root_save", CONST_GC);

    if (!build_gc_root_begin(compiler, NULL)) return false;

    *return_val = DATA_NOTHING;
    return true;
}

bool block_on_start(Compiler* compiler, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state) {
    (void) control_state;
    (void) block;
    (void) argc;
    (void) argv;
    LLVMValueRef main_func = LLVMGetNamedFunction(compiler->module, MAIN_NAME);
    LLVMBasicBlockRef last_block = LLVMGetLastBasicBlock(main_func);
    LLVMPositionBuilderAtEnd(compiler->builder, last_block);
    *return_val = DATA_NOTHING;
    return true;
}

#endif // USE_LLVM

// Creates and registers blocks (commands) for the Vm/Compiler virtual machine
void register_blocks(Vm* vm) {
    BlockCategory cat;
    cat = block_category_new(gettext("Control"),  (Color) CATEGORY_CONTROL_COLOR);
    BlockCategory* cat_control = block_category_register(cat);
    cat = block_category_new(gettext("Terminal"), (Color) CATEGORY_TERMINAL_COLOR);
    BlockCategory* cat_terminal = block_category_register(cat);
    cat = block_category_new(gettext("Math"),     (Color) CATEGORY_MATH_COLOR);
    BlockCategory* cat_math = block_category_register(cat);
    cat = block_category_new(gettext("Logic"),    (Color) CATEGORY_LOGIC_COLOR);
    BlockCategory* cat_logic = block_category_register(cat);
    cat = block_category_new(gettext("Data"),     (Color) CATEGORY_DATA_COLOR);
    BlockCategory* cat_data = block_category_register(cat);
    cat = block_category_new(gettext("Misc."),    (Color) CATEGORY_MISC_COLOR);
    BlockCategory* cat_misc = block_category_register(cat);

    BlockdefImage term_img = (BlockdefImage) {
        .image_ptr = &assets.textures.icon_term,
        .image_color = (BlockdefColor) { 0xff, 0xff, 0xff, 0xff },
    };

    BlockdefImage list_img = (BlockdefImage) {
        .image_ptr = &assets.textures.icon_list,
        .image_color = (BlockdefColor) { 0xff, 0xff, 0xff, 0xff },
    };

    Blockdef* sc_end = blockdef_new("end", BLOCKTYPE_END, (BlockdefColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_end, gettext("End"));
    blockdef_register(vm, sc_end);

    Blockdef* on_start = blockdef_new("on_start", BLOCKTYPE_HAT, (BlockdefColor) { 0xff, 0x77, 0x00, 0xFF }, block_on_start);
    blockdef_add_text(on_start, gettext("When"));
    blockdef_add_image(on_start, (BlockdefImage) { .image_ptr = &assets.textures.button_run, .image_color = (BlockdefColor) { 0x60, 0xff, 0x00, 0xff } });
    blockdef_add_text(on_start, gettext("clicked"));
    blockdef_register(vm, on_start);
    block_category_add_blockdef(cat_control, on_start);

    block_category_add_label(cat_control, gettext("Conditionals"), (Color) CATEGORY_CONTROL_COLOR);

    Blockdef* sc_if = blockdef_new("if", BLOCKTYPE_CONTROL, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_if);
    blockdef_add_text(sc_if, gettext("If"));
    blockdef_add_argument(sc_if, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_if, gettext(", then"));
    blockdef_register(vm, sc_if);
    block_category_add_blockdef(cat_control, sc_if);

    Blockdef* sc_else_if = blockdef_new("else_if", BLOCKTYPE_CONTROLEND, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_else_if);
    blockdef_add_text(sc_else_if, gettext("Else if"));
    blockdef_add_argument(sc_else_if, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_else_if, gettext(", then"));
    blockdef_register(vm, sc_else_if);
    block_category_add_blockdef(cat_control, sc_else_if);

    Blockdef* sc_else = blockdef_new("else", BLOCKTYPE_CONTROLEND, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_else);
    blockdef_add_text(sc_else, gettext("Else"));
    blockdef_register(vm, sc_else);
    block_category_add_blockdef(cat_control, sc_else);

    block_category_add_label(cat_control, gettext("Loops"), (Color) CATEGORY_CONTROL_COLOR);

    Blockdef* sc_loop = blockdef_new("loop", BLOCKTYPE_CONTROL, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_loop);
    blockdef_add_text(sc_loop, gettext("Loop"));
    blockdef_register(vm, sc_loop);
    block_category_add_blockdef(cat_control, sc_loop);

    Blockdef* sc_repeat = blockdef_new("repeat", BLOCKTYPE_CONTROL, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_repeat);
    blockdef_add_text(sc_repeat, gettext("Repeat"));
    blockdef_add_argument(sc_repeat, "10", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_repeat, gettext("times"));
    blockdef_register(vm, sc_repeat);
    block_category_add_blockdef(cat_control, sc_repeat);

    Blockdef* sc_while = blockdef_new("while", BLOCKTYPE_CONTROL, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_while);
    blockdef_add_text(sc_while, gettext("While"));
    blockdef_add_argument(sc_while, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_while);
    block_category_add_blockdef(cat_control, sc_while);

    block_category_add_label(cat_control, gettext("Functions"), (Color) { 0x99, 0x00, 0xff, 0xff });

    Blockdef* sc_define_block = blockdef_new("define_block", BLOCKTYPE_HAT, (BlockdefColor) { 0x99, 0x00, 0xff, 0xff }, block_define_block);
    blockdef_add_image(sc_define_block, (BlockdefImage) { .image_ptr = &assets.textures.icon_special, .image_color = (BlockdefColor) { 0xff, 0xff, 0xff, 0xff } });
    blockdef_add_text(sc_define_block, gettext("Define"));
    blockdef_add_blockdef_editor(sc_define_block);
    blockdef_register(vm, sc_define_block);
    block_category_add_blockdef(cat_control, sc_define_block);

    Blockdef* sc_return = blockdef_new("return", BLOCKTYPE_NORMAL, (BlockdefColor) { 0x99, 0x00, 0xff, 0xff }, block_return);
    blockdef_add_image(sc_return, (BlockdefImage) { .image_ptr = &assets.textures.icon_special, .image_color = (BlockdefColor) { 0xff, 0xff, 0xff, 0xff } });
    blockdef_add_text(sc_return, gettext("Return"));
    blockdef_add_argument(sc_return, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_return);
    block_category_add_blockdef(cat_control, sc_return);

    block_category_add_label(cat_terminal, gettext("Input/Output"), (Color) CATEGORY_TERMINAL_COLOR);

    Blockdef* sc_print = blockdef_new("print", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_print);
    blockdef_add_image(sc_print, term_img);
    blockdef_add_text(sc_print, gettext("Print"));
    blockdef_add_argument(sc_print, gettext("Hello, scrap!"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_print);
    block_category_add_blockdef(cat_terminal, sc_print);

    Blockdef* sc_println = blockdef_new("println", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_println);
    blockdef_add_image(sc_println, term_img);
    blockdef_add_text(sc_println, gettext("Print line"));
    blockdef_add_argument(sc_println, gettext("Hello, scrap!"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_println);
    block_category_add_blockdef(cat_terminal, sc_println);

    Blockdef* sc_input = blockdef_new("input", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_input);
    blockdef_add_image(sc_input, term_img);
    blockdef_add_text(sc_input, gettext("Get input"));
    blockdef_register(vm, sc_input);
    block_category_add_blockdef(cat_terminal, sc_input);

    Blockdef* sc_char = blockdef_new("get_char", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_get_char);
    blockdef_add_image(sc_char, term_img);
    blockdef_add_text(sc_char, gettext("Get char"));
    blockdef_register(vm, sc_char);
    block_category_add_blockdef(cat_terminal, sc_char);

    block_category_add_label(cat_terminal, gettext("Cursor"), (Color) CATEGORY_TERMINAL_COLOR);

    Blockdef* sc_set_cursor = blockdef_new("set_cursor", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_set_cursor);
    blockdef_add_image(sc_set_cursor, term_img);
    blockdef_add_text(sc_set_cursor, gettext("Set cursor X:"));
    blockdef_add_argument(sc_set_cursor, "0", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_set_cursor, gettext("Y:"));
    blockdef_add_argument(sc_set_cursor, "0", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_set_cursor);
    block_category_add_blockdef(cat_terminal, sc_set_cursor);

    Blockdef* sc_cursor_x = blockdef_new("cursor_x", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_cursor_x);
    blockdef_add_image(sc_cursor_x, term_img);
    blockdef_add_text(sc_cursor_x, gettext("Cursor X"));
    blockdef_register(vm, sc_cursor_x);
    block_category_add_blockdef(cat_terminal, sc_cursor_x);

    Blockdef* sc_cursor_y = blockdef_new("cursor_y", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_cursor_y);
    blockdef_add_image(sc_cursor_y, term_img);
    blockdef_add_text(sc_cursor_y, gettext("Cursor Y"));
    blockdef_register(vm, sc_cursor_y);
    block_category_add_blockdef(cat_terminal, sc_cursor_y);

    Blockdef* sc_cursor_max_x = blockdef_new("cursor_max_x", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_cursor_max_x);
    blockdef_add_image(sc_cursor_max_x, term_img);
    blockdef_add_text(sc_cursor_max_x, gettext("Terminal width"));
    blockdef_register(vm, sc_cursor_max_x);
    block_category_add_blockdef(cat_terminal, sc_cursor_max_x);

    Blockdef* sc_cursor_max_y = blockdef_new("cursor_max_y", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_cursor_max_y);
    blockdef_add_image(sc_cursor_max_y, term_img);
    blockdef_add_text(sc_cursor_max_y, gettext("Terminal height"));
    blockdef_register(vm, sc_cursor_max_y);
    block_category_add_blockdef(cat_terminal, sc_cursor_max_y);

    block_category_add_label(cat_terminal, gettext("Colors"), (Color) CATEGORY_TERMINAL_COLOR);

    Blockdef* sc_set_fg_color = blockdef_new("set_fg_color", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_set_fg_color);
    blockdef_add_image(sc_set_fg_color, term_img);
    blockdef_add_text(sc_set_fg_color, gettext("Set text color"));
    blockdef_add_color_input(sc_set_fg_color, (BlockdefColor) { 0xff, 0xff, 0xff, 0xff });
    blockdef_register(vm, sc_set_fg_color);
    block_category_add_blockdef(cat_terminal, sc_set_fg_color);

    Blockdef* sc_set_bg_color = blockdef_new("set_bg_color", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_set_bg_color);
    blockdef_add_image(sc_set_bg_color, term_img);
    blockdef_add_text(sc_set_bg_color, gettext("Set background color"));
    blockdef_add_color_input(sc_set_bg_color, (BlockdefColor) { 0x30, 0x30, 0x30, 0xff });
    blockdef_register(vm, sc_set_bg_color);
    block_category_add_blockdef(cat_terminal, sc_set_bg_color);

    Blockdef* sc_reset_color = blockdef_new("reset_color", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_reset_color);
    blockdef_add_image(sc_reset_color, term_img);
    blockdef_add_text(sc_reset_color, gettext("Reset color"));
    blockdef_register(vm, sc_reset_color);
    block_category_add_blockdef(cat_terminal, sc_reset_color);

    Blockdef* sc_term_clear = blockdef_new("term_clear", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_term_clear);
    blockdef_add_image(sc_term_clear, term_img);
    blockdef_add_text(sc_term_clear, gettext("Clear terminal"));
    blockdef_register(vm, sc_term_clear);
    block_category_add_blockdef(cat_terminal, sc_term_clear);

    Blockdef* sc_term_set_clear = blockdef_new("term_set_clear", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_term_set_clear);
    blockdef_add_image(sc_term_set_clear, term_img);
    blockdef_add_text(sc_term_set_clear, gettext("Set clear color"));
    blockdef_add_color_input(sc_term_set_clear, (BlockdefColor) { 0x00, 0x00, 0x00, 0xff });
    blockdef_register(vm, sc_term_set_clear);
    block_category_add_blockdef(cat_terminal, sc_term_set_clear);

    Blockdef* sc_plus = blockdef_new("plus", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_plus);
    blockdef_add_argument(sc_plus, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_plus, "+");
    blockdef_add_argument(sc_plus, "10", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_plus);
    block_category_add_blockdef(cat_math, sc_plus);

    Blockdef* sc_minus = blockdef_new("minus", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_minus);
    blockdef_add_argument(sc_minus, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_minus, "-");
    blockdef_add_argument(sc_minus, "10", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_minus);
    block_category_add_blockdef(cat_math, sc_minus);

    Blockdef* sc_mult = blockdef_new("mult", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_mult);
    blockdef_add_argument(sc_mult, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_mult, "*");
    blockdef_add_argument(sc_mult, "10", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_mult);
    block_category_add_blockdef(cat_math, sc_mult);

    Blockdef* sc_div = blockdef_new("div", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_div);
    blockdef_add_argument(sc_div, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_div, "/");
    blockdef_add_argument(sc_div, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_div);
    block_category_add_blockdef(cat_math, sc_div);

    Blockdef* sc_rem = blockdef_new("rem", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_rem);
    blockdef_add_argument(sc_rem, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_rem, "%");
    blockdef_add_argument(sc_rem, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_rem);
    block_category_add_blockdef(cat_math, sc_rem);

    Blockdef* sc_pow = blockdef_new("pow", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_pow);
    blockdef_add_text(sc_pow, gettext("Pow"));
    blockdef_add_argument(sc_pow, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_argument(sc_pow, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_pow);
    block_category_add_blockdef(cat_math, sc_pow);

    Blockdef* sc_math = blockdef_new("math", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_math);
    blockdef_add_dropdown(sc_math, DROPDOWN_SOURCE_LISTREF, math_list_access);
    blockdef_add_argument(sc_math, "", "0.0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_math);
    block_category_add_blockdef(cat_math, sc_math);

    Blockdef* sc_pi = blockdef_new("pi", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_pi);
    blockdef_add_image(sc_pi, (BlockdefImage) { .image_ptr = &assets.textures.icon_pi, .image_color = (BlockdefColor) { 0xff, 0xff, 0xff, 0xff } });
    blockdef_register(vm, sc_pi);
    block_category_add_blockdef(cat_math, sc_pi);

    block_category_add_label(cat_logic, gettext("Comparisons"), (Color) CATEGORY_LOGIC_COLOR);

    Blockdef* sc_less = blockdef_new("less", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_less);
    blockdef_add_argument(sc_less, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_less, "<");
    blockdef_add_argument(sc_less, "11", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_less);
    block_category_add_blockdef(cat_logic, sc_less);

    Blockdef* sc_less_eq = blockdef_new("less_eq", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_less_eq);
    blockdef_add_argument(sc_less_eq, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_less_eq, "<=");
    blockdef_add_argument(sc_less_eq, "11", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_less_eq);
    block_category_add_blockdef(cat_logic, sc_less_eq);

    Blockdef* sc_eq = blockdef_new("eq", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_eq);
    blockdef_add_argument(sc_eq, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_eq, "=");
    blockdef_add_argument(sc_eq, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_eq);
    block_category_add_blockdef(cat_logic, sc_eq);

    Blockdef* sc_not_eq = blockdef_new("not_eq", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_not_eq);
    blockdef_add_argument(sc_not_eq, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_not_eq, "!=");
    blockdef_add_argument(sc_not_eq, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_not_eq);
    block_category_add_blockdef(cat_logic, sc_not_eq);

    Blockdef* sc_more_eq = blockdef_new("more_eq", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_more_eq);
    blockdef_add_argument(sc_more_eq, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_more_eq, ">=");
    blockdef_add_argument(sc_more_eq, "11", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_more_eq);
    block_category_add_blockdef(cat_logic, sc_more_eq);

    Blockdef* sc_more = blockdef_new("more", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_more);
    blockdef_add_argument(sc_more, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_more, ">");
    blockdef_add_argument(sc_more, "11", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_more);
    block_category_add_blockdef(cat_logic, sc_more);

    block_category_add_label(cat_logic, gettext("Boolean math"), (Color) CATEGORY_LOGIC_COLOR);

    Blockdef* sc_not = blockdef_new("not", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_not);
    blockdef_add_text(sc_not, gettext("Not"));
    blockdef_add_argument(sc_not, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_not);
    block_category_add_blockdef(cat_logic, sc_not);

    Blockdef* sc_and = blockdef_new("and", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_and);
    blockdef_add_argument(sc_and, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_and, gettext("and"));
    blockdef_add_argument(sc_and, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_and);
    block_category_add_blockdef(cat_logic, sc_and);

    Blockdef* sc_or = blockdef_new("or", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_or);
    blockdef_add_argument(sc_or, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_or, gettext("or"));
    blockdef_add_argument(sc_or, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_or);
    block_category_add_blockdef(cat_logic, sc_or);

    Blockdef* sc_true = blockdef_new("true", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_true);
    blockdef_add_text(sc_true, gettext("True"));
    blockdef_register(vm, sc_true);
    block_category_add_blockdef(cat_logic, sc_true);

    Blockdef* sc_false = blockdef_new("false", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_false);
    blockdef_add_text(sc_false, gettext("False"));
    blockdef_register(vm, sc_false);
    block_category_add_blockdef(cat_logic, sc_false);

    block_category_add_label(cat_logic, gettext("Bitwise math"), (Color) CATEGORY_LOGIC_COLOR);

    Blockdef* sc_bit_not = blockdef_new("bit_not", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_bit_not);
    blockdef_add_text(sc_bit_not, "~");
    blockdef_add_argument(sc_bit_not, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_not);
    block_category_add_blockdef(cat_logic, sc_bit_not);

    Blockdef* sc_bit_and = blockdef_new("bit_and", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_bit_and);
    blockdef_add_argument(sc_bit_and, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_and, "&");
    blockdef_add_argument(sc_bit_and, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_and);
    block_category_add_blockdef(cat_logic, sc_bit_and);

    Blockdef* sc_bit_or = blockdef_new("bit_or", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_bit_or);
    blockdef_add_argument(sc_bit_or, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_or, "|");
    blockdef_add_argument(sc_bit_or, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_or);
    block_category_add_blockdef(cat_logic, sc_bit_or);

    Blockdef* sc_bit_xor = blockdef_new("bit_xor", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_bit_xor);
    blockdef_add_argument(sc_bit_xor, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_xor, "^");
    blockdef_add_argument(sc_bit_xor, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_xor);
    block_category_add_blockdef(cat_logic, sc_bit_xor);

    block_category_add_label(cat_misc, gettext("System"), (Color) CATEGORY_MISC_COLOR);

    Blockdef* sc_sleep = blockdef_new("sleep", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_sleep);
    blockdef_add_text(sc_sleep, gettext("Sleep"));
    blockdef_add_argument(sc_sleep, "", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_sleep, gettext("μs"));
    blockdef_register(vm, sc_sleep);
    block_category_add_blockdef(cat_misc, sc_sleep);

    Blockdef* sc_random = blockdef_new("random", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_random);
    blockdef_add_text(sc_random, gettext("Random from"));
    blockdef_add_argument(sc_random, "0", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_random, gettext("to"));
    blockdef_add_argument(sc_random, "10", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_random);
    block_category_add_blockdef(cat_misc, sc_random);

    Blockdef* sc_unix_time = blockdef_new("unix_time", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_unix_time);
    blockdef_add_text(sc_unix_time, gettext("Time since 1970"));
    blockdef_register(vm, sc_unix_time);
    block_category_add_blockdef(cat_misc, sc_unix_time);

    block_category_add_label(cat_misc, gettext("Type casting"), (Color) CATEGORY_MISC_COLOR);

    Blockdef* sc_int = blockdef_new("convert_int", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_convert_int);
    blockdef_add_text(sc_int, gettext("Int"));
    blockdef_add_argument(sc_int, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_int);
    block_category_add_blockdef(cat_misc, sc_int);

    Blockdef* sc_float = blockdef_new("convert_float", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_convert_float);
    blockdef_add_text(sc_float, gettext("Float"));
    blockdef_add_argument(sc_float, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_float);
    block_category_add_blockdef(cat_misc, sc_float);

    Blockdef* sc_str = blockdef_new("convert_str", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_convert_str);
    blockdef_add_text(sc_str, gettext("Str"));
    blockdef_add_argument(sc_str, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_str);
    block_category_add_blockdef(cat_misc, sc_str);

    Blockdef* sc_bool = blockdef_new("convert_bool", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_convert_bool);
    blockdef_add_text(sc_bool, gettext("Bool"));
    blockdef_add_argument(sc_bool, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bool);
    block_category_add_blockdef(cat_misc, sc_bool);

    Blockdef* sc_color = blockdef_new("convert_color", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_convert_color);
    blockdef_add_text(sc_color, gettext("Color"));
    blockdef_add_color_input(sc_color, (BlockdefColor) { 0x00, 0xff, 0x00, 0xff });
    blockdef_register(vm, sc_color);
    block_category_add_blockdef(cat_misc, sc_color);

    Blockdef* sc_typeof = blockdef_new("typeof", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_typeof);
    blockdef_add_text(sc_typeof, gettext("Type of"));
    blockdef_add_argument(sc_typeof, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_typeof);
    block_category_add_blockdef(cat_misc, sc_typeof);

    block_category_add_label(cat_misc, gettext("Doing nothing"), (Color) { 0x77, 0x77, 0x77, 0xff });

    Blockdef* sc_nothing = blockdef_new("nothing", BLOCKTYPE_NORMAL, (BlockdefColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_nothing, gettext("Nothing"));
    blockdef_register(vm, sc_nothing);
    block_category_add_blockdef(cat_misc, sc_nothing);

    Blockdef* sc_do_nothing = blockdef_new("do_nothing", BLOCKTYPE_CONTROL, (BlockdefColor) { 0x77, 0x77, 0x77, 0xff }, block_do_nothing);
    blockdef_add_text(sc_do_nothing, gettext("Do nothing"));
    blockdef_register(vm, sc_do_nothing);
    block_category_add_blockdef(cat_misc, sc_do_nothing);

    Blockdef* sc_comment = blockdef_new("comment", BLOCKTYPE_NORMAL, (BlockdefColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_comment, "//");
    blockdef_add_argument(sc_comment, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_comment);
    block_category_add_blockdef(cat_misc, sc_comment);

#ifdef DEBUG
    block_category_add_label(cat_misc, gettext("Debug blocks"), (Color) { 0xa0, 0x70, 0x00, 0xff });

    Blockdef* sc_gc_collect = blockdef_new("gc_collect", BLOCKTYPE_NORMAL, (BlockdefColor) { 0xa0, 0x70, 0x00, 0xff }, block_gc_collect);
    blockdef_add_text(sc_gc_collect, gettext("Collect garbage"));
    blockdef_register(vm, sc_gc_collect);
    block_category_add_blockdef(cat_misc, sc_gc_collect);
#endif

    block_category_add_label(cat_data, gettext("Variables"), (Color) CATEGORY_DATA_COLOR);

    Blockdef* sc_decl_var = blockdef_new("decl_var", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_DATA_COLOR, block_declare_var);
    blockdef_add_image(sc_decl_var, (BlockdefImage) { .image_ptr = &assets.textures.icon_variable, .image_color = (BlockdefColor) { 0xff, 0xff, 0xff, 0xff } });
    blockdef_add_text(sc_decl_var, gettext("Declare"));
    blockdef_add_argument(sc_decl_var, gettext("my variable"), gettext("Abc"), BLOCKCONSTR_STRING);
    blockdef_add_text(sc_decl_var, "=");
    blockdef_add_argument(sc_decl_var, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_decl_var);
    block_category_add_blockdef(cat_data, sc_decl_var);

    Blockdef* sc_get_var = blockdef_new("get_var", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_DATA_COLOR, block_get_var);
    blockdef_add_image(sc_get_var, (BlockdefImage) { .image_ptr = &assets.textures.icon_variable, .image_color = (BlockdefColor) { 0xff, 0xff, 0xff, 0xff } });
    blockdef_add_argument(sc_get_var, gettext("my variable"), gettext("Abc"), BLOCKCONSTR_STRING);
    blockdef_register(vm, sc_get_var);
    block_category_add_blockdef(cat_data, sc_get_var);

    Blockdef* sc_set_var = blockdef_new("set_var", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_DATA_COLOR, block_set_var);
    blockdef_add_image(sc_set_var, (BlockdefImage) { .image_ptr = &assets.textures.icon_variable, .image_color = (BlockdefColor) { 0xff, 0xff, 0xff, 0xff } });
    blockdef_add_text(sc_set_var, gettext("Set"));
    blockdef_add_argument(sc_set_var, gettext("my variable"), gettext("Abc"), BLOCKCONSTR_STRING);
    blockdef_add_text(sc_set_var, "=");
    blockdef_add_argument(sc_set_var, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_set_var);
    block_category_add_blockdef(cat_data, sc_set_var);

    block_category_add_label(cat_data, gettext("Strings"), (Color) CATEGORY_STRING_COLOR);

    Blockdef* sc_join = blockdef_new("join", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_join);
    blockdef_add_text(sc_join, gettext("Join"));
    blockdef_add_argument(sc_join, gettext("left and "), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_argument(sc_join, gettext("right"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_join);
    block_category_add_blockdef(cat_data, sc_join);

    Blockdef* sc_letter_in = blockdef_new("letter_in", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_letter_in);
    blockdef_add_text(sc_letter_in, gettext("Letter"));
    blockdef_add_argument(sc_letter_in, "1", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_letter_in, gettext("in"));
    blockdef_add_argument(sc_letter_in, gettext("string"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_letter_in);
    block_category_add_blockdef(cat_data, sc_letter_in);

    Blockdef* sc_substring = blockdef_new("substring", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_substring);
    blockdef_add_text(sc_substring, gettext("Substring"));
    blockdef_add_argument(sc_substring, "2", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_substring, gettext("to"));
    blockdef_add_argument(sc_substring, "4", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_substring, gettext("in"));
    blockdef_add_argument(sc_substring, gettext("string"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_substring);
    block_category_add_blockdef(cat_data, sc_substring);

    Blockdef* sc_length = blockdef_new("length", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_length);
    blockdef_add_text(sc_length, gettext("Length"));
    blockdef_add_argument(sc_length, gettext("string"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_length);
    block_category_add_blockdef(cat_data, sc_length);

    Blockdef* sc_ord = blockdef_new("ord", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_ord);
    blockdef_add_text(sc_ord, gettext("Ord"));
    blockdef_add_argument(sc_ord, "A", gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_ord);
    block_category_add_blockdef(cat_data, sc_ord);

    Blockdef* sc_chr = blockdef_new("chr", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_chr);
    blockdef_add_text(sc_chr, gettext("Chr"));
    blockdef_add_argument(sc_chr, "65", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_chr);
    block_category_add_blockdef(cat_data, sc_chr);

    block_category_add_label(cat_data, gettext("Lists"), (Color) { 0xff, 0x44, 0x00, 0xff });

    Blockdef* sc_create_list = blockdef_new("create_list", BLOCKTYPE_NORMAL, (BlockdefColor) { 0xff, 0x44, 0x00, 0xff }, block_create_list);
    blockdef_add_image(sc_create_list, list_img);
    blockdef_add_text(sc_create_list, gettext("Empty list"));
    blockdef_register(vm, sc_create_list);
    block_category_add_blockdef(cat_data, sc_create_list);

    Blockdef* sc_list_add = blockdef_new("list_add", BLOCKTYPE_NORMAL, (BlockdefColor) { 0xff, 0x44, 0x00, 0xff }, block_list_add);
    blockdef_add_image(sc_list_add, list_img);
    blockdef_add_text(sc_list_add, gettext("Add"));
    blockdef_add_argument(sc_list_add, "", gettext("list"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_add, gettext("value"));
    blockdef_add_argument(sc_list_add, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_list_add);
    block_category_add_blockdef(cat_data, sc_list_add);

    Blockdef* sc_list_get = blockdef_new("list_get", BLOCKTYPE_NORMAL, (BlockdefColor) { 0xff, 0x44, 0x00, 0xff }, block_list_get);
    blockdef_add_image(sc_list_get, list_img);
    blockdef_add_argument(sc_list_get, "", gettext("list"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_get, gettext("at"));
    blockdef_add_argument(sc_list_get, "0", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_list_get);
    block_category_add_blockdef(cat_data, sc_list_get);

    Blockdef* sc_list_set = blockdef_new("list_set", BLOCKTYPE_NORMAL, (BlockdefColor) { 0xff, 0x44, 0x00, 0xff }, block_list_set);
    blockdef_add_image(sc_list_set, list_img);
    blockdef_add_text(sc_list_set, gettext("Set"));
    blockdef_add_argument(sc_list_set, "", gettext("list"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_set, gettext("at"));
    blockdef_add_argument(sc_list_set, "0", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_set, "=");
    blockdef_add_argument(sc_list_set, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_list_set);
    block_category_add_blockdef(cat_data, sc_list_set);

    Blockdef* sc_list_len = blockdef_new("list_length", BLOCKTYPE_NORMAL, (BlockdefColor) { 0xff, 0x44, 0x00, 0xff }, block_list_length);
    blockdef_add_image(sc_list_len, list_img);
    blockdef_add_text(sc_list_len, gettext("Length"));
    blockdef_add_argument(sc_list_len, "", gettext("list"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_list_len);
    block_category_add_blockdef(cat_data, sc_list_len);
}
