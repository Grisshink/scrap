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

Compiler compiler_new(Thread* thread) {
    Compiler compiler = (Compiler) {
        .code = NULL,
        .thread = thread,
        .current_error_block = NULL,
    };
    compiler.current_error[0] = 0;
    return compiler;
}

void compiler_free(Compiler* compiler) {
    (void) compiler;
}

bool print_str(IrExec* exec) {
    IrList* list = exec_pop_list(exec);
    if (!list) return false;
    for (size_t i = 0; i < list->size; i++) {
        IrValue c = list->items[i];
        switch (c.type) {
        case IR_TYPE_INT: printf("%lc", (wint_t)c.as.int_val); break;
        case IR_TYPE_BYTE: printf("%c", c.as.byte_val); break;
        default: printf("?"); break;
        }
    }
    printf("\n");
    return true;
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

bool compiler_run(void* e) {
    Compiler* compiler = e;

    compiler->label_counter = 0;
    compiler->arena = ir_arena_new(GiB(1), MiB(1));
    compiler->bc_pool = bytecode_pool_new(compiler->arena);
    compiler->chains_to_compile = vector_create();

    compiler->bytecode = bytecode_new("main", compiler->bc_pool);
    compiler->exec_running = false;
    compiler->pid = -1;
    compiler->pid_terminate_attempted = false;

    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        assert(!CHAIN_EMPTY(compiler->code[i].chain));
        Block* block = compiler->code[i].chain->start;
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;
        if (strcmp(block->blockdef->id, "on_start")) continue;

        Block* next = NULL;
        CompilerValue value = compiler_evaluate_block(compiler, block, &next, (Block*)-1);
        if (value.type == DATA_TYPE_UNKNOWN) {
            if (!compiler->current_error_root_blockchain) compiler->current_error_root_blockchain = &compiler->code[i];
            return false;
        }
    }

    for (size_t i = 0; i < vector_size(compiler->code); i++) {
        assert(!CHAIN_EMPTY(compiler->code[i].chain));
        Block* block = compiler->code[i].chain->start;
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;
        if (!strcmp(block->blockdef->id, "on_start")) continue;

        Block* next = NULL;
        CompilerValue value = compiler_evaluate_block(compiler, block, &next, (Block*)-1);
        if (value.type == DATA_TYPE_UNKNOWN) {
            if (!compiler->current_error_root_blockchain) compiler->current_error_root_blockchain = &compiler->code[i];
            return false;
        }
    }

    for (size_t i = 0; i < vector_size(compiler->chains_to_compile); i++) {
        compiler->variables.size = 0;
        CompilerValue value = compiler_evaluate_chain(compiler, compiler->chains_to_compile[i]);
        if (value.type == DATA_TYPE_UNKNOWN) {
            if (!compiler->current_error_root_blockchain) {
                compiler->current_error_root_blockchain = find_root_blockchain(compiler, compiler->chains_to_compile[i]);
            }
            return false;
        }
        bytecode_join(&compiler->bytecode, &value.data.chunk_val.bc);

        Block* next = NULL;
        value = compiler_evaluate_block(compiler, compiler->chains_to_compile[i]->start, &next, compiler->chains_to_compile[i]->end);
        if (value.type == DATA_TYPE_UNKNOWN) {
            if (!compiler->current_error_root_blockchain) {
                compiler->current_error_root_blockchain = find_root_blockchain(compiler, compiler->chains_to_compile[i]);
            }
            return false;
        }
        bytecode_join(&compiler->bytecode, &value.data.chunk_val.bc);
    }

    bytecode_print(&compiler->bytecode);

    scrap_log(
        LOG_INFO,
        "[COMPILER] Arena used %s/%s bytes (%.2f%%) (Commit pos: %s)",
        format_byte_count(compiler, compiler->arena->pos),
        format_byte_count(compiler, compiler->arena->reserve_size),
        (double)compiler->arena->pos / (double)compiler->arena->reserve_size * 100.0,
        format_byte_count(compiler, compiler->arena->commit_pos)
    );

    thread_handle_stopping_state(compiler->thread);

    char error_buf[512];

    compiler->pid = spawn_process_pty("bash", error_buf, 512);
    if (compiler->pid == -1) {
        compiler_set_error(compiler, "%s", error_buf);
        return false;
    }

    while (term_wait_for_output());

    if (!wait_for_process_pty(compiler->pid, error_buf, 512)) {
        compiler_set_error(compiler, "%s", error_buf);
        return false;
    }

    compiler->pid = -1;
    compiler->pid_terminate_attempted = false;

    // compiler->exec = exec_new(MiB(1), GiB(1));
    // if (compiler->exec.last_error[0] != 0) {
    //     compiler_set_error(compiler, "Exec create error: %s", compiler->exec.last_error);
    //     exec_free(&compiler->exec);
    //     return false;
    // }
    // exec_set_run_function_resolver(&compiler->exec, std_resolve_function);
    // exec_add_bytecode(&compiler->exec, compiler->bytecode);
    // compiler->exec_running = true;

    // if (!exec_run(&compiler->exec, "main", "entry")) {
    //     compiler_set_error(compiler, "Runtime error: %s", compiler->exec.last_error);
    //     return false;
    // }

    return true;
}

