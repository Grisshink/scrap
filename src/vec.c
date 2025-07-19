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

#include "vec.h"
#include <string.h>
#include <stdbool.h>

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
