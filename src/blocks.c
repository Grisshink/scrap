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

typedef struct {
    ConstId label;
    IrBytecode bc;
    size_t var_slot;
} ControlData;

typedef struct {
    ConstId label;
    IrBytecode bc;
    size_t arg_count;
} CustomFunctionData;

static MathFunc block_math_func_list[MATH_LIST_LEN] = {
    sqrt, round, floor, ceil,
    sin, cos, tan,
    asin, acos, atan,
};

#include "std.h"
#include <stdio.h>

CompilerValue cast_to_const_string(Compiler* compiler, CompilerValue value);
CompilerValue cast_to_const_int(Compiler* compiler, CompilerValue value);
CompilerValue cast_to_const_float(Compiler* compiler, CompilerValue value);
CompilerValue cast_to_const_bool(Compiler* compiler, CompilerValue value);
CompilerValue cast_to_const_color(Compiler* compiler, CompilerValue value);
CompilerValue cast_to_const_nothing(Compiler* compiler, CompilerValue value);
CompilerValue cast_to_const_list(Compiler* compiler, CompilerValue value);

CompilerValue cast_to_bc(Compiler* compiler, CompilerValue value, DataType dst_type);

CompilerValue string_to_bc(Compiler* compiler, char* str) {
    IrBytecode bc = EMPTY_BYTECODE;
    IrList* list = bytecode_const_list_new(compiler->bc_pool);

    int codepoint_size = 0;
    for (char* ch = str; *ch; ch += codepoint_size) {
        int codepoint = GetCodepointNext(ch, &codepoint_size);

        bytecode_const_list_append(compiler->bc_pool, list, (IrValue) {
            .type = IR_TYPE_INT,
            .as.int_val = codepoint,
        });
    }

    bytecode_push_op_list_string(&bc, IR_PUSHA, list);
    return DATA_CHUNK(DATA_TYPE_STRING, bc);
}

CompilerValue cast_to_bc_string(Compiler* compiler, CompilerValue value) {
    IrBytecode bc;
    const DataType result_type = DATA_TYPE_STRING;
    char str[32];

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_bc_string");
    switch (value.type) {
    case DATA_TYPE_ANY:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_NULL:
    case DATA_TYPE_BLOCKDEF:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(result_type));
        return DATA_ERROR;
    case DATA_TYPE_LIST:
        snprintf(str, 32, "[List: Empty]");
        return string_to_bc(compiler, str);
    case DATA_TYPE_COLOR:
        snprintf(str, 32, "#%02x%02x%02x%02x", value.data.color_val.r, value.data.color_val.g, value.data.color_val.b, value.data.color_val.a);
        return string_to_bc(compiler, str);
    case DATA_TYPE_NOTHING:
        return string_to_bc(compiler, "nothing");
    case DATA_TYPE_INTEGER:
        snprintf(str, 32, "%d", value.data.integer_val);
        return string_to_bc(compiler, str);
    case DATA_TYPE_FLOAT:
        snprintf(str, 32, "%gf", value.data.float_val);
        return string_to_bc(compiler, str);
    case DATA_TYPE_BOOL:
        return string_to_bc(compiler, value.data.bool_val ? "true" : "false");
    case DATA_TYPE_STRING:
        return string_to_bc(compiler, value.data.str_val);
    case DATA_TYPE_CHUNK:
        bc = value.data.chunk_val.bc;

        static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_bc_string");
        switch (value.data.chunk_val.return_type) {
        case DATA_TYPE_UNKNOWN:
        case DATA_TYPE_NULL:
        case DATA_TYPE_BLOCKDEF:
        case DATA_TYPE_CHUNK:
            compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.data.chunk_val.return_type), type_to_str(result_type));
            return DATA_ERROR;
        case DATA_TYPE_LIST:
            bytecode_push_op(&bc, IR_LTOA);
            return DATA_CHUNK(result_type, bc);
        case DATA_TYPE_NOTHING:
            bytecode_push_op(&bc, IR_NTOA);
            return DATA_CHUNK(result_type, bc);
        case DATA_TYPE_COLOR:
            bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_color_to_string"));
            return DATA_CHUNK(result_type, bc);
        case DATA_TYPE_INTEGER:
            bytecode_push_op(&bc, IR_ITOA);
            return DATA_CHUNK(result_type, bc);
        case DATA_TYPE_FLOAT:
            bytecode_push_op(&bc, IR_FTOA);
            return DATA_CHUNK(result_type, bc);
        case DATA_TYPE_STRING:
            return value;
        case DATA_TYPE_BOOL:
            bytecode_push_op(&bc, IR_BTOA);
            return DATA_CHUNK(result_type, bc);
        case DATA_TYPE_ANY:
            bytecode_push_op(&bc, IR_TOA);
            return DATA_CHUNK(result_type, bc);
        default:
            assert(false && "Unhandled data type in cast_to_bc_string");
        }
        assert(false && "Unreachable");
    default:
        assert(false && "Unhandled data type in cast_to_bc_string");
    }
}

CompilerValue cast_to_bc_int(Compiler* compiler, CompilerValue value) {
    const DataType result_type = DATA_TYPE_INTEGER;
    IrBytecode bc;

    if (value.type != DATA_TYPE_CHUNK) {
        CompilerValue integer_val = cast_to_const_int(compiler, value);
        if (integer_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

        bc = EMPTY_BYTECODE;
        bytecode_push_op_int(&bc, IR_PUSHI, integer_val.data.integer_val);
        return DATA_CHUNK(result_type, bc);
    }

    bc = value.data.chunk_val.bc;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_bc_int");
    switch (value.data.chunk_val.return_type) {
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_NULL:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_LIST:
    case DATA_TYPE_CHUNK:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.data.chunk_val.return_type), type_to_str(result_type));
        return DATA_ERROR;
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_ANY:
        bytecode_push_op(&bc, IR_TOI);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_COLOR:
    case DATA_TYPE_INTEGER:
        return value;
    case DATA_TYPE_FLOAT:
        bytecode_push_op(&bc, IR_FTOI);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_STRING:
        bytecode_push_op(&bc, IR_ATOI);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_BOOL:
        bytecode_push_op(&bc, IR_BTOI);
        return DATA_CHUNK(result_type, bc);
    default:
        assert(false && "Unhandled data type in cast_to_bc_int");
    }
}

CompilerValue cast_to_bc_float(Compiler* compiler, CompilerValue value) {
    const DataType result_type = DATA_TYPE_FLOAT;
    IrBytecode bc;

    if (value.type != DATA_TYPE_CHUNK) {
        CompilerValue float_val = cast_to_const_float(compiler, value);
        if (float_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

        bc = EMPTY_BYTECODE;
        bytecode_push_op_float(&bc, IR_PUSHF, float_val.data.float_val);
        return DATA_CHUNK(result_type, bc);
    }

    bc = value.data.chunk_val.bc;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_bc_float");
    switch (value.data.chunk_val.return_type) {
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_NULL:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_LIST:
    case DATA_TYPE_CHUNK:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.data.chunk_val.return_type), type_to_str(result_type));
        return DATA_ERROR;
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_ANY:
        bytecode_push_op(&bc, IR_TOF);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_COLOR:
    case DATA_TYPE_INTEGER:
        bytecode_push_op(&bc, IR_ITOF);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_FLOAT:
        return value;
    case DATA_TYPE_STRING:
        bytecode_push_op(&bc, IR_ATOF);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_BOOL:
        bytecode_push_op(&bc, IR_BTOF);
        return DATA_CHUNK(result_type, bc);
    default:
        assert(false && "Unhandled data type in cast_to_bc_float");
    }
}

CompilerValue cast_to_bc_bool(Compiler* compiler, CompilerValue value) {
    const DataType result_type = DATA_TYPE_BOOL;
    IrBytecode bc;

    if (value.type != DATA_TYPE_CHUNK) {
        CompilerValue bool_val = cast_to_const_bool(compiler, value);
        if (bool_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

        bc = EMPTY_BYTECODE;
        bytecode_push_op_bool(&bc, IR_PUSHB, bool_val.data.bool_val);
        return DATA_CHUNK(result_type, bc);
    }

    bc = value.data.chunk_val.bc;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_bc_bool");
    switch (value.data.chunk_val.return_type) {
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_NULL:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_LIST:
    case DATA_TYPE_CHUNK:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.data.chunk_val.return_type), type_to_str(result_type));
        return DATA_ERROR;
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_ANY:
        bytecode_push_op(&bc, IR_TOB);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_COLOR:
    case DATA_TYPE_INTEGER:
        bytecode_push_op(&bc, IR_ITOB);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_FLOAT:
        bytecode_push_op(&bc, IR_FTOB);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_STRING:
        bytecode_push_op(&bc, IR_ATOB);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_BOOL:
        return value;
    default:
        assert(false && "Unhandled data type in cast_to_bc_bool");
    }
}

CompilerValue cast_to_bc_color(Compiler* compiler, CompilerValue value) {
    const DataType result_type = DATA_TYPE_COLOR;
    IrBytecode bc;

    if (value.type != DATA_TYPE_CHUNK) {
        CompilerValue color_val = cast_to_const_color(compiler, value);
        if (color_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

        bc = EMPTY_BYTECODE;
        bytecode_push_op_int(&bc, IR_PUSHI, *(int*)&color_val.data.color_val);
        return DATA_CHUNK(result_type, bc);
    }

    bc = value.data.chunk_val.bc;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_bc_color");
    switch (value.data.chunk_val.return_type) {
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_NULL:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_LIST:
    case DATA_TYPE_CHUNK:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.data.chunk_val.return_type), type_to_str(result_type));
        return DATA_ERROR;
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_ANY:
        bytecode_push_op(&bc, IR_TOI);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_COLOR:
    case DATA_TYPE_INTEGER:
        return value;
    case DATA_TYPE_FLOAT:
        bytecode_push_op(&bc, IR_FTOI);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_STRING:
        bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_string_to_color"));
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_BOOL:
        bytecode_push_op(&bc, IR_BTOI);
        return DATA_CHUNK(result_type, bc);
    default:
        assert(false && "Unhandled data type in cast_to_bc_color");
    }
}

