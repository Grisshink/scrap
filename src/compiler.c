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

#include "scrap.h"
#include "ast.h"
#include "vec.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libintl.h>
#include <wchar.h>
#include <assert.h>
#include <math.h>

#define KiB(n) ((size_t)(n) << 10)
#define MiB(n) ((size_t)(n) << 20)
#define GiB(n) ((size_t)(n) << 30)

Compiler compiler_new(void) {
    Compiler compiler = {0};
    compiler.arena = ir_arena_new(GiB(1), MiB(1));
    compiler.bc_pool = bytecode_pool_new(compiler.arena);
    compiler.chains_to_compile = vector_create();
    return compiler;
}

void compiler_free(Compiler* compiler) {
    bytecode_pool_free(compiler->bc_pool);
    vector_free(compiler->chains_to_compile);
}

CompilerError compiler_error_new(size_t msg_size) {
    CompilerError error = {
        .buf = malloc(sizeof(char) * (msg_size + 1)),
        .buf_size = msg_size,
    };
    error.buf[0] = 0;
    return error;
}

void compiler_error_free(CompilerError* error) {
    if (error->buf) free(error->buf);
}

static const char* format_byte_count(Compiler* compiler, size_t size) {
    if (size < KiB(1)) {
        return ir_arena_sprintf(compiler->arena, 32, "%zu", size);
    } else if (size < MiB(1)) {
        return ir_arena_sprintf(compiler->arena, 32, "%.2gKiB", (double)size / 1024.0);
    } else if (size < GiB(1)) {
        return ir_arena_sprintf(compiler->arena, 32, "%.2gMiB", (double)size / (1024.0 * 1024.0));
    } else {
        return ir_arena_sprintf(compiler->arena, 32, "%.2gGiB", (double)size / (1024.0 * 1024.0 * 1024.0));
    }
}

static RootBlockChain* find_root_blockchain(Compiler* compiler, BlockChain* chain) {
    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        if (compiler->code[i].chain == chain) return &compiler->code[i];
    }
    return NULL;
}

bool compiler_compile(Compiler* compiler, RootBlockChain* code, IrBytecode* out_bytecode, CompilerError* error) {
    compiler->label_counter = 0;
    vector_clear(compiler->chains_to_compile);
    compiler->last_error = error;
    compiler->code = code;

    compiler->bytecode = bytecode_new("main", compiler->bc_pool);

    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        assert(!CHAIN_EMPTY(compiler->code[i].chain));
        Block* block = compiler->code[i].chain->start;
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;
        if (strcmp(block->blockdef->id, "on_start")) continue;

        Block* next = NULL;
        Value value = compiler_evaluate_block(compiler, block, &next, (Block*)-1);
        if (value.type == DATA_TYPE_UNKNOWN) {
            if (!compiler->last_error->root_blockchain) compiler->last_error->root_blockchain = &compiler->code[i];
            return false;
        }
    }

    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        assert(!CHAIN_EMPTY(compiler->code[i].chain));
        Block* block = compiler->code[i].chain->start;
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;
        if (!strcmp(block->blockdef->id, "on_start")) continue;

        Block* next = NULL;
        Value value = compiler_evaluate_block(compiler, block, &next, (Block*)-1);
        if (value.type == DATA_TYPE_UNKNOWN) {
            if (!compiler->last_error->root_blockchain) compiler->last_error->root_blockchain = &compiler->code[i];
            return false;
        }
    }

    for (size_t i = 0; i < vector_size(compiler->chains_to_compile); i++) {
        compiler->variables.size = 0;
        Value value = compiler_evaluate_chain(compiler, compiler->chains_to_compile[i]);
        if (value.type == DATA_TYPE_UNKNOWN) {
            if (!compiler->last_error->root_blockchain) {
                compiler->last_error->root_blockchain = find_root_blockchain(compiler, compiler->chains_to_compile[i]);
            }
            return false;
        }
        bytecode_join(&compiler->bytecode, &value.data.chunk_val.bc);

        Block* next = NULL;
        value = compiler_evaluate_block(compiler, compiler->chains_to_compile[i]->start, &next, compiler->chains_to_compile[i]->end);
        if (value.type == DATA_TYPE_UNKNOWN) {
            if (!compiler->last_error->root_blockchain) {
                compiler->last_error->root_blockchain = find_root_blockchain(compiler, compiler->chains_to_compile[i]);
            }
            return false;
        }
        bytecode_join(&compiler->bytecode, &value.data.chunk_val.bc);
    }

#ifdef _DEBUG
    bytecode_print(&compiler->bytecode);

    scrap_log(
        LOG_INFO,
        "[COMPILER] Arena used %s/%s bytes (%.2f%%) (Commit pos: %s)",
        format_byte_count(compiler, compiler->arena->pos),
        format_byte_count(compiler, compiler->arena->reserve_size),
        (double)compiler->arena->pos / (double)compiler->arena->reserve_size * 100.0,
        format_byte_count(compiler, compiler->arena->commit_pos)
    );
