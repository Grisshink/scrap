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

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "std.h"

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

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

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

int std_int_pow(int base, int exp) {
    if (exp == 0) return 1;

    int result = 1;
    while (exp) {
        if (exp & 1) result *= base;
        exp >>= 1;
        base *= base;
    }
    return result;
}

List* std_list_new(Gc* gc) {
    List* list = gc_malloc(gc, sizeof(List), DATA_TYPE_LIST);
    list->size = 0;
    list->capacity = 0;
    list->values = NULL;
    return list;
}

static AnyValue std_get_any(DataType data_type, va_list va) {
    AnyValueData data;

    switch (data_type) {
    case DATA_TYPE_BOOL:
    case DATA_TYPE_INTEGER:
        data.integer_val = va_arg(va, int);
        break;
    case DATA_TYPE_FLOAT:
        data.float_val = va_arg(va, double);
        break;
    case DATA_TYPE_STRING:
        data.str_val = va_arg(va, StringHeader*);
        break;
    case DATA_TYPE_LITERAL:
        data.literal_val = va_arg(va, char*);
        break;
    case DATA_TYPE_LIST:
        data.list_val = va_arg(va, List*);
        break;
    case DATA_TYPE_ANY:
        return *va_arg(va, AnyValue*);
    default:
        break;
    }

    return (AnyValue) {
        .type = data_type,
        .data = data,
    };
}

void std_list_add(Gc* gc, List* list, DataType data_type, ...) {
    AnyValue any;
    
    va_list va;
    va_start(va, data_type);
    any = std_get_any(data_type, va);
    va_end(va);

    std_list_add_any(gc, list, any);
}

void std_list_add_any(Gc* gc, List* list, AnyValue any) {
    if (!list->values) {
        list->values = gc_malloc(gc, sizeof(AnyValue), 0);
        list->capacity = 1;
    }

    if (list->size >= list->capacity) {
        AnyValue* new_list = gc_malloc(gc, sizeof(AnyValue) * list->size * 2, 0);
        memcpy(new_list, list->values, sizeof(AnyValue) * list->size);
        list->values = new_list;
        list->capacity = list->size * 2;
    }

    list->values[list->size++] = any;
}

void std_list_set(List* list, int index, DataType data_type, ...) {
    if (index >= list->size || index < 0) return;

    AnyValue any;

    va_list va;
    va_start(va, data_type);
    any = std_get_any(data_type, va);
    va_end(va);

    list->values[index] = any;
}

AnyValue* std_list_get(Gc* gc, List* list, int index) {
    AnyValue* out = gc_malloc(gc, sizeof(AnyValue), DATA_TYPE_ANY);
    *out = (AnyValue) { .type = DATA_TYPE_NOTHING };

    if (index >= list->size || index < 0) return out;

    *out = list->values[index];
    return out;
}

int std_list_length(List* list) {
    return list->size;
}

AnyValue* std_any_from_value(Gc* gc, DataType data_type, ...) {
    AnyValue any;

    va_list va;
    va_start(va, data_type);
    if (data_type == DATA_TYPE_ANY) {
        return va_arg(va, AnyValue*);
    } else {
        any = std_get_any(data_type, va);
    }
    va_end(va);

    AnyValue* value = gc_malloc(gc, sizeof(AnyValue), DATA_TYPE_ANY);
    *value = any;
    return value;
}

StringHeader* std_string_from_literal(Gc* gc, const char* literal, unsigned int size) {
    StringHeader* out_str = gc_malloc(gc, sizeof(StringHeader) + size + 1, DATA_TYPE_STRING); // Don't forget null terminator. It is not included in size
    memcpy(out_str->str, literal, size);
    out_str->size = size;
    out_str->capacity = size;
    out_str->str[size] = 0;
    return out_str;
}

char* std_string_get_data(StringHeader* str) {
    return str->str;
}

StringHeader* std_string_letter_in(Gc* gc, int target, StringHeader* input_str) {
    int pos = 0;
    if (target <= 0) return std_string_from_literal(gc, "", 0);
    for (char* str = input_str->str; *str; str++) {
        // Increment pos only on the beginning of multibyte char
        if ((*str & 0x80) == 0 || (*str & 0x40) != 0) pos++;

        if (pos == target) {
            int codepoint_size;
            get_codepoint(str, &codepoint_size);
            return std_string_from_literal(gc, str, codepoint_size);
        }
    }

    return std_string_from_literal(gc, "", 0);
}

