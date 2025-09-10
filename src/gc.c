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
#include <time.h>
#include <stdarg.h>

#include "gc.h"
#include "std.h"

#ifdef STANDALONE_STD

#define EXIT exit(1)
#define TRACE_LOG(loglevel, ...) fprintf(stderr, __VA_ARGS__)

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

#else

#include <pthread.h>
#include "raylib.h"
#define EXIT pthread_exit((void*)0)
#define TRACE_LOG(loglevel, ...) TraceLog(loglevel, __VA_ARGS__)

#endif // STANDALONE_STD

static void gc_mark_refs(Gc* gc, GcChunkData* chunk);

#define MAX_TEXT_BUFFER_LENGTH 512

static const char *text_format(const char *text, ...) {
    static char buffer[MAX_TEXT_BUFFER_LENGTH] = {0};
    buffer[0] = 0;

    va_list args;
    va_start(args, text);
    int requiredByteCount = vsnprintf(buffer, MAX_TEXT_BUFFER_LENGTH, text, args);
    va_end(args);

    // If requiredByteCount is larger than the MAX_TEXT_BUFFER_LENGTH, then overflow occured
    if (requiredByteCount >= MAX_TEXT_BUFFER_LENGTH) {
        // Inserting "..." at the end of the string to mark as truncated
        char *truncBuffer = buffer + MAX_TEXT_BUFFER_LENGTH - 4; // Adding 4 bytes = "...\0"
        sprintf(truncBuffer, "...");
    }

    return buffer;
}

Gc gc_new(size_t memory_max) {
    return (Gc) {
        .chunks = vector_create(),
        .roots_stack = vector_create(),
        .roots_bases = vector_create(),
        .root_chunks = vector_create(),
        .root_temp_chunks = vector_create(),
        .memory_used = 0,
        .memory_max = memory_max,
    };
}

void gc_free(Gc* gc) {
#ifdef DEBUG
    TRACE_LOG(LOG_INFO, "[GC] gc_free: used %zu bytes, allocated %zu chunks", gc->memory_used, vector_size(gc->chunks));
#endif
    for (size_t i = 0; i < vector_size(gc->chunks); i++) {
        free(gc->chunks[i].ptr);
    }
    vector_free(gc->roots_bases);
    vector_free(gc->root_chunks);
    vector_free(gc->root_temp_chunks);
    vector_free(gc->roots_stack);
    vector_free(gc->chunks);
    gc->memory_max = 0;
    gc->memory_used = 0;
}

static void gc_mark_any(Gc* gc, AnyValue* any) {
    GcChunkData* chunk_inner;

    if (any->type == DATA_TYPE_LIST) {
        chunk_inner = ((GcChunkData*)any->data.list_val) - 1;
        if (!chunk_inner->marked) {
            chunk_inner->marked = 1;
            gc_mark_refs(gc, chunk_inner);
        }
    } else if (any->type == DATA_TYPE_STRING_REF) {
        chunk_inner = ((GcChunkData*)any->data.str_val) - 1;
        chunk_inner->marked = 1;
    }
}

static void gc_mark_refs(Gc* gc, GcChunkData* chunk) {
    switch (chunk->data_type) {
    case DATA_TYPE_LIST: ;
        List* list = (List*)chunk->data;
        if (!list->values) break;

        GcChunkData* list_values_chunk = ((GcChunkData*)list->values) - 1;
        list_values_chunk->marked = 1;
        
        for (long i = 0; i < list->size; i++) {
            gc_mark_any(gc, &list->values[i]);
        }
        break;
    case DATA_TYPE_ANY: ;
        gc_mark_any(gc, (AnyValue*)chunk->data);
        break;
    default:
        break;
    }
}

