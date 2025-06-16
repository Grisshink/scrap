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

#include <assert.h>
#include <stdio.h>

#include "gc.h"
#include "raylib.h"

#define STB_DS_IMPLEMENTATION
#include "../external/stb_ds.h"

void gc_init(void) {
    stbds_rand_seed(GetRandomValue(-0x7fffffff, 0x80000000));
}

Gc gc_new(size_t memory_max) {
    return (Gc) {
        .chunks = NULL,
        .roots_stack = vector_create(),
        .memory_used = 0,
        .memory_max = memory_max,
    };
}

void gc_root_begin(Gc* gc) {
    GcRoot* root = vector_add_dst(&gc->roots_stack);
    root->chunks = vector_create();
}

void gc_collect(Gc* gc) {
    // Unmark everything
    for (size_t i = 0; i < hmlenu(gc->chunks); i++) {
        gc->chunks[i].value.marked = false;
    }

    // Mark roots
    for (size_t i = 0; i < vector_size(gc->roots_stack); i++) {
        for (size_t j = 0; j < vector_size(gc->roots_stack[i].chunks); j++) {
            ptrdiff_t chunk_ind = hmgeti(gc->chunks, gc->roots_stack[i].chunks[j]);
            if (chunk_ind == -1) continue;
            gc->chunks[chunk_ind].value.marked = true;
        }
    }

    // Find unmarked chunks
    void** sweep_addrs = vector_create();
    for (size_t i = 0; i < hmlenu(gc->chunks); i++) {
        if (gc->chunks[i].value.marked) continue;
        vector_add(&sweep_addrs, gc->chunks[i].key);
    }

    // Free unmarked chunks
    size_t memory_freed = 0;
    for (size_t i = 0; i < vector_size(sweep_addrs); i++) {
        ptrdiff_t chunk_ind = hmgeti(gc->chunks, sweep_addrs[i]);
        free(gc->chunks[chunk_ind].value.ptr);
        memory_freed += gc->chunks[chunk_ind].value.len;
        int del_result = hmdel(gc->chunks, gc->chunks[chunk_ind].key);
        (void) del_result;
    }

    gc->memory_used -= memory_freed;
    //TraceLog(LOG_INFO, "[GC] Garbage sweep. %zu bytes freed, %zu chunks deleted", memory_freed, vector_size(sweep_addrs));
    vector_free(sweep_addrs);
}

void* gc_malloc(Gc* gc, size_t size) {
    assert(vector_size(gc->roots_stack) > 0);

    if (gc->memory_used + size > gc->memory_max) gc_collect(gc);
    if (gc->memory_used + size > gc->memory_max) {
        TraceLog(LOG_ERROR, "[GC] Memory limit exeeded! Tried to allocate %zu bytes in gc with %zu bytes free", size, gc->memory_max - gc->memory_used);
        return NULL;
    }

    void* mem = malloc(size);
    if (mem == NULL) return NULL;

    GcChunk chunk = (GcChunk) {
        .ptr = mem,
        .len = size,
        .marked = false,
    };

    hmput(gc->chunks, chunk.ptr, chunk);
    vector_add(&gc->roots_stack[vector_size(gc->roots_stack) - 1].chunks, chunk.ptr);
    gc->memory_used += size;
    return mem;
}

void gc_root_end(Gc* gc) {
    assert(vector_size(gc->roots_stack) > 0);
    vector_free(gc->roots_stack[vector_size(gc->roots_stack) - 1].chunks);
    vector_pop(gc->roots_stack);
}

void gc_free(Gc* gc) {
    for (size_t i = 0; i < hmlenu(gc->chunks); i++) {
        free(gc->chunks[i].value.ptr);
    }
    for (size_t i = 0; i < vector_size(gc->roots_stack); i++) {
        vector_free(gc->roots_stack[i].chunks);
    }
    vector_free(gc->roots_stack);
    hmfree(gc->chunks);
    gc->memory_max = 0;
    gc->memory_used = 0;
}
