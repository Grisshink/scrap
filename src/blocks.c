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
#include "vec.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <libintl.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))
#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define MATH_LIST_LEN 10
#define TERM_COLOR_LIST_LEN 8

typedef struct {
    char* str;
    size_t len;
    size_t cap;
} String;

char* block_math_list[MATH_LIST_LEN] = {
    "sqrt", "round", "floor", "ceil",
    "sin", "cos", "tan",
    "asin", "acos", "atan",
};

char* term_color_list[TERM_COLOR_LIST_LEN] = {
    "black", "red", "yellow", "green", "blue", "purple", "cyan", "white",
};

char** math_list_access(Block* block, size_t* list_len) {
    (void) block;
    *list_len = MATH_LIST_LEN;
    return block_math_list;
}

char** term_color_list_access(Block* block, size_t* list_len) {
    (void) block;
    *list_len = TERM_COLOR_LIST_LEN;
    return term_color_list;
}

String string_new(size_t cap) {
    String string;
    string.str = malloc((cap + 1)* sizeof(char));
    *string.str = 0;
    string.len = 0;
    string.cap = cap;
    return string;
}

void string_add(String* string, const char* other) {
    size_t new_len = string->len + strlen(other);
    if (new_len > string->cap) {
        string->str = realloc(string->str, (new_len + 1) * sizeof(char));
        string->cap = new_len;
    }
    strcat(string->str, other);
    string->len = new_len;
}

void string_add_array(String* string, const char* arr, int arr_len) {
    size_t new_len = string->len + arr_len;
    if (new_len > string->cap) {
        string->str = realloc(string->str, (new_len + 1) * sizeof(char));
        string->cap = new_len;
    }
    memcpy(&string->str[string->len], arr, arr_len);
    string->str[new_len] = 0;
    string->len = new_len;
}

void string_free(String string) {
    free(string.str);
}

#ifdef USE_INTERPRETER

Data string_make_managed(String* string) {
    Data out;
    out.type = DATA_STR;
    out.storage.type = DATA_STORAGE_MANAGED;
    out.storage.storage_len = string->len + 1;
    out.data.str_arg = string->str;
    return out;
}

Data block_noop(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_NOTHING;
}

Data block_loop(Exec* exec, int argc, Data* argv) {
    if (argc < 1) RETURN_OMIT_ARGS;
    if (argv[0].type != DATA_CONTROL) RETURN_OMIT_ARGS;

    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        control_stack_pop_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
    }

    RETURN_OMIT_ARGS;
}

Data block_if(Exec* exec, int argc, Data* argv) {
    if (argc < 1) RETURN_BOOL(1);
    if (argv[0].type != DATA_CONTROL) RETURN_BOOL(1);
    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (!data_to_bool(argv[1])) {
            exec_set_skip_block(exec);
            control_stack_push_data((int)0, int)
        } else {
            control_stack_push_data((int)1, int)
        }
        RETURN_OMIT_ARGS;
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        int is_success = 0;
        control_stack_pop_data(is_success, int)
        RETURN_BOOL(is_success);
    }
    RETURN_BOOL(1);
}

Data block_else_if(Exec* exec, int argc, Data* argv) {
    if (argc < 1) RETURN_BOOL(1);
    if (argv[0].type != DATA_CONTROL) RETURN_BOOL(1);
    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (argc < 3 || data_to_bool(argv[1])) {
            exec_set_skip_block(exec);
            control_stack_push_data((int)1, int)
        } else {
            int condition = data_to_bool(argv[2]);
            if (!condition) exec_set_skip_block(exec);
            control_stack_push_data(condition, int)
        }
        RETURN_OMIT_ARGS;
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        int is_success = 0;
        control_stack_pop_data(is_success, int)
        RETURN_BOOL(is_success);
    }
    RETURN_BOOL(1);
}

Data block_else(Exec* exec, int argc, Data* argv) {
    if (argc < 1) RETURN_BOOL(1);
    if (argv[0].type != DATA_CONTROL) RETURN_BOOL(1);
    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (argc < 2 || data_to_bool(argv[1])) {
            exec_set_skip_block(exec);
        }
        RETURN_OMIT_ARGS;
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        RETURN_BOOL(1);
    }
    RETURN_BOOL(1);
}

// Visualization of control stack (stack grows downwards):
// - loop block index
// - cycles left to loop
// - 1 <- indicator for end block to do looping
//
// If the loop should not loop then the stack will look like this:
// - 0 <- indicator for end block that it should stop immediately
Data block_repeat(Exec* exec, int argc, Data* argv) {
    if (argc < 1) RETURN_OMIT_ARGS;
    if (argv[0].type != DATA_CONTROL) RETURN_OMIT_ARGS;

    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        int cycles = data_to_int(argv[1]);
        if (cycles <= 0) {
            exec_set_skip_block(exec);
            control_stack_push_data((int)0, int) // This indicates the end block that it should NOT loop
            RETURN_OMIT_ARGS;
        }
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
        control_stack_push_data(cycles - 1, int)
        control_stack_push_data((int)1, int) // This indicates the end block that it should loop
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        int should_loop = 0;
        control_stack_pop_data(should_loop, int)
        if (!should_loop) RETURN_BOOL(0);

        int left = -1;
        control_stack_pop_data(left, int)
        if (left <= 0) {
            size_t bin;
            control_stack_pop_data(bin, size_t)
            (void) bin; // Cleanup stack
            RETURN_BOOL(1);
        }

        control_stack_pop_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
        control_stack_push_data(left - 1, int)
        control_stack_push_data((int)1, int)
    }

    RETURN_OMIT_ARGS;
}

Data block_while(Exec* exec, int argc, Data* argv) {
    if (argc < 2) RETURN_BOOL(0);
    if (argv[0].type != DATA_CONTROL) RETURN_BOOL(0);

    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (!data_to_bool(argv[1])) {
            exec_set_skip_block(exec);
            RETURN_OMIT_ARGS;
        }
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        if (!data_to_bool(argv[1])) {
            size_t bin;
            control_stack_pop_data(bin, size_t)
            (void) bin; 
            RETURN_BOOL(1);
        }

        control_stack_pop_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
    }

    RETURN_NOTHING;
}

Data block_sleep(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    int usecs = data_to_int(argv[0]);
    if (usecs < 0) RETURN_INT(0);

    struct timespec sleep_time = {0};
    sleep_time.tv_sec = usecs / 1000000;
    sleep_time.tv_nsec = (usecs % 1000000) * 1000;

    if (nanosleep(&sleep_time, &sleep_time) == -1) RETURN_INT(0);
    RETURN_INT(usecs);
}

Data block_declare_var(Exec* exec, int argc, Data* argv) {
    if (argc < 2) RETURN_NOTHING;
    if (argv[0].type != DATA_STR || argv[0].storage.type != DATA_STORAGE_STATIC) RETURN_NOTHING;

    Data var_value = data_copy(argv[1]);
    if (var_value.storage.type == DATA_STORAGE_MANAGED) var_value.storage.type = DATA_STORAGE_UNMANAGED;

    variable_stack_push_var(exec, argv[0].data.str_arg, var_value);
    return var_value;
}

Data block_get_var(Exec* exec, int argc, Data* argv) {
    if (argc < 1) RETURN_NOTHING;
    Variable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    return var->value;
}

Data block_set_var(Exec* exec, int argc, Data* argv) {
    if (argc < 2) RETURN_NOTHING;

    Variable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;

    Data new_value = data_copy(argv[1]);
    if (new_value.storage.type == DATA_STORAGE_MANAGED) new_value.storage.type = DATA_STORAGE_UNMANAGED;

    if (var->value.storage.type == DATA_STORAGE_UNMANAGED) {
        data_free(var->value);
    }

    var->value = new_value;
    return var->value;
}

Data block_create_list(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argc;
    (void) argv;

    Data out;
    out.type = DATA_LIST;
    out.storage.type = DATA_STORAGE_MANAGED;
    out.storage.storage_len = 0;
    out.data.list_arg.items = NULL;
    out.data.list_arg.len = 0;
    return out;
}

Data block_list_add(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;

    Variable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    if (var->value.type != DATA_LIST) RETURN_NOTHING;

    if (!var->value.data.list_arg.items) {
        var->value.data.list_arg.items = malloc(sizeof(Data));
        var->value.data.list_arg.len = 1;
    } else {
        var->value.data.list_arg.items = realloc(var->value.data.list_arg.items, ++var->value.data.list_arg.len * sizeof(Data));
    }
    var->value.storage.storage_len = var->value.data.list_arg.len * sizeof(Data);
    Data* list_item = &var->value.data.list_arg.items[var->value.data.list_arg.len - 1];
    if (argv[1].storage.type == DATA_STORAGE_MANAGED) {
        argv[1].storage.type = DATA_STORAGE_UNMANAGED;
        *list_item = argv[1];
    } else {
        *list_item = data_copy(argv[1]);
        if (list_item->storage.type == DATA_STORAGE_MANAGED) list_item->storage.type = DATA_STORAGE_UNMANAGED;
    }

    return *list_item;
}

Data block_list_get(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;

    Variable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    if (var->value.type != DATA_LIST) RETURN_NOTHING;
    if (!var->value.data.list_arg.items || var->value.data.list_arg.len == 0) RETURN_NOTHING;
    int index = data_to_int(argv[1]);
    if (index < 0 || (size_t)index >= var->value.data.list_arg.len) RETURN_NOTHING;

    return var->value.data.list_arg.items[index];
}

Data block_list_set(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 3) RETURN_NOTHING;

    Variable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    if (var->value.type != DATA_LIST) RETURN_NOTHING;
    if (!var->value.data.list_arg.items || var->value.data.list_arg.len == 0) RETURN_NOTHING;
    int index = data_to_int(argv[1]);
    if (index < 0 || (size_t)index >= var->value.data.list_arg.len) RETURN_NOTHING;

    Data new_value = data_copy(argv[2]);
    if (new_value.storage.type == DATA_STORAGE_MANAGED) new_value.storage.type = DATA_STORAGE_UNMANAGED;

    if (var->value.data.list_arg.items[index].storage.type == DATA_STORAGE_UNMANAGED) {
        data_free(var->value.data.list_arg.items[index]);
    }
    var->value.data.list_arg.items[index] = new_value;
    return var->value.data.list_arg.items[index];
}

