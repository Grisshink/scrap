// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-2025 Grisshink
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

static void set_mark(void) {
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        if (hover.select_input_mark == -1) hover.select_input_mark = hover.select_input_cursor;
    } else {
        hover.select_input_mark = -1;
    }
}

static void copy_text(char* text, int start, int end) {
    char* clipboard = vector_create();
    for (int i = start; i < end; i++) vector_add(&clipboard, text[i]);
    vector_add(&clipboard, 0);

    SetClipboardText(clipboard);
    vector_free(clipboard);
}

static void delete_region(char** text) {
    if (hover.select_input_mark == -1) return;

    int remove_pos  = MIN(hover.select_input_cursor, hover.select_input_mark),
        remove_size = ABS(hover.select_input_cursor - hover.select_input_mark);
    hover.select_input_mark = -1;
    hover.select_input_cursor = remove_pos;
    vector_erase(*text, remove_pos, remove_size);
    render_surface_needs_redraw = true;
}

static bool edit_text(char** text) {
    if (!text) return false;

    if (IsKeyPressed(KEY_HOME)) {
        set_mark();
        hover.select_input_cursor = 0;
        render_surface_needs_redraw = true;
        return false;
    }

    if (IsKeyPressed(KEY_END)) {
        set_mark();
        hover.select_input_cursor = vector_size(*text) - 1;
        render_surface_needs_redraw = true;
        return false;
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A)) {
        hover.select_input_cursor = 0;
        hover.select_input_mark = strlen(*text);
        render_surface_needs_redraw = true;
        return false;
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_U)) {
        vector_clear(*text);
        vector_add(text, 0);
        hover.select_input_cursor = 0;
        hover.select_input_mark = -1;
        render_surface_needs_redraw = true;
        return true;
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_C)) {
        if (hover.select_input_mark != -1) {
            copy_text(*text, MIN(hover.select_input_cursor, hover.select_input_mark),
                             MAX(hover.select_input_cursor, hover.select_input_mark));
        }
        return false;
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V)) {
        const char* clipboard = GetClipboardText();
        if (clipboard) {
            delete_region(text);

            for (int i = 0; clipboard[i]; i++) {
                if (clipboard[i] == '\n' || clipboard[i] == '\r') continue;
                vector_insert(text, hover.select_input_cursor++, clipboard[i]);
            }
            render_surface_needs_redraw = true;
            return true;
        }
        return false;
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_X)) {
        if (hover.select_input_mark != -1) {
            int sel_start = MIN(hover.select_input_cursor, hover.select_input_mark),
                sel_end   = MAX(hover.select_input_cursor, hover.select_input_mark);

            copy_text(*text, sel_start, sel_end);
            delete_region(text);
            return true;
        }
        return false;
    }

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        set_mark();
        hover.select_input_cursor--;
        if (hover.select_input_cursor < 0) {
            hover.select_input_cursor = 0;
        } else {
            while (((unsigned char)(*text)[hover.select_input_cursor] >> 6) == 2) hover.select_input_cursor--;
        }
        render_surface_needs_redraw = true;
        return false;
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        set_mark();
        hover.select_input_cursor++;
        if (hover.select_input_cursor >= (int)vector_size(*text)) {
            hover.select_input_cursor = vector_size(*text) - 1;
        } else {
            while (((unsigned char)(*text)[hover.select_input_cursor] >> 6) == 2) hover.select_input_cursor++;
        }
        render_surface_needs_redraw = true;
        return false;
    }

    if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
        if (vector_size(*text) <= 1 || (hover.select_input_cursor == (int)vector_size(*text) - 1 && hover.select_input_mark == -1)) return false;

        if (hover.select_input_mark != -1) {
            delete_region(text);
        } else {
            int remove_pos = hover.select_input_cursor;
            int remove_size;
            GetCodepointNext(*text + remove_pos, &remove_size);
            vector_erase(*text, remove_pos, remove_size);
            render_surface_needs_redraw = true;
        }
        return true;
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (vector_size(*text) <= 1 || (hover.select_input_cursor == 0 && hover.select_input_mark == -1)) {
            return false;
        }

        if (hover.select_input_mark != -1) {
            delete_region(text);
        } else {
            int remove_pos = hover.select_input_cursor - 1;
            int remove_size = 1;
            while (((unsigned char)(*text)[remove_pos] >> 6) == 2) { // This checks if we are in the middle of UTF-8 char
                remove_pos--;
                remove_size++;
            }
            hover.select_input_cursor -= remove_size;
            vector_erase(*text, remove_pos, remove_size);
            render_surface_needs_redraw = true;
        }

        return true;
    }

    bool input_changed = false;
    int char_val;
    while ((char_val = GetCharPressed())) {
        delete_region(text);
        int utf_size = 0;
        const char* utf_char = CodepointToUTF8(char_val, &utf_size);
        for (int i = 0; i < utf_size; i++) {
            vector_insert(text, hover.select_input_cursor++, utf_char[i]);
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
bool start_vm(void) {
#else
bool start_vm(CompilerMode mode) {
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
    hover.editor.select_argument = NULL;
    hover.select_input = NULL;
    dropdown.scroll_amount = 0;
}

void show_dropdown(DropdownLocations location, char** list, int list_len, ButtonClickHandler handler) {
    hover.dropdown.location = location;
    hover.dropdown.list = list;
    hover.dropdown.list_len = list_len;
    hover.dropdown.handler = handler;
    hover.dropdown.select_ind = 0;
    hover.dropdown.scroll_amount = 0;
}

bool handle_dropdown_close(void) {
    hover.dropdown.location = LOCATION_NONE;
    hover.dropdown.list = NULL;
    hover.dropdown.list_len = 0;
    hover.dropdown.handler = NULL;
    hover.dropdown.select_ind = 0;
    hover.dropdown.scroll_amount = 0;
    hover.editor.select_block = NULL;
    hover.select_input = NULL;
    hover.editor.select_argument = NULL;
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

    switch (hover.dropdown.select_ind) {
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
    argument_set_const_string(hover.editor.select_argument, hover.dropdown.list[hover.dropdown.select_ind]);
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

bool handle_category_click(void) {
    palette.current_category = hover.category - palette.categories;
    assert(palette.current_category >= 0 && palette.current_category < (int)vector_size(palette.categories));
    return true;
}

bool handle_jump_to_block_button_click(void) {
    hover.editor.select_block = exec_compile_error_block;
    hover.editor.select_blockchain = exec_compile_error_blockchain;
    return true;
}

bool handle_error_window_close_button_click(void) {
    clear_compile_error();
    return true;
}

bool handle_tab_button(void) {
    current_tab = (int)(size_t)hover.button.data;
    shader_time = 0.0;
    return true;
}

bool handle_add_tab_button(void) {
    char* name = "";
    switch (hover.panels.mouse_panel) {
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

    tab_insert(name, panel_new(hover.panels.mouse_panel), (int)(size_t)hover.button.data);

    hover.panels.mouse_panel = PANEL_NONE;
    current_tab = (int)(size_t)hover.button.data;
    shader_time = 0.0;
    return true;
}

bool handle_panel_editor_save_button(void) {
    hover.is_panel_edit_mode = false;
    save_config(&conf);
    return true;
}

bool handle_panel_editor_cancel_button(void) {
    hover.is_panel_edit_mode = false;
    return true;
}

bool handle_editor_add_arg_button(void) {
    Blockdef* blockdef = hover.editor.argument->data.blockdef;
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

    deselect_all();
    return true;
}

bool handle_editor_add_text_button(void) {
    Blockdef* blockdef = hover.editor.argument->data.blockdef;
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
    Blockdef* blockdef = hover.editor.argument->data.blockdef;

    assert(hover.editor.blockdef_input != (size_t)-1);
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

    blockdef_delete_input(blockdef, hover.editor.blockdef_input);

    deselect_all();
    return true;
}

bool handle_editor_edit_button(void) {
    hover.editor.edit_blockdef = hover.editor.argument->data.blockdef;
    hover.editor.edit_block = hover.editor.block;
    deselect_all();
    return true;
}

bool handle_editor_close_button(void) {
    hover.editor.edit_blockdef = NULL;
    hover.editor.edit_block = NULL;
    deselect_all();
    return true;
}

static void remove_blockdef(void) {
    for (size_t i = 0; i < vector_size(mouse_blockchain.blocks); i++) {
        for (size_t j = 0; j < vector_size(mouse_blockchain.blocks[i].arguments); j++) {
            Argument* arg = &mouse_blockchain.blocks[i].arguments[j];
            if (arg->type != ARGUMENT_BLOCKDEF) continue;
            arg->data.blockdef->func = NULL;
            for (size_t k = 0; k < vector_size(arg->data.blockdef->inputs); k++) {
                Input* input = &arg->data.blockdef->inputs[k];
                if (input->type != INPUT_ARGUMENT) continue;
                input->data.arg.blockdef->func = NULL;
            }
        }
    }
}

static bool handle_block_palette_click(bool mouse_empty) {
    if (hover.editor.select_argument) {
        deselect_all();
        return true;
    }
    if (mouse_empty && hover.editor.block) {
        // Pickup block
        TraceLog(LOG_INFO, "Pickup block");
        int ind = hover.editor.blockchain - palette.categories[palette.current_category].chains;
        if (ind < 0 || ind > (int)vector_size(palette.categories[palette.current_category].chains)) return true;

        blockchain_free(&mouse_blockchain);
        mouse_blockchain = blockchain_copy(&palette.categories[palette.current_category].chains[ind], 0);
        return true;
    } else if (!mouse_empty) {
        // Drop block
        TraceLog(LOG_INFO, "Drop block");
        remove_blockdef();
        blockchain_clear_blocks(&mouse_blockchain);
        return true;
    }
    return true;
}

static bool handle_blockdef_editor_click(void) {
    if (!hover.editor.blockdef) return true;
    if (hover.editor.edit_blockdef == hover.editor.argument->data.blockdef) return false;
    blockchain_add_block(&mouse_blockchain, block_new_ms(hover.editor.blockdef));
    deselect_all();
    return true;
}

static bool handle_code_editor_click(bool mouse_empty) {
    if (!mouse_empty) {
        mouse_blockchain.x = gui->mouse_x;
        mouse_blockchain.y = gui->mouse_y;
        if (hover.editor.argument || hover.editor.parent_argument) {
            if (vector_size(mouse_blockchain.blocks) > 1) return true;
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_CONTROLEND) return true;
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return true;

            if (hover.editor.argument) {
                // Attach to argument
                TraceLog(LOG_INFO, "Attach to argument");
                if (hover.editor.argument->type != ARGUMENT_TEXT) return true;
                mouse_blockchain.blocks[0].parent = hover.editor.block;
                argument_set_block(hover.editor.argument, mouse_blockchain.blocks[0]);
                vector_clear(mouse_blockchain.blocks);
                hover.editor.select_blockchain = hover.editor.blockchain;
                hover.editor.select_block = &hover.editor.argument->data.block;
                hover.select_input = NULL;
            } else if (hover.editor.parent_argument) {
                // Swap argument
                TraceLog(LOG_INFO, "Swap argument");
                if (hover.editor.parent_argument->type != ARGUMENT_BLOCK) return true;
                mouse_blockchain.blocks[0].parent = hover.editor.block->parent;
                Block temp = mouse_blockchain.blocks[0];
                mouse_blockchain.blocks[0] = *hover.editor.block;
                mouse_blockchain.blocks[0].parent = NULL;
                block_update_parent_links(&mouse_blockchain.blocks[0]);
                argument_set_block(hover.editor.parent_argument, temp);
                hover.editor.select_block = &hover.editor.parent_argument->data.block;
                hover.editor.select_blockchain = hover.editor.blockchain;
            }
        } else if (
            hover.editor.block &&
            hover.editor.blockchain &&
            hover.editor.block->parent == NULL
        ) {
            // Attach block
            TraceLog(LOG_INFO, "Attach block");
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return true;

            int ind = hover.editor.block - hover.editor.blockchain->blocks;
            blockchain_insert(hover.editor.blockchain, &mouse_blockchain, ind);
            // Update block link to make valgrind happy
            hover.editor.block = &hover.editor.blockchain->blocks[ind];
            hover.editor.select_block = hover.editor.block + 1;
            hover.editor.select_blockchain = hover.editor.blockchain;
        } else {
            // Put block
            TraceLog(LOG_INFO, "Put block");
            mouse_blockchain.x += camera_pos.x - hover.panels.panel_size.x;
            mouse_blockchain.y += camera_pos.y - hover.panels.panel_size.y;
            vector_add(&editor_code, mouse_blockchain);
            mouse_blockchain = blockchain_new();
            hover.editor.select_blockchain = &editor_code[vector_size(editor_code) - 1];
            hover.editor.select_block = &hover.editor.select_blockchain->blocks[0];
        }
        return true;
    } else if (hover.editor.block) {
        if (hover.editor.block->parent) {
            if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                // Copy argument
                TraceLog(LOG_INFO, "Copy argument");
                blockchain_add_block(&mouse_blockchain, block_copy(hover.editor.block, NULL));
            } else {
                // Detach argument
                TraceLog(LOG_INFO, "Detach argument");
                assert(hover.editor.parent_argument != NULL);

                blockchain_add_block(&mouse_blockchain, *hover.editor.block);
                mouse_blockchain.blocks[0].parent = NULL;

                argument_set_text(hover.editor.parent_argument, "");
                hover.editor.select_blockchain = NULL;
                hover.editor.select_block = NULL;
            }
        } else if (hover.editor.blockchain) {
            int ind = hover.editor.block - hover.editor.blockchain->blocks;

            if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Copy block
                    TraceLog(LOG_INFO, "Copy block");
                    blockchain_free(&mouse_blockchain);

                    mouse_blockchain = blockchain_copy_single(hover.editor.blockchain, ind);
                } else {
                    // Copy chain
                    TraceLog(LOG_INFO, "Copy chain");
                    blockchain_free(&mouse_blockchain);
                    mouse_blockchain = blockchain_copy(hover.editor.blockchain, ind);
                }
            } else {
                hover.editor.edit_blockdef = NULL;
                hover.editor.edit_block = NULL;
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Detach block
                    TraceLog(LOG_INFO, "Detach block");
                    blockchain_detach_single(&mouse_blockchain, hover.editor.blockchain, ind);
                    if (vector_size(hover.editor.blockchain->blocks) == 0) {
                        blockchain_free(hover.editor.blockchain);
                        vector_remove(editor_code, hover.editor.blockchain - editor_code);
                        hover.editor.block = NULL;
                    }
                } else {
                    // Detach chain
                    TraceLog(LOG_INFO, "Detach chain");
                    int ind = hover.editor.block - hover.editor.blockchain->blocks;
                    blockchain_detach(&mouse_blockchain, hover.editor.blockchain, ind);
                    if (ind == 0) {
                        blockchain_free(hover.editor.blockchain);
                        vector_remove(editor_code, hover.editor.blockchain - editor_code);
                        hover.editor.block = NULL;
                    }
                }
                hover.editor.select_blockchain = NULL;
                hover.editor.select_block = NULL;
            }
        }
        return true;
    }
    return false;
}

static bool handle_editor_panel_click(void) {
    if (!hover.panels.panel) return true;

    if (hover.panels.panel->type == PANEL_SPLIT) {
        hover.panels.drag_panel = hover.panels.panel;
        hover.panels.drag_panel_size = hover.panels.panel_size;
        return false;
    }

    if (hover.panels.mouse_panel == PANEL_NONE) {
        PanelTree* parent = hover.panels.panel->parent;
        if (!parent) {
            if (vector_size(code_tabs) > 1) {
                hover.panels.mouse_panel = hover.panels.panel->type;
                tab_delete(current_tab);
            }
            return true;
        }

        hover.panels.mouse_panel = hover.panels.panel->type;
        free(hover.panels.panel);
        PanelTree* other_panel = parent->left == hover.panels.panel ? parent->right : parent->left;

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
        panel_split(hover.panels.panel, hover.panels.panel_side, hover.panels.mouse_panel, 0.5);
        hover.panels.mouse_panel = PANEL_NONE;
    }

    return true;
}

static void get_input_ind(void) {
    assert(hover.input_info.font != NULL);
    assert(hover.input_info.input != NULL);

    float width = 0.0;
    float prev_width = 0.0;

    int codepoint = 0; // Current character
    int index = 0; // Index position in sprite font
    int text_size = strlen(*hover.input_info.input);
    float scale_factor = hover.input_info.font_size / (float)hover.input_info.font->baseSize;

    int prev_i = 0;
    int i = 0;

    while (i < text_size && (width * scale_factor) < hover.input_info.rel_pos.x) {
        int next = 0;
        codepoint = GetCodepointNext(&(*hover.input_info.input)[i], &next);
        index = search_glyph(codepoint);

        prev_width = width;
        prev_i = i;

        if (hover.input_info.font->glyphs[index].advanceX != 0) {
            width += hover.input_info.font->glyphs[index].advanceX;
        } else {
            width += hover.input_info.font->recs[index].width + hover.input_info.font->glyphs[index].offsetX;
        }
        i += next;
    }
    prev_width *= scale_factor;
    width *= scale_factor;

    if (width - hover.input_info.rel_pos.x < hover.input_info.rel_pos.x - prev_width) { // Right side of char is closer
        hover.select_input_cursor = i;
    } else {
        hover.select_input_cursor = prev_i;
    }
    hover.select_input_mark = -1;
}

// Return value indicates if we should cancel dragging
static bool handle_mouse_click(void) {
    hover.mouse_click_pos = (Vector2) { gui->mouse_x, gui->mouse_y };
    camera_click_pos = camera_pos;
    hover.dragged_slider.value = NULL;

    if (hover.select_input == &search_list_search) {
        if (hover.editor.block) {
            blockchain_add_block(&mouse_blockchain, block_new_ms(hover.editor.block->blockdef));
            if (hover.editor.block->blockdef->type == BLOCKTYPE_CONTROL && vm.end_blockdef != (size_t)-1) {
                blockchain_add_block(&mouse_blockchain, block_new_ms(vm.blockdefs[vm.end_blockdef]));
            }
        }

        hover.select_input = NULL;
        hover.editor.block = NULL;
        return true;
    }

    if (hover.button.handler) return hover.button.handler();
    if (hover.hover_slider.value) {
        hover.dragged_slider = hover.hover_slider;
        hover.slider_last_val = *hover.dragged_slider.value;
        return false;
    }
    if (gui_window_is_shown()) {
        if (hover.input_info.input) get_input_ind();
        if (hover.input_info.input != hover.select_input) hover.select_input = hover.input_info.input;
        return true;
    }
    if (!hover.panels.panel) return true;
    if (hover.is_panel_edit_mode) return handle_editor_panel_click();
    if (hover.panels.panel->type == PANEL_TERM) return true;
    if (vm.is_running) return hover.panels.panel->type != PANEL_CODE;

    if (hover.input_info.input) get_input_ind();
    if (hover.input_info.input != hover.select_input) hover.select_input = hover.input_info.input;

    bool mouse_empty = vector_size(mouse_blockchain.blocks) == 0;

    if (hover.panels.panel->type == PANEL_BLOCK_PALETTE) return handle_block_palette_click(mouse_empty);

    if (mouse_empty && hover.editor.argument && hover.editor.argument->type == ARGUMENT_BLOCKDEF) {
        if (handle_blockdef_editor_click()) return true;
    }

    if (mouse_empty) {
        if (hover.editor.block && hover.editor.argument) {
            Input block_input = hover.editor.block->blockdef->inputs[hover.editor.argument->input_id];
            if (block_input.type == INPUT_DROPDOWN) {
                size_t list_len = 0;
                char** list = block_input.data.drop.list(hover.editor.block, &list_len);

                show_dropdown(LOCATION_BLOCK_DROPDOWN, list, list_len, handle_block_dropdown_click);
            }
        }
        if (hover.editor.blockchain != hover.editor.select_blockchain) {
            hover.editor.select_blockchain = hover.editor.blockchain;
            if (hover.editor.select_blockchain) blockchain_select_counter = hover.editor.select_blockchain - editor_code;
        }
        if (hover.editor.block != hover.editor.select_block) hover.editor.select_block = hover.editor.block;
        if (hover.editor.argument != hover.editor.select_argument) {
            if (!hover.editor.argument || hover.input_info.input || hover.dropdown.location != LOCATION_NONE) hover.editor.select_argument = hover.editor.argument;
            dropdown.scroll_amount = 0;
            return true;
        }
        if (hover.editor.select_argument) return true;
    }

    if (hover.panels.panel->type == PANEL_CODE && handle_code_editor_click(mouse_empty)) return true;
    return hover.panels.panel->type != PANEL_CODE;
}

static void block_next_argument() {
    Argument* args = hover.editor.select_block->arguments;
    Argument* arg = hover.editor.select_argument ? hover.editor.select_argument + 1 : &args[0];
    if (arg - args >= (int)vector_size(args)) {
        if (hover.editor.select_block->parent) {
            Argument* parent_args = hover.editor.select_block->parent->arguments;
            for (size_t i = 0; i < vector_size(parent_args); i++) {
                if (parent_args[i].type == ARGUMENT_BLOCK && &parent_args[i].data.block == hover.editor.select_block) hover.editor.select_argument = &parent_args[i];
            }
            hover.editor.select_block = hover.editor.select_block->parent;
            block_next_argument();
        } else {
            hover.editor.select_argument = NULL;
        }
        return;
    }

    if (arg->type == ARGUMENT_TEXT || arg->type == ARGUMENT_CONST_STRING) {
        hover.editor.select_argument = arg;
    } else if (arg->type == ARGUMENT_BLOCK) {
        hover.editor.select_argument = NULL;
        hover.editor.select_block = &arg->data.block;
    }
}

static void block_prev_argument() {
    Argument* args = hover.editor.select_block->arguments;
    Argument* arg = hover.editor.select_argument ? hover.editor.select_argument - 1 : &args[-1];
    if (arg - args < 0) {
        if (hover.editor.select_argument) {
            hover.editor.select_argument = NULL;
            return;
        }
        if (hover.editor.select_block->parent) {
            Argument* parent_args = hover.editor.select_block->parent->arguments;
            for (size_t i = 0; i < vector_size(parent_args); i++) {
                if (parent_args[i].type == ARGUMENT_BLOCK && &parent_args[i].data.block == hover.editor.select_block) hover.editor.select_argument = &parent_args[i];
            }
            hover.editor.select_block = hover.editor.select_block->parent;
            block_prev_argument();
        } else {
            hover.editor.select_argument = NULL;
        }
        return;
    }

    if (arg->type == ARGUMENT_TEXT || arg->type == ARGUMENT_CONST_STRING) {
        hover.editor.select_argument = arg;
    } else if (arg->type == ARGUMENT_BLOCK) {
        hover.editor.select_argument = NULL;
        hover.editor.select_block = &arg->data.block;
        while (vector_size(hover.editor.select_block->arguments) != 0) {
            arg = &hover.editor.select_block->arguments[vector_size(hover.editor.select_block->arguments) - 1];
            if (arg->type == ARGUMENT_TEXT || arg->type == ARGUMENT_CONST_STRING) {
                hover.editor.select_argument = arg;
                break;
            } else if (arg->type == ARGUMENT_BLOCK) {
                hover.editor.select_block = &arg->data.block;
            }
        }
    }
}

static bool handle_code_panel_key_press(void) {
    if (hover.editor.select_argument && !hover.select_input) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            hover.select_input = &hover.editor.select_argument->data.text;
            hover.select_input_mark = 0;
            hover.select_input_cursor = strlen(*hover.select_input);
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

        hover.select_input = NULL;
        hover.editor.select_argument = NULL;
        hover.editor.select_block = &editor_code[blockchain_select_counter].blocks[0];
        hover.editor.select_blockchain = &editor_code[blockchain_select_counter];
        camera_pos.x = editor_code[blockchain_select_counter].x - 50;
        camera_pos.y = editor_code[blockchain_select_counter].y - 50;
        actionbar_show(TextFormat(gettext("Jump to chain (%d/%d)"), blockchain_select_counter + 1, vector_size(editor_code)));
        render_surface_needs_redraw = true;
        return true;
    }

    if (!hover.editor.select_blockchain || !hover.editor.select_block || hover.select_input) return false;

    int bounds_x = MIN(200, hover.panels.code_panel_bounds.width / 2);
    int bounds_y = MIN(200, hover.panels.code_panel_bounds.height / 2);

    if (hover.editor.select_block_pos.x - (hover.panels.code_panel_bounds.x + hover.panels.code_panel_bounds.width) > -bounds_x) {
        camera_pos.x += hover.editor.select_block_pos.x - (hover.panels.code_panel_bounds.x + hover.panels.code_panel_bounds.width) + bounds_x;
        render_surface_needs_redraw = true;
    }

    if (hover.editor.select_block_pos.x - hover.panels.code_panel_bounds.x < bounds_x) {
        camera_pos.x += hover.editor.select_block_pos.x - hover.panels.code_panel_bounds.x - bounds_x;
        render_surface_needs_redraw = true;
    }

    if (hover.editor.select_block_pos.y - (hover.panels.code_panel_bounds.y + hover.panels.code_panel_bounds.height) > -bounds_y) {
        camera_pos.y += hover.editor.select_block_pos.y - (hover.panels.code_panel_bounds.y + hover.panels.code_panel_bounds.height) + bounds_y;
        render_surface_needs_redraw = true;
    }

    if (hover.editor.select_block_pos.y - hover.panels.code_panel_bounds.y < bounds_y) {
        camera_pos.y += hover.editor.select_block_pos.y - hover.panels.code_panel_bounds.y - bounds_y;
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
        while (hover.editor.select_block->parent) hover.editor.select_block = hover.editor.select_block->parent;
        hover.editor.select_block--;
        hover.editor.select_argument = NULL;
        if (hover.editor.select_block < hover.editor.select_blockchain->blocks) hover.editor.select_block = hover.editor.select_blockchain->blocks;
        render_surface_needs_redraw = true;
        return true;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) {
        while (hover.editor.select_block->parent) hover.editor.select_block = hover.editor.select_block->parent;
        hover.editor.select_block++;
        hover.editor.select_argument = NULL;
        if (hover.editor.select_block - hover.editor.select_blockchain->blocks >= (int)vector_size(hover.editor.select_blockchain->blocks)) {
            hover.editor.select_block--;
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
        for (size_t j = 0; j < vector_size(palette.categories[i].chains); j++) {
            if (search_blockdef(palette.categories[i].chains[j].blocks[0].blockdef)) vector_add(&search_list, &palette.categories[i].chains[j].blocks[0]);
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
        hover.select_input != &search_list_search &&
        vector_size(mouse_blockchain.blocks) == 0 &&
        !hover.is_panel_edit_mode &&
        hover.panels.panel &&
        hover.panels.panel->type == PANEL_CODE &&
        !vm.is_running &&
        !gui_window_is_shown() &&
        !hover.select_input)
    {
        vector_clear(search_list_search);
        vector_add(&search_list_search, 0);
        hover.select_input = &search_list_search;
        hover.select_input_cursor = 0;
        hover.select_input_mark = -1;
        render_surface_needs_redraw = true;
        update_search();
        return;
    }

    if (hover.panels.panel) {
        if (hover.panels.panel->type == PANEL_TERM) {
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
        } else if (hover.panels.panel->type == PANEL_CODE) {
            if (handle_code_panel_key_press()) return;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        hover.select_input = NULL;
        hover.editor.select_argument = NULL;
        render_surface_needs_redraw = true;
        return;
    }
    if (hover.editor.select_block && hover.editor.select_argument && hover.editor.select_block->blockdef->inputs[hover.editor.select_argument->input_id].type == INPUT_DROPDOWN) return;

    if (edit_text(hover.select_input)) {
        if (hover.select_input == &search_list_search) update_search();
    }
}

static void handle_mouse_wheel(void) {
    if (!hover.panels.panel) return;
    if (hover.panels.panel->type != PANEL_CODE) return;
    if (hover.editor.select_argument) return;
    if (hover.is_panel_edit_mode) return;
    if (hover.select_input) return;
    if (gui_window_is_shown()) return;

    Vector2 wheel = GetMouseWheelMoveV();
    camera_pos.x -= wheel.x * conf.font_size * 2;
    camera_pos.y -= wheel.y * conf.font_size * 2;

    if (wheel.x != 0 || wheel.y != 0) {
        hover.editor.select_block = NULL;
        hover.editor.select_argument = NULL;
        hover.select_input = NULL;
        hover.editor.select_blockchain = NULL;
    }
}

static void handle_mouse_drag(void) {
    if (hover.drag_cancelled) return;

    if (hover.is_panel_edit_mode && hover.panels.drag_panel && hover.panels.drag_panel->type == PANEL_SPLIT) {
        if (hover.panels.drag_panel->direction == DIRECTION_HORIZONTAL) {
            hover.panels.drag_panel->split_percent = CLAMP(
                (gui->mouse_x - hover.panels.drag_panel_size.x - 5) / hover.panels.drag_panel_size.width,
                0.0,
                1.0 - (10.0 / hover.panels.drag_panel_size.width)
            );
        } else {
            hover.panels.drag_panel->split_percent = CLAMP(
                (gui->mouse_y - hover.panels.drag_panel_size.y - 5) / hover.panels.drag_panel_size.height,
                0.0,
                1.0 - (10.0 / hover.panels.drag_panel_size.height)
            );
        }
        return;
    }

    if (hover.dragged_slider.value) {
        *hover.dragged_slider.value = CLAMP(
            hover.slider_last_val + (gui->mouse_x - hover.mouse_click_pos.x) / 2,
            hover.dragged_slider.min,
            hover.dragged_slider.max
        );
        return;
    }

    camera_pos.x = camera_click_pos.x - (gui->mouse_x - hover.mouse_click_pos.x);
    camera_pos.y = camera_click_pos.y - (gui->mouse_y - hover.mouse_click_pos.y);
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
        hover.drag_cancelled = handle_mouse_click();
        render_surface_needs_redraw = true;
#ifdef DEBUG
        // This will traverse through all blocks in codebase, which is expensive in large codebase.
        // Ideally all functions should not be broken in the first place. This helps with debugging invalid states
        sanitize_links();
#endif
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        hover.mouse_click_pos = (Vector2) { gui->mouse_x, gui->mouse_y };
        camera_click_pos = camera_pos;
        hover.editor.select_block = NULL;
        hover.editor.select_argument = NULL;
        hover.select_input = NULL;
        hover.editor.select_blockchain = NULL;
        render_surface_needs_redraw = true;
    } else if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        handle_mouse_drag();
    } else {
        hover.drag_cancelled = false;
        hover.dragged_slider.value = NULL;
        hover.panels.drag_panel = NULL;
        handle_key_press();
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) render_surface_needs_redraw = true;

    handle_window();

    if (render_surface_needs_redraw) {
        hover.editor.block = NULL;
        hover.editor.argument = NULL;
        hover.input_info.input = NULL;
        hover.category = NULL;
        hover.editor.parent_argument = NULL;
        hover.editor.prev_blockchain = NULL;
        hover.editor.blockchain = NULL;
        hover.editor.part = EDITOR_NONE;
        hover.editor.blockdef = NULL;
        hover.editor.blockdef_input = -1;
        hover.button.handler = NULL;
        hover.button.data = NULL;
        hover.hover_slider.value = NULL;
        hover.panels.panel = NULL;
        hover.panels.panel_size = (Rectangle) {0};
        hover.editor.select_valid = false;

#ifdef DEBUG
        Timer t = start_timer("gui process");
#endif
        scrap_gui_process();
#ifdef DEBUG
        ui_time = end_timer(t);
#endif

        if (start_vm_timeout >= 0) start_vm_timeout--;
        // This fixes selecting wrong argument of a block when two blocks overlap
        if (hover.editor.block && hover.editor.argument) {
            int ind = hover.editor.argument - hover.editor.block->arguments;
            if (ind < 0 || ind > (int)vector_size(hover.editor.block->arguments)) hover.editor.argument = NULL;
        }

        if (hover.editor.select_block && !hover.editor.select_valid) {
            TraceLog(LOG_WARNING, "Invalid selection: %p", hover.editor.select_block);
            hover.editor.select_block = NULL;
            hover.editor.select_blockchain = NULL;
        }
    }

    hover.editor.prev_block = hover.editor.block;
    hover.editor.prev_argument = hover.editor.argument;
    hover.editor.prev_blockdef = hover.editor.blockdef;
    hover.panels.prev_panel = hover.panels.panel;
}
