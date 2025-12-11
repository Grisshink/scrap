// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-2025 Grisshink
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

#include "scrap.h"
#include "term.h"
#include "../external/tinyfiledialogs.h"
#include "vec.h"
#include "util.h"

#include <assert.h>
#include <math.h>
#include <wctype.h>
#include <libintl.h>
#include <stdio.h>
#include <string.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

typedef enum {
    FILE_MENU_NEW_PROJECT = 0,
    FILE_MENU_SAVE_PROJECT,
    FILE_MENU_LOAD_PROJECT,
} FileMenuInds;

char* file_menu_list[] = {
    "New project",
    "Save project",
    "Load project",
};

Block block_new_ms(Blockdef* blockdef) {
    Block block = block_new(blockdef);
    for (size_t i = 0; i < vector_size(block.arguments); i++) {
        if (block.arguments[i].type != ARGUMENT_BLOCKDEF) continue;
        block.arguments[i].data.blockdef->func = block_exec_custom;
    }
    return block;
}

static void switch_tab_to_panel(PanelType panel) {
    for (size_t i = 0; i < vector_size(code_tabs); i++) {
        if (find_panel(code_tabs[i].root_panel, panel)) {
            if (current_tab != (int)i) shader_time = 0.0;
            current_tab = i;
            render_surface_needs_redraw = true;
            return;
        }
    }
}

// Removes a block and all blocks within it if it matches the specified blockdef
static void block_delete_blockdef(Block* block, Blockdef* blockdef) {
    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        if (blockdef->ref_count <= 1) break;
        if (block->arguments[i].type != ARGUMENT_BLOCK) continue;
        if (block->arguments[i].data.block.blockdef == blockdef) {
            block_free(&block->arguments[i].data.block);
            argument_set_text(&block->arguments[i], "");
            continue;
        }
        block_delete_blockdef(&block->arguments[i].data.block, blockdef);
    }
}

// Deletes blocks in the chain that have a reference to the specified blockdef
static void blockchain_delete_blockdef(BlockChain* chain, Blockdef* blockdef) {
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        if (blockdef->ref_count <= 1) break;
        if (chain->blocks[i].blockdef == blockdef) {
            block_free(&chain->blocks[i]);
            vector_remove(chain->blocks, i);
            i--;
            continue;
        }
        block_delete_blockdef(&chain->blocks[i], blockdef);
    }
    blockchain_update_parent_links(chain);
}

// Removes blocks associated with blockdef, freeing memory
static void editor_code_remove_blockdef(Blockdef* blockdef) {
    for (size_t i = 0; i < vector_size(editor_code); i++) {
        if (blockdef->ref_count <= 1) break;
        blockchain_delete_blockdef(&editor_code[i], blockdef);
        if (vector_size(editor_code[i].blocks) == 0) {
            blockchain_free(&editor_code[i]);
            vector_remove(editor_code, i);
            i--;
        }
    }
}

static bool edit_text(char** text) {
    if (!text) return false;

    if (IsKeyPressed(KEY_HOME)) {
        hover_info.select_input_ind = 0;
        render_surface_needs_redraw = true;
        return false;
    }

    if (IsKeyPressed(KEY_END)) {
        hover_info.select_input_ind = vector_size(*text) - 1;
        render_surface_needs_redraw = true;
        return false;
    }

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        hover_info.select_input_ind--;
        if (hover_info.select_input_ind < 0) {
            hover_info.select_input_ind = 0;
        } else {
            while (((unsigned char)(*text)[hover_info.select_input_ind] >> 6) == 2) hover_info.select_input_ind--;
        }
        render_surface_needs_redraw = true;
        return false;
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        hover_info.select_input_ind++;
        if (hover_info.select_input_ind >= (int)vector_size(*text)) {
            hover_info.select_input_ind = vector_size(*text) - 1;
        } else {
            while (((unsigned char)(*text)[hover_info.select_input_ind] >> 6) == 2) hover_info.select_input_ind++;
        }
        render_surface_needs_redraw = true;
        return false;
    }

    if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
        if (vector_size(*text) <= 1 || hover_info.select_input_ind == (int)vector_size(*text) - 1) return false;

        int remove_pos = hover_info.select_input_ind;
        int remove_size;
        GetCodepointNext(*text + remove_pos, &remove_size);

        vector_erase(*text, remove_pos, remove_size);
        render_surface_needs_redraw = true;
        return true;
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (vector_size(*text) <= 1 || hover_info.select_input_ind == 0) return false;

        int remove_pos = hover_info.select_input_ind - 1;
        int remove_size = 1;
        while (((unsigned char)(*text)[remove_pos] >> 6) == 2) { // This checks if we are in the middle of UTF-8 char
            remove_pos--;
            remove_size++;
        }

        vector_erase(*text, remove_pos, remove_size);
        render_surface_needs_redraw = true;
        hover_info.select_input_ind -= remove_size;
        return true;
    }

    bool input_changed = false;
    int char_val;
    while ((char_val = GetCharPressed())) {
        int utf_size = 0;
        const char* utf_char = CodepointToUTF8(char_val, &utf_size);
        for (int i = 0; i < utf_size; i++) {
            vector_insert(text, hover_info.select_input_ind++, utf_char[i]);
        }
        input_changed = true;
        render_surface_needs_redraw = true;
    }
    return input_changed;
}

PanelTree* find_panel(PanelTree* root, PanelType panel) {
    if (root->type == panel) return root;
    if (root->type == PANEL_SPLIT) {
        PanelTree* out = NULL;
        out = find_panel(root->left, panel);
        if (out) return out;
        out = find_panel(root->right, panel);
        if (out) return out;
    }

    return NULL;
}

