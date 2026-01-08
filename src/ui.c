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

// Divides the panel into two parts along the specified side with the specified split percentage
void panel_split(PanelTree* panel, SplitSide side, PanelType new_panel_type, float split_percent) {
    if (panel->type == PANEL_SPLIT) return;

    PanelTree* old_panel = malloc(sizeof(PanelTree));
    old_panel->type = panel->type;
    old_panel->left = NULL;
    old_panel->right = NULL;
    old_panel->parent = panel;

    PanelTree* new_panel = malloc(sizeof(PanelTree));
    new_panel->type = new_panel_type;
    new_panel->left = NULL;
    new_panel->right = NULL;
    new_panel->parent = panel;

    panel->type = PANEL_SPLIT;

    switch (side) {
    case SPLIT_SIDE_TOP:
        panel->direction = DIRECTION_VERTICAL;
        panel->left = new_panel;
        panel->right = old_panel;
        panel->split_percent = split_percent;
        break;
    case SPLIT_SIDE_BOTTOM:
        panel->direction = DIRECTION_VERTICAL;
        panel->left = old_panel;
        panel->right = new_panel;
        panel->split_percent = 1.0 - split_percent;
        break;
    case SPLIT_SIDE_LEFT:
        panel->direction = DIRECTION_HORIZONTAL;
        panel->left = new_panel;
        panel->right = old_panel;
        panel->split_percent = split_percent;
        break;
    case SPLIT_SIDE_RIGHT:
        panel->direction = DIRECTION_HORIZONTAL;
        panel->left = old_panel;
        panel->right = new_panel;
        panel->split_percent = 1.0 - split_percent;
        break;
    case SPLIT_SIDE_NONE:
        assert(false && "Got SPLIT_SIDE_NONE");
        break;
    default:
        assert(false && "Got unknown split side");
        break;
    }
}

PanelTree* panel_new(PanelType type) {
    PanelTree* panel = malloc(sizeof(PanelTree));
    panel->type = type;
    panel->left = NULL;
    panel->right = NULL;
    panel->parent = NULL;
    return panel;
}

// Removes a panel and its child panels recursively, freeing memory
void panel_delete(PanelTree* panel) {
    assert(panel != NULL);

    if (panel->type == PANEL_SPLIT) {
        panel_delete(panel->left);
        panel_delete(panel->right);
        panel->left = NULL;
        panel->right = NULL;
    }

    panel->type = PANEL_NONE;
    free(panel);
}

// Removes a tab by index and frees its resources
void tab_delete(size_t tab) {
    assert(tab < vector_size(editor.tabs));
    panel_delete(editor.tabs[tab].root_panel);
    vector_free(editor.tabs[tab].name);
    vector_remove(editor.tabs, tab);
    if (editor.current_tab >= (int)vector_size(editor.tabs)) editor.current_tab = vector_size(editor.tabs) - 1;
}

void delete_all_tabs(void) {
    for (ssize_t i = vector_size(editor.tabs) - 1; i >= 0; i--) tab_delete(i);
}

// Creates a new tab with the given name and panel, adding it to the list of tabs
size_t tab_new(char* name, PanelTree* root_panel) {
    if (!root_panel) {
        TraceLog(LOG_WARNING, "Got root_panel == NULL, not adding");
        return -1;
    }

    Tab* tab = vector_add_dst(&editor.tabs);
    tab->name = vector_create();
    for (char* str = name; *str; str++) vector_add(&tab->name, *str);
    vector_add(&tab->name, 0);
    tab->root_panel = root_panel;

    return vector_size(editor.tabs) - 1;
}

// Inserts a new tab with the given name and panel at the specified position in the list of tabs
void tab_insert(char* name, PanelTree* root_panel, size_t position) {
    if (!root_panel) {
        TraceLog(LOG_WARNING, "Got root_panel == NULL, not adding");
        return;
    }

    Tab* tab = vector_insert_dst(&editor.tabs, position);
    tab->name = vector_create();
    for (char* str = name; *str; str++) vector_add(&tab->name, *str);
    vector_add(&tab->name, 0);
    tab->root_panel = root_panel;
}

// Initializes codespace, using a default panel layout
void init_panels(void) {
    PanelTree* code_panel = panel_new(PANEL_CODE);
    panel_split(code_panel, SPLIT_SIDE_LEFT, PANEL_BLOCK_PALETTE, 0.3);
    panel_split(code_panel->left, SPLIT_SIDE_TOP, PANEL_BLOCK_CATEGORIES, 0.35);
    tab_new("Code", code_panel);
    tab_new("Output", panel_new(PANEL_TERM));
}

int search_glyph(int codepoint) {
    // We assume that ASCII region is the first region, so this index should correspond to char '?' in the glyph table
    const int fallback = 31;
    for (int i = 0; i < CODEPOINT_REGION_COUNT; i++) {
        if (codepoint < codepoint_regions[i][0] || codepoint > codepoint_regions[i][1]) continue;
        return codepoint - codepoint_regions[i][0] + codepoint_start_ranges[i];
    }
    return fallback;
}

static GuiMeasurement measure_slice(Font font, const char *text, unsigned int text_size, float font_size) {
    GuiMeasurement ms = {0};

    if ((font.texture.id == 0) || !text) return ms;

    int codepoint = 0; // Current character
    int index = 0; // Index position in sprite font

    for (unsigned int i = 0; i < text_size;) {
        int next = 0;
        codepoint = GetCodepointNext(&text[i], &next);
        index = search_glyph(codepoint);
        i += next;

        if (font.glyphs[index].advanceX != 0) {
            ms.w += font.glyphs[index].advanceX;
        } else {
            ms.w += font.recs[index].width + font.glyphs[index].offsetX;
        }
    }

    ms.w *= font_size / (float)font.baseSize;
    ms.h = font_size;
    return ms;
}

GuiMeasurement scrap_gui_measure_image(void* image, unsigned short size) {
    Texture2D* img = image;
    return (GuiMeasurement) { img->width * ((float)size / (float)img->height), size };
}

GuiMeasurement scrap_gui_measure_text(void* font, const char* text, unsigned int text_size, unsigned short font_size) {
    return measure_slice(*(Font*)font, text, text_size, font_size);
}

TermVec term_measure_text(void* font, const char* text, unsigned int text_size, unsigned short font_size) {
    GuiMeasurement m = measure_slice(*(Font*)font, text, text_size, font_size);
    return (TermVec) { .x = m.w, .y = m.h };
}

