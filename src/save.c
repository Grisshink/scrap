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
#include "vec.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <libintl.h>

#include "../external/cfgpath.h"

typedef struct {
    void* ptr;
    size_t size;
    size_t capacity;
} SaveData;

const int codepoint_regions[CODEPOINT_REGION_COUNT][2] = {
    { 0x20, 0x7e }, // All printable ASCII chars
    { 0x3bc, 0x3bc }, // Letter μ
    { 0x400, 0x4ff }, // Cyrillic letters
};
int codepoint_start_ranges[CODEPOINT_REGION_COUNT] = {0};

char* language_list[5] = {
    "System",
    "English [en]",
    "Russian [ru]",
    "Kazakh [kk]",
    "Ukrainian [uk]",
};

char scrap_ident[] = "SCRAP";
const char** save_block_ids = NULL;
Blockdef** save_blockdefs = NULL;
static unsigned int ver = 0;

int save_find_id(const char* id);
void save_code(const char* file_path, ProjectConfig* config, BlockChain* code);
BlockChain* load_code(const char* file_path, ProjectConfig* out_config);
void save_block(SaveData* save, Block* block);
bool load_block(SaveData* save, Block* block);
void save_blockdef(SaveData* save, Blockdef* blockdef);
Blockdef* load_blockdef(SaveData* save);

const char* language_to_code(Language lang) {
    switch (lang) {
        case LANG_SYSTEM: return "system";
        case LANG_EN: return "en";
        case LANG_RU: return "ru";
        case LANG_KK: return "kk";
        case LANG_UK: return "uk";
    }
    assert(false && "Unreachable");
}

Language code_to_language(const char* code) {
    if (!strcmp(code, "en")) {
        return LANG_EN;
    } else if (!strcmp(code, "ru")) {
        return LANG_RU;
    } else if (!strcmp(code, "kk")) {
        return LANG_KK;
    } else if (!strcmp(code, "uk")) {
        return LANG_UK;
    } else {
        return LANG_SYSTEM;
    }
}

void vector_set_string(char** vec, char* str) {
    vector_clear(*vec);
    for (char* i = str; *i; i++) vector_add(vec, *i);
    vector_add(vec, 0);
}

void config_new(Config* config) {
    config->font_path = vector_create();
    config->font_bold_path = vector_create();
    config->font_mono_path = vector_create();
}

void config_free(Config* config) {
    vector_free(config->font_path);
    vector_free(config->font_bold_path);
    vector_free(config->font_mono_path);
}

void config_copy(Config* dst, Config* src) {
    dst->ui_size = src->ui_size;
    dst->fps_limit = src->fps_limit;
    dst->language = src->language;
    dst->block_size_threshold = src->block_size_threshold;
    dst->font_path = vector_copy(src->font_path);
    dst->font_bold_path = vector_copy(src->font_bold_path);
    dst->font_mono_path = vector_copy(src->font_mono_path);
}

void set_default_config(Config* config) {
    config->ui_size = 32;
    config->fps_limit = 60;
    config->block_size_threshold = 1000;
    config->language = LANG_SYSTEM;
    vector_set_string(&config->font_path, DATA_PATH "nk57-cond.otf");
    vector_set_string(&config->font_bold_path, DATA_PATH "nk57-eb.otf");
    vector_set_string(&config->font_mono_path, DATA_PATH "nk57.otf");
}

void project_config_new(ProjectConfig* config) {
    config->executable_name = vector_create();
    config->linker_name = vector_create();
}

void project_config_free(ProjectConfig* config) {
    vector_free(config->executable_name);
    vector_free(config->linker_name);
}

void project_config_set_default(ProjectConfig* config) {
    vector_set_string(&config->executable_name, "project");
    vector_set_string(&config->linker_name, "ld");
}

void apply_config(Config* dst, Config* src) {
    dst->fps_limit = src->fps_limit; SetTargetFPS(dst->fps_limit);
    dst->block_size_threshold = src->block_size_threshold;
}