void gc_collect(Gc* gc) {
#ifdef DEBUG
#if defined(_WIN32) && defined(STANDALONE_STD)
    long t;
    long t_freq;

    QueryPerformanceCounter((LARGE_INTEGER*)&t);
    QueryPerformanceFrequency((LARGE_INTEGER*)&t_freq);
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
#endif // defined(_WIN32) && defined(STANDALONE_STD)
#endif // DEBUG

    // Mark roots
    for (size_t i = 0; i < vector_size(gc->root_chunks); i++) {
        if (gc->root_chunks[i]->marked) continue;
        gc->root_chunks[i]->marked = 1;
        gc_mark_refs(gc, gc->root_chunks[i]);
    }

    for (size_t i = 0; i < vector_size(gc->root_temp_chunks); i++) {
        if (gc->root_temp_chunks[i]->marked) continue;
        gc->root_temp_chunks[i]->marked = 1;
        gc_mark_refs(gc, gc->root_temp_chunks[i]);
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
#if defined(_WIN32) && defined(STANDALONE_STD)
    long end_time;
    QueryPerformanceCounter((LARGE_INTEGER*)&end_time);

    double gc_time = (double)(end_time - t) * 1e+6 / (double)t_freq;
#else
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double gc_time = (end_time.tv_sec - t.tv_sec) * 1e+6 + (end_time.tv_nsec - t.tv_nsec) * 1e-3;
#endif // defined(_WIN32) && defined(STANDALONE_STD)
    TRACE_LOG(LOG_INFO, "[GC] gc_collect: freed %zu bytes, deleted %zu chunks, time: %.2fus", memory_freed, chunks_deleted, gc_time);
#endif // DEBUG
}

void* gc_malloc(Gc* gc, size_t size, DataType data_type) {
    assert(vector_size(gc->roots_stack) > 0);

    if (size > gc->memory_max) {
        std_term_print_str(text_format("*[GC] Memory limit exeeded! Tried to allocate %zu bytes past maximum memory limit in gc (%zu bytes)*", size, gc->memory_max));
        EXIT;
    }

    if (gc->memory_used + size > gc->memory_max) gc_collect(gc);
    if (gc->memory_used + size > gc->memory_max) {
        std_term_print_str(text_format("*[GC] Memory limit exeeded! Tried to allocate %zu bytes in gc with %zu bytes free*", size, gc->memory_max - gc->memory_used));
        EXIT;
    }

    GcChunkData* chunk_data = malloc(size + sizeof(GcChunkData));
    if (chunk_data == NULL) return NULL;

    chunk_data->marked = 0;
    chunk_data->data_type = data_type;
    GcChunk chunk = (GcChunk) {
        .ptr = chunk_data,
        .len = size,
    };

    vector_add(&gc->chunks, chunk);
    vector_add(&gc->root_temp_chunks, chunk.ptr);
    gc->memory_used += size;

    return chunk_data->data;
}

void gc_root_begin(Gc* gc) {
    if (vector_size(gc->roots_stack) > 1024) {
        std_term_print_str("*[GC] Root stack overflow!*");
        EXIT;
    }

    GcRoot* root = vector_add_dst(&gc->roots_stack);
    root->chunks_base = vector_size(gc->root_chunks);
    root->temp_chunks_base = vector_size(gc->root_temp_chunks);
}

void gc_root_end(Gc* gc) {
    assert(vector_size(gc->roots_stack) > 0);
    vector_get_header(gc->root_chunks)->size = gc->roots_stack[vector_size(gc->roots_stack) - 1].chunks_base;
    vector_get_header(gc->root_temp_chunks)->size = gc->roots_stack[vector_size(gc->roots_stack) - 1].temp_chunks_base;
    vector_pop(gc->roots_stack);
}

void gc_add_root(Gc* gc, void* ptr) {
    GcChunkData* chunk_ptr = ((GcChunkData*)ptr) - 1;
    vector_add(&gc->root_chunks, chunk_ptr);
}

void gc_add_str_root(Gc* gc, char* str) {
    StringHeader* header = ((StringHeader*)str) - 1;
    GcChunkData* chunk_ptr = ((GcChunkData*)header) - 1;
    vector_add(&gc->root_chunks, chunk_ptr);
}

void gc_root_save(Gc* gc) {
    if (vector_size(gc->roots_bases) > 1024) {
        std_term_print_str("*[GC] Root stack overflow!*");
        EXIT;
    }

    vector_add(&gc->roots_bases, vector_size(gc->roots_stack));
}

void gc_root_restore(Gc* gc) {
    if (vector_size(gc->roots_bases) == 0) {
        std_term_print_str("*[GC] Root stack underflow!*");
        EXIT;
    }

    size_t* size = &vector_get_header(gc->roots_stack)->size;
    size_t prev_size = *size;
    *size = gc->roots_bases[vector_size(gc->roots_bases) - 1];
    vector_pop(gc->roots_bases);

    if (*size == 0) {
        vector_clear(gc->root_chunks);
        vector_clear(gc->root_temp_chunks);
    } else if (*size < prev_size) {
        vector_get_header(gc->root_chunks)->size = gc->roots_stack[*size].chunks_base;
        vector_get_header(gc->root_temp_chunks)->size = gc->roots_stack[*size].temp_chunks_base;
    }
}

void gc_flush(Gc* gc) {
    vector_get_header(gc->root_temp_chunks)->size = gc->roots_stack[vector_size(gc->roots_stack) - 1].temp_chunks_base;
}
