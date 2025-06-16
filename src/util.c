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

#include "scrap.h"
#include "raylib.h"
#include "vec.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

Timer start_timer(const char* name) {
    Timer timer;
    timer.name = name;
    clock_gettime(CLOCK_MONOTONIC, &timer.start);
    return timer;
}

double end_timer(Timer timer) {
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_taken = (end.tv_sec - timer.start.tv_sec) * 1e+6 + (end.tv_nsec - timer.start.tv_nsec) * 1e-3;
    return time_taken;
}

int leading_ones(unsigned char byte) {
    int out = 0;
    while (byte & 0x80) {
        out++;
        byte <<= 1;
    }
    return out;
}

const char* into_data_path(const char* path) {
    return TextFormat("%s%s", GetApplicationDirectory(), path);
}

Block block_new_ms(Blockdef* blockdef) {
    Block block = block_new(blockdef);
    for (size_t i = 0; i < vector_size(block.arguments); i++) {
        if (block.arguments[i].type != ARGUMENT_BLOCKDEF) continue;
        block.arguments[i].data.blockdef->func = block_exec_custom;
    }
    return block;
}

const char* language_to_code(Language lang) {
    switch (lang) {
        case LANG_SYSTEM: return "system";
        case LANG_EN: return "en";
        case LANG_RU: return "ru";
        case LANG_KK: return "kk";
        case LANG_UK: return "uk";
    }
    assert(false && "Unreachable");
}

Language code_to_language(const char* code) {
    if (!strcmp(code, "en")) {
        return LANG_EN;
    } else if (!strcmp(code, "ru")) {
        return LANG_RU;
    } else if (!strcmp(code, "kk")) {
        return LANG_KK;
    } else if (!strcmp(code, "uk")) {
        return LANG_UK;
    } else {
        return LANG_SYSTEM;
    }
}

#define CSI_DARK_GRAY "\e[90m"
#define CSI_YELLOW "\e[93m"
#define CSI_RED "\e[91m"
#define CSI_RESET "\e[0m"

void scrap_log(int log_level, const char *text, va_list args) {
    switch (log_level) {
    case LOG_TRACE:
        printf(CSI_DARK_GRAY "[TRACE] ");
        break;
    case LOG_DEBUG:
        printf("[DEBUG] ");
        break;
    case LOG_INFO:
        printf("[INFO] ");
        break;
    case LOG_WARNING:
        printf(CSI_YELLOW "[WARN] ");
        break;
    case LOG_ERROR:
        printf(CSI_RED "[ERROR] ");
        break;
    case LOG_FATAL:
        printf(CSI_RED "[FATAL] ");
        break;
    default:
        printf(CSI_RED "[UNKNOWN] ");
        break;
    }

    vprintf(text, args);

    printf(CSI_RESET "\n");
}
