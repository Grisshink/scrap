#include <stddef.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#define VM_ARG_STACK_SIZE 1024
#define VM_CONTROL_STACK_SIZE 1024
#define VM_VARIABLE_STACK_SIZE 1024

typedef struct {
    char* str;
    size_t len;
    size_t cap;
} ScrString;

typedef struct {
    float x, y;
} ScrVec;

typedef struct {
    unsigned char r, g, b, a;
} ScrColor;

typedef struct {
    ScrVec size;
} ScrMeasurement;

typedef enum {
    BLOCKCONSTR_UNLIMITED, // Can put anything as argument
    BLOCKCONSTR_STRING, // Can only put strings as argument
} ScrBlockArgumentConstraint;

typedef enum {
    DROPDOWN_SOURCE_LISTREF,
} ScrBlockDropdownSource;

typedef struct {
    ScrMeasurement ms;
    char* text;
} ScrInputStaticText;

typedef struct {
    ScrBlockArgumentConstraint constr;
    ScrMeasurement ms;
    char* text;
} ScrInputArgument;

typedef struct {
    void* image_ptr;
} ScrImage;

typedef struct {
    ScrMeasurement ms;
    ScrImage image;
} ScrInputStaticImage;

typedef enum {
    INPUT_TEXT_DISPLAY,
    INPUT_ARGUMENT,
    INPUT_DROPDOWN,
    INPUT_IMAGE_DISPLAY,
} ScrBlockInputType;

typedef enum {
    BLOCKTYPE_NORMAL,
    BLOCKTYPE_CONTROL,
    BLOCKTYPE_CONTROLEND,
    BLOCKTYPE_END,
    BLOCKTYPE_HAT,
} ScrBlockType;

struct ScrBlockArgument;

typedef struct ScrBlock {
    size_t id;
    struct ScrBlockArgument* arguments;
    ScrMeasurement ms;
    struct ScrBlock* parent;
} ScrBlock;

typedef char** (*ScrListAccessor)(ScrBlock* block, size_t* list_len);

typedef struct {
    ScrBlockDropdownSource source;
    ScrListAccessor list;
} ScrInputDropdown;

typedef union {
    ScrInputStaticText stext;
    ScrInputStaticImage simage;
    ScrInputArgument arg;
    ScrInputDropdown drop;
} ScrBlockInputData;

typedef struct {
    ScrBlockInputType type;
    ScrBlockInputData data;
} ScrBlockInput;

typedef enum {
    CONTROL_ARG_BEGIN,
    CONTROL_ARG_END,
} ScrControlArgType;

typedef enum {
    FUNC_ARG_INT,
    FUNC_ARG_STATIC_STR, // String is borrowed from static or immutable memory
    FUNC_ARG_MANAGED_STR, // String that was allocated by argument temporarily and will be cleared by vm automatically
    FUNC_ARG_UNMANAGED_STR, // String that was previously allocated in chain and it's owned by different entity (e.g. variable)
    FUNC_ARG_BOOL,
    FUNC_ARG_CONTROL,
    FUNC_ARG_NOTHING,
    FUNC_ARG_OMIT_ARGS, // Marker for vm used in C-blocks that do not require argument recomputation
} ScrFuncArgType;

typedef union {
    int int_arg;
    const char* str_arg;
    ScrControlArgType control_arg;
} ScrFuncArgData;

typedef struct {
    ScrFuncArgType type;
    ScrFuncArgData data;
} ScrFuncArg;

typedef struct ScrExec ScrExec;

typedef ScrFuncArg (*ScrBlockFunc)(ScrExec* exec, int argc, ScrFuncArg* argv);

typedef struct {
    const char* id;
    ScrColor color;
    ScrBlockType type;
    bool hidden;
    ScrBlockInput* inputs;
    ScrBlockFunc func;
} ScrBlockdef;

typedef enum {
    ARGUMENT_TEXT,
    ARGUMENT_BLOCK,
    ARGUMENT_CONST_STRING,
} ScrBlockArgumentType;

typedef union {
    char* text;
    ScrBlock block;
} ScrBlockArgumentData;

typedef struct ScrBlockArgument {
    ScrMeasurement ms;
    size_t input_id;
    ScrBlockArgumentType type;
    ScrBlockArgumentData data;
} ScrBlockArgument;

typedef struct {
    ScrVec pos;
    ScrBlock* blocks;
} ScrBlockChain;

typedef ScrMeasurement (*ScrTextMeasureFunc)(char* text);
typedef ScrMeasurement (*ScrTextArgMeasureFunc)(char* text);
typedef ScrMeasurement (*ScrImageMeasureFunc)(ScrImage image);

typedef struct {
    const char* name;
    ScrFuncArg value;
    int layer;
} ScrVariable;

typedef struct ScrVm ScrVm;