static void sanitize_block(Block* block) {
    for (vec_size_t i = 0; i < vector_size(block->arguments); i++) {
        if (block->arguments[i].type != ARGUMENT_BLOCK) continue;
        if (block->arguments[i].data.block.parent != block) {
            TraceLog(LOG_ERROR, "Block %p detached from parent %p! (Got %p)", &block->arguments[i].data.block, block, block->arguments[i].data.block.parent);
            assert(false);
            return;
        }
        sanitize_block(&block->arguments[i].data.block);
    }
}

static void sanitize_links(void) {
    for (vec_size_t i = 0; i < vector_size(editor.code); i++) {
        Block* blocks = editor.code[i].blocks;
        for (vec_size_t j = 0; j < vector_size(blocks); j++) {
            sanitize_block(&blocks[j]);
        }
    }

    for (vec_size_t i = 0; i < vector_size(editor.mouse_blockchain.blocks); i++) {
        sanitize_block(&editor.mouse_blockchain.blocks[i]);
    }
}

static void switch_tab_to_panel(PanelType panel) {
    for (size_t i = 0; i < vector_size(editor.tabs); i++) {
        if (find_panel(editor.tabs[i].root_panel, panel)) {
            if (editor.current_tab != (int)i) ui.shader_time = 0.0;
            editor.current_tab = i;
            ui.render_surface_needs_redraw = true;
            return;
        }
    }
}

static void set_mark(void) {
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        if (ui.hover.select_input_mark == -1) ui.hover.select_input_mark = ui.hover.select_input_cursor;
    } else {
        ui.hover.select_input_mark = -1;
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
    if (ui.hover.select_input_mark == -1) return;

    int remove_pos  = MIN(ui.hover.select_input_cursor, ui.hover.select_input_mark),
        remove_size = ABS(ui.hover.select_input_cursor - ui.hover.select_input_mark);
    ui.hover.select_input_mark = -1;
    ui.hover.select_input_cursor = remove_pos;
    vector_erase(*text, remove_pos, remove_size);
    ui.render_surface_needs_redraw = true;
}

static bool edit_text(char** text) {
    if (!text) return false;

    if (IsKeyPressed(KEY_HOME)) {
        set_mark();
        ui.hover.select_input_cursor = 0;
        ui.render_surface_needs_redraw = true;
        return false;
    }

    if (IsKeyPressed(KEY_END)) {
        set_mark();
        ui.hover.select_input_cursor = vector_size(*text) - 1;
        ui.render_surface_needs_redraw = true;
        return false;
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A)) {
        ui.hover.select_input_cursor = 0;
        ui.hover.select_input_mark = strlen(*text);
        ui.render_surface_needs_redraw = true;
        return false;
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_U)) {
        vector_clear(*text);
        vector_add(text, 0);
        ui.hover.select_input_cursor = 0;
        ui.hover.select_input_mark = -1;
        ui.render_surface_needs_redraw = true;
        return true;
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_C)) {
        if (ui.hover.select_input_mark != -1) {
            copy_text(*text, MIN(ui.hover.select_input_cursor, ui.hover.select_input_mark),
                             MAX(ui.hover.select_input_cursor, ui.hover.select_input_mark));
        }
        return false;
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V)) {
        const char* clipboard = GetClipboardText();
        if (clipboard) {
            delete_region(text);

            for (int i = 0; clipboard[i]; i++) {
                if (clipboard[i] == '\n' || clipboard[i] == '\r') continue;
                vector_insert(text, ui.hover.select_input_cursor++, clipboard[i]);
            }
            ui.render_surface_needs_redraw = true;
            return true;
        }
        return false;
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_X)) {
        if (ui.hover.select_input_mark != -1) {
            int sel_start = MIN(ui.hover.select_input_cursor, ui.hover.select_input_mark),
                sel_end   = MAX(ui.hover.select_input_cursor, ui.hover.select_input_mark);

            copy_text(*text, sel_start, sel_end);
            delete_region(text);
            return true;
        }
        return false;
    }

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        set_mark();
        ui.hover.select_input_cursor--;
        if (ui.hover.select_input_cursor < 0) {
            ui.hover.select_input_cursor = 0;
        } else {
            while (((unsigned char)(*text)[ui.hover.select_input_cursor] >> 6) == 2) ui.hover.select_input_cursor--;
        }
        ui.render_surface_needs_redraw = true;
        return false;
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        set_mark();
        ui.hover.select_input_cursor++;
        if (ui.hover.select_input_cursor >= (int)vector_size(*text)) {
            ui.hover.select_input_cursor = vector_size(*text) - 1;
        } else {
            while (((unsigned char)(*text)[ui.hover.select_input_cursor] >> 6) == 2) ui.hover.select_input_cursor++;
        }
        ui.render_surface_needs_redraw = true;
        return false;
    }

    if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
        if (vector_size(*text) <= 1 || (ui.hover.select_input_cursor == (int)vector_size(*text) - 1 && ui.hover.select_input_mark == -1)) return false;

        if (ui.hover.select_input_mark != -1) {
            delete_region(text);
        } else {
            int remove_pos = ui.hover.select_input_cursor;
            int remove_size;
            GetCodepointNext(*text + remove_pos, &remove_size);
            vector_erase(*text, remove_pos, remove_size);
            ui.render_surface_needs_redraw = true;
        }
        return true;
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (vector_size(*text) <= 1 || (ui.hover.select_input_cursor == 0 && ui.hover.select_input_mark == -1)) {
            return false;
        }

        if (ui.hover.select_input_mark != -1) {
            delete_region(text);
        } else {
            int remove_pos = ui.hover.select_input_cursor - 1;
            int remove_size = 1;
            while (((unsigned char)(*text)[remove_pos] >> 6) == 2) { // This checks if we are in the middle of UTF-8 char
                remove_pos--;
                remove_size++;
            }
            ui.hover.select_input_cursor -= remove_size;
            vector_erase(*text, remove_pos, remove_size);
            ui.render_surface_needs_redraw = true;
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
            vector_insert(text, ui.hover.select_input_cursor++, utf_char[i]);
        }
        input_changed = true;
        ui.render_surface_needs_redraw = true;
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

static void deselect_all(void) {
    ui.hover.editor.select_argument = NULL;
    ui.hover.select_input = NULL;
    ui.hover.dropdown.scroll_amount = 0;
}

