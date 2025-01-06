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

#ifndef BLOCKS_H
#define BLOCKS_H

#include "vm.h"

ScrData block_noop(ScrExec* exec, int argc, ScrData* argv);

ScrData block_loop(ScrExec* exec, int argc, ScrData* argv);
ScrData block_if(ScrExec* exec, int argc, ScrData* argv);
ScrData block_else_if(ScrExec* exec, int argc, ScrData* argv);
ScrData block_else(ScrExec* exec, int argc, ScrData* argv);
ScrData block_repeat(ScrExec* exec, int argc, ScrData* argv);
ScrData block_while(ScrExec* exec, int argc, ScrData* argv);
ScrData block_sleep(ScrExec* exec, int argc, ScrData* argv);

ScrData block_declare_var(ScrExec* exec, int argc, ScrData* argv);
ScrData block_get_var(ScrExec* exec, int argc, ScrData* argv);
ScrData block_set_var(ScrExec* exec, int argc, ScrData* argv);

ScrData block_create_list(ScrExec* exec, int argc, ScrData* argv);
ScrData block_list_add(ScrExec* exec, int argc, ScrData* argv);
ScrData block_list_get(ScrExec* exec, int argc, ScrData* argv);
ScrData block_list_set(ScrExec* exec, int argc, ScrData* argv);

ScrData block_print(ScrExec* exec, int argc, ScrData* argv);
ScrData block_println(ScrExec* exec, int argc, ScrData* argv);
ScrData block_cursor_x(ScrExec* exec, int argc, ScrData* argv);
ScrData block_cursor_y(ScrExec* exec, int argc, ScrData* argv);
ScrData block_cursor_max_x(ScrExec* exec, int argc, ScrData* argv);
ScrData block_cursor_max_y(ScrExec* exec, int argc, ScrData* argv);
ScrData block_set_cursor(ScrExec* exec, int argc, ScrData* argv);
ScrData block_term_clear(ScrExec* exec, int argc, ScrData* argv);
ScrData block_input(ScrExec* exec, int argc, ScrData* argv);
ScrData block_get_char(ScrExec* exec, int argc, ScrData* argv);

ScrData block_join(ScrExec* exec, int argc, ScrData* argv);
ScrData block_ord(ScrExec* exec, int argc, ScrData* argv);
ScrData block_chr(ScrExec* exec, int argc, ScrData* argv);
ScrData block_letter_in(ScrExec* exec, int argc, ScrData* argv);
ScrData block_substring(ScrExec* exec, int argc, ScrData* argv);
ScrData block_length(ScrExec* exec, int argc, ScrData* argv);

ScrData block_convert_int(ScrExec* exec, int argc, ScrData* argv);
ScrData block_convert_float(ScrExec* exec, int argc, ScrData* argv);
ScrData block_convert_str(ScrExec* exec, int argc, ScrData* argv);
ScrData block_convert_bool(ScrExec* exec, int argc, ScrData* argv);

ScrData block_unix_time(ScrExec* exec, int argc, ScrData* argv);

ScrData block_plus(ScrExec* exec, int argc, ScrData* argv);
ScrData block_minus(ScrExec* exec, int argc, ScrData* argv);
ScrData block_mult(ScrExec* exec, int argc, ScrData* argv);
ScrData block_div(ScrExec* exec, int argc, ScrData* argv);
ScrData block_rem(ScrExec* exec, int argc, ScrData* argv);
ScrData block_pow(ScrExec* exec, int argc, ScrData* argv);
ScrData block_math(ScrExec* exec, int argc, ScrData* argv);
ScrData block_pi(ScrExec* exec, int argc, ScrData* argv);
ScrData block_random(ScrExec* exec, int argc, ScrData* argv);

ScrData block_bit_not(ScrExec* exec, int argc, ScrData* argv);
ScrData block_bit_and(ScrExec* exec, int argc, ScrData* argv);
ScrData block_bit_xor(ScrExec* exec, int argc, ScrData* argv);
ScrData block_bit_or(ScrExec* exec, int argc, ScrData* argv);

ScrData block_less(ScrExec* exec, int argc, ScrData* argv);
ScrData block_less_eq(ScrExec* exec, int argc, ScrData* argv);
ScrData block_more(ScrExec* exec, int argc, ScrData* argv);
ScrData block_more_eq(ScrExec* exec, int argc, ScrData* argv);
ScrData block_not(ScrExec* exec, int argc, ScrData* argv);
ScrData block_and(ScrExec* exec, int argc, ScrData* argv);
ScrData block_or(ScrExec* exec, int argc, ScrData* argv);
ScrData block_true(ScrExec* exec, int argc, ScrData* argv);
ScrData block_false(ScrExec* exec, int argc, ScrData* argv);
ScrData block_eq(ScrExec* exec, int argc, ScrData* argv);
ScrData block_not_eq(ScrExec* exec, int argc, ScrData* argv);

ScrData block_exec_custom(ScrExec* exec, int argc, ScrData* argv);
ScrData block_custom_arg(ScrExec* exec, int argc, ScrData* argv);
ScrData block_return(ScrExec* exec, int argc, ScrData* argv);

#endif // BLOCKS_H
