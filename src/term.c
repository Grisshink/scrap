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

#include "term.h"
#include "util.h"
#include "vec.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pty.h>
#include <fcntl.h>

#define TERM_BLACK (TermColor) { 0x00, 0x00, 0x00, 0xff }
#define TERM_RED (TermColor) { 230, 41, 55, 255 }
#define TERM_YELLOW (TermColor) { 253, 249, 0, 255 }
#define TERM_GREEN (TermColor) { 0, 228, 48, 255 }
#define TERM_BLUE (TermColor) { 0, 121, 241, 255 }
#define TERM_PURPLE (TermColor) { 200, 122, 255, 255 }
#define TERM_CYAN (TermColor) { 0x00, 0xff, 0xff, 0xff }
#define TERM_WHITE (TermColor) { 0xff, 0xff, 0xff, 0xff }

Terminal term = {0};

static void term_clear_forward(int pos);
static void term_clear_backward(int pos);

int leading_ones(unsigned char byte) {
    int out = 0;
    while (byte & 0x80) {
        out++;
        byte <<= 1;
    }
    return out;
}

void term_init(MeasureTextSliceFunc measure_text, void* font, unsigned short font_size) {
    term.is_buffer_dirty = true;
    term.cursor_fg_color = TERM_WHITE;
    term.cursor_bg_color = TERM_BLACK;
    term.measure_text = measure_text;
    term.font = font;
    term.font_size = font_size;
    term.input_buf_size = 0;
    term.master_fd = -1;
    term.slave_fd = -1;
    term.output_buf = vector_create();
    memset(&term.print_state, 0, sizeof(term.print_state));

    if (openpty(&term.master_fd, &term.slave_fd, NULL, NULL, NULL) == -1) {
        scrap_log(LOG_ERROR, "openpty: %s", strerror(errno));
    } else {
        int flags = fcntl(term.master_fd, F_GETFL);
        flags |= O_NONBLOCK;
        fcntl(term.master_fd, F_SETFL, flags);
    }

    term_resize(0, 0);
}

void term_restart(void) {
    term.input_buf_size = 0;
    term.cursor_fg_color = TERM_WHITE;
    term.cursor_bg_color = TERM_BLACK;
    term.clear_color = TERM_BLACK;
    vector_clear(term.output_buf);
    memset(&term.print_state, 0, sizeof(term.print_state));
    term_clear();
}

void term_free(void) {
    vector_free(term.output_buf);
    term.output_buf = NULL;

    if (term.master_fd != -1) {
        if (close(term.master_fd) == -1) scrap_log(LOG_ERROR, "close: %s", strerror(errno));
        term.master_fd = -1;
    }

    if (term.slave_fd != -1) {
        if (close(term.slave_fd) == -1) scrap_log(LOG_ERROR, "close: %s", strerror(errno));
        term.slave_fd = -1;
    }
}

void term_input_put_char(char ch) {
    if (term.master_fd == -1) {
        scrap_log(LOG_WARNING, "[TERM] Attempt to write '%c' char to closed fd", ch);
        return;
    }

    term.input_buf[term.input_buf_size++] = ch;
    if (term.input_buf_size >= TERM_INPUT_BUF_SIZE || ch == '\n') {
        write(term.master_fd, term.input_buf, term.input_buf_size);
        term.input_buf_size = 0;
    }
}

void term_flush_input(void) {
    write(term.master_fd, term.input_buf, term.input_buf_size);
    term.input_buf_size = 0;
}

int term_read_output(void) {
    if (term.master_fd == -1) return -1;

    char read_buf[1024];
    int buf_size = read(term.master_fd, read_buf, 1024);
    if (buf_size == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
        scrap_log(LOG_ERROR, "[TERM] read: %s", strerror(errno));
        return -1;
    }

    if (buf_size == 0) return -1; // EOF

    for (int i = 0; i < buf_size; i++) vector_add(&term.output_buf, read_buf[i]);
    return 0;
}