#ifdef USE_INTERPRETER
static bool start_vm(void) {
#else
static bool start_vm(CompilerMode mode) {
#endif
    if (vm.is_running) return false;

    for (size_t i = 0; i < vector_size(code_tabs); i++) {
        if (find_panel(code_tabs[i].root_panel, PANEL_TERM)) {
#ifndef USE_INTERPRETER
            start_vm_mode = mode;
#endif
            if (current_tab != (int)i) {
                shader_time = 0.0;
                // Delay vm startup until next frame. Because this handler only runs after the layout is computed and
                // before the actual rendering begins, we need to add delay to vm startup to make sure the terminal buffer
                // is initialized and vm does not try to write to uninitialized buffer
                start_vm_timeout = 2;
            } else {
                start_vm_timeout = 1;
            }
            current_tab = i;
            render_surface_needs_redraw = true;
            break;
        }
    }
    return true;
}

static bool stop_vm(void) {
    if (!vm.is_running) return false;
    TraceLog(LOG_INFO, "STOP");
    exec_stop(&vm, &exec);
    render_surface_needs_redraw = true;
    return true;
}

static void deselect_all(void) {
    hover_info.select_argument = NULL;
    hover_info.select_input = NULL;
    dropdown.scroll_amount = 0;
}

static void show_dropdown(DropdownLocations location, char** list, int list_len, ButtonClickHandler handler) {
    hover_info.dropdown.location = location;
    hover_info.dropdown.list = list;
    hover_info.dropdown.list_len = list_len;
    hover_info.dropdown.handler = handler;
    hover_info.dropdown.select_ind = 0;
    hover_info.dropdown.scroll_amount = 0;
}

bool handle_dropdown_close(void) {
    hover_info.dropdown.location = LOCATION_NONE;
    hover_info.dropdown.list = NULL;
    hover_info.dropdown.list_len = 0;
    hover_info.dropdown.handler = NULL;
    hover_info.dropdown.select_ind = 0;
    hover_info.dropdown.scroll_amount = 0;
    hover_info.select_block = NULL;
    hover_info.select_input = NULL;
    hover_info.select_argument = NULL;
    return true;
}

static char* get_basename(char* path) {
    char* base_name = path;
    for (char* str = path; *str; str++) {
        if (*str == '/' || *str == '\\') {
            base_name = str + 1;
        }
    }
    return base_name;
}

bool handle_file_menu_click(void) {
    char const* filters[] = {"*.scrp"};
    char* path;
    char* base_path;
    int i;

    switch (hover_info.dropdown.select_ind) {
    case FILE_MENU_NEW_PROJECT:
        for (size_t i = 0; i < vector_size(editor_code); i++) blockchain_free(&editor_code[i]);
        vector_clear(editor_code);
        switch_tab_to_panel(PANEL_CODE);
        break;
    case FILE_MENU_SAVE_PROJECT:
        path = tinyfd_saveFileDialog(NULL, project_name, ARRLEN(filters), filters, "Scrap project files (.scrp)");
        if (!path) break;
        save_code(path, &project_conf, editor_code);

        base_path = get_basename(path);
        for (i = 0; base_path[i]; i++) project_name[i] = base_path[i];
        project_name[i] = 0;
        break;
    case FILE_MENU_LOAD_PROJECT:
        path = tinyfd_openFileDialog(NULL, project_name, ARRLEN(filters), filters, "Scrap project files (.scrp)", 0);
        if (!path) break;

        ProjectConfig new_config;
        BlockChain* chain = load_code(path, &new_config);
        switch_tab_to_panel(PANEL_CODE);
        if (!chain) {
            actionbar_show(gettext("File load failed :("));
            break;
        }

        project_config_free(&project_conf);
        project_conf = new_config;

        for (size_t i = 0; i < vector_size(editor_code); i++) blockchain_free(&editor_code[i]);
        vector_free(editor_code);
        exec_compile_error_block = NULL;
        exec_compile_error_blockchain = NULL;
        editor_code = chain;

        blockchain_select_counter = 0;
        camera_pos.x = editor_code[blockchain_select_counter].x - 50;
        camera_pos.y = editor_code[blockchain_select_counter].y - 50;

        base_path = get_basename(path);
        for (i = 0; base_path[i]; i++) project_name[i] = base_path[i];
        project_name[i] = 0;

        actionbar_show(gettext("File load succeeded!"));
        break;
    default:
        printf("idk\n");
        break;
    }
    return handle_dropdown_close();
}

bool handle_block_dropdown_click(void) {
    argument_set_const_string(hover_info.select_argument, hover_info.dropdown.list[hover_info.dropdown.select_ind]);
    return handle_dropdown_close();
}

bool handle_file_button_click(void) {
    if (vm.is_running) return true;
    show_dropdown(LOCATION_FILE_MENU, file_menu_list, ARRLEN(file_menu_list), handle_file_menu_click);
    return true;
}

bool handle_settings_button_click(void) {
    gui_window_show(GUI_TYPE_SETTINGS);
    return true;
}

bool handle_about_button_click(void) {
    gui_window_show(GUI_TYPE_ABOUT);
    return true;
}

bool handle_run_button_click(void) {
#ifdef USE_INTERPRETER
    start_vm();
#else
    start_vm(COMPILER_MODE_JIT);
#endif
    return true;
}

bool handle_build_button_click(void) {
    if (vm.is_running) return true;
    gui_window_show(GUI_TYPE_PROJECT_SETTINGS);
    return true;
}

bool handle_stop_button_click(void) {
    stop_vm();
    return true;
}

bool handle_project_settings_build_button_click(void) {
#ifdef USE_INTERPRETER
    start_vm();
#else
    start_vm(COMPILER_MODE_BUILD);
#endif
    gui_window_hide();
    return true;
}

bool handle_category_click(void) {
    palette.current_category = hover_info.category - palette.categories;
    assert(palette.current_category >= 0 && palette.current_category < (int)vector_size(palette.categories));
    return true;
}

bool handle_jump_to_block_button_click(void) {
    hover_info.select_block = exec_compile_error_block;
    hover_info.select_blockchain = exec_compile_error_blockchain;
    return true;
}

bool handle_error_window_close_button_click(void) {
    clear_compile_error();
    return true;
}

bool handle_tab_button(void) {
    current_tab = hover_info.tab;
    shader_time = 0.0;
    return true;
}

bool handle_add_tab_button(void) {
    char* name = "";
    switch (hover_info.mouse_panel) {
    case PANEL_NONE:
        name = "Unknown";
        break;
    case PANEL_CODE:
        name = "Code";
        break;
    case PANEL_BLOCK_PALETTE:
        name = "Block palette";
        break;
    case PANEL_TERM:
        name = "Output";
        break;
    case PANEL_BLOCK_CATEGORIES:
        name = "Block categories";
        break;
    case PANEL_SPLIT:
        name = "Multiple...";
        break;
    }

    tab_insert(name, panel_new(hover_info.mouse_panel), hover_info.tab);

    hover_info.mouse_panel = PANEL_NONE;
    current_tab = hover_info.tab;
    shader_time = 0.0;
    return true;
}

bool handle_window_gui_close_button_click(void) {
    gui_window_hide();
    return true;
}

bool handle_settings_panel_editor_button_click(void) {
    gui_window_hide();
    hover_info.is_panel_edit_mode = true;
    hover_info.select_input = NULL;
    hover_info.select_argument = NULL;
    hover_info.select_block = NULL;
    hover_info.select_blockchain = NULL;
    return true;
}

bool handle_settings_reset_button_click(void) {
    set_default_config(&window_conf);
    return true;
}

bool handle_settings_reset_panels_button_click(void) {
    delete_all_tabs();
    init_panels();
    current_tab = 0;
    return true;
}

bool handle_settings_apply_button_click(void) {
    apply_config(&conf, &window_conf);
    save_config(&window_conf);
    return true;
}

bool handle_left_slider_button_click(void) {
    *hover_info.hover_slider.value = MAX(*hover_info.hover_slider.value - 1, hover_info.hover_slider.min);
    return true;
}

bool handle_right_slider_button_click(void) {
    *hover_info.hover_slider.value = MIN(*hover_info.hover_slider.value + 1, hover_info.hover_slider.max);
    return true;
}

bool handle_settings_dropdown_button_click(void) {
    *hover_info.select_settings_dropdown_value = hover_info.dropdown.select_ind;
    return handle_dropdown_close();
}

bool handle_settings_dropdown_click(void) {
    hover_info.select_settings_dropdown_value = hover_info.settings_dropdown_data.value;
    show_dropdown(LOCATION_SETTINGS, hover_info.settings_dropdown_data.list, hover_info.settings_dropdown_data.list_len, handle_settings_dropdown_button_click);
    return true;
}

bool handle_about_license_button_click(void) {
    OpenURL(LICENSE_URL);
    return true;
}

bool handle_panel_editor_save_button(void) {
    hover_info.is_panel_edit_mode = false;
    save_config(&conf);
    return true;
}

bool handle_panel_editor_cancel_button(void) {
    hover_info.is_panel_edit_mode = false;
    return true;
}

bool handle_editor_add_arg_button(void) {
    Blockdef* blockdef = hover_info.argument->data.blockdef;
    size_t last_input = vector_size(blockdef->inputs);
    char str[32];

    // TODO: Update block arguments when new argument is added
    if (blockdef->ref_count > 1) {
        deselect_all();
        return true;
    }
    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
        if (blockdef->inputs[i].data.arg.blockdef->ref_count > 1) {
            deselect_all();
            return true;
        }
    }

    blockdef_add_argument(blockdef, "", gettext("any"), BLOCKCONSTR_UNLIMITED);

    sprintf(str, "arg%zu", last_input);
    Blockdef* arg_blockdef = blockdef->inputs[last_input].data.arg.blockdef;
    blockdef_add_text(arg_blockdef, str);
    arg_blockdef->func = block_custom_arg;

    int arg_count = 0;
    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (blockdef->inputs[i].type == INPUT_ARGUMENT) arg_count++;
    }

    deselect_all();
    return true;
}

