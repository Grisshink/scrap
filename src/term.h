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

#ifndef TERM_H
#define TERM_H

#include <semaphore.h>
#include <pthread.h>
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

    pthread_mutex_t lock;
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
int term_print_int(int value);
int term_print_double(double value);
int term_print_bool(bool value);
void term_clear(void);
void term_resize(float screen_w, float screen_h);
void term_free(void);
void term_restart(void);

#endif // TERM_H
