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

// TODO: Move type declarations into ast.h to avoid this gross shit

#ifndef COMPILER_COMMON_H
#define COMPILER_COMMON_H

#include <llvm-c/Core.h>

typedef enum {
    CONTROL_BEGIN,
    CONTROL_END,
} FuncArgControlType;

typedef struct {
    FuncArgControlType type;
    LLVMBasicBlockRef block;
} ControlData;

typedef enum {
    FUNC_ARG_UNKNOWN = 0,
    FUNC_ARG_NOTHING,
    FUNC_ARG_INT,
    FUNC_ARG_DOUBLE,
    FUNC_ARG_STRING_LITERAL, // Literal string, stored in global memory
    FUNC_ARG_STRING_REF, // Pointer to a string type, managed by the current memory allocator (GC)
    FUNC_ARG_BOOL,
    FUNC_ARG_LIST,
    FUNC_ARG_ANY,
    FUNC_ARG_CONTROL,
} FuncArgType;

typedef union {
    LLVMValueRef value;
    ControlData control;
    const char* str;
} FuncArgData;

typedef struct {
    FuncArgType type;
    FuncArgData data;
} FuncArg;

typedef struct List List;

typedef union {
    char* str_val;
    int int_val;
    double double_val;
    List* list_val;
} AnyValueData;

typedef struct {
    FuncArgType type;
    AnyValueData data;
} AnyValue;

struct List {
    long size;
    long capacity;
    AnyValue* values;
};

#endif // COMPILER_COMMON_H
