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
#include <string.h>
#include <sys/mman.h>
#include <errno.h>

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

static void gc_copy_refs(Gc* gc, GcChunkData* chunk);

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

static GcAlloc alloc_new(size_t memory_max) {
    GcAlloc alloc = (GcAlloc) {0};
    void* mem = mmap(NULL, memory_max, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((size_t)mem == (size_t)-1) {
        TRACE_LOG(LOG_ERROR, "Could not allocate memory for allocator: %s", strerror(errno));
        return alloc;
    }

    alloc.mem = mem;
    alloc.mem_max = memory_max;
    return alloc;
}

static void* alloc_malloc(GcAlloc* alloc, size_t size) {
    // Align all chunks to 8 bytes
    size += 7 - ((size - 1) % 8);

    if (alloc->mem_used + size > alloc->mem_max) return NULL;
    void* ptr = alloc->mem + alloc->mem_used;
    alloc->mem_used += size;
    alloc->chunks_count++;
    return ptr;
}

static void alloc_lock(GcAlloc* alloc) {
    if (!alloc->mem) return;
    mprotect(alloc->mem, alloc->mem_max, PROT_NONE);
}

static void alloc_unlock(GcAlloc* alloc) {
    if (!alloc->mem) return;
    mprotect(alloc->mem, alloc->mem_max, PROT_READ | PROT_WRITE);
}

static void alloc_swap(GcAlloc* main, GcAlloc* second) {
    GcAlloc temp = *main;
    *main = *second;
    *second = temp;
}

static void alloc_free(GcAlloc* alloc) {
    if (alloc->mem) munmap(alloc->mem, alloc->mem_max);
    alloc->mem_used = 0;
    alloc->mem_max = 0;
}

static GcChunkData* copy_chunk(GcAlloc* old_alloc, GcAlloc* new_alloc, void* ref) {
    GcChunkData* chunk = (*(GcChunkData**)ref) - 1;
    if ((void*)chunk < old_alloc->mem || 
        (void*)chunk >= old_alloc->mem + old_alloc->mem_max)
    {
        return NULL;
    }

    if (chunk->copy_ptr) {
        *(GcChunkData**)ref = chunk->copy_ptr;
        return NULL;
    }

    GcChunkData* new_chunk = alloc_malloc(new_alloc, chunk->len + sizeof(GcChunkData));
    if (!new_chunk) return NULL;
    new_chunk->len = chunk->len;
    new_chunk->copy_ptr = NULL;
    new_chunk->data_type = chunk->data_type;
    memcpy(new_chunk->data, chunk->data, chunk->len);

    chunk->copy_ptr = new_chunk;

    *(GcChunkData**)ref = (void*)new_chunk->data;
    return new_chunk;
}

Gc gc_new(size_t memory_max) {
    GcAlloc main_alloc = alloc_new(memory_max);
    GcAlloc second_alloc = alloc_new(memory_max);
    alloc_lock(&second_alloc);

    return (Gc) {
        .main_alloc = main_alloc,
        .second_alloc = second_alloc,
        .roots_stack = vector_create(),
        .roots_bases = vector_create(),
        .root_chunks = vector_create(),
    };
}

void gc_free(Gc* gc) {
#ifdef DEBUG
    TRACE_LOG(LOG_INFO, "[GC] gc_free: used %zu bytes, allocated %zu chunks", gc->main_alloc.mem_used, gc->main_alloc.chunks_count);
#endif
    alloc_free(&gc->main_alloc);
    alloc_free(&gc->second_alloc);
    vector_free(gc->roots_bases);
    vector_free(gc->root_chunks);
    vector_free(gc->roots_stack);
}

static void gc_copy_any(Gc* gc, AnyValue* any) {
    if (any->type == DATA_TYPE_LIST) {
        GcChunkData* new_chunk = copy_chunk(&gc->main_alloc, &gc->second_alloc, &any->data.list_val);
        if (new_chunk) {
            gc_copy_refs(gc, new_chunk);
        }
    } else if (any->type == DATA_TYPE_STRING) {
        copy_chunk(&gc->main_alloc, &gc->second_alloc, &any->data.str_val);
    }
}

static void gc_copy_refs(Gc* gc, GcChunkData* chunk) {
    switch (chunk->data_type) {
    case DATA_TYPE_LIST: ;
        List* list = (List*)chunk->data;
        if (!list->values) break;

        if (!copy_chunk(&gc->main_alloc, &gc->second_alloc, &list->values)) break;
        
        for (long i = 0; i < list->size; i++) {
            gc_copy_any(gc, &list->values[i]);
        }
        break;
    case DATA_TYPE_ANY: ;
        gc_copy_any(gc, (AnyValue*)chunk->data);
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

    alloc_unlock(&gc->second_alloc);

    gc->second_alloc.mem_used = 0;
    gc->second_alloc.chunks_count = 0;

    // Mark roots
    for (size_t i = 0; i < vector_size(gc->root_chunks); i++) {
        GcChunkData* new_chunk = copy_chunk(&gc->main_alloc, &gc->second_alloc, gc->root_chunks[i]);
        if (!new_chunk) continue;
        gc_copy_refs(gc, new_chunk);
    }

    size_t memory_freed = gc->main_alloc.mem_used - gc->second_alloc.mem_used;
    size_t chunks_deleted = gc->main_alloc.chunks_count - gc->second_alloc.chunks_count;

    alloc_swap(&gc->main_alloc, &gc->second_alloc);
    alloc_lock(&gc->second_alloc);

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

    const size_t chunk_size = size + sizeof(GcChunkData);
    GcChunkData* chunk_data = alloc_malloc(&gc->main_alloc, chunk_size);

    if (chunk_data == NULL) {
        gc_collect(gc);
        chunk_data = alloc_malloc(&gc->main_alloc, chunk_size);
    }
    if (chunk_data == NULL) {
        std_term_print_str(text_format("*[GC] Memory limit exeeded! Tried to allocate %zu bytes in gc with %zu bytes free*", chunk_size, gc->main_alloc.mem_max - gc->main_alloc.mem_used));
        EXIT;
    }

    chunk_data->copy_ptr = NULL;
    chunk_data->data_type = data_type;
    chunk_data->len = size;

    return chunk_data->data;
}

void gc_root_begin(Gc* gc) {
    if (vector_size(gc->roots_stack) > 1024) {
        std_term_print_str("*[GC] Root stack overflow!*");
        EXIT;
    }

    vector_add(&gc->roots_stack, vector_size(gc->root_chunks));
}

void gc_root_end(Gc* gc) {
    assert(vector_size(gc->roots_stack) > 0);
    vector_get_header(gc->root_chunks)->size = gc->roots_stack[vector_size(gc->roots_stack) - 1];
    vector_pop(gc->roots_stack);
}

void gc_add_root(Gc* gc, void* stack_ptr) {
    vector_add(&gc->root_chunks, stack_ptr);
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
    } else if (*size < prev_size) {
        vector_get_header(gc->root_chunks)->size = gc->roots_stack[*size];
    }
}