void save_panel_config(char* file_str, int* cursor, PanelTree* panel) {
    switch (panel->type) {
    case PANEL_NONE:
        *cursor += sprintf(file_str + *cursor, "PANEL_NONE ");
        break;
    case PANEL_CODE:
        *cursor += sprintf(file_str + *cursor, "PANEL_CODE ");
        break;
    case PANEL_TERM:
        *cursor += sprintf(file_str + *cursor, "PANEL_TERM ");
        break;
    case PANEL_BLOCK_PALETTE:
        *cursor += sprintf(file_str + *cursor, "PANEL_BLOCK_PALETTE ");
        break;
    case PANEL_SPLIT:
        *cursor += sprintf(
            file_str + *cursor,
            "PANEL_SPLIT %s %f ",
            panel->direction == DIRECTION_HORIZONTAL ? "DIRECTION_HORIZONTAL" : "DIRECTION_VERTICAL",
            panel->split_percent
        );
        save_panel_config(file_str, cursor, panel->left);
        save_panel_config(file_str, cursor, panel->right);
        break;
    case PANEL_BLOCK_CATEGORIES:
        *cursor += sprintf(file_str + *cursor, "PANEL_BLOCK_CATEGORIES ");
        break;
    }
}

static char* read_panel_token(char** str, bool* is_eof) {
    if (*is_eof) return NULL;
    while (**str == ' ' || **str == '\0') {
        (*str)++;
        if (**str == '\0') return NULL;
    }

    char* out = *str;
    while (**str != ' ' && **str != '\0') (*str)++;
    if (**str == '\0') *is_eof = true;
    **str = '\0';

    return out;
}

PanelTree* load_panel_config(char** config) {
    bool is_eof = false;

    char* name = read_panel_token(config, &is_eof);
    if (!name) return NULL;

    if (!strcmp(name, "PANEL_SPLIT")) {
        char* direction = read_panel_token(config, &is_eof);
        if (!direction) return NULL;
        char* split_percent = read_panel_token(config, &is_eof);
        if (!split_percent) return NULL;

        GuiElementDirection dir;
        if (!strcmp(direction, "DIRECTION_HORIZONTAL")) {
            dir = DIRECTION_HORIZONTAL;
        } else if (!strcmp(direction, "DIRECTION_VERTICAL")) {
            dir = DIRECTION_VERTICAL;
        } else {
            return NULL;
        }

        float percent = CLAMP(atof(split_percent), 0.0, 1.0);

        PanelTree* left = load_panel_config(config);
        if (!left) return NULL;
        PanelTree* right = load_panel_config(config);
        if (!right) {
            panel_delete(left);
            return NULL;
        }

        PanelTree* panel = malloc(sizeof(PanelTree));
        panel->type = PANEL_SPLIT;
        panel->direction = dir;
        panel->parent = NULL;
        panel->split_percent = percent;
        panel->left = left;
        panel->right = right;

        left->parent = panel;
        right->parent = panel;

        return panel;
    } else if (!strcmp(name, "PANEL_NONE")) {
        return panel_new(PANEL_NONE);
    } else if (!strcmp(name, "PANEL_CODE")) {
        return panel_new(PANEL_CODE);
    } else if (!strcmp(name, "PANEL_TERM")) {
        return panel_new(PANEL_TERM);
    } else if (!strcmp(name, "PANEL_SIDEBAR")) { // Legacy panel name
        return panel_new(PANEL_BLOCK_PALETTE);
    } else if (!strcmp(name, "PANEL_BLOCK_PALETTE")) {
        return panel_new(PANEL_BLOCK_PALETTE);
    } else if (!strcmp(name, "PANEL_BLOCK_CATEGORIES")) {
        return panel_new(PANEL_BLOCK_CATEGORIES);
    }

    TraceLog(LOG_ERROR, "Unknown panel type: %s", name);
    return NULL;
}

