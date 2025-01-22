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

#include "blocks.h"
#include "term.h"
#include "scrap.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

#define MATH_LIST_LEN 10

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

char** math_list_access(ScrBlock* block, size_t* list_len) {
    (void) block;
    *list_len = MATH_LIST_LEN;
    return block_math_list;
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

ScrData string_make_managed(String* string) {
    ScrData out;
    out.type = DATA_STR;
    out.storage.type = DATA_STORAGE_MANAGED;
    out.storage.storage_len = string->len + 1;
    out.data.str_arg = string->str;
    return out;
}

void string_free(String string) {
    free(string.str);
}

ScrData block_noop(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_NOTHING;
}

ScrData block_loop(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_if(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_else_if(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_else(ScrExec* exec, int argc, ScrData* argv) {
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
ScrData block_repeat(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_while(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_sleep(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_declare_var(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 2) RETURN_NOTHING;
    if (argv[0].type != DATA_STR || argv[0].storage.type != DATA_STORAGE_STATIC) RETURN_NOTHING;

    ScrData var_value = data_copy(argv[1]);
    if (var_value.storage.type == DATA_STORAGE_MANAGED) var_value.storage.type = DATA_STORAGE_UNMANAGED;

    variable_stack_push_var(exec, argv[0].data.str_arg, var_value);
    return var_value;
}

ScrData block_get_var(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_NOTHING;
    ScrVariable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    return var->value;
}

ScrData block_set_var(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 2) RETURN_NOTHING;

    ScrVariable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;

    ScrData new_value = data_copy(argv[1]);
    if (new_value.storage.type == DATA_STORAGE_MANAGED) new_value.storage.type = DATA_STORAGE_UNMANAGED;

    if (var->value.storage.type == DATA_STORAGE_UNMANAGED) {
        data_free(var->value);
    }

    var->value = new_value;
    return var->value;
}

ScrData block_create_list(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;

    ScrData out;
    out.type = DATA_LIST;
    out.storage.type = DATA_STORAGE_MANAGED;
    out.storage.storage_len = 0;
    out.data.list_arg.items = NULL;
    out.data.list_arg.len = 0;
    return out;
}

ScrData block_list_add(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;

    ScrVariable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    if (var->value.type != DATA_LIST) RETURN_NOTHING;

    if (!var->value.data.list_arg.items) {
        var->value.data.list_arg.items = malloc(sizeof(ScrData));
        var->value.data.list_arg.len = 1;
    } else {
        var->value.data.list_arg.items = realloc(var->value.data.list_arg.items, ++var->value.data.list_arg.len * sizeof(ScrData));
    }
    var->value.storage.storage_len = var->value.data.list_arg.len * sizeof(ScrData);
    ScrData* list_item = &var->value.data.list_arg.items[var->value.data.list_arg.len - 1];
    if (argv[1].storage.type == DATA_STORAGE_MANAGED) {
        argv[1].storage.type = DATA_STORAGE_UNMANAGED;
        *list_item = argv[1];
    } else {
        *list_item = data_copy(argv[1]);
        if (list_item->storage.type == DATA_STORAGE_MANAGED) list_item->storage.type = DATA_STORAGE_UNMANAGED;
    }

    return *list_item;
}

ScrData block_list_get(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;

    ScrVariable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    if (var->value.type != DATA_LIST) RETURN_NOTHING;
    if (!var->value.data.list_arg.items || var->value.data.list_arg.len == 0) RETURN_NOTHING;
    int index = data_to_int(argv[1]);
    if (index < 0 || (size_t)index >= var->value.data.list_arg.len) RETURN_NOTHING;

    return var->value.data.list_arg.items[index];
}

ScrData block_list_set(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 3) RETURN_NOTHING;

    ScrVariable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    if (var->value.type != DATA_LIST) RETURN_NOTHING;
    if (!var->value.data.list_arg.items || var->value.data.list_arg.len == 0) RETURN_NOTHING;
    int index = data_to_int(argv[1]);
    if (index < 0 || (size_t)index >= var->value.data.list_arg.len) RETURN_NOTHING;

    ScrData new_value = data_copy(argv[2]);
    if (new_value.storage.type == DATA_STORAGE_MANAGED) new_value.storage.type = DATA_STORAGE_UNMANAGED;

    if (var->value.data.list_arg.items[index].storage.type == DATA_STORAGE_UNMANAGED) {
        data_free(var->value.data.list_arg.items[index]);
    }
    var->value.data.list_arg.items[index] = new_value;
    return var->value.data.list_arg.items[index];
}

ScrData block_print(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_println(ScrExec* exec, int argc, ScrData* argv) {
    ScrData out = block_print(exec, argc, argv);
    term_print_str("\r\n");
    return out;
}

ScrData block_cursor_x(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&term.lock);
    int cur_x = 0;
    if (term.char_w != 0) cur_x = term.cursor_pos % term.char_w;
    pthread_mutex_unlock(&term.lock);
    RETURN_INT(cur_x);
}

ScrData block_cursor_y(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&term.lock);
    int cur_y = 0;
    if (term.char_w != 0) cur_y = term.cursor_pos / term.char_w;
    pthread_mutex_unlock(&term.lock);
    RETURN_INT(cur_y);
}

ScrData block_cursor_max_x(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&term.lock);
    int cur_max_x = term.char_w;
    pthread_mutex_unlock(&term.lock);
    RETURN_INT(cur_max_x);
}

ScrData block_cursor_max_y(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&term.lock);
    int cur_max_y = term.char_h;
    pthread_mutex_unlock(&term.lock);
    RETURN_INT(cur_max_y);
}

ScrData block_set_cursor(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;
    pthread_mutex_lock(&term.lock);
    int x = CLAMP(data_to_int(argv[0]), 0, term.char_w - 1);
    int y = CLAMP(data_to_int(argv[1]), 0, term.char_h - 1);
    term.cursor_pos = x + y * term.char_w;
    pthread_mutex_unlock(&term.lock);
    RETURN_NOTHING;
}

ScrData block_term_clear(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    term_clear();
    RETURN_NOTHING;
}

ScrData block_input(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_get_char(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_random(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_join(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;

    String string = string_new(0);
    string_add(&string, data_to_str(argv[0]));
    string_add(&string, data_to_str(argv[1]));
    return string_make_managed(&string);
}

ScrData block_ord(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);

    const char* str = data_to_str(argv[0]);
    int codepoint_size;
    int codepoint = GetCodepoint(str, &codepoint_size);

    RETURN_INT(codepoint);
}

ScrData block_chr(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_NOTHING;

    int text_size;
    const char* text = CodepointToUTF8(data_to_int(argv[0]), &text_size);

    String string = string_new(0);
    string_add_array(&string, text, text_size);
    return string_make_managed(&string);
}

ScrData block_letter_in(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_substring(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_length(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_unix_time(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_INT(time(NULL));
}

ScrData block_convert_int(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    RETURN_INT(data_to_int(argv[0]));
}

ScrData block_convert_float(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_DOUBLE(0.0);
    RETURN_DOUBLE(data_to_double(argv[0]));
}

ScrData block_convert_str(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    String string = string_new(0);
    if (argc < 1) return string_make_managed(&string);
    string_add(&string, data_to_str(argv[0]));
    return string_make_managed(&string);
}

ScrData block_convert_bool(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    RETURN_BOOL(data_to_bool(argv[0]));
}

ScrData block_plus(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg + data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) + data_to_int(argv[1]));
    }
}

ScrData block_minus(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg - data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) - data_to_int(argv[1]));
    }
}

ScrData block_mult(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg * data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) * data_to_int(argv[1]));
    }
}