struct ScrExec {
    ScrBlockChain* code;
    ScrFuncArg arg_stack[VM_ARG_STACK_SIZE];
    size_t arg_stack_len;
    unsigned char control_stack[VM_CONTROL_STACK_SIZE];
    size_t control_stack_len;
    ScrVariable variable_stack[VM_VARIABLE_STACK_SIZE];
    size_t variable_stack_len;
    pthread_t thread;
    atomic_bool is_running;
    bool skip_block;
    size_t running_chain_ind;
    size_t running_ind;
    int layer;
    ScrVm* vm;
};

struct ScrVm {
    ScrBlockdef* blockdefs;
    size_t end_block_id;
    bool is_running;
    ScrTextMeasureFunc text_measure;
    ScrTextArgMeasureFunc arg_measure;
    ScrImageMeasureFunc img_measure;
};

// Public macros
#define RETURN_NOTHING return (ScrFuncArg) { \
    .type = FUNC_ARG_NOTHING, \
    .data = (ScrFuncArgData) {0}, \
}

#define RETURN_OMIT_ARGS return (ScrFuncArg) { \
    .type = FUNC_ARG_OMIT_ARGS, \
    .data = (ScrFuncArgData) {0}, \
}

#define RETURN_INT(val) return (ScrFuncArg) { \
    .type = FUNC_ARG_INT, \
    .data = (ScrFuncArgData) { \
        .int_arg = (val) \
    }, \
}

#define RETURN_BOOL(val) return (ScrFuncArg) { \
    .type = FUNC_ARG_BOOL, \
    .data = (ScrFuncArgData) { \
        .int_arg = (val) \
    }, \
}

#define control_stack_push_data(data, type) \
    if (exec->control_stack_len + sizeof(type) > VM_CONTROL_STACK_SIZE) { \
        printf("[VM] CRITICAL: Control stack overflow\n"); \
        pthread_exit((void*)0); \
    } \
    *(type *)(exec->control_stack + exec->control_stack_len) = (data); \
    exec->control_stack_len += sizeof(type);

#define control_stack_pop_data(data, type) \
    if (sizeof(type) > exec->control_stack_len) { \
        printf("[VM] CRITICAL: Control stack underflow\n"); \
        pthread_exit((void*)0); \
    } \
    exec->control_stack_len -= sizeof(type); \
    data = *(type*)(exec->control_stack + exec->control_stack_len);

// Public functions
ScrVm vm_new(ScrTextMeasureFunc text_measure, ScrTextArgMeasureFunc arg_measure, ScrImageMeasureFunc img_measure);
void vm_free(ScrVm* vm);

ScrExec exec_new(ScrVm* vm);
void exec_free(ScrExec* exec);
void exec_add_chain(ScrVm* vm, ScrExec* exec, ScrBlockChain chain);
void exec_remove_chain(ScrVm* vm, ScrExec* exec, size_t ind);
bool exec_run_chain(ScrVm* vm, ScrExec* exec, ScrBlockChain chain);
bool exec_start(ScrVm* vm, ScrExec* exec);
bool exec_stop(ScrVm* vm, ScrExec* exec);
bool exec_join(ScrVm* vm, ScrExec* exec, size_t* return_code);
bool exec_try_join(ScrVm* vm, ScrExec* exec, size_t* return_code);

bool variable_stack_push_var(ScrExec* exec, const char* name, ScrFuncArg arg);
ScrVariable* variable_stack_get_variable(ScrExec* exec, const char* name);

int func_arg_to_int(ScrFuncArg arg);

size_t block_register(ScrVm* vm, const char* id, ScrBlockType type, ScrColor color, ScrBlockFunc func);
void block_add_text(ScrVm* vm, size_t block_id, char* text);
void block_add_argument(ScrVm* vm, size_t block_id, char* defualt_data, ScrBlockArgumentConstraint constraint);
void block_add_dropdown(ScrVm* vm, size_t block_id, ScrBlockDropdownSource dropdown_source, ScrListAccessor accessor);
void block_add_image(ScrVm* vm, size_t block_id, ScrImage image);
void block_unregister(ScrVm* vm, size_t block_id);

ScrBlockChain blockchain_new(void);
ScrBlockChain blockchain_copy(ScrBlockChain* chain);
void blockchain_add_block(ScrBlockChain* chain, ScrBlock block);
void blockchain_clear_blocks(ScrBlockChain* chain);
void blockchain_insert(ScrBlockChain* dst, ScrBlockChain* src, size_t pos);
// Splits off blockchain src in two at specified pos, placing lower half into blockchain dst
void blockchain_detach(ScrVm* vm, ScrBlockChain* dst, ScrBlockChain* src, size_t pos);
void blockchain_free(ScrBlockChain* chain);

ScrBlock block_new(ScrVm* vm, size_t id);
ScrBlock block_copy(ScrBlock* block, ScrBlock* parent);
void block_free(ScrBlock* block);

void argument_set_block(ScrBlockArgument* block_arg, ScrBlock block);
void argument_set_const_string(ScrBlockArgument* block_arg, char* text);
void argument_set_text(ScrBlockArgument* block_arg, char* text);

#ifdef SCRVM_IMPLEMENTATION

////////////////////////////////////////////////////////////////////
//                           BEGIN vec.h                          //
////////////////////////////////////////////////////////////////////

