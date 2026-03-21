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

#ifndef SCRAP_STD_H
#define SCRAP_STD_H

#include "scrap_ir.h"
#include <stdbool.h>

typedef struct {
    unsigned char r, g, b, a;
} StdColor;

IrRunFunction std_resolve_function(IrExec* exec, const char* hint);

void std_init(void);

// Math
int std_int_pow(int base, int exp);

// Terminal control
#ifndef STANDALONE_STD
#include "term.h"
#endif

// Misc
bool std_sleep(IrExec* exec);
int std_get_random(int min, int max);
void std_set_random_seed(int seed);
// StdColor std_parse_color(const char* value);

#endif // SCRAP_STD_H