bool handle_editor_add_text_button(void) {
    Blockdef* blockdef = hover_info.argument->data.blockdef;
    size_t last_input = vector_size(blockdef->inputs);
    char str[32];

    // TODO: Update block arguments when new argument is added
    if (blockdef->ref_count > 1) {
        deselect_all();
        return true;
    }
    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
        if (blockdef->inputs[i].data.arg.blockdef->ref_count > 1) {
            deselect_all();
            return true;
        }
    }

    sprintf(str, "text%zu", last_input);
    blockdef_add_text(blockdef, str);

    deselect_all();
    return true;
}

bool handle_editor_del_arg_button(void) {
    Blockdef* blockdef = hover_info.argument->data.blockdef;

    assert(hover_info.editor.blockdef_input != (size_t)-1);
    if (blockdef->ref_count > 1) {
        deselect_all();
        return true;
    }
    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
        if (blockdef->inputs[i].data.arg.blockdef->ref_count > 1) {
            deselect_all();
            return true;
        }
    }

    blockdef_delete_input(blockdef, hover_info.editor.blockdef_input);

    deselect_all();
    return true;
}

bool handle_editor_edit_button(void) {
    hover_info.editor.edit_blockdef = hover_info.argument->data.blockdef;
    hover_info.editor.edit_block = hover_info.block;
    deselect_all();
    return true;
}

bool handle_editor_close_button(void) {
    hover_info.editor.edit_blockdef = NULL;
    hover_info.editor.edit_block = NULL;
    deselect_all();
    return true;
}