ScrData block_div(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg / data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) / data_to_int(argv[1]));
    }
}

ScrData block_pow(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_math(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_pi(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_DOUBLE(M_PI);
}

ScrData block_bit_not(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(~0);
    RETURN_INT(~data_to_int(argv[0]));
}

ScrData block_bit_and(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    RETURN_INT(data_to_int(argv[0]) & data_to_int(argv[1]));
}

ScrData block_bit_xor(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    if (argc < 2) RETURN_INT(data_to_int(argv[0]));
    RETURN_INT(data_to_int(argv[0]) ^ data_to_int(argv[1]));
}

ScrData block_bit_or(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    if (argc < 2) RETURN_INT(data_to_int(argv[0]));
    RETURN_INT(data_to_int(argv[0]) | data_to_int(argv[1]));
}

ScrData block_rem(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(fmod(argv[0].data.double_arg, data_to_double(argv[1])));
    } else {
        RETURN_INT(data_to_int(argv[0]) % data_to_int(argv[1]));
    }
}

ScrData block_less(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) < 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg < data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) < data_to_int(argv[1]));
    }
}

ScrData block_less_eq(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) <= 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg <= data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) <= data_to_int(argv[1]));
    }
}

ScrData block_more(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) > 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg > data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) > data_to_int(argv[1]));
    }
}