/*
BSD 3-Clause License

Copyright (c) 2024, Mashpoe
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef vec_h
#define vec_h

#ifdef __cpp_decltype
#include <type_traits>
#define typeof(T) std::remove_reference<std::add_lvalue_reference<decltype(T)>::type>::type
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

// generic type for internal use
typedef void* vector;
// number of elements in a vector
typedef size_t vec_size_t;
// number of bytes for a type
typedef size_t vec_type_t;

// TODO: more rigorous check for typeof support with different compilers
#if _MSC_VER == 0 || __STDC_VERSION__ >= 202311L || defined __cpp_decltype

// shortcut defines

// vec_addr is a vector* (aka type**)
#define vector_add_dst(vec_addr)\
	((typeof(*vec_addr))(\
	    _vector_add_dst((vector*)vec_addr, sizeof(**vec_addr))\
	))
#define vector_insert_dst(vec_addr, pos)\
	((typeof(*vec_addr))(\
	    _vector_insert_dst((vector*)vec_addr, sizeof(**vec_addr), pos)))

#define vector_add(vec_addr, value)\
	(*vector_add_dst(vec_addr) = value)
#define vector_insert(vec_addr, pos, value)\
	(*vector_insert_dst(vec_addr, pos) = value)

#else

#define vector_add_dst(vec_addr, type)\
	((type*)_vector_add_dst((vector*)vec_addr, sizeof(type)))
#define vector_insert_dst(vec_addr, type, pos)\
	((type*)_vector_insert_dst((vector*)vec_addr, sizeof(type), pos))

#define vector_add(vec_addr, type, value)\
	(*vector_add_dst(vec_addr, type) = value)
#define vector_insert(vec_addr, type, pos, value)\
	(*vector_insert_dst(vec_addr, type, pos) = value)

#endif

// vec is a vector (aka type*)
#define vector_erase(vec, pos, len)\
	(_vector_erase((vector)vec, sizeof(*vec), pos, len))
#define vector_remove(vec, pos)\
	(_vector_remove((vector)vec, sizeof(*vec), pos))

#define vector_reserve(vec_addr, capacity)\
	(_vector_reserve((vector*)vec_addr, sizeof(**vec_addr), capacity))

#define vector_copy(vec)\
	(_vector_copy((vector)vec, sizeof(*vec)))

vector vector_create(void);

void vector_free(vector vec);

void* _vector_add_dst(vector* vec_addr, vec_type_t type_size);

void* _vector_insert_dst(vector* vec_addr, vec_type_t type_size, vec_size_t pos);

void _vector_erase(vector vec_addr, vec_type_t type_size, vec_size_t pos, vec_size_t len);

void _vector_remove(vector vec_addr, vec_type_t type_size, vec_size_t pos);

void vector_pop(vector vec);

void vector_clear(vector vec);

void _vector_reserve(vector* vec_addr, vec_type_t type_size, vec_size_t capacity);

vector _vector_copy(vector vec, vec_type_t type_size);

vec_size_t vector_size(vector vec);

vec_size_t vector_capacity(vector vec);

// closing bracket for extern "C"
#ifdef __cplusplus
}
#endif

#endif /* vec_h */

////////////////////////////////////////////////////////////////////
//                            END vec.h                           //
////////////////////////////////////////////////////////////////////

