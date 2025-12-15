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

#include "gc.h"
#include "config.h"

Gc* gc;

void llvm_main(void);

int main(void) {
    Gc _gc = gc_new(MIN_MEMORY_LIMIT, MAX_MEMORY_LIMIT);
    gc = &_gc;
    llvm_main();
    gc_free(gc);
    return 0;
}
