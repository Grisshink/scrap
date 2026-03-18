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

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#include "vec.h"
#include "std.h"
#include "util.h"
#include "thread.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif

// Explicitly including rprand.h here as this translation unit will soon need
// to compile as standalone library, which means we should not depend on
// raylib in any way
#define RPRAND_IMPLEMENTATION
#define RPRANDAPI static __attribute__ ((unused))
#include "../external/rprand.h"

// NOTE: Shamelessly stolen from raylib codebase ;)
// Get next codepoint in a UTF-8 encoded text, scanning until '\0' is found
// When an invalid UTF-8 byte is encountered we exit as soon as possible and a '?'(0x3f) codepoint is returned
// Total number of bytes processed are returned as a parameter
// NOTE: The standard says U+FFFD should be returned in case of errors
// but that character is not supported by the default font in raylib
static int get_codepoint(const char *text, int *codepoint_size) {
/*
    UTF-8 specs from https://www.ietf.org/rfc/rfc3629.txt

    Char. number range  |        UTF-8 octet sequence
      (hexadecimal)    |              (binary)
    --------------------+---------------------------------------------
    0000 0000-0000 007F | 0xxxxxxx
    0000 0080-0000 07FF | 110xxxxx 10xxxxxx
    0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
    0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
*/
    // NOTE: on decode errors we return as soon as possible

    int codepoint = 0x3f;   // Codepoint (defaults to '?')
    int octet = (unsigned char)(text[0]); // The first UTF8 octet
    *codepoint_size = 1;

    if (octet <= 0x7f) {
        // Only one octet (ASCII range x00-7F)
        codepoint = text[0];
    } else if ((octet & 0xe0) == 0xc0) {
        // Two octets

        // [0]xC2-DF    [1]UTF8-tail(x80-BF)
        unsigned char octet1 = text[1];

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) {
            // Unexpected sequence
            *codepoint_size = 2;
            return codepoint;
        }

        if ((octet >= 0xc2) && (octet <= 0xdf)) {
            codepoint = ((octet & 0x1f) << 6) | (octet1 & 0x3f);
            *codepoint_size = 2;
        }
    } else if ((octet & 0xf0) == 0xe0) {
        // Three octets
        unsigned char octet1 = text[1];
        unsigned char octet2 = '\0';

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) {
            // Unexpected sequence
            *codepoint_size = 2;
            return codepoint;
        }

        octet2 = text[2];

        if ((octet2 == '\0') || ((octet2 >> 6) != 2)) {
            // Unexpected sequence
            *codepoint_size = 3;
            return codepoint;
        }

        // [0]xE0    [1]xA0-BF       [2]UTF8-tail(x80-BF)
        // [0]xE1-EC [1]UTF8-tail    [2]UTF8-tail(x80-BF)
        // [0]xED    [1]x80-9F       [2]UTF8-tail(x80-BF)
        // [0]xEE-EF [1]UTF8-tail    [2]UTF8-tail(x80-BF)

        if (((octet == 0xe0) && !((octet1 >= 0xa0) && (octet1 <= 0xbf))) ||
            ((octet == 0xed) && !((octet1 >= 0x80) && (octet1 <= 0x9f)))) {
            *codepoint_size = 2;
            return codepoint;
        }

        if ((octet >= 0xe0) && (octet <= 0xef)) {
            codepoint = ((octet & 0xf) << 12) | ((octet1 & 0x3f) << 6) | (octet2 & 0x3f);
            *codepoint_size = 3;
        }
    } else if ((octet & 0xf8) == 0xf0) {
        // Four octets
        if (octet > 0xf4) return codepoint;

        unsigned char octet1 = text[1];
        unsigned char octet2 = '\0';
        unsigned char octet3 = '\0';

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) {
            // Unexpected sequence
            *codepoint_size = 2;
            return codepoint;
        }

        octet2 = text[2];

        if ((octet2 == '\0') || ((octet2 >> 6) != 2)) {
            // Unexpected sequence
            *codepoint_size = 3;
            return codepoint;
        }

        octet3 = text[3];

        if ((octet3 == '\0') || ((octet3 >> 6) != 2)) {
            // Unexpected sequence
            *codepoint_size = 4;
            return codepoint;
        }

        // [0]xF0       [1]x90-BF       [2]UTF8-tail  [3]UTF8-tail
        // [0]xF1-F3    [1]UTF8-tail    [2]UTF8-tail  [3]UTF8-tail
        // [0]xF4       [1]x80-8F       [2]UTF8-tail  [3]UTF8-tail

        if (((octet == 0xf0) && !((octet1 >= 0x90) && (octet1 <= 0xbf))) ||
            ((octet == 0xf4) && !((octet1 >= 0x80) && (octet1 <= 0x8f)))) {
            // Unexpected sequence
            *codepoint_size = 2;
            return codepoint;
        }

        if (octet >= 0xf0) {
            codepoint = ((octet & 0x7) << 18) | ((octet1 & 0x3f) << 12) | ((octet2 & 0x3f) << 6) | (octet3 & 0x3f);
            *codepoint_size = 4;
        }
    }

    if (codepoint > 0x10ffff) codepoint = 0x3f;     // Codepoints after U+10ffff are invalid

    return codepoint;
}