#ifdef SCRVM_VEC_C
////////////////////////////////////////////////////////////////////
//                           BEGIN vec.c                          //
////////////////////////////////////////////////////////////////////
/*
BSD 3-Clause License

Copyright (c) 2024, Mashpoe
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>

typedef struct
{
	vec_size_t size;
	vec_size_t capacity;
	unsigned char data[]; 
} vector_header;

vector_header* vector_get_header(vector vec) { return &((vector_header*)vec)[-1]; }

vector vector_create(void)
{
	vector_header* h = (vector_header*)malloc(sizeof(vector_header));
	h->capacity = 0;
	h->size = 0;

	return &h->data;
}

void vector_free(vector vec) { free(vector_get_header(vec)); }

vec_size_t vector_size(vector vec) { return vector_get_header(vec)->size; }

vec_size_t vector_capacity(vector vec) { return vector_get_header(vec)->capacity; }

vector_header* vector_realloc(vector_header* h, vec_type_t type_size)
{
	vec_size_t new_capacity = (h->capacity == 0) ? 1 : h->capacity * 2;
	vector_header* new_h = (vector_header*)realloc(h, sizeof(vector_header) + new_capacity * type_size);
	new_h->capacity = new_capacity;

	return new_h;
}

bool vector_has_space(vector_header* h)
{
	return h->capacity - h->size > 0;
}

void* _vector_add_dst(vector* vec_addr, vec_type_t type_size)
{
	vector_header* h = vector_get_header(*vec_addr);

	if (!vector_has_space(h))
	{
		h = vector_realloc(h, type_size);
		*vec_addr = h->data;
	}

	return &h->data[type_size * h->size++];
}

void* _vector_insert_dst(vector* vec_addr, vec_type_t type_size, vec_size_t pos)
{
	vector_header* h = vector_get_header(*vec_addr);

	vec_size_t new_length = h->size + 1;

	// make sure there is enough room for the new element
	if (!vector_has_space(h))
	{
		h = vector_realloc(h, type_size);
		*vec_addr = h->data;
	}
	// move trailing elements
	memmove(&h->data[(pos + 1) * type_size],
		&h->data[pos * type_size],
		(h->size - pos) * type_size);

	h->size = new_length;

	return &h->data[pos * type_size];
}

void _vector_erase(vector vec, vec_type_t type_size, vec_size_t pos, vec_size_t len)
{
	vector_header* h = vector_get_header(vec);
	memmove(&h->data[pos * type_size],
		&h->data[(pos + len) * type_size],
		(h->size - pos - len) * type_size);

	h->size -= len;
}

void _vector_remove(vector vec, vec_type_t type_size, vec_size_t pos)
{
	_vector_erase(vec, type_size, pos, 1);
}

void vector_pop(vector vec) { --vector_get_header(vec)->size; }

void vector_clear(vector vec) { vector_get_header(vec)->size = 0; }

void _vector_reserve(vector* vec_addr, vec_type_t type_size, vec_size_t capacity)
{
	vector_header* h = vector_get_header(*vec_addr);
	if (h->capacity >= capacity)
	{
		return;
	}

	h = (vector_header*)realloc(h, sizeof(vector_header) + capacity * type_size);
	h->capacity = capacity;
	*vec_addr = &h->data;
}

vector _vector_copy(vector vec, vec_type_t type_size)
{
	vector_header* h = vector_get_header(vec);
	size_t alloc_size = sizeof(vector_header) + h->size * type_size;
	vector_header* copy_h = (vector_header*)malloc(alloc_size);
	memcpy(copy_h, h, alloc_size);
	copy_h->capacity = copy_h->size;

	return &copy_h->data;
}

////////////////////////////////////////////////////////////////////
//                            END vec.c                           //
////////////////////////////////////////////////////////////////////
#endif /* SCRVM_VEC_C */

#include <assert.h>

// Private functions
void block_update_parent_links(ScrBlock* block);
void blockchain_update_parent_links(ScrBlockChain* chain);
bool arg_stack_push_arg(ScrExec* exec, ScrFuncArg arg);
bool arg_stack_undo_args(ScrExec* exec, size_t count);
void variable_stack_pop_layer(ScrExec* exec);
void variable_stack_cleanup(ScrExec* exec);

ScrVm vm_new(ScrTextMeasureFunc text_measure, ScrTextArgMeasureFunc arg_measure, ScrImageMeasureFunc img_measure) {
    ScrVm vm = (ScrVm) {
        .blockdefs = vector_create(),
        .end_block_id = (size_t)-1,
        .is_running = false,
        .text_measure = text_measure,
        .arg_measure = arg_measure,
        .img_measure = img_measure,
    };
    return vm;
}

void vm_free(ScrVm* vm) {
    for (ssize_t i = (ssize_t)vector_size(vm->blockdefs) - 1; i >= 0 ; i--) {
        block_unregister(vm, i);
    }
    vector_free(vm->blockdefs);
}

ScrExec exec_new(ScrVm* vm) {
    ScrExec exec = (ScrExec) {
        .code = vector_create(),
        .arg_stack_len = 0,
        .control_stack_len = 0,
        .running_ind = 0,
        .layer = 0,
        .thread = (pthread_t) {0},
        .is_running = false,
        .skip_block = false,
        .vm = vm,
    };
    return exec;
}

void exec_free(ScrExec* exec) {
    for (vec_size_t i = 0; i < vector_size(exec->code); i++) {
        blockchain_free(&exec->code[i]);
    }
    vector_free(exec->code);
}

void exec_copy_code(ScrVm* vm, ScrExec* exec, ScrBlockChain* code) {
    if (vm->is_running) return;
    for (vec_size_t i = 0; i < vector_size(code); i++) {
        exec_add_chain(vm, exec, blockchain_copy(&code[i]));
    }
}

void exec_add_chain(ScrVm* vm, ScrExec* exec, ScrBlockChain chain) {
    if (vm->is_running) return;
    vector_add(&exec->code, chain);
}

void exec_remove_chain(ScrVm* vm, ScrExec* exec, size_t ind) {
    if (vm->is_running) return;
    vector_remove(exec->code, ind);
}