Data block_print(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc >= 1) {
        int bytes_sent = 0;
        switch (argv[0].type) {
        case DATA_INT:
            bytes_sent = term_print_int(argv[0].data.int_arg);
            break;
        case DATA_BOOL:
            bytes_sent = term_print_str(argv[0].data.int_arg ? "true" : "false");
            break;
        case DATA_STR:
            bytes_sent = term_print_str(argv[0].data.str_arg);
            break;
        case DATA_DOUBLE:
            bytes_sent = term_print_double(argv[0].data.double_arg);
            break;
        case DATA_LIST:
            bytes_sent += term_print_str("[");
            if (argv[0].data.list_arg.items && argv[0].data.list_arg.len) {
                for (size_t i = 0; i < argv[0].data.list_arg.len; i++) {
                    bytes_sent += block_print(exec, 1, &argv[0].data.list_arg.items[i]).data.int_arg;
                    bytes_sent += term_print_str(", ");
                }
            }
            bytes_sent += term_print_str("]");
            break;
        default:
            break;
        }
        RETURN_INT(bytes_sent);
    }
    RETURN_INT(0);
}

Data block_println(Exec* exec, int argc, Data* argv) {
    Data out = block_print(exec, argc, argv);
    term_print_str("\r\n");
    return out;
}

Data block_cursor_x(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&term.lock);
    int cur_x = 0;
    if (term.char_w != 0) cur_x = term.cursor_pos % term.char_w;
    pthread_mutex_unlock(&term.lock);
    RETURN_INT(cur_x);
}

Data block_cursor_y(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&term.lock);
    int cur_y = 0;
    if (term.char_w != 0) cur_y = term.cursor_pos / term.char_w;
    pthread_mutex_unlock(&term.lock);
    RETURN_INT(cur_y);
}

Data block_cursor_max_x(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&term.lock);
    int cur_max_x = term.char_w;
    pthread_mutex_unlock(&term.lock);
    RETURN_INT(cur_max_x);
}

Data block_cursor_max_y(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&term.lock);
    int cur_max_y = term.char_h;
    pthread_mutex_unlock(&term.lock);
    RETURN_INT(cur_max_y);
}

Data block_set_cursor(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;
    pthread_mutex_lock(&term.lock);
    int x = CLAMP(data_to_int(argv[0]), 0, term.char_w - 1);
    int y = CLAMP(data_to_int(argv[1]), 0, term.char_h - 1);
    term.cursor_pos = x + y * term.char_w;
    pthread_mutex_unlock(&term.lock);
    RETURN_NOTHING;
}

Data block_set_fg_color(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_NOTHING;
    if (argv[0].type != DATA_STR) RETURN_NOTHING;

    if (!strcmp(argv[0].data.str_arg, "black")) {
        term_set_fg_color(BLACK);
    } else if (!strcmp(argv[0].data.str_arg, "red")) {
        term_set_fg_color(RED);
    } else if (!strcmp(argv[0].data.str_arg, "yellow")) {
        term_set_fg_color(YELLOW);
    } else if (!strcmp(argv[0].data.str_arg, "green")) {
        term_set_fg_color(GREEN);
    } else if (!strcmp(argv[0].data.str_arg, "blue")) {
        term_set_fg_color(BLUE);
    } else if (!strcmp(argv[0].data.str_arg, "purple")) {
        term_set_fg_color(PURPLE);
    } else if (!strcmp(argv[0].data.str_arg, "cyan")) {
        term_set_fg_color((Color) { 0x00, 0xff, 0xff, 0xff});
    } else if (!strcmp(argv[0].data.str_arg, "white")) {
        term_set_fg_color(WHITE);
    }

    RETURN_NOTHING;
}

Data block_set_bg_color(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_NOTHING;
    if (argv[0].type != DATA_STR) RETURN_NOTHING;

    if (!strcmp(argv[0].data.str_arg, "black")) {
        term_set_bg_color(BLACK);
    } else if (!strcmp(argv[0].data.str_arg, "red")) {
        term_set_bg_color(RED);
    } else if (!strcmp(argv[0].data.str_arg, "yellow")) {
        term_set_bg_color(YELLOW);
    } else if (!strcmp(argv[0].data.str_arg, "green")) {
        term_set_bg_color(GREEN);
    } else if (!strcmp(argv[0].data.str_arg, "blue")) {
        term_set_bg_color(BLUE);
    } else if (!strcmp(argv[0].data.str_arg, "purple")) {
        term_set_bg_color(PURPLE);
    } else if (!strcmp(argv[0].data.str_arg, "cyan")) {
        term_set_bg_color((Color) { 0x00, 0xff, 0xff, 0xff});
    } else if (!strcmp(argv[0].data.str_arg, "white")) {
        term_set_bg_color(WHITE);
    }

    RETURN_NOTHING;
}

Data block_reset_color(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    term_set_fg_color(WHITE);
    term_set_bg_color(BLACK);
    RETURN_NOTHING;
}

Data block_term_clear(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    term_clear();
    RETURN_NOTHING;
}

Data block_term_set_clear(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_NOTHING;
    if (argv[0].type != DATA_STR) RETURN_NOTHING;

    if (!strcmp(argv[0].data.str_arg, "black")) {
        term_set_clear_color(BLACK);
    } else if (!strcmp(argv[0].data.str_arg, "red")) {
        term_set_clear_color(RED);
    } else if (!strcmp(argv[0].data.str_arg, "yellow")) {
        term_set_clear_color(YELLOW);
    } else if (!strcmp(argv[0].data.str_arg, "green")) {
        term_set_clear_color(GREEN);
    } else if (!strcmp(argv[0].data.str_arg, "blue")) {
        term_set_clear_color(BLUE);
    } else if (!strcmp(argv[0].data.str_arg, "purple")) {
        term_set_clear_color(PURPLE);
    } else if (!strcmp(argv[0].data.str_arg, "cyan")) {
        term_set_clear_color((Color) { 0x00, 0xff, 0xff, 0xff});
    } else if (!strcmp(argv[0].data.str_arg, "white")) {
        term_set_clear_color(WHITE);
    }

    RETURN_NOTHING;
}

Data block_input(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argv;
    (void) argc;

    String string = string_new(0);
    char input_char = 0;

    while (input_char != '\n') {
        char input[256];
        int i = 0;
        for (; i < 255 && input_char != '\n'; i++) input[i] = (input_char = term_input_get_char());
        if (input[i - 1] == '\n') input[i - 1] = 0;
        input[i] = 0;
        string_add(&string, input);
    }

    return string_make_managed(&string);
}

Data block_get_char(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argv;
    (void) argc;

    String string = string_new(0);
    char input[10];
    input[0] = term_input_get_char();
    int mb_size = leading_ones(input[0]);
    if (mb_size == 0) mb_size = 1;
    for (int i = 1; i < mb_size && i < 10; i++) input[i] = term_input_get_char();
    input[mb_size] = 0;
    string_add(&string, input);

    return string_make_managed(&string);
}

Data block_random(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    int min = data_to_int(argv[0]);
    int max = data_to_int(argv[1]);
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    int val = GetRandomValue(min, max);
    RETURN_INT(val);
}

Data block_join(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;

    String string = string_new(0);
    string_add(&string, data_to_str(argv[0]));
    string_add(&string, data_to_str(argv[1]));
    return string_make_managed(&string);
}

Data block_ord(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);

    const char* str = data_to_str(argv[0]);
    int codepoint_size;
    int codepoint = GetCodepoint(str, &codepoint_size);

    RETURN_INT(codepoint);
}

Data block_chr(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_NOTHING;

    int text_size;
    const char* text = CodepointToUTF8(data_to_int(argv[0]), &text_size);

    String string = string_new(0);
    string_add_array(&string, text, text_size);
    return string_make_managed(&string);
}

Data block_letter_in(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;

    int pos = 0;
    int target = data_to_int(argv[0]);
    if (target <= 0) RETURN_NOTHING;
    for (char* str = (char*)data_to_str(argv[1]); *str; str++) {
        // Increment pos only on the beginning of multibyte char
        if ((*str & 0x80) == 0 || (*str & 0x40) != 0) pos++;

        if (pos == target) {
            int codepoint_size;
            GetCodepoint(str, &codepoint_size);

            String string = string_new(0);
            string_add_array(&string, str, codepoint_size);
            return string_make_managed(&string);
        }
    }

    RETURN_NOTHING;
}

Data block_substring(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 3) RETURN_NOTHING;

    int start_pos = data_to_int(argv[0]);
    int end_pos = data_to_int(argv[1]);
    if (start_pos <= 0) start_pos = 1;
    if (end_pos <= 0) RETURN_NOTHING;
    if (start_pos > end_pos) RETURN_NOTHING;

    char* substr_start = NULL;
    int substr_len = 0;

    int pos = 0;
    for (char* str = (char*)data_to_str(argv[2]); *str; str++) {
        // Increment pos only on the beginning of multibyte char
        if ((*str & 0x80) == 0 || (*str & 0x40) != 0) pos++;
        if (substr_start) substr_len++;

        if (pos == start_pos && !substr_start) {
            substr_start = str;
            substr_len = 1;
        }
        if (pos == end_pos) {
            if (!substr_start) RETURN_NOTHING;
            int codepoint_size;
            GetCodepoint(str, &codepoint_size);
            substr_len += codepoint_size - 1;

            String string = string_new(0);
            string_add_array(&string, substr_start, substr_len);
            return string_make_managed(&string);
        }
    }

    if (substr_start) {
        String string = string_new(0);
        string_add_array(&string, substr_start, substr_len);
        return string_make_managed(&string);
    }

    RETURN_NOTHING;
}

Data block_length(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    if (argv[0].type == DATA_LIST) RETURN_INT(argv[0].data.list_arg.len);
    int len = 0;
    const char* str = data_to_str(argv[0]);
    while (*str) {
        int mb_size = leading_ones(*str);
        if (mb_size == 0) mb_size = 1;
        str += mb_size;
        len++;
    }
    RETURN_INT(len);
}

Data block_unix_time(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_INT(time(NULL));
}

Data block_convert_int(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    RETURN_INT(data_to_int(argv[0]));
}