CompilerValue cast_to_bc_nothing(Compiler* compiler, CompilerValue value) {
    const DataType result_type = DATA_TYPE_NOTHING;

    if (value.type != DATA_TYPE_CHUNK) {
        CompilerValue nothing_val = cast_to_const_nothing(compiler, value);
        if (nothing_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

        IrBytecode bc = EMPTY_BYTECODE;
        bytecode_push_op(&bc, IR_PUSHN);
        return DATA_CHUNK(result_type, bc);
    }

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_bc_nothing");
    switch (value.data.chunk_val.return_type) {
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_NULL:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_LIST:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_ANY:
    case DATA_TYPE_COLOR:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_FLOAT:
    case DATA_TYPE_STRING:
    case DATA_TYPE_BOOL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.data.chunk_val.return_type), type_to_str(result_type));
        return DATA_ERROR;
    case DATA_TYPE_NOTHING:
        return value;
    default:
        assert(false && "Unhandled data type in cast_to_bc_nothing");
    }
}

CompilerValue cast_to_bc_list(Compiler* compiler, CompilerValue value) {
    const DataType result_type = DATA_TYPE_LIST;
    IrBytecode bc;

    if (value.type != DATA_TYPE_CHUNK) {
        CompilerValue list_val = cast_to_const_list(compiler, value);
        if (list_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

        bc = EMPTY_BYTECODE;
        bytecode_push_op_list(&bc, IR_PUSHL, list_val.data.list_val);
        return DATA_CHUNK(result_type, bc);
    }

    bc = value.data.chunk_val.bc;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_bc_list");
    switch (value.data.chunk_val.return_type) {
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_NULL:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_COLOR:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_FLOAT:
    case DATA_TYPE_STRING:
    case DATA_TYPE_BOOL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.data.chunk_val.return_type), type_to_str(result_type));
        return DATA_ERROR;
    case DATA_TYPE_ANY:
        bytecode_push_op(&bc, IR_TOL);
        return DATA_CHUNK(result_type, bc);
    case DATA_TYPE_LIST:
        return value;
    default:
        assert(false && "Unhandled data type in cast_to_bc_list");
    }
}

CompilerValue cast_to_bc_any(Compiler* compiler, CompilerValue value) {
    const DataType result_type = DATA_TYPE_ANY;

    if (value.type != DATA_TYPE_CHUNK) {
        value = cast_to_bc(compiler, value, value.type);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
    }

    return DATA_CHUNK(result_type, value.data.chunk_val.bc);
}

CompilerValue cast_to_bc(Compiler* compiler, CompilerValue value, DataType dst_type) {
    DataType src_type = value.type;
    if (src_type == DATA_TYPE_CHUNK) {
        src_type = value.data.chunk_val.return_type;
        if (src_type == dst_type) return value;
    }

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_bc");
    switch (dst_type) {
    case DATA_TYPE_INTEGER: return cast_to_bc_int(compiler, value);
    case DATA_TYPE_FLOAT: return cast_to_bc_float(compiler, value);
    case DATA_TYPE_STRING: return cast_to_bc_string(compiler, value);
    case DATA_TYPE_BOOL: return cast_to_bc_bool(compiler, value);
    case DATA_TYPE_COLOR: return cast_to_bc_color(compiler, value);
    case DATA_TYPE_NOTHING: return cast_to_bc_nothing(compiler, value);
    case DATA_TYPE_LIST: return cast_to_bc_list(compiler, value);
    case DATA_TYPE_ANY: return cast_to_bc_any(compiler, value);
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_NULL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(src_type), type_to_str(dst_type));
        return DATA_ERROR;
    default:
        assert(false && "Unhandled data type in cast_to_bc");
    }
}

CompilerValue cast_to_const_string(Compiler* compiler, CompilerValue value) {
    DataType dst_type = DATA_TYPE_STRING;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_const_string");
    switch (value.type) {
    case DATA_TYPE_STRING: return value;
    case DATA_TYPE_NOTHING: return DATA_STRING("nothing");
    case DATA_TYPE_INTEGER:
        return DATA_STRING(ir_arena_sprintf(compiler->arena, 32, "%d", value.data.integer_val));
    case DATA_TYPE_COLOR:
        return DATA_STRING(ir_arena_sprintf(compiler->arena, 32, "#%02x%02x%02x%02x", value.data.color_val.r, value.data.color_val.g, value.data.color_val.b, value.data.color_val.a));
    case DATA_TYPE_FLOAT:
        return DATA_STRING(ir_arena_sprintf(compiler->arena, 32, "%gf", value.data.float_val));
    case DATA_TYPE_BOOL: return DATA_STRING(value.data.bool_val ? "true" : "false");
    case DATA_TYPE_LIST: return DATA_STRING("[List: Empty]");
    case DATA_TYPE_ANY:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_NULL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(dst_type));
        return DATA_ERROR;
    default:
        assert(false && "Unhandled data type in cast_to_const_string");
    }
}

CompilerValue cast_to_const_int(Compiler* compiler, CompilerValue value) {
    DataType dst_type = DATA_TYPE_INTEGER;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_const_int");
    switch (value.type) {
    case DATA_TYPE_INTEGER: return value;
    case DATA_TYPE_FLOAT: return DATA_INTEGER(value.data.float_val);
    case DATA_TYPE_STRING: return DATA_INTEGER(atoi(value.data.str_val));
    case DATA_TYPE_BOOL: return DATA_INTEGER(value.data.bool_val);
    case DATA_TYPE_COLOR: return DATA_INTEGER(*(int*)&value.data.color_val);
    case DATA_TYPE_NOTHING: return DATA_INTEGER(0);
    case DATA_TYPE_LIST:
    case DATA_TYPE_ANY:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_NULL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(dst_type));
        return DATA_ERROR;
    default:
        assert(false && "Unhandled data type in cast_to_const_int");
    }
}

CompilerValue cast_to_const_float(Compiler* compiler, CompilerValue value) {
    DataType dst_type = DATA_TYPE_FLOAT;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_const_float");
    switch (value.type) {
    case DATA_TYPE_INTEGER: return DATA_FLOAT(value.data.integer_val);
    case DATA_TYPE_FLOAT: return value;
    case DATA_TYPE_STRING: return DATA_FLOAT(atof(value.data.str_val));
    case DATA_TYPE_BOOL: return DATA_FLOAT(value.data.bool_val);
    case DATA_TYPE_COLOR: return DATA_FLOAT(*(int*)&value.data.color_val);
    case DATA_TYPE_NOTHING: return DATA_FLOAT(0.0);
    case DATA_TYPE_LIST:
    case DATA_TYPE_ANY:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_NULL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(dst_type));
        return DATA_ERROR;
    default:
        assert(false && "Unhandled data type in cast_to_const_float");
    }
}

CompilerValue cast_to_const_bool(Compiler* compiler, CompilerValue value) {
    DataType dst_type = DATA_TYPE_BOOL;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_const_bool");
    switch (value.type) {
    case DATA_TYPE_INTEGER: return DATA_BOOL(value.data.integer_val != 0);
    case DATA_TYPE_FLOAT: return DATA_BOOL(value.data.float_val != 0);
    case DATA_TYPE_STRING: return DATA_BOOL(*value.data.str_val != 0);
    case DATA_TYPE_BOOL: return value;
    case DATA_TYPE_COLOR: return DATA_BOOL(*(int*)&value.data.color_val != 0);
    case DATA_TYPE_NOTHING: return DATA_BOOL(false);
    case DATA_TYPE_LIST:
    case DATA_TYPE_ANY:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_NULL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(dst_type));
        return DATA_ERROR;
    default:
        assert(false && "Unhandled data type in cast_to_const_bool");
    }
}

CompilerValue cast_to_const_color(Compiler* compiler, CompilerValue value) {
    DataType dst_type = DATA_TYPE_COLOR;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_const_color");
    switch (value.type) {
    case DATA_TYPE_INTEGER: return DATA_COLOR(*(Color*)&value.data.integer_val);
    case DATA_TYPE_FLOAT: return DATA_COLOR(*(Color*)&value.data.float_val);
    case DATA_TYPE_STRING:
        char* str = value.data.str_val;
        if (*str == '#') str++;
        unsigned char r = 0x00, g = 0x00, b = 0x00, a = 0xff;
        sscanf(str, "%02hhx%02hhx%02hhx%02hhx", &r, &g, &b, &a);
        return DATA_COLOR(((Color) { r, g, b, a }));
    case DATA_TYPE_BOOL: return DATA_COLOR(value.data.bool_val ? ((Color) { 0xff, 0xff, 0xff, 0xff }) : ((Color) { 0x00, 0x00, 0x00, 0xff }));
    case DATA_TYPE_COLOR: return value;
    case DATA_TYPE_NOTHING: return DATA_COLOR(((Color) { 0x00, 0x00, 0x00, 0xff }));
    case DATA_TYPE_LIST:
    case DATA_TYPE_ANY:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_NULL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(dst_type));
        return DATA_ERROR;
    default:
        assert(false && "Unhandled data type in cast_to_const_color");
    }
}

CompilerValue cast_to_const_nothing(Compiler* compiler, CompilerValue value) {
    DataType dst_type = DATA_TYPE_NOTHING;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_const_nothing");
    switch (value.type) {
    case DATA_TYPE_NOTHING:
        return value;
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_FLOAT:
    case DATA_TYPE_STRING:
    case DATA_TYPE_BOOL:
    case DATA_TYPE_COLOR:
    case DATA_TYPE_LIST:
    case DATA_TYPE_ANY:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_NULL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(dst_type));
        return DATA_ERROR;
    default:
        assert(false && "Unhandled data type in cast_to_const_nothing");
    }
}