// NOTE: Shamelessly stolen from raylib codebase ;)
// Encode codepoint into utf8 text (char array length returned as parameter)
// NOTE: It uses a static array to store UTF-8 bytes
const char *codepoint_to_utf8(int codepoint, int *utf8_size) {
    static char utf8[6] = { 0 };
    int size = 0;   // Byte size of codepoint

    if (codepoint <= 0x7f) {
        utf8[0] = (char)codepoint;
        size = 1;
    } else if (codepoint <= 0x7ff) {
        utf8[0] = (char)(((codepoint >> 6) & 0x1f) | 0xc0);
        utf8[1] = (char)((codepoint & 0x3f) | 0x80);
        size = 2;
    } else if (codepoint <= 0xffff) {
        utf8[0] = (char)(((codepoint >> 12) & 0x0f) | 0xe0);
        utf8[1] = (char)(((codepoint >>  6) & 0x3f) | 0x80);
        utf8[2] = (char)((codepoint & 0x3f) | 0x80);
        size = 3;
    } else if (codepoint <= 0x10ffff) {
        utf8[0] = (char)(((codepoint >> 18) & 0x07) | 0xf0);
        utf8[1] = (char)(((codepoint >> 12) & 0x3f) | 0x80);
        utf8[2] = (char)(((codepoint >>  6) & 0x3f) | 0x80);
        utf8[3] = (char)((codepoint & 0x3f) | 0x80);
        size = 4;
    }

    *utf8_size = size;

    return utf8;
}

static int leading_ones(unsigned char byte) {
    int out = 0;
    while (byte & 0x80) {
        out++;
        byte <<= 1;
    }
    return out;
}

#define STD_MATH_FUNC(_f) bool std_##_f(IrExec* exec) { \
    exec_push_float(exec, _f(exec_pop_float(exec))); \
    return true; \
}

STD_MATH_FUNC(sqrt)
STD_MATH_FUNC(round)
STD_MATH_FUNC(floor)
STD_MATH_FUNC(ceil)
STD_MATH_FUNC(sin)
STD_MATH_FUNC(cos)
STD_MATH_FUNC(tan)
STD_MATH_FUNC(asin)
STD_MATH_FUNC(acos)
STD_MATH_FUNC(atan)

#undef STD_MATH_FUNC

bool std_color_to_string(IrExec* exec) {
    int64_t color_int = exec_pop_int(exec);
    StdColor color = *(StdColor*)&color_int;

    char str[32];
    snprintf(str, 32, "#%02x%02x%02x%02x", color.r, color.g, color.b, color.a);
    if (!exec_push_string(exec, str)) return false;
    return true;
}

bool std_string_to_color(IrExec* exec) {
    char str_buf[32];
    exec_pop_string(exec, str_buf, 32);

    char* str = str_buf;

    if (*str == '#') str++;
    unsigned char r = 0x00, g = 0x00, b = 0x00, a = 0xff;
    sscanf(str, "%02hhx%02hhx%02hhx%02hhx", &r, &g, &b, &a);
    StdColor color = { r, g, b, a };
    int32_t color_int = *(int32_t*)&color;
    exec_push_int(exec, color_int);
    return true;
}