Data block_convert_float(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_DOUBLE(0.0);
    RETURN_DOUBLE(data_to_double(argv[0]));
}

Data block_convert_str(Exec* exec, int argc, Data* argv) {
    (void) exec;
    String string = string_new(0);
    if (argc < 1) return string_make_managed(&string);
    string_add(&string, data_to_str(argv[0]));
    return string_make_managed(&string);
}

Data block_convert_bool(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    RETURN_BOOL(data_to_bool(argv[0]));
}

Data block_plus(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg + data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) + data_to_int(argv[1]));
    }
}

Data block_minus(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg - data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) - data_to_int(argv[1]));
    }
}

Data block_mult(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg * data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) * data_to_int(argv[1]));
    }
}

Data block_div(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg / data_to_double(argv[1]));
    } else {
        int divisor = data_to_int(argv[1]);
        if (divisor == 0) {
            term_print_str("VM ERROR: Division by zero!");
            TraceLog(LOG_ERROR, "[VM] Division by zero");
            PTHREAD_FAIL(exec);
        }
        RETURN_INT(data_to_int(argv[0]) / divisor);
    }
}

Data block_pow(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) RETURN_DOUBLE(pow(argv[0].data.double_arg, data_to_double(argv[1])));

    int base = data_to_int(argv[0]);
    unsigned int exp = data_to_int(argv[1]);
    if (!exp) RETURN_INT(1);

    int result = 1;
    while (exp) {
        if (exp & 1) result *= base;
        exp >>= 1;
        base *= base;
    }
    RETURN_INT(result);
}

Data block_math(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_DOUBLE(0.0);
    if (argv[0].type != DATA_STR) RETURN_DOUBLE(0.0);

    if (!strcmp(argv[0].data.str_arg, "sin")) {
        RETURN_DOUBLE(sin(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "cos")) {
        RETURN_DOUBLE(cos(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "tan")) {
        RETURN_DOUBLE(tan(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "asin")) {
        RETURN_DOUBLE(asin(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "acos")) {
        RETURN_DOUBLE(acos(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "atan")) {
        RETURN_DOUBLE(atan(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "sqrt")) {
        RETURN_DOUBLE(sqrt(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "round")) {
        RETURN_DOUBLE(round(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "floor")) {
        RETURN_DOUBLE(floor(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "ceil")) {
        RETURN_DOUBLE(ceil(data_to_double(argv[1])));
    } else {
        RETURN_DOUBLE(0.0);
    }
}

Data block_pi(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_DOUBLE(M_PI);
}

Data block_bit_not(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(~0);
    RETURN_INT(~data_to_int(argv[0]));
}

Data block_bit_and(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    RETURN_INT(data_to_int(argv[0]) & data_to_int(argv[1]));
}

Data block_bit_xor(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    if (argc < 2) RETURN_INT(data_to_int(argv[0]));
    RETURN_INT(data_to_int(argv[0]) ^ data_to_int(argv[1]));
}

Data block_bit_or(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    if (argc < 2) RETURN_INT(data_to_int(argv[0]));
    RETURN_INT(data_to_int(argv[0]) | data_to_int(argv[1]));
}

Data block_rem(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(fmod(argv[0].data.double_arg, data_to_double(argv[1])));
    } else {
        RETURN_INT(data_to_int(argv[0]) % data_to_int(argv[1]));
    }
}

Data block_less(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) < 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg < data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) < data_to_int(argv[1]));
    }
}

Data block_less_eq(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) <= 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg <= data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) <= data_to_int(argv[1]));
    }
}

Data block_more(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) > 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg > data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) > data_to_int(argv[1]));
    }
}

Data block_more_eq(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) >= 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg >= data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) >= data_to_int(argv[1]));
    }
}

Data block_not(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(1);
    RETURN_BOOL(!data_to_bool(argv[0]));
}

Data block_and(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_BOOL(0);
    RETURN_BOOL(data_to_bool(argv[0]) && data_to_bool(argv[1]));
}

Data block_or(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_BOOL(0);
    RETURN_BOOL(data_to_bool(argv[0]) || data_to_bool(argv[1]));
}

Data block_true(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_BOOL(1);
}

Data block_false(Exec* exec, int argc, Data* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_BOOL(0);
}

Data block_eq(Exec* exec, int argc, Data* argv) {
    (void) exec;
    if (argc < 2) RETURN_BOOL(0);
    if (argv[0].type != argv[1].type) RETURN_BOOL(0);

    switch (argv[0].type) {
    case DATA_BOOL:
    case DATA_INT:
        RETURN_BOOL(argv[0].data.int_arg == argv[1].data.int_arg);
    case DATA_DOUBLE:
        RETURN_BOOL(argv[0].data.double_arg == argv[1].data.double_arg);
    case DATA_STR:
        RETURN_BOOL(!strcmp(argv[0].data.str_arg, argv[1].data.str_arg));
    case DATA_NOTHING:
        RETURN_BOOL(1);
    default:
        RETURN_BOOL(0);
    }
}

Data block_not_eq(Exec* exec, int argc, Data* argv) {
    Data out = block_eq(exec, argc, argv);
    out.data.int_arg = !out.data.int_arg;
    return out;
}

Data block_exec_custom(Exec* exec, int argc, Data* argv) {
    if (argc < 1) RETURN_NOTHING;
    if (argv[0].type != DATA_CHAIN) RETURN_NOTHING;
    Data return_val;
    exec_run_chain(exec, argv[0].data.chain_arg, argc - 1, argv + 1, &return_val);
    return return_val;
}

// Checks the arguments and returns the value from custom_argv at index if all conditions are met
Data block_custom_arg(Exec* exec, int argc, Data* argv) {
    if (argc < 1) RETURN_NOTHING;
    if (argv[0].type != DATA_INT) RETURN_NOTHING;
    if (argv[0].data.int_arg >= exec->chain_stack[exec->chain_stack_len - 1].custom_argc) RETURN_NOTHING;
    return data_copy(exec->chain_stack[exec->chain_stack_len - 1].custom_argv[argv[0].data.int_arg]);
}

// Modifies the internal state of the current code chain so that it returns early with the data written to .return_arg
Data block_return(Exec* exec, int argc, Data* argv) {
    if (argc < 1) RETURN_NOTHING;
    exec->chain_stack[exec->chain_stack_len - 1].return_arg = data_copy(argv[0]);
    exec->chain_stack[exec->chain_stack_len - 1].is_returning = true;
    RETURN_NOTHING;
}

#else

#define MIN_ARG_COUNT(count) \
    if (argc < count) { \
        TraceLog(LOG_ERROR, "[LLVM] Not enough arguments! Expected: %d or more, Got: %d", count, argc); \
        return false; \
    }

#define INTEGER(val) LLVMConstInt(LLVMInt32Type(), val, true)
#define BOOLEAN(val) LLVMConstInt(LLVMInt1Type(), val, false)
#define DOUBLE(val) LLVMConstReal(LLVMDoubleType(), val)

LLVMValueRef arg_to_bool(Exec* exec, FuncArg arg) {
    if (arg.type == FUNC_ARG_STRING) return BOOLEAN(*arg.data.str != 0);

    LLVMTypeRef type = LLVMTypeOf(arg.data.value);
    LLVMTypeKind type_kind = LLVMGetTypeKind(type);
    unsigned int width;

    switch (type_kind) {
    case LLVMIntegerTypeKind:
        width = LLVMGetIntTypeWidth(type);
        if (width == 1) return arg.data.value;
        if (width != 32) {
            TraceLog(LOG_WARNING, "Non 32-bit int conversion (%u) to bool", width);
            return BOOLEAN(0);
        }
        return LLVMBuildICmp(exec->builder, LLVMIntNE, arg.data.value, INTEGER(0), "bool_cast");
    case LLVMDoubleTypeKind:
        return LLVMBuildFCmp(exec->builder, LLVMRealONE, arg.data.value, DOUBLE(0.0), "bool_cast");
    default:
        TraceLog(LOG_ERROR, "Unknown type: %d", type_kind);
        return BOOLEAN(0);
    }
}

LLVMValueRef arg_to_int(Exec* exec, FuncArg arg) {
    if (arg.type == FUNC_ARG_STRING) return INTEGER(atoi(arg.data.str));

    LLVMTypeKind type_kind = LLVMGetTypeKind(LLVMTypeOf(arg.data.value));
    switch (type_kind) {
    case LLVMIntegerTypeKind:
        return arg.data.value;
    case LLVMDoubleTypeKind:
        return LLVMBuildFPToSI(exec->builder, arg.data.value, LLVMInt32Type(), "int_cast");
    default:
        TraceLog(LOG_ERROR, "Unknown type: %d", type_kind);
        return INTEGER(0);
    }
}

LLVMValueRef arg_to_double(Exec* exec, FuncArg arg) {
    if (arg.type == FUNC_ARG_STRING) return DOUBLE(atof(arg.data.str));

    LLVMTypeKind type_kind = LLVMGetTypeKind(LLVMTypeOf(arg.data.value));
    switch (type_kind) {
    case LLVMIntegerTypeKind:
        return LLVMBuildSIToFP(exec->builder, arg.data.value, LLVMDoubleType(), "double_cast");
    case LLVMDoubleTypeKind:
        return arg.data.value;
    default:
        TraceLog(LOG_ERROR, "Unknown type: %d", type_kind);
        return DOUBLE(0.0);
    }
}

static LLVMValueRef call_print(Exec* exec, LLVMValueRef str) {
    LLVMValueRef print_func = LLVMGetNamedFunction(exec->module, "term_print_str");
    LLVMTypeRef print_func_type = LLVMGlobalGetValueType(print_func);
    return LLVMBuildCall2(exec->builder, print_func_type, print_func, &str, 1, "print");
}

static LLVMValueRef call_print_int(Exec* exec, LLVMValueRef int_val) {
    LLVMValueRef print_func = LLVMGetNamedFunction(exec->module, "term_print_int");
    LLVMTypeRef print_func_type = LLVMGlobalGetValueType(print_func);
    return LLVMBuildCall2(exec->builder, print_func_type, print_func, &int_val, 1, "print");
}