StringHeader* std_string_substring(Gc* gc, int begin, int end, StringHeader* input_str) {
    if (begin <= 0) begin = 1;
    if (end <= 0) return std_string_from_literal(gc, "", 0);
    if (begin > end) return std_string_from_literal(gc, "", 0);

    char* substr_start = NULL;
    int substr_len = 0;

    int pos = 0;
    for (char* str = input_str->str; *str; str++) {
        // Increment pos only on the beginning of multibyte char
        if ((*str & 0x80) == 0 || (*str & 0x40) != 0) pos++;
        if (substr_start) substr_len++;

        if (pos == begin && !substr_start) {
            substr_start = str;
            substr_len = 1;
        }
        if (pos == end) {
            if (!substr_start) return std_string_from_literal(gc, "", 0);
            int codepoint_size;
            get_codepoint(str, &codepoint_size);
            substr_len += codepoint_size - 1;

            return std_string_from_literal(gc, substr_start, substr_len);
        }
    }

    if (substr_start) return std_string_from_literal(gc, substr_start, substr_len);
    return std_string_from_literal(gc, "", 0);
}

StringHeader* std_string_join(Gc* gc, StringHeader* left, StringHeader* right) {
    StringHeader* out_str = gc_malloc(gc, sizeof(StringHeader) + left->size + right->size + 1, DATA_TYPE_STRING);
    memcpy(out_str->str, left->str, left->size);
    memcpy(out_str->str + left->size, right->str, right->size);
    out_str->size = left->size + right->size;
    out_str->capacity = out_str->size;
    out_str->str[out_str->size] = 0;
    return out_str;
}

int std_string_length(StringHeader* str) {
    int len = 0;
    char* cur = str->str;
    while (*cur) {
        int mb_size = leading_ones(*cur);
        if (mb_size == 0) mb_size = 1;
        cur += mb_size;
        len++;
    }
    return len;
}

bool std_string_is_eq(StringHeader* left, StringHeader* right) {
    if (left->size != right->size) return false;
    for (unsigned int i = 0; i < left->size; i++) {
        if (left->str[i] != right->str[i]) return false;
    }
    return true;
}

StringHeader* std_string_chr(Gc* gc, int value) {
    int text_size;
    const char* text = codepoint_to_utf8(value, &text_size);
    return std_string_from_literal(gc, text, text_size);
}

int std_string_ord(StringHeader* str) {
    int codepoint_size;
    int codepoint = get_codepoint(str->str, &codepoint_size);
    (void) codepoint_size;
    return codepoint;
}

StringHeader* std_string_from_integer(Gc* gc, int value) {
    char str[20];
    unsigned int len = snprintf(str, 20, "%d", value);
    return std_string_from_literal(gc, str, len);
}

StringHeader* std_string_from_bool(Gc* gc, bool value) {
    return value ? std_string_from_literal(gc, "true", 4) : std_string_from_literal(gc, "false", 5);
}

StringHeader* std_string_from_float(Gc* gc, double value) {
    char str[20];
    unsigned int len = snprintf(str, 20, "%f", value);
    return std_string_from_literal(gc, str, len);
}

StringHeader* std_string_from_any(Gc* gc, AnyValue* value) {
    if (!value) return std_string_from_literal(gc, "", 0);

    switch (value->type) {
    case DATA_TYPE_INTEGER:
        return std_string_from_integer(gc, value->data.integer_val);
    case DATA_TYPE_FLOAT:
        return std_string_from_float(gc, value->data.float_val);
    case DATA_TYPE_LITERAL:
        return std_string_from_literal(gc, value->data.literal_val, strlen(value->data.literal_val));
    case DATA_TYPE_STRING:
        return value->data.str_val;
    case DATA_TYPE_BOOL:
        return std_string_from_bool(gc, value->data.integer_val);
    case DATA_TYPE_LIST: ;
        char str[32];
        int size = snprintf(str, 32, "*LIST (%lu)*", value->data.list_val->size);
        return std_string_from_literal(gc, str, size);
    default:
        return std_string_from_literal(gc, "", 0);
    }
}

int std_integer_from_any(AnyValue* value) {
    if (!value) return 0;

    switch (value->type) {
    case DATA_TYPE_BOOL:
    case DATA_TYPE_INTEGER:
        return value->data.integer_val;
    case DATA_TYPE_FLOAT:
        return (int)value->data.float_val;
    case DATA_TYPE_STRING:
        return atoi(value->data.str_val->str);
    case DATA_TYPE_LITERAL:
        return atoi(value->data.literal_val);
    default:
        return 0;
    }
}

double std_float_from_any(AnyValue* value) {
    if (!value) return 0;

    switch (value->type) {
    case DATA_TYPE_BOOL:
    case DATA_TYPE_INTEGER:
        return (double)value->data.integer_val;
    case DATA_TYPE_FLOAT:
        return value->data.float_val;
    case DATA_TYPE_STRING:
        return atof(value->data.str_val->str);
    case DATA_TYPE_LITERAL:
        return atof(value->data.literal_val);
    default:
        return 0;
    }
}

