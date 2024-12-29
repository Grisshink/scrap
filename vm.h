// Scrap is a project that allows anyone to build software using simple, block based interface.
// This file contains all code for interpreter.
//
// Copyright (C) 2024 Grisshink
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

#include <stddef.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#define VM_ARG_STACK_SIZE 1024
#define VM_CONTROL_STACK_SIZE 32768
#define VM_VARIABLE_STACK_SIZE 1024
#define VM_CHAIN_STACK_SIZE 1024

typedef struct ScrString ScrString;
typedef struct ScrVec ScrVec;
typedef struct ScrColor ScrColor;
typedef struct ScrImage ScrImage;
typedef enum ScrPlacementStrategy ScrPlacementStrategy;
typedef struct ScrMeasurement ScrMeasurement;

typedef enum ScrArgumentType ScrArgumentType;
typedef union ScrArgumentData ScrArgumentData;
typedef struct ScrArgument ScrArgument;
typedef struct ScrBlock ScrBlock;

typedef enum ScrInputArgumentConstraint ScrInputArgumentConstraint;
typedef enum ScrInputDropdownSource ScrInputDropdownSource;
typedef struct ScrInputStaticText ScrInputStaticText;
typedef struct ScrInputStaticImage ScrInputStaticImage;
typedef struct ScrInputArgument ScrInputArgument;
typedef struct ScrInputDropdown ScrInputDropdown;
typedef enum ScrInputType ScrInputType;
typedef union ScrInputData ScrInputData;
typedef struct ScrInput ScrInput;

typedef enum ScrBlockdefType ScrBlockdefType;
typedef struct ScrBlockdef ScrBlockdef;

typedef enum ScrDataControlArgType ScrDataControlArgType;
typedef enum ScrDataType ScrDataType;
typedef enum ScrDataStorageType ScrDataStorageType;
typedef struct ScrDataList ScrDataList;
typedef union ScrDataContents ScrDataContents;
typedef struct ScrDataStorage ScrDataStorage;
typedef struct ScrData ScrData;

typedef struct ScrBlockChain ScrBlockChain;
typedef struct ScrVariable ScrVariable;
typedef struct ScrExec ScrExec;
typedef struct ScrVm ScrVm;
typedef struct ScrChainStackData ScrChainStackData;

typedef char** (*ScrListAccessor)(ScrBlock* block, size_t* list_len);
typedef ScrData (*ScrBlockFunc)(ScrExec* exec, int argc, ScrData* argv);

struct ScrString {
    char* str;
    size_t len;
    size_t cap;
};

struct ScrVec {
    float x, y;
};

struct ScrColor {
    unsigned char r, g, b, a;
};

struct ScrImage {
    void* image_ptr;
};

enum ScrPlacementStrategy {
    PLACEMENT_HORIZONTAL = 0,
    PLACEMENT_VERTICAL,
};

struct ScrMeasurement {
    ScrVec size;
    ScrPlacementStrategy placement;
};

struct ScrInputStaticText {
    ScrMeasurement ms;
    ScrMeasurement editor_ms;
    char* text;
};

struct ScrInputStaticImage {
    ScrMeasurement ms;
    ScrImage image;
};

enum ScrInputArgumentConstraint {
    BLOCKCONSTR_UNLIMITED, // Can put anything as argument
    BLOCKCONSTR_STRING, // Can only put strings as argument
};

struct ScrInputArgument {
    ScrBlockdef* blockdef;
    ScrInputArgumentConstraint constr;
    ScrMeasurement ms;
    char* text;
};

enum ScrInputDropdownSource {
    DROPDOWN_SOURCE_LISTREF,
};

struct ScrInputDropdown {
    ScrInputDropdownSource source;
    ScrListAccessor list;
};

union ScrInputData {
    ScrInputStaticText stext;
    ScrInputStaticImage simage;
    ScrInputArgument arg;
    ScrInputDropdown drop;
};

enum ScrInputType {
    INPUT_TEXT_DISPLAY,
    INPUT_ARGUMENT,
    INPUT_DROPDOWN,
    INPUT_BLOCKDEF_EDITOR,
    INPUT_IMAGE_DISPLAY,
};

struct ScrInput {
    ScrInputType type;
    ScrInputData data;
};

enum ScrBlockdefType {
    BLOCKTYPE_NORMAL,
    BLOCKTYPE_CONTROL,
    BLOCKTYPE_CONTROLEND,
    BLOCKTYPE_END,
    BLOCKTYPE_HAT,
};

struct ScrBlockdef {
    const char* id;
    int ref_count;
    ScrBlockChain* chain;
    int arg_id;
    ScrColor color;
    ScrBlockdefType type;
    // TODO: Maybe remove hidden from here
    bool hidden;
    ScrMeasurement ms;
    ScrInput* inputs;
    ScrBlockFunc func;
};

struct ScrBlock {
    ScrBlockdef* blockdef;
    struct ScrArgument* arguments;
    ScrMeasurement ms;
    struct ScrBlock* parent;
};

union ScrArgumentData {
    char* text;
    ScrBlock block;
    ScrBlockdef* blockdef;
};

enum ScrArgumentType {
    ARGUMENT_TEXT = 0,
    ARGUMENT_BLOCK,
    ARGUMENT_CONST_STRING,
    ARGUMENT_BLOCKDEF,
};

struct ScrArgument {
    ScrMeasurement ms;
    int input_id;
    ScrArgumentType type;
    ScrArgumentData data;
};