bool std_sleep(IrExec* exec) {
    double secs = exec_pop_float(exec);

    if (secs < 0) return true;
#ifdef _WIN32
    Sleep(secs * 1000);
#else
    struct timespec sleep_time = {0};
    sleep_time.tv_sec = secs;
    sleep_time.tv_nsec = fmod(secs, 1.0) * 1e9;

    if (nanosleep(&sleep_time, &sleep_time) == -1) return false;
#endif
    return true;
}

bool std_unix_time(IrExec* exec) {
    time_t t = time(NULL);
    if (t == (time_t)-1) return false;
    exec_push_int(exec, t);
    return true;
}

bool std_random_float(IrExec* exec) {
    double max = exec_pop_float(exec);
    double min = exec_pop_float(exec);
    if (min > max) {
        double temp = max;
        max = min;
        min = temp;
    }

    double val = (double)rprand_xoshiro() / (double)UINT32_MAX;
    exec_push_float(exec, val * (max - min) + min);
    return true;
}

bool std_random_int(IrExec* exec) {
    int64_t max = exec_pop_int(exec);
    int64_t min = exec_pop_int(exec);

    if (min > max) {
        exec_push_int(exec, rprand_get_value(max, min));
    } else {
        exec_push_int(exec, rprand_get_value(min, max));
    }
    return true;
}

bool std_thread_handle_stopping_state(IrExec* exec) {
    Thread* thread = (void*)exec_pop_int(exec);
    thread_handle_stopping_state(thread);
    return true;
}

bool std_string_join(IrExec* exec) {
    IrList* right = exec_pop_list_string(exec);
    IrList* left  = exec_pop_list_string(exec);
    IR_ASSERT(left  != NULL);
    IR_ASSERT(right != NULL);

    // Push lists back to avoid them being freed by gc
    exec_push_list_string(exec, left);
    exec_push_list_string(exec, right);

    IrList* new_list = exec_list_new(exec);
    exec_push_list_string(exec, new_list);

    new_list->capacity = left->size + right->size;

    void* items = exec_malloc(exec, new_list->capacity * sizeof(*new_list->items));
    if (!items) return false;

    new_list = exec_pop_list_string(exec);
    right = exec_pop_list_string(exec);
    left  = exec_pop_list_string(exec);

    new_list->items = items;

    memcpy(new_list->items, left->items, left->size * sizeof(*left->items));
    memcpy(new_list->items + left->size, right->items, right->size * sizeof(*right->items));
    new_list->size = left->size + right->size;
    exec_push_list_string(exec, new_list);

    return true;
}

bool std_string_substring(IrExec* exec) {
    IrList* str = exec_pop_list_string(exec);
    int64_t end = exec_pop_int(exec);
    int64_t start = exec_pop_int(exec);

    start = MAX(start, 1);
    end = MIN(end, (int64_t)str->size);
    if (start > end || start > (int64_t)str->size || end < 1) {
        IrList* new_list = exec_list_new(exec);
        memset(new_list, 0, sizeof(IrList));
        exec_push_list_string(exec, new_list);
        return true;
    }

    exec_push_list_string(exec, str);

    IrList* new_list = exec_list_new(exec);
    exec_push_list_string(exec, new_list);

    new_list->capacity = end - start + 1;

    void* items = exec_malloc(exec, new_list->capacity * sizeof(*new_list->items));
    if (!items) return false;

    new_list = exec_pop_list_string(exec);
    str = exec_pop_list_string(exec);

    new_list->items = items;

    memcpy(new_list->items, str->items + start - 1, (end - start + 1) * sizeof(*str->items));
    exec_push_list_string(exec, new_list);
    new_list->size = new_list->capacity;

    return true;
}

bool std_gc_collect(IrExec* exec) {
    exec_collect(exec);
    return true;
}

#if 1 //def STANDALONE_STD

#include <wchar.h>