bool term_wait_for_output(void) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(term.master_fd, &fds);

    if (pselect(term.master_fd + 1, &fds, NULL, NULL, NULL, NULL) == -1) {
        if (errno != EINTR) scrap_log(LOG_ERROR, "[TERM] pselect: %s", strerror(errno));
        return false;
    }

    vector_clear(term.output_buf);

    int err = 0;
    while (!err) err = term_read_output();
    vector_add(&term.output_buf, 0);

    if (vector_size(term.output_buf) > 1) term_print_str(term.output_buf);

    return err == 1;
}

void term_scroll_up(void) {
    memmove(term.buffer + term.char_w, term.buffer, term.char_w * (term.char_h - 1) * sizeof(*term.buffer));
    for (int i = 0; i < term.char_w; i++) {
        strncpy(term.buffer[i].ch, " ", ARRLEN(term.buffer[i].ch));
        term.buffer[i].fg_color = TERM_WHITE;
        term.buffer[i].bg_color = term.clear_color;
    }
}

void term_scroll_down(void) {
    memmove(term.buffer, term.buffer + term.char_w, term.char_w * (term.char_h - 1) * sizeof(*term.buffer));
    for (int i = term.char_w * (term.char_h - 1); i < term.char_w * term.char_h; i++) {
        strncpy(term.buffer[i].ch, " ", ARRLEN(term.buffer[i].ch));
        term.buffer[i].fg_color = TERM_WHITE;
        term.buffer[i].bg_color = term.clear_color;
    }
}

void term_set_fg_color(TermColor color) {
    term.cursor_fg_color = color;
}

void term_set_bg_color(TermColor color) {
    term.cursor_bg_color = color;
}

void term_set_clear_color(TermColor color) {
    term.clear_color = color;
}

void handle_graphic_mode(void) {
    switch (term.print_state.args[0]) {
    case 0: // Reset
        term.cursor_fg_color = TERM_WHITE;
        term.cursor_bg_color = TERM_BLACK;
        break;

    // Foreground colors
    case 30: case 90: term.cursor_fg_color = TERM_BLACK; break;
    case 31: case 91: term.cursor_fg_color = TERM_RED; break;
    case 32: case 92: term.cursor_fg_color = TERM_GREEN; break;
    case 33: case 93: term.cursor_fg_color = TERM_YELLOW; break;
    case 34: case 94: term.cursor_fg_color = TERM_BLUE; break;
    case 35: case 95: term.cursor_fg_color = TERM_PURPLE; break;
    case 36: case 96: term.cursor_fg_color = TERM_CYAN; break;
    case 37: case 97: term.cursor_fg_color = TERM_WHITE; break;
    case 38:
        if (term.print_state.args[1] == 5) {
            // 256-color mode is not supported
        } else if (term.print_state.args[1] == 2) {
            term.cursor_fg_color = (TermColor) { term.print_state.args[2], term.print_state.args[3], term.print_state.args[4], 0xff };
        }
        break;
    // Background colors
    case 40: case 100: term.cursor_bg_color = TERM_BLACK; break;
    case 41: case 101: term.cursor_bg_color = TERM_RED; break;
    case 42: case 102: term.cursor_bg_color = TERM_GREEN; break;
    case 43: case 103: term.cursor_bg_color = TERM_YELLOW; break;
    case 44: case 104: term.cursor_bg_color = TERM_BLUE; break;
    case 45: case 105: term.cursor_bg_color = TERM_PURPLE; break;
    case 46: case 106: term.cursor_bg_color = TERM_CYAN; break;
    case 47: case 107: term.cursor_bg_color = TERM_WHITE; break;
    case 48:
        if (term.print_state.args[1] == 5) {
            // 256-color mode is not supported
        } else if (term.print_state.args[1] == 2) {
            term.cursor_bg_color = (TermColor) { term.print_state.args[2], term.print_state.args[3], term.print_state.args[4], 0xff };
        }
        break;
    default:
        // Not supported
        break;
    }
}