void save_config(Config* config) {
    char* file_str = malloc(sizeof(char) * 32768);
    file_str[0] = 0;
    int cursor = 0;

    cursor += sprintf(file_str + cursor, "LANGUAGE=%s\n", language_to_code(config->language));
    cursor += sprintf(file_str + cursor, "UI_SIZE=%u\n", config->ui_size);
    cursor += sprintf(file_str + cursor, "FPS_LIMIT=%u\n", config->fps_limit);
    cursor += sprintf(file_str + cursor, "BLOCK_SIZE_THRESHOLD=%u\n", config->block_size_threshold);
    cursor += sprintf(file_str + cursor, "FONT_PATH=%s\n", config->font_path);
    cursor += sprintf(file_str + cursor, "FONT_BOLD_PATH=%s\n", config->font_bold_path);
    cursor += sprintf(file_str + cursor, "FONT_MONO_PATH=%s\n", config->font_mono_path);
    for (size_t i = 0; i < vector_size(editor.tabs); i++) {
        cursor += sprintf(file_str + cursor, "CONFIG_TAB_%s=", editor.tabs[i].name);
        save_panel_config(file_str, &cursor, editor.tabs[i].root_panel);
        cursor += sprintf(file_str + cursor, "\n");
    }

    char config_path[MAX_PATH + 10];
    get_user_config_folder(config_path, ARRLEN(config_path), CONFIG_FOLDER_NAME);
    strcat(config_path, CONFIG_PATH);

    SaveFileText(config_path, file_str);
    free(file_str);
}

PanelTree* find_panel_in_all_tabs(PanelType panel_type) {
    for (size_t i = 0; i < vector_size(editor.tabs); i++) {
        PanelTree* panel = find_panel(editor.tabs[i].root_panel, panel_type);
        if (panel) return panel;
    }
    return NULL;
}

void add_missing_panels(void) {
    PanelTree* categories = find_panel_in_all_tabs(PANEL_BLOCK_CATEGORIES);
    if (categories) return;

    PanelTree* palette = find_panel_in_all_tabs(PANEL_BLOCK_PALETTE);
    if (!palette) {
        TraceLog(LOG_ERROR, "Failed to insert missing panel PANEL_BLOCK_CATEGORIES: panel PANEL_BLOCK_PALETTE is missing");
        return;
    }
    panel_split(palette, SPLIT_SIDE_TOP, PANEL_BLOCK_CATEGORIES, 0.35);
}

void load_config(Config* config) {
    delete_all_tabs();

    char config_path[MAX_PATH + 10];
    get_user_config_folder(config_path, ARRLEN(config_path), CONFIG_FOLDER_NAME);
    strcat(config_path, CONFIG_PATH);

    char* file = LoadFileText(config_path);
    if (!file) {
        init_panels();
        editor.current_tab = 0;
        return;
    }
    int cursor = 0;

    bool has_lines = true;
    while (has_lines) {
        char* field = &file[cursor];
        while(file[cursor] != '=' && file[cursor] != '\n' && file[cursor] != '\0') cursor++;
        if (file[cursor] == '\n') {
            cursor++;
            continue;
        };
        if (file[cursor] == '\0') break;
        file[cursor++] = '\0';

        char* value = &file[cursor];
        int value_size = 0;
        while(file[cursor] != '\n' && file[cursor] != '\0') {
            cursor++;
            value_size++;
        }
        (void) value_size;
        if (file[cursor] == '\0') has_lines = false;
        file[cursor++] = '\0';

        if (!strcmp(field, "UI_SIZE")) {
            int val = atoi(value);
            config->ui_size = val ? val : config->ui_size;
        } else if (!strcmp(field, "FPS_LIMIT")) {
            int val = atoi(value);
            config->fps_limit = val ? val : config->fps_limit;
        } else if (!strcmp(field, "BLOCK_SIZE_THRESHOLD")) {
            int val = atoi(value);
            config->block_size_threshold = val ? val : config->block_size_threshold;
        } else if (!strcmp(field, "FONT_PATH")) {
            vector_set_string(&config->font_path, value);
        } else if (!strcmp(field, "FONT_BOLD_PATH")) {
            vector_set_string(&config->font_bold_path, value);
        } else if (!strcmp(field, "FONT_MONO_PATH")) {
            vector_set_string(&config->font_mono_path, value);
        } else if (!strncmp(field, "CONFIG_TAB_", sizeof("CONFIG_TAB_") - 1)) {
            char* panel_value = value;
            tab_new(field + sizeof("CONFIG_TAB_") - 1, load_panel_config(&panel_value));
        } else if (!strcmp(field, "LANGUAGE")) {
            Language lang = code_to_language(value);
            config->language = lang;
        } else {
            TraceLog(LOG_WARNING, "Unknown key: %s", field);
        }
    }

    add_missing_panels();
    if (vector_size(editor.tabs) == 0) init_panels();
    if (editor.current_tab >= (int)vector_size(editor.tabs)) editor.current_tab = vector_size(editor.tabs) - 1;

    UnloadFileText(file);
}