void show_dropdown(char** list, int list_len, void* ref_object, ButtonClickHandler handler) {
    ui.hover.dropdown.ref_object = ref_object;
    ui.hover.dropdown.list = list;
    ui.hover.dropdown.list_len = list_len;
    ui.hover.dropdown.handler = handler;
    ui.hover.dropdown.select_ind = 0;
    ui.hover.dropdown.scroll_amount = 0;
    ui.hover.dropdown.shown = true;
}

bool handle_dropdown_close(void) {
    ui.hover.dropdown.ref_object = NULL;
    ui.hover.dropdown.list = NULL;
    ui.hover.dropdown.list_len = 0;
    ui.hover.dropdown.handler = NULL;
    ui.hover.dropdown.select_ind = 0;
    ui.hover.dropdown.scroll_amount = 0;
    ui.hover.editor.select_block = NULL;
    ui.hover.select_input = NULL;
    ui.hover.editor.select_argument = NULL;
    ui.hover.dropdown.shown = false;
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

bool save_project(void) {
    char const* filters[] = {"*.scrp"};

    char* path = tinyfd_saveFileDialog(NULL, editor.project_name, ARRLEN(filters), filters, "Scrap project files (.scrp)");
    if (!path) return false;
    save_code(path, &project_config, editor.code);

    char* base_path = get_basename(path);
    int i;
    for (i = 0; base_path[i]; i++) editor.project_name[i] = base_path[i];
    editor.project_name[i] = 0;
    editor.project_modified = false;
    return true;
}

void load_project(void) {
    char const* filters[] = {"*.scrp"};

    char* path = tinyfd_openFileDialog(NULL, editor.project_name, ARRLEN(filters), filters, "Scrap project files (.scrp)", 0);
    if (!path) return;

    ProjectConfig new_config;
    BlockChain* chain = load_code(path, &new_config);
    switch_tab_to_panel(PANEL_CODE);
    if (!chain) {
        actionbar_show(gettext("File load failed :("));
        return;
    }

    project_config_free(&project_config);
    project_config = new_config;

    for (size_t i = 0; i < vector_size(editor.code); i++) blockchain_free(&editor.code[i]);
    vector_free(editor.code);
    vm.compile_error_block = NULL;
    vm.compile_error_blockchain = NULL;
    editor.code = chain;

    editor.blockchain_select_counter = 0;
    editor.camera_pos.x = editor.code[editor.blockchain_select_counter].x - 50;
    editor.camera_pos.y = editor.code[editor.blockchain_select_counter].y - 50;

    char* base_path = get_basename(path);

    int i;
    for (i = 0; base_path[i]; i++) editor.project_name[i] = base_path[i];
    editor.project_name[i] = 0;

    actionbar_show(gettext("File load succeeded!"));
    editor.project_modified = false;
}

bool handle_file_menu_click(void) {
    switch (ui.hover.dropdown.select_ind) {
    case FILE_MENU_NEW_PROJECT:
        for (size_t i = 0; i < vector_size(editor.code); i++) blockchain_free(&editor.code[i]);
        vector_clear(editor.code);
        switch_tab_to_panel(PANEL_CODE);
        editor.project_modified = false;
        break;
    case FILE_MENU_SAVE_PROJECT:
        save_project();
        break;
    case FILE_MENU_LOAD_PROJECT:
        load_project();
        break;
    default:
        printf("idk\n");
        break;
    }
    return handle_dropdown_close();
}

bool handle_block_dropdown_click(void) {
    argument_set_const_string(ui.hover.editor.select_argument, ui.hover.dropdown.list[ui.hover.dropdown.select_ind]);
    return handle_dropdown_close();
}

bool handle_file_button_click(void) {
    if (thread_is_running(&vm.thread)) return true;
    show_dropdown(file_menu_list, ARRLEN(file_menu_list), NULL, handle_file_menu_click);
    return true;
}

bool handle_settings_button_click(void) {
    gui_window_show(draw_settings_window);
    return true;
}

bool handle_about_button_click(void) {
    gui_window_show(draw_about_window);
    return true;
}

bool handle_run_button_click(void) {
#ifdef USE_INTERPRETER
    vm_start();
#else
    vm_start(COMPILER_MODE_JIT);
#endif
    return true;
}

bool handle_build_button_click(void) {
    if (thread_is_running(&vm.thread)) return true;
    gui_window_show(draw_project_settings_window);
    return true;
}

bool handle_stop_button_click(void) {
    vm_stop();
    return true;
}

bool handle_category_click(void) {
    editor.palette.current_category = ui.hover.category;
    return true;
}

bool handle_jump_to_block_button_click(void) {
    ui.hover.editor.select_block = vm.compile_error_block;
    ui.hover.editor.select_blockchain = vm.compile_error_blockchain;
    return true;
}

bool handle_error_window_close_button_click(void) {
    clear_compile_error();
    return true;
}

bool handle_tab_button(void) {
    editor.current_tab = (int)(size_t)ui.hover.button.data;
    ui.shader_time = 0.0;
    return true;
}

bool handle_add_tab_button(void) {
    char* name = "";
    switch (ui.hover.panels.mouse_panel) {
    case PANEL_NONE:
        name = "Unknown";
        break;
    case PANEL_CODE:
        name = "Code";
        break;
    case PANEL_BLOCK_PALETTE:
        name = "Block editor.palette";
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

    tab_insert(name, panel_new(ui.hover.panels.mouse_panel), (int)(size_t)ui.hover.button.data);

    ui.hover.panels.mouse_panel = PANEL_NONE;
    editor.current_tab = (int)(size_t)ui.hover.button.data;
    ui.shader_time = 0.0;
    return true;
}

bool handle_panel_editor_save_button(void) {
    ui.hover.is_panel_edit_mode = false;
    save_config(&config);
    return true;
}

bool handle_panel_editor_cancel_button(void) {
    ui.hover.is_panel_edit_mode = false;
    return true;
}

bool handle_editor_add_arg_button(void) {
    Blockdef* blockdef = ui.hover.editor.argument->data.blockdef;
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
    Blockdef* blockdef = ui.hover.editor.argument->data.blockdef;
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
    Blockdef* blockdef = ui.hover.editor.argument->data.blockdef;

    assert(ui.hover.editor.blockdef_input != (size_t)-1);
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

    blockdef_delete_input(blockdef, ui.hover.editor.blockdef_input);

    deselect_all();
    return true;
}

bool handle_editor_edit_button(void) {
    ui.hover.editor.edit_blockdef = ui.hover.editor.argument->data.blockdef;
    ui.hover.editor.edit_block = ui.hover.editor.block;
    deselect_all();
    return true;
}

bool handle_editor_close_button(void) {
    ui.hover.editor.edit_blockdef = NULL;
    ui.hover.editor.edit_block = NULL;
    deselect_all();
    return true;
}

static void remove_blockdef(void) {
    for (size_t i = 0; i < vector_size(editor.mouse_blockchain.blocks); i++) {
        for (size_t j = 0; j < vector_size(editor.mouse_blockchain.blocks[i].arguments); j++) {
            Argument* arg = &editor.mouse_blockchain.blocks[i].arguments[j];
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
    if (ui.hover.editor.select_argument) {
        deselect_all();
        return true;
    }
    if (mouse_empty && ui.hover.editor.block) {
        // Pickup block
        TraceLog(LOG_INFO, "Pickup block");
        assert(editor.palette.current_category != NULL);

        // int ind = ui.hover.editor.blockchain - editor.palette.current_category->chains;
        // if (ind < 0 || ind > (int)vector_size(editor.palette.current_category->chains)) return true;

        blockchain_free(&editor.mouse_blockchain);
        editor.mouse_blockchain = blockchain_copy(ui.hover.editor.blockchain, 0);
        return true;
    } else if (!mouse_empty) {
        // Drop block
        TraceLog(LOG_INFO, "Drop block");
        remove_blockdef();
        blockchain_clear_blocks(&editor.mouse_blockchain);
        return true;
    }
    return true;
}

static bool handle_blockdef_editor_click(void) {
    if (!ui.hover.editor.blockdef) return true;
    if (ui.hover.editor.edit_blockdef == ui.hover.editor.argument->data.blockdef) return false;
    blockchain_add_block(&editor.mouse_blockchain, block_new_ms(ui.hover.editor.blockdef));
    deselect_all();
    return true;
}

static bool handle_code_editor_click(bool mouse_empty) {
    if (!mouse_empty) {
        editor.mouse_blockchain.x = gui->mouse_x;
        editor.mouse_blockchain.y = gui->mouse_y;
        if (ui.hover.editor.argument || ui.hover.editor.parent_argument) {
            if (vector_size(editor.mouse_blockchain.blocks) > 1) return true;
            if (editor.mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_CONTROLEND) return true;
            if (editor.mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return true;

            if (ui.hover.editor.argument) {
                // Attach to argument
                TraceLog(LOG_INFO, "Attach to argument");
                if (ui.hover.editor.argument->type != ARGUMENT_TEXT) return true;
                editor.mouse_blockchain.blocks[0].parent = ui.hover.editor.block;
                argument_set_block(ui.hover.editor.argument, editor.mouse_blockchain.blocks[0]);
                vector_clear(editor.mouse_blockchain.blocks);
                ui.hover.editor.select_blockchain = ui.hover.editor.blockchain;
                ui.hover.editor.select_block = &ui.hover.editor.argument->data.block;
                ui.hover.select_input = NULL;
                editor.project_modified = true;
            } else if (ui.hover.editor.parent_argument) {
                // Swap argument
                TraceLog(LOG_INFO, "Swap argument");
                if (ui.hover.editor.parent_argument->type != ARGUMENT_BLOCK) return true;
                editor.mouse_blockchain.blocks[0].parent = ui.hover.editor.block->parent;
                Block temp = editor.mouse_blockchain.blocks[0];
                editor.mouse_blockchain.blocks[0] = *ui.hover.editor.block;
                editor.mouse_blockchain.blocks[0].parent = NULL;
                block_update_parent_links(&editor.mouse_blockchain.blocks[0]);
                argument_set_block(ui.hover.editor.parent_argument, temp);
                ui.hover.editor.select_block = &ui.hover.editor.parent_argument->data.block;
                ui.hover.editor.select_blockchain = ui.hover.editor.blockchain;
                editor.project_modified = true;
            }
        } else if (
            ui.hover.editor.block &&
            ui.hover.editor.blockchain &&
            ui.hover.editor.block->parent == NULL
        ) {
            // Attach block
            TraceLog(LOG_INFO, "Attach block");
            if (editor.mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return true;

            int ind = ui.hover.editor.block - ui.hover.editor.blockchain->blocks;
            blockchain_insert(ui.hover.editor.blockchain, &editor.mouse_blockchain, ind);
            // Update block link to make valgrind happy
            ui.hover.editor.block = &ui.hover.editor.blockchain->blocks[ind];
            ui.hover.editor.select_block = ui.hover.editor.block + 1;
            ui.hover.editor.select_blockchain = ui.hover.editor.blockchain;
            editor.project_modified = true;
        } else {
            // Put block
            TraceLog(LOG_INFO, "Put block");
            editor.mouse_blockchain.x += editor.camera_pos.x - ui.hover.panels.panel_size.x;
            editor.mouse_blockchain.y += editor.camera_pos.y - ui.hover.panels.panel_size.y;
            vector_add(&editor.code, editor.mouse_blockchain);
            editor.mouse_blockchain = blockchain_new();
            ui.hover.editor.select_blockchain = &editor.code[vector_size(editor.code) - 1];
            ui.hover.editor.select_block = &ui.hover.editor.select_blockchain->blocks[0];
            editor.project_modified = true;
        }
        return true;
    } else if (ui.hover.editor.block) {
        if (ui.hover.editor.block->parent) {
            if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                // Copy argument
                TraceLog(LOG_INFO, "Copy argument");
                blockchain_add_block(&editor.mouse_blockchain, block_copy(ui.hover.editor.block, NULL));
            } else {
                // Detach argument
                TraceLog(LOG_INFO, "Detach argument");
                assert(ui.hover.editor.parent_argument != NULL);

                blockchain_add_block(&editor.mouse_blockchain, *ui.hover.editor.block);
                editor.mouse_blockchain.blocks[0].parent = NULL;

                argument_set_text(ui.hover.editor.parent_argument, "");
                ui.hover.editor.select_blockchain = NULL;
                ui.hover.editor.select_block = NULL;
                editor.project_modified = true;
            }
        } else if (ui.hover.editor.blockchain) {
            int ind = ui.hover.editor.block - ui.hover.editor.blockchain->blocks;

            if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Copy block
                    TraceLog(LOG_INFO, "Copy block");
                    blockchain_free(&editor.mouse_blockchain);

                    editor.mouse_blockchain = blockchain_copy_single(ui.hover.editor.blockchain, ind);
                } else {
                    // Copy chain
                    TraceLog(LOG_INFO, "Copy chain");
                    blockchain_free(&editor.mouse_blockchain);
                    editor.mouse_blockchain = blockchain_copy(ui.hover.editor.blockchain, ind);
                }
            } else {
                ui.hover.editor.edit_blockdef = NULL;
                ui.hover.editor.edit_block = NULL;
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Detach block
                    TraceLog(LOG_INFO, "Detach block");
                    blockchain_detach_single(&editor.mouse_blockchain, ui.hover.editor.blockchain, ind);
                    if (vector_size(ui.hover.editor.blockchain->blocks) == 0) {
                        blockchain_free(ui.hover.editor.blockchain);
                        vector_remove(editor.code, ui.hover.editor.blockchain - editor.code);
                        ui.hover.editor.block = NULL;
                    }
                    editor.project_modified = true;
                } else {
                    // Detach chain
                    TraceLog(LOG_INFO, "Detach chain");
                    int ind = ui.hover.editor.block - ui.hover.editor.blockchain->blocks;
                    blockchain_detach(&editor.mouse_blockchain, ui.hover.editor.blockchain, ind);
                    if (ind == 0) {
                        blockchain_free(ui.hover.editor.blockchain);
                        vector_remove(editor.code, ui.hover.editor.blockchain - editor.code);
                        ui.hover.editor.block = NULL;
                    }
                    editor.project_modified = true;
                }
                ui.hover.editor.select_blockchain = NULL;
                ui.hover.editor.select_block = NULL;
            }
        }
        return true;
    }
    return false;
}

static bool handle_editor_panel_click(void) {
    if (!ui.hover.panels.panel) return true;

    if (ui.hover.panels.panel->type == PANEL_SPLIT) {
        ui.hover.panels.drag_panel = ui.hover.panels.panel;
        ui.hover.panels.drag_panel_size = ui.hover.panels.panel_size;
        return false;
    }

    if (ui.hover.panels.mouse_panel == PANEL_NONE) {
        PanelTree* parent = ui.hover.panels.panel->parent;
        if (!parent) {
            if (vector_size(editor.tabs) > 1) {
                ui.hover.panels.mouse_panel = ui.hover.panels.panel->type;
                tab_delete(editor.current_tab);
            }
            return true;
        }

        ui.hover.panels.mouse_panel = ui.hover.panels.panel->type;
        free(ui.hover.panels.panel);
        PanelTree* other_panel = parent->left == ui.hover.panels.panel ? parent->right : parent->left;

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
        panel_split(ui.hover.panels.panel, ui.hover.panels.panel_side, ui.hover.panels.mouse_panel, 0.5);
        ui.hover.panels.mouse_panel = PANEL_NONE;
    }

    return true;
}

static void get_input_ind(void) {
    assert(ui.hover.input_info.font != NULL);
    assert(ui.hover.input_info.input != NULL);

    float width = 0.0;
    float prev_width = 0.0;

    int codepoint = 0; // Current character
    int index = 0; // Index position in sprite font
    int text_size = strlen(*ui.hover.input_info.input);
    float scale_factor = ui.hover.input_info.font_size / (float)ui.hover.input_info.font->baseSize;

    int prev_i = 0;
    int i = 0;

    while (i < text_size && (width * scale_factor) < ui.hover.input_info.rel_pos.x) {
        int next = 0;
        codepoint = GetCodepointNext(&(*ui.hover.input_info.input)[i], &next);
        index = search_glyph(codepoint);

        prev_width = width;
        prev_i = i;

        if (ui.hover.input_info.font->glyphs[index].advanceX != 0) {
            width += ui.hover.input_info.font->glyphs[index].advanceX;
        } else {
            width += ui.hover.input_info.font->recs[index].width + ui.hover.input_info.font->glyphs[index].offsetX;
        }
        i += next;
    }
    prev_width *= scale_factor;
    width *= scale_factor;

    if (width - ui.hover.input_info.rel_pos.x < ui.hover.input_info.rel_pos.x - prev_width) { // Right side of char is closer
        ui.hover.select_input_cursor = i;
    } else {
        ui.hover.select_input_cursor = prev_i;
    }
    ui.hover.select_input_mark = -1;
}

// Return value indicates if we should cancel dragging
static bool handle_mouse_click(void) {
    ui.hover.mouse_click_pos = (Vector2) { gui->mouse_x, gui->mouse_y };
    editor.camera_click_pos = editor.camera_pos;
    ui.hover.dragged_slider.value = NULL;

    if (ui.hover.select_input == &editor.search_list_search) {
        if (ui.hover.editor.blockdef) {
            blockchain_add_block(&editor.mouse_blockchain, block_new_ms(ui.hover.editor.blockdef));
            if (ui.hover.editor.blockdef->type == BLOCKTYPE_CONTROL && vm.end_blockdef != (size_t)-1) {
                blockchain_add_block(&editor.mouse_blockchain, block_new_ms(vm.blockdefs[vm.end_blockdef]));
            }
        }

        ui.hover.select_input = NULL;
        ui.hover.editor.block = NULL;
        return true;
    }

    if (ui.hover.button.handler) return ui.hover.button.handler();
    if (ui.hover.hover_slider.value) {
        ui.hover.dragged_slider = ui.hover.hover_slider;
        ui.hover.slider_last_val = *ui.hover.dragged_slider.value;
        return false;
    }
    if (gui_window_is_shown()) {
        if (ui.hover.input_info.input) get_input_ind();
        if (ui.hover.input_info.input != ui.hover.select_input) ui.hover.select_input = ui.hover.input_info.input;
        return true;
    }
    if (!ui.hover.panels.panel) return true;
    if (ui.hover.is_panel_edit_mode) return handle_editor_panel_click();
    if (ui.hover.panels.panel->type == PANEL_TERM) return true;
    if (thread_is_running(&vm.thread)) return ui.hover.panels.panel->type != PANEL_CODE;

    if (ui.hover.input_info.input) get_input_ind();
    if (ui.hover.input_info.input != ui.hover.select_input) ui.hover.select_input = ui.hover.input_info.input;

    bool mouse_empty = vector_size(editor.mouse_blockchain.blocks) == 0;

    if (ui.hover.panels.panel->type == PANEL_BLOCK_PALETTE) return handle_block_palette_click(mouse_empty);

    if (mouse_empty && ui.hover.editor.argument && ui.hover.editor.argument->type == ARGUMENT_BLOCKDEF) {
        if (handle_blockdef_editor_click()) return true;
    }

    if (mouse_empty) {
        if (ui.hover.editor.block && ui.hover.editor.argument) {
            Input block_input = ui.hover.editor.block->blockdef->inputs[ui.hover.editor.argument->input_id];
            if (block_input.type == INPUT_DROPDOWN) {
                size_t list_len = 0;
                char** list = block_input.data.drop.list(ui.hover.editor.block, &list_len);

                show_dropdown(list, list_len, ui.hover.editor.argument, handle_block_dropdown_click);
            }
        }

        if (ui.hover.editor.blockchain != ui.hover.editor.select_blockchain) {
            ui.hover.editor.select_blockchain = ui.hover.editor.blockchain;
            if (ui.hover.editor.select_blockchain) editor.blockchain_select_counter = ui.hover.editor.select_blockchain - editor.code;
        }

        if (ui.hover.editor.block != ui.hover.editor.select_block) {
            ui.hover.editor.select_block = ui.hover.editor.block;
        }

        if (ui.hover.editor.argument != ui.hover.editor.select_argument) {
            if (!ui.hover.editor.argument || ui.hover.input_info.input || ui.hover.dropdown.shown) {
                ui.hover.editor.select_argument = ui.hover.editor.argument;
            }
            ui.hover.dropdown.scroll_amount = 0;
            return true;
        }

        if (ui.hover.editor.select_argument) {
            return true;
        }
    }

    if (ui.hover.panels.panel->type == PANEL_CODE && handle_code_editor_click(mouse_empty)) return true;
    return ui.hover.panels.panel->type != PANEL_CODE;
}

static void block_next_argument() {
    Argument* args = ui.hover.editor.select_block->arguments;
    Argument* arg = ui.hover.editor.select_argument ? ui.hover.editor.select_argument + 1 : &args[0];
    if (arg - args >= (int)vector_size(args)) {
        if (ui.hover.editor.select_block->parent) {
            Argument* parent_args = ui.hover.editor.select_block->parent->arguments;
            for (size_t i = 0; i < vector_size(parent_args); i++) {
                if (parent_args[i].type == ARGUMENT_BLOCK && &parent_args[i].data.block == ui.hover.editor.select_block) ui.hover.editor.select_argument = &parent_args[i];
            }
            ui.hover.editor.select_block = ui.hover.editor.select_block->parent;
            block_next_argument();
        } else {
            ui.hover.editor.select_argument = NULL;
        }
        return;
    }

    if (arg->type == ARGUMENT_TEXT || arg->type == ARGUMENT_CONST_STRING) {
        ui.hover.editor.select_argument = arg;
    } else if (arg->type == ARGUMENT_BLOCK) {
        ui.hover.editor.select_argument = NULL;
        ui.hover.editor.select_block = &arg->data.block;
    }
}

static void block_prev_argument() {
    Argument* args = ui.hover.editor.select_block->arguments;
    Argument* arg = ui.hover.editor.select_argument ? ui.hover.editor.select_argument - 1 : &args[-1];
    if (arg - args < 0) {
        if (ui.hover.editor.select_argument) {
            ui.hover.editor.select_argument = NULL;
            return;
        }
        if (ui.hover.editor.select_block->parent) {
            Argument* parent_args = ui.hover.editor.select_block->parent->arguments;
            for (size_t i = 0; i < vector_size(parent_args); i++) {
                if (parent_args[i].type == ARGUMENT_BLOCK && &parent_args[i].data.block == ui.hover.editor.select_block) ui.hover.editor.select_argument = &parent_args[i];
            }
            ui.hover.editor.select_block = ui.hover.editor.select_block->parent;
            block_prev_argument();
        } else {
            ui.hover.editor.select_argument = NULL;
        }
        return;
    }

    if (arg->type == ARGUMENT_TEXT || arg->type == ARGUMENT_CONST_STRING) {
        ui.hover.editor.select_argument = arg;
    } else if (arg->type == ARGUMENT_BLOCK) {
        ui.hover.editor.select_argument = NULL;
        ui.hover.editor.select_block = &arg->data.block;
        while (vector_size(ui.hover.editor.select_block->arguments) != 0) {
            arg = &ui.hover.editor.select_block->arguments[vector_size(ui.hover.editor.select_block->arguments) - 1];
            if (arg->type == ARGUMENT_TEXT || arg->type == ARGUMENT_CONST_STRING) {
                ui.hover.editor.select_argument = arg;
                break;
            } else if (arg->type == ARGUMENT_BLOCK) {
                ui.hover.editor.select_block = &arg->data.block;
            }
        }
    }
}

static bool handle_code_panel_key_press(void) {
    if (ui.hover.editor.select_argument && !ui.hover.select_input) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            ui.hover.select_input = &ui.hover.editor.select_argument->data.text;
            ui.hover.select_input_mark = 0;
            ui.hover.select_input_cursor = strlen(*ui.hover.select_input);
            ui.render_surface_needs_redraw = true;
            return true;
        }
    }

    if (IsKeyPressed(KEY_TAB) && vector_size(editor.code) > 0) {
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            editor.blockchain_select_counter--;
            if (editor.blockchain_select_counter < 0) editor.blockchain_select_counter = vector_size(editor.code) - 1;
        } else {
            editor.blockchain_select_counter++;
            if ((vec_size_t)editor.blockchain_select_counter >= vector_size(editor.code)) editor.blockchain_select_counter = 0;
        }

        ui.hover.select_input = NULL;
        ui.hover.editor.select_argument = NULL;
        ui.hover.editor.select_block = &editor.code[editor.blockchain_select_counter].blocks[0];
        ui.hover.editor.select_blockchain = &editor.code[editor.blockchain_select_counter];
        editor.camera_pos.x = editor.code[editor.blockchain_select_counter].x - 50;
        editor.camera_pos.y = editor.code[editor.blockchain_select_counter].y - 50;
        actionbar_show(TextFormat(gettext("Jump to chain (%d/%d)"), editor.blockchain_select_counter + 1, vector_size(editor.code)));
        ui.render_surface_needs_redraw = true;
        return true;
    }

    if (!ui.hover.editor.select_blockchain || !ui.hover.editor.select_block || ui.hover.select_input) return false;

    int bounds_x = MIN(200, ui.hover.panels.code_panel_bounds.width / 2);
    int bounds_y = MIN(200, ui.hover.panels.code_panel_bounds.height / 2);

    if (ui.hover.editor.select_block_pos.x - (ui.hover.panels.code_panel_bounds.x + ui.hover.panels.code_panel_bounds.width) > -bounds_x) {
        editor.camera_pos.x += ui.hover.editor.select_block_pos.x - (ui.hover.panels.code_panel_bounds.x + ui.hover.panels.code_panel_bounds.width) + bounds_x;
        ui.render_surface_needs_redraw = true;
    }

    if (ui.hover.editor.select_block_pos.x - ui.hover.panels.code_panel_bounds.x < bounds_x) {
        editor.camera_pos.x += ui.hover.editor.select_block_pos.x - ui.hover.panels.code_panel_bounds.x - bounds_x;
        ui.render_surface_needs_redraw = true;
    }

    if (ui.hover.editor.select_block_pos.y - (ui.hover.panels.code_panel_bounds.y + ui.hover.panels.code_panel_bounds.height) > -bounds_y) {
        editor.camera_pos.y += ui.hover.editor.select_block_pos.y - (ui.hover.panels.code_panel_bounds.y + ui.hover.panels.code_panel_bounds.height) + bounds_y;
        ui.render_surface_needs_redraw = true;
    }

    if (ui.hover.editor.select_block_pos.y - ui.hover.panels.code_panel_bounds.y < bounds_y) {
        editor.camera_pos.y += ui.hover.editor.select_block_pos.y - ui.hover.panels.code_panel_bounds.y - bounds_y;
        ui.render_surface_needs_redraw = true;
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        block_next_argument();
        ui.render_surface_needs_redraw = true;
        return true;
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        block_prev_argument();
        ui.render_surface_needs_redraw = true;
        return true;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) {
        while (ui.hover.editor.select_block->parent) ui.hover.editor.select_block = ui.hover.editor.select_block->parent;
        ui.hover.editor.select_block--;
        ui.hover.editor.select_argument = NULL;
        if (ui.hover.editor.select_block < ui.hover.editor.select_blockchain->blocks) ui.hover.editor.select_block = ui.hover.editor.select_blockchain->blocks;
        ui.render_surface_needs_redraw = true;
        return true;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) {
        while (ui.hover.editor.select_block->parent) ui.hover.editor.select_block = ui.hover.editor.select_block->parent;
        ui.hover.editor.select_block++;
        ui.hover.editor.select_argument = NULL;
        if (ui.hover.editor.select_block - ui.hover.editor.select_blockchain->blocks >= (int)vector_size(ui.hover.editor.select_blockchain->blocks)) {
            ui.hover.editor.select_block--;
        }
        ui.render_surface_needs_redraw = true;
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
    if (search_string(blockdef->id, editor.search_list_search)) return true;
    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (blockdef->inputs[i].type != INPUT_TEXT_DISPLAY) continue;
        if (search_string(blockdef->inputs[i].data.text, editor.search_list_search)) return true;
    }
    return false;
}

