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

#ifndef SCRAP_UTIL_H
#define SCRAP_UTIL_H

#include <time.h>
#include <stdarg.h>

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

#define CONVERT_COLOR(color, type) (type) { color.r, color.g, color.b, color.a }

#define MOD(x, y) (((x) % (y) + (y)) % (y))
#define LERP(min, max, t) (((max) - (min)) * (t) + (min))
#define UNLERP(min, max, v) (((float)(v) - (float)(min)) / ((float)(max) - (float)(min)))

typedef struct {
    struct timespec start;
    const char* name;
} Timer;

Timer start_timer(const char* name);
double end_timer(Timer timer);
void scrap_log(int log_level, const char *text, va_list args);

#endif // SCRAP_UTIL_H