static bool handle_block_palette_click(bool mouse_empty) {
    if (hover_info.select_argument) {
        deselect_all();
        return true;
    }
    if (mouse_empty && hover_info.block) {
        // Pickup block
        TraceLog(LOG_INFO, "Pickup block");
        int ind = hover_info.block - palette.categories[palette.current_category].blocks;
        if (ind < 0 || ind > (int)vector_size(palette.categories[palette.current_category].blocks)) return true;

        blockchain_add_block(&mouse_blockchain, block_new_ms(hover_info.block->blockdef));
        if (hover_info.block->blockdef->type == BLOCKTYPE_CONTROL && vm.end_blockdef) {
            blockchain_add_block(&mouse_blockchain, block_new_ms(vm.blockdefs[vm.end_blockdef]));
        }
        return true;
    } else if (!mouse_empty) {
        // Drop block
        TraceLog(LOG_INFO, "Drop block");
        for (size_t i = 0; i < vector_size(mouse_blockchain.blocks); i++) {
            for (size_t j = 0; j < vector_size(mouse_blockchain.blocks[i].arguments); j++) {
                Argument* arg = &mouse_blockchain.blocks[i].arguments[j];
                if (arg->type != ARGUMENT_BLOCKDEF) continue;
                if (arg->data.blockdef->ref_count > 1) editor_code_remove_blockdef(arg->data.blockdef);
                for (size_t k = 0; k < vector_size(arg->data.blockdef->inputs); k++) {
                    Input* input = &arg->data.blockdef->inputs[k];
                    if (input->type != INPUT_ARGUMENT) continue;
                    if (input->data.arg.blockdef->ref_count > 1) editor_code_remove_blockdef(input->data.arg.blockdef);
                }
            }
        }
        blockchain_clear_blocks(&mouse_blockchain);
        return true;
    }
    return true;
}

static bool handle_blockdef_editor_click(void) {
    if (!hover_info.editor.blockdef) return true;
    if (hover_info.editor.edit_blockdef == hover_info.argument->data.blockdef) return false;
    blockchain_add_block(&mouse_blockchain, block_new_ms(hover_info.editor.blockdef));
    deselect_all();
    return true;
}

static bool handle_code_editor_click(bool mouse_empty) {
    if (!mouse_empty) {
        mouse_blockchain.x = gui->mouse_x;
        mouse_blockchain.y = gui->mouse_y;
        if (hover_info.argument || hover_info.prev_argument) {
            if (vector_size(mouse_blockchain.blocks) > 1) return true;
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_CONTROLEND) return true;
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return true;

            if (hover_info.argument) {
                // Attach to argument
                TraceLog(LOG_INFO, "Attach to argument");
                if (hover_info.argument->type != ARGUMENT_TEXT) return true;
                mouse_blockchain.blocks[0].parent = hover_info.block;
                argument_set_block(hover_info.argument, mouse_blockchain.blocks[0]);
                vector_clear(mouse_blockchain.blocks);
                hover_info.select_blockchain = hover_info.blockchain;
                hover_info.select_block = &hover_info.argument->data.block;
                hover_info.select_input = NULL;
            } else if (hover_info.prev_argument) {
                // Swap argument
                TraceLog(LOG_INFO, "Swap argument");
                if (hover_info.prev_argument->type != ARGUMENT_BLOCK) return true;
                mouse_blockchain.blocks[0].parent = hover_info.block->parent;
                Block temp = mouse_blockchain.blocks[0];
                mouse_blockchain.blocks[0] = *hover_info.block;
                mouse_blockchain.blocks[0].parent = NULL;
                block_update_parent_links(&mouse_blockchain.blocks[0]);
                argument_set_block(hover_info.prev_argument, temp);
                hover_info.select_block = &hover_info.prev_argument->data.block;
                hover_info.select_blockchain = hover_info.blockchain;
            }
        } else if (
            hover_info.block &&
            hover_info.blockchain &&
            hover_info.block->parent == NULL
        ) {
            // Attach block
            TraceLog(LOG_INFO, "Attach block");
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return true;

            int ind = hover_info.block - hover_info.blockchain->blocks;
            blockchain_insert(hover_info.blockchain, &mouse_blockchain, ind);
            // Update block link to make valgrind happy
            hover_info.block = &hover_info.blockchain->blocks[ind];
            hover_info.select_block = hover_info.block + 1;
            hover_info.select_blockchain = hover_info.blockchain;
        } else {
            // Put block
            TraceLog(LOG_INFO, "Put block");
            mouse_blockchain.x += camera_pos.x - hover_info.panel_size.x;
            mouse_blockchain.y += camera_pos.y - hover_info.panel_size.y;
            vector_add(&editor_code, mouse_blockchain);
            mouse_blockchain = blockchain_new();
            hover_info.select_blockchain = &editor_code[vector_size(editor_code) - 1];
            hover_info.select_block = &hover_info.select_blockchain->blocks[0];
        }
        return true;
    } else if (hover_info.block) {
        if (hover_info.block->parent) {
            if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                // Copy argument
                TraceLog(LOG_INFO, "Copy argument");
                blockchain_add_block(&mouse_blockchain, block_copy(hover_info.block, NULL));
            } else {
                // Detach argument
                TraceLog(LOG_INFO, "Detach argument");
                assert(hover_info.prev_argument != NULL);

                blockchain_add_block(&mouse_blockchain, *hover_info.block);
                mouse_blockchain.blocks[0].parent = NULL;

                argument_set_text(hover_info.prev_argument, "");
                hover_info.select_blockchain = NULL;
                hover_info.select_block = NULL;
            }
        } else if (hover_info.blockchain) {
            int ind = hover_info.block - hover_info.blockchain->blocks;

            if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Copy block
                    TraceLog(LOG_INFO, "Copy block");
                    blockchain_free(&mouse_blockchain);

                    mouse_blockchain = blockchain_copy_single(hover_info.blockchain, ind);
                } else {
                    // Copy chain
                    TraceLog(LOG_INFO, "Copy chain");
                    blockchain_free(&mouse_blockchain);
                    mouse_blockchain = blockchain_copy(hover_info.blockchain, ind);
                }
            } else {
                hover_info.editor.edit_blockdef = NULL;
                hover_info.editor.edit_block = NULL;
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Detach block
                    TraceLog(LOG_INFO, "Detach block");
                    blockchain_detach_single(&mouse_blockchain, hover_info.blockchain, ind);
                    if (vector_size(hover_info.blockchain->blocks) == 0) {
                        blockchain_free(hover_info.blockchain);
                        vector_remove(editor_code, hover_info.blockchain - editor_code);
                        hover_info.block = NULL;
                    }
                } else {
                    // Detach chain
                    TraceLog(LOG_INFO, "Detach chain");
                    int ind = hover_info.block - hover_info.blockchain->blocks;
                    blockchain_detach(&mouse_blockchain, hover_info.blockchain, ind);
                    if (ind == 0) {
                        blockchain_free(hover_info.blockchain);
                        vector_remove(editor_code, hover_info.blockchain - editor_code);
                        hover_info.block = NULL;
                    }
                }
                hover_info.select_blockchain = NULL;
                hover_info.select_block = NULL;
            }
        }
        return true;
    }
    return false;
}

