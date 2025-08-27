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
    unsigned char marked;
    DataType data_type;
    unsigned char data[];
} GcChunkData;

typedef GcChunkData* ChunkAddr;
typedef GcChunkData** ChunkAddrList;

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
    ChunkAddrList root_chunks;
    ChunkAddrList root_temp_chunks;
    size_t memory_used;
    size_t memory_max;
} Gc;

Gc gc_new(size_t memory_max);
void gc_free(Gc* gc);

void gc_root_begin(Gc* gc);
void gc_root_end(Gc* gc);
void* gc_malloc(Gc* gc, size_t size, DataType data_type);
void gc_collect(Gc* gc);
void gc_flush(Gc* gc);
void gc_add_root(Gc* gc, void* ptr);
void gc_add_str_root(Gc* gc, char* str);
void gc_root_save(Gc* gc);
void gc_root_restore(Gc* gc);

#endif // SCRAP_GC_H