#define save_add(save, data) save_add_item(save, &data, sizeof(data))

void* save_read_item(SaveData* save, size_t data_size) {
    if (save->size + data_size > save->capacity) {
        TraceLog(LOG_ERROR, "[LOAD] Unexpected EOF reading data");
        return NULL;
    }
    void* ptr = save->ptr + save->size;
    save->size += data_size;
    return ptr;
}

bool save_read_varint(SaveData* save, unsigned int* out) {
    *out = 0;
    int pos = 0;
    unsigned char* chunk = NULL;
    do {
        chunk = save_read_item(save, sizeof(unsigned char));
        if (!chunk) return false;
        *out |= (*chunk & 0x7f) << pos;
        pos += 7;
    } while ((*chunk & 0x80) == 0);
    return true;
}

void* save_read_array(SaveData* save, size_t data_size, unsigned int* array_len) {
    if (!save_read_varint(save, array_len)) return NULL;
    return save_read_item(save, data_size * *array_len);
}

void save_add_item(SaveData* save, const void* data, size_t data_size) {
    if (save->size + data_size > save->capacity) {
        save->capacity = save->capacity > 0 ? save->capacity * 2 : 256;
        save->ptr = realloc(save->ptr, save->capacity);
    }

    memcpy(save->ptr + save->size, data, data_size);
    save->size += data_size;
}

void save_add_varint(SaveData* save, unsigned int data) {
    unsigned char varint = 0;
    do {
        varint = data & 0x7f;
        data >>= 7;
        varint |= (data == 0) << 7;
        save_add(save, varint);
    } while (data);
}

void save_add_array(SaveData* save, const void* array, int array_size, size_t data_size) {
    save_add_varint(save, array_size);
    for (int i = 0; i < array_size; i++) save_add_item(save, array + data_size * i, data_size);
}

void free_save(SaveData* save) {
    free(save->ptr);
    save->size = 0;
    save->capacity = 0;
}

void save_blockdef_input(SaveData* save, Input* input) {
    save_add_varint(save, input->type);
    switch (input->type) {
    case INPUT_TEXT_DISPLAY:
        save_add_array(save, input->data.text, vector_size(input->data.text), sizeof(input->data.text[0]));
        break;
    case INPUT_ARGUMENT:
        save_add_varint(save, input->data.arg.constr);
        save_blockdef(save, input->data.arg.blockdef);
        break;
    default:
        assert(false && "Unimplemented input save");
        break;
    }
}