enum ScrDataControlArgType {
    CONTROL_ARG_BEGIN,
    CONTROL_ARG_END,
};

struct ScrDataList {
    ScrData* items;
    size_t len; // Length is NOT in bytes, if you want length in bytes, use data.storage.storage_len
};

union ScrDataContents {
    int int_arg;
    double double_arg;
    const char* str_arg;
    ScrDataList list_arg;
    ScrDataControlArgType control_arg;
    const void* custom_arg;
    ScrBlockChain* chain_arg;
};

enum ScrDataStorageType {
    // Data that is contained within arg or lives for the entire lifetime of exec
    DATA_STORAGE_STATIC,
    // Data that is allocated on heap and should be cleaned up by exec.
    // Exec usually cleans up allocated data right after the block execution
    DATA_STORAGE_MANAGED,
    // Data that is allocated on heap and should be cleaned up manually.
    // Exec may free this memory for you if it's necessary
    DATA_STORAGE_UNMANAGED,
};

struct ScrDataStorage {
    ScrDataStorageType type;
    size_t storage_len; // Length is in bytes, so to make copy function work correctly. Only applicable if you don't use DATA_STORAGE_STATIC
};

enum ScrDataType {
    DATA_NOTHING = 0,
    DATA_INT,
    DATA_DOUBLE,
    DATA_STR,
    DATA_BOOL,
    DATA_LIST,
    DATA_CONTROL,
    DATA_OMIT_ARGS, // Marker for vm used in C-blocks that do not require argument recomputation
    DATA_CHAIN,
};

struct ScrData {
    ScrDataType type;
    ScrDataStorage storage;
    ScrDataContents data;
};

struct ScrBlockChain {
    ScrVec pos;
    ScrBlock* blocks;
    int custom_argc;
    ScrData* custom_argv;
};

struct ScrVariable {
    const char* name;
    ScrData value;
    size_t chain_layer;
    int layer;
};

struct ScrChainStackData {
    bool skip_block;
    int layer;
    size_t running_ind;
    int custom_argc;
    ScrData* custom_argv;
    bool is_returning;
    ScrData return_arg;
};

struct ScrExec {
    ScrBlockChain* code;

    ScrData arg_stack[VM_ARG_STACK_SIZE];
    size_t arg_stack_len;

    unsigned char control_stack[VM_CONTROL_STACK_SIZE];
    size_t control_stack_len;

    ScrVariable variable_stack[VM_VARIABLE_STACK_SIZE];
    size_t variable_stack_len;

    ScrChainStackData chain_stack[VM_CHAIN_STACK_SIZE];
    size_t chain_stack_len;

    pthread_t thread;
    atomic_bool is_running;
    ScrBlockChain* running_chain;
};

struct ScrVm {
    ScrBlockdef** blockdefs;
    // TODO: Maybe remove end_blockdef from here
    size_t end_blockdef;
    bool is_running;
};

// Public macros
#define RETURN_NOTHING return (ScrData) { \
    .type = DATA_NOTHING, \
    .storage = DATA_STORAGE_STATIC, \
    .data = (ScrDataContents) {0}, \
}

#define RETURN_OMIT_ARGS return (ScrData) { \
    .type = DATA_OMIT_ARGS, \
    .storage = DATA_STORAGE_STATIC, \
    .data = (ScrDataContents) {0}, \
}

#define RETURN_INT(val) return (ScrData) { \
    .type = DATA_INT, \
    .storage = DATA_STORAGE_STATIC, \
    .data = (ScrDataContents) { \
        .int_arg = (val) \
    }, \
}

#define RETURN_DOUBLE(val) return (ScrData) { \
    .type = DATA_DOUBLE, \
    .storage = DATA_STORAGE_STATIC, \
    .data = (ScrDataContents) { \
        .double_arg = (val) \
    }, \
}