static int cursor_x = 0;
static int cursor_y = 0;
static StdColor clear_color = {0};
static StdColor bg_color = {0};

void test_cancel(void) {}

bool std_term_print_str(IrExec* exec) {
    IrList* list = exec_pop_list_string(exec);
    if (!list) return false;
    for (size_t i = 0; i < list->size; i++) {
        IrValue c = list->items[i];
        switch (c.type) {
        case IR_TYPE_INT: printf("%lc", (wint_t)c.as.int_val); break;
        case IR_TYPE_BYTE: printf("%c", c.as.byte_val); break;
        default: printf("?"); break;
        }
    }
    fflush(stdout);
    return true;
}

bool std_term_println_str(IrExec* exec) {
    if (!std_term_print_str(exec)) return false;
    printf("\n");
    return true;
}

bool std_term_set_fg_color(IrExec* exec) {
    int32_t color_val = exec_pop_int(exec);
    StdColor color = *(StdColor*)&color_val;
    // ESC[38;2;⟨r⟩;⟨g⟩;⟨b⟩m Select RGB foreground color
    printf("\033[38;2;%d;%d;%dm", color.r, color.g, color.b);
    fflush(stdout);
    return true;
}

bool std_term_set_bg_color(IrExec* exec) {
    int32_t color_val = exec_pop_int(exec);
    StdColor color = *(StdColor*)&color_val;
    // ESC[48;2;⟨r⟩;⟨g⟩;⟨b⟩m Select RGB background color
    printf("\033[48;2;%d;%d;%dm", color.r, color.g, color.b);
    bg_color = color;
    fflush(stdout);
    return true;
}

bool std_term_set_clear_color(IrExec* exec) {
    int32_t color_val = exec_pop_int(exec);
    clear_color = *(StdColor*)&color_val;
    return true;
}

bool std_term_cursor_max_y(IrExec* exec) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) return false;
    exec_push_int(exec, csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
#else
    struct winsize w;
    if (ioctl(0, TIOCGWINSZ, &w)) return false;
    exec_push_int(exec, w.ws_row);
#endif
    return true;
}

bool std_term_cursor_max_x(IrExec* exec) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) return false;
    exec_push_int(exec, csbi.srWindow.Right - csbi.srWindow.Left + 1);
#else
    struct winsize w;
    if (ioctl(0, TIOCGWINSZ, &w)) return false;
    exec_push_int(exec, w.ws_col);
#endif
    return true;
}

bool std_term_cursor_x(IrExec* exec) {
    exec_push_int(exec, cursor_x);
    return true;
}

bool std_term_cursor_y(IrExec* exec) {
    exec_push_int(exec, cursor_y);
    return true;
}

bool std_term_clear(IrExec* exec) {
    (void) exec;
    // ESC[48;2;⟨r⟩;⟨g⟩;⟨b⟩m Select RGB background color
    printf("\033[48;2;%d;%d;%dm", clear_color.r, clear_color.g, clear_color.b);
    printf("\033[2J");
    // ESC[48;2;⟨r⟩;⟨g⟩;⟨b⟩m Select RGB background color
    printf("\033[48;2;%d;%d;%dm", bg_color.r, bg_color.g, bg_color.b);
    fflush(stdout);
    return true;
}

bool std_term_set_cursor(IrExec* exec) {
    int64_t y = exec_pop_int(exec);
    int64_t x = exec_pop_int(exec);

    cursor_x = x;
    cursor_y = y;
    printf("\033[%ld;%ldH", y + 1, x + 1);
    fflush(stdout);
    return true;
}

bool std_term_get_char(IrExec* exec) {
    char input[10];
    input[0] = (char)getchar();

    int mb_size = leading_ones(input[0]);

    if (mb_size == 0) mb_size = 1;
    for (int i = 1; i < mb_size && i < 10; i++) input[i] = (char)getchar();
    input[mb_size] = 0;

    exec_push_int(exec, get_codepoint(input, &mb_size));
    return true;
}