#endif

    *out_bytecode = compiler->bytecode;
    return true;
}

bool compiler_run(void* e) {
    bool return_val = false;

    Vm* vm = e;
    IrBytecode bytecode;

    Compiler compiler = compiler_new();
    if (!compiler_compile(&compiler, vm->code, &bytecode, &vm->compiler_error)) {
        scrap_log(LOG_ERROR, "Compilation stage failed. Aborting runtime thread");
        goto thread_return;
    }

    bytecode_save(&bytecode, "bytecode.scrb");

#ifdef _WIN32
    char* cmd = ir_arena_sprintf(compiler.arena, 2048, "scrap.exe -run bytecode.scrb");
#else
    char* cmd = ir_arena_sprintf(compiler.arena, 2048, "%sscrap -run bytecode.scrb", GetApplicationDirectory());
#endif

    if (!term_run_process(cmd, vm->compiler_error.buf, vm->compiler_error.buf_size)) {
        scrap_log(LOG_ERROR, "[RUNTIME] %s", vm->compiler_error.buf);
        scrap_log(LOG_ERROR, "Runtime stage failed. Aborting runtime thread");
        goto thread_return;
    }

    return_val = true;
thread_return:
    compiler_free(&compiler);
    return return_val;
}

void compiler_cleanup(void* e) {
    (void) e;
}

void compiler_set_error(Compiler* compiler, const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vsnprintf(compiler->last_error->buf, compiler->last_error->buf_size, fmt, va);
    va_end(va);
    scrap_log(LOG_ERROR, "[COMPILER] %s", compiler->last_error->buf);
}

Value compiler_evaluate_argument(Compiler* compiler, Argument* arg) {
    static_assert(ARGUMENT_LAST == 6, "Exhaustive argument type in compiler_evaluate_argument");
    Block* next = NULL;
    switch (arg->type) {
    case ARGUMENT_BLOCK:
        return compiler_evaluate_block(compiler, arg->data.block, &next, NULL);
    case ARGUMENT_BLOCKDEF:
        compiler_set_error(compiler, gettext("Tried to evaluate blockdef"));
        return DATA_UNKNOWN;
    case ARGUMENT_VALUE:
        if (arg->data.value.type == DATA_TYPE_ANY) {
            Value value_to_convert = arg->data.value;
            DataType target_type = value_determine_type(&value_to_convert);
            value_to_convert.type = DATA_TYPE_STRING;
            return cast_to_const(compiler, value_to_convert, target_type);
        }
        return arg->data.value;
    case _ARGUMENT_TEXT:
    case _ARGUMENT_CONST_STRING:
        scrap_log(LOG_WARNING, "Use of deprecated text argument type in block \"%s\"", arg->block->blockdef->id);
        return DATA_STRING("");
    case _ARGUMENT_COLOR:
        scrap_log(LOG_WARNING, "Use of deprecated color argument type in block \"%s\"", arg->block->blockdef->id);
        return DATA_COLOR(((BlockdefColor) { 0xff, 0x00, 0xff, 0xff }));
    default:
        assert(false && "Unimplemented argument type in compiler_evaluate_argument");
        return (Value) {0};
    }
}

Value compiler_evaluate_block(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    if (!block->blockdef) {
        compiler_set_error(compiler, gettext("Tried to execute block without definition"));
        if (!compiler->last_error->block) compiler->last_error->block = block;
        return DATA_UNKNOWN;
    }
    if (!block->blockdef->func) {
        compiler_set_error(compiler, gettext("Tried to execute block \"%s\" without implementation"), block->blockdef->id);
        if (!compiler->last_error->block) compiler->last_error->block = block;
        return DATA_UNKNOWN;
    }

    BlockFunc* execute_block = block->blockdef->func;
    Value value = execute_block(compiler, block, next_block, prev_block);
    if (value.type == DATA_TYPE_UNKNOWN) {
        scrap_log(LOG_ERROR, "[COMPILER] Error from block id: \"%s\" (at block %p)", block->blockdef->id, &block);
        if (!compiler->last_error->block) compiler->last_error->block = block;
    } else if (block->blockdef->return_type != DATA_TYPE_ANY) {
        DataType returned_type = value.type;
        if (returned_type == DATA_TYPE_CHUNK) {
            returned_type = value.data.chunk_val.return_type;
        }
        
        if (returned_type != block->blockdef->return_type) {
            scrap_log(LOG_WARNING, "[COMPILER] Block \"%s\" returned type %s, but blockdef expects return type to be %s", block->blockdef->id, type_to_str(returned_type), type_to_str(block->blockdef->return_type));
        }
    }

    return value;
}