int std_bool_from_any(AnyValue* value) {
    if (!value) return 0;

    switch (value->type) {
    case DATA_TYPE_BOOL:
    case DATA_TYPE_INTEGER:
        return value->data.integer_val != 0;
    case DATA_TYPE_FLOAT:
        return value->data.float_val != 0.0;
    case DATA_TYPE_STRING:
        return value->data.str_val->size > 0;
    case DATA_TYPE_LITERAL:
        return *value->data.literal_val != 0;
    default:
        return 0;
    }
}

List* std_list_from_any(Gc* gc, AnyValue* value) {
    if (!value) return 0;

    switch (value->type) {
    case DATA_TYPE_LIST:
        return value->data.list_val;
    default:
        return std_list_new(gc);
    }
}

bool std_any_is_eq(AnyValue* left, AnyValue* right) {
    if (left->type != right->type) return false;

    switch (left->type) {
    case DATA_TYPE_NOTHING:
        return true;
    case DATA_TYPE_LITERAL:
        return !strcmp(left->data.literal_val, right->data.literal_val);
    case DATA_TYPE_STRING:
        return std_string_is_eq(left->data.str_val, right->data.str_val);
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_BOOL:
        return left->data.integer_val == right->data.integer_val;
    case DATA_TYPE_FLOAT:
        return left->data.float_val == right->data.float_val;
    case DATA_TYPE_LIST:
        return left->data.list_val == right->data.list_val;
    default:
        return false;
    }
}

int std_sleep(int usecs) {
    if (usecs < 0) return 0;
#ifdef _WIN32
    Sleep(usecs / 1000);
#else
    struct timespec sleep_time = {0};
    sleep_time.tv_sec = usecs / 1000000;
    sleep_time.tv_nsec = (usecs % 1000000) * 1000;

    if (nanosleep(&sleep_time, &sleep_time) == -1) return 0;
#endif
    return usecs;
}

int std_get_random(int min, int max) {
    if (min > max) {
        return rprand_get_value(max, min);
    } else {
        return rprand_get_value(min, max);
    }
}

int std_term_print_any(AnyValue* any) {
    if (!any) return 0;

    switch (any->type) {
    case DATA_TYPE_STRING:
        return std_term_print_str(any->data.str_val->str);
    case DATA_TYPE_LITERAL:
        return std_term_print_str(any->data.literal_val);
    case DATA_TYPE_NOTHING:
        return 0;
    case DATA_TYPE_INTEGER:
        return std_term_print_integer(any->data.integer_val);
    case DATA_TYPE_BOOL:
        return std_term_print_bool(any->data.integer_val);
    case DATA_TYPE_FLOAT:
        return std_term_print_float(any->data.float_val);
    case DATA_TYPE_LIST:
        return std_term_print_list(any->data.list_val);
    default:
        return 0;
    }
}

#ifdef STANDALONE_STD

static int cursor_x = 0;
static int cursor_y = 0;
static Color clear_color = {0};
static Color bg_color = {0};

void test_cancel(void) {}

int std_term_print_str(const char* str) {
    int len = printf("%s", str);
    fflush(stdout);
    return len;
}

int std_term_print_integer(int value) {
    int len = printf("%d", value);
    fflush(stdout);
    return len;
}

int std_term_print_float(double value) {
    int len = printf("%f", value);
    fflush(stdout);
    return len;
}

int std_term_print_bool(bool value) {
    int len = printf("%s", value ? "true" : "false");
    fflush(stdout);
    return len;
}

void std_term_set_fg_color(Color color) {
    // ESC[38;2;⟨r⟩;⟨g⟩;⟨b⟩m Select RGB foreground color
    printf("\033[38;2;%d;%d;%dm", color.r, color.g, color.b);
    fflush(stdout);
}

void std_term_set_bg_color(Color color) {
    // ESC[48;2;⟨r⟩;⟨g⟩;⟨b⟩m Select RGB background color
    printf("\033[48;2;%d;%d;%dm", color.r, color.g, color.b);
    bg_color = color;
    fflush(stdout);
}

void std_term_set_clear_color(Color color) {
    clear_color = color;
}

int std_term_cursor_max_y(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) return 0;
    return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
    struct winsize w;
    if (ioctl(0, TIOCGWINSZ, &w)) return 0;
    return w.ws_row;
#endif
}

int std_term_cursor_max_x(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) return 0;
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
    struct winsize w;
    if (ioctl(0, TIOCGWINSZ, &w)) return 0;
    return w.ws_col;
#endif
}

int std_term_cursor_x(void) {
    return cursor_x;
}

int std_term_cursor_y(void) {
    return cursor_y;
}

