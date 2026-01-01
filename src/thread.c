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

#include "thread.h"

Mutex mutex_new(void) {
    Mutex mutex;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex, &attr);
    return mutex;
}

void mutex_free(Mutex* mutex) {
    pthread_mutex_destroy(mutex);
}

void mutex_lock(Mutex* mutex) {
    pthread_mutex_lock(mutex);
}

void mutex_unlock(Mutex* mutex) {
    pthread_mutex_unlock(mutex);
}

Thread thread_new(ThreadEntry entry_func, ThreadCleanup cleanup_func) {
    return (Thread) {
        .state = THREAD_STATE_NOT_RUNNING,
        .entry = entry_func,
        .cleanup = cleanup_func,
    };
}

static void* thread_entry(void* t) {
    Thread* thread = t;
    thread->state = THREAD_STATE_RUNNING;

    void* return_val = thread->entry(thread->entry_data) ? (void*)THREAD_RETURN_SUCCESS : (void*)THREAD_RETURN_FAILURE;
    if (thread->cleanup) thread->cleanup(thread->entry_data);

    thread->state = THREAD_STATE_DONE;
    return return_val;
}

bool thread_start(Thread* thread, void* data) {
    if (thread->state != THREAD_STATE_NOT_RUNNING) return false;

    thread->entry_data = data;
    thread->state = THREAD_STATE_STARTING;
    if (pthread_create(&thread->handle, NULL, thread_entry, thread)) {
        thread->state = THREAD_STATE_NOT_RUNNING;
        return false;
    }

    return true;
}

bool thread_is_running(Thread* thread) {
    return thread->state != THREAD_STATE_NOT_RUNNING;
}

void thread_handle_stopping_state(Thread* thread) {
    if (thread->state != THREAD_STATE_STOPPING) return;
    if (thread->cleanup) thread->cleanup(thread->entry_data);
    thread->state = THREAD_STATE_DONE;
    pthread_exit((void*)THREAD_RETURN_STOPPED);
}

void thread_exit(Thread* thread, bool success) {
    thread_handle_stopping_state(thread);

    if (thread->state != THREAD_STATE_RUNNING) return;
    if (thread->cleanup) thread->cleanup(thread->entry_data);
    thread->state = THREAD_STATE_DONE;
    pthread_exit(success ? (void*)THREAD_RETURN_SUCCESS : (void*)THREAD_RETURN_FAILURE);
}

bool thread_stop(Thread* thread) {
    if (thread->state != THREAD_STATE_RUNNING) return false;
    thread->state = THREAD_STATE_STOPPING;
    return true;
}

ThreadReturnCode thread_join(Thread* thread) {
    if (thread->state == THREAD_STATE_NOT_RUNNING) return THREAD_RETURN_FAILURE;

    void* return_val;
    if (pthread_join(thread->handle, &return_val)) return THREAD_RETURN_FAILURE;
    thread->state = THREAD_STATE_NOT_RUNNING;

    ThreadReturnCode thread_return = (ThreadReturnCode)return_val;
    switch (thread_return) {
    case THREAD_RETURN_SUCCESS:
    case THREAD_RETURN_STOPPED:
        return thread_return;
    default:
        return THREAD_RETURN_FAILURE;
    }
}

ThreadReturnCode thread_try_join(Thread* thread) {
    if (thread->state != THREAD_STATE_DONE) return THREAD_RETURN_RUNNING;
    return thread_join(thread);
}
