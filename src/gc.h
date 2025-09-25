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

#ifndef SCRAP_GC_H
#define SCRAP_GC_H

#include <stddef.h>

#include "ast.h"
#include "vec.h"

typedef struct {
    void* copy_ptr;
    DataType data_type;
    size_t len;
    unsigned char data[];
} GcChunkData;

typedef struct {
    void* mem;
    size_t mem_used;
    size_t mem_max;
    size_t chunks_count;
} GcAlloc;

typedef struct {
    GcAlloc main_alloc;
    GcAlloc second_alloc;

    size_t* roots_bases;
    size_t* roots_stack;
    // NOTE: This variable stores a list of stack addresses pointing at gc_malloc'd memory, 
    // so you need to offset a pointer by -1 before dereferencing GcChunkData
    GcChunkData*** root_chunks;
} Gc;

Gc gc_new(size_t memory_max);
void gc_free(Gc* gc);

void gc_root_begin(Gc* gc);
void gc_root_end(Gc* gc);
void* gc_malloc(Gc* gc, size_t size, DataType data_type);
void gc_collect(Gc* gc);
void gc_add_root(Gc* gc, void* ptr);
void gc_root_save(Gc* gc);
void gc_root_restore(Gc* gc);

#endif // SCRAP_GC_H
