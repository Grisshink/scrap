// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-2025 Grisshink
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
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define TERM_WHITE (TermColor) { 0xff, 0xff, 0xff, 0xff }
#define TERM_BLACK (TermColor) { 0x00, 0x00, 0x00, 0xff }

Terminal term = {0};

int leading_ones(unsigned char byte) {
    int out = 0;
    while (byte & 0x80) {
        out++;
        byte <<= 1;
    }
    return out;
}

void term_init(MeasureTextSliceFunc measure_text, void* font, unsigned short font_size) {
    sem_init(&term.input_sem, 0, 0);
    term.lock = mutex_new();
    term.is_buffer_dirty = true;
    term.cursor_fg_color = TERM_WHITE;
    term.cursor_bg_color = TERM_BLACK;
    term.measure_text = measure_text;
    term.font = font;
    term.font_size = font_size;

    term_resize(0, 0);
}

void term_restart(void) {
    sem_destroy(&term.input_sem);
    sem_init(&term.input_sem, 0, 0);
    term.buf_start = 0;
    term.buf_end = 0;
    term.cursor_fg_color = TERM_WHITE;
    term.cursor_bg_color = TERM_BLACK;
    term.clear_color = TERM_BLACK;
    term_clear();
}

void term_free(void) {
    mutex_free(&term.lock);
    sem_destroy(&term.input_sem);
}

void term_input_put_char(char ch) {
    mutex_lock(&term.lock);
    term.input_buf[term.buf_end] = ch;
    term.buf_end = (term.buf_end + 1) % TERM_INPUT_BUF_SIZE;
    mutex_unlock(&term.lock);
    sem_post(&term.input_sem);
}

char term_input_get_char(void) {
    sem_wait(&term.input_sem);
    mutex_lock(&term.lock);
    int out = term.input_buf[term.buf_start];
    term.buf_start = (term.buf_start + 1) % TERM_INPUT_BUF_SIZE;
    mutex_unlock(&term.lock);
    return out;
}

void term_scroll_down(void) {
    mutex_lock(&term.lock);
    memmove(term.buffer, term.buffer + term.char_w, term.char_w * (term.char_h - 1) * sizeof(*term.buffer));
    for (int i = term.char_w * (term.char_h - 1); i < term.char_w * term.char_h; i++) {
        strncpy(term.buffer[i].ch, " ", ARRLEN(term.buffer[i].ch));
        term.buffer[i].fg_color = TERM_WHITE;
        term.buffer[i].bg_color = term.clear_color;
    }
    mutex_unlock(&term.lock);
}

void term_set_fg_color(TermColor color) {
    mutex_lock(&term.lock);
    term.cursor_fg_color = color;
    mutex_unlock(&term.lock);
}

void term_set_bg_color(TermColor color) {
    mutex_lock(&term.lock);
    term.cursor_bg_color = color;
    mutex_unlock(&term.lock);
}

void term_set_clear_color(TermColor color) {
    mutex_lock(&term.lock);
    term.clear_color = color;
    mutex_unlock(&term.lock);
}

int term_print_str(const char* str) {
    int len = 0;
    assert(term.buffer != NULL);

    mutex_lock(&term.lock);
    if (*str) term.is_buffer_dirty = true;
    while (*str) {
        if (term.cursor_pos >= term.char_w * term.char_h) {
            term.cursor_pos = term.char_w * term.char_h - term.char_w;
            term_scroll_down();
        }
        if (*str == '\t') {
            term_print_str("    ");
            str++;
            continue;
        }
        if (*str == '\n') {
            term.cursor_pos += term.char_w;
            term.cursor_pos -= term.cursor_pos % term.char_w;
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
        for (; i < mb_size; i++) term.buffer[term.cursor_pos].ch[i] = str[i];
        term.buffer[term.cursor_pos].ch[i] = 0;
        term.buffer[term.cursor_pos].fg_color = term.cursor_fg_color;
        term.buffer[term.cursor_pos].bg_color = term.cursor_bg_color;

        str += mb_size;
        term.cursor_pos++;
        len++;
    }
    mutex_unlock(&term.lock);

    return len;
}

int term_print_integer(int value) {
    char converted[12];
    snprintf(converted, 12, "%d", value);
    return term_print_str(converted);
}

int term_print_float(double value) {
    char converted[20];
    snprintf(converted, 20, "%f", value);
    return term_print_str(converted);
}

int term_print_bool(bool value) {
    return term_print_str(value ? "true" : "false");
}

void term_clear(void) {
    mutex_lock(&term.lock);
    for (int i = 0; i < term.char_w * term.char_h; i++) {
        strncpy(term.buffer[i].ch, " ", ARRLEN(term.buffer[i].ch));
        term.buffer[i].fg_color = TERM_WHITE;
        term.buffer[i].bg_color = term.clear_color;
    }
    term.cursor_pos = 0;
    mutex_unlock(&term.lock);
}

void term_resize(float screen_w, float screen_h) {
    mutex_lock(&term.lock);
    term.size = (TermVec) { screen_w, screen_h };

    term.char_size = term.measure_text(term.font, "A", 1, term.font_size);
    TermVec new_buffer_size = { term.size.x / term.char_size.x, term.size.y / term.char_size.y };
    int new_char_w = (int)new_buffer_size.x,
        new_char_h = (int)new_buffer_size.y;

    if (term.char_w != new_char_w || term.char_h != new_char_h) {
        int buf_size = new_char_w * new_char_h * sizeof(*term.buffer);
        TerminalChar* new_buffer = malloc(buf_size);
        if (term.buffer) {
            for (int y = 0; y < new_char_h; y++) {
                for (int x = 0; x < new_char_w; x++) {
                    TerminalChar* ch = &new_buffer[x + y * new_char_w];

                    if (x >= term.char_w || y >= term.char_h) {
                        strncpy(ch->ch, " ", ARRLEN(ch->ch));
                        ch->fg_color = TERM_WHITE;
                        ch->bg_color = term.clear_color;
                        continue;
                    }

                    *ch = term.buffer[x + y * term.char_w];
                }
            }

            int term_x = term.cursor_pos % term.char_w,
                term_y = term.cursor_pos / term.char_w;
            if (term_x >= new_char_w) term_x = new_char_w - 1;
            if (term_y >= new_char_h) term_y = new_char_h - 1;
            term.cursor_pos = term_x + term_y * new_char_w;

            free(term.buffer);

            term.char_w = new_char_w;
            term.char_h = new_char_h;
            term.buffer = new_buffer;
        } else {
            term.char_w = new_char_w;
            term.char_h = new_char_h;
            term.buffer = new_buffer;
            term_clear();
        }
    }
    mutex_unlock(&term.lock);
}
