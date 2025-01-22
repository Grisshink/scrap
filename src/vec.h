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
	((__typeof__(*vec_addr))(\
	    _vector_add_dst((vector*)vec_addr, sizeof(**vec_addr))\
	))
#define vector_insert_dst(vec_addr, pos)\
	((__typeof__(*vec_addr))(\
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
