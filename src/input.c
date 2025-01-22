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
#include "blocks.h"
#include "../external/tinyfiledialogs.h"

#include <assert.h>
#include <math.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

typedef enum {
    FILE_MENU_SAVE_PROJECT = 0,
    FILE_MENU_LOAD_PROJECT,
} FileMenuInds;

char* file_menu_list[] = {
    "Save project",
    "Load project",
};

void block_delete_blockdef(ScrBlock* block, ScrBlockdef* blockdef) {
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

void blockchain_delete_blockdef(ScrBlockChain* chain, ScrBlockdef* blockdef) {
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

void editor_code_remove_blockdef(ScrBlockdef* blockdef) {
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

void edit_text(char** text) {
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (vector_size(*text) <= 1) return;

        int remove_pos = vector_size(*text) - 2;
        int remove_size = 1;

        while (((unsigned char)(*text)[remove_pos] >> 6) == 2) { // This checks if we are in the middle of UTF-8 char
            remove_pos--;
            remove_size++;
        }

        vector_erase(*text, remove_pos, remove_size);
        return;
    }

    int char_val;
    while ((char_val = GetCharPressed())) {
        int utf_size = 0;
        const char* utf_char = CodepointToUTF8(char_val, &utf_size);
        for (int i = 0; i < utf_size; i++) {
            vector_insert(text, vector_size(*text) - 1, utf_char[i]);
        }
    }
}

bool start_vm(void) {
    if (vm.is_running) return false;

    term_restart();
    exec = exec_new();
    exec_copy_code(&vm, &exec, editor_code);
    if (!exec_start(&vm, &exec)) {
        actionbar_show("Start failed!");
        return false;
    }

    actionbar_show("Started successfully!");
    if (current_tab != TAB_OUTPUT) {
        shader_time = 0.0;
        //current_tab = TAB_OUTPUT;
    }
    return true;
}

bool stop_vm(void) {
    if (!vm.is_running) return false;
    TraceLog(LOG_INFO, "STOP");
    exec_stop(&vm, &exec);
    return true;
}

void deselect_all(void) {
    hover_info.select_argument = NULL;
    hover_info.select_input = NULL;
    dropdown.scroll_amount = 0;
}

void show_dropdown(DropdownLocations location, char** list, int list_len, ButtonClickHandler handler) {
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

char* basename(char* path) {
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
    case FILE_MENU_SAVE_PROJECT:
        path = tinyfd_saveFileDialog(NULL, project_name, ARRLEN(filters), filters, "Scrap project files (.scrp)"); 
        if (!path) break;
        save_code(path, editor_code);

        base_path = basename(path);
        for (i = 0; base_path[i]; i++) project_name[i] = base_path[i]; 
        project_name[i] = 0;
        break;
    case FILE_MENU_LOAD_PROJECT:
        path = tinyfd_openFileDialog(NULL, project_name, ARRLEN(filters), filters, "Scrap project files (.scrp)", 0);
        if (!path) break;

        ScrBlockChain* chain = load_code(path);
        if (!chain) {
            actionbar_show("File load failed :(");
            break;
        }

        for (size_t i = 0; i < vector_size(editor_code); i++) blockchain_free(&editor_code[i]);
        vector_free(editor_code);
        editor_code = chain;

        blockchain_select_counter = 0;
        camera_pos.x = editor_code[blockchain_select_counter].x - ((GetScreenWidth() - conf.side_bar_size) / 2 + conf.side_bar_size);
        camera_pos.y = editor_code[blockchain_select_counter].y - ((GetScreenHeight() - conf.font_size * 2.2) / 2 + conf.font_size * 2.2);

        base_path = basename(path);
        for (i = 0; base_path[i]; i++) project_name[i] = base_path[i]; 
        project_name[i] = 0;

        actionbar_show("File load succeeded!");
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
    start_vm();
    return true;
}

bool handle_stop_button_click(void) {
    stop_vm();
    return true;
}

bool handle_code_tab_click(void) {
    if (current_tab != TAB_CODE) shader_time = 0.0;
    current_tab = TAB_CODE;
    return true;
}

bool handle_output_tab_click(void) {
    if (current_tab != TAB_OUTPUT) shader_time = 0.0;
    current_tab = TAB_OUTPUT;
    return true;
}

bool handle_window_gui_close_button_click(void) {
    gui_window_hide();
    return true;
}

bool handle_settings_panel_editor_button_click(void) {
    gui_window_hide();
    hover_info.is_panel_edit_mode = true;
    return true;
}

bool handle_settings_reset_button_click(void) {
    set_default_config(&window_conf);
    return true;
}

bool handle_settings_apply_button_click(void) {
    apply_config(&conf, &window_conf);
    save_config(&window_conf);
    return true;
}

bool handle_about_license_button_click(void) {
    OpenURL(LICENSE_URL);
    return true;
}

bool handle_panel_editor_done_button(void) {
    hover_info.is_panel_edit_mode = false;
    return true;
}

bool handle_editor_add_arg_button(void) {
    ScrBlockdef* blockdef = hover_info.argument->data.blockdef;
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

    blockdef_add_argument(blockdef, "", BLOCKCONSTR_UNLIMITED);

    sprintf(str, "arg%zu", last_input);
    ScrBlockdef* arg_blockdef = blockdef->inputs[last_input].data.arg.blockdef;
    blockdef_add_text(arg_blockdef, str);
    arg_blockdef->func = block_custom_arg;

    int arg_count = 0;
    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (blockdef->inputs[i].type == INPUT_ARGUMENT) arg_count++;
    }
    arg_blockdef->arg_id = arg_count - 1;

    deselect_all();
    return true;
}

bool handle_editor_add_text_button(void) {
    ScrBlockdef* blockdef = hover_info.argument->data.blockdef;
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
    ScrBlockdef* blockdef = hover_info.argument->data.blockdef;

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

    bool is_arg = blockdef->inputs[hover_info.editor.blockdef_input].type == INPUT_ARGUMENT;
    blockdef_delete_input(blockdef, hover_info.editor.blockdef_input);
    if (is_arg) {
        for (size_t i = hover_info.editor.blockdef_input; i < vector_size(blockdef->inputs); i++) {
            if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
            blockdef->inputs[i].data.arg.blockdef->arg_id--;
        }
    }

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

bool handle_sidebar_click(bool mouse_empty) {
    if (hover_info.select_argument) {
        deselect_all();
        return true;
    }
    if (mouse_empty && hover_info.block) {
        // Pickup block
        TraceLog(LOG_INFO, "Pickup block");
        int ind = hover_info.block - sidebar.blocks;
        if (ind < 0 || ind > (int)vector_size(sidebar.blocks)) return true;

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
                ScrArgument* arg = &mouse_blockchain.blocks[i].arguments[j];
                if (arg->type != ARGUMENT_BLOCKDEF) continue;
                if (arg->data.blockdef->ref_count > 1) editor_code_remove_blockdef(arg->data.blockdef);
                for (size_t k = 0; k < vector_size(arg->data.blockdef->inputs); k++) {
                    ScrInput* input = &arg->data.blockdef->inputs[k];
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

bool handle_blockdef_editor_click(void) {
    if (!hover_info.editor.blockdef) return true;
    if (hover_info.editor.edit_blockdef == hover_info.argument->data.blockdef) return false;
    blockchain_add_block(&mouse_blockchain, block_new_ms(hover_info.editor.blockdef));
    deselect_all();
    return true;
}

bool handle_code_editor_click(bool mouse_empty) {
    if (!mouse_empty) {
        mouse_blockchain.x = GetMouseX();
        mouse_blockchain.y = GetMouseY();
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
            } else if (hover_info.prev_argument) {
                // Swap argument
                TraceLog(LOG_INFO, "Swap argument");
                if (hover_info.prev_argument->type != ARGUMENT_BLOCK) return true;
                mouse_blockchain.blocks[0].parent = hover_info.block->parent;
                ScrBlock temp = mouse_blockchain.blocks[0];
                mouse_blockchain.blocks[0] = *hover_info.block;
                mouse_blockchain.blocks[0].parent = NULL;
                block_update_parent_links(&mouse_blockchain.blocks[0]);
                argument_set_block(hover_info.prev_argument, temp);
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
        } else {
            // Put block
            TraceLog(LOG_INFO, "Put block");
            mouse_blockchain.x += camera_pos.x;
            mouse_blockchain.y += camera_pos.y;
            vector_add(&editor_code, mouse_blockchain);
            mouse_blockchain = blockchain_new();
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
            }
        }
        return true;
    }
    return false;
}

bool handle_editor_panel_click(void) {
    if (!hover_info.panel) return true;

    if (hover_info.panel->type == PANEL_SPLIT) {
        hover_info.drag_panel = hover_info.panel;
        hover_info.drag_panel_size = hover_info.panel_size;
        return false;
    }

    if (hover_info.mouse_panel == PANEL_NONE) {
        PanelTree* parent = hover_info.panel->parent;
        if (!parent) return true;

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

// Return value indicates if we should cancel dragging
bool handle_mouse_click(void) {
    hover_info.mouse_click_pos = GetMousePosition();
    camera_click_pos = camera_pos;
    hover_info.dragged_slider.value = NULL;

    if (hover_info.hover_slider.value) {
        hover_info.dragged_slider = hover_info.hover_slider;
        hover_info.slider_last_val = *hover_info.dragged_slider.value;
        return false;
    }
    if (hover_info.top_bars.handler) return hover_info.top_bars.handler();
    if (gui_window_is_shown()) {
        if (hover_info.input != hover_info.select_input) hover_info.select_input = hover_info.input;
        return true;
    }
    if (hover_info.is_panel_edit_mode) return handle_editor_panel_click();
    if (current_tab != TAB_CODE) return true;
    if (vm.is_running) return false;

    bool mouse_empty = vector_size(mouse_blockchain.blocks) == 0;

    if (hover_info.sidebar) return handle_sidebar_click(mouse_empty);

    if (mouse_empty && hover_info.argument && hover_info.argument->type == ARGUMENT_BLOCKDEF) {
        if (handle_blockdef_editor_click()) return true;
    }

    if (mouse_empty) {
        if (hover_info.block && hover_info.argument) {
            ScrInput block_input = hover_info.block->blockdef->inputs[hover_info.argument->input_id];
            if (block_input.type == INPUT_DROPDOWN) {
                size_t list_len = 0;
                char** list = block_input.data.drop.list(hover_info.block, &list_len);
                
                show_dropdown(LOCATION_BLOCK_DROPDOWN, list, list_len, handle_block_dropdown_click);
            }
        }

        if (hover_info.block != hover_info.select_block) hover_info.select_block = hover_info.block;
        if (hover_info.input != hover_info.select_input) hover_info.select_input = hover_info.input;
        if (hover_info.argument != hover_info.select_argument) {
            hover_info.select_argument = hover_info.argument;
            dropdown.scroll_amount = 0;
            return true;
        }
        if (hover_info.select_argument) return true;
    }

    if (handle_code_editor_click(mouse_empty)) return true;
    return false;
}

void handle_key_press(void) {
    if (IsKeyPressed(KEY_F5)) {
        start_vm();
        return;
    }
    if (IsKeyPressed(KEY_F6)) {
        stop_vm();
        return;
    }

    if (current_tab == TAB_OUTPUT) {
        if (!vm.is_running) return;
        if (IsKeyPressed(KEY_ENTER)) {
            term_input_put_char('\n');
            term_print_str("\r\n");
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
        }
        return;
    }

    if (!hover_info.select_input) {
        if (IsKeyPressed(KEY_SPACE) && vector_size(editor_code) > 0) {
            blockchain_select_counter++;
            if ((vec_size_t)blockchain_select_counter >= vector_size(editor_code)) blockchain_select_counter = 0;

            camera_pos.x = editor_code[blockchain_select_counter].x - ((GetScreenWidth() - conf.side_bar_size) / 2 + conf.side_bar_size);
            camera_pos.y = editor_code[blockchain_select_counter].y - ((GetScreenHeight() - conf.font_size * 2.2) / 2 + conf.font_size * 2.2);
            actionbar_show(TextFormat("Jump to chain (%d/%d)", blockchain_select_counter + 1, vector_size(editor_code)));
            return;
        }
        return;
    };
    if (hover_info.select_block && hover_info.select_argument && hover_info.select_block->blockdef->inputs[hover_info.select_argument->input_id].type == INPUT_DROPDOWN) return;

    edit_text(hover_info.select_input);
}

void handle_mouse_wheel(void) {
    if (current_tab != TAB_CODE) return;
    if (hover_info.sidebar) return;
    if (hover_info.select_argument) return;
    if (hover_info.is_panel_edit_mode) return;

    Vector2 wheel = GetMouseWheelMoveV();
    camera_pos.x -= wheel.x * conf.font_size * 2;
    camera_pos.y -= wheel.y * conf.font_size * 2;
}

void handle_mouse_drag(void) {
    if (hover_info.drag_cancelled) return;

    Vector2 mouse_pos = GetMousePosition();

    if (hover_info.is_panel_edit_mode && hover_info.drag_panel && hover_info.drag_panel->type == PANEL_SPLIT) {
        if (hover_info.drag_panel->direction == DIRECTION_HORIZONTAL) {
            hover_info.drag_panel->split_percent = (mouse_pos.x - hover_info.drag_panel_size.x) / hover_info.drag_panel_size.width;
        } else {
            hover_info.drag_panel->split_percent = (mouse_pos.y - hover_info.drag_panel_size.y) / hover_info.drag_panel_size.height;
        }
        return;
    }

    if (hover_info.dragged_slider.value) {
        *hover_info.dragged_slider.value = CLAMP(
            hover_info.slider_last_val + (mouse_pos.x - hover_info.mouse_click_pos.x) / 2, 
            hover_info.dragged_slider.min, 
            hover_info.dragged_slider.max
        );
        return;
    }

    camera_pos.x = camera_click_pos.x - (mouse_pos.x - hover_info.mouse_click_pos.x);
    camera_pos.y = camera_click_pos.y - (mouse_pos.y - hover_info.mouse_click_pos.y);
}

void scrap_gui_process_input(void) {
    hover_info.sidebar = 0;
    hover_info.block = NULL;
    hover_info.argument = NULL;
    hover_info.input = NULL;
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

    gui_update_mouse_scroll(gui, GetMouseWheelMove());

#ifdef DEBUG
    Timer t = start_timer("gui process");
#endif
    scrap_gui_process();
#ifdef DEBUG
    ui_time = end_timer(t);
#endif

    if (GetMouseWheelMove() != 0.0) {
        handle_mouse_wheel();
    }

    if (hover_info.block && hover_info.argument) {
        int ind = hover_info.argument - hover_info.block->arguments;
        if (ind < 0 || ind > (int)vector_size(hover_info.block->arguments)) hover_info.argument = NULL;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        hover_info.drag_cancelled = handle_mouse_click();
#ifdef DEBUG
        // This will traverse through all blocks in codebase, which is expensive in large codebase.
        // Ideally all functions should not be broken in the first place. This helps with debugging invalid states
        sanitize_links();
#endif
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        hover_info.mouse_click_pos = GetMousePosition();
        camera_click_pos = camera_pos;
    } else if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        handle_mouse_drag();
    } else {
        hover_info.drag_cancelled = false;
        hover_info.dragged_slider.value = NULL;
        hover_info.drag_panel = NULL;
        handle_key_press();
    }

    if (IsWindowResized()) {
        shader_time = 0.0;
        gui_update_window_size(gui, GetScreenWidth(), GetScreenHeight());
    }

    gui_update_mouse_pos(gui, GetMouseX(), GetMouseY());
    mouse_blockchain.x = GetMouseX();
    mouse_blockchain.y = GetMouseY();

    hover_info.prev_block = hover_info.block;
    hover_info.prev_panel = hover_info.panel;
    hover_info.editor.prev_blockdef = hover_info.editor.blockdef;
}
