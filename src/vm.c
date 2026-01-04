// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-202 Grisshink
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

#include <stdlib.h>
#include <assert.h>
#include <libintl.h>

static BlockChain* find_blockchain(Block* block) {
    if (!block) return NULL;
    while (block->parent) block = block->parent;
    for (size_t i = 0; i < vector_size(editor_code); i++) {
        if (block >= editor_code[i].blocks && block < editor_code[i].blocks + vector_size(editor_code[i].blocks)) {
            return &editor_code[i];
        }
    }
    return NULL;
}

Block block_new_ms(Blockdef* blockdef) {
    Block block = block_new(blockdef);
    for (size_t i = 0; i < vector_size(block.arguments); i++) {
        if (block.arguments[i].type != ARGUMENT_BLOCKDEF) continue;
        block.arguments[i].data.blockdef->func = block_exec_custom;
    }
    return block;
}

size_t blockdef_register(Vm* vm, Blockdef* blockdef) {
    if (!blockdef->func) TraceLog(LOG_WARNING, "[VM] Block \"%s\" has not defined its implementation!", blockdef->id);

    vector_add(&vm->blockdefs, blockdef);
    blockdef->ref_count++;
    if (blockdef->type == BLOCKTYPE_END && vm->end_blockdef == (size_t)-1) {
        vm->end_blockdef = vector_size(vm->blockdefs) - 1;
    }

    return vector_size(vm->blockdefs) - 1;
}

void blockdef_unregister(Vm* vm, size_t block_id) {
    blockdef_free(vm->blockdefs[block_id]);
    vector_remove(vm->blockdefs, block_id);
}

BlockCategory block_category_new(const char* name, Color color) {
    return (BlockCategory) {
        .name = name,
        .color = color,
        .chains = vector_create(),
        .next = NULL,
        .prev = NULL,
    };
}

BlockCategory* block_category_register(BlockCategory category) {
    BlockCategory* cat = malloc(sizeof(category));
    assert(cat != NULL);
    *cat = category;
    if (!palette.categories_end) {
        palette.categories_end   = cat;
        palette.categories_start = cat;
        palette.current_category = palette.categories_start;
        return cat;
    }
    palette.categories_end->next = cat;
    cat->prev = palette.categories_end;
    palette.categories_end = cat;
    return cat;
}

void block_category_unregister(BlockCategory* category) {
    for (size_t i = 0; i < vector_size(category->chains); i++) blockchain_free(&category->chains[i]);
    vector_free(category->chains);
    if (category->next) category->next->prev = NULL;
    if (category->prev) category->prev->next = NULL;

    if (palette.categories_start == category) palette.categories_start = category->next;
    if (palette.categories_end == category) palette.categories_end = category->prev;
    if (palette.current_category == category) palette.current_category = palette.categories_start;

    free(category);
}

void add_to_category(Blockdef* blockdef, BlockCategory* category) {
    BlockChain chain = blockchain_new();
    blockchain_add_block(&chain, block_new_ms(blockdef));
    if (blockdef->type == BLOCKTYPE_CONTROL && vm.end_blockdef != (size_t)-1) {
        blockchain_add_block(&chain, block_new(vm.blockdefs[vm.end_blockdef])); }

    vector_add(&category->chains, chain);
}

void unregister_categories(void) {
    if (!palette.categories_start) return;
    BlockCategory* cat = palette.categories_start;
    while (cat) {
        BlockCategory* next = cat->next;
        block_category_unregister(cat);
        cat = next;
    }
    palette.categories_start = NULL;
    palette.categories_end = NULL;
}

void clear_compile_error(void) {
    exec_compile_error_block = NULL;
    exec_compile_error_blockchain = NULL;
    for (size_t i = 0; i < vector_size(exec_compile_error); i++) vector_free(exec_compile_error[i]);
    vector_clear(exec_compile_error);
}

Vm vm_new(void) {
    Vm vm = (Vm) {
        .blockdefs = vector_create(),
        .end_blockdef = -1,
        .thread = thread_new(exec_run, exec_cleanup),
    };
    return vm;
}

void vm_free(Vm* vm) {
    for (ssize_t i = (ssize_t)vector_size(vm->blockdefs) - 1; i >= 0 ; i--) {
        blockdef_unregister(vm, i);
    }
    vector_free(vm->blockdefs);
}

#ifdef USE_INTERPRETER
bool vm_start(void) {
#else
bool vm_start(CompilerMode mode) {
#endif
    if (thread_is_running(&vm.thread)) return false;

    for (size_t i = 0; i < vector_size(code_tabs); i++) {
        if (find_panel(code_tabs[i].root_panel, PANEL_TERM)) {
#ifndef USE_INTERPRETER
            vm_start_mode = mode;
#endif
            if (current_tab != (int)i) {
                shader_time = 0.0;
                // Delay vm startup until next frame. Because this handler only runs after the layout is computed and
                // before the actual rendering begins, we need to add delay to vm startup to make sure the terminal buffer
                // is initialized and vm does not try to write to uninitialized buffer
                vm_start_timeout = 2;
            } else {
                vm_start_timeout = 1;
            }
            current_tab = i;
            render_surface_needs_redraw = true;
            break;
        }
    }
    return true;
}

bool vm_stop(void) {
    if (!thread_is_running(&vm.thread)) return false;
    TraceLog(LOG_INFO, "STOP");
    thread_stop(&vm.thread);
    render_surface_needs_redraw = true;
    return true;
}

void vm_handle_running_thread(void) {
    ThreadReturnCode thread_return = thread_try_join(&vm.thread);
    if (thread_return != THREAD_RETURN_RUNNING) {
        switch (thread_return) {
        case THREAD_RETURN_SUCCESS:
            actionbar_show(gettext("Vm executed successfully"));
            break;
        case THREAD_RETURN_FAILURE:
            actionbar_show(gettext("Vm shitted and died :("));
            break;
        case THREAD_RETURN_STOPPED:
            actionbar_show(gettext("Vm stopped >:("));
            break;
        default:
            break;
        }

        size_t i = 0;
        while (exec.current_error[i]) {
            vector_add(&exec_compile_error, vector_create());
            size_t line_len = 0;
            while (line_len < 50 && exec.current_error[i]) {
                if (((unsigned char)exec.current_error[i] >> 6) != 2) line_len++;
                if (line_len >= 50) break;
                vector_add(&exec_compile_error[vector_size(exec_compile_error) - 1], exec.current_error[i++]);
            }
            vector_add(&exec_compile_error[vector_size(exec_compile_error) - 1], 0);
        }
        exec_compile_error_block = exec.current_error_block;
        exec_compile_error_blockchain = find_blockchain(exec_compile_error_block);
        exec_free(&exec);
        render_surface_needs_redraw = true;
    } else if (thread_is_running(&vm.thread)) {
        mutex_lock(&term.lock);
        if (find_panel(code_tabs[current_tab].root_panel, PANEL_TERM) && term.is_buffer_dirty) {
            render_surface_needs_redraw = true;
            term.is_buffer_dirty = false;
        }
        mutex_unlock(&term.lock);
    } else {
        if (vector_size(exec_compile_error) > 0) render_surface_needs_redraw = true;
    }
}