void update_search(void) {
    vector_clear(editor.search_list);
    for (size_t i = 0; i < vector_size(vm.blockdefs); i++) {
        if (vm.blockdefs[i]->type == BLOCKTYPE_END) continue;
        if (!search_blockdef(vm.blockdefs[i])) continue;

        vector_add(&editor.search_list, vm.blockdefs[i]);
    }
}

static void handle_key_press(void) {
    if (IsKeyPressed(KEY_F5)) {
#ifdef USE_INTERPRETER
        vm_start();
#else
        vm_start(COMPILER_MODE_JIT);
#endif
        return;
    }
    if (IsKeyPressed(KEY_F6)) {
        vm_stop();
        return;
    }
    if (IsKeyPressed(KEY_S) &&
        ui.hover.select_input != &editor.search_list_search &&
        vector_size(editor.mouse_blockchain.blocks) == 0 &&
        !ui.hover.is_panel_edit_mode &&
        ui.hover.panels.panel &&
        ui.hover.panels.panel->type == PANEL_CODE &&
        !thread_is_running(&vm.thread) &&
        !gui_window_is_shown() &&
        !ui.hover.select_input)
    {
        vector_clear(editor.search_list_search);
        vector_add(&editor.search_list_search, 0);
        ui.hover.select_input = &editor.search_list_search;
        ui.hover.select_input_cursor = 0;
        ui.hover.select_input_mark = -1;
        ui.render_surface_needs_redraw = true;
        update_search();
        return;
    }

    if (ui.hover.panels.panel) {
        if (ui.hover.panels.panel->type == PANEL_TERM) {
            if (!thread_is_running(&vm.thread)) return;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                term_input_put_char('\n');
                term_print_str("\n");
                ui.render_surface_needs_redraw = true;
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
                ui.render_surface_needs_redraw = true;
            }
            return;
        } else if (ui.hover.panels.panel->type == PANEL_CODE) {
            if (handle_code_panel_key_press()) return;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        ui.hover.select_input = NULL;
        ui.hover.editor.select_argument = NULL;
        ui.render_surface_needs_redraw = true;
        return;
    }
    if (ui.hover.editor.select_block && ui.hover.editor.select_argument && ui.hover.editor.select_block->blockdef->inputs[ui.hover.editor.select_argument->input_id].type == INPUT_DROPDOWN) return;

    if (edit_text(ui.hover.select_input)) {
        if (ui.hover.select_input == &editor.search_list_search) update_search();
    }
}