void compiler_cleanup(void* e) {
    Compiler* compiler = e;
    compiler->pid = -1;
    compiler->pid_terminate_attempted = false;
    if (compiler->exec_running) {
        exec_free(&compiler->exec);
    }

    bytecode_pool_free(compiler->bc_pool);
    vector_free(compiler->chains_to_compile);
}

void compiler_set_error(Compiler* compiler, const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vsnprintf(compiler->current_error, MAX_ERROR_LEN, fmt, va);
    va_end(va);
    scrap_log(LOG_ERROR, "[COMPILER] %s", compiler->current_error);
}

CompilerValue compiler_evaluate_argument(Compiler* compiler, Argument* arg) {
    static_assert(ARGUMENT_LAST == 5, "Exhaustive argument type in compiler_evaluate_argument");
    Block* next = NULL;
    switch (arg->type) {
    case ARGUMENT_TEXT:
    case ARGUMENT_CONST_STRING:
        return DATA_STRING(arg->data.text);
    case ARGUMENT_BLOCK:
        return compiler_evaluate_block(compiler, arg->data.block, &next, NULL);
    case ARGUMENT_BLOCKDEF:
        compiler_set_error(compiler, gettext("Tried to evaluate blockdef"));
        return DATA_UNKNOWN;
    case ARGUMENT_COLOR:
        return DATA_COLOR(CONVERT_COLOR(arg->data.color, Color));
    default:
        assert(false && "Unimplemented argument type in compiler_evaluate_argument");
        return (CompilerValue) {0};
    }
}

CompilerValue compiler_evaluate_block(Compiler* compiler, Block* block, Block** next_block, Block* prev_block) {
    if (!block->blockdef) {
        compiler_set_error(compiler, gettext("Tried to execute block without definition"));
        return DATA_UNKNOWN;
    }
    if (!block->blockdef->func) {
        compiler_set_error(compiler, gettext("Tried to execute block \"%s\" without implementation"), block->blockdef->id);
        return DATA_UNKNOWN;
    }

    BlockFunc* execute_block = block->blockdef->func;
    CompilerValue value = execute_block(compiler, block, next_block, prev_block);
    if (value.type == DATA_TYPE_UNKNOWN) {
        scrap_log(LOG_ERROR, "[COMPILER] Error from block id: \"%s\" (at block %p)", block->blockdef->id, &block);
        if (!compiler->current_error_block) compiler->current_error_block = block;
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

CompilerValue compiler_evaluate_chain(Compiler* compiler, BlockChain* chain) {
    IrBytecode bc = EMPTY_BYTECODE;
    DataType bc_type = DATA_TYPE_NULL;

    BlockChain* prev_chain = compiler->current_chain;
    compiler->current_chain = chain;

    Block* prev = NULL;
    Block* iter = chain->start;
    while (iter) {
        thread_handle_stopping_state(compiler->thread);
        Block* next = compiler_get_next_block(iter);

        CompilerValue value = compiler_evaluate_block(compiler, iter, &next, prev);
        if (value.type == DATA_TYPE_UNKNOWN) {
            scrap_log(LOG_ERROR, "[COMPILER] From chain: %p", chain);
            if (!compiler->current_error_blockchain) compiler->current_error_blockchain = chain;
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