bool std_term_get_input(IrExec* exec) {
    char* string_buf = vector_create();
    char last_char = 0;
    char buf[256];

    while (last_char != '\n') {
        if (!fgets(buf, 256, stdin)) {
            exec_set_error(exec, "Error getting input: %s", strerror(errno));
            vector_free(string_buf);
            return false;
        }

        int size = strlen(buf);
        last_char = buf[size - 1];
        if (last_char == '\n') buf[--size] = 0;

        for (char* str = buf; *str; str++) vector_add(&string_buf, *str);
    }
    vector_add(&string_buf, 0);

    IrList* list = exec_list_new(exec);
    exec_push_list_string(exec, list);

    size_t char_count = 0;
    int mb_size = 0;
    for (char* str = string_buf; *str; str += mb_size) {
        get_codepoint(str, &mb_size);
        char_count++;
    }

    list->size = char_count;
    list->capacity = char_count;

    IrValue* items = exec_malloc(exec, list->capacity * sizeof(*list->items));
    list->items = items;

    char_count = 0;
    for (char* str = string_buf; *str; str += mb_size) {
        list->items[char_count] = (IrValue) {
            .type = IR_TYPE_INT,
            .as.int_val = get_codepoint(str, &mb_size),
        };
        char_count++;
    }

    vector_free(string_buf);

    return true;
}

#else

bool std_term_print_str(IrExec* exec) {
    IrList* list = exec_pop_list_string(exec);
    if (!list) return false;
    char buf[64];
    int buf_size = 0;

    char* char_buf;
    int char_size = 0;

    for (size_t i = 0; i < list->size; i++) {
        IrValue c = list->items[i];
        switch (c.type) {
        case IR_TYPE_INT:  char_buf = (char*)codepoint_to_utf8(c.as.int_val, &char_size); break;
        case IR_TYPE_BYTE: char_buf = (char*)codepoint_to_utf8(c.as.byte_val, &char_size); break;
        default:           char_buf = (char*)codepoint_to_utf8('?', &char_size); break;
        }
        char_buf[char_size] = 0;

        if (buf_size + char_size + 1 > 64) {
            buf[buf_size] = 0;
            term_print_str(buf);
            buf_size = 0;
        }

        strcpy(buf + buf_size, char_buf);
        buf_size += char_size;
    }

    if (buf_size > 0) {
        buf[buf_size] = 0;
        term_print_str(buf);
    }

    return true;
}

bool std_term_println_str(IrExec* exec) {
    if (!std_term_print_str(exec)) return false;
    term_print_str("\n");
    return true;
}

bool std_term_set_fg_color(IrExec* exec) {
    int32_t color_val = exec_pop_int(exec);
    term_set_fg_color(*(TermColor*)&color_val);
    return true;
}

bool std_term_set_bg_color(IrExec* exec) {
    int32_t color_val = exec_pop_int(exec);
    term_set_bg_color(*(TermColor*)&color_val);
    return true;
}

bool std_term_set_clear_color(IrExec* exec) {
    int32_t color_val = exec_pop_int(exec);
    term_set_clear_color(*(TermColor*)&color_val);
    return true;
}

bool std_term_clear(IrExec* exec) {
    (void) exec;
    term_clear();
    return true;
}

bool std_term_get_char(IrExec* exec) {
    char input[10];
    input[0] = term_input_get_char();
    int mb_size = leading_ones(input[0]);

    if (mb_size == 0) mb_size = 1;
    for (int i = 1; i < mb_size && i < 10; i++) input[i] = term_input_get_char();
    input[mb_size] = 0;

    exec_push_int(exec, get_codepoint(input, &mb_size));
    return true;
}