static LLVMValueRef call_print_bool(Exec* exec, LLVMValueRef bool_val) {
    LLVMValueRef print_func = LLVMGetNamedFunction(exec->module, "term_print_bool");
    LLVMTypeRef print_func_type = LLVMGlobalGetValueType(print_func);
    return LLVMBuildCall2(exec->builder, print_func_type, print_func, &bool_val, 1, "print");
}

static LLVMValueRef call_print_double(Exec* exec, LLVMValueRef int_val) {
    LLVMValueRef print_func = LLVMGetNamedFunction(exec->module, "term_print_double");
    LLVMTypeRef print_func_type = LLVMGlobalGetValueType(print_func);
    return LLVMBuildCall2(exec->builder, print_func_type, print_func, &int_val, 1, "print");
}

bool block_return(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_return");
    return false;
}

bool block_custom_arg(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_custom_arg");
    return false;
}

bool block_exec_custom(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_exec_custom");
    return false;
}

bool block_not_eq(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING && argv[1].type == FUNC_ARG_STRING) {
        *return_val = BOOLEAN(!!strcmp(argv[0].data.str, argv[1].data.str));
        return true;
    }

    if (argv[0].type == FUNC_ARG_STRING) {
        LLVMTypeRef type = LLVMTypeOf(argv[1].data.value);
        switch (LLVMGetTypeKind(type)) {
        case LLVMIntegerTypeKind: ;
            unsigned int width = LLVMGetIntTypeWidth(type);
            if (width == 1) {
                *return_val = LLVMBuildICmp(exec->builder, LLVMIntNE, arg_to_bool(exec, argv[0]), argv[1].data.value, "bool_eq");
            } else {
                *return_val = LLVMBuildICmp(exec->builder, LLVMIntNE, arg_to_int(exec, argv[0]), argv[1].data.value, "int_eq");
            }
            break;
        case LLVMDoubleTypeKind:
            *return_val = LLVMBuildFCmp(exec->builder, LLVMRealONE, arg_to_double(exec, argv[0]), argv[1].data.value, "double_eq");
            break;
        case LLVMVoidTypeKind:
            *return_val = BOOLEAN(1);
            break;
        default:
            TraceLog(LOG_ERROR, "Unknown compare types!");
            return false;
        }
        return true;
    } else if (argv[1].type == FUNC_ARG_STRING) {
        LLVMTypeRef type = LLVMTypeOf(argv[0].data.value);
        switch (LLVMGetTypeKind(type)) {
        case LLVMIntegerTypeKind: ;
            unsigned int width = LLVMGetIntTypeWidth(type);
            if (width == 1) {
                *return_val = LLVMBuildICmp(exec->builder, LLVMIntNE, argv[0].data.value, arg_to_bool(exec, argv[1]), "bool_eq");
            } else {
                *return_val = LLVMBuildICmp(exec->builder, LLVMIntNE, argv[0].data.value, arg_to_int(exec, argv[1]), "int_eq");
            }
            break;
        case LLVMDoubleTypeKind:
            *return_val = LLVMBuildFCmp(exec->builder, LLVMRealONE, argv[0].data.value, arg_to_double(exec, argv[1]), "double_eq");
            break;
        case LLVMVoidTypeKind:
            *return_val = BOOLEAN(1);
            break;
        default:
            TraceLog(LOG_ERROR, "Unknown compare types!");
            return false;
        }
        return true;
    }

    LLVMTypeRef left_type = LLVMTypeOf(argv[0].data.value);
    LLVMTypeRef right_type = LLVMTypeOf(argv[1].data.value);
    if (LLVMGetTypeKind(left_type) != LLVMGetTypeKind(right_type)) {
        *return_val = BOOLEAN(1);
        return true;
    }

    switch (LLVMGetTypeKind(LLVMTypeOf(argv[0].data.value))) {
    case LLVMIntegerTypeKind: ;
        unsigned int left_width = LLVMGetIntTypeWidth(left_type);
        unsigned int right_width = LLVMGetIntTypeWidth(right_type);
        if (left_width != right_width) {
            *return_val = BOOLEAN(1);
            return true;
        }
        if (left_width == 1) {
            *return_val = LLVMBuildICmp(exec->builder, LLVMIntNE, argv[0].data.value, argv[1].data.value, "bool_eq");
        } else {
            *return_val = LLVMBuildICmp(exec->builder, LLVMIntNE, argv[0].data.value, argv[1].data.value, "int_eq");
        }
        break;
    case LLVMDoubleTypeKind:
        *return_val = LLVMBuildFCmp(exec->builder, LLVMRealONE, argv[0].data.value, argv[1].data.value, "double_eq");
        break;
    case LLVMVoidTypeKind:
        *return_val = BOOLEAN(0);
        break;
    default:
        TraceLog(LOG_ERROR, "Unknown compare types!");
        return false;
    }

    return true;
}

bool block_eq(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING && argv[1].type == FUNC_ARG_STRING) {
        *return_val = BOOLEAN(!strcmp(argv[0].data.str, argv[1].data.str));
        return true;
    }

    if (argv[0].type == FUNC_ARG_STRING) {
        LLVMTypeRef type = LLVMTypeOf(argv[1].data.value);
        switch (LLVMGetTypeKind(LLVMTypeOf(argv[1].data.value))) {
        case LLVMIntegerTypeKind: ;
            unsigned int width = LLVMGetIntTypeWidth(type);
            if (width == 1) {
                *return_val = LLVMBuildICmp(exec->builder, LLVMIntEQ, arg_to_bool(exec, argv[0]), argv[1].data.value, "bool_eq");
            } else {
                *return_val = LLVMBuildICmp(exec->builder, LLVMIntEQ, arg_to_int(exec, argv[0]), argv[1].data.value, "int_eq");
            }
            break;
        case LLVMDoubleTypeKind:
            *return_val = LLVMBuildFCmp(exec->builder, LLVMRealOEQ, arg_to_double(exec, argv[0]), argv[1].data.value, "double_eq");
            break;
        case LLVMVoidTypeKind:
            *return_val = BOOLEAN(0);
            break;
        default:
            TraceLog(LOG_ERROR, "Unknown compare types!");
            return false;
        }
        return true;
    } else if (argv[1].type == FUNC_ARG_STRING) {
        LLVMTypeRef type = LLVMTypeOf(argv[0].data.value);
        switch (LLVMGetTypeKind(type)) {
        case LLVMIntegerTypeKind: ;
            unsigned int width = LLVMGetIntTypeWidth(type);
            if (width == 1) {
                *return_val = LLVMBuildICmp(exec->builder, LLVMIntEQ, argv[0].data.value, arg_to_bool(exec, argv[1]), "bool_eq");
            } else {
                *return_val = LLVMBuildICmp(exec->builder, LLVMIntEQ, argv[0].data.value, arg_to_int(exec, argv[1]), "int_eq");
            }
            break;
        case LLVMDoubleTypeKind:
            *return_val = LLVMBuildFCmp(exec->builder, LLVMRealOEQ, argv[0].data.value, arg_to_double(exec, argv[1]), "double_eq");
            break;
        case LLVMVoidTypeKind:
            *return_val = BOOLEAN(0);
            break;
        default:
            TraceLog(LOG_ERROR, "Unknown compare types!");
            return false;
        }
        return true;
    }

    LLVMTypeRef left_type = LLVMTypeOf(argv[0].data.value);
    LLVMTypeRef right_type = LLVMTypeOf(argv[1].data.value);
    if (LLVMGetTypeKind(left_type) != LLVMGetTypeKind(right_type)) {
        *return_val = BOOLEAN(0);
        return true;
    }

    switch (LLVMGetTypeKind(left_type)) {
    case LLVMIntegerTypeKind: ;
        unsigned int left_width = LLVMGetIntTypeWidth(left_type);
        unsigned int right_width = LLVMGetIntTypeWidth(right_type);
        if (left_width != right_width) {
            *return_val = BOOLEAN(0);
            return true;
        }
        if (left_width == 1) {
            *return_val = LLVMBuildICmp(exec->builder, LLVMIntEQ, argv[0].data.value, argv[1].data.value, "bool_eq");
        } else {
            *return_val = LLVMBuildICmp(exec->builder, LLVMIntEQ, argv[0].data.value, argv[1].data.value, "int_eq");
        }
        break;
    case LLVMDoubleTypeKind:
        *return_val = LLVMBuildFCmp(exec->builder, LLVMRealOEQ, argv[0].data.value, argv[1].data.value, "double_eq");
        break;
    case LLVMVoidTypeKind:
        *return_val = BOOLEAN(1);
        break;
    default:
        TraceLog(LOG_ERROR, "Unknown compare types!");
        return false;
    }

    return true;
}

bool block_false(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    *return_val = BOOLEAN(0);
    return true;
}

bool block_true(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    *return_val = BOOLEAN(1);
    return true;
}

bool block_or(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    *return_val = LLVMBuildOr(exec->builder, arg_to_bool(exec, argv[0]), arg_to_bool(exec, argv[1]), "non_const_bool_or");
    return true;
}

bool block_and(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    *return_val = LLVMBuildAnd(exec->builder, arg_to_bool(exec, argv[0]), arg_to_bool(exec, argv[1]), "non_const_bool_and");
    return true;
}

bool block_not(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);
    *return_val = LLVMBuildXor(exec->builder, arg_to_bool(exec, argv[0]), BOOLEAN(1), "non_const_not");
    return true;
}

bool block_more_eq(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING || LLVMGetTypeKind(LLVMTypeOf(argv[0].data.value)) != LLVMDoubleTypeKind) {
        *return_val = LLVMBuildICmp(exec->builder, LLVMIntSGE, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_int_more_or_eq");
    } else {
        *return_val = LLVMBuildFCmp(exec->builder, LLVMRealOGE, argv[0].data.value, arg_to_double(exec, argv[1]), "non_const_double_more_or_eq");
    }
    return true;
}

bool block_more(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING || LLVMGetTypeKind(LLVMTypeOf(argv[0].data.value)) != LLVMDoubleTypeKind) {
        *return_val = LLVMBuildICmp(exec->builder, LLVMIntSGT, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_int_more");
    } else {
        *return_val = LLVMBuildFCmp(exec->builder, LLVMRealOGT, argv[0].data.value, arg_to_double(exec, argv[1]), "non_const_double_more");
    }
    return true;
}

