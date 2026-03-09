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

#ifndef SCRAP_STD_H
#define SCRAP_STD_H

#include "scrap_ir.h"
#include <stdbool.h>

IrRunFunction std_resolve_function(IrExec* exec, const char* hint);

// Math
int std_int_pow(int base, int exp);

// Terminal control
// StringHeader* std_term_get_char(Gc* gc);
void std_term_set_cursor(int x, int y);
int std_term_cursor_x(void);
int std_term_cursor_y(void);
int std_term_cursor_max_x(void);
int std_term_cursor_max_y(void);
// StringHeader* std_term_get_input(Gc* gc);
void std_term_clear(void);

#ifdef STANDALONE_STD
typedef struct {
    unsigned char r, g, b, a;
} Color;

void std_term_set_fg_color(Color color);
void std_term_set_bg_color(Color color);
void std_term_set_clear_color(Color color);
#else
#include "term.h"

void std_term_set_fg_color(TermColor color);
void std_term_set_bg_color(TermColor color);
void std_term_set_clear_color(TermColor color);
#endif

// Misc
int std_sleep(int usecs);
int std_get_random(int min, int max);
void std_set_random_seed(int seed);
// StdColor std_parse_color(const char* value);

#endif // SCRAP_STD_H