void save_blockdef(SaveData* save, Blockdef* blockdef) {
    save_add_array(save, blockdef->id, strlen(blockdef->id) + 1, sizeof(blockdef->id[0]));
    save_add(save, blockdef->color);
    save_add_varint(save, blockdef->type);

    int input_count = vector_size(blockdef->inputs);
    save_add_varint(save, input_count);
    for (int i = 0; i < input_count; i++) save_blockdef_input(save, &blockdef->inputs[i]);
}

void save_block_arguments(SaveData* save, Argument* arg) {
    save_add_varint(save, arg->input_id);
    save_add_varint(save, arg->type);

    int string_id;
    switch (arg->type) {
    case ARGUMENT_TEXT:
    case ARGUMENT_CONST_STRING:
        string_id = save_find_id(arg->data.text);
        assert(string_id != -1);
        save_add_varint(save, string_id);
        break;
    case ARGUMENT_BLOCK:
        save_block(save, &arg->data.block);
        break;
    case ARGUMENT_BLOCKDEF:
        string_id = save_find_id(arg->data.blockdef->id);
        assert(string_id != -1);
        save_add_varint(save, string_id);
        break;
    default:
        assert(false && "Unimplemented argument save");
        break;
    }
}

void save_block(SaveData* save, Block* block) {
    assert(block->blockdef->id != NULL);

    int arg_count = vector_size(block->arguments);

    int string_id = save_find_id(block->blockdef->id);
    assert(string_id != -1);
    save_add_varint(save, string_id);
    save_add_varint(save, arg_count);
    for (int i = 0; i < arg_count; i++) save_block_arguments(save, &block->arguments[i]);
}

void save_blockchain(SaveData* save, BlockChain* chain) {
    int blocks_count = vector_size(chain->blocks);

    save_add(save, chain->x);
    save_add(save, chain->y);
    save_add_varint(save, blocks_count);
    for (int i = 0; i < blocks_count; i++) save_block(save, &chain->blocks[i]);
}

void rename_blockdef(Blockdef* blockdef, int id) {
    blockdef_set_id(blockdef, TextFormat("custom%d", id));
    int arg_id = 0;
    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
        blockdef_set_id(blockdef->inputs[i].data.arg.blockdef, TextFormat("custom%d_arg%d", id, arg_id++));
    }
}

int save_find_id(const char* id) {
    for (size_t i = 0; i < vector_size(save_block_ids); i++) {
        if (!strcmp(save_block_ids[i], id)) return i;
    }
    return -1;
}

void save_add_id(const char* id) {
    if (save_find_id(id) != -1) return;
    vector_add(&save_block_ids, id);
}

void block_collect_ids(Block* block) {
    save_add_id(block->blockdef->id);
    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        switch (block->arguments[i].type) {
        case ARGUMENT_TEXT:
        case ARGUMENT_CONST_STRING:
            save_add_id(block->arguments[i].data.text);
            break;
        case ARGUMENT_BLOCK:
            block_collect_ids(&block->arguments[i].data.block);
            break;
        case ARGUMENT_BLOCKDEF:
            save_add_id(block->arguments[i].data.blockdef->id);
            break;
        default:
            assert(false && "Unimplemented argument save id");
            break;
        }
    }
}

void collect_all_code_ids(BlockChain* code) {
    for (size_t i = 0; i < vector_size(code); i++) {
        BlockChain* chain = &code[i];
        for (size_t j = 0; j < vector_size(chain->blocks); j++) {
            block_collect_ids(&chain->blocks[j]);
        }
    }
}

