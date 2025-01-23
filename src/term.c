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

#include "term.h"
#include "scrap.h"

#include <stdlib.h>
#include <string.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))

Terminal term = {0};

void term_init(void) {
    sem_init(&term.input_sem, 0, 0);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&term.lock, &attr);
    pthread_mutexattr_destroy(&attr);
    term_resize(0, 0);
}

void term_restart(void) {
    sem_destroy(&term.input_sem);
    sem_init(&term.input_sem, 0, 0);
    term.buf_start = 0;
    term.buf_end = 0;
    term_clear();
}

void term_free(void) {
    pthread_mutex_destroy(&term.lock);
    sem_destroy(&term.input_sem);
}

void term_input_put_char(char ch) {
    pthread_mutex_lock(&term.lock);
    term.input_buf[term.buf_end] = ch;
    term.buf_end = (term.buf_end + 1) % TERM_INPUT_BUF_SIZE;
    pthread_mutex_unlock(&term.lock);
    sem_post(&term.input_sem);
}

char term_input_get_char(void) {
    sem_wait(&term.input_sem);
    pthread_mutex_lock(&term.lock);
    int out = term.input_buf[term.buf_start];
    term.buf_start = (term.buf_start + 1) % TERM_INPUT_BUF_SIZE;
    pthread_mutex_unlock(&term.lock);
    return out;
}

void term_scroll_down(void) {
    pthread_mutex_lock(&term.lock);
    memmove(term.buffer, term.buffer + term.char_w, term.char_w * (term.char_h - 1) * sizeof(*term.buffer));
    for (int i = term.char_w * (term.char_h - 1); i < term.char_w * term.char_h; i++) strncpy(term.buffer[i], " ", ARRLEN(*term.buffer));
    pthread_mutex_unlock(&term.lock);
}

int term_print_str(const char* str) {
    int len = 0;
    if (!term.buffer) return len;

    pthread_mutex_lock(&term.lock);
    while (*str) {
        if (term.cursor_pos >= term.char_w * term.char_h) {
            term.cursor_pos = term.char_w * term.char_h - term.char_w;
            term_scroll_down();
        }
        if (*str == '\n') {
            term.cursor_pos += term.char_w;
            str++;
            if (term.cursor_pos >= term.char_w * term.char_h) {
                term.cursor_pos -= term.char_w;
                term_scroll_down();
            }
            continue;
        }
        if (*str == '\r') {
            term.cursor_pos -= term.cursor_pos % term.char_w;
            str++;
            continue;
        }

        int mb_size = leading_ones(*str);
        if (mb_size == 0) mb_size = 1;
        int i = 0;
        for (; i < mb_size; i++) term.buffer[term.cursor_pos][i] = str[i];
        term.buffer[term.cursor_pos][i] = 0;

        str += mb_size;
        term.cursor_pos++;
        len++;
    }
    pthread_mutex_unlock(&term.lock);

    return len;
}

int term_print_int(int value) {
    char converted[12];
    snprintf(converted, 12, "%d", value);
    return term_print_str(converted);
}

int term_print_double(double value) {
    char converted[20];
    snprintf(converted, 20, "%f", value);
    return term_print_str(converted);
}

void term_clear(void) {
    pthread_mutex_lock(&term.lock);
    for (int i = 0; i < term.char_w * term.char_h; i++) strncpy(term.buffer[i], " ", ARRLEN(*term.buffer));
    term.cursor_pos = 0;
    pthread_mutex_unlock(&term.lock);
}

void term_resize(float screen_w, float screen_h) {
    pthread_mutex_lock(&term.lock);
    term.size = (Rectangle) { 0, 0, screen_w, screen_h };

    GuiMeasurement char_size = custom_measure(font_mono, "A", TERM_CHAR_SIZE);
    term.char_size = (Vector2) { char_size.w, char_size.h };
    Vector2 new_buffer_size = { term.size.width / term.char_size.x, term.size.height / term.char_size.y };

    if (term.char_w != (int)new_buffer_size.x || term.char_h != (int)new_buffer_size.y) {
        term.char_w = new_buffer_size.x;
        term.char_h = new_buffer_size.y;

        if (term.buffer) free(term.buffer);
        int buf_size = term.char_w * term.char_h * sizeof(*term.buffer);
        term.buffer = malloc(buf_size);
        term_clear();
    }
    pthread_mutex_unlock(&term.lock);
}