static void handle_mouse_wheel(void) {
    if (!ui.hover.panels.panel) return;
    if (ui.hover.panels.panel->type != PANEL_CODE) return;
    if (ui.hover.editor.select_argument) return;
    if (ui.hover.is_panel_edit_mode) return;
    if (ui.hover.select_input) return;
    if (gui_window_is_shown()) return;

    Vector2 wheel = GetMouseWheelMoveV();
    editor.camera_pos.x -= wheel.x * config.ui_size * 2;
    editor.camera_pos.y -= wheel.y * config.ui_size * 2;

    if (wheel.x != 0 || wheel.y != 0) {
        ui.hover.editor.select_block = NULL;
        ui.hover.editor.select_argument = NULL;
        ui.hover.select_input = NULL;
        ui.hover.editor.select_blockchain = NULL;
    }
}

static void handle_mouse_drag(void) {
    if (ui.hover.drag_cancelled) return;

    if (ui.hover.is_panel_edit_mode && ui.hover.panels.drag_panel && ui.hover.panels.drag_panel->type == PANEL_SPLIT) {
        if (ui.hover.panels.drag_panel->direction == DIRECTION_HORIZONTAL) {
            ui.hover.panels.drag_panel->split_percent = CLAMP(
                (gui->mouse_x - ui.hover.panels.drag_panel_size.x - 5) / ui.hover.panels.drag_panel_size.width,
                0.0,
                1.0 - (10.0 / ui.hover.panels.drag_panel_size.width)
            );
        } else {
            ui.hover.panels.drag_panel->split_percent = CLAMP(
                (gui->mouse_y - ui.hover.panels.drag_panel_size.y - 5) / ui.hover.panels.drag_panel_size.height,
                0.0,
                1.0 - (10.0 / ui.hover.panels.drag_panel_size.height)
            );
        }
        return;
    }

    if (ui.hover.dragged_slider.value) {
        *ui.hover.dragged_slider.value = CLAMP(
            ui.hover.slider_last_val + (gui->mouse_x - ui.hover.mouse_click_pos.x) / 2,
            ui.hover.dragged_slider.min,
            ui.hover.dragged_slider.max
        );
        return;
    }

    editor.camera_pos.x = editor.camera_click_pos.x - (gui->mouse_x - ui.hover.mouse_click_pos.x);
    editor.camera_pos.y = editor.camera_click_pos.y - (gui->mouse_y - ui.hover.mouse_click_pos.y);
}