bool exec_block(ScrVm* vm, ScrExec* exec, ScrBlock block, ScrFuncArg* block_return, bool from_end, bool omit_args, ScrFuncArg control_arg) {
    ScrBlockdef blockdef = vm->blockdefs[block.id];
    ScrBlockFunc execute_block = blockdef.func;
    if (!execute_block) return false;

    int stack_begin = exec->arg_stack_len;
    if (blockdef.type == BLOCKTYPE_CONTROL || blockdef.type == BLOCKTYPE_CONTROLEND) {
        arg_stack_push_arg(exec, (ScrFuncArg) {
            .type = FUNC_ARG_CONTROL,
            .data = (ScrFuncArgData) {
                .control_arg = from_end ? CONTROL_ARG_END : CONTROL_ARG_BEGIN,
            },
        });
        if (!from_end && blockdef.type == BLOCKTYPE_CONTROLEND) {
            arg_stack_push_arg(exec, control_arg);
        }
    }
    if (!omit_args) {
        for (vec_size_t i = 0; i < vector_size(block.arguments); i++) {
            ScrBlockArgument block_arg = block.arguments[i];
            switch (block_arg.type) {
            case ARGUMENT_TEXT:
            case ARGUMENT_CONST_STRING:
                arg_stack_push_arg(exec, (ScrFuncArg) {
                    .type = FUNC_ARG_STATIC_STR,
                    .data = (ScrFuncArgData) {
                        .str_arg = block_arg.data.text,
                    },
                });
                break;
            case ARGUMENT_BLOCK:
                ScrFuncArg arg_return;
                if (!exec_block(vm, exec, block_arg.data.block, &arg_return, false, false, (ScrFuncArg) {0})) return false;
                arg_stack_push_arg(exec, arg_return);
                break;
            default:
                return false;
            }
        }
    }

    *block_return = execute_block(exec, exec->arg_stack_len - stack_begin, exec->arg_stack + stack_begin);
    arg_stack_undo_args(exec, exec->arg_stack_len - stack_begin);

    return true;
}

#define BLOCKDEF vm->blockdefs[chain.blocks[exec->running_ind].id]
bool exec_run_chain(ScrVm* vm, ScrExec* exec, ScrBlockChain chain) {
    int skip_layer = -1;
    exec->skip_block = false;
    exec->layer = 0;
    ScrFuncArg block_return;
    for (exec->running_ind = 0; exec->running_ind < vector_size(chain.blocks); exec->running_ind++) {
        pthread_testcancel();
        size_t block_ind = exec->running_ind;
        bool from_end = false;
        bool omit_args = false;
        bool return_used = false;

        if (BLOCKDEF.type == BLOCKTYPE_END || BLOCKDEF.type == BLOCKTYPE_CONTROLEND) {
            if (BLOCKDEF.type == BLOCKTYPE_CONTROLEND && exec->layer == 0) continue;
            variable_stack_pop_layer(exec);
            exec->layer--;
            control_stack_pop_data(block_ind, size_t)
            control_stack_pop_data(block_return, ScrFuncArg)
            if (block_return.type == FUNC_ARG_OMIT_ARGS) omit_args = true;
            if (block_return.type == FUNC_ARG_MANAGED_STR) free((char*)block_return.data.str_arg);
            from_end = true;
            if (exec->skip_block && skip_layer == exec->layer) {
                exec->skip_block = false;
                skip_layer = -1;
            }
        }
        if (!exec->skip_block) {
            if (!exec_block(vm, exec, chain.blocks[block_ind], &block_return, from_end, omit_args, (ScrFuncArg){0})) return false;
        }
        if (BLOCKDEF.type == BLOCKTYPE_CONTROLEND && block_ind != exec->running_ind) {
            from_end = false;
            if (!exec_block(vm, exec, chain.blocks[exec->running_ind], &block_return, from_end, false, block_return)) return false;
            return_used = true;
        }
        if (BLOCKDEF.type == BLOCKTYPE_CONTROL || BLOCKDEF.type == BLOCKTYPE_CONTROLEND) {
            control_stack_push_data(block_return, ScrFuncArg)
            control_stack_push_data(exec->running_ind, size_t)
            if (exec->skip_block && skip_layer == -1) skip_layer = exec->layer;
            return_used = true;
            exec->layer++;
        } 
        if (!return_used && block_return.type == FUNC_ARG_MANAGED_STR) {
            free((char*)block_return.data.str_arg);
        }
    }
    return true;
}
#undef BLOCKDEF

void exec_thread_exit(void* thread_exec) {
    ScrExec* exec = thread_exec;
    variable_stack_cleanup(exec);
    arg_stack_undo_args(exec, exec->arg_stack_len);
    exec->is_running = false;
}

void* exec_thread_entry(void* thread_exec) {
    ScrExec* exec = thread_exec;
    pthread_cleanup_push(exec_thread_exit, thread_exec);

    exec->is_running = true;
    exec->arg_stack_len = 0;
    exec->control_stack_len = 0;
    for (exec->running_chain_ind = 0; exec->running_chain_ind < vector_size(exec->code); exec->running_chain_ind++) {
        if (exec->vm->blockdefs[exec->code[exec->running_chain_ind].blocks[0].id].type != BLOCKTYPE_HAT) continue;
        if (!exec_run_chain(exec->vm, exec, exec->code[exec->running_chain_ind])) {
            pthread_exit((void*)0);
        }
    }

    pthread_cleanup_pop(1);
    pthread_exit((void*)1);
}

bool exec_start(ScrVm* vm, ScrExec* exec) {
    if (vm->is_running) return false;
    if (exec->is_running) return false;
    vm->is_running = true;

    if (pthread_create(&exec->thread, NULL, exec_thread_entry, exec)) return false;
    exec->is_running = true;
    return true;
}