void save_code(const char* file_path, ProjectConfig* config, BlockChain* code) {
    (void) config;
    SaveData save = {0};
    ver = 3;
    int chains_count = vector_size(code);

    Blockdef** blockdefs = vector_create();
    save_block_ids = vector_create();

    int id = 0;
    for (int i = 0; i < chains_count; i++) {
        Block* block = &code[i].blocks[0];
        for (size_t j = 0; j < vector_size(block->arguments); j++) {
            if (block->arguments[j].type != ARGUMENT_BLOCKDEF) continue;
            rename_blockdef(block->arguments[j].data.blockdef, id++);
            vector_add(&blockdefs, block->arguments[j].data.blockdef);
        }
    }

    collect_all_code_ids(code);

    save_add_varint(&save, ver);
    save_add_array(&save, scrap_ident, ARRLEN(scrap_ident), sizeof(scrap_ident[0]));

    save_add_varint(&save, vector_size(save_block_ids));
    for (size_t i = 0; i < vector_size(save_block_ids); i++) {
        save_add_array(&save, save_block_ids[i], strlen(save_block_ids[i]) + 1, sizeof(save_block_ids[i][0]));
    }

    save_add_varint(&save, id);
    for (size_t i = 0; i < vector_size(blockdefs); i++) save_blockdef(&save, blockdefs[i]);

    save_add_varint(&save, chains_count);
    for (int i = 0; i < chains_count; i++) save_blockchain(&save, &code[i]);

    SaveFileData(file_path, save.ptr, save.size);
    TraceLog(LOG_INFO, "%zu bytes written into %s", save.size, file_path);

    vector_free(save_block_ids);
    vector_free(blockdefs);
    free_save(&save);
}

Blockdef* find_blockdef(Blockdef** blockdefs, const char* id) {
    for (size_t i = 0; i < vector_size(blockdefs); i++) {
        if (!strcmp(id, blockdefs[i]->id)) return blockdefs[i];
    }
    return NULL;
}

bool load_blockdef_input(SaveData* save, Input* input) {
    InputType type;
    if (!save_read_varint(save, (unsigned int*)&type)) return false;
    input->type = type;

    unsigned int text_len;
    InputArgumentConstraint constr;
    char* text;

    switch (input->type) {
    case INPUT_TEXT_DISPLAY:
        text = save_read_array(save, sizeof(char), &text_len);
        if (!text) return false;
        if (text[text_len - 1] != 0) return false;

        input->data.text = vector_create();

        for (char* str = text; *str; str++) vector_add(&input->data.text, *str);
        vector_add(&input->data.text, 0);
        break;
    case INPUT_ARGUMENT:
        if (!save_read_varint(save, (unsigned int*)&constr)) return false;

        Blockdef* blockdef = load_blockdef(save);
        if (!blockdef) return false;

        input->data.arg.text = "";
        input->data.arg.hint_text = gettext("any");
        input->data.arg.constr = constr;
        input->data.arg.blockdef = blockdef;
        input->data.arg.blockdef->ref_count++;
        input->data.arg.blockdef->func = block_custom_arg;
        vector_add(&save_blockdefs, input->data.arg.blockdef);
        break;
    default:
        TraceLog(LOG_ERROR, "[LOAD] Unimplemented input load");
        return false;
        break;
    }
    return true;
}

Blockdef* load_blockdef(SaveData* save) {
    unsigned int id_len;
    char* id = save_read_array(save, sizeof(char), &id_len);
    if (!id) return NULL;
    if (id_len == 0) return false;
    if (id[id_len - 1] != 0) return false;

    BlockdefColor* color = save_read_item(save, sizeof(BlockdefColor));
    if (!color) return NULL;

    BlockdefType type;
    if (!save_read_varint(save, (unsigned int*)&type)) return NULL;

    if (ver < 3) {
        // Deprecated: Arg ids are now not needed for blockdefs
        int arg_id;
        if (!save_read_varint(save, (unsigned int*)&arg_id)) return NULL;
    }

    unsigned int input_count;
    if (!save_read_varint(save, &input_count)) return NULL;

    Blockdef* blockdef = malloc(sizeof(Blockdef));
    blockdef->id = strcpy(malloc(id_len * sizeof(char)), id);
    blockdef->color = *color;
    blockdef->type = type;
    blockdef->ref_count = 0;
    blockdef->inputs = vector_create();
    blockdef->func = block_exec_custom;

    for (unsigned int i = 0; i < input_count; i++) {
        Input input;
        if (!load_blockdef_input(save, &input)) {
            blockdef_free(blockdef);
            return NULL;
        }
        vector_add(&blockdef->inputs, input);
    }

    return blockdef;
}