#define RETURN_BOOL(val) return (ScrData) { \
    .type = DATA_BOOL, \
    .storage = DATA_STORAGE_STATIC, \
    .data = (ScrDataContents) { \
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
ScrVm vm_new(void);
void vm_free(ScrVm* vm);

ScrExec exec_new(void);
void exec_free(ScrExec* exec);
void exec_add_chain(ScrVm* vm, ScrExec* exec, ScrBlockChain chain);
void exec_remove_chain(ScrVm* vm, ScrExec* exec, size_t ind);
bool exec_run_chain(ScrExec* exec, ScrBlockChain* chain, ScrData* return_val);
bool exec_start(ScrVm* vm, ScrExec* exec);
bool exec_stop(ScrVm* vm, ScrExec* exec);
bool exec_join(ScrVm* vm, ScrExec* exec, size_t* return_code);
bool exec_try_join(ScrVm* vm, ScrExec* exec, size_t* return_code);
void exec_set_skip_block(ScrExec* exec);

bool variable_stack_push_var(ScrExec* exec, const char* name, ScrData data);
ScrVariable* variable_stack_get_variable(ScrExec* exec, const char* name);

int data_to_int(ScrData arg);
int data_to_bool(ScrData arg);
const char* data_to_str(ScrData arg);

ScrBlockdef* blockdef_new(const char* id, ScrBlockdefType type, ScrColor color, ScrBlockFunc func);
size_t blockdef_register(ScrVm* vm, ScrBlockdef* blockdef);
void blockdef_add_text(ScrBlockdef* blockdef, char* text);
void blockdef_add_argument(ScrBlockdef* blockdef, char* defualt_data, ScrInputArgumentConstraint constraint);
void blockdef_add_dropdown(ScrBlockdef* blockdef, ScrInputDropdownSource dropdown_source, ScrListAccessor accessor);
void blockdef_add_image(ScrBlockdef* blockdef, ScrImage image);
void blockdef_add_blockdef_editor(ScrBlockdef* blockdef);
void blockdef_delete_input(ScrBlockdef* blockdef, size_t input);
void blockdef_set_id(ScrBlockdef* blockdef, const char* new_id);
void blockdef_unregister(ScrVm* vm, size_t id);

ScrBlockChain blockchain_new(void);
ScrBlockChain blockchain_copy(ScrBlockChain* chain, size_t ind);
void blockchain_add_block(ScrBlockChain* chain, ScrBlock block);
void blockchain_clear_blocks(ScrBlockChain* chain);
void blockchain_insert(ScrBlockChain* dst, ScrBlockChain* src, size_t pos);
// Splits off blockchain src in two at specified pos, placing lower half into blockchain dst
void blockchain_detach(ScrBlockChain* dst, ScrBlockChain* src, size_t pos);
void blockchain_detach_single(ScrBlockChain* dst, ScrBlockChain* src, size_t pos);
void blockchain_free(ScrBlockChain* chain);

ScrBlock block_new(ScrBlockdef* blockdef);
ScrBlock block_copy(ScrBlock* block, ScrBlock* parent);
void block_update_parent_links(ScrBlock* block);
void block_update_all_links(ScrBlock* block);
void block_free(ScrBlock* block);

void argument_set_block(ScrArgument* block_arg, ScrBlock block);
void argument_set_const_string(ScrArgument* block_arg, char* text);
void argument_set_text(ScrArgument* block_arg, char* text);

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
void blockchain_update_parent_links(ScrBlockChain* chain);
void arg_stack_push_arg(ScrExec* exec, ScrData data);
void arg_stack_undo_args(ScrExec* exec, size_t count);
void variable_stack_pop_layer(ScrExec* exec);
void variable_stack_cleanup(ScrExec* exec);
void data_free(ScrData data);
void blockdef_free(ScrBlockdef* blockdef);
ScrBlockdef* blockdef_copy(ScrBlockdef* blockdef);
void chain_stack_push(ScrExec* exec, ScrChainStackData data);
void chain_stack_pop(ScrExec* exec);

ScrVm vm_new(void) {
    ScrVm vm = (ScrVm) {
        .blockdefs = vector_create(),
        .end_blockdef = -1,
        .is_running = false,
    };
    return vm;
}

void vm_free(ScrVm* vm) {
    for (ssize_t i = (ssize_t)vector_size(vm->blockdefs) - 1; i >= 0 ; i--) {
        blockdef_unregister(vm, i);
    }
    vector_free(vm->blockdefs);
}

ScrExec exec_new(void) {
    ScrExec exec = (ScrExec) {
        .code = NULL,
        .arg_stack_len = 0,
        .control_stack_len = 0,
        .thread = (pthread_t) {0},
        .is_running = false,
    };
    return exec;
}

void exec_free(ScrExec* exec) {
    (void) exec;
}

void exec_copy_code(ScrVm* vm, ScrExec* exec, ScrBlockChain* code) {
    if (vm->is_running) return;
    exec->code = code;
}

bool exec_block(ScrExec* exec, ScrBlock block, ScrData* block_return, bool from_end, bool omit_args, ScrData control_arg) {
    ScrBlockFunc execute_block = block.blockdef->func;
    if (!execute_block) return false;

    int stack_begin = exec->arg_stack_len;

    if (block.blockdef->arg_id != -1) {
        arg_stack_push_arg(exec, (ScrData) {
            .type = DATA_INT,
            .storage = DATA_STORAGE_STATIC,
            .data = (ScrDataContents) {
                .int_arg = block.blockdef->arg_id,
            },
        });
    }
    
    if (block.blockdef->chain) {
        arg_stack_push_arg(exec, (ScrData) {
            .type = DATA_CHAIN,
            .storage = DATA_STORAGE_STATIC,
            .data = (ScrDataContents) {
                .chain_arg = block.blockdef->chain,
            },
        });
    }

    if (block.blockdef->type == BLOCKTYPE_CONTROL || block.blockdef->type == BLOCKTYPE_CONTROLEND) {
        arg_stack_push_arg(exec, (ScrData) {
            .type = DATA_CONTROL,
            .storage = DATA_STORAGE_STATIC,
            .data = (ScrDataContents) {
                .control_arg = from_end ? CONTROL_ARG_END : CONTROL_ARG_BEGIN,
            },
        });
        if (!from_end && block.blockdef->type == BLOCKTYPE_CONTROLEND) {
            arg_stack_push_arg(exec, control_arg);
        }
    }
    if (!omit_args) {
        for (vec_size_t i = 0; i < vector_size(block.arguments); i++) {
            ScrArgument block_arg = block.arguments[i];
            switch (block_arg.type) {
            case ARGUMENT_TEXT:
            case ARGUMENT_CONST_STRING:
                arg_stack_push_arg(exec, (ScrData) {
                    .type = DATA_STR,
                    .storage = DATA_STORAGE_STATIC,
                    .data = (ScrDataContents) {
                        .str_arg = block_arg.data.text,
                    },
                });
                break;
            case ARGUMENT_BLOCK:
                ScrData arg_return;
                if (!exec_block(exec, block_arg.data.block, &arg_return, false, false, (ScrData) {0})) return false;
                arg_stack_push_arg(exec, arg_return);
                break;
            case ARGUMENT_BLOCKDEF:
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

#define BLOCKDEF chain->blocks[i].blockdef
bool exec_run_custom(ScrExec* exec, ScrBlockChain* chain, int argc, ScrData* argv, ScrData* return_val) {
    chain->custom_argc = argc;
    chain->custom_argv = argv;
    return exec_run_chain(exec, chain, return_val);
}

bool exec_run_chain(ScrExec* exec, ScrBlockChain* chain, ScrData* return_val) {
    int skip_layer = -1;
    size_t base_len = exec->control_stack_len;
    chain_stack_push(exec, (ScrChainStackData) {
        .skip_block = false,
        .layer = 0,
        .running_ind = 0,
        .custom_argc = chain->custom_argc,
        .custom_argv = chain->custom_argv,
        .is_returning = false,
        .return_arg = (ScrData) {0},
    });
    exec->running_chain = chain;
    ScrData block_return;
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        pthread_testcancel();
        size_t block_ind = i;
        ScrChainStackData* chain_data = &exec->chain_stack[exec->chain_stack_len - 1];
        chain_data->running_ind = i;
        bool from_end = false;
        bool omit_args = false;
        bool return_used = false;
        if (chain_data->is_returning) {
            break;
        }

        if (BLOCKDEF->type == BLOCKTYPE_END || BLOCKDEF->type == BLOCKTYPE_CONTROLEND) {
            if (BLOCKDEF->type == BLOCKTYPE_CONTROLEND && chain_data->layer == 0) continue;
            variable_stack_pop_layer(exec);
            chain_data->layer--;
            control_stack_pop_data(block_ind, size_t)
            control_stack_pop_data(block_return, ScrData)
            if (block_return.type == DATA_OMIT_ARGS) omit_args = true;
            if (block_return.storage.type == DATA_STORAGE_MANAGED) data_free(block_return);
            from_end = true;
            if (chain_data->skip_block && skip_layer == chain_data->layer) {
                chain_data->skip_block = false;
                skip_layer = -1;
            }
        }
        if (!chain_data->skip_block) {
            if (!exec_block(exec, chain->blocks[block_ind], &block_return, from_end, omit_args, (ScrData){0})) {
                chain_stack_pop(exec);
                return false;
            }
            if (chain_data->running_ind != i) i = chain_data->running_ind;
        }
        if (BLOCKDEF->type == BLOCKTYPE_CONTROLEND && block_ind != i) {
            from_end = false;
            if (!exec_block(exec, chain->blocks[i], &block_return, from_end, false, block_return)) {
                chain_stack_pop(exec);
                return false;
            }
            if (chain_data->running_ind != i) i = chain_data->running_ind;
            return_used = true;
        }
        if (BLOCKDEF->type == BLOCKTYPE_CONTROL || BLOCKDEF->type == BLOCKTYPE_CONTROLEND) {
            control_stack_push_data(block_return, ScrData)
            control_stack_push_data(i, size_t)
            if (chain_data->skip_block && skip_layer == -1) skip_layer = chain_data->layer;
            return_used = true;
            chain_data->layer++;
        } 
        if (!return_used && block_return.storage.type == DATA_STORAGE_MANAGED) {
            data_free(block_return);
        }
    }
    *return_val = exec->chain_stack[exec->chain_stack_len - 1].return_arg;
    while (exec->chain_stack[exec->chain_stack_len - 1].layer >= 0) {
        variable_stack_pop_layer(exec);
        exec->chain_stack[exec->chain_stack_len - 1].layer--;
    }
    exec->control_stack_len = base_len;
    chain_stack_pop(exec);
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
    exec->chain_stack_len = 0;
    exec->running_chain = NULL;

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        ScrBlock* block = &exec->code[i].blocks[0];
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;
        for (size_t j = 0; j < vector_size(block->arguments); j++) {
            if (block->arguments[j].type != ARGUMENT_BLOCKDEF) continue;
            block->arguments[j].data.blockdef->chain = &exec->code[i];
        }
    }

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        ScrBlock* block = &exec->code[i].blocks[0];
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;
        bool cont = false;
        for (size_t j = 0; j < vector_size(block->arguments); j++) {
            if (block->arguments[j].type == ARGUMENT_BLOCKDEF) {
                cont = true;
                break;
            }
        }
        if (cont) continue;
        exec->code[i].custom_argc = -1;
        exec->code[i].custom_argv = NULL;
        ScrData bin;
        if (!exec_run_chain(exec, &exec->code[i], &bin)) {
            exec->running_chain = NULL;
            pthread_exit((void*)0);
        }
        exec->running_chain = NULL;
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

void exec_set_skip_block(ScrExec* exec) {
    exec->chain_stack[exec->chain_stack_len - 1].skip_block = true;
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

bool variable_stack_push_var(ScrExec* exec, const char* name, ScrData arg) {
    if (exec->variable_stack_len >= VM_VARIABLE_STACK_SIZE) return false;
    if (*name == 0) return false;
    ScrVariable var;
    var.name = name;
    var.value = arg;
    var.chain_layer = exec->chain_stack_len - 1;
    var.layer = exec->chain_stack[var.chain_layer].layer;
    exec->variable_stack[exec->variable_stack_len++] = var;
    return true;
}

void variable_stack_pop_layer(ScrExec* exec) {
    size_t count = 0;
    for (int i = exec->variable_stack_len - 1; i >= 0 && 
                                               exec->variable_stack[i].layer == exec->chain_stack[exec->chain_stack_len - 1].layer && 
                                               exec->variable_stack[i].chain_layer == exec->chain_stack_len - 1; i--) {
        ScrData arg = exec->variable_stack[i].value;
        if (arg.storage.type == DATA_STORAGE_UNMANAGED || arg.storage.type == DATA_STORAGE_MANAGED) {
            data_free(arg);
        }
        count++;
    }
    exec->variable_stack_len -= count;
}

void variable_stack_cleanup(ScrExec* exec) {
    for (size_t i = 0; i < exec->variable_stack_len; i++) {
        ScrData arg = exec->variable_stack[i].value;
        if (arg.storage.type == DATA_STORAGE_UNMANAGED || arg.storage.type == DATA_STORAGE_MANAGED) {
            data_free(arg);
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

void chain_stack_push(ScrExec* exec, ScrChainStackData data) {
    if (exec->chain_stack_len >= VM_CHAIN_STACK_SIZE) {
        printf("[VM] CRITICAL: Chain stack overflow\n");
        pthread_exit((void*)0);
    }
    exec->chain_stack[exec->chain_stack_len++] = data;
}

void chain_stack_pop(ScrExec* exec) {
    if (exec->chain_stack_len == 0) {
        printf("[VM] CRITICAL: Chain stack underflow\n");
        pthread_exit((void*)0);
    }
    exec->chain_stack_len--;
}

void arg_stack_push_arg(ScrExec* exec, ScrData arg) {
    if (exec->arg_stack_len >= VM_ARG_STACK_SIZE) {
        printf("[VM] CRITICAL: Arg stack overflow\n");
        pthread_exit((void*)0);
    }
    exec->arg_stack[exec->arg_stack_len++] = arg;
}

void arg_stack_undo_args(ScrExec* exec, size_t count) {
    if (count > exec->arg_stack_len) {
        printf("[VM] CRITICAL: Arg stack underflow\n");
        pthread_exit((void*)0);
    }
    for (size_t i = 0; i < count; i++) {
        ScrData arg = exec->arg_stack[exec->arg_stack_len - 1 - i];
        if (arg.storage.type != DATA_STORAGE_MANAGED) continue;
        data_free(arg);
    }
    exec->arg_stack_len -= count;
}

ScrData data_copy(ScrData arg) {
    if (arg.storage.type == DATA_STORAGE_STATIC) return arg;

    ScrData out;
    out.type = arg.type;
    out.storage.type = DATA_STORAGE_MANAGED;
    out.storage.storage_len = arg.storage.storage_len;
    out.data.custom_arg = malloc(arg.storage.storage_len);
    if (arg.type == DATA_LIST) {
        out.data.list_arg.len = arg.data.list_arg.len;
        for (size_t i = 0; i < arg.data.list_arg.len; i++) {
            out.data.list_arg.items[i] = data_copy(arg.data.list_arg.items[i]);
        }
    } else {
        memcpy((void*)out.data.custom_arg, arg.data.custom_arg, arg.storage.storage_len);
    }
    return out;
}

void data_free(ScrData arg) {
    if (arg.storage.type == DATA_STORAGE_STATIC) return;
    switch (arg.type) {
    case DATA_LIST:
        if (!arg.data.list_arg.items) break;
        for (size_t i = 0; i < arg.data.list_arg.len; i++) {
            data_free(arg.data.list_arg.items[i]);
        }
        free((ScrData*)arg.data.list_arg.items);
        break;
    default:
        if (!arg.data.custom_arg) break;
        free((void*)arg.data.custom_arg);
        break;
    }
}

int data_to_int(ScrData arg) {
    switch (arg.type) {
    case DATA_BOOL:
    case DATA_INT:
        return arg.data.int_arg;
    case DATA_DOUBLE:
        return (int)arg.data.double_arg;
    case DATA_STR:
        return atoi(arg.data.str_arg);
    default:
        return 0;
    }
}

double data_to_double(ScrData arg) {
    switch (arg.type) {
    case DATA_BOOL:
    case DATA_INT:
        return (double)arg.data.int_arg;
    case DATA_DOUBLE:
        return arg.data.double_arg;
    case DATA_STR:
        return atof(arg.data.str_arg);
    default:
        return 0.0;
    }
}

int data_to_bool(ScrData arg) {
    switch (arg.type) {
    case DATA_BOOL:
    case DATA_INT:
        return arg.data.int_arg != 0;
    case DATA_DOUBLE:
        return arg.data.double_arg != 0.0;
    case DATA_STR:
        return *arg.data.str_arg != 0;
    case DATA_LIST:
        return arg.data.list_arg.len != 0;
    default:
        return 0;
    }
}

const char* data_to_str(ScrData arg) {
    static char buf[32];

    switch (arg.type) {
    case DATA_STR:
        return arg.data.str_arg;
    case DATA_BOOL:
        return arg.data.int_arg ? "true" : "false";
    case DATA_DOUBLE:
        buf[0] = 0;
        snprintf(buf, 32, "%f", arg.data.double_arg);
        return buf;
    case DATA_INT:
        buf[0] = 0;
        snprintf(buf, 32, "%d", arg.data.int_arg);
        return buf;
    case DATA_LIST:
        return "# LIST #";
    default:
        return "";
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

ScrData string_make_managed(ScrString* string) {
    ScrData out;
    out.type = DATA_STR;
    out.storage.type = DATA_STORAGE_MANAGED;
    out.storage.storage_len = string->len + 1;
    out.data.str_arg = string->str;
    return out;
}

void string_free(ScrString string) {
    free(string.str);
}

ScrBlock block_new(ScrBlockdef* blockdef) {
    ScrBlock block;
    block.blockdef = blockdef;
    block.ms = (ScrMeasurement) {0};
    block.arguments = vector_create();
    block.parent = NULL;
    blockdef->ref_count++;

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (block.blockdef->inputs[i].type != INPUT_ARGUMENT && 
            block.blockdef->inputs[i].type != INPUT_DROPDOWN &&
            block.blockdef->inputs[i].type != INPUT_BLOCKDEF_EDITOR) continue;
        ScrArgument* arg = vector_add_dst((ScrArgument**)&block.arguments);
        arg->input_id = i;

        switch (blockdef->inputs[i].type) {
        case INPUT_ARGUMENT:
            arg->ms = blockdef->inputs[i].data.arg.ms;
            switch (blockdef->inputs[i].data.arg.constr) {
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

            arg->data.text = vector_create();
            for (char* pos = blockdef->inputs[i].data.arg.text; *pos; pos++) {
                vector_add(&arg->data.text, *pos);
            }
            vector_add(&arg->data.text, 0);
            break;
        case INPUT_DROPDOWN:
            arg->type = ARGUMENT_CONST_STRING;
            arg->ms = (ScrMeasurement) {0};

            size_t list_len = 0;
            char** list = blockdef->inputs[i].data.drop.list(&block, &list_len);
            if (!list || list_len == 0) break;

            arg->data.text = vector_create();
            for (char* pos = list[0]; *pos; pos++) {
                vector_add(&arg->data.text, *pos);
            }
            vector_add(&arg->data.text, 0);
            break;
        case INPUT_BLOCKDEF_EDITOR:
            arg->type = ARGUMENT_BLOCKDEF;
            arg->ms = (ScrMeasurement) {0};
            arg->data.blockdef = blockdef_new("custom", BLOCKTYPE_NORMAL, blockdef->color, NULL);
            arg->data.blockdef->ref_count++;
            blockdef_add_text(arg->data.blockdef, "My block");
            break;
        default:
            assert(false && "Unreachable");
            break;
        }
    }
    return block;
}

ScrBlock block_copy(ScrBlock* block, ScrBlock* parent) {
    if (!block->arguments) return *block;

    ScrBlock new;
    new.blockdef = block->blockdef;
    new.ms = block->ms;
    new.parent = parent;
    new.arguments = vector_create();
    new.blockdef->ref_count++;

    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        ScrArgument* arg = vector_add_dst((ScrArgument**)&new.arguments);
        arg->ms = block->arguments[i].ms;
        arg->type = block->arguments[i].type;
        arg->input_id = block->arguments[i].input_id;
        switch (block->arguments[i].type) {
        case ARGUMENT_CONST_STRING:
        case ARGUMENT_TEXT:
            arg->data.text = vector_copy(block->arguments[i].data.text);
            break;
        case ARGUMENT_BLOCK:
            arg->data.block = block_copy(&block->arguments[i].data.block, &new);
            break;
        case ARGUMENT_BLOCKDEF:
            arg->data.blockdef = blockdef_copy(block->arguments[i].data.blockdef);
            break;
        default:
            assert(false && "Unimplemented argument copy");
            break;
        }
    }

    for (size_t i = 0; i < vector_size(new.arguments); i++) {
        if (new.arguments[i].type != ARGUMENT_BLOCK) continue;
        block_update_parent_links(&new.arguments[i].data.block);
    }

    return new;
}

void block_free(ScrBlock* block) {
    blockdef_free(block->blockdef);
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
            case ARGUMENT_BLOCKDEF:
                blockdef_free(block->arguments[i].data.blockdef);
                break;
            default:
                assert(false && "Unimplemented argument free");
                break;
            }
        }
        vector_free((ScrArgument*)block->arguments);
    }
}

void block_update_all_links(ScrBlock* block) {
    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        if (block->arguments[i].type != ARGUMENT_BLOCK) continue;
        block->arguments[i].data.block.parent = block;
        block_update_all_links(&block->arguments[i].data.block);
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

ScrBlockChain blockchain_copy_single(ScrBlockChain* chain, size_t pos) {
    assert(pos < vector_size(chain->blocks) || pos == 0);

    ScrBlockChain new;
    new.pos = chain->pos;
    new.blocks = vector_create();

    ScrBlockdefType block_type = chain->blocks[pos].blockdef->type;
    if (block_type == BLOCKTYPE_END) return new;
    if (block_type != BLOCKTYPE_CONTROL) {
        vector_add(&new.blocks, block_copy(&chain->blocks[pos], NULL));
        blockchain_update_parent_links(&new);
        return new;
    }

    int size = 0;
    int layer = 0;
    for (size_t i = pos; i < vector_size(chain->blocks) && layer >= 0; i++) {
        block_type = chain->blocks[i].blockdef->type;
        vector_add(&new.blocks, block_copy(&chain->blocks[i], NULL));
        if (block_type == BLOCKTYPE_CONTROL && i != pos) {
            layer++;
        } else if (block_type == BLOCKTYPE_END) {
            layer--;
        }
        size++;
    }

    blockchain_update_parent_links(&new);
    return new;
}

ScrBlockChain blockchain_copy(ScrBlockChain* chain, size_t pos) {
    assert(pos < vector_size(chain->blocks) || pos == 0);

    ScrBlockChain new;
    new.pos = chain->pos;
    new.blocks = vector_create();

    int pos_layer = 0;
    for (size_t i = 0; i < pos; i++) {
        ScrBlockdefType block_type = chain->blocks[i].blockdef->type;
        if (block_type == BLOCKTYPE_CONTROL) {
            pos_layer++;
        } else if (block_type == BLOCKTYPE_END) {
            pos_layer--;
            if (pos_layer < 0) pos_layer = 0;
        }
    }
    int current_layer = pos_layer;

    vector_reserve(&new.blocks, vector_size(chain->blocks) - pos);
    for (vec_size_t i = pos; i < vector_size(chain->blocks); i++) {
        ScrBlockdefType block_type = chain->blocks[i].blockdef->type;
        if ((block_type == BLOCKTYPE_END || (block_type == BLOCKTYPE_CONTROLEND && i != pos)) &&
            pos_layer == current_layer &&
            current_layer != 0) break;

        vector_add(&new.blocks, block_copy(&chain->blocks[i], NULL));
        block_update_parent_links(&new.blocks[vector_size(new.blocks) - 1]);

        if (block_type == BLOCKTYPE_CONTROL) {
            current_layer++;
        } else if (block_type == BLOCKTYPE_END) {
            current_layer--;
        }
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

void blockchain_detach_single(ScrBlockChain* dst, ScrBlockChain* src, size_t pos) {
    assert(pos < vector_size(src->blocks));

    ScrBlockdefType block_type = src->blocks[pos].blockdef->type;
    if (block_type == BLOCKTYPE_END) return;
    if (block_type != BLOCKTYPE_CONTROL) {
        vector_add(&dst->blocks, src->blocks[pos]);
        blockchain_update_parent_links(dst);
        vector_remove(src->blocks, pos);
        for (size_t i = pos; i < vector_size(src->blocks); i++) block_update_parent_links(&src->blocks[i]);
        return;
    }

    int size = 0;
    int layer = 0;
    for (size_t i = pos; i < vector_size(src->blocks) && layer >= 0; i++) {
        ScrBlockdefType block_type = src->blocks[i].blockdef->type;
        vector_add(&dst->blocks, src->blocks[i]);
        if (block_type == BLOCKTYPE_CONTROL && i != pos) {
            layer++;
        } else if (block_type == BLOCKTYPE_END) {
            layer--;
        }
        size++;
    }

    blockchain_update_parent_links(dst);
    vector_erase(src->blocks, pos, size);
    for (size_t i = pos; i < vector_size(src->blocks); i++) block_update_parent_links(&src->blocks[i]);
}

void blockchain_detach(ScrBlockChain* dst, ScrBlockChain* src, size_t pos) {
    assert(pos < vector_size(src->blocks));

    int pos_layer = 0;
    for (size_t i = 0; i < pos; i++) {
        ScrBlockdefType block_type = src->blocks[i].blockdef->type;
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
        ScrBlockdefType block_type = src->blocks[i].blockdef->type;
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

void argument_set_block(ScrArgument* block_arg, ScrBlock block) {
    if (block_arg->type == ARGUMENT_TEXT || block_arg->type == ARGUMENT_CONST_STRING) vector_free(block_arg->data.text);
    block_arg->type = ARGUMENT_BLOCK;
    block_arg->data.block = block;

    block_update_parent_links(&block_arg->data.block);
}

void argument_set_const_string(ScrArgument* block_arg, char* text) {
    assert(block_arg->type == ARGUMENT_CONST_STRING);

    block_arg->type = ARGUMENT_CONST_STRING;
    vector_clear(block_arg->data.text);

    for (char* pos = text; *pos; pos++) {
        vector_add(&block_arg->data.text, *pos);
    }
    vector_add(&block_arg->data.text, 0);
}

void argument_set_text(ScrArgument* block_arg, char* text) {
    assert(block_arg->type == ARGUMENT_BLOCK);
    assert(block_arg->data.block.parent != NULL);

    block_arg->type = ARGUMENT_TEXT;
    block_arg->data.text = vector_create();

    for (char* pos = text; *pos; pos++) {
        vector_add(&block_arg->data.text, *pos);
    }
    vector_add(&block_arg->data.text, 0);
}

ScrBlockdef* blockdef_new(const char* id, ScrBlockdefType type, ScrColor color, ScrBlockFunc func) {
    assert(id != NULL);
    ScrBlockdef* blockdef = malloc(sizeof(ScrBlockdef));
    blockdef->id = strcpy(malloc((strlen(id) + 1) * sizeof(char)), id);
    blockdef->color = color;
    blockdef->type = type;
    blockdef->ms = (ScrMeasurement) {0};
    blockdef->hidden = false;
    blockdef->ref_count = 0;
    blockdef->inputs = vector_create();
    blockdef->func = func;
    blockdef->chain = NULL;
    blockdef->arg_id = -1;

    return blockdef;
}

ScrBlockdef* blockdef_copy(ScrBlockdef* blockdef) {
    ScrBlockdef* new = malloc(sizeof(ScrBlockdef));
    new->id = strcpy(malloc((strlen(blockdef->id) + 1) * sizeof(char)), blockdef->id);
    new->color = blockdef->color;
    new->type = blockdef->type;
    new->ms = blockdef->ms;
    new->hidden = blockdef->hidden;
    new->ref_count = blockdef->ref_count;
    new->inputs = vector_create();
    new->func = blockdef->func;

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        ScrInput* input = vector_add_dst(&new->inputs);
        input->type = blockdef->inputs[i].type;
        switch (blockdef->inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            input->data = (ScrInputData) {
                .stext = {
                    .text = vector_copy(blockdef->inputs[i].data.stext.text),
                    .ms = blockdef->inputs[i].data.stext.ms,
                },
            };
            break;
        case INPUT_ARGUMENT:
            input->data = (ScrInputData) {
                .arg = {
                    .blockdef = blockdef_copy(blockdef->inputs[i].data.arg.blockdef),
                    .text = blockdef->inputs[i].data.arg.text,
                    .constr = blockdef->inputs[i].data.arg.constr,
                    .ms = blockdef->inputs[i].data.arg.ms,
                },
            };
            break;
        case INPUT_IMAGE_DISPLAY:
            input->data = (ScrInputData) {
                .simage = {
                    .image = blockdef->inputs[i].data.simage.image,
                    .ms = blockdef->inputs[i].data.simage.ms,
                }
            };
            break;
        case INPUT_DROPDOWN:
            input->data = (ScrInputData) {
                .drop = {
                    .source = blockdef->inputs[i].data.drop.source,
                    .list = blockdef->inputs[i].data.drop.list,
                },
            };
            break;
        case INPUT_BLOCKDEF_EDITOR:
            input->data = (ScrInputData) {0};
            break;
        default:
            assert(false && "Unimplemented input copy");
            break;
        }
    }

    return new;
}

size_t blockdef_register(ScrVm* vm, ScrBlockdef* blockdef) {
    if (!blockdef->func) printf("[VM] WARNING: Block \"%s\" has not defined its implementation!\n", blockdef->id);

    vector_add(&vm->blockdefs, blockdef);
    blockdef->ref_count++;
    if (blockdef->type == BLOCKTYPE_END && vm->end_blockdef == (size_t)-1) {
        vm->end_blockdef = vector_size(vm->blockdefs) - 1;
        blockdef->hidden = true;
    }

    return vector_size(vm->blockdefs) - 1;
}

void blockdef_add_text(ScrBlockdef* blockdef, char* text) {
    ScrInput* input = vector_add_dst(&blockdef->inputs);
    ScrMeasurement ms = (ScrMeasurement) {0};
    input->type = INPUT_TEXT_DISPLAY;
    input->data = (ScrInputData) {
        .stext = {
            .text = vector_create(),
            .ms = ms,
        },
    };

    for (char* str = text; *str; str++) vector_add(&input->data.stext.text, *str);
    vector_add(&input->data.stext.text, 0);
}

void blockdef_add_argument(ScrBlockdef* blockdef, char* defualt_data, ScrInputArgumentConstraint constraint) {
    ScrInput* input = vector_add_dst(&blockdef->inputs);
    ScrMeasurement ms = (ScrMeasurement) {0};
    input->type = INPUT_ARGUMENT;
    input->data = (ScrInputData) {
        .arg = {
            .blockdef = blockdef_new("custom_arg", BLOCKTYPE_NORMAL, blockdef->color, NULL),
            .text = defualt_data,
            .constr = constraint,
            .ms = ms,
        },
    };
    input->data.arg.blockdef->ref_count++;
}

void blockdef_add_blockdef_editor(ScrBlockdef* blockdef) {
    ScrInput* input = vector_add_dst(&blockdef->inputs);
    input->type = INPUT_BLOCKDEF_EDITOR;
    input->data = (ScrInputData) {0};
}

void blockdef_add_dropdown(ScrBlockdef* blockdef, ScrInputDropdownSource dropdown_source, ScrListAccessor accessor) {
    ScrInput* input = vector_add_dst(&blockdef->inputs);
    input->type = INPUT_DROPDOWN;
    input->data = (ScrInputData) {
        .drop = {
            .source = dropdown_source,
            .list = accessor,
        },
    };
}

void blockdef_add_image(ScrBlockdef* blockdef, ScrImage image) {
    ScrInput* input = vector_add_dst(&blockdef->inputs);
    ScrMeasurement ms = (ScrMeasurement) {0};
    input->type = INPUT_IMAGE_DISPLAY;
    input->data = (ScrInputData) {
        .simage = {
            .image = image,
            .ms = ms,
        }
    };
}

void blockdef_set_id(ScrBlockdef* blockdef, const char* new_id) {
    free((void*)blockdef->id);
    blockdef->id = strcpy(malloc((strlen(new_id) + 1) * sizeof(char)), new_id);
}

void blockdef_delete_input(ScrBlockdef* blockdef, size_t input) {
    assert(input < vector_size(blockdef->inputs));

    switch (blockdef->inputs[input].type) {
    case INPUT_TEXT_DISPLAY:
        vector_free(blockdef->inputs[input].data.stext.text);
        break;
    case INPUT_ARGUMENT:
        blockdef_free(blockdef->inputs[input].data.arg.blockdef);
        break;
    default:
        assert(false && "Unimplemented input delete");
        break;
    }
    vector_remove(blockdef->inputs, input);
}

void blockdef_free(ScrBlockdef* blockdef) {
    blockdef->ref_count--;
    if (blockdef->ref_count > 0) return;
    for (vec_size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        switch (blockdef->inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            vector_free(blockdef->inputs[i].data.stext.text);
            break;
        case INPUT_ARGUMENT:
            blockdef_free(blockdef->inputs[i].data.arg.blockdef);
            break;
        default:
            break;
        }
    }
    vector_free(blockdef->inputs);
    free((void*)blockdef->id);
    free(blockdef);
}

void blockdef_unregister(ScrVm* vm, size_t block_id) {
    blockdef_free(vm->blockdefs[block_id]);
    vector_remove(vm->blockdefs, block_id);
}

#endif /* SCRVM_IMPLEMENTATION */
