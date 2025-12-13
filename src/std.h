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

#ifndef SCRAP_STD_H
#define SCRAP_STD_H

#include "gc.h"

#include <stdbool.h>

typedef struct {
    unsigned int size;
    unsigned int capacity;
    char str[];
} StringHeader;

typedef struct AnyValue AnyValue;
typedef struct List List;

typedef union {
    char* literal_val;
    StringHeader* str_val;
    int integer_val;
    double float_val;
    List* list_val;
    AnyValue* any_val;
} AnyValueData;

struct AnyValue {
    DataType type;
    AnyValueData data;
};

struct List {
    long size;
    long capacity;
    AnyValue* values;
};

// Math
int std_int_pow(int base, int exp);

// List operations
List* std_list_new(Gc* gc);
void std_list_add_any(Gc* gc, List* list, AnyValue any);
void std_list_add(Gc* gc, List* list, DataType data_type, ...);
void std_list_set(List* list, int index, DataType data_type, ...);
AnyValue* std_list_get(Gc* gc, List* list, int index);
int std_list_length(List* list);

// Any operations
AnyValue* std_any_from_value(Gc* gc, DataType data_type, ...);
int std_integer_from_any(AnyValue* value);
double std_float_from_any(AnyValue* value);
int std_bool_from_any(AnyValue* value);
List* std_list_from_any(Gc* gc, AnyValue* value);
StringHeader* std_string_from_any(Gc* gc, AnyValue* value);
bool std_any_is_eq(AnyValue* left, AnyValue* right);

// String operations
StringHeader* std_string_from_literal(Gc* gc, const char* literal, unsigned int size);
StringHeader* std_string_from_integer(Gc* gc, int value);
StringHeader* std_string_from_bool(Gc* gc, bool value);
StringHeader* std_string_from_float(Gc* gc, double value);
char* std_string_get_data(StringHeader* str);

int std_string_length(StringHeader* str);
StringHeader* std_string_letter_in(Gc* gc, int target, StringHeader* input_str);
StringHeader* std_string_substring(Gc* gc, int begin, int end, StringHeader* input_str);
StringHeader* std_string_join(Gc* gc, StringHeader* left, StringHeader* right);
bool std_string_is_eq(StringHeader* left, StringHeader* right);
StringHeader* std_string_chr(Gc* gc, int value);
int std_string_ord(StringHeader* str);

// Terminal control
StringHeader* std_term_get_char(Gc* gc);
void std_term_set_cursor(int x, int y);
int std_term_cursor_x(void);
int std_term_cursor_y(void);
int std_term_cursor_max_x(void);
int std_term_cursor_max_y(void);
StringHeader* std_term_get_input(Gc* gc);
int std_term_print_list(List* list);
int std_term_print_any(AnyValue* any);
int std_term_print_str(const char* str);
int std_term_print_integer(int value);
int std_term_print_float(double value);
int std_term_print_bool(bool value);
void std_term_clear(void);

#ifdef STANDALONE_STD
typedef struct {
    unsigned char r, g, b, a;
} Color;

void std_term_set_fg_color(Color color);
void std_term_set_bg_color(Color color);
void std_term_set_clear_color(Color color);
#else
// TODO: Remove this dependency by doing stdio
#include "term.h"

void std_term_set_fg_color(TermColor color);
void std_term_set_bg_color(TermColor color);
void std_term_set_clear_color(TermColor color);
#endif

// Misc
int std_sleep(int usecs);
int std_get_random(int min, int max);
void std_set_random_seed(int seed);

#endif // SCRAP_STD_H