bool block_less_eq(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING || LLVMGetTypeKind(LLVMTypeOf(argv[0].data.value)) != LLVMDoubleTypeKind) {
        *return_val = LLVMBuildICmp(exec->builder, LLVMIntSLE, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_int_less_or_eq");
    } else {
        *return_val = LLVMBuildFCmp(exec->builder, LLVMRealOLE, argv[0].data.value, arg_to_double(exec, argv[1]), "non_const_double_less_or_eq");
    }
    return true;
}

bool block_less(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING || LLVMGetTypeKind(LLVMTypeOf(argv[0].data.value)) != LLVMDoubleTypeKind) {
        *return_val = LLVMBuildICmp(exec->builder, LLVMIntSLT, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_int_less");
    } else {
        *return_val = LLVMBuildFCmp(exec->builder, LLVMRealOLT, argv[0].data.value, arg_to_double(exec, argv[1]), "non_const_double_less");
    }
    return true;
}

bool block_bit_or(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    *return_val = LLVMBuildOr(exec->builder, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_or");
    return true;
}

bool block_bit_xor(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    *return_val = LLVMBuildXor(exec->builder, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_xor");
    return true;
}

bool block_bit_and(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    *return_val = LLVMBuildAnd(exec->builder, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_and");
    return true;
}

bool block_bit_not(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);
    LLVMValueRef add_op = LLVMBuildAdd(exec->builder, arg_to_int(exec, argv[0]), INTEGER(1), "");
    *return_val = LLVMBuildNeg(exec->builder, add_op, "bit_not");
    return true;
}

bool block_pi(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    *return_val = DOUBLE(M_PI);
    return true;
}

bool block_math(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_math");
    return false;
}

bool block_pow(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    LLVMValueRef pow_func = LLVMGetNamedFunction(exec->module, "int_pow");
    LLVMTypeRef pow_func_type = LLVMGlobalGetValueType(pow_func);
    LLVMValueRef pow_func_params[] = { arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]) };
    *return_val = LLVMBuildCall2(exec->builder, pow_func_type, pow_func, pow_func_params, ARRLEN(pow_func_params), "pow");
    return true;
}

bool block_rem(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING || LLVMGetTypeKind(LLVMTypeOf(argv[0].data.value)) != LLVMDoubleTypeKind) {
        *return_val = LLVMBuildSRem(exec->builder, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_rem");
    } else {
        *return_val = LLVMBuildFRem(exec->builder, argv[0].data.value, arg_to_double(exec, argv[1]), "non_const_rem");
    }
    
    if (LLVMIsPoison(*return_val)) {
        // TODO: Uncorporate runtime checks for division by zero
        TraceLog(LOG_ERROR, "[LLVM] Division by zero!");
        return false;
    }
    return true;
}

bool block_div(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING || LLVMGetTypeKind(LLVMTypeOf(argv[0].data.value)) != LLVMDoubleTypeKind) {
        *return_val = LLVMBuildSDiv(exec->builder, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_div");
    } else {
        *return_val = LLVMBuildFDiv(exec->builder, argv[0].data.value, arg_to_double(exec, argv[1]), "non_const_div");
    }
    
    if (LLVMIsPoison(*return_val)) {
        // TODO: Uncorporate runtime checks for division by zero
        TraceLog(LOG_ERROR, "[LLVM] Division by zero!");
        return false;
    }
    return true;
}

bool block_mult(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING || LLVMGetTypeKind(LLVMTypeOf(argv[0].data.value)) != LLVMDoubleTypeKind) {
        *return_val = LLVMBuildMul(exec->builder, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_mul");
    } else {
        *return_val = LLVMBuildFMul(exec->builder, argv[0].data.value, arg_to_double(exec, argv[1]), "non_const_mul");
    }
    return true;
}

bool block_minus(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING || LLVMGetTypeKind(LLVMTypeOf(argv[0].data.value)) != LLVMDoubleTypeKind) {
        *return_val = LLVMBuildSub(exec->builder, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_sub");
    } else {
        *return_val = LLVMBuildFSub(exec->builder, argv[0].data.value, arg_to_double(exec, argv[1]), "non_const_sub");
    }
    return true;
}

bool block_plus(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(2);
    if (argv[0].type == FUNC_ARG_STRING || LLVMGetTypeKind(LLVMTypeOf(argv[0].data.value)) != LLVMDoubleTypeKind) {
        *return_val = LLVMBuildAdd(exec->builder, arg_to_int(exec, argv[0]), arg_to_int(exec, argv[1]), "non_const_add");
    } else {
        *return_val = LLVMBuildFAdd(exec->builder, argv[0].data.value, arg_to_double(exec, argv[1]), "non_const_add");
    }
    return true;
}

bool block_convert_bool(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);
    *return_val = arg_to_bool(exec, argv[0]);
    return true;
}

bool block_convert_str(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_convert_str");
    return false;
}

bool block_convert_float(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);
    *return_val = arg_to_double(exec, argv[0]);
    return true;
}

bool block_convert_int(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);
    *return_val = arg_to_int(exec, argv[0]);
    return true;
}

bool block_unix_time(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_unix_time");
    return false;
}

bool block_length(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_length");
    return false;
}

bool block_substring(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_substring");
    return false;
}

bool block_letter_in(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_letter_in");
    return false;
}

bool block_chr(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_chr");
    return false;
}

bool block_ord(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_ord");
    return false;
}

bool block_join(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_join");
    return false;
}

bool block_random(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_random");
    return false;
}

bool block_get_char(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_get_char");
    return false;
}

bool block_input(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_input");
    return false;
}

bool block_term_set_clear(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_term_set_clear");
    return false;
}

bool block_term_clear(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_term_clear");
    return false;
}

bool block_reset_color(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_reset_color");
    return false;
}

bool block_set_bg_color(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_set_bg_color");
    return false;
}

bool block_set_fg_color(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_set_fg_color");
    return false;
}

bool block_set_cursor(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_set_cursor");
    return false;
}

bool block_cursor_max_y(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_cursor_max_y");
    return false;
}

bool block_cursor_max_x(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_cursor_max_x");
    return false;
}

bool block_cursor_y(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_cursor_y");
    return false;
}

bool block_cursor_x(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_cursor_x");
    return false;
}

bool block_print(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);

    if (argv[0].type == FUNC_ARG_STRING) {
        *return_val = *argv[0].data.str 
                      ? call_print(exec, LLVMBuildGlobalStringPtr(exec->builder, argv[0].data.str, ""))
                      : INTEGER(0);
        return true;
    }

    LLVMTypeRef type = LLVMTypeOf(argv[0].data.value);
    switch (LLVMGetTypeKind(type)) {
    case LLVMPointerTypeKind:
        *return_val = call_print(exec, argv[0].data.value);
        return true;
    case LLVMIntegerTypeKind:
        *return_val = LLVMGetIntTypeWidth(type) == 1 ? call_print_bool(exec, argv[0].data.value) : call_print_int(exec, argv[0].data.value);
        return true;
    case LLVMDoubleTypeKind:
        *return_val = call_print_double(exec, argv[0].data.value);
        return true;
    case LLVMVoidTypeKind:
        *return_val = INTEGER(0);
        return true;
    default:
        TraceLog(LOG_INFO, "[PRINT] Got non string value, idk i will just crash >:( !");
        LLVMTypeRef type = LLVMTypeOf(argv[0].data.value);

        char* type_str = LLVMPrintTypeToString(type);
        TraceLog(LOG_INFO, "[PRINT] The type is: %s", type_str);
        LLVMDisposeMessage(type_str);
        return false;
    }
}

bool block_println(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);
    block_print(exec, argc, argv, return_val);
    call_print(exec, LLVMBuildGlobalStringPtr(exec->builder, "\r\n", "new_line"));
    return true;
}

bool block_list_set(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_list_set");
    return false;
}

bool block_list_get(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_list_get");
    return false;
}

bool block_list_add(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_list_add");
    return false;
}

bool block_create_list(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_create_list");
    return false;
}

bool block_set_var(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_set_var");
    return false;
}

bool block_get_var(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_get_var");
    return false;
}

bool block_declare_var(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_declare_var");
    return false;
}

bool block_sleep(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_sleep");
    return false;
}

bool block_while(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_while");
    return false;
}

bool block_repeat(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    (void) return_val;
    TraceLog(LOG_ERROR, "[LLVM] Not implemented block_repeat");
    return false;
}

bool block_else(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);
    if (argv[0].type != FUNC_ARG_CONTROL) {
        TraceLog(LOG_ERROR, "[LLVM] First argument is not control argument!");
        return false;
    }

    if (argv[0].data.control == CONTROL_BEGIN) {
        MIN_ARG_COUNT(2);

        LLVMBasicBlockRef current_branch = LLVMGetInsertBlock(exec->builder);
        LLVMBasicBlockRef else_branch = LLVMInsertBasicBlock(current_branch, "else");
        LLVMBasicBlockRef end_branch = LLVMInsertBasicBlock(current_branch, "end_else");

        LLVMMoveBasicBlockAfter(end_branch, current_branch);
        LLVMMoveBasicBlockAfter(else_branch, current_branch);

        LLVMBuildCondBr(exec->builder, arg_to_bool(exec, argv[1]), end_branch, else_branch);

        LLVMPositionBuilderAtEnd(exec->builder, else_branch);
        control_data_stack_push_data(end_branch, LLVMBasicBlockRef);
    } else {
        LLVMBasicBlockRef end_branch;
        control_data_stack_pop_data(end_branch, LLVMBasicBlockRef);

        LLVMBuildBr(exec->builder, end_branch);
        LLVMPositionBuilderAtEnd(exec->builder, end_branch);
        *return_val = BOOLEAN(1);
    }

    return true;
}