bool load_block_argument(SaveData* save, Argument* arg) {
    unsigned int input_id;
    if (!save_read_varint(save, &input_id)) return false;

    ArgumentType arg_type;
    if (!save_read_varint(save, (unsigned int*)&arg_type)) return false;

    arg->type = arg_type;
    arg->input_id = input_id;

    unsigned int text_id;
    Block block;
    unsigned int blockdef_id;

    switch (arg_type) {
    case ARGUMENT_TEXT:
    case ARGUMENT_CONST_STRING:
        if (!save_read_varint(save, &text_id)) return false;

        arg->data.text = vector_create();
        for (char* str = (char*)save_block_ids[text_id]; *str; str++) vector_add(&arg->data.text, *str);
        vector_add(&arg->data.text, 0);
        break;
    case ARGUMENT_BLOCK:
        if (!load_block(save, &block)) return false;

        arg->data.block = block;
        break;
    case ARGUMENT_BLOCKDEF:
        if (!save_read_varint(save, &blockdef_id)) return false;
        if (blockdef_id >= vector_size(save_block_ids)) {
            TraceLog(LOG_ERROR, "[LOAD] Out of bounds read of save_block_id at %u", blockdef_id);
            return false;
        }

        Blockdef* blockdef = find_blockdef(save_blockdefs, save_block_ids[blockdef_id]);
        if (!blockdef) return false;

        arg->data.blockdef = blockdef;
        arg->data.blockdef->ref_count++;
        break;
    default:
        TraceLog(LOG_ERROR, "[LOAD] Unimplemented argument load");
        return false;
    }
    return true;
}

bool load_block(SaveData* save, Block* block) {
    unsigned int block_id;
    if (!save_read_varint(save, &block_id)) return false;

    bool unknown_blockdef = false;
    Blockdef* blockdef = NULL;
    blockdef = find_blockdef(save_blockdefs, save_block_ids[block_id]);
    if (!blockdef) {
        blockdef = find_blockdef(vm.blockdefs, save_block_ids[block_id]);
        if (!blockdef) {
            TraceLog(LOG_WARNING, "[LOAD] No blockdef matched id: %s", save_block_ids[block_id]);
            unknown_blockdef = true;

            blockdef = blockdef_new(save_block_ids[block_id], BLOCKTYPE_NORMAL, (BlockdefColor) { 0x66, 0x66, 0x66, 0xff }, NULL);
            blockdef_add_text(blockdef, TextFormat(gettext("UNKNOWN %s"), save_block_ids[block_id]));
        }
    }

    unsigned int arg_count;
    if (!save_read_varint(save, &arg_count)) return false;

    block->blockdef = blockdef;
    block->arguments = vector_create();
    block->parent = NULL;
    blockdef->ref_count++;

    for (unsigned int i = 0; i < arg_count; i++) {
        Argument arg;
        if (!load_block_argument(save, &arg)) {
            block_free(block);
            return false;
        }
        vector_add(&block->arguments, arg);
        if (unknown_blockdef) {
            blockdef_add_argument(blockdef, "", "", BLOCKCONSTR_UNLIMITED);
        }
    }

    return true;
}