bool exec_stop(ScrVm* vm, ScrExec* exec) {
    if (!vm->is_running) return false;
    if (!exec->is_running) return false;
    if (pthread_cancel(exec->thread)) return false;
    return true;
}

bool exec_join(ScrVm* vm, ScrExec* exec, size_t* return_code) {
    if (!vm->is_running) return false;
    if (!exec->is_running) return false;
    
    void* return_val;
    if (pthread_join(exec->thread, &return_val)) return false;
    vm->is_running = false;
    *return_code = (size_t)return_val;
    return true;
}

bool exec_try_join(ScrVm* vm, ScrExec* exec, size_t* return_code) {
    if (!vm->is_running) return false;
    if (exec->is_running) return false;

    void* return_val;
    if (pthread_join(exec->thread, &return_val)) return false;
    vm->is_running = false;
    *return_code = (size_t)return_val;
    return true;
}

bool variable_stack_push_var(ScrExec* exec, const char* name, ScrFuncArg arg) {
    if (exec->variable_stack_len >= VM_VARIABLE_STACK_SIZE) return false;
    if (*name == 0) return false;
    ScrVariable var;
    var.name = name;
    var.value = arg;
    var.layer = exec->layer;
    exec->variable_stack[exec->variable_stack_len++] = var;
    return true;
}

void variable_stack_pop_layer(ScrExec* exec) {
    size_t count = 0;
    for (int i = exec->variable_stack_len - 1; i >= 0 && exec->variable_stack[i].layer == exec->layer; i--) {
        ScrFuncArg arg = exec->variable_stack[i].value;
        if (arg.type == FUNC_ARG_UNMANAGED_STR) {
            free((char*)arg.data.str_arg);
        }
        count++;
    }
    exec->variable_stack_len -= count;
}

void variable_stack_cleanup(ScrExec* exec) {
    for (size_t i = 0; i < exec->variable_stack_len; i++) {
        ScrFuncArg arg = exec->variable_stack[i].value;
        if (arg.type == FUNC_ARG_UNMANAGED_STR) {
            free((char*)arg.data.str_arg);
        }
    }
    exec->variable_stack_len = 0;
}

ScrVariable* variable_stack_get_variable(ScrExec* exec, const char* name) {
    for (int i = exec->variable_stack_len - 1; i >= 0; i--) {
        if (!strcmp(exec->variable_stack[i].name, name)) return &exec->variable_stack[i];
    }
    return NULL;
}

bool arg_stack_push_arg(ScrExec* exec, ScrFuncArg arg) {
    if (exec->arg_stack_len >= VM_ARG_STACK_SIZE) return false;
    exec->arg_stack[exec->arg_stack_len++] = arg;
    return true;
}

bool arg_stack_undo_args(ScrExec* exec, size_t count) {
    if (count > exec->arg_stack_len) return false;
    for (size_t i = 0; i < count; i++) {
        ScrFuncArg arg = exec->arg_stack[exec->arg_stack_len - 1 - i];
        if (arg.type != FUNC_ARG_MANAGED_STR) continue;
        free((char*)arg.data.str_arg);
    }
    exec->arg_stack_len -= count;
    return true;
}

int func_arg_to_int(ScrFuncArg arg) {
    switch (arg.type) {
    case FUNC_ARG_BOOL:
    case FUNC_ARG_INT:
        return arg.data.int_arg;
    case FUNC_ARG_MANAGED_STR:
    case FUNC_ARG_UNMANAGED_STR:
    case FUNC_ARG_STATIC_STR:
        return atoi(arg.data.str_arg);
    default: return 0;
    }
}

int func_arg_to_bool(ScrFuncArg arg) {
    switch (arg.type) {
    case FUNC_ARG_BOOL:
    case FUNC_ARG_INT:
        return arg.data.int_arg != 0;
    case FUNC_ARG_MANAGED_STR:
    case FUNC_ARG_UNMANAGED_STR:
    case FUNC_ARG_STATIC_STR:
        return *arg.data.str_arg != 0;
    default: return 0;
    }
}

ScrString string_new(size_t cap) {
    ScrString string;
    string.str = malloc((cap + 1)* sizeof(char));
    *string.str = 0;
    string.len = 0;
    string.cap = cap;
    return string;
}

void string_add(ScrString* string, const char* other) {
    size_t new_len = string->len + strlen(other);
    if (new_len > string->cap) {
        string->str = realloc(string->str, (new_len + 1) * sizeof(char));
        string->cap = new_len;
    }
    strcat(string->str, other);
    string->len = new_len;
}

void string_free(ScrString string) {
    free(string.str);
}