static bool handle_editor_panel_click(void) {
    if (!hover_info.panel) return true;

    if (hover_info.panel->type == PANEL_SPLIT) {
        hover_info.drag_panel = hover_info.panel;
        hover_info.drag_panel_size = hover_info.panel_size;
        return false;
    }

    if (hover_info.mouse_panel == PANEL_NONE) {
        PanelTree* parent = hover_info.panel->parent;
        if (!parent) {
            if (vector_size(code_tabs) > 1) {
                hover_info.mouse_panel = hover_info.panel->type;
                tab_delete(current_tab);
            }
            return true;
        }

        hover_info.mouse_panel = hover_info.panel->type;
        free(hover_info.panel);
        PanelTree* other_panel = parent->left == hover_info.panel ? parent->right : parent->left;

        parent->type = other_panel->type;
        parent->split_percent = other_panel->split_percent;
        parent->direction = other_panel->direction;
        parent->left = other_panel->left;
        parent->right = other_panel->right;
        if (other_panel->type == PANEL_SPLIT) {
            parent->left->parent = parent;
            parent->right->parent = parent;
        }
        free(other_panel);
    } else {
        panel_split(hover_info.panel, hover_info.panel_side, hover_info.mouse_panel, 0.5);
        hover_info.mouse_panel = PANEL_NONE;
    }

    return true;
}

static void get_input_ind(void) {
    assert(hover_info.input_info.font != NULL);
    assert(hover_info.input_info.input != NULL);

    float width = 0.0;
    float prev_width = 0.0;

    int codepoint = 0; // Current character
    int index = 0; // Index position in sprite font
    int text_size = strlen(*hover_info.input_info.input);
    float scale_factor = hover_info.input_info.font_size / (float)hover_info.input_info.font->baseSize;

    int prev_i = 0;
    int i = 0;

    while (i < text_size && (width * scale_factor) < hover_info.input_info.rel_pos.x) {
        int next = 0;
        codepoint = GetCodepointNext(&(*hover_info.input_info.input)[i], &next);
        index = search_glyph(codepoint);

        prev_width = width;
        prev_i = i;

        if (hover_info.input_info.font->glyphs[index].advanceX != 0) {
            width += hover_info.input_info.font->glyphs[index].advanceX;
        } else {
            width += hover_info.input_info.font->recs[index].width + hover_info.input_info.font->glyphs[index].offsetX;
        }
        i += next;
    }
    prev_width *= scale_factor;
    width *= scale_factor;

    if (width - hover_info.input_info.rel_pos.x < hover_info.input_info.rel_pos.x - prev_width) { // Right side of char is closer
        hover_info.select_input_ind = i;
    } else {
        hover_info.select_input_ind = prev_i;
    }
}

// Return value indicates if we should cancel dragging
static bool handle_mouse_click(void) {
    hover_info.mouse_click_pos = (Vector2) { gui->mouse_x, gui->mouse_y };
    camera_click_pos = camera_pos;
    hover_info.dragged_slider.value = NULL;

    if (hover_info.select_input == &search_list_search) {
        if (hover_info.block) {
            blockchain_add_block(&mouse_blockchain, block_new_ms(hover_info.block->blockdef));
            if (hover_info.block->blockdef->type == BLOCKTYPE_CONTROL && vm.end_blockdef) {
                blockchain_add_block(&mouse_blockchain, block_new_ms(vm.blockdefs[vm.end_blockdef]));
            }
        }

        hover_info.select_input = NULL;
        hover_info.block = NULL;
        return true;
    }

    if (hover_info.top_bars.handler) return hover_info.top_bars.handler();
    if (hover_info.hover_slider.value) {
        hover_info.dragged_slider = hover_info.hover_slider;
        hover_info.slider_last_val = *hover_info.dragged_slider.value;
        return false;
    }
    if (gui_window_is_shown()) {
        if (hover_info.input_info.input) get_input_ind();
        if (hover_info.input_info.input != hover_info.select_input) hover_info.select_input = hover_info.input_info.input;
        return true;
    }
    if (!hover_info.panel) return true;
    if (hover_info.is_panel_edit_mode) return handle_editor_panel_click();
    if (hover_info.panel->type == PANEL_TERM) return true;
    if (vm.is_running) return false;

    if (hover_info.input_info.input) get_input_ind();
    if (hover_info.input_info.input != hover_info.select_input) hover_info.select_input = hover_info.input_info.input;

    bool mouse_empty = vector_size(mouse_blockchain.blocks) == 0;

    if (hover_info.panel->type == PANEL_BLOCK_PALETTE) return handle_block_palette_click(mouse_empty);

    if (mouse_empty && hover_info.argument && hover_info.argument->type == ARGUMENT_BLOCKDEF) {
        if (handle_blockdef_editor_click()) return true;
    }

    if (mouse_empty) {
        if (hover_info.block && hover_info.argument) {
            Input block_input = hover_info.block->blockdef->inputs[hover_info.argument->input_id];
            if (block_input.type == INPUT_DROPDOWN) {
                size_t list_len = 0;
                char** list = block_input.data.drop.list(hover_info.block, &list_len);

                show_dropdown(LOCATION_BLOCK_DROPDOWN, list, list_len, handle_block_dropdown_click);
            }
        }
        if (hover_info.blockchain != hover_info.select_blockchain) {
            hover_info.select_blockchain = hover_info.blockchain;
            if (hover_info.select_blockchain) blockchain_select_counter = hover_info.select_blockchain - editor_code;
        }
        if (hover_info.block != hover_info.select_block) hover_info.select_block = hover_info.block;
        if (hover_info.argument != hover_info.select_argument) {
            if (!hover_info.argument || hover_info.input_info.input || hover_info.dropdown.location != LOCATION_NONE) hover_info.select_argument = hover_info.argument;
            dropdown.scroll_amount = 0;
            return true;
        }
        if (hover_info.select_argument) return true;
    }

    if (hover_info.panel->type == PANEL_CODE && handle_code_editor_click(mouse_empty)) return true;
    return hover_info.panel->type != PANEL_CODE;
}