bool std_term_get_input(IrExec* exec) {
    char input_char = 0;
    char* string_buf = vector_create();

    while (input_char != '\n') {
        char input[256];
        int i = 0;
        for (; i < 255 && input_char != '\n'; i++) input[i] = (input_char = term_input_get_char());
        if (input[i - 1] == '\n') input[i - 1] = 0;
        input[i] = 0;

        for (char* str = input; *str; str++) vector_add(&string_buf, *str);
    }
    vector_add(&string_buf, 0);

    IrList* list = exec_list_new(exec);
    exec_push_list_string(exec, list);

    size_t char_count = 0;
    int mb_size = 0;
    for (char* str = string_buf; *str; str += mb_size) {
        get_codepoint(str, &mb_size);
        char_count++;
    }

    list->size = char_count;
    list->capacity = char_count;

    IrValue* items = exec_malloc(exec, list->capacity * sizeof(*list->items));
    list->items = items;

    char_count = 0;
    for (char* str = string_buf; *str; str += mb_size) {
        list->items[char_count] = (IrValue) {
            .type = IR_TYPE_INT,
            .as.int_val = get_codepoint(str, &mb_size),
        };
        char_count++;
    }

    vector_free(string_buf);

    return true;
}

bool std_term_set_cursor(IrExec* exec) {
    int64_t y = exec_pop_int(exec);
    int64_t x = exec_pop_int(exec);

    mutex_lock(&term.lock);
    x = CLAMP(x, 0, term.char_w - 1);
    y = CLAMP(y, 0, term.char_h - 1);
    term.cursor_pos = x + y * term.char_w;
    mutex_unlock(&term.lock);
    return true;
}

bool std_term_cursor_x(IrExec* exec) {
    mutex_lock(&term.lock);
    int cur_x = 0;
    if (term.char_w != 0) cur_x = term.cursor_pos % term.char_w;
    mutex_unlock(&term.lock);
    exec_push_int(exec, cur_x);
    return true;
}

bool std_term_cursor_y(IrExec* exec) {
    mutex_lock(&term.lock);
    int cur_y = 0;
    if (term.char_w != 0) cur_y = term.cursor_pos / term.char_w;
    mutex_unlock(&term.lock);
    exec_push_int(exec, cur_y);
    return true;
}

bool std_term_cursor_max_x(IrExec* exec) {
    mutex_lock(&term.lock);
    int cur_max_x = term.char_w;
    mutex_unlock(&term.lock);
    exec_push_int(exec, cur_max_x);
    return true;
}

bool std_term_cursor_max_y(IrExec* exec) {
    mutex_lock(&term.lock);
    int cur_max_y = term.char_h;
    mutex_unlock(&term.lock);
    exec_push_int(exec, cur_max_y);
    return true;
}

#endif // STANDALONE_STD

#define STD_FUNC(_f) { #_f, _f }
IrRunFunction std_resolve_function(IrExec* exec, const char* hint) {
    (void) exec;

    struct {
        char* name;
        IrRunFunction func;
    } funcs[] = {
        STD_FUNC(std_random_int),
        STD_FUNC(std_random_float),
        STD_FUNC(std_sleep),
        STD_FUNC(std_unix_time),
        STD_FUNC(std_term_print_str),
        STD_FUNC(std_term_println_str),
        STD_FUNC(std_term_get_input),
        STD_FUNC(std_term_get_char),
        STD_FUNC(std_term_set_cursor),
        STD_FUNC(std_term_cursor_x),
        STD_FUNC(std_term_cursor_y),
        STD_FUNC(std_term_cursor_max_x),
        STD_FUNC(std_term_cursor_max_y),
        STD_FUNC(std_term_set_fg_color),
        STD_FUNC(std_term_set_bg_color),
        STD_FUNC(std_term_set_clear_color),
        STD_FUNC(std_term_clear),
        STD_FUNC(std_thread_handle_stopping_state),
        STD_FUNC(std_color_to_string),
        STD_FUNC(std_string_to_color),
        STD_FUNC(std_string_join),
        STD_FUNC(std_string_substring),
        STD_FUNC(std_gc_collect),
        { "sqrt",  std_sqrt  },
        { "round", std_round },
        { "floor", std_floor },
        { "ceil",  std_ceil  },
        { "sin",   std_sin   },
        { "cos",   std_cos   },
        { "tan",   std_tan   },
        { "asin",  std_asin  },
        { "acos",  std_acos  },
        { "atan",  std_atan  },
        { NULL,    NULL      },
    };

    for (size_t i = 0; funcs[i].name != NULL; i++) {
        if (!strcmp(hint, funcs[i].name)) return funcs[i].func;
    }

    return NULL;
}