bool load_blockchain(SaveData* save, BlockChain* chain) {
    int pos_x, pos_y;
    if (ver == 1) {
        struct { float x; float y; }* pos = save_read_item(save, sizeof(struct { float x; float y; }));
        if (!pos) return false;
        pos_x = pos->x;
        pos_y = pos->y;
    } else {
        int* pos = save_read_item(save, sizeof(int));
        if (!pos) return false;
        pos_x = *pos;

        pos = save_read_item(save, sizeof(int));
        if (!pos) return false;
        pos_y = *pos;
    }

    unsigned int blocks_count;
    if (!save_read_varint(save, &blocks_count)) return false;

    *chain = blockchain_new();
    chain->x = pos_x;
    chain->y = pos_y;

    for (unsigned int i = 0; i < blocks_count; i++) {
        Block block;
        if (!load_block(save, &block)) {
            blockchain_free(chain);
            return false;
        }
        blockchain_add_block(chain, block);
        block_update_all_links(&chain->blocks[vector_size(chain->blocks) - 1]);
    }

    return true;
}

BlockChain* load_code(const char* file_path, ProjectConfig* out_config) {
    ProjectConfig config;
    project_config_new(&config);
    project_config_set_default(&config);

    BlockChain* code = vector_create();
    save_blockdefs = vector_create();
    save_block_ids = vector_create();

    int save_size;
    void* file_data = LoadFileData(file_path, &save_size);
    if (!file_data) goto load_fail;
    TraceLog(LOG_INFO, "%zu bytes read from %s", save_size, file_path);

    SaveData save;
    save.ptr = file_data;
    save.size = 0;
    save.capacity = save_size;

    if (!save_read_varint(&save, &ver)) goto load_fail;
    if (ver < 1 || ver > 3) {
        TraceLog(LOG_ERROR, "[LOAD] Unsupported version %d. Current scrap build expects save versions from 1 to 3", ver);
        goto load_fail;
    }

    unsigned int ident_len;
    char* ident = save_read_array(&save, sizeof(char), &ident_len);
    if (!ident) goto load_fail;
    if (ident_len == 0) goto load_fail;

    if (ident[ident_len - 1] != 0 || ident_len != sizeof(scrap_ident) || strncmp(ident, scrap_ident, sizeof(scrap_ident))) {
        TraceLog(LOG_ERROR, "[LOAD] Not valid scrap save");
        goto load_fail;
    }

    unsigned int block_ids_len;
    if (!save_read_varint(&save, &block_ids_len)) goto load_fail;
    for (unsigned int i = 0; i < block_ids_len; i++) {
        unsigned int id_len;
        char* id = save_read_array(&save, sizeof(char), &id_len);
        if (!id) goto load_fail;
        if (id_len == 0) goto load_fail;
        if (id[id_len - 1] != 0) goto load_fail;

        vector_add(&save_block_ids, id);
    }

    unsigned int custom_block_len;
    if (!save_read_varint(&save, &custom_block_len)) goto load_fail;
    for (unsigned int i = 0; i < custom_block_len; i++) {
        Blockdef* blockdef = load_blockdef(&save);
        if (!blockdef) goto load_fail;
        vector_add(&save_blockdefs, blockdef);
    }

    unsigned int code_len;
    if (!save_read_varint(&save, &code_len)) goto load_fail;

    for (unsigned int i = 0; i < code_len; i++) {
        BlockChain chain;
        if (!load_blockchain(&save, &chain)) goto load_fail;
        vector_add(&code, chain);
    }

    unsigned int len;
    char* executable_name = save_read_array(&save, sizeof(char), &len);
    if (executable_name) vector_set_string(&config.executable_name, executable_name);

    char* linker_name = save_read_array(&save, sizeof(char), &len);
    if (linker_name) vector_set_string(&config.linker_name, linker_name);

    *out_config = config;

    UnloadFileData(file_data);
    vector_free(save_block_ids);
    vector_free(save_blockdefs);
    return code;

load_fail:
    if (file_data) UnloadFileData(file_data);
    for (size_t i = 0; i < vector_size(code); i++) blockchain_free(&code[i]);
    project_config_free(&config);
    vector_free(code);
    vector_free(save_block_ids);
    vector_free(save_blockdefs);
    return NULL;
}