ScrBlock block_new(ScrVm* vm, size_t id) {
    ScrBlock block;
    block.id = id;
    block.ms = (ScrMeasurement) {0};
    block.arguments = vector_create();
    block.parent = NULL;

    ScrBlockdef blockdef = vm->blockdefs[block.id];
    for (size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        if (blockdef.inputs[i].type != INPUT_ARGUMENT && blockdef.inputs[i].type != INPUT_DROPDOWN) continue;
        ScrBlockArgument* arg = vector_add_dst((ScrBlockArgument**)&block.arguments);
        arg->data.text = vector_create();
        arg->input_id = i;

        switch (blockdef.inputs[i].type) {
        case INPUT_ARGUMENT:
            arg->ms = blockdef.inputs[i].data.arg.ms;
            switch (blockdef.inputs[i].data.arg.constr) {
            case BLOCKCONSTR_UNLIMITED:
                arg->type = ARGUMENT_TEXT;
                break;
            case BLOCKCONSTR_STRING:
                arg->type = ARGUMENT_CONST_STRING;
                break;
            default:
                assert(false && "Unimplemented argument constraint");
                break;
            }

            for (char* pos = blockdef.inputs[i].data.arg.text; *pos; pos++) {
                vector_add(&arg->data.text, *pos);
            }
            break;
        case INPUT_DROPDOWN:
            arg->type = ARGUMENT_CONST_STRING;
            arg->ms = (ScrMeasurement) {0};

            size_t list_len = 0;
            char** list = blockdef.inputs[i].data.drop.list(&block, &list_len);
            if (!list || list_len == 0) break;

            for (char* pos = list[0]; *pos; pos++) {
                vector_add(&arg->data.text, *pos);
            }
            break;
        default:
            assert(false && "Unreachable");
            break;
        }
        vector_add(&arg->data.text, 0);
    }
    return block;
}

// Broken at the moment, not sure why
ScrBlock block_copy(ScrBlock* block, ScrBlock* parent) {
    if (!block->arguments) return *block;

    ScrBlock new;
    new.id = block->id;
    new.ms = block->ms;
    new.parent = parent;
    new.arguments = vector_create();

    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        ScrBlockArgument* arg = vector_add_dst((ScrBlockArgument**)&new.arguments);
        arg->ms = block->arguments[i].ms;
        arg->type = block->arguments[i].type;
        switch (block->arguments[i].type) {
        case ARGUMENT_CONST_STRING:
        case ARGUMENT_TEXT:
            arg->data.text = vector_copy(block->arguments[i].data.text);
            break;
        case ARGUMENT_BLOCK:
            arg->data.block = block_copy(&block->arguments[i].data.block, &new);
            break;
        default:
            assert(false && "Unimplemented argument copy");
            break;
        }
    }

    return new;
}

void block_free(ScrBlock* block) {
    if (block->arguments) {
        for (size_t i = 0; i < vector_size(block->arguments); i++) {
            switch (block->arguments[i].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                vector_free(block->arguments[i].data.text);
                break;
            case ARGUMENT_BLOCK:
                block_free(&block->arguments[i].data.block);
                break;
            default:
                assert(false && "Unimplemented argument free");
                break;
            }
        }
        vector_free((ScrBlockArgument*)block->arguments);
    }
}

void block_update_parent_links(ScrBlock* block) {
    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        if (block->arguments[i].type != ARGUMENT_BLOCK) continue;
        block->arguments[i].data.block.parent = block;
    }
}

ScrBlockChain blockchain_new(void) {
    ScrBlockChain chain;
    chain.pos = (ScrVec) {0};
    chain.blocks = vector_create();

    return chain;
}

ScrBlockChain blockchain_copy(ScrBlockChain* chain) {
    ScrBlockChain new;
    new.pos = chain->pos;
    new.blocks = vector_create();

    vector_reserve(&new.blocks, vector_size(chain->blocks));
    for (vec_size_t i = 0; i < vector_size(chain->blocks); i++) {
        vector_add(&new.blocks, block_copy(&chain->blocks[i], NULL));
    }

    return new;
}

void blockchain_update_parent_links(ScrBlockChain* chain) {
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        block_update_parent_links(&chain->blocks[i]);
    }
}

void blockchain_add_block(ScrBlockChain* chain, ScrBlock block) {
    vector_add(&chain->blocks, block);
    blockchain_update_parent_links(chain);
}

void blockchain_clear_blocks(ScrBlockChain* chain) {
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        block_free(&chain->blocks[i]);
    }
    vector_clear(chain->blocks);
}

void blockchain_insert(ScrBlockChain* dst, ScrBlockChain* src, size_t pos) {
    assert(pos < vector_size(dst->blocks));

    vector_reserve(&dst->blocks, vector_size(dst->blocks) + vector_size(src->blocks));
    for (ssize_t i = (ssize_t)vector_size(src->blocks) - 1; i >= 0; i--) {
        vector_insert(&dst->blocks, pos + 1, src->blocks[i]);
    }
    blockchain_update_parent_links(dst);
    vector_clear(src->blocks);
}

