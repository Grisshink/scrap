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
    for (size_t i = 0; i < vector_size(editor.code); i++) {
        if (block >= editor.code[i].blocks && block < editor.code[i].blocks + vector_size(editor.code[i].blocks)) {
            return &editor.code[i];
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
    if (!blockdef->func) scrap_log(LOG_WARNING, "[VM] Block \"%s\" has not defined its implementation!", blockdef->id);

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
        .items = vector_create(),
        .next = NULL,
        .prev = NULL,
    };
}

BlockCategory* block_category_register(BlockCategory category) {
    BlockCategory* cat = malloc(sizeof(category));
    assert(cat != NULL);
    *cat = category;
    if (!editor.palette.categories_end) {
        editor.palette.categories_end   = cat;
        editor.palette.categories_start = cat;
        editor.palette.current_category = editor.palette.categories_start;
        return cat;
    }
    editor.palette.categories_end->next = cat;
    cat->prev = editor.palette.categories_end;
    editor.palette.categories_end = cat;
    return cat;
}

void block_category_unregister(BlockCategory* category) {
    for (size_t i = 0; i < vector_size(category->items); i++) {
        switch (category->items[i].type) {
        case CATEGORY_ITEM_CHAIN:
            blockchain_free(&category->items[i].data.chain);
            break;
        case CATEGORY_ITEM_LABEL:
            break;
        }
    }
    vector_free(category->items);
    if (category->next) category->next->prev = NULL;
    if (category->prev) category->prev->next = NULL;

    if (editor.palette.categories_start == category) editor.palette.categories_start = category->next;
    if (editor.palette.categories_end == category) editor.palette.categories_end = category->prev;
    if (editor.palette.current_category == category) editor.palette.current_category = editor.palette.categories_start;

    free(category);
}

void block_category_add_blockdef(BlockCategory* category, Blockdef* blockdef) {
    BlockChain chain = blockchain_new();
    blockchain_add_block(&chain, block_new_ms(blockdef));
    if (blockdef->type == BLOCKTYPE_CONTROL && vm.end_blockdef != (size_t)-1) {
        blockchain_add_block(&chain, block_new(vm.blockdefs[vm.end_blockdef])); }

    BlockCategoryItem* item = vector_add_dst(&category->items);
    item->type = CATEGORY_ITEM_CHAIN;
    item->data.chain = chain;
}

void block_category_add_label(BlockCategory* category, const char* label, Color color) {
    BlockCategoryItem* item = vector_add_dst(&category->items);
    item->type = CATEGORY_ITEM_LABEL;
    item->data.label.text = label;
    item->data.label.color = color;
}

void unregister_categories(void) {
    if (!editor.palette.categories_start) return;
    BlockCategory* cat = editor.palette.categories_start;
    while (cat) {
        BlockCategory* next = cat->next;
        block_category_unregister(cat);
        cat = next;
    }
    editor.palette.categories_start = NULL;
    editor.palette.categories_end = NULL;
}

void clear_compile_error(void) {
    vm.compile_error_block = NULL;
    vm.compile_error_blockchain = NULL;
    for (size_t i = 0; i < vector_size(vm.compile_error); i++) vector_free(vm.compile_error[i]);
    vector_clear(vm.compile_error);
}

Vm vm_new(void) {
    Vm vm = (Vm) {
        .blockdefs = vector_create(),
        .end_blockdef = -1,
        .thread = thread_new(exec_run, exec_cleanup),
        .exec = (Exec) {0},

        .compile_error = vector_create(),
        .compile_error_block = NULL,
        .compile_error_blockchain = NULL,
        .start_timeout = -1,
#ifndef USE_INTERPRETER
        .start_mode = COMPILER_MODE_JIT,
#endif
    };
    return vm;
}

void vm_free(Vm* vm) {
    if (thread_is_running(&vm->thread)) {
        thread_stop(&vm->thread);
        thread_join(&vm->thread);
        exec_free(&vm->exec);
    }

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

    for (size_t i = 0; i < vector_size(editor.tabs); i++) {
        if (find_panel(editor.tabs[i].root_panel, PANEL_TERM)) {
#ifndef USE_INTERPRETER
            vm.start_mode = mode;
#endif
            if (editor.current_tab != (int)i) {
                ui.shader_time = 0.0;
                // Delay vm startup until next frame. Because this handler only runs after the layout is computed and
                // before the actual rendering begins, we need to add delay to vm startup to make sure the terminal buffer
                // is initialized and vm does not try to write to uninitialized buffer
                vm.start_timeout = 2;
            } else {
                vm.start_timeout = 1;
            }
            editor.current_tab = i;
            ui.render_surface_needs_redraw = true;
            break;
        }
    }
    return true;
}

bool vm_stop(void) {
    if (!thread_is_running(&vm.thread)) return false;
    scrap_log(LOG_INFO, "STOP");
    thread_stop(&vm.thread);
    ui.render_surface_needs_redraw = true;
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
        while (vm.exec.current_error[i]) {
            vector_add(&vm.compile_error, vector_create());
            size_t line_len = 0;
            while (line_len < 50 && vm.exec.current_error[i]) {
                if (((unsigned char)vm.exec.current_error[i] >> 6) != 2) line_len++;
                if (line_len >= 50) break;
                vector_add(&vm.compile_error[vector_size(vm.compile_error) - 1], vm.exec.current_error[i++]);
            }
            vector_add(&vm.compile_error[vector_size(vm.compile_error) - 1], 0);
        }
        vm.compile_error_block = vm.exec.current_error_block;
        vm.compile_error_blockchain = find_blockchain(vm.compile_error_block);
        exec_free(&vm.exec);
        ui.render_surface_needs_redraw = true;
    } else if (thread_is_running(&vm.thread)) {
        mutex_lock(&term.lock);
        if (find_panel(editor.tabs[editor.current_tab].root_panel, PANEL_TERM) && term.is_buffer_dirty) {
            ui.render_surface_needs_redraw = true;
            term.is_buffer_dirty = false;
        }
        mutex_unlock(&term.lock);
    } else {
        if (vector_size(vm.compile_error) > 0) ui.render_surface_needs_redraw = true;
    }
}