void exec_escape(char command) {
    if (term.print_state.mode == TERM_ESCAPE_CSI) {
        term.print_state.mode = TERM_NONE;
        int clear_mode;
        switch (command) {
        case 'A': term.cursor_pos -= term.char_w * (term.print_state.args_size ? term.print_state.args[0] : 1); break; // Cursor up
        case 'B': term.cursor_pos += term.char_w * (term.print_state.args_size ? term.print_state.args[0] : 1); break; // Cursor down
        case 'C': term.cursor_pos += term.print_state.args_size ? term.print_state.args[0] : 1; break; // Cursor right
        case 'D': term.cursor_pos -= term.print_state.args_size ? term.print_state.args[0] : 1; break; // Cursor left
        case 'E': // Cursor next line
            term.cursor_pos += term.char_w * (term.print_state.args_size ? term.print_state.args[0] : 1);
            term.cursor_pos -= term.cursor_pos % term.char_w;
            break;
        case 'F': // Cursor prev line
            term.cursor_pos -= term.char_w * (term.print_state.args_size ? term.print_state.args[0] : 1);
            term.cursor_pos -= term.cursor_pos % term.char_w;
            break;
        case 'G': // Cursor set col
            term.cursor_pos -= term.cursor_pos % term.char_w;
            term.cursor_pos += term.print_state.args_size ? term.print_state.args[0] - 1 : 0; break;
            break;
        case 'f':
        case 'H': // Cursor set pos
            int pos_y = term.print_state.args_size > 0 ? term.print_state.args[0] - 1 : 0;
            int pos_x = term.print_state.args_size > 1 ? term.print_state.args[1] - 1 : 0;

            pos_x = CLAMP(pos_x, 0, term.char_w - 1);
            pos_y = CLAMP(pos_y, 0, term.char_h - 1);

            term.cursor_pos = pos_x + pos_y * term.char_w;
            break;
        case 'J': // Erase term
            clear_mode = term.print_state.args[0];
            if (clear_mode == 0) {
                term_clear_forward(term.cursor_pos);
            } else if (clear_mode == 1) {
                term_clear_backward(term.cursor_pos);
            } else if (clear_mode == 2) {
                term_clear();
            }
            break;
        case 'K': // Erase line
            clear_mode = term.print_state.args[0];
            if (clear_mode == 0) {
                int end_pos = term.cursor_pos + term.char_w - (term.cursor_pos % term.char_w);
                for (int pos = term.cursor_pos; pos < end_pos; pos++) {
                    strncpy(term.buffer[pos].ch, " ", ARRLEN(term.buffer[pos].ch));
                    term.buffer[pos].fg_color = TERM_WHITE;
                    term.buffer[pos].bg_color = term.clear_color;
                }
            } else if (clear_mode == 1) {
                int start_pos = term.cursor_pos - (term.cursor_pos % term.char_w);
                for (int pos = start_pos; pos <= term.cursor_pos; pos++) {
                    strncpy(term.buffer[pos].ch, " ", ARRLEN(term.buffer[pos].ch));
                    term.buffer[pos].fg_color = TERM_WHITE;
                    term.buffer[pos].bg_color = term.clear_color;
                }
            } else if (clear_mode == 2) {
                int start_pos = term.cursor_pos - (term.cursor_pos % term.char_w);
                int end_pos = term.cursor_pos + term.char_w - (term.cursor_pos % term.char_w);
                for (int pos = start_pos; pos < end_pos; pos++) {
                    strncpy(term.buffer[pos].ch, " ", ARRLEN(term.buffer[pos].ch));
                    term.buffer[pos].fg_color = TERM_WHITE;
                    term.buffer[pos].bg_color = term.clear_color;
                }
            }
            break;
        case 'm': handle_graphic_mode(); break;
        case '?':
            term.print_state.mode = TERM_ESCAPE_PRIVATE;
            term.print_state.arg_mode = true;
            term.print_state.args_size = 0;
            term.print_state.arg_buf_size = 0;
            break;
        default:
            scrap_log(LOG_WARNING, "Unhandled CSI: %c (0x%02X)", command, (unsigned char)command);
            break;
        }
    } else {
        term.print_state.mode = TERM_NONE;
    }
}