ScrData block_more_eq(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) >= 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg >= data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) >= data_to_int(argv[1]));
    }
}

ScrData block_not(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(1);
    RETURN_BOOL(!data_to_bool(argv[0]));
}

ScrData block_and(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_BOOL(0);
    RETURN_BOOL(data_to_bool(argv[0]) && data_to_bool(argv[1]));
}

ScrData block_or(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_BOOL(0);
    RETURN_BOOL(data_to_bool(argv[0]) || data_to_bool(argv[1]));
}

ScrData block_true(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_BOOL(1);
}

ScrData block_false(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_BOOL(0);
}

ScrData block_eq(ScrExec* exec, int argc, ScrData* argv) {
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

ScrData block_not_eq(ScrExec* exec, int argc, ScrData* argv) {
    ScrData out = block_eq(exec, argc, argv);
    out.data.int_arg = !out.data.int_arg;
    return out;
}

ScrData block_exec_custom(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_NOTHING;
    if (argv[0].type != DATA_CHAIN) RETURN_NOTHING;
    ScrData return_val;
    exec_run_custom(exec, argv[0].data.chain_arg, argc - 1, argv + 1, &return_val);
    return return_val;
}

ScrData block_custom_arg(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_NOTHING;
    if (argv[0].type != DATA_INT) RETURN_NOTHING;
    if (argv[0].data.int_arg >= exec->chain_stack[exec->chain_stack_len - 1].custom_argc) RETURN_NOTHING;
    return data_copy(exec->chain_stack[exec->chain_stack_len - 1].custom_argv[argv[0].data.int_arg]);
}

ScrData block_return(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_NOTHING;
    exec->chain_stack[exec->chain_stack_len - 1].return_arg = data_copy(argv[0]);
    exec->chain_stack[exec->chain_stack_len - 1].is_returning = true;
    RETURN_NOTHING;
}

void load_blocks(ScrVm* vm) {
    ScrBlockdef* on_start = blockdef_new("on_start", BLOCKTYPE_HAT, (ScrColor) { 0xff, 0x77, 0x00, 0xFF }, block_noop);
    blockdef_add_text(on_start, "When");
    blockdef_add_image(on_start, (ScrImage) { .image_ptr = &run_tex });
    blockdef_add_text(on_start, "clicked");
    blockdef_register(vm, on_start);

    ScrBlockdef* sc_input = blockdef_new("input", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xff }, block_input);
    blockdef_add_image(sc_input, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_input, "Get input");
    blockdef_register(vm, sc_input);

    ScrBlockdef* sc_char = blockdef_new("get_char", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xff }, block_get_char);
    blockdef_add_image(sc_char, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_char, "Get char");
    blockdef_register(vm, sc_char);

    ScrBlockdef* sc_print = blockdef_new("print", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_print);
    blockdef_add_image(sc_print, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_print, "Print");
    blockdef_add_argument(sc_print, "Привет, мусороид!", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_print);

    ScrBlockdef* sc_println = blockdef_new("println", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_println);
    blockdef_add_image(sc_println, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_println, "Print line");
    blockdef_add_argument(sc_println, "Привет, мусороид!", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_println);

    ScrBlockdef* sc_cursor_x = blockdef_new("cursor_x", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_cursor_x);
    blockdef_add_image(sc_cursor_x, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_x, "Cursor X");
    blockdef_register(vm, sc_cursor_x);

    ScrBlockdef* sc_cursor_y = blockdef_new("cursor_y", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_cursor_y);
    blockdef_add_image(sc_cursor_y, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_y, "Cursor Y");
    blockdef_register(vm, sc_cursor_y);

    ScrBlockdef* sc_cursor_max_x = blockdef_new("cursor_max_x", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_cursor_max_x);
    blockdef_add_image(sc_cursor_max_x, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_max_x, "Terminal width");
    blockdef_register(vm, sc_cursor_max_x);

    ScrBlockdef* sc_cursor_max_y = blockdef_new("cursor_max_y", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_cursor_max_y);
    blockdef_add_image(sc_cursor_max_y, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_max_y, "Terminal height");
    blockdef_register(vm, sc_cursor_max_y);

    ScrBlockdef* sc_set_cursor = blockdef_new("set_cursor", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_set_cursor);
    blockdef_add_image(sc_set_cursor, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_set_cursor, "Set cursor X:");
    blockdef_add_argument(sc_set_cursor, "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_set_cursor, "Y:");
    blockdef_add_argument(sc_set_cursor, "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_set_cursor);

    ScrBlockdef* sc_term_clear = blockdef_new("term_clear", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_term_clear);
    blockdef_add_image(sc_term_clear, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_term_clear, "Clear terminal");
    blockdef_register(vm, sc_term_clear);

    ScrBlockdef* sc_loop = blockdef_new("loop", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_loop);
    blockdef_add_text(sc_loop, "Loop");
    blockdef_register(vm, sc_loop);

    ScrBlockdef* sc_repeat = blockdef_new("repeat", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_repeat);
    blockdef_add_text(sc_repeat, "Repeat");
    blockdef_add_argument(sc_repeat, "10", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_repeat, "times");
    blockdef_register(vm, sc_repeat);

    ScrBlockdef* sc_while = blockdef_new("while", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_while);
    blockdef_add_text(sc_while, "While");
    blockdef_add_argument(sc_while, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_while);

    ScrBlockdef* sc_if = blockdef_new("if", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_if);
    blockdef_add_text(sc_if, "If");
    blockdef_add_argument(sc_if, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_if, ", then");
    blockdef_register(vm, sc_if);

    ScrBlockdef* sc_else_if = blockdef_new("else_if", BLOCKTYPE_CONTROLEND, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_else_if);
    blockdef_add_text(sc_else_if, "Else if");
    blockdef_add_argument(sc_else_if, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_else_if, ", then");
    blockdef_register(vm, sc_else_if);

    ScrBlockdef* sc_else = blockdef_new("else", BLOCKTYPE_CONTROLEND, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_else);
    blockdef_add_text(sc_else, "Else");
    blockdef_register(vm, sc_else);

    ScrBlockdef* sc_do_nothing = blockdef_new("do_nothing", BLOCKTYPE_CONTROL, (ScrColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_do_nothing, "Do nothing");
    blockdef_register(vm, sc_do_nothing);

    ScrBlockdef* sc_sleep = blockdef_new("sleep", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_sleep);
    blockdef_add_text(sc_sleep, "Sleep");
    blockdef_add_argument(sc_sleep, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_sleep, "us");
    blockdef_register(vm, sc_sleep);

    ScrBlockdef* sc_end = blockdef_new("end", BLOCKTYPE_END, (ScrColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_end, "End");
    blockdef_register(vm, sc_end);

    ScrBlockdef* sc_plus = blockdef_new("plus", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_plus);
    blockdef_add_argument(sc_plus, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_plus, "+");
    blockdef_add_argument(sc_plus, "10", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_plus);

    ScrBlockdef* sc_minus = blockdef_new("minus", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_minus);
    blockdef_add_argument(sc_minus, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_minus, "-");
    blockdef_add_argument(sc_minus, "10", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_minus);

    ScrBlockdef* sc_mult = blockdef_new("mult", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_mult);
    blockdef_add_argument(sc_mult, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_mult, "*");
    blockdef_add_argument(sc_mult, "10", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_mult);

    ScrBlockdef* sc_div = blockdef_new("div", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_div);
    blockdef_add_argument(sc_div, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_div, "/");
    blockdef_add_argument(sc_div, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_div);

    ScrBlockdef* sc_pow = blockdef_new("pow", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_pow);
    blockdef_add_text(sc_pow, "Pow");
    blockdef_add_argument(sc_pow, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_add_argument(sc_pow, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_pow);

    ScrBlockdef* sc_math = blockdef_new("math", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xff }, block_math);
    blockdef_add_dropdown(sc_math, DROPDOWN_SOURCE_LISTREF, math_list_access);
    blockdef_add_argument(sc_math, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_math);

    ScrBlockdef* sc_pi = blockdef_new("pi", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xff }, block_pi);
    blockdef_add_text(sc_pi, "Pi");
    blockdef_register(vm, sc_pi);

    ScrBlockdef* sc_bit_not = blockdef_new("bit_not", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_bit_not);
    blockdef_add_text(sc_bit_not, "~");
    blockdef_add_argument(sc_bit_not, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_not);

    ScrBlockdef* sc_bit_and = blockdef_new("bit_and", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_bit_and);
    blockdef_add_argument(sc_bit_and, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_and, "&");
    blockdef_add_argument(sc_bit_and, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_and);

    ScrBlockdef* sc_bit_or = blockdef_new("bit_or", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_bit_or);
    blockdef_add_argument(sc_bit_or, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_or, "|");
    blockdef_add_argument(sc_bit_or, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_or);

    ScrBlockdef* sc_bit_xor = blockdef_new("bit_xor", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_bit_xor);
    blockdef_add_argument(sc_bit_xor, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_xor, "^");
    blockdef_add_argument(sc_bit_xor, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bit_xor);

    ScrBlockdef* sc_rem = blockdef_new("rem", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_rem);
    blockdef_add_argument(sc_rem, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_rem, "%");
    blockdef_add_argument(sc_rem, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_rem);

    ScrBlockdef* sc_less = blockdef_new("less", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_less);
    blockdef_add_argument(sc_less, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_less, "<");
    blockdef_add_argument(sc_less, "11", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_less);

    ScrBlockdef* sc_less_eq = blockdef_new("less_eq", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_less_eq);
    blockdef_add_argument(sc_less_eq, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_less_eq, "<=");
    blockdef_add_argument(sc_less_eq, "11", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_less_eq);

    ScrBlockdef* sc_eq = blockdef_new("eq", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_eq);
    blockdef_add_argument(sc_eq, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_eq, "=");
    blockdef_add_argument(sc_eq, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_eq);

    ScrBlockdef* sc_not_eq = blockdef_new("not_eq", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_not_eq);
    blockdef_add_argument(sc_not_eq, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_not_eq, "!=");
    blockdef_add_argument(sc_not_eq, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_not_eq);

    ScrBlockdef* sc_more_eq = blockdef_new("more_eq", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_more_eq);
    blockdef_add_argument(sc_more_eq, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_more_eq, ">=");
    blockdef_add_argument(sc_more_eq, "11", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_more_eq);

    ScrBlockdef* sc_more = blockdef_new("more", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_more);
    blockdef_add_argument(sc_more, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_more, ">");
    blockdef_add_argument(sc_more, "11", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_more);

    ScrBlockdef* sc_not = blockdef_new("not", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_not);
    blockdef_add_text(sc_not, "Not");
    blockdef_add_argument(sc_not, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_not);

    ScrBlockdef* sc_and = blockdef_new("and", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_and);
    blockdef_add_argument(sc_and, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_and, "and");
    blockdef_add_argument(sc_and, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_and);

    ScrBlockdef* sc_or = blockdef_new("or", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_or);
    blockdef_add_argument(sc_or, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_or, "or");
    blockdef_add_argument(sc_or, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_or);

    ScrBlockdef* sc_true = blockdef_new("true", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_true);
    blockdef_add_text(sc_true, "True");
    blockdef_register(vm, sc_true);

    ScrBlockdef* sc_false = blockdef_new("false", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_false);
    blockdef_add_text(sc_false, "False");
    blockdef_register(vm, sc_false);

    ScrBlockdef* sc_random = blockdef_new("random", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_random);
    blockdef_add_text(sc_random, "Random");
    blockdef_add_argument(sc_random, "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_random, "to");
    blockdef_add_argument(sc_random, "10", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_random);

    ScrBlockdef* sc_join = blockdef_new("join", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_join);
    blockdef_add_text(sc_join, "Join");
    blockdef_add_argument(sc_join, "абоба ", BLOCKCONSTR_UNLIMITED);
    blockdef_add_argument(sc_join, "мусор", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_join);

    ScrBlockdef* sc_ord = blockdef_new("ord", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_ord);
    blockdef_add_text(sc_ord, "Ord");
    blockdef_add_argument(sc_ord, "A", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_ord);

    ScrBlockdef* sc_chr = blockdef_new("chr", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_chr);
    blockdef_add_text(sc_chr, "Chr");
    blockdef_add_argument(sc_chr, "65", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_chr);

    ScrBlockdef* sc_letter_in = blockdef_new("letter_in", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_letter_in);
    blockdef_add_text(sc_letter_in, "Letter");
    blockdef_add_argument(sc_letter_in, "1", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_letter_in, "in");
    blockdef_add_argument(sc_letter_in, "string", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_letter_in);

    ScrBlockdef* sc_substring = blockdef_new("substring", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_substring);
    blockdef_add_text(sc_substring, "Substring");
    blockdef_add_argument(sc_substring, "2", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_substring, "-");
    blockdef_add_argument(sc_substring, "4", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_substring, "in");
    blockdef_add_argument(sc_substring, "string", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_substring);

    ScrBlockdef* sc_length = blockdef_new("length", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_length);
    blockdef_add_text(sc_length, "Length");
    blockdef_add_argument(sc_length, "мусорка", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_length);

    ScrBlockdef* sc_unix_time = blockdef_new("unix_time", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0x99, 0xff, 0xff }, block_unix_time);
    blockdef_add_text(sc_unix_time, "Time since 1970");
    blockdef_register(vm, sc_unix_time);

    ScrBlockdef* sc_int = blockdef_new("convert_int", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0x99, 0xff, 0xff }, block_convert_int);
    blockdef_add_text(sc_int, "Int");
    blockdef_add_argument(sc_int, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_int);

    ScrBlockdef* sc_float = blockdef_new("convert_float", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0x99, 0xff, 0xff }, block_convert_float);
    blockdef_add_text(sc_float, "Float");
    blockdef_add_argument(sc_float, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_float);

    ScrBlockdef* sc_str = blockdef_new("convert_str", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0x99, 0xff, 0xff }, block_convert_str);
    blockdef_add_text(sc_str, "Str");
    blockdef_add_argument(sc_str, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_str);

    ScrBlockdef* sc_bool = blockdef_new("convert_bool", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0x99, 0xff, 0xff }, block_convert_bool);
    blockdef_add_text(sc_bool, "Bool");
    blockdef_add_argument(sc_bool, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_bool);

    ScrBlockdef* sc_nothing = blockdef_new("nothing", BLOCKTYPE_NORMAL, (ScrColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_nothing, "Nothing");
    blockdef_register(vm, sc_nothing);

    ScrBlockdef* sc_comment = blockdef_new("comment", BLOCKTYPE_NORMAL, (ScrColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_comment, "//");
    blockdef_add_argument(sc_comment, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_comment);

    ScrBlockdef* sc_decl_var = blockdef_new("decl_var", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x77, 0x00, 0xff }, block_declare_var);
    blockdef_add_text(sc_decl_var, "Declare");
    blockdef_add_argument(sc_decl_var, "my variable", BLOCKCONSTR_STRING);
    blockdef_add_text(sc_decl_var, "=");
    blockdef_add_argument(sc_decl_var, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_decl_var);

    ScrBlockdef* sc_get_var = blockdef_new("get_var", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x77, 0x00, 0xff }, block_get_var);
    blockdef_add_text(sc_get_var, "Get");
    blockdef_add_argument(sc_get_var, "my variable", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_get_var);

    ScrBlockdef* sc_set_var = blockdef_new("set_var", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x77, 0x00, 0xff }, block_set_var);
    blockdef_add_text(sc_set_var, "Set");
    blockdef_add_argument(sc_set_var, "my variable", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_set_var, "=");
    blockdef_add_argument(sc_set_var, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_set_var);

    ScrBlockdef* sc_create_list = blockdef_new("create_list", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x44, 0x00, 0xff }, block_create_list);
    blockdef_add_image(sc_create_list, (ScrImage) { .image_ptr = &list_tex });
    blockdef_add_text(sc_create_list, "Empty list");
    blockdef_register(vm, sc_create_list);

    ScrBlockdef* sc_list_add = blockdef_new("list_add", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x44, 0x00, 0xff }, block_list_add);
    blockdef_add_image(sc_list_add, (ScrImage) { .image_ptr = &list_tex });
    blockdef_add_text(sc_list_add, "Add");
    blockdef_add_argument(sc_list_add, "my variable", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_add, "value");
    blockdef_add_argument(sc_list_add, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_list_add);

    ScrBlockdef* sc_list_get = blockdef_new("list_get", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x44, 0x00, 0xff }, block_list_get);
    blockdef_add_image(sc_list_get, (ScrImage) { .image_ptr = &list_tex });
    blockdef_add_argument(sc_list_get, "my variable", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_get, "get at");
    blockdef_add_argument(sc_list_get, "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_list_get);

    ScrBlockdef* sc_list_set = blockdef_new("list_set", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x44, 0x00, 0xff }, block_list_set);
    blockdef_add_image(sc_list_set, (ScrImage) { .image_ptr = &list_tex });
    blockdef_add_argument(sc_list_set, "my variable", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_set, "set at");
    blockdef_add_argument(sc_list_set, "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_set, "=");
    blockdef_add_argument(sc_list_set, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_list_set);

    ScrBlockdef* sc_define_block = blockdef_new("define_block", BLOCKTYPE_HAT, (ScrColor) { 0x99, 0x00, 0xff, 0xff }, block_noop);
    blockdef_add_image(sc_define_block, (ScrImage) { .image_ptr = &special_tex });
    blockdef_add_text(sc_define_block, "Define");
    blockdef_add_blockdef_editor(sc_define_block);
    blockdef_register(vm, sc_define_block);

    ScrBlockdef* sc_return = blockdef_new("return", BLOCKTYPE_NORMAL, (ScrColor) { 0x99, 0x00, 0xff, 0xff }, block_return);
    blockdef_add_image(sc_return, (ScrImage) { .image_ptr = &special_tex });
    blockdef_add_text(sc_return, "Return");
    blockdef_add_argument(sc_return, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(vm, sc_return);
}