static void block_next_argument() {
    Argument* args = hover_info.select_block->arguments;
    Argument* arg = hover_info.select_argument ? hover_info.select_argument + 1 : &args[0];
    if (arg - args >= (int)vector_size(args)) {
        if (hover_info.select_block->parent) {
            Argument* parent_args = hover_info.select_block->parent->arguments;
            for (size_t i = 0; i < vector_size(parent_args); i++) {
                if (parent_args[i].type == ARGUMENT_BLOCK && &parent_args[i].data.block == hover_info.select_block) hover_info.select_argument = &parent_args[i];
            }
            hover_info.select_block = hover_info.select_block->parent;
            block_next_argument();
        } else {
            hover_info.select_argument = NULL;
        }
        return;
    }

    if (arg->type == ARGUMENT_TEXT || arg->type == ARGUMENT_CONST_STRING) {
        hover_info.select_argument = arg;
    } else if (arg->type == ARGUMENT_BLOCK) {
        hover_info.select_argument = NULL;
        hover_info.select_block = &arg->data.block;
    }
}

static void block_prev_argument() {
    Argument* args = hover_info.select_block->arguments;
    Argument* arg = hover_info.select_argument ? hover_info.select_argument - 1 : &args[-1];
    if (arg - args < 0) {
        if (hover_info.select_argument) {
            hover_info.select_argument = NULL;
            return;
        }
        if (hover_info.select_block->parent) {
            Argument* parent_args = hover_info.select_block->parent->arguments;
            for (size_t i = 0; i < vector_size(parent_args); i++) {
                if (parent_args[i].type == ARGUMENT_BLOCK && &parent_args[i].data.block == hover_info.select_block) hover_info.select_argument = &parent_args[i];
            }
            hover_info.select_block = hover_info.select_block->parent;
            block_prev_argument();
        } else {
            hover_info.select_argument = NULL;
        }
        return;
    }

    if (arg->type == ARGUMENT_TEXT || arg->type == ARGUMENT_CONST_STRING) {
        hover_info.select_argument = arg;
    } else if (arg->type == ARGUMENT_BLOCK) {
        hover_info.select_argument = NULL;
        hover_info.select_block = &arg->data.block;
        while (vector_size(hover_info.select_block->arguments) != 0) {
            arg = &hover_info.select_block->arguments[vector_size(hover_info.select_block->arguments) - 1];
            if (arg->type == ARGUMENT_TEXT || arg->type == ARGUMENT_CONST_STRING) {
                hover_info.select_argument = arg;
                break;
            } else if (arg->type == ARGUMENT_BLOCK) {
                hover_info.select_block = &arg->data.block;
            }
        }
    }
}