void handle_args(char ch) {
    if (ch >= '0' && ch <= '9') {
        term.print_state.arg_buf[term.print_state.arg_buf_size++] = ch;
        return;
    }
    term.print_state.arg_buf[term.print_state.arg_buf_size] = 0;
    if (term.print_state.arg_buf_size != 0) {
        term.print_state.arg_buf_size = 0;
        term.print_state.args[term.print_state.args_size++] = atoi(term.print_state.arg_buf);
    }

    if (ch != ';') {
        term.print_state.arg_mode = false;
        memset(term.print_state.args + term.print_state.args_size, 0, sizeof(int) * (5 - term.print_state.args_size));
        exec_escape(ch);
    }
}

void handle_escapes(char command) {
    if (command == '[') {
        term.print_state.mode = TERM_ESCAPE_CSI;
        term.print_state.arg_mode = true;
        term.print_state.args_size = 0;
        term.print_state.arg_buf_size = 0;
    } else if (command == ']' || command == 'k') {
        term.print_state.mode = TERM_ESCAPE_OSC;
        term.print_state.string_mode = true;
    } else if (command == 'M') { // \n but backward
        term.cursor_pos -= term.char_w;
        if (term.cursor_pos < 0) {
            term.cursor_pos += term.char_w;
            term_scroll_up();
        }
        term.print_state.mode = TERM_NONE;
    } else {
        term.print_state.mode = TERM_NONE;
        term.print_state.arg_mode = false;
        term.print_state.string_mode = false;
    }
}

#define ADVANCE_CHAR do { \
    term.print_state.prev_char = *str; \
    str++;  \
} while (0)

