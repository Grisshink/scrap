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

#include <stdbool.h>
#include <ffi.h>

#include "scrap_ir.h"
#include "term.h"

typedef struct {
    unsigned char r, g, b, a;
} StdColor;

typedef struct {
    IrValueType ir;
    ffi_type ffi;
} StdType;

typedef struct {
    StdType* items;
    size_t size, capacity;
} StdTypeList;

typedef struct {
    void* addr;
    StdTypeList args;
    StdType return_type;
} StdSymbol;

typedef struct {
    StdSymbol* items;
    size_t size, capacity;
} StdSymbolList;

typedef struct {
    char* name;
#ifdef _WIN32
    HMODULE handle;
#else
    void* handle;
#endif
} StdLibrary;

typedef struct {
    StdLibrary* items;
    size_t size, capacity;
} StdLibraryList;

IrRunFunction std_resolve_function(IrExec* exec, const char* hint);

void std_init(void);

#endif // SCRAP_STD_H