void blockchain_detach(ScrVm* vm, ScrBlockChain* dst, ScrBlockChain* src, size_t pos) {
    assert(pos < vector_size(src->blocks));

    int pos_layer = 0;
    for (size_t i = 0; i < pos; i++) {
        ScrBlockType block_type = vm->blockdefs[src->blocks[i].id].type;
        if (block_type == BLOCKTYPE_CONTROL) {
            pos_layer++;
        } else if (block_type == BLOCKTYPE_END) {
            pos_layer--;
            if (pos_layer < 0) pos_layer = 0;
        }
    }

    int current_layer = pos_layer;
    int layer_size = 0;

    vector_reserve(&dst->blocks, vector_size(dst->blocks) + vector_size(src->blocks) - pos);
    for (size_t i = pos; i < vector_size(src->blocks); i++) {
        ScrBlockType block_type = vm->blockdefs[src->blocks[i].id].type;
        if ((block_type == BLOCKTYPE_END || (block_type == BLOCKTYPE_CONTROLEND && i != pos)) && pos_layer == current_layer && current_layer != 0) break;
        vector_add(&dst->blocks, src->blocks[i]);
        if (block_type == BLOCKTYPE_CONTROL) {
            current_layer++;
        } else if (block_type == BLOCKTYPE_END) {
            current_layer--;
        }
        layer_size++;
    }
    blockchain_update_parent_links(dst);
    vector_erase(src->blocks, pos, layer_size);
    blockchain_update_parent_links(src);
}

void blockchain_free(ScrBlockChain* chain) {
    blockchain_clear_blocks(chain);
    vector_free(chain->blocks);
}

void argument_set_block(ScrBlockArgument* block_arg, ScrBlock block) {
    assert(block_arg->type == ARGUMENT_TEXT);

    vector_free(block_arg->data.text);
    block_arg->type = ARGUMENT_BLOCK;
    block_arg->data.block = block;

    block_update_parent_links(&block_arg->data.block);
}

void argument_set_const_string(ScrBlockArgument* block_arg, char* text) {
    assert(block_arg->type == ARGUMENT_CONST_STRING);

    block_arg->type = ARGUMENT_CONST_STRING;
    vector_clear(block_arg->data.text);

    for (char* pos = text; *pos; pos++) {
        vector_add(&block_arg->data.text, *pos);
    }
    vector_add(&block_arg->data.text, 0);
}

void argument_set_text(ScrBlockArgument* block_arg, char* text) {
    assert(block_arg->type == ARGUMENT_BLOCK);
    assert(block_arg->data.block.parent != NULL);

    block_arg->type = ARGUMENT_TEXT;
    block_arg->data.text = vector_create();

    for (char* pos = text; *pos; pos++) {
        vector_add(&block_arg->data.text, *pos);
    }
    vector_add(&block_arg->data.text, 0);
}

size_t block_register(ScrVm* vm, const char* id, ScrBlockType type, ScrColor color, ScrBlockFunc func) {
    ScrBlockdef* blockdef = vector_add_dst(&vm->blockdefs);
    blockdef->id = id;
    blockdef->color = color;
    blockdef->type = type;
    blockdef->hidden = false;
    blockdef->inputs = vector_create();
    blockdef->func = func;

    if (!func) printf("[VM] WARNING: Block \"%s\" has not defined its implementation!\n", id);

    if (type == BLOCKTYPE_END && vm->end_block_id == (size_t)-1) {
        vm->end_block_id = vector_size(vm->blockdefs) - 1;
        blockdef->hidden = true;
    }

    return vector_size(vm->blockdefs) - 1;
}

void block_add_text(ScrVm* vm, size_t block_id, char* text) {
    ScrBlockInput* input = vector_add_dst(&vm->blockdefs[block_id].inputs);
    input->type = INPUT_TEXT_DISPLAY;
    input->data = (ScrBlockInputData) {
        .stext = {
            .text = text,
            .ms = vm->text_measure(text),
        },
    };
}

void block_add_argument(ScrVm* vm, size_t block_id, char* defualt_data, ScrBlockArgumentConstraint constraint) {
    ScrBlockInput* input = vector_add_dst(&vm->blockdefs[block_id].inputs);
    input->type = INPUT_ARGUMENT;
    input->data = (ScrBlockInputData) {
        .arg = {
            .text = defualt_data,
            .constr = constraint,
            .ms = vm->arg_measure(defualt_data),
        },
    };
}

void block_add_dropdown(ScrVm* vm, size_t block_id, ScrBlockDropdownSource dropdown_source, ScrListAccessor accessor) {
    ScrBlockInput* input = vector_add_dst(&vm->blockdefs[block_id].inputs);
    input->type = INPUT_DROPDOWN;
    input->data = (ScrBlockInputData) {
        .drop = {
            .source = dropdown_source,
            .list = accessor,
        },
    };
}

void block_add_image(ScrVm* vm, size_t block_id, ScrImage image) {
    ScrBlockInput* input = vector_add_dst(&vm->blockdefs[block_id].inputs);
    input->type = INPUT_IMAGE_DISPLAY;
    input->data = (ScrBlockInputData) {
        .simage = {
            .image = image,
            .ms = vm->img_measure(image),
        }
    };
}

void block_unregister(ScrVm* vm, size_t block_id) {
    vector_free(vm->blockdefs[block_id].inputs);
    vector_remove(vm->blockdefs, block_id);
}

#endif /* SCRVM_IMPLEMENTATION */