void term_print_str(const char* str) {
    assert(term.buffer != NULL);

    if (*str) term.is_buffer_dirty = true;
    while (*str) {
        if (term.print_state.mode == TERM_UTF) {
            assert(term.print_state.mb_current < term.print_state.mb_count);
            term.buffer[term.cursor_pos].ch[term.print_state.mb_current++] = *str;
            if (term.print_state.mb_current == term.print_state.mb_count) {
                term.print_state.mode = TERM_NONE;

                term.buffer[term.cursor_pos].fg_color = term.cursor_fg_color;
                term.buffer[term.cursor_pos].bg_color = term.cursor_bg_color;
                term.buffer[term.cursor_pos].ch[term.print_state.mb_current] = 0;

                term.cursor_pos++;
                if (term.cursor_pos >= term.char_w * term.char_h) {
                    term.cursor_pos = term.char_w * term.char_h - term.char_w;
                    term_scroll_down();
                }
            }
            ADVANCE_CHAR;
            continue;
        }

        if (term.print_state.string_mode) {
            if (*str == '\a' || (*str == '\\' && term.print_state.prev_char == '\033')) {
                term.print_state.string_mode = false;
                term.print_state.mode = TERM_NONE;
            }
            ADVANCE_CHAR;
            continue;
        }

        if (term.print_state.arg_mode) {
            handle_args(*str);
            ADVANCE_CHAR;
            continue;
        }

        if (term.print_state.mode == TERM_ESCAPE_COMMAND) {
            handle_escapes(*str);
            ADVANCE_CHAR;
            continue;
        }

        if (*str == '\033') {
            term.print_state.mode = TERM_ESCAPE_COMMAND;
            ADVANCE_CHAR;
            continue;
        }

        if (*str == '\b') {
            int pos = --term.cursor_pos;
            strncpy(term.buffer[pos].ch, " ", ARRLEN(term.buffer[pos].ch));
            term.buffer[pos].fg_color = TERM_WHITE;
            term.buffer[pos].bg_color = term.clear_color;
            ADVANCE_CHAR;
            continue;
        }
        if (*str == '\a') { // Bell char, skip it
            ADVANCE_CHAR;
            continue;
        }
        if (*str == '\t') {
            term_print_str("    ");
            ADVANCE_CHAR;
            continue;
        }
        if (*str == '\n') {
            term.cursor_pos += term.char_w;
            term.cursor_pos -= term.cursor_pos % term.char_w;
            ADVANCE_CHAR;
            if (term.cursor_pos >= term.char_w * term.char_h) {
                term.cursor_pos -= term.char_w;
                term_scroll_down();
            }
            continue;
        }
        if (*str == '\r') {
            term.cursor_pos -= term.cursor_pos % term.char_w;
            ADVANCE_CHAR;
            continue;
        }

        if ((unsigned char)*str < 32 || *str == 0x7f) {
            scrap_log(LOG_WARNING, "Unhandled control char 0x%02X", (unsigned char)*str);
            ADVANCE_CHAR;
            continue;
        }

        int mb_size = leading_ones(*str);
        if (mb_size == 0) mb_size = 1;

        if (mb_size > 1) {
            term.print_state.mode = TERM_UTF;
            term.print_state.mb_count = mb_size;
            term.print_state.mb_current = 0;
        } else {
            term.buffer[term.cursor_pos].ch[0] = *str;
            term.buffer[term.cursor_pos].ch[1] = 0;
            term.buffer[term.cursor_pos].fg_color = term.cursor_fg_color;
            term.buffer[term.cursor_pos].bg_color = term.cursor_bg_color;
            ADVANCE_CHAR;

            term.cursor_pos++;
            if (term.cursor_pos >= term.char_w * term.char_h) {
                term.cursor_pos = term.char_w * term.char_h - term.char_w;
                term_scroll_down();
            }
        }
    }
}

static void term_clear_forward(int pos) {
    for (int i = pos; i < term.char_w * term.char_h; i++) {
        strncpy(term.buffer[i].ch, " ", ARRLEN(term.buffer[i].ch));
        term.buffer[i].fg_color = TERM_WHITE;
        term.buffer[i].bg_color = term.clear_color;
    }
}

static void term_clear_backward(int pos) {
    for (int i = pos; i >= 0; i--) {
        strncpy(term.buffer[i].ch, " ", ARRLEN(term.buffer[i].ch));
        term.buffer[i].fg_color = TERM_WHITE;
        term.buffer[i].bg_color = term.clear_color;
    }
}

void term_clear(void) {
    for (int i = 0; i < term.char_w * term.char_h; i++) {
        strncpy(term.buffer[i].ch, " ", ARRLEN(term.buffer[i].ch));
        term.buffer[i].fg_color = TERM_WHITE;
        term.buffer[i].bg_color = term.clear_color;
    }
    term.cursor_pos = 0;
}

void term_resize(float screen_w, float screen_h) {
    term.size = (TermVec) { screen_w, screen_h };

    term.char_size = term.measure_text(term.font, "A", 1, term.font_size);
    TermVec new_buffer_size = { term.size.x / term.char_size.x, term.size.y / term.char_size.y };
    int new_char_w = (int)new_buffer_size.x,
        new_char_h = (int)new_buffer_size.y;

    if (term.char_w != new_char_w || term.char_h != new_char_h) {
        if (term.master_fd != -1) {
            struct winsize w = {
                .ws_col = new_char_w,
                .ws_row = new_char_h,
            };

            if (ioctl(term.master_fd, TIOCSWINSZ, &w) == -1) {
                scrap_log(LOG_ERROR, "ioctl: %s", strerror(errno));
            }
        }

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
}
