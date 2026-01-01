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

#include "vec.h"
#include "util.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#define LOG_ALL 0
#define LOG_TRACE 1
#define LOG_DEBUG 2
#define LOG_INFO 3
#define LOG_WARNING 4
#define LOG_ERROR 5
#define LOG_FATAL 6
#define LOG_NONE 7

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
