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

#include "raylib.h"
#include <semaphore.h>
#include <pthread.h>

#define TERM_INPUT_BUF_SIZE 256
#define TERM_CHAR_SIZE (int)(conf.font_size * 0.6)

typedef struct {
    char ch[5];
    Color fg_color;
    Color bg_color;
} TerminalChar;

typedef struct {
    pthread_mutex_t lock;
    Rectangle size;
    int char_w, char_h;
    int cursor_pos;
    Color cursor_fg_color, cursor_bg_color;
    Vector2 char_size;
    TerminalChar *buffer;
    bool is_buffer_dirty;

    sem_t input_sem;
    char input_buf[TERM_INPUT_BUF_SIZE];
    int buf_start;
    int buf_end;
} Terminal;

extern Terminal term;

void term_init(void);
void term_input_put_char(char ch);
char term_input_get_char(void);
void term_scroll_down(void);
void term_set_fg_color(Color color);
void term_set_bg_color(Color color);
int term_print_str(const char* str);
int term_print_int(int value);
int term_print_double(double value);
void term_clear(void);
void term_resize(float screen_w, float screen_h);
void term_free(void);
void term_restart(void);

#endif // TERM_H
