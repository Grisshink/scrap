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

#ifndef THREAD_H
#define THREAD_H

#include <stdbool.h>
#include <pthread.h>

typedef enum {
    THREAD_STATE_NOT_RUNNING = 0,
    THREAD_STATE_STARTING,
    THREAD_STATE_RUNNING,
    THREAD_STATE_STOPPING,
    THREAD_STATE_DONE,
} ThreadState;

typedef enum {
    THREAD_RETURN_FAILURE = 0,
    THREAD_RETURN_SUCCESS = 1,
    THREAD_RETURN_STOPPED,
    THREAD_RETURN_RUNNING, // Returned from thread_try_join to signify that thread is still running
} ThreadReturnCode;

// Return value indicates if the thread was executed successfully
typedef bool (*ThreadEntry)(void*);
typedef void (*ThreadCleanup)(void*);

typedef struct {
    ThreadState state;
    ThreadEntry entry;
    ThreadCleanup cleanup;
    void* entry_data;
    pthread_t handle;
} Thread;

typedef pthread_mutex_t Mutex;

Mutex mutex_new(void);
void mutex_lock(Mutex* mutex);
void mutex_unlock(Mutex* mutex);
void mutex_free(Mutex* mutex);

Thread thread_new(ThreadEntry entry_func, ThreadCleanup cleanup_func);
bool thread_start(Thread* thread, void* data);
bool thread_is_running(Thread* thread);
void thread_handle_stopping_state(Thread* thread);
void thread_exit(Thread* thread, bool success);
bool thread_stop(Thread* thread);
ThreadReturnCode thread_join(Thread* thread);
ThreadReturnCode thread_try_join(Thread* thread);

#endif // THREAD_H
