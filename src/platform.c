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

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>

#include "term.h"

#define SCRAP_IR_IMPLEMENTATION
#include "scrap_ir.h"

void scrap_set_env(const char* name, const char* value) {
#ifdef _WIN32
    char buf[256];
    snprintf(buf, 256, "%s=%s", name, value);
    putenv(buf);
    SetEnvironmentVariableA(name, value);
#else
    setenv(name, value, false);
#endif // _WIN32
}


void init_console(void) {
#ifdef _WIN32
    AllocConsole();
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
    freopen("CON", "r", stdin);
    SetConsoleTitle(gettext("Scrap console"));
#endif
}
