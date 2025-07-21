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

#ifndef SCRAP_STD_TYPES_H
#define SCRAP_STD_TYPES_H

typedef struct {
    unsigned int size;
    unsigned int capacity;
    char str[];
} StringHeader;

typedef enum {
    ANY_TYPE_UNKNOWN = 0,
    ANY_TYPE_NOTHING,
    ANY_TYPE_INT,
    ANY_TYPE_DOUBLE,
    ANY_TYPE_STRING_LITERAL, // Literal string, stored in global memory
    ANY_TYPE_STRING_REF, // Pointer to a string type, managed by the current memory allocator (GC)
    ANY_TYPE_BOOL,
    ANY_TYPE_LIST,
    ANY_TYPE_ANY,
    ANY_TYPE_CONTROL,
    ANY_TYPE_BLOCKDEF,
} AnyValueType;

typedef struct AnyValue AnyValue;
typedef struct List List;

typedef union {
    char* str_val;
    int int_val;
    double double_val;
    List* list_val;
    AnyValue* any_val;
} AnyValueData;

struct AnyValue {
    AnyValueType type;
    AnyValueData data;
};

struct List {
    long size;
    long capacity;
    AnyValue* values;
};

#endif // SCRAP_STD_TYPES_H