static bool handle_code_panel_key_press(void) {
    if (hover_info.select_argument && !hover_info.select_input) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            hover_info.select_input = &hover_info.select_argument->data.text;
            hover_info.select_input_ind = strlen(*hover_info.select_input);
            render_surface_needs_redraw = true;
            return true;
        }
    }

    if (IsKeyPressed(KEY_TAB) && vector_size(editor_code) > 0) {
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            blockchain_select_counter--;
            if (blockchain_select_counter < 0) blockchain_select_counter = vector_size(editor_code) - 1;
        } else {
            blockchain_select_counter++;
            if ((vec_size_t)blockchain_select_counter >= vector_size(editor_code)) blockchain_select_counter = 0;
        }

        hover_info.select_input = NULL;
        hover_info.select_argument = NULL;
        hover_info.select_block = &editor_code[blockchain_select_counter].blocks[0];
        hover_info.select_blockchain = &editor_code[blockchain_select_counter];
        camera_pos.x = editor_code[blockchain_select_counter].x - 50;
        camera_pos.y = editor_code[blockchain_select_counter].y - 50;
        actionbar_show(TextFormat(gettext("Jump to chain (%d/%d)"), blockchain_select_counter + 1, vector_size(editor_code)));
        render_surface_needs_redraw = true;
        return true;
    }

    if (!hover_info.select_blockchain || !hover_info.select_block || hover_info.select_input) return false;

    int bounds_x = MIN(200, hover_info.code_panel_bounds.width / 2);
    int bounds_y = MIN(200, hover_info.code_panel_bounds.height / 2);

    if (hover_info.select_block_pos.x - (hover_info.code_panel_bounds.x + hover_info.code_panel_bounds.width) > -bounds_x) {
        camera_pos.x += hover_info.select_block_pos.x - (hover_info.code_panel_bounds.x + hover_info.code_panel_bounds.width) + bounds_x;
        render_surface_needs_redraw = true;
    }

    if (hover_info.select_block_pos.x - hover_info.code_panel_bounds.x < bounds_x) {
        camera_pos.x += hover_info.select_block_pos.x - hover_info.code_panel_bounds.x - bounds_x;
        render_surface_needs_redraw = true;
    }

    if (hover_info.select_block_pos.y - (hover_info.code_panel_bounds.y + hover_info.code_panel_bounds.height) > -bounds_y) {
        camera_pos.y += hover_info.select_block_pos.y - (hover_info.code_panel_bounds.y + hover_info.code_panel_bounds.height) + bounds_y;
        render_surface_needs_redraw = true;
    }

    if (hover_info.select_block_pos.y - hover_info.code_panel_bounds.y < bounds_y) {
        camera_pos.y += hover_info.select_block_pos.y - hover_info.code_panel_bounds.y - bounds_y;
        render_surface_needs_redraw = true;
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        block_next_argument();
        render_surface_needs_redraw = true;
        return true;
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        block_prev_argument();
        render_surface_needs_redraw = true;
        return true;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) {
        while (hover_info.select_block->parent) hover_info.select_block = hover_info.select_block->parent;
        hover_info.select_block--;
        hover_info.select_argument = NULL;
        if (hover_info.select_block < hover_info.select_blockchain->blocks) hover_info.select_block = hover_info.select_blockchain->blocks;
        render_surface_needs_redraw = true;
        return true;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) {
        while (hover_info.select_block->parent) hover_info.select_block = hover_info.select_block->parent;
        hover_info.select_block++;
        hover_info.select_argument = NULL;
        if (hover_info.select_block - hover_info.select_blockchain->blocks >= (int)vector_size(hover_info.select_blockchain->blocks)) {
            hover_info.select_block--;
        }
        render_surface_needs_redraw = true;
        return true;
    }

    return false;
}

static bool search_string(const char* str, const char* substr) {
    if (*substr == 0) return true;

    int next_ch, next_subch, cur_ch, cur_subch;
    char* cur_substr = (char*)substr;
    char* cur_str = (char*)str;

    while (*cur_str != 0 && *cur_substr != 0) {
        cur_ch = GetCodepointNext(cur_str, &next_ch);
        cur_subch = GetCodepointNext(cur_substr, &next_subch);

        if (towlower(cur_ch) == towlower(cur_subch)) {
            cur_substr += next_subch;
            cur_str += next_ch;
        } else {
            if (cur_substr == substr) cur_str += next_ch;
            cur_substr = (char*)substr;
        }
    }
    return *cur_substr == 0;
}

static bool search_blockdef(Blockdef* blockdef) {
    if (search_string(blockdef->id, search_list_search)) return true;
    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (blockdef->inputs[i].type != INPUT_TEXT_DISPLAY) continue;
        if (search_string(blockdef->inputs[i].data.text, search_list_search)) return true;
    }
    return false;
}

void update_search(void) {
    vector_clear(search_list);
    for (size_t i = 0; i < vector_size(palette.categories); i++) {
        for (size_t j = 0; j < vector_size(palette.categories[i].blocks); j++) {
            if (search_blockdef(palette.categories[i].blocks[j].blockdef)) vector_add(&search_list, &palette.categories[i].blocks[j]);
        }
    }
}

static void handle_key_press(void) {
    if (IsKeyPressed(KEY_F5)) {
#ifdef USE_INTERPRETER
        start_vm();
#else
        start_vm(COMPILER_MODE_JIT);
#endif
        return;
    }
    if (IsKeyPressed(KEY_F6)) {
        stop_vm();
        return;
    }
    if (IsKeyPressed(KEY_S) &&
        hover_info.select_input != &search_list_search &&
        vector_size(mouse_blockchain.blocks) == 0 &&
        !hover_info.is_panel_edit_mode &&
        hover_info.panel &&
        hover_info.panel->type == PANEL_CODE &&
        !vm.is_running &&
        !gui_window_is_shown() &&
        !hover_info.select_input)
    {
        vector_clear(search_list_search);
        vector_add(&search_list_search, 0);
        hover_info.select_input = &search_list_search;
        hover_info.select_input_ind = 0;
        render_surface_needs_redraw = true;
        update_search();
        return;
    }

    if (hover_info.panel) {
        if (hover_info.panel->type == PANEL_TERM) {
            if (!vm.is_running) return;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                term_input_put_char('\n');
                term_print_str("\n");
                render_surface_needs_redraw = true;
                return;
            }

            int char_val;
            while ((char_val = GetCharPressed())) {
                int utf_size = 0;
                const char* utf_char = CodepointToUTF8(char_val, &utf_size);
                for (int i = 0; i < utf_size; i++) {
                    term_input_put_char(utf_char[i]);
                }
                // CodepointToUTF8() returns an array, not a null terminated string, so we copy it to satisfy constraints
                char utf_str[7];
                memcpy(utf_str, utf_char, utf_size);
                utf_str[utf_size] = 0;
                term_print_str(utf_str);
                render_surface_needs_redraw = true;
            }
            return;
        } else if (hover_info.panel->type == PANEL_CODE) {
            if (handle_code_panel_key_press()) return;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        hover_info.select_input = NULL;
        hover_info.select_argument = NULL;
        render_surface_needs_redraw = true;
        return;
    }
    if (hover_info.select_block && hover_info.select_argument && hover_info.select_block->blockdef->inputs[hover_info.select_argument->input_id].type == INPUT_DROPDOWN) return;

    if (edit_text(hover_info.select_input)) {
        if (hover_info.select_input == &search_list_search) update_search();
    }
}