CompilerValue cast_to_const_list(Compiler* compiler, CompilerValue value) {
    DataType dst_type = DATA_TYPE_LIST;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_const_list");
    switch (value.type) {
    case DATA_TYPE_LIST:
        return value;
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_FLOAT:
    case DATA_TYPE_STRING:
    case DATA_TYPE_BOOL:
    case DATA_TYPE_COLOR:
    case DATA_TYPE_ANY:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_NULL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(value.type), type_to_str(dst_type));
        return DATA_ERROR;
    default:
        assert(false && "Unhandled data type in cast_to_const_list");
    }
}

CompilerValue cast_to_const(Compiler* compiler, CompilerValue value, DataType dst_type) {
    DataType src_type = value.type;
    if (src_type == dst_type) return value;

    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in cast_to_const");
    switch (dst_type) {
    case DATA_TYPE_INTEGER: return cast_to_const_int(compiler, value);
    case DATA_TYPE_FLOAT: return cast_to_const_float(compiler, value);
    case DATA_TYPE_STRING: return cast_to_const_string(compiler, value);
    case DATA_TYPE_BOOL: return cast_to_const_bool(compiler, value);
    case DATA_TYPE_COLOR: return cast_to_const_color(compiler, value);
    case DATA_TYPE_LIST: return cast_to_const_list(compiler, value);
    case DATA_TYPE_CHUNK: return cast_to_bc(compiler, value, value.type);
    case DATA_TYPE_NOTHING: return cast_to_const_nothing(compiler, value);
    case DATA_TYPE_ANY:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_NULL:
        compiler_set_error(compiler, gettext("Cannot cast type %s into %s"), type_to_str(src_type), type_to_str(dst_type));
        return DATA_ERROR;
    default:
        assert(false && "Unhandled data type in cast_to_const");
    }
}

CompilerValue cast_to(Compiler* compiler, CompilerValue value, DataType dst_type) {
    if (value.type == DATA_TYPE_CHUNK) {
        return cast_to_bc(compiler, value, dst_type);
    } else {
        return cast_to_const(compiler, value, dst_type);
    }
}

bool is_type_storable(DataType type) {
    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in is_type_storable");
    switch (type) {
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_FLOAT:
    case DATA_TYPE_STRING:
    case DATA_TYPE_BOOL:
    case DATA_TYPE_COLOR:
    case DATA_TYPE_LIST:
    case DATA_TYPE_NOTHING:
    case DATA_TYPE_ANY:
        return true;
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_UNKNOWN:
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_NULL:
        return false;
    default:
        assert(false && "Unhandled data type in is_type_storable");
    }
}

void cast_binary(Compiler* compiler, CompilerValue* left, CompilerValue* right, DataType type) {
    if (left->type == DATA_TYPE_CHUNK || right->type == DATA_TYPE_CHUNK) {
        *left = cast_to_bc(compiler, *left, type);
        *right = cast_to_bc(compiler, *right, type);
    } else {
        *left = cast_to_const(compiler, *left, type);
        *right = cast_to_const(compiler, *right, type);
    }
}

int64_t int_pow(int64_t base, int64_t exp) {
    if (exp == 0) return 1;

    int64_t result = 1;
    while (exp) {
        if (exp & 1) result *= base;
        exp >>= 1;
        base *= base;
    }
    return result;
}

int64_t execute_int_binary(int64_t left, int64_t right, IrOpcode op) {
    switch (op) {
    case IR_ADDI:    return left +  right;
    case IR_SUBI:    return left -  right;
    case IR_MULI:    return left *  right;
    case IR_DIVI:    return left /  right;
    case IR_MODI:    return left %  right;
    case IR_POWI:    return int_pow(left, right);
    case IR_LESSI:   return left <  right;
    case IR_MOREI:   return left >  right;
    case IR_LESSEQI: return left <= right;
    case IR_MOREEQI: return left >= right;
    case IR_ANDI:    return left &  right;
    case IR_ORI:     return left |  right;
    case IR_XORI:    return left ^  right;
    case IR_AND:     return left && right;
    case IR_OR:      return left || right;
    case IR_XOR:     return (left != 0) != (right != 0);
    default: assert(false && "Invalid opcode for binary operation");
    }
}

double execute_float_binary(double left, double right, IrOpcode op) {
    switch (op) {
    case IR_ADDF:    return left +  right;
    case IR_SUBF:    return left -  right;
    case IR_MULF:    return left *  right;
    case IR_DIVF:    return left /  right;
    case IR_MODF:    return fmod(left, right);
    case IR_POWF:    return pow(left, right);
    case IR_LESSF:   return left <  right;
    case IR_MOREF:   return left >  right;
    case IR_LESSEQF: return left <= right;
    case IR_MOREEQF: return left >= right;
    default: assert(false && "Invalid opcode for binary operation");
    }
}

DataType get_op_return_type(IrOpcode op) {
    switch (op) {
    case IR_ADDI:
    case IR_SUBI:
    case IR_MULI:
    case IR_DIVI:
    case IR_MODI:
    case IR_POWI:
    case IR_ANDI:
    case IR_ORI:
    case IR_XORI:
        return DATA_TYPE_INTEGER;
    case IR_LESSI:
    case IR_MOREI:
    case IR_LESSEQI:
    case IR_MOREEQI:
    case IR_LESSF:
    case IR_MOREF:
    case IR_LESSEQF:
    case IR_MOREEQF:
    case IR_AND:
    case IR_OR:
    case IR_XOR:
        return DATA_TYPE_BOOL;
    case IR_ADDF:
    case IR_SUBF:
    case IR_MULF:
    case IR_DIVF:
    case IR_MODF:
    case IR_POWF:
        return DATA_TYPE_FLOAT;
    default:
        assert(false && "Unhandled opcode in get_op_return_type");
    }
}

