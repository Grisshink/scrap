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

#ifndef TERM_H
#define TERM_H

#include "thread.h"

#include <semaphore.h>
#include <stdbool.h>

#define TERM_INPUT_BUF_SIZE 256

typedef struct {
    unsigned char r, g, b, a;
} TermColor;

typedef struct {
    char ch[5];
    TermColor fg_color;
    TermColor bg_color;
} TerminalChar;

typedef struct {
    float x, y;
} TermVec;

typedef TermVec (*MeasureTextSliceFunc)(void* font, const char* text, unsigned int text_size, unsigned short font_size);

typedef struct {
    MeasureTextSliceFunc measure_text;
    void* font;
    unsigned short font_size;

    Mutex lock;
    TermVec size;
    int char_w, char_h;
    int cursor_pos;
    TermColor cursor_fg_color, cursor_bg_color;
    TermVec char_size;
    TerminalChar *buffer;
    bool is_buffer_dirty;

    TermColor clear_color;

    sem_t input_sem;
    char input_buf[TERM_INPUT_BUF_SIZE];
    int buf_start;
    int buf_end;
} Terminal;

extern Terminal term;

void term_init(MeasureTextSliceFunc measure_text, void* font, unsigned short font_size);
void term_input_put_char(char ch);
char term_input_get_char(void);
void term_scroll_down(void);
void term_set_fg_color(TermColor color);
void term_set_bg_color(TermColor color);
void term_set_clear_color(TermColor color);
int term_print_str(const char* str);
int term_print_integer(int value);
int term_print_float(double value);
int term_print_bool(bool value);
int term_print_color(TermColor value);
void term_clear(void);
void term_resize(float screen_w, float screen_h);
void term_free(void);
void term_restart(void);

#endif // TERM_H