static void handle_mouse_wheel(void) {
    if (!hover_info.panel) return;
    if (hover_info.panel->type != PANEL_CODE) return;
    if (hover_info.select_argument) return;
    if (hover_info.is_panel_edit_mode) return;
    if (hover_info.select_input) return;
    if (gui_window_is_shown()) return;

    Vector2 wheel = GetMouseWheelMoveV();
    camera_pos.x -= wheel.x * conf.font_size * 2;
    camera_pos.y -= wheel.y * conf.font_size * 2;

    if (wheel.x != 0 || wheel.y != 0) {
        hover_info.select_block = NULL;
        hover_info.select_argument = NULL;
        hover_info.select_input = NULL;
        hover_info.select_blockchain = NULL;
    }
}

static void handle_mouse_drag(void) {
    if (hover_info.drag_cancelled) return;

    if (hover_info.is_panel_edit_mode && hover_info.drag_panel && hover_info.drag_panel->type == PANEL_SPLIT) {
        if (hover_info.drag_panel->direction == DIRECTION_HORIZONTAL) {
            hover_info.drag_panel->split_percent = CLAMP(
                (gui->mouse_x - hover_info.drag_panel_size.x - 5) / hover_info.drag_panel_size.width,
                0.0,
                1.0 - (10.0 / hover_info.drag_panel_size.width)
            );
        } else {
            hover_info.drag_panel->split_percent = CLAMP(
                (gui->mouse_y - hover_info.drag_panel_size.y - 5) / hover_info.drag_panel_size.height,
                0.0,
                1.0 - (10.0 / hover_info.drag_panel_size.height)
            );
        }
        return;
    }

    if (hover_info.dragged_slider.value) {
        *hover_info.dragged_slider.value = CLAMP(
            hover_info.slider_last_val + (gui->mouse_x - hover_info.mouse_click_pos.x) / 2,
            hover_info.dragged_slider.min,
            hover_info.dragged_slider.max
        );
        return;
    }

    camera_pos.x = camera_click_pos.x - (gui->mouse_x - hover_info.mouse_click_pos.x);
    camera_pos.y = camera_click_pos.y - (gui->mouse_y - hover_info.mouse_click_pos.y);
}

void scrap_gui_process_input(void) {
    int prev_mouse_scroll = gui->mouse_scroll;
    gui_update_mouse_scroll(gui, GetMouseWheelMove());
    if (prev_mouse_scroll != gui->mouse_scroll) render_surface_needs_redraw = true;

    if (IsWindowResized()) {
        shader_time = 0.0;
        gui_update_window_size(gui, GetScreenWidth(), GetScreenHeight());
        UnloadRenderTexture(render_surface);
        render_surface = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        SetTextureWrap(render_surface.texture, TEXTURE_WRAP_MIRROR_REPEAT);
        render_surface_needs_redraw = true;
    }

    Vector2 delta = GetMouseDelta();
    if (delta.x != 0 || delta.y != 0) render_surface_needs_redraw = true;

    if (GetMouseWheelMove() != 0.0) {
        handle_mouse_wheel();
        render_surface_needs_redraw = true;
    }

#ifdef ARABIC_MODE
    gui_update_mouse_pos(gui, gui->win_w - GetMouseX(), GetMouseY());
#else
    gui_update_mouse_pos(gui, GetMouseX(), GetMouseY());
#endif

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        hover_info.drag_cancelled = handle_mouse_click();
        render_surface_needs_redraw = true;
#ifdef DEBUG
        // This will traverse through all blocks in codebase, which is expensive in large codebase.
        // Ideally all functions should not be broken in the first place. This helps with debugging invalid states
        sanitize_links();
#endif
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        hover_info.mouse_click_pos = (Vector2) { gui->mouse_x, gui->mouse_y };
        camera_click_pos = camera_pos;
        hover_info.select_block = NULL;
        hover_info.select_argument = NULL;
        hover_info.select_input = NULL;
        hover_info.select_blockchain = NULL;
        render_surface_needs_redraw = true;
    } else if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        handle_mouse_drag();
    } else {
        hover_info.drag_cancelled = false;
        hover_info.dragged_slider.value = NULL;
        hover_info.drag_panel = NULL;
        handle_key_press();
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) render_surface_needs_redraw = true;

    handle_window();

    if (render_surface_needs_redraw) {
        hover_info.block = NULL;
        hover_info.argument = NULL;
        hover_info.input_info.input = NULL;
        hover_info.category = NULL;
        hover_info.prev_argument = NULL;
        hover_info.prev_blockchain = NULL;
        hover_info.blockchain = NULL;
        hover_info.editor.part = EDITOR_NONE;
        hover_info.editor.blockdef = NULL;
        hover_info.editor.blockdef_input = -1;
        hover_info.top_bars.handler = NULL;
        hover_info.hover_slider.value = NULL;
        hover_info.panel = NULL;
        hover_info.panel_size = (Rectangle) {0};
        hover_info.tab = -1;
        hover_info.select_valid = false;

#ifdef DEBUG
        Timer t = start_timer("gui process");
#endif
        scrap_gui_process();
#ifdef DEBUG
        ui_time = end_timer(t);
#endif

        if (start_vm_timeout >= 0) start_vm_timeout--;
        // This fixes selecting wrong argument of a block when two blocks overlap
        if (hover_info.block && hover_info.argument) {
            int ind = hover_info.argument - hover_info.block->arguments;
            if (ind < 0 || ind > (int)vector_size(hover_info.block->arguments)) hover_info.argument = NULL;
        }

        if (hover_info.select_block && !hover_info.select_valid) {
            TraceLog(LOG_WARNING, "Invalid selection: %p", hover_info.select_block);
            hover_info.select_block = NULL;
            hover_info.select_blockchain = NULL;
        }
    }

    hover_info.prev_block = hover_info.block;
    hover_info.prev_panel = hover_info.panel;
    hover_info.editor.prev_blockdef = hover_info.editor.blockdef;
}