void std_term_clear(void) {
    // ESC[48;2;⟨r⟩;⟨g⟩;⟨b⟩m Select RGB background color
    printf("\033[48;2;%d;%d;%dm", clear_color.r, clear_color.g, clear_color.b);
    printf("\033[2J");
    // ESC[48;2;⟨r⟩;⟨g⟩;⟨b⟩m Select RGB background color
    printf("\033[48;2;%d;%d;%dm", bg_color.r, bg_color.g, bg_color.b);
    fflush(stdout);
}

void std_term_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    printf("\033[%d;%dH", y + 1, x + 1);
    fflush(stdout);
}

StringHeader* std_term_get_char(Gc* gc) {
    char input[10];
    input[0] = (char)getchar();
    if (input[0] == '\n') return std_string_from_literal(gc, "", 0);

    int mb_size = leading_ones(input[0]);

    if (mb_size == 0) mb_size = 1;
    for (int i = 1; i < mb_size && i < 10; i++) input[i] = (char)getchar();
    input[mb_size] = 0;

    return std_string_from_literal(gc, input, mb_size);
}

StringHeader* std_term_get_input(Gc* gc) {
    StringHeader* out_string = NULL;
    char last_char = 0;
    char buf[256];
    
    while (last_char != '\n') {
        if (!fgets(buf, 256, stdin)) return std_string_from_literal(gc, "", 0);
        int size = strlen(buf);
        last_char = buf[size - 1];
        if (last_char == '\n') buf[--size] = 0;

        if (!out_string) {
            out_string = std_string_from_literal(gc, buf, size);
        } else {
            out_string = std_string_join(gc, out_string, std_string_from_literal(gc, buf, size));
        }
    }

    return out_string;
}

int std_term_print_list(List* list) {
    int len = printf("*LIST (%lu)*", list->size);
    fflush(stdout);
    return len;
}

#else

int std_term_print_str(const char* str) {
    return term_print_str(str);
}

int std_term_print_integer(int value) {
    return term_print_integer(value);
}

int std_term_print_float(double value) {
    return term_print_float(value);
}

int std_term_print_bool(bool value) {
    return term_print_bool(value);
}

void std_term_set_fg_color(TermColor color) {
    return term_set_fg_color(color);
}

void std_term_set_bg_color(TermColor color) {
    return term_set_bg_color(color);
}

void std_term_set_clear_color(TermColor color) {
    return term_set_clear_color(color);
}

void std_term_clear(void) {
    term_clear();
}

StringHeader* std_term_get_char(Gc* gc) {
    char input[10];
    input[0] = term_input_get_char();
    int mb_size = leading_ones(input[0]);

    if (mb_size == 0) mb_size = 1;
    for (int i = 1; i < mb_size && i < 10; i++) input[i] = term_input_get_char();
    input[mb_size] = 0;

    return std_string_from_literal(gc, input, mb_size);
}

void std_term_set_cursor(int x, int y) {
    pthread_mutex_lock(&term.lock);
    x = CLAMP(x, 0, term.char_w - 1);
    y = CLAMP(y, 0, term.char_h - 1);
    term.cursor_pos = x + y * term.char_w;
    pthread_mutex_unlock(&term.lock);
}

int std_term_cursor_x(void) {
    pthread_mutex_lock(&term.lock);
    int cur_x = 0;
    if (term.char_w != 0) cur_x = term.cursor_pos % term.char_w;
    pthread_mutex_unlock(&term.lock);
    return cur_x;
}

int std_term_cursor_y(void) {
    pthread_mutex_lock(&term.lock);
    int cur_y = 0;
    if (term.char_w != 0) cur_y = term.cursor_pos / term.char_w;
    pthread_mutex_unlock(&term.lock);
    return cur_y;
}

int std_term_cursor_max_x(void) {
    pthread_mutex_lock(&term.lock);
    int cur_max_x = term.char_w;
    pthread_mutex_unlock(&term.lock);
    return cur_max_x;
}

int std_term_cursor_max_y(void) {
    pthread_mutex_lock(&term.lock);
    int cur_max_y = term.char_h;
    pthread_mutex_unlock(&term.lock);
    return cur_max_y;
}

StringHeader* std_term_get_input(Gc* gc) {
    char input_char = 0;
    StringHeader* out_string = NULL;

    while (input_char != '\n') {
        char input[256];
        int i = 0;
        for (; i < 255 && input_char != '\n'; i++) input[i] = (input_char = term_input_get_char());
        if (input[i - 1] == '\n') input[i - 1] = 0;
        input[i] = 0;

        if (!out_string) {
            out_string = std_string_from_literal(gc, input, i - 1);
        } else {
            out_string = std_string_join(gc, out_string, std_string_from_literal(gc, input, i - 1));
        }
    }

    return out_string;
}

int std_term_print_list(List* list) {
    char converted[32];
    snprintf(converted, 32, "*LIST (%zu)*", list->size);
    return term_print_str(converted);
}

#endif