bool block_else_if(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);
    if (argv[0].type != FUNC_ARG_CONTROL) {
        TraceLog(LOG_ERROR, "[LLVM] First argument is not control argument!");
        return false;
    }

    if (argv[0].data.control == CONTROL_BEGIN) {
        MIN_ARG_COUNT(3);

        LLVMBasicBlockRef current_branch = LLVMGetInsertBlock(exec->builder);
        LLVMBasicBlockRef else_if_check_branch = LLVMInsertBasicBlock(current_branch, "else_if_check");
        LLVMBasicBlockRef else_if_branch = LLVMInsertBasicBlock(current_branch, "else_if");
        LLVMBasicBlockRef else_if_fail_branch = LLVMInsertBasicBlock(current_branch, "else_if_fail");
        LLVMBasicBlockRef end_branch = LLVMInsertBasicBlock(current_branch, "end_else_if");

        LLVMMoveBasicBlockAfter(end_branch, current_branch);
        LLVMMoveBasicBlockAfter(else_if_fail_branch, current_branch);
        LLVMMoveBasicBlockAfter(else_if_branch, current_branch);
        LLVMMoveBasicBlockAfter(else_if_check_branch, current_branch);

        LLVMBuildCondBr(exec->builder, arg_to_bool(exec, argv[1]), end_branch, else_if_check_branch);
        control_data_stack_push_data(current_branch, LLVMBasicBlockRef);

        LLVMPositionBuilderAtEnd(exec->builder, else_if_check_branch);
        LLVMBuildCondBr(exec->builder, arg_to_bool(exec, argv[2]), else_if_branch, else_if_fail_branch);

        LLVMPositionBuilderAtEnd(exec->builder, else_if_fail_branch);
        LLVMBuildBr(exec->builder, end_branch);

        LLVMPositionBuilderAtEnd(exec->builder, else_if_branch);

        control_data_stack_push_data(else_if_fail_branch, LLVMBasicBlockRef);
        control_data_stack_push_data(end_branch, LLVMBasicBlockRef);
    } else {
        LLVMBasicBlockRef else_if_branch = LLVMGetInsertBlock(exec->builder);
        LLVMBasicBlockRef top_branch, fail_branch, end_branch;
        control_data_stack_pop_data(end_branch, LLVMBasicBlockRef);
        control_data_stack_pop_data(fail_branch, LLVMBasicBlockRef);
        control_data_stack_pop_data(top_branch, LLVMBasicBlockRef);

        LLVMBuildBr(exec->builder, end_branch);

        LLVMPositionBuilderAtEnd(exec->builder, end_branch);
        *return_val = LLVMBuildPhi(exec->builder, LLVMInt1Type(), "");

        LLVMValueRef vals[] = { BOOLEAN(1), BOOLEAN(1), BOOLEAN(0) };
        LLVMBasicBlockRef blocks[] = { top_branch, else_if_branch, fail_branch };
        LLVMAddIncoming(*return_val, vals, blocks, ARRLEN(blocks));
    }

    return true;
}

bool block_if(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);
    if (argv[0].type != FUNC_ARG_CONTROL) {
        TraceLog(LOG_ERROR, "[LLVM] First argument is not control argument!");
        return false;
    }

    if (argv[0].data.control == CONTROL_BEGIN) {
        MIN_ARG_COUNT(2);

        LLVMBasicBlockRef current_branch = LLVMGetInsertBlock(exec->builder);
        LLVMBasicBlockRef then_branch = LLVMInsertBasicBlock(current_branch, "if_cond");
        // This is needed for a phi block to determine if this condition has failed. The result of this phi node is then passed into a C-end block
        LLVMBasicBlockRef fail_branch = LLVMInsertBasicBlock(current_branch, "if_fail");
        LLVMBasicBlockRef end_branch = LLVMInsertBasicBlock(current_branch, "end_if");

        LLVMMoveBasicBlockAfter(end_branch, current_branch);
        LLVMMoveBasicBlockAfter(fail_branch, current_branch);
        LLVMMoveBasicBlockAfter(then_branch, current_branch);

        LLVMBuildCondBr(exec->builder, arg_to_bool(exec, argv[1]), then_branch, fail_branch);

        LLVMPositionBuilderAtEnd(exec->builder, fail_branch);
        LLVMBuildBr(exec->builder, end_branch);

        LLVMPositionBuilderAtEnd(exec->builder, then_branch);

        control_data_stack_push_data(fail_branch, LLVMBasicBlockRef);
        control_data_stack_push_data(end_branch, LLVMBasicBlockRef);
    } else {
        LLVMBasicBlockRef then_branch = LLVMGetInsertBlock(exec->builder);
        LLVMBasicBlockRef fail_branch, end_branch;
        control_data_stack_pop_data(end_branch, LLVMBasicBlockRef);
        control_data_stack_pop_data(fail_branch, LLVMBasicBlockRef);

        LLVMBuildBr(exec->builder, end_branch);

        LLVMPositionBuilderAtEnd(exec->builder, end_branch);
        *return_val = LLVMBuildPhi(exec->builder, LLVMInt1Type(), "");

        LLVMValueRef vals[] = { BOOLEAN(1), BOOLEAN(0) };
        LLVMBasicBlockRef blocks[] = { then_branch, fail_branch };
        LLVMAddIncoming(*return_val, vals, blocks, ARRLEN(blocks));
    }

    return true;
}

bool block_loop(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    MIN_ARG_COUNT(1);
    if (argv[0].type != FUNC_ARG_CONTROL) {
        TraceLog(LOG_ERROR, "[LLVM] First argument is not control argument!");
        return false;
    }

    if (argv[0].data.control == CONTROL_BEGIN) {
        LLVMBasicBlockRef current = LLVMGetInsertBlock(exec->builder);
        LLVMBasicBlockRef loop = LLVMInsertBasicBlock(current, "loop");
        LLVMBasicBlockRef loop_end = LLVMInsertBasicBlock(current, "loop_end");

        LLVMMoveBasicBlockAfter(loop_end, current);
        LLVMMoveBasicBlockAfter(loop, current);

        LLVMBuildBr(exec->builder, loop);
        LLVMPositionBuilderAtEnd(exec->builder, loop);

        control_data_stack_push_data(loop_end, LLVMBasicBlockRef);
    } else {
        LLVMBasicBlockRef loop_end;
        control_data_stack_pop_data(loop_end, LLVMBasicBlockRef);

        LLVMBasicBlockRef loop = LLVMGetInsertBlock(exec->builder);
        LLVMBuildBr(exec->builder, loop);
        LLVMPositionBuilderAtEnd(exec->builder, loop_end);
        *return_val = BOOLEAN(0);
    }

    return true;
}

bool block_noop(Exec* exec, int argc, FuncArg* argv, LLVMValueRef* return_val) {
    (void) exec;
    (void) argc;
    (void) argv;
    *return_val = LLVMConstPointerNull(LLVMVoidType());
    return true;
}

#endif // USE_INTERPRETER

BlockCategory* find_category(const char* name) {
    if (!palette.categories) return NULL;
    for (size_t i = 0; i < vector_size(palette.categories); i++) {
        if (!strcmp(palette.categories[i].name, name)) return &palette.categories[i];
    }
    return NULL;
}

void add_to_category(Blockdef* blockdef, BlockCategory* category) {
    vector_add(&category->blocks, block_new(blockdef));
}

void register_categories(void) {
    if (!palette.categories) palette.categories = vector_create();
    vector_add(&palette.categories, block_category_new(gettext("Control"),  (Color) CATEGORY_CONTROL_COLOR));
    vector_add(&palette.categories, block_category_new(gettext("Terminal"), (Color) CATEGORY_TERMINAL_COLOR));
    vector_add(&palette.categories, block_category_new(gettext("Math"),     (Color) CATEGORY_MATH_COLOR));
    vector_add(&palette.categories, block_category_new(gettext("Logic"),    (Color) CATEGORY_LOGIC_COLOR));
    vector_add(&palette.categories, block_category_new(gettext("Strings"),  (Color) CATEGORY_STRING_COLOR));
    vector_add(&palette.categories, block_category_new(gettext("Misc."),    (Color) CATEGORY_MISC_COLOR));
    vector_add(&palette.categories, block_category_new(gettext("Data"),     (Color) CATEGORY_DATA_COLOR));
}