CompilerValue evaluate_binary_number(Compiler* compiler, Argument* left, Argument* right, IrOpcode int_op, IrOpcode float_op) {
    CompilerValue left_val = compiler_evaluate_argument(compiler, left);
    if (left_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue right_val = compiler_evaluate_argument(compiler, right);
    if (right_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    DataType left_type  = left_val.type  == DATA_TYPE_CHUNK ? left_val.data.chunk_val.return_type  : left_val.type;
    DataType right_type = right_val.type == DATA_TYPE_CHUNK ? right_val.data.chunk_val.return_type : right_val.type;

    if (left_type == DATA_TYPE_FLOAT || right_type == DATA_TYPE_FLOAT) {
        cast_binary(compiler, &left_val, &right_val, DATA_TYPE_FLOAT);
    } else {
        cast_binary(compiler, &left_val, &right_val, DATA_TYPE_INTEGER);
    }
    if (left_val.type == DATA_TYPE_ERROR || right_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if (left_val.type == DATA_TYPE_CHUNK) {
        IrBytecode bc = left_val.data.chunk_val.bc;
        bytecode_join(&bc, &right_val.data.chunk_val.bc);

        IrOpcode select_op = left_val.data.chunk_val.return_type == DATA_TYPE_FLOAT ? float_op : int_op;
        bytecode_push_op(&bc, select_op);
        return DATA_CHUNK(get_op_return_type(select_op), bc);
    } else {
        CompilerValue val;
        if (left_val.type == DATA_TYPE_FLOAT) {
            val = DATA_FLOAT(execute_float_binary(left_val.data.float_val, right_val.data.float_val, float_op));
        } else {
            val = DATA_INTEGER(execute_int_binary(left_val.data.integer_val, right_val.data.integer_val, int_op));
        }

        IrOpcode select_op = left_val.type == DATA_TYPE_FLOAT ? float_op : int_op;
        return cast_to_const(compiler, val, get_op_return_type(select_op));
    }
}

CompilerValue evaluate_binary_int(Compiler* compiler, Argument* left, Argument* right, IrOpcode int_op) {
    CompilerValue left_val = compiler_evaluate_argument(compiler, left);
    if (left_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue right_val = compiler_evaluate_argument(compiler, right);
    if (right_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    cast_binary(compiler, &left_val, &right_val, DATA_TYPE_INTEGER);
    if (left_val.type == DATA_TYPE_ERROR) return DATA_ERROR;
    if (right_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if (left_val.type == DATA_TYPE_CHUNK) {
        IrBytecode bc = left_val.data.chunk_val.bc;
        bytecode_join(&bc, &right_val.data.chunk_val.bc);
        bytecode_push_op(&bc, int_op);
        return DATA_CHUNK(DATA_TYPE_INTEGER, bc);
    } else {
        return DATA_INTEGER(execute_int_binary(left_val.data.integer_val, right_val.data.integer_val, int_op));
    }
}

CompilerValue evaluate_binary_bool(Compiler* compiler, Argument* left, Argument* right, IrOpcode bool_op) {
    CompilerValue left_val = compiler_evaluate_argument(compiler, left);
    if (left_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue right_val = compiler_evaluate_argument(compiler, right);
    if (right_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    cast_binary(compiler, &left_val, &right_val, DATA_TYPE_BOOL);
    if (left_val.type == DATA_TYPE_ERROR) return DATA_ERROR;
    if (right_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if (left_val.type == DATA_TYPE_CHUNK) {
        IrBytecode bc = left_val.data.chunk_val.bc;
        bytecode_join(&bc, &right_val.data.chunk_val.bc);
        bytecode_push_op(&bc, bool_op);
        return DATA_CHUNK(DATA_TYPE_BOOL, bc);
    } else {
        return DATA_BOOL(execute_int_binary(left_val.data.bool_val, right_val.data.bool_val, bool_op));
    }
}

CompilerValue block_on_start(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;

    assert(block->parent.type == BLOCK_PARENT_BLOCKCHAIN);

    if (prev_block == (Block*)-1) {
        vector_add(&compiler->chains_to_compile, block->parent.as.chain);
        return DATA_NOTHING;
    } else if (prev_block == NULL) {
        IrBytecode bc = EMPTY_BYTECODE;

        IrConstValue label;
        label.type = IR_TYPE_LABEL;
        label.as.label_val.name = "entry";
        if (bytecode_pool_get(compiler->bc_pool, label) != (size_t)-1) {
            compiler_set_error(compiler, gettext("Duplicate on_start block"));
            return DATA_ERROR;
        }
        bytecode_push_label(&bc, "entry");
        return DATA_CHUNK(DATA_TYPE_NULL, bc);
    } else if (prev_block == block->parent.as.chain->end) {
        IrBytecode bc = EMPTY_BYTECODE;
        bytecode_push_op(&compiler->bytecode, IR_RET);
        return DATA_CHUNK(DATA_TYPE_NULL, bc);
    }

    assert(false && "Unreachable");
}

CompilerValue block_if(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    IrBytecode bc = EMPTY_BYTECODE;

    Block* prev = block->prev;
    if (!prev) prev = block->parent.as.chain->parent;

    if (prev_block == prev) {
        size_t var_slot = compiler->variables.size;

        CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
        value = cast_to_bc_bool(compiler, value);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

        IrBytecode bc_label = EMPTY_BYTECODE;
        ConstId label = bytecode_push_label(&bc_label, ir_arena_sprintf(compiler->arena, 32, "if_false_%zu", compiler->label_counter++));

        bc = value.data.chunk_val.bc;
        bytecode_push_op_label(&bc, IR_IFNOT, label);

        if (!CHAIN_EMPTY(block->contents)) {
            ControlData* block_data = ir_arena_alloc(compiler->arena, sizeof(ControlData));
            block_data->bc = bc_label;
            block_data->var_slot = var_slot;
            compiler_object_info_insert(compiler, block, block_data);

            *next_block = block->contents->start;
            return DATA_CHUNK(DATA_TYPE_NULL, bc);
        } else if (!CHAIN_EMPTY(block->controlend_contents)) {
            ControlData* block_data = ir_arena_alloc(compiler->arena, sizeof(ControlData));
            block_data->bc = EMPTY_BYTECODE;

            ConstId end_label = bytecode_push_label(&block_data->bc, ir_arena_sprintf(compiler->arena, 32, "if_end_%zu", compiler->label_counter++));

            ControlData* controlend_data = ir_arena_alloc(compiler->arena, sizeof(ControlData));
            controlend_data->label = end_label;

            bytecode_push_op_label(&bc, IR_JMP, end_label);

            compiler_object_info_insert(compiler, block, block_data);
            compiler_object_info_insert(compiler, block->controlend_contents->start, controlend_data);
            *next_block = block->controlend_contents->start;
        }

        compiler->variables.size = var_slot;
        bytecode_join(&bc, &bc_label);
    } else if (prev_block == block->contents->end) {
        ControlData* block_data = compiler_object_info_get(compiler, block);
        if (block_data == OBJECT_NOT_FOUND) {
            compiler_set_error(compiler, "Could not find block data in if block");
            return DATA_ERROR;
        }

        IrBytecode bc_end_label = EMPTY_BYTECODE;
        ConstId end_label = bytecode_push_label(&bc_end_label, ir_arena_sprintf(compiler->arena, 32, "if_end_%zu", compiler->label_counter++));

        bytecode_push_op_label(&bc, IR_JMP, end_label);
        bytecode_join(&bc, &block_data->bc);

        compiler->variables.size = block_data->var_slot;

        if (!CHAIN_EMPTY(block->controlend_contents)) {
            block_data->bc = bc_end_label;

            ControlData* controlend_data = ir_arena_alloc(compiler->arena, sizeof(ControlData));
            controlend_data->label = end_label;

            compiler_object_info_insert(compiler, block->controlend_contents->start, controlend_data);
            *next_block = block->controlend_contents->start;
        } else {
            bytecode_join(&bc, &bc_end_label);
        }
    } else if (prev_block == block->controlend_contents->end) {
        ControlData* block_data = compiler_object_info_get(compiler, block);
        if (block_data == OBJECT_NOT_FOUND) {
            compiler_set_error(compiler, "Could not find block data in if block");
            return DATA_ERROR;
        }
        bc = block_data->bc;
    }

    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_else_if(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    Block* prev = block->prev;
    if (!prev) prev = block->parent.as.chain->parent;

    IrBytecode bc = EMPTY_BYTECODE;

    ControlData* block_data = compiler_object_info_get(compiler, block);
    if (block_data == OBJECT_NOT_FOUND) {
        compiler_set_error(compiler, "Could not find block data in else if block");
        return DATA_ERROR;
    }

    if (prev_block == prev) {
        size_t var_slot = compiler->variables.size;

        CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
        value = cast_to_bc_bool(compiler, value);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

        IrBytecode end_bc = EMPTY_BYTECODE;
        ConstId end_label = bytecode_push_label(&end_bc, ir_arena_sprintf(compiler->arena, 64, "else_if_end_%zu", compiler->label_counter++));

        bc = value.data.chunk_val.bc;
        bytecode_push_op_label(&bc, IR_IFNOT, end_label);

        if (!CHAIN_EMPTY(block->contents)) {
            block_data->bc = end_bc;
            block_data->var_slot = var_slot;
            *next_block = block->contents->start;
        } else {
            bytecode_push_op_label(&bc, IR_JMP, block_data->label);
            bytecode_join(&bc, &end_bc);
            compiler->variables.size = var_slot;
            if (block->next) compiler_object_info_insert(compiler, block->next, block_data);
        }
    } else if (prev_block == block->contents->end) {
        bytecode_push_op_label(&bc, IR_JMP, block_data->label);
        bytecode_join(&bc, &block_data->bc);
        compiler->variables.size = block_data->var_slot;
        if (block->next) compiler_object_info_insert(compiler, block->next, block_data);
    }
    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_else(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    Block* prev = block->prev;
    if (!prev) prev = block->parent.as.chain->parent;

    IrBytecode bc = EMPTY_BYTECODE;

    ControlData* block_data = compiler_object_info_get(compiler, block);
    if (block_data == OBJECT_NOT_FOUND) {
        compiler_set_error(compiler, "Could not find block data in else block");
        return DATA_ERROR;
    }

    if (prev_block == prev) {
        if (!CHAIN_EMPTY(block->contents)) {
            *next_block = block->contents->start;
            block_data->var_slot = compiler->variables.size;
        }
    } else {
        bytecode_push_op_label(&bc, IR_JMP, block_data->label);
        compiler->variables.size = block_data->var_slot;
        if (block->next) compiler_object_info_insert(compiler, block->next, block_data);
    }
    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_loop(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    Block* prev = block->prev;
    if (!prev) prev = block->parent.as.chain->parent;

    IrBytecode bc = EMPTY_BYTECODE;

    if (prev_block == prev) {
        ConstId loop_label = bytecode_push_label(&bc, ir_arena_sprintf(compiler->arena, 32, "loop_%zu", compiler->label_counter++));

        if (!CHAIN_EMPTY(block->contents)) {
            ControlData* block_data = ir_arena_alloc(compiler->arena, sizeof(ControlData));
            block_data->label = loop_label;
            block_data->var_slot = compiler->variables.size;
            compiler_object_info_insert(compiler, block, block_data);
            *next_block = block->contents->start;
        } else {
            bytecode_push_op_label(&bc, IR_JMP, loop_label);
        }
    } else if (prev_block == block->contents->end) {
        ControlData* block_data = compiler_object_info_get(compiler, block);
        if (block_data == OBJECT_NOT_FOUND) {
            compiler_set_error(compiler, "Could not find loop label in loop block");
            return DATA_ERROR;
        }

        bytecode_push_op_label(&bc, IR_JMP, block_data->label);
        compiler->variables.size = block_data->var_slot;
    }

    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_repeat(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    Block* prev = block->prev;
    if (!prev) prev = block->parent.as.chain->parent;

    IrBytecode bc = EMPTY_BYTECODE;

    if (prev_block == prev) {
        // Compute constant, it will be stored on the stack
        CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
        value = cast_to_bc_int(compiler, value);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

        bc = value.data.chunk_val.bc;

        // Create new variable & reserve the slot for it
        size_t var_slot = compiler->variables.size;
        bytecode_push_op_int(&bc, IR_PUSHI, 0);
        bytecode_push_op_int(&bc, IR_STORE, var_slot);

        Variable var = {
            .name = "__scrap_repeat_index",
            .type = DATA_TYPE_UNKNOWN, // Compiler cannot cast anything to unknown, so any other block cannot mess with this variable
        };
        ir_arena_append(compiler->arena, compiler->variables, var);

        // Loop start
        ConstId loop_label = bytecode_push_label(&bc, ir_arena_sprintf(compiler->arena, 32, "repeat_%zu", compiler->label_counter++));

        // Check the condition
        bytecode_push_op(&bc, IR_DUP);
        bytecode_push_op_int(&bc, IR_LOAD, var_slot);
        bytecode_push_op(&bc, IR_MOREI);

        IrBytecode end_label_bc = EMPTY_BYTECODE;
        ConstId loop_end = bytecode_push_label(&end_label_bc, ir_arena_sprintf(compiler->arena, 32, "repeat_end_%zu", compiler->label_counter++));

        bytecode_push_op_label(&bc, IR_IFNOT, loop_end);

        if (!CHAIN_EMPTY(block->contents)) {
            ControlData* block_data = ir_arena_alloc(compiler->arena, sizeof(ControlData));
            block_data->label = loop_label;
            block_data->bc = end_label_bc;
            block_data->var_slot = var_slot;

            compiler_object_info_insert(compiler, block, block_data);
            *next_block = block->contents->start;
        } else {
            // Increment the counter
            bytecode_push_op_int(&bc, IR_LOAD, var_slot);
            bytecode_push_op_int(&bc, IR_PUSHI, 1);
            bytecode_push_op(&bc, IR_ADDI);
            bytecode_push_op_int(&bc, IR_STORE, var_slot);

            compiler->variables.size--;

            bytecode_push_op_label(&bc, IR_JMP, loop_label);
            bytecode_join(&bc, &end_label_bc);
        }
    } else if (prev_block == block->contents->end) {
        ControlData* block_data = compiler_object_info_get(compiler, block);
        if (block_data == OBJECT_NOT_FOUND) {
            compiler_set_error(compiler, "Could not find block data in while block");
            return DATA_ERROR;
        }

        // Increment the counter
        bytecode_push_op_int(&bc, IR_LOAD, block_data->var_slot);
        bytecode_push_op_int(&bc, IR_PUSHI, 1);
        bytecode_push_op(&bc, IR_ADDI);
        bytecode_push_op_int(&bc, IR_STORE, block_data->var_slot);

        compiler->variables.size = block_data->var_slot;

        bytecode_push_op_label(&bc, IR_JMP, block_data->label);
        bytecode_join(&bc, &block_data->bc);
    }

    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_while(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    Block* prev = block->prev;
    if (!prev) prev = block->parent.as.chain->parent;

    IrBytecode bc = EMPTY_BYTECODE;

    if (prev_block == prev) {
        ConstId loop_label = bytecode_push_label(&bc, ir_arena_sprintf(compiler->arena, 32, "while_%zu", compiler->label_counter++));

        CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
        value = cast_to_bc_bool(compiler, value);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

        bytecode_join(&bc, &value.data.chunk_val.bc);

        IrBytecode end_label_bc = EMPTY_BYTECODE;
        ConstId loop_end = bytecode_push_label(&end_label_bc, ir_arena_sprintf(compiler->arena, 32, "while_end_%zu", compiler->label_counter++));

        bytecode_push_op_label(&bc, IR_IFNOT, loop_end);

        if (!CHAIN_EMPTY(block->contents)) {
            ControlData* block_data = ir_arena_alloc(compiler->arena, sizeof(ControlData));
            block_data->label = loop_label;
            block_data->bc = end_label_bc;
            block_data->var_slot = compiler->variables.size;

            compiler_object_info_insert(compiler, block, block_data);
            *next_block = block->contents->start;
        } else {
            bytecode_push_op_label(&bc, IR_JMP, loop_label);
            bytecode_join(&bc, &end_label_bc);
        }
    } else if (prev_block == block->contents->end) {
        ControlData* block_data = compiler_object_info_get(compiler, block);
        if (block_data == OBJECT_NOT_FOUND) {
            compiler_set_error(compiler, "Could not find block data in while block");
            return DATA_ERROR;
        }

        bytecode_push_op_label(&bc, IR_JMP, block_data->label);
        bytecode_join(&bc, &block_data->bc);
        compiler->variables.size = block_data->var_slot;
    }

    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_block(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    Block* prev = block->prev;
    if (!prev) prev = block->parent.as.chain->parent;

    if (prev_block == prev) {
        if (!CHAIN_EMPTY(block->contents)) {
            compiler_object_info_insert(compiler, block, (void*)compiler->variables.size);
            *next_block = block->contents->start;
        }
    } else if (prev_block == block->contents->end) {
        void* block_data = compiler_object_info_get(compiler, block);
        if (block_data == OBJECT_NOT_FOUND) {
            compiler_set_error(compiler, "Could not find block data in block block");
            return DATA_ERROR;
        }
        compiler->variables.size = (size_t)block_data;
    }

    return EMPTY_CHUNK;
}

#define print_func_name(...) func_name_size += snprintf(func_name + func_name_size, 1024 - func_name_size, __VA_ARGS__)
CompilerValue block_define_block(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;

    assert(block->parent.type == BLOCK_PARENT_BLOCKCHAIN);
    if (prev_block == (Block*)-1) {
        assert(block->arguments[0].type == ARGUMENT_BLOCKDEF);

        Blockdef* blockdef = block->arguments[0].data.blockdef;

        char* func_name = ir_arena_alloc(compiler->arena, 1024);
        size_t func_name_size = 0;

        for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
            static_assert(INPUT_LAST == 6, "Exhaustive input type in block_define_block");
            switch (blockdef->inputs[i].type) {
            case INPUT_TEXT_DISPLAY:
                print_func_name("%s ", blockdef->inputs[i].data.text);
                break;
            case INPUT_BLOCKDEF_EDITOR:
            case INPUT_COLOR:
            case INPUT_DROPDOWN:
                print_func_name("[] ");
                break;
            case INPUT_IMAGE_DISPLAY:
                break;
            case INPUT_ARGUMENT:
                print_func_name("[] ");
                break;
            default:
                assert(false && "Unhandled input type in block_define_block");
            }

            if (func_name_size >= 1024) break;
        }
        if (func_name_size > 1) func_name[func_name_size - 1] = 0;

        IrConstValue label_val;
        label_val.type = IR_TYPE_LABEL;
        label_val.as.label_val.name = func_name;
        if (bytecode_pool_get(compiler->bc_pool, label_val) != (size_t)-1) {
            compiler_set_error(compiler, gettext("Duplicate function name \"%s\""), func_name);
            return DATA_ERROR;
        }

        IrBytecode bc = EMPTY_BYTECODE;
        ConstId label = bytecode_push_label(&bc, func_name);

        CustomFunctionData* block_data = ir_arena_alloc(compiler->arena, sizeof(CustomFunctionData));
        block_data->label = label;
        block_data->bc = bc;

        size_t arg_id = 0;
        for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
            if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
            compiler_object_info_insert(compiler, blockdef->inputs[i].data.arg.blockdef, (void*)arg_id);
            arg_id++;
        }
        block_data->arg_count = arg_id;

        compiler_object_info_insert(compiler, blockdef, block_data);

        vector_add(&compiler->chains_to_compile, block->parent.as.chain);
        return DATA_NOTHING;
    } else if (prev_block == NULL) {
        Blockdef* blockdef = block->arguments[0].data.blockdef;
        CustomFunctionData* block_data = compiler_object_info_get(compiler, blockdef);
        if (block_data == OBJECT_NOT_FOUND) {
            compiler_set_error(compiler, "Could not find block data in define block");
            return DATA_ERROR;
        }

        IrBytecode bc = block_data->bc;
        for (ssize_t i = block_data->arg_count - 1; i >= 0; i--) {
            bytecode_push_op_int(&bc, IR_STORE, i);

            Variable var = {
                .name = "__define_arg",
                .type = DATA_TYPE_ANY,
            };
            ir_arena_append(compiler->arena, compiler->variables, var);
        }

        return DATA_CHUNK(DATA_TYPE_NULL, bc);
    } else if (prev_block == block->parent.as.chain->end) {
        IrBytecode bc = EMPTY_BYTECODE;
        if (strcmp(block->parent.as.chain->end->blockdef->id, "return")) {
            bytecode_push_op(&bc, IR_PUSHN);
            bytecode_push_op(&bc, IR_RET);
        }
        return DATA_CHUNK(DATA_TYPE_NULL, bc);
    }

    assert(false && "Unreachable");
}

CompilerValue block_return(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    Block* define_block = compiler->current_chain->start;
    if (strcmp(define_block->blockdef->id, "define_block")) {
        compiler_set_error(compiler, gettext("Return block used outside of function"));
        return DATA_ERROR;
    }

    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
    if (value.type != DATA_TYPE_CHUNK) {
        value = cast_to_bc(compiler, value, value.type);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
    }

    assert(define_block->arguments[0].type == ARGUMENT_BLOCKDEF);

    IrBytecode bc = value.data.chunk_val.bc;
    bytecode_push_op(&bc, IR_RET);

    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_print(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue val = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    val = cast_to_bc_string(compiler, val);
    if (val.type == DATA_TYPE_ERROR) return DATA_ERROR;
    assert(val.type == DATA_TYPE_CHUNK);

    bytecode_push_op_func(&val.data.chunk_val.bc, IR_RUN, ir_func_by_hint("std_term_print_str"));
    val.data.chunk_val.return_type = DATA_TYPE_NULL;

    return val;
}

CompilerValue block_println(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue val = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    val = cast_to_bc_string(compiler, val);
    if (val.type == DATA_TYPE_ERROR) return DATA_ERROR;
    assert(val.type == DATA_TYPE_CHUNK);

    bytecode_push_op_func(&val.data.chunk_val.bc, IR_RUN, ir_func_by_hint("std_term_println_str"));
    val.data.chunk_val.return_type = DATA_TYPE_NULL;

    return val;
}

CompilerValue block_input(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) block;
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_get_input"));
    return DATA_CHUNK(DATA_TYPE_STRING, bc);
}

CompilerValue block_get_char(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) block;
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_get_char"));
    return DATA_CHUNK(DATA_TYPE_INTEGER, bc);
}

CompilerValue block_set_cursor(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue x = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (x.type == DATA_TYPE_ERROR) return DATA_ERROR;
    x = cast_to_bc_int(compiler, x);
    if (x.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue y = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (y.type == DATA_TYPE_ERROR) return DATA_ERROR;
    y = cast_to_bc_int(compiler, y);
    if (y.type == DATA_TYPE_ERROR) return DATA_ERROR;

    IrBytecode bc = x.data.chunk_val.bc;
    bytecode_join(&bc, &y.data.chunk_val.bc);
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_set_cursor"));
    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_cursor_x(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) block;
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_cursor_x"));
    return DATA_CHUNK(DATA_TYPE_INTEGER, bc);
}

CompilerValue block_cursor_y(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) block;
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_cursor_y"));
    return DATA_CHUNK(DATA_TYPE_INTEGER, bc);
}

CompilerValue block_cursor_max_x(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) block;
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_cursor_max_x"));
    return DATA_CHUNK(DATA_TYPE_INTEGER, bc);
}

CompilerValue block_cursor_max_y(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) block;
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_cursor_max_y"));
    return DATA_CHUNK(DATA_TYPE_INTEGER, bc);
}

CompilerValue block_set_fg_color(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue color = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (color.type == DATA_TYPE_ERROR) return DATA_ERROR;
    color = cast_to_bc_color(compiler, color);
    if (color.type == DATA_TYPE_ERROR) return DATA_ERROR;

    IrBytecode bc = color.data.chunk_val.bc;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_set_fg_color"));
    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_set_bg_color(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue color = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (color.type == DATA_TYPE_ERROR) return DATA_ERROR;
    color = cast_to_bc_color(compiler, color);
    if (color.type == DATA_TYPE_ERROR) return DATA_ERROR;

    IrBytecode bc = color.data.chunk_val.bc;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_set_bg_color"));
    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_reset_color(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) block;
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_int(&bc, IR_PUSHI, 0xffffffff);
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_set_fg_color"));
    bytecode_push_op_int(&bc, IR_PUSHI, *(int*)&(BLACK));
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_set_bg_color"));

    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_term_clear(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) block;
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_clear"));
    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_term_set_clear(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue color = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (color.type == DATA_TYPE_ERROR) return DATA_ERROR;
    color = cast_to_bc_color(compiler, color);
    if (color.type == DATA_TYPE_ERROR) return DATA_ERROR;

    IrBytecode bc = color.data.chunk_val.bc;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_term_set_clear_color"));
    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_plus(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_number(compiler, &block->arguments[0], &block->arguments[1], IR_ADDI, IR_ADDF);
}

CompilerValue block_minus(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_number(compiler, &block->arguments[0], &block->arguments[1], IR_SUBI, IR_SUBF);
}

CompilerValue block_mult(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_number(compiler, &block->arguments[0], &block->arguments[1], IR_MULI, IR_MULF);
}

CompilerValue block_div(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_number(compiler, &block->arguments[0], &block->arguments[1], IR_DIVI, IR_DIVF);
}

CompilerValue block_rem(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_number(compiler, &block->arguments[0], &block->arguments[1], IR_MODI, IR_MODF);
}

CompilerValue block_pow(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_number(compiler, &block->arguments[0], &block->arguments[1], IR_POWI, IR_POWF);
}

CompilerValue block_math(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue op = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (op.type == DATA_TYPE_ERROR) return DATA_ERROR;
    op = cast_to_const_string(compiler, op);
    if (op.type == DATA_TYPE_ERROR) return DATA_ERROR;

    for (size_t i = 0; i < MATH_LIST_LEN; i++) {
        if (!strcmp(block_math_list[i], op.data.str_val)) {
            CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[1]);
            if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
            value = cast_to(compiler, value, DATA_TYPE_FLOAT);
            if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

            if (value.type == DATA_TYPE_CHUNK) {
                bytecode_push_op_func(&value.data.chunk_val.bc, IR_RUN, ir_func_by_hint(op.data.str_val));
                return value;
            } else {
                return DATA_FLOAT(block_math_func_list[i](value.data.float_val));
            }
        }
    }

    compiler_set_error(compiler, gettext("Invalid operation \"%s\" in math block"), op.data.str_val);
    return DATA_ERROR;
}

CompilerValue block_pi(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) compiler;
    (void) block;
    (void) next_block;
    (void) prev_block;
    return DATA_FLOAT(M_PI);
}

CompilerValue block_less(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_number(compiler, &block->arguments[0], &block->arguments[1], IR_LESSI, IR_LESSF);
}

CompilerValue block_less_eq(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_number(compiler, &block->arguments[0], &block->arguments[1], IR_LESSEQI, IR_LESSEQF);
}

CompilerValue block_eq(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue left = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (left.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue right = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (right.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if ((left.type != DATA_TYPE_CHUNK) != (right.type != DATA_TYPE_CHUNK)) {
        if (left.type != DATA_TYPE_CHUNK) {
            left = cast_to_bc(compiler, left, right.data.chunk_val.return_type);
            if (left.type == DATA_TYPE_ERROR) return DATA_ERROR;
        } else if (right.type != DATA_TYPE_CHUNK) {
            right = cast_to_bc(compiler, right, left.data.chunk_val.return_type);
            if (right.type == DATA_TYPE_ERROR) return DATA_ERROR;
        }
    } else if (left.type != DATA_TYPE_CHUNK && right.type != DATA_TYPE_CHUNK) {
        if (left.type != right.type) {
            compiler_set_error(compiler, gettext("Incompatible types %s and %s in eq block"), type_to_str(left.type), type_to_str(right.type));
            return DATA_ERROR;
        }

        static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in block_eq");
        switch (left.type) {
        case DATA_TYPE_NOTHING: return DATA_BOOL(true);
        case DATA_TYPE_INTEGER: return DATA_BOOL(left.data.integer_val == right.data.integer_val);
        case DATA_TYPE_FLOAT: return DATA_BOOL(left.data.float_val == right.data.float_val);
        case DATA_TYPE_STRING: return DATA_BOOL(!strcmp(left.data.str_val, right.data.str_val));
        case DATA_TYPE_BOOL: return DATA_BOOL(left.data.bool_val == right.data.bool_val);
        case DATA_TYPE_LIST: return DATA_BOOL(left.data.list_val == right.data.list_val);
        case DATA_TYPE_COLOR: return DATA_BOOL(!memcmp(&left.data.color_val, &right.data.color_val, sizeof(left.data.color_val)));
        case DATA_TYPE_ANY:
        case DATA_TYPE_BLOCKDEF:
        case DATA_TYPE_UNKNOWN:
        case DATA_TYPE_CHUNK:
        case DATA_TYPE_NULL:
            compiler_set_error(compiler, gettext("Type %s cannot be compared"), type_to_str(left.type));
            return DATA_ERROR;
        default:
            assert(false && "Unhandled data type in block_eq");

        }
    }

    DataType left_type  = left.data.chunk_val.return_type,
             right_type = right.data.chunk_val.return_type;

    if (left_type != right_type && left_type != DATA_TYPE_ANY && right_type != DATA_TYPE_ANY) {
        compiler_set_error(compiler, gettext("Incompatible types %s and %s in eq block"), type_to_str(left_type), type_to_str(right_type));
        return DATA_ERROR;
    }

    IrBytecode bc = left.data.chunk_val.bc;
    bytecode_join(&bc, &right.data.chunk_val.bc);
    bytecode_push_op(&bc, IR_EQ);

    return DATA_CHUNK(DATA_TYPE_BOOL, bc);
}

CompilerValue block_not_eq(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue left = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (left.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue right = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (right.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if ((left.type != DATA_TYPE_CHUNK) != (right.type != DATA_TYPE_CHUNK)) {
        if (left.type != DATA_TYPE_CHUNK) {
            left = cast_to_bc(compiler, left, right.data.chunk_val.return_type);
            if (left.type == DATA_TYPE_ERROR) return DATA_ERROR;
        } else if (right.type != DATA_TYPE_CHUNK) {
            right = cast_to_bc(compiler, right, left.data.chunk_val.return_type);
            if (right.type == DATA_TYPE_ERROR) return DATA_ERROR;
        }
    } else if (left.type != DATA_TYPE_CHUNK && right.type != DATA_TYPE_CHUNK) {
        if (left.type != right.type) {
            compiler_set_error(compiler, gettext("Incompatible types %s and %s in not_eq block"), type_to_str(left.type), type_to_str(right.type));
            return DATA_ERROR;
        }

        static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in block_eq");
        switch (left.type) {
        case DATA_TYPE_NOTHING: return DATA_BOOL(false);
        case DATA_TYPE_INTEGER: return DATA_BOOL(left.data.integer_val != right.data.integer_val);
        case DATA_TYPE_FLOAT: return DATA_BOOL(left.data.float_val != right.data.float_val);
        case DATA_TYPE_STRING: return DATA_BOOL(!!strcmp(left.data.str_val, right.data.str_val));
        case DATA_TYPE_BOOL: return DATA_BOOL(left.data.bool_val != right.data.bool_val);
        case DATA_TYPE_LIST: return DATA_BOOL(left.data.list_val != right.data.list_val);
        case DATA_TYPE_COLOR: return DATA_BOOL(!!memcmp(&left.data.color_val, &right.data.color_val, sizeof(left.data.color_val)));
        case DATA_TYPE_ANY:
        case DATA_TYPE_BLOCKDEF:
        case DATA_TYPE_UNKNOWN:
        case DATA_TYPE_CHUNK:
        case DATA_TYPE_NULL:
            compiler_set_error(compiler, gettext("Type %s cannot be compared"), type_to_str(left.type));
            return DATA_ERROR;
        default:
            assert(false && "Unhandled data type in block_eq");

        }
    }

    DataType left_type  = left.data.chunk_val.return_type,
             right_type = right.data.chunk_val.return_type;

    if (left_type != right_type && left_type != DATA_TYPE_ANY && right_type != DATA_TYPE_ANY) {
        compiler_set_error(compiler, gettext("Incompatible types %s and %s in not_eq block"), type_to_str(left_type), type_to_str(right_type));
        return DATA_ERROR;
    }

    IrBytecode bc = left.data.chunk_val.bc;
    bytecode_join(&bc, &right.data.chunk_val.bc);
    bytecode_push_op(&bc, IR_NEQ);

    return DATA_CHUNK(DATA_TYPE_BOOL, bc);
}

CompilerValue block_more_eq(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_number(compiler, &block->arguments[0], &block->arguments[1], IR_MOREEQI, IR_MOREEQF);
}

CompilerValue block_more(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_number(compiler, &block->arguments[0], &block->arguments[1], IR_MOREI, IR_MOREF);
}

CompilerValue block_not(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue val = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    val = cast_to(compiler, val, DATA_TYPE_BOOL);
    if (val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if (val.type == DATA_TYPE_CHUNK) {
        bytecode_push_op(&val.data.chunk_val.bc, IR_NOT);
        return DATA_CHUNK(DATA_TYPE_BOOL, val.data.chunk_val.bc);
    } else {
        return DATA_BOOL(!val.data.bool_val);
    }
}

CompilerValue block_and(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_bool(compiler, &block->arguments[0], &block->arguments[1], IR_AND);
}

CompilerValue block_or(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_bool(compiler, &block->arguments[0], &block->arguments[1], IR_OR);
}

CompilerValue block_true(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) compiler;
    (void) block;
    (void) next_block;
    (void) prev_block;
    return DATA_BOOL(true);
}

CompilerValue block_false(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) compiler;
    (void) block;
    (void) next_block;
    (void) prev_block;
    return DATA_BOOL(false);
}

CompilerValue block_bit_not(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue val = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    val = cast_to(compiler, val, DATA_TYPE_INTEGER);
    if (val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if (val.type == DATA_TYPE_CHUNK) {
        bytecode_push_op(&val.data.chunk_val.bc, IR_NOTI);
        return DATA_CHUNK(DATA_TYPE_INTEGER, val.data.chunk_val.bc);
    } else {
        return DATA_INTEGER(~val.data.integer_val);
    }
}

CompilerValue block_bit_and(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_int(compiler, &block->arguments[0], &block->arguments[1], IR_ANDI);
}

CompilerValue block_bit_or(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_int(compiler, &block->arguments[0], &block->arguments[1], IR_ORI);
}

CompilerValue block_bit_xor(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    return evaluate_binary_int(compiler, &block->arguments[0], &block->arguments[1], IR_XORI);
}

CompilerValue block_declare_var(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue name = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (name.type == DATA_TYPE_ERROR) return DATA_ERROR;
    name = cast_to_const_string(compiler, name);
    if (name.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if (value.type != DATA_TYPE_CHUNK) {
        value = cast_to_bc(compiler, value, value.type);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
    }

    if (!is_type_storable(value.data.chunk_val.return_type)) {
        compiler_set_error(compiler, gettext("Cannot declare variable with type %s"), type_to_str(value.data.chunk_val.return_type));
        return DATA_ERROR;
    }

    IrBytecode bc = value.data.chunk_val.bc;

    Variable var = {
        .name = name.data.str_val,
        .type = value.data.chunk_val.return_type,
    };

    Block* first_block = block;
    while (first_block->prev) first_block = first_block->prev;
    if (!strcmp(first_block->blockdef->id, "on_start")) {
        bytecode_push_op_int(&bc, IR_GSTORE, compiler->global_variables.size);
        ir_arena_append(compiler->arena, compiler->global_variables, var);
    } else {
        bytecode_push_op_int(&bc, IR_STORE, compiler->variables.size);
        ir_arena_append(compiler->arena, compiler->variables, var);
    }

    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_get_var(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue name = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (name.type == DATA_TYPE_ERROR) return DATA_ERROR;
    name = cast_to_const_string(compiler, name);
    if (name.type == DATA_TYPE_ERROR) return DATA_ERROR;

    bool global;
    ssize_t var_slot = compiler_find_variable(compiler, name.data.str_val, &global);
    if (var_slot == -1) {
        compiler_set_error(compiler, gettext("Variable with name \"%s\" does not exist in the current scope"), name.data.str_val);
        return DATA_ERROR;
    }

    IrBytecode bc = EMPTY_BYTECODE;
    if (global) {
        bytecode_push_op_int(&bc, IR_GLOAD, var_slot);
        return DATA_CHUNK(compiler->global_variables.items[var_slot].type, bc);
    } else {
        bytecode_push_op_int(&bc, IR_LOAD, var_slot);
        return DATA_CHUNK(compiler->variables.items[var_slot].type, bc);
    }
}

CompilerValue block_set_var(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue name = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (name.type == DATA_TYPE_ERROR) return DATA_ERROR;
    name = cast_to_const_string(compiler, name);
    if (name.type == DATA_TYPE_ERROR) return DATA_ERROR;

    bool global;
    ssize_t var_slot = compiler_find_variable(compiler, name.data.str_val, &global);
    if (var_slot == -1) {
        compiler_set_error(compiler, gettext("Variable with name \"%s\" does not exist in the current scope"), name.data.str_val);
        return DATA_ERROR;
    }

    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if (value.type != DATA_TYPE_CHUNK) {
        value = cast_to_bc(compiler, value, value.type);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
    }

    Variable var = global ? compiler->global_variables.items[var_slot] : compiler->variables.items[var_slot];

    if (var.type != DATA_TYPE_ANY && value.data.chunk_val.return_type != var.type) {
        compiler_set_error(
            compiler,
            gettext("Assign to variable \"%s\" of type %s with incompatible type %s"),
            name.data.str_val,
            type_to_str(var.type),
            type_to_str(value.data.chunk_val.return_type)
        );
        return DATA_ERROR;
    }

    IrBytecode bc = value.data.chunk_val.bc;
    bytecode_push_op_int(&bc, global ? IR_GSTORE : IR_STORE, var_slot);

    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_join(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue left_val = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (left_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue right_val = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (right_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    cast_binary(compiler, &left_val, &right_val, DATA_TYPE_STRING);
    if (left_val.type == DATA_TYPE_ERROR || right_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if (left_val.type == DATA_TYPE_CHUNK) {
        IrBytecode bc = left_val.data.chunk_val.bc;
        bytecode_join(&bc, &right_val.data.chunk_val.bc);
        bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_string_join"));
        return DATA_CHUNK(DATA_TYPE_STRING, bc);
    } else {
        size_t left_len = strlen(left_val.data.str_val);
        size_t right_len = strlen(right_val.data.str_val);

        char* str = ir_arena_alloc(compiler->arena, left_len + right_len + 1);
        strcpy(str, left_val.data.str_val);
        strcpy(str + left_len, right_val.data.str_val);
        return DATA_STRING(str);
    }
}

CompilerValue block_letter_in(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue ind = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (ind.type == DATA_TYPE_ERROR) return DATA_ERROR;
    ind = cast_to_bc_int(compiler, ind);
    if (ind.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue str = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (str.type == DATA_TYPE_ERROR) return DATA_ERROR;
    str = cast_to_bc_string(compiler, str);
    if (str.type == DATA_TYPE_ERROR) return DATA_ERROR;

    IrBytecode bc = EMPTY_BYTECODE;

    bytecode_push_op_list(&bc, IR_PUSHA, NULL);
    bytecode_push_op(&bc, IR_DUP);

    bytecode_join(&bc, &str.data.chunk_val.bc);
    bytecode_join(&bc, &ind.data.chunk_val.bc);

    bytecode_push_op(&bc, IR_INDEXL);
    bytecode_push_op(&bc, IR_ADDL);
    return DATA_CHUNK(DATA_TYPE_STRING, bc);
}

CompilerValue block_substring(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue start = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (start.type == DATA_TYPE_ERROR) return DATA_ERROR;
    start = cast_to_bc_int(compiler, start);
    if (start.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue end = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (end.type == DATA_TYPE_ERROR) return DATA_ERROR;
    end = cast_to_bc_int(compiler, end);
    if (end.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue str = compiler_evaluate_argument(compiler, &block->arguments[2]);
    if (str.type == DATA_TYPE_ERROR) return DATA_ERROR;
    str = cast_to_bc_string(compiler, str);
    if (str.type == DATA_TYPE_ERROR) return DATA_ERROR;

    IrBytecode bc = start.data.chunk_val.bc;
    bytecode_join(&bc, &end.data.chunk_val.bc);
    bytecode_join(&bc, &str.data.chunk_val.bc);
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_string_substring"));
    return DATA_CHUNK(DATA_TYPE_STRING, bc);
}

CompilerValue block_length(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue str = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (str.type == DATA_TYPE_ERROR) return DATA_ERROR;
    str = cast_to_bc_string(compiler, str);
    if (str.type == DATA_TYPE_ERROR) return DATA_ERROR;

    bytecode_push_op(&str.data.chunk_val.bc, IR_LENL);
    return DATA_CHUNK(DATA_TYPE_INTEGER, str.data.chunk_val.bc);
}

CompilerValue block_ord(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue str = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (str.type == DATA_TYPE_ERROR) return DATA_ERROR;
    str = cast_to(compiler, str, DATA_TYPE_STRING);
    if (str.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if (str.type == DATA_TYPE_CHUNK) {
        IrBytecode bc = str.data.chunk_val.bc;
        bytecode_push_op_int(&bc, IR_PUSHI, 1);
        bytecode_push_op(&bc, IR_INDEXL);
        return DATA_CHUNK(DATA_TYPE_INTEGER, bc);
    } else {
        int bin;
        return DATA_INTEGER(GetCodepointNext(str.data.str_val, &bin));
    }
}

CompilerValue block_chr(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue int_val = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (int_val.type == DATA_TYPE_ERROR) return DATA_ERROR;
    int_val = cast_to(compiler, int_val, DATA_TYPE_INTEGER);
    if (int_val.type == DATA_TYPE_ERROR) return DATA_ERROR;

    if (int_val.type == DATA_TYPE_CHUNK) {
        IrBytecode bc = EMPTY_BYTECODE;
        bytecode_push_op_list(&bc, IR_PUSHA, NULL);
        bytecode_push_op(&bc, IR_DUP);
        bytecode_join(&bc, &int_val.data.chunk_val.bc);
        bytecode_push_op(&bc, IR_ADDL);
        return DATA_CHUNK(DATA_TYPE_STRING, bc);
    } else {
        return DATA_STRING(ir_arena_sprintf(compiler->arena, 16, "%lc", int_val.data.integer_val));
    }
}

CompilerValue block_create_list(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) compiler;
    (void) block;
    (void) next_block;
    (void) prev_block;
    return DATA_LIST(NULL);
}

CompilerValue block_list_add(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue list = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (list.type == DATA_TYPE_ERROR) return DATA_ERROR;
    list = cast_to_bc_list(compiler, list);
    if (list.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
    if (value.type != DATA_TYPE_CHUNK) {
        value = cast_to_bc(compiler, value, value.type);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
    }

    IrBytecode bc = list.data.chunk_val.bc;
    bytecode_join(&bc, &value.data.chunk_val.bc);
    bytecode_push_op(&bc, IR_ADDL);

    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_list_get(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue list = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (list.type == DATA_TYPE_ERROR) return DATA_ERROR;
    list = cast_to_bc_list(compiler, list);
    if (list.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue ind = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (ind.type == DATA_TYPE_ERROR) return DATA_ERROR;
    ind = cast_to_bc_int(compiler, ind);
    if (ind.type == DATA_TYPE_ERROR) return DATA_ERROR;

    IrBytecode bc = list.data.chunk_val.bc;
    bytecode_join(&bc, &ind.data.chunk_val.bc);
    bytecode_push_op(&bc, IR_INDEXL);

    return DATA_CHUNK(DATA_TYPE_ANY, bc);
}

CompilerValue block_list_set(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue list = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (list.type == DATA_TYPE_ERROR) return DATA_ERROR;
    list = cast_to_bc_list(compiler, list);
    if (list.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue ind = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (ind.type == DATA_TYPE_ERROR) return DATA_ERROR;
    ind = cast_to_bc_int(compiler, ind);
    if (ind.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[2]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
    if (value.type != DATA_TYPE_CHUNK) {
        value = cast_to_bc(compiler, value, value.type);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
    }

    IrBytecode bc = list.data.chunk_val.bc;
    bytecode_join(&bc, &ind.data.chunk_val.bc);
    bytecode_join(&bc, &value.data.chunk_val.bc);
    bytecode_push_op(&bc, IR_SETL);

    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_list_length(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue list = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (list.type == DATA_TYPE_ERROR) return DATA_ERROR;
    list = cast_to_bc_list(compiler, list);
    if (list.type == DATA_TYPE_ERROR) return DATA_ERROR;

    bytecode_push_op(&list.data.chunk_val.bc, IR_LENL);
    return DATA_CHUNK(DATA_TYPE_INTEGER, list.data.chunk_val.bc);
}

CompilerValue block_sleep(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

    value = cast_to_bc_float(compiler, value);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
    assert(value.type == DATA_TYPE_CHUNK);

    bytecode_push_op_func(&value.data.chunk_val.bc, IR_RUN, ir_func_by_hint("std_sleep"));
    return DATA_CHUNK(DATA_TYPE_NULL, value.data.chunk_val.bc);
}

CompilerValue block_random(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue left = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (left.type == DATA_TYPE_ERROR) return DATA_ERROR;

    CompilerValue right = compiler_evaluate_argument(compiler, &block->arguments[1]);
    if (right.type == DATA_TYPE_ERROR) return DATA_ERROR;

    DataType left_type  = left.type  == DATA_TYPE_CHUNK ? left.data.chunk_val.return_type  : left.type;
    DataType right_type = right.type == DATA_TYPE_CHUNK ? right.data.chunk_val.return_type : right.type;

    if (left_type == DATA_TYPE_FLOAT || right_type == DATA_TYPE_FLOAT) {
        left  = cast_to_bc(compiler, left,  DATA_TYPE_FLOAT);
        right = cast_to_bc(compiler, right, DATA_TYPE_FLOAT);
    } else {
        left  = cast_to_bc(compiler, left,  DATA_TYPE_INTEGER);
        right = cast_to_bc(compiler, right, DATA_TYPE_INTEGER);
    }
    if (left.type == DATA_TYPE_ERROR || right.type == DATA_TYPE_ERROR) return DATA_ERROR;

    IrBytecode bc = left.data.chunk_val.bc;
    bytecode_join(&bc, &right.data.chunk_val.bc);

    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint(left.data.chunk_val.return_type == DATA_TYPE_FLOAT ? "std_random_float" : "std_random_int"));
    return DATA_CHUNK(right.data.chunk_val.return_type, bc);
}

CompilerValue block_unix_time(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) block;
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_unix_time"));
    return DATA_CHUNK(DATA_TYPE_INTEGER, bc);
}

CompilerValue block_convert_int(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

    return cast_to(compiler, value, DATA_TYPE_INTEGER);
}

CompilerValue block_convert_float(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

    return cast_to(compiler, value, DATA_TYPE_FLOAT);
}

CompilerValue block_convert_str(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

    return cast_to(compiler, value, DATA_TYPE_STRING);
}

CompilerValue block_convert_bool(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

    return cast_to(compiler, value, DATA_TYPE_BOOL);
}

CompilerValue block_convert_color(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;
    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

    return cast_to(compiler, value, DATA_TYPE_COLOR);
}

CompilerValue block_typeof(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[0]);
    if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;

    DataType type = value.type;
    if (type == DATA_TYPE_CHUNK) type = value.data.chunk_val.return_type;
    if (type == DATA_TYPE_ANY) {
        IrBytecode bc = value.data.chunk_val.bc;
        bytecode_push_op(&bc, IR_TYPEOF);
        return DATA_CHUNK(DATA_TYPE_STRING, bc);
    } else {
        return DATA_STRING((char*)type_to_str(type));
    }
}

CompilerValue block_noop(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) compiler;
    (void) block;
    (void) next_block;
    (void) prev_block;
    return DATA_NOTHING;
}

CompilerValue block_do_nothing(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) compiler;
    (void) block;
    (void) next_block;
    (void) prev_block;
    return EMPTY_CHUNK;
}

CompilerValue block_gc_collect(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) block;
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("std_gc_collect"));
    return DATA_CHUNK(DATA_TYPE_NULL, bc);
}

CompilerValue block_exec_custom(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    IrBytecode bc = EMPTY_BYTECODE;
    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        CompilerValue value = compiler_evaluate_argument(compiler, &block->arguments[i]);
        if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
        if (value.type != DATA_TYPE_CHUNK) {
            value = cast_to_bc(compiler, value, value.type);
            if (value.type == DATA_TYPE_ERROR) return DATA_ERROR;
        }
        bytecode_join(&bc, &value.data.chunk_val.bc);
    }

    CustomFunctionData* block_data = compiler_object_info_get(compiler, block->blockdef);
    if (block_data == OBJECT_NOT_FOUND) {
        compiler_set_error(compiler, "Could not find block data in exec custom block");
        return DATA_ERROR;
    }

    bytecode_push_op_label(&bc, IR_CALL, block_data->label);

    return DATA_CHUNK(DATA_TYPE_ANY, bc);
}

CompilerValue block_custom_arg(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    (void) next_block;
    (void) prev_block;

    Block* define_block = compiler->current_chain->start;
    if (strcmp(define_block->blockdef->id, "define_block")) {
        compiler_set_error(compiler, gettext("Argument block used outside of function"));
        return DATA_ERROR;
    }

    Blockdef* root_blockdef = define_block->arguments[0].data.blockdef;
    bool blockdef_found = false;
    for (size_t i = 0; i < vector_size(root_blockdef->inputs); i++) {
        if (root_blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
        if (root_blockdef->inputs[i].data.arg.blockdef == block->blockdef) {
            blockdef_found = true;
            break;
        }
    }

    if (!blockdef_found) {
        compiler_set_error(compiler, gettext("Argument block used outside of function"));
        return DATA_ERROR;
    }

    void* block_data = compiler_object_info_get(compiler, block->blockdef);
    if (block_data == OBJECT_NOT_FOUND) {
        compiler_set_error(compiler, "Could not find block data in exec custom block");
        return DATA_ERROR;
    }
    size_t arg_id = (size_t)block_data;

    IrBytecode bc = EMPTY_BYTECODE;
    bytecode_push_op_int(&bc, IR_LOAD, arg_id);
    return DATA_CHUNK(compiler->variables.items[arg_id].type, bc);
}

#else

#define MIN_ARG_COUNT(count) \
    if (argc < count) { \
        scrap_log(LOG_ERROR, "Not enough arguments! Expected: %d or more, Got: %d", count, argc); \
        return false; \
    }

LLVMValueRef arg_to_value(Compiler* compiler, Block* block, FuncArg arg) {
    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in arg_to_value");
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
    default:
        assert(false && "Unhandled arg_to_value");
    }
}

LLVMValueRef arg_to_bool(Compiler* compiler, Block* block, FuncArg arg) {
    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in arg_to_bool");
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
    default:
        assert(false && "Unhandled cast to bool");
    }
}

LLVMValueRef arg_to_integer(Compiler* compiler, Block* block, FuncArg arg) {
    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in arg_to_integer");
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
    default:
        assert(false && "Unhandled cast to integer");
    }
}

LLVMValueRef arg_to_float(Compiler* compiler, Block* block, FuncArg arg) {
    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in arg_to_float");
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
    default:
        assert(false && "Unhandled cast to float");
    }
}

LLVMValueRef arg_to_any_string(Compiler* compiler, Block* block, FuncArg arg) {
    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in arg_to_any_string");
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
    default:
        assert(false && "Unhandled cast to any string");
    }
}

LLVMValueRef arg_to_string_ref(Compiler* compiler, Block* block, FuncArg arg) {
    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in arg_to_string_ref");
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
    default:
        assert(false && "Unhandled cast to string ref");
    }
}

LLVMValueRef arg_to_color(Compiler* compiler, Block* block, FuncArg arg) {
    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in arg_to_color");
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
    default:
        assert(false && "Unhandled cast to color");
    }
}

LLVMValueRef arg_to_list(Compiler* compiler, Block* block, FuncArg arg) {
    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in arg_to_list");
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
    default:
        assert(false && "Unhandled cast to string ref");
    }
}

LLVMValueRef arg_to_any(Compiler* compiler, Block* block, FuncArg arg) {
    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in arg_to_any");
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
    default:
        assert(false && "Unhandled cast to string ref");
    }
}

FuncArg arg_cast(Compiler* compiler, Block* block, FuncArg arg, DataType cast_to_type) {
    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in arg_cast");
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
        return DATA_ERROR;
    default:
        assert(false && "Unhandled cast to value typed");
    }
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

    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in block_not_eq");
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

    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in block_eq");
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

    static_assert(DATA_TYPE_LAST == 11, "Exhaustive data type in block_print");
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

    Blockdef* sc_block = blockdef_new("block", BLOCKTYPE_CONTROL, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_block);
    blockdef_add_text(sc_block, gettext("Block"));
    blockdef_register(vm, sc_block);
    block_category_add_blockdef(cat_control, sc_block);

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
    blockdef_add_text(sc_sleep, gettext("s"));
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
    blockdef_add_text(sc_do_nothing, "//    "); // Spaces make the block longer
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
    blockdef_add_argument(sc_list_get, "1", "0", BLOCKCONSTR_UNLIMITED);
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