void scrap_gui_process_ui(void) {
    editor.actionbar.show_time -= GetFrameTime();
    if (editor.actionbar.show_time < 0) {
        editor.actionbar.show_time = 0;
    } else {
        ui.render_surface_needs_redraw = true;
    }

    if (ui.shader_time_loc != -1) SetShaderValue(assets.line_shader, ui.shader_time_loc, &ui.shader_time, SHADER_UNIFORM_FLOAT);
    ui.shader_time += GetFrameTime() / 2.0;
    if (ui.shader_time >= 1.0) {
        ui.shader_time = 1.0;
    } else {
        ui.render_surface_needs_redraw = true;
    }

    int prev_mouse_scroll = gui->mouse_scroll;
    gui_update_mouse_scroll(gui, GetMouseWheelMove());
    if (prev_mouse_scroll != gui->mouse_scroll) ui.render_surface_needs_redraw = true;

    if (IsWindowResized()) {
        ui.shader_time = 0.0;
        gui_update_window_size(gui, GetScreenWidth(), GetScreenHeight());
        UnloadRenderTexture(ui.render_surface);
        ui.render_surface = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        SetTextureWrap(ui.render_surface.texture, TEXTURE_WRAP_MIRROR_REPEAT);
        ui.render_surface_needs_redraw = true;
    }

    Vector2 delta = GetMouseDelta();
    if (delta.x != 0 || delta.y != 0) ui.render_surface_needs_redraw = true;

    if (GetMouseWheelMove() != 0.0) {
        handle_mouse_wheel();
        ui.render_surface_needs_redraw = true;
    }

#ifdef ARABIC_MODE
    gui_update_mouse_pos(gui, gui->win_w - GetMouseX(), GetMouseY());
#else
    gui_update_mouse_pos(gui, GetMouseX(), GetMouseY());
#endif

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ui.hover.drag_cancelled = handle_mouse_click();
        ui.render_surface_needs_redraw = true;
#ifdef DEBUG
        // This will traverse through all blocks in codebase, which is expensive in large codebase.
        // Ideally all functions should not be broken in the first place. This helps with debugging invalid states
        sanitize_links();
#endif
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        ui.hover.mouse_click_pos = (Vector2) { gui->mouse_x, gui->mouse_y };
        editor.camera_click_pos = editor.camera_pos;
        ui.hover.editor.select_block = NULL;
        ui.hover.editor.select_argument = NULL;
        ui.hover.select_input = NULL;
        ui.hover.editor.select_blockchain = NULL;
        ui.render_surface_needs_redraw = true;
    } else if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        handle_mouse_drag();
    } else {
        ui.hover.drag_cancelled = false;
        ui.hover.dragged_slider.value = NULL;
        ui.hover.panels.drag_panel = NULL;
        handle_key_press();
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) ui.render_surface_needs_redraw = true;

    handle_window();

    if (ui.render_surface_needs_redraw) {
        ui.hover.editor.block = NULL;
        ui.hover.editor.argument = NULL;
        ui.hover.input_info.input = NULL;
        ui.hover.category = NULL;
        ui.hover.editor.parent_argument = NULL;
        ui.hover.editor.prev_blockchain = NULL;
        ui.hover.editor.blockchain = NULL;
        ui.hover.editor.part = EDITOR_NONE;
        ui.hover.editor.blockdef = NULL;
        ui.hover.editor.blockdef_input = -1;
        ui.hover.button.handler = NULL;
        ui.hover.button.data = NULL;
        ui.hover.hover_slider.value = NULL;
        ui.hover.panels.panel = NULL;
        ui.hover.panels.panel_size = (Rectangle) {0};
        ui.hover.editor.select_valid = false;

#ifdef DEBUG
        Timer t = start_timer("gui process");
#endif
        scrap_gui_process();
#ifdef DEBUG
        ui.ui_time = end_timer(t);
#endif

        if (vm.start_timeout >= 0) vm.start_timeout--;
        // This fixes selecting wrong argument of a block when two blocks overlap
        if (ui.hover.editor.block && ui.hover.editor.argument) {
            int ind = ui.hover.editor.argument - ui.hover.editor.block->arguments;
            if (ind < 0 || ind > (int)vector_size(ui.hover.editor.block->arguments)) ui.hover.editor.argument = NULL;
        }

        if (ui.hover.editor.select_block && !ui.hover.editor.select_valid) {
            TraceLog(LOG_WARNING, "Invalid selection: %p", ui.hover.editor.select_block);
            ui.hover.editor.select_block = NULL;
            ui.hover.editor.select_blockchain = NULL;
        }
    }

    ui.hover.editor.prev_block = ui.hover.editor.block;
    ui.hover.editor.prev_argument = ui.hover.editor.argument;
    ui.hover.editor.prev_blockdef = ui.hover.editor.blockdef;
    ui.hover.panels.prev_panel = ui.hover.panels.panel;
}
