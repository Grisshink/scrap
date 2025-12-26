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

#ifndef SCRAP_GC_H
#define SCRAP_GC_H

#include <stddef.h>
#ifndef STANDALONE_STD
#include <setjmp.h>
#endif

#include "ast.h"
#include "vec.h"

typedef struct {
    unsigned char marked;
    DataType data_type;
    unsigned char data[];
} GcChunkData;

typedef struct {
    GcChunkData* ptr;
    size_t len;
} GcChunk;

typedef struct {
    size_t chunks_base;
    size_t temp_chunks_base;
} GcRoot;

typedef struct {
    GcChunk* chunks;
    size_t* roots_bases;
    GcRoot* roots_stack;
    // NOTE: This variable stores a list of stack addresses pointing at gc_malloc'd memory, 
    // so you need to offset a pointer by -1 before dereferencing GcChunkData
    GcChunkData*** root_chunks;
    GcChunkData** root_temp_chunks;
    size_t memory_used;
    size_t memory_allocated;
    size_t memory_max;
#ifndef STANDALONE_STD
    jmp_buf run_jump_buf;
#endif
} Gc;

Gc gc_new(size_t memory_min, size_t memory_max);
void gc_free(Gc* gc);

void gc_root_begin(Gc* gc);
void gc_root_end(Gc* gc);
void* gc_malloc(Gc* gc, size_t size, DataType data_type);
void gc_collect(Gc* gc);
void gc_flush(Gc* gc);
void gc_add_root(Gc* gc, void* ptr);
void gc_add_temp_root(Gc* gc, void* ptr);
void gc_root_save(Gc* gc);
void gc_root_restore(Gc* gc);

#endif // SCRAP_GC_H