Block* compiler_get_next_block(Block* block) {
    if (block->next) return block->next;
    if (block->parent.type == BLOCK_PARENT_BLOCKCHAIN) {
        Block* parent = block->parent.as.chain->parent;
        if (parent) return parent;
    }
    return NULL;
}

Value compiler_evaluate_chain(Compiler* compiler, BlockChain* chain) {
    IrBytecode bc = EMPTY_BYTECODE;
    DataType bc_type = DATA_TYPE_NULL;

    BlockChain* prev_chain = compiler->current_chain;
    compiler->current_chain = chain;

    Block* prev = NULL;
    Block* iter = chain->start;
    while (iter) {
        Block* next = compiler_get_next_block(iter);

        Value value = compiler_evaluate_block(compiler, iter, &next, prev);
        if (value.type == DATA_TYPE_UNKNOWN) {
            scrap_log(LOG_ERROR, "[COMPILER] From chain: %p", chain);
            if (!compiler->last_error->blockchain) compiler->last_error->blockchain = chain;
            compiler->current_chain = prev_chain;
            return value;
        }

        if (value.type == DATA_TYPE_CHUNK) {
            bc_type = value.data.chunk_val.return_type;
            bytecode_join(&bc, &value.data.chunk_val.bc);
            if (bc_type != DATA_TYPE_NULL && next) {
                bytecode_push_op(&bc, IR_POP);
            }
        }

        prev = iter;
        iter = next;
    }

    compiler->current_chain = prev_chain;
    return DATA_CHUNK(bc_type, bc);
}
void* compiler_object_info_get(Compiler* compiler, void* object) {
    ObjectPool* pool = &compiler->object_info;
    if (pool->hash_table.capacity == 0) return OBJECT_NOT_FOUND;

    size_t hash = (size_t)object % pool->hash_table.capacity;
    size_t idx = pool->hash_table.items[hash];
    if (idx == (size_t)-1) return OBJECT_NOT_FOUND;
    while (pool->items[idx].object != object) {
        hash++;
        if (hash >= pool->hash_table.capacity) hash = 0;
        idx = pool->hash_table.items[hash];
        if (idx == (size_t)-1) return OBJECT_NOT_FOUND;
    }
    return pool->items[idx].data;
}

size_t compiler_object_info_insert(Compiler* compiler, void* object, void* data) {
    ObjectPool* pool = &compiler->object_info;

    if ((float)pool->hash_table.size / (float)pool->hash_table.capacity > 0.6 || pool->hash_table.capacity == 0) {
        size_t old_cap = pool->hash_table.capacity;

        if (pool->hash_table.capacity == 0) pool->hash_table.capacity = 1024;
        else pool->hash_table.capacity *= 2;

        pool->hash_table.items = ir_arena_realloc(compiler->arena, pool->hash_table.items, old_cap, sizeof(*pool->hash_table.items) * pool->hash_table.capacity);
        // This sets all buckets in hash table to -1 (empty)
        memset(pool->hash_table.items, 0xff, sizeof(*pool->hash_table.items) * pool->hash_table.capacity);

        for (size_t i = 0; i < pool->size; i++) {
            size_t hash = (size_t)pool->items[i].object % pool->hash_table.capacity;
            size_t idx = pool->hash_table.items[hash];
            while (idx != (size_t)-1) {
                hash++;
                if (hash >= pool->hash_table.capacity) hash = 0;
                idx = pool->hash_table.items[hash];
            }
            pool->hash_table.items[hash] = i;
        }
    }

    size_t hash = (size_t)object % pool->hash_table.capacity;
    size_t idx = pool->hash_table.items[hash];

    while (idx != (size_t)-1) {
        if (pool->items[idx].object == object) {
            pool->items[idx].data = data;
            return idx;
        }

        hash++;
        if (hash >= pool->hash_table.capacity) hash = 0;
        idx = pool->hash_table.items[hash];
    }

    // Insert value
    idx = pool->size;
    pool->hash_table.items[hash] = idx;
    pool->hash_table.size++;
    ir_arena_append(compiler->arena, compiler->object_info, ((ObjectInfoMap) { object, data }));
    return idx;
}

ssize_t compiler_find_variable(Compiler* compiler, const char* name, bool* global) {
    for (ssize_t i = compiler->variables.size - 1; i >= 0; i--) {
        if (!strcmp(compiler->variables.items[i].name, name)) {
            *global = false;
            return i;
        }
    }

    for (ssize_t i = compiler->global_variables.size - 1; i >= 0; i--) {
        if (!strcmp(compiler->global_variables.items[i].name, name)) {
            *global = true;
            return i;
        }
    }

    return -1;
}
