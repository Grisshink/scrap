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
#include "scrap.h"

void gc_init(void) {
}

Gc gc_new(size_t memory_max) {
    return (Gc) {
        .chunks = vector_create(),
        .roots_stack = vector_create(),
        .memory_used = 0,
        .memory_max = memory_max,
    };
}

void gc_free(Gc* gc) {
#ifdef DEBUG
    TraceLog(LOG_INFO, "[GC] gc_free: used %zu bytes, allocated %zu chunks", gc->memory_used, vector_size(gc->chunks));
#endif
    for (size_t i = 0; i < vector_size(gc->chunks); i++) {
        free(gc->chunks[i].ptr);
    }
    for (size_t i = 0; i < vector_size(gc->roots_stack); i++) {
        vector_free(gc->roots_stack[i].chunks);
    }
    vector_free(gc->roots_stack);
    vector_free(gc->chunks);
    gc->memory_max = 0;
    gc->memory_used = 0;
}

void gc_root_begin(Gc* gc) {
    GcRoot* root = vector_add_dst(&gc->roots_stack);
    root->chunks = vector_create();
}

void gc_collect(Gc* gc) {
#ifdef DEBUG
    Timer t = start_timer("gc_collect");
#endif

    // Mark roots
    for (size_t i = 0; i < vector_size(gc->roots_stack); i++) {
        for (size_t j = 0; j < vector_size(gc->roots_stack[i].chunks); j++) {
            gc->roots_stack[i].chunks[j]->marked = 1;
        }
    }

    // Find unmarked chunks
    size_t memory_freed = 0;
    size_t chunks_deleted = 0;
    for (int i = vector_size(gc->chunks) - 1; i >= 0; i--) {
        if (gc->chunks[i].ptr->marked) {
            gc->chunks[i].ptr->marked = false;
            continue;
        }
        free(gc->chunks[i].ptr);
        memory_freed += gc->chunks[i].len;
        chunks_deleted++;
        vector_remove(gc->chunks, i);
    }

    gc->memory_used -= memory_freed;

#ifdef DEBUG
    double gc_time = end_timer(t);
    TraceLog(LOG_INFO, "[GC] gc_collect: freed %zu bytes, deleted %zu chunks, time: %.2fus", memory_freed, chunks_deleted, gc_time);
#endif
}

void* gc_malloc(Gc* gc, size_t size) {
    assert(vector_size(gc->roots_stack) > 0);

    if (gc->memory_used + size > gc->memory_max) gc_collect(gc);
    if (gc->memory_used + size > gc->memory_max) {
        TraceLog(LOG_ERROR, "[GC] Memory limit exeeded! Tried to allocate %zu bytes in gc with %zu bytes free", size, gc->memory_max - gc->memory_used);
        return NULL;
    }

    GcChunkData* chunk_data = malloc(size + sizeof(GcChunkData));
    if (chunk_data == NULL) return NULL;

    chunk_data->marked = 0;
    GcChunk chunk = (GcChunk) {
        .ptr = chunk_data,
        .len = size,
    };

    vector_add(&gc->chunks, chunk);
    vector_add(&gc->roots_stack[vector_size(gc->roots_stack) - 1].chunks, chunk.ptr);
    gc->memory_used += size;

    return chunk_data->data;
}

void gc_root_end(Gc* gc) {
    assert(vector_size(gc->roots_stack) > 0);
    vector_free(gc->roots_stack[vector_size(gc->roots_stack) - 1].chunks);
    vector_pop(gc->roots_stack);
}