// Creates and registers blocks (commands) for the Vm/Exec virtual machine
void register_blocks(Vm* vm) {
    BlockCategory* cat_control = find_category(gettext("Control"));
    assert(cat_control != NULL);
    BlockCategory* cat_terminal = find_category(gettext("Terminal"));
    assert(cat_terminal != NULL);
    BlockCategory* cat_math = find_category(gettext("Math"));
    assert(cat_math != NULL);
    BlockCategory* cat_logic = find_category(gettext("Logic"));
    assert(cat_logic != NULL);
    BlockCategory* cat_string = find_category(gettext("Strings"));
    assert(cat_string != NULL);
    BlockCategory* cat_misc = find_category(gettext("Misc."));
    assert(cat_misc != NULL);
    BlockCategory* cat_data = find_category(gettext("Data"));
    assert(cat_data != NULL);

    Blockdef* on_start = blockdef_new("on_start", BLOCKTYPE_HAT, (BlockdefColor) { 0xff, 0x77, 0x00, 0xFF }, block_noop);
    blockdef_add_text(on_start, gettext("When"));
    blockdef_add_image(on_start, (BlockdefImage) { .image_ptr = &run_tex });
    blockdef_add_text(on_start, gettext("clicked"));
    blockdef_register(vm, on_start);
    add_to_category(on_start, cat_control);

    Blockdef* sc_input = blockdef_new("input", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_input);
    blockdef_add_image(sc_input, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_input, gettext("Get input"));
    blockdef_register(vm, sc_input);
    add_to_category(sc_input, cat_terminal);

    Blockdef* sc_char = blockdef_new("get_char", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_get_char);
    blockdef_add_image(sc_char, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_char, gettext("Get char"));
    blockdef_register(vm, sc_char);
    add_to_category(sc_char, cat_terminal);

    Blockdef* sc_print = blockdef_new("print", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_print);
    blockdef_add_image(sc_print, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_print, gettext("Print"));
    blockdef_add_argument(sc_print, gettext("Hello, scrap!"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_print);
    add_to_category(sc_print, cat_terminal);

    Blockdef* sc_println = blockdef_new("println", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_println);
    blockdef_add_image(sc_println, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_println, gettext("Print line"));
    blockdef_add_argument(sc_println, gettext("Hello, scrap!"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_println);
    add_to_category(sc_println, cat_terminal);

    Blockdef* sc_cursor_x = blockdef_new("cursor_x", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_cursor_x);
    blockdef_add_image(sc_cursor_x, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_x, gettext("Cursor X"));
    blockdef_register(vm, sc_cursor_x);
    add_to_category(sc_cursor_x, cat_terminal);

    Blockdef* sc_cursor_y = blockdef_new("cursor_y", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_cursor_y);
    blockdef_add_image(sc_cursor_y, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_y, gettext("Cursor Y"));
    blockdef_register(vm, sc_cursor_y);
    add_to_category(sc_cursor_y, cat_terminal);

    Blockdef* sc_cursor_max_x = blockdef_new("cursor_max_x", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_cursor_max_x);
    blockdef_add_image(sc_cursor_max_x, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_max_x, gettext("Terminal width"));
    blockdef_register(vm, sc_cursor_max_x);
    add_to_category(sc_cursor_max_x, cat_terminal);

    Blockdef* sc_cursor_max_y = blockdef_new("cursor_max_y", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_cursor_max_y);
    blockdef_add_image(sc_cursor_max_y, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_max_y, gettext("Terminal height"));
    blockdef_register(vm, sc_cursor_max_y);
    add_to_category(sc_cursor_max_y, cat_terminal);

    Blockdef* sc_set_cursor = blockdef_new("set_cursor", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_set_cursor);
    blockdef_add_image(sc_set_cursor, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_set_cursor, gettext("Set cursor X:"));
    blockdef_add_argument(sc_set_cursor, "0", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_set_cursor, gettext("Y:"));
    blockdef_add_argument(sc_set_cursor, "0", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_set_cursor);
    add_to_category(sc_set_cursor, cat_terminal);

    Blockdef* sc_set_fg_color = blockdef_new("set_fg_color", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_set_fg_color);
    blockdef_add_image(sc_set_fg_color, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_set_fg_color, gettext("Set text color"));
    blockdef_add_dropdown(sc_set_fg_color, DROPDOWN_SOURCE_LISTREF, term_color_list_access);
    blockdef_register(vm, sc_set_fg_color);
    add_to_category(sc_set_fg_color, cat_terminal);

    Blockdef* sc_set_bg_color = blockdef_new("set_bg_color", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_set_bg_color);
    blockdef_add_image(sc_set_bg_color, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_set_bg_color, gettext("Set background color"));
    blockdef_add_dropdown(sc_set_bg_color, DROPDOWN_SOURCE_LISTREF, term_color_list_access);
    blockdef_register(vm, sc_set_bg_color);
    add_to_category(sc_set_bg_color, cat_terminal);

    Blockdef* sc_reset_color = blockdef_new("reset_color", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_reset_color);
    blockdef_add_image(sc_reset_color, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_reset_color, gettext("Reset color"));
    blockdef_register(vm, sc_reset_color);
    add_to_category(sc_reset_color, cat_terminal);

    Blockdef* sc_term_clear = blockdef_new("term_clear", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_term_clear);
    blockdef_add_image(sc_term_clear, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_term_clear, gettext("Clear terminal"));
    blockdef_register(vm, sc_term_clear);
    add_to_category(sc_term_clear, cat_terminal);
    
    Blockdef* sc_term_set_clear = blockdef_new("term_set_clear", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_TERMINAL_COLOR, block_term_set_clear);
    blockdef_add_image(sc_term_set_clear, (BlockdefImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_term_set_clear, gettext("Set clear color"));
    blockdef_add_dropdown(sc_term_set_clear, DROPDOWN_SOURCE_LISTREF, term_color_list_access);
    blockdef_register(vm, sc_term_set_clear);
    add_to_category(sc_term_set_clear, cat_terminal);

    Blockdef* sc_loop = blockdef_new("loop", BLOCKTYPE_CONTROL, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_loop);
    blockdef_add_text(sc_loop, gettext("Loop"));
    blockdef_register(vm, sc_loop);
    add_to_category(sc_loop, cat_control);

    Blockdef* sc_repeat = blockdef_new("repeat", BLOCKTYPE_CONTROL, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_repeat);
    blockdef_add_text(sc_repeat, gettext("Repeat"));
    blockdef_add_argument(sc_repeat, "10", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_repeat, gettext("times"));
    blockdef_register(vm, sc_repeat);
    add_to_category(sc_repeat, cat_control);

    Blockdef* sc_while = blockdef_new("while", BLOCKTYPE_CONTROL, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_while);
    blockdef_add_text(sc_while, gettext("While"));
    blockdef_add_argument(sc_while, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_while);
    add_to_category(sc_while, cat_control);

    Blockdef* sc_if = blockdef_new("if", BLOCKTYPE_CONTROL, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_if);
    blockdef_add_text(sc_if, gettext("If"));
    blockdef_add_argument(sc_if, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_if, gettext(", then"));
    blockdef_register(vm, sc_if);
    add_to_category(sc_if, cat_control);

    Blockdef* sc_else_if = blockdef_new("else_if", BLOCKTYPE_CONTROLEND, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_else_if);
    blockdef_add_text(sc_else_if, gettext("Else if"));
    blockdef_add_argument(sc_else_if, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_else_if, gettext(", then"));
    blockdef_register(vm, sc_else_if);
    add_to_category(sc_else_if, cat_control);

    Blockdef* sc_else = blockdef_new("else", BLOCKTYPE_CONTROLEND, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_else);
    blockdef_add_text(sc_else, gettext("Else"));
    blockdef_register(vm, sc_else);
    add_to_category(sc_else, cat_control);

    Blockdef* sc_do_nothing = blockdef_new("do_nothing", BLOCKTYPE_CONTROL, (BlockdefColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_do_nothing, gettext("Do nothing"));
    blockdef_register(vm, sc_do_nothing);
    add_to_category(sc_do_nothing, cat_control);

    Blockdef* sc_sleep = blockdef_new("sleep", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_CONTROL_COLOR, block_sleep);
    blockdef_add_text(sc_sleep, gettext("Sleep"));
    blockdef_add_argument(sc_sleep, "", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_sleep, gettext("μs"));
    blockdef_register(vm, sc_sleep);
    add_to_category(sc_sleep, cat_control);

    Blockdef* sc_end = blockdef_new("end", BLOCKTYPE_END, (BlockdefColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_end, gettext("End"));
    blockdef_register(vm, sc_end);

    Blockdef* sc_plus = blockdef_new("plus", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_plus);
    blockdef_add_argument(sc_plus, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_plus, "+");
    blockdef_add_argument(sc_plus, "10", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_plus);
    add_to_category(sc_plus, cat_math);

    Blockdef* sc_minus = blockdef_new("minus", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_minus);
    blockdef_add_argument(sc_minus, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_minus, "-");
    blockdef_add_argument(sc_minus, "10", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_minus);
    add_to_category(sc_minus, cat_math);

    Blockdef* sc_mult = blockdef_new("mult", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_mult);
    blockdef_add_argument(sc_mult, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_mult, "*");
    blockdef_add_argument(sc_mult, "10", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_mult);
    add_to_category(sc_mult, cat_math);

    Blockdef* sc_div = blockdef_new("div", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_div);
    blockdef_add_argument(sc_div, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_div, "/");
    blockdef_add_argument(sc_div, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_div);
    add_to_category(sc_div, cat_math);

    Blockdef* sc_pow = blockdef_new("pow", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_pow);
    blockdef_add_text(sc_pow, gettext("Pow"));
    blockdef_add_argument(sc_pow, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_argument(sc_pow, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_pow);
    add_to_category(sc_pow, cat_math);

    Blockdef* sc_math = blockdef_new("math", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_math);
    blockdef_add_dropdown(sc_math, DROPDOWN_SOURCE_LISTREF, math_list_access);
    blockdef_add_argument(sc_math, "", "0.0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_math);
    add_to_category(sc_math, cat_math);

    Blockdef* sc_pi = blockdef_new("pi", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_pi);
    blockdef_add_image(sc_pi, (BlockdefImage) { .image_ptr = &pi_symbol_tex });
    blockdef_register(vm, sc_pi);
    add_to_category(sc_pi, cat_math);

    Blockdef* sc_bit_not = blockdef_new("bit_not", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_bit_not);
    blockdef_add_text(sc_bit_not, "~");
    blockdef_add_argument(sc_bit_not, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_not);
    add_to_category(sc_bit_not, cat_logic);

    Blockdef* sc_bit_and = blockdef_new("bit_and", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_bit_and);
    blockdef_add_argument(sc_bit_and, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_and, "&");
    blockdef_add_argument(sc_bit_and, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_and);
    add_to_category(sc_bit_and, cat_logic);

    Blockdef* sc_bit_or = blockdef_new("bit_or", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_bit_or);
    blockdef_add_argument(sc_bit_or, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_or, "|");
    blockdef_add_argument(sc_bit_or, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_or);
    add_to_category(sc_bit_or, cat_logic);

    Blockdef* sc_bit_xor = blockdef_new("bit_xor", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_bit_xor);
    blockdef_add_argument(sc_bit_xor, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_xor, "^");
    blockdef_add_argument(sc_bit_xor, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_xor);
    add_to_category(sc_bit_xor, cat_logic);

    Blockdef* sc_rem = blockdef_new("rem", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MATH_COLOR, block_rem);
    blockdef_add_argument(sc_rem, "39", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_rem, "%");
    blockdef_add_argument(sc_rem, "5", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_rem);
    add_to_category(sc_rem, cat_math);

    Blockdef* sc_less = blockdef_new("less", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_less);
    blockdef_add_argument(sc_less, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_less, "<");
    blockdef_add_argument(sc_less, "11", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_less);
    add_to_category(sc_less, cat_logic);

    Blockdef* sc_less_eq = blockdef_new("less_eq", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_less_eq);
    blockdef_add_argument(sc_less_eq, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_less_eq, "<=");
    blockdef_add_argument(sc_less_eq, "11", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_less_eq);
    add_to_category(sc_less_eq, cat_logic);

    Blockdef* sc_eq = blockdef_new("eq", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_eq);
    blockdef_add_argument(sc_eq, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_eq, "=");
    blockdef_add_argument(sc_eq, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_eq);
    add_to_category(sc_eq, cat_logic);

    Blockdef* sc_not_eq = blockdef_new("not_eq", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_not_eq);
    blockdef_add_argument(sc_not_eq, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_not_eq, "!=");
    blockdef_add_argument(sc_not_eq, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_not_eq);
    add_to_category(sc_not_eq, cat_logic);

    Blockdef* sc_more_eq = blockdef_new("more_eq", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_more_eq);
    blockdef_add_argument(sc_more_eq, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_more_eq, ">=");
    blockdef_add_argument(sc_more_eq, "11", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_more_eq);
    add_to_category(sc_more_eq, cat_logic);

    Blockdef* sc_more = blockdef_new("more", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_more);
    blockdef_add_argument(sc_more, "9", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_more, ">");
    blockdef_add_argument(sc_more, "11", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_more);
    add_to_category(sc_more, cat_logic);

    Blockdef* sc_not = blockdef_new("not", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_not);
    blockdef_add_text(sc_not, gettext("Not"));
    blockdef_add_argument(sc_not, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_not);
    add_to_category(sc_not, cat_logic);

    Blockdef* sc_and = blockdef_new("and", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_and);
    blockdef_add_argument(sc_and, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_and, gettext("and"));
    blockdef_add_argument(sc_and, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_and);
    add_to_category(sc_and, cat_logic);

    Blockdef* sc_or = blockdef_new("or", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_or);
    blockdef_add_argument(sc_or, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_or, gettext("or"));
    blockdef_add_argument(sc_or, "", gettext("cond."), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_or);
    add_to_category(sc_or, cat_logic);

    Blockdef* sc_true = blockdef_new("true", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_true);
    blockdef_add_text(sc_true, gettext("True"));
    blockdef_register(vm, sc_true);
    add_to_category(sc_true, cat_logic);

    Blockdef* sc_false = blockdef_new("false", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_false);
    blockdef_add_text(sc_false, gettext("False"));
    blockdef_register(vm, sc_false);
    add_to_category(sc_false, cat_logic);

    Blockdef* sc_random = blockdef_new("random", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_LOGIC_COLOR, block_random);
    blockdef_add_text(sc_random, gettext("Random"));
    blockdef_add_argument(sc_random, "0", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_random, gettext("to"));
    blockdef_add_argument(sc_random, "10", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_random);
    add_to_category(sc_random, cat_logic);

    Blockdef* sc_join = blockdef_new("join", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_join);
    blockdef_add_text(sc_join, gettext("Join"));
    blockdef_add_argument(sc_join, gettext("left and "), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_argument(sc_join, gettext("right"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_join);
    add_to_category(sc_join, cat_string);

    Blockdef* sc_ord = blockdef_new("ord", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_ord);
    blockdef_add_text(sc_ord, gettext("Ord"));
    blockdef_add_argument(sc_ord, "A", gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_ord);
    add_to_category(sc_ord, cat_string);

    Blockdef* sc_chr = blockdef_new("chr", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_chr);
    blockdef_add_text(sc_chr, gettext("Chr"));
    blockdef_add_argument(sc_chr, "65", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_chr);
    add_to_category(sc_chr, cat_string);

    Blockdef* sc_letter_in = blockdef_new("letter_in", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_letter_in);
    blockdef_add_text(sc_letter_in, gettext("Letter"));
    blockdef_add_argument(sc_letter_in, "1", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_letter_in, gettext("in"));
    blockdef_add_argument(sc_letter_in, gettext("string"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_letter_in);
    add_to_category(sc_letter_in, cat_string);

    Blockdef* sc_substring = blockdef_new("substring", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_substring);
    blockdef_add_text(sc_substring, gettext("Substring"));
    blockdef_add_argument(sc_substring, "2", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_substring, gettext("to"));
    blockdef_add_argument(sc_substring, "4", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_substring, gettext("in"));
    blockdef_add_argument(sc_substring, gettext("string"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_substring);
    add_to_category(sc_substring, cat_string);

    Blockdef* sc_length = blockdef_new("length", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_STRING_COLOR, block_length);
    blockdef_add_text(sc_length, gettext("Length"));
    blockdef_add_argument(sc_length, gettext("string"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_length);
    add_to_category(sc_length, cat_string);

    Blockdef* sc_unix_time = blockdef_new("unix_time", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_unix_time);
    blockdef_add_text(sc_unix_time, gettext("Time since 1970"));
    blockdef_register(vm, sc_unix_time);
    add_to_category(sc_unix_time, cat_misc);
    
    Blockdef* sc_int = blockdef_new("convert_int", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_convert_int);
    blockdef_add_text(sc_int, gettext("Int"));
    blockdef_add_argument(sc_int, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_int);
    add_to_category(sc_int, cat_misc);

    Blockdef* sc_float = blockdef_new("convert_float", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_convert_float);
    blockdef_add_text(sc_float, gettext("Float"));
    blockdef_add_argument(sc_float, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_float);
    add_to_category(sc_float, cat_misc);

    Blockdef* sc_str = blockdef_new("convert_str", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_convert_str);
    blockdef_add_text(sc_str, gettext("Str"));
    blockdef_add_argument(sc_str, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_str);
    add_to_category(sc_str, cat_misc);

    Blockdef* sc_bool = blockdef_new("convert_bool", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_MISC_COLOR, block_convert_bool);
    blockdef_add_text(sc_bool, gettext("Bool"));
    blockdef_add_argument(sc_bool, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bool);
    add_to_category(sc_bool, cat_misc);

    Blockdef* sc_nothing = blockdef_new("nothing", BLOCKTYPE_NORMAL, (BlockdefColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_nothing, gettext("Nothing"));
    blockdef_register(vm, sc_nothing);
    add_to_category(sc_nothing, cat_misc);

    Blockdef* sc_comment = blockdef_new("comment", BLOCKTYPE_NORMAL, (BlockdefColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_comment, "//");
    blockdef_add_argument(sc_comment, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_comment);
    add_to_category(sc_comment, cat_misc);

    Blockdef* sc_decl_var = blockdef_new("decl_var", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_DATA_COLOR, block_declare_var);
    blockdef_add_text(sc_decl_var, gettext("Declare"));
    blockdef_add_argument(sc_decl_var, gettext("my variable"), gettext("Abc"), BLOCKCONSTR_STRING);
    blockdef_add_text(sc_decl_var, "=");
    blockdef_add_argument(sc_decl_var, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_decl_var);
    add_to_category(sc_decl_var, cat_data);

    Blockdef* sc_get_var = blockdef_new("get_var", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_DATA_COLOR, block_get_var);
    blockdef_add_text(sc_get_var, gettext("Get"));
    blockdef_add_argument(sc_get_var, gettext("my variable"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_get_var);
    add_to_category(sc_get_var, cat_data);

    Blockdef* sc_set_var = blockdef_new("set_var", BLOCKTYPE_NORMAL, (BlockdefColor) CATEGORY_DATA_COLOR, block_set_var);
    blockdef_add_text(sc_set_var, gettext("Set"));
    blockdef_add_argument(sc_set_var, gettext("my variable"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_set_var, "=");
    blockdef_add_argument(sc_set_var, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_set_var);
    add_to_category(sc_set_var, cat_data);

    Blockdef* sc_create_list = blockdef_new("create_list", BLOCKTYPE_NORMAL, (BlockdefColor) { 0xff, 0x44, 0x00, 0xff }, block_create_list);
    blockdef_add_image(sc_create_list, (BlockdefImage) { .image_ptr = &list_tex });
    blockdef_add_text(sc_create_list, gettext("Empty list"));
    blockdef_register(vm, sc_create_list);
    add_to_category(sc_create_list, cat_data);

    Blockdef* sc_list_add = blockdef_new("list_add", BLOCKTYPE_NORMAL, (BlockdefColor) { 0xff, 0x44, 0x00, 0xff }, block_list_add);
    blockdef_add_image(sc_list_add, (BlockdefImage) { .image_ptr = &list_tex });
    blockdef_add_text(sc_list_add, gettext("Add"));
    blockdef_add_argument(sc_list_add, gettext("my variable"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_add, gettext("value"));
    blockdef_add_argument(sc_list_add, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_list_add);
    add_to_category(sc_list_add, cat_data);

    Blockdef* sc_list_get = blockdef_new("list_get", BLOCKTYPE_NORMAL, (BlockdefColor) { 0xff, 0x44, 0x00, 0xff }, block_list_get);
    blockdef_add_image(sc_list_get, (BlockdefImage) { .image_ptr = &list_tex });
    blockdef_add_argument(sc_list_get, gettext("my variable"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_get, gettext("get at"));
    blockdef_add_argument(sc_list_get, "0", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_list_get);
    add_to_category(sc_list_get, cat_data);

    Blockdef* sc_list_set = blockdef_new("list_set", BLOCKTYPE_NORMAL, (BlockdefColor) { 0xff, 0x44, 0x00, 0xff }, block_list_set);
    blockdef_add_image(sc_list_set, (BlockdefImage) { .image_ptr = &list_tex });
    blockdef_add_argument(sc_list_set, gettext("my variable"), gettext("Abc"), BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_set, gettext("set at"));
    blockdef_add_argument(sc_list_set, "0", "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_set, "=");
    blockdef_add_argument(sc_list_set, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_list_set);
    add_to_category(sc_list_set, cat_data);

    Blockdef* sc_define_block = blockdef_new("define_block", BLOCKTYPE_HAT, (BlockdefColor) { 0x99, 0x00, 0xff, 0xff }, block_noop);
    blockdef_add_image(sc_define_block, (BlockdefImage) { .image_ptr = &special_tex });
    blockdef_add_text(sc_define_block, gettext("Define"));
    blockdef_add_blockdef_editor(sc_define_block);
    blockdef_register(vm, sc_define_block);
    add_to_category(sc_define_block, cat_control);

    Blockdef* sc_return = blockdef_new("return", BLOCKTYPE_NORMAL, (BlockdefColor) { 0x99, 0x00, 0xff, 0xff }, block_return);
    blockdef_add_image(sc_return, (BlockdefImage) { .image_ptr = &special_tex });
    blockdef_add_text(sc_return, gettext("Return"));
    blockdef_add_argument(sc_return, "", gettext("any"), BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_return);
    add_to_category(sc_return, cat_control);
}
