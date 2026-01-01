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

#include "gc.h"
#include "config.h"

#ifdef _WIN32
#include <windows.h>
#endif

Gc* gc;

void llvm_main(void);

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    Gc _gc = gc_new(MIN_MEMORY_LIMIT, MAX_MEMORY_LIMIT);
    gc = &_gc;
    llvm_main();
    gc_free(gc);
    return 0;
}
