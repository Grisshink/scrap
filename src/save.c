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
#include "vec.h"
#include "blocks.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))

typedef struct {
    void* ptr;
    void* next;
    size_t used_size;
    size_t max_size;
} SaveArena;

char scrap_ident[] = "SCRAP";
const char** save_block_ids = NULL;
ScrBlockdef** save_blockdefs = NULL;

int save_find_id(const char* id);
void save_code(const char* file_path, ScrBlockChain* code);
ScrBlockChain* load_code(const char* file_path);
void save_block(SaveArena* save, ScrBlock* block);
bool load_block(SaveArena* save, ScrBlock* block);
void save_blockdef(SaveArena* save, ScrBlockdef* blockdef);
ScrBlockdef* load_blockdef(SaveArena* save);

void set_default_config(Config* config) {
    config->font_size = 32;
    config->side_bar_size = 300;
    config->fps_limit = 60;
    config->block_size_threshold = 1000;
    strncpy(config->font_symbols, "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNMйцукенгшщзхъфывапролджэячсмитьбюёЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮЁ ,./;'\\[]=-0987654321`~!@#$%^&*()_+{}:\"|<>?", sizeof(config->font_symbols) - 1);
    strncpy(config->font_path, DATA_PATH "nk57-cond.otf", sizeof(config->font_path) - 1);
    strncpy(config->font_bold_path, DATA_PATH "nk57-eb.otf", sizeof(config->font_bold_path) - 1);
    strncpy(config->font_mono_path, DATA_PATH "nk57.otf", sizeof(config->font_mono_path) - 1);
}

void apply_config(Config* dst, Config* src) {
    dst->fps_limit = src->fps_limit; SetTargetFPS(dst->fps_limit);
    dst->block_size_threshold = src->block_size_threshold;
    dst->side_bar_size = src->side_bar_size;
}

void save_config(Config* config) {
    int file_size = 1;
    // ARRLEN also includes \0 into size, but we are using this size to put = sign instead
    file_size += ARRLEN("UI_SIZE") + 10 + 1;
    file_size += ARRLEN("SIDE_BAR_SIZE") + 10 + 1;
    file_size += ARRLEN("FPS_LIMIT") + 10 + 1;
    file_size += ARRLEN("BLOCK_SIZE_THRESHOLD") + 10 + 1;
    file_size += ARRLEN("FONT_SYMBOLS") + strlen(config->font_symbols) + 1;
    file_size += ARRLEN("FONT_PATH") + strlen(config->font_path) + 1;
    file_size += ARRLEN("FONT_BOLD_PATH") + strlen(config->font_bold_path) + 1;
    file_size += ARRLEN("FONT_MONO_PATH") + strlen(config->font_mono_path) + 1;
    
    char* file_str = malloc(sizeof(char) * file_size);
    int cursor = 0;

    cursor += sprintf(file_str + cursor, "UI_SIZE=%u\n", config->font_size);
    cursor += sprintf(file_str + cursor, "SIDE_BAR_SIZE=%u\n", config->side_bar_size);
    cursor += sprintf(file_str + cursor, "FPS_LIMIT=%u\n", config->fps_limit);
    cursor += sprintf(file_str + cursor, "BLOCK_SIZE_THRESHOLD=%u\n", config->block_size_threshold);
    cursor += sprintf(file_str + cursor, "FONT_SYMBOLS=%s\n", config->font_symbols);
    cursor += sprintf(file_str + cursor, "FONT_PATH=%s\n", config->font_path);
    cursor += sprintf(file_str + cursor, "FONT_BOLD_PATH=%s\n", config->font_bold_path);
    cursor += sprintf(file_str + cursor, "FONT_MONO_PATH=%s\n", config->font_mono_path);

    SaveFileText(CONFIG_PATH, file_str);

    free(file_str);
}

void load_config(Config* config) {
    char* file = LoadFileText(CONFIG_PATH);
    if (!file) return;
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

        TraceLog(LOG_INFO, "Field = \"%s\" Value = \"%s\"\n", field, value);
        if (!strcmp(field, "UI_SIZE")) {
            int val = atoi(value);
            config->font_size = val ? val : config->font_size;
        } else if (!strcmp(field, "SIDE_BAR_SIZE")) {
            int val = atoi(value);
            config->side_bar_size = val ? val : config->side_bar_size;
        } else if (!strcmp(field, "FPS_LIMIT")) {
            int val = atoi(value);
            config->fps_limit = val ? val : config->fps_limit;
        } else if (!strcmp(field, "BLOCK_SIZE_THRESHOLD")) {
            int val = atoi(value);
            config->block_size_threshold = val ? val : config->block_size_threshold;
        } else if (!strcmp(field, "FONT_SYMBOLS")) {
            strncpy(config->font_symbols, value, sizeof(config->font_symbols) - 1);
        } else if (!strcmp(field, "FONT_PATH")) {
            strncpy(config->font_path, value, sizeof(config->font_path) - 1);
        } else if (!strcmp(field, "FONT_BOLD_PATH")) {
            strncpy(config->font_bold_path, value, sizeof(config->font_bold_path) - 1);
        } else if (!strcmp(field, "FONT_MONO_PATH")) {
            strncpy(config->font_mono_path, value, sizeof(config->font_mono_path) - 1);
        } else {
            TraceLog(LOG_WARNING, "Unknown key: %s\n", field);
        }
    }

    UnloadFileText(file);
}

SaveArena new_save(size_t size) {
    void* ptr = malloc(size); 
    return (SaveArena) {
        .ptr = ptr,
        .next = ptr,
        .used_size = 0,
        .max_size = size,
    };
}

#define save_add(save, data) save_add_item(save, &data, sizeof(data))

void* save_alloc(SaveArena* save, size_t size) {
    assert(save->ptr != NULL);
    assert(save->next != NULL);
    assert((size_t)((save->next + size) - save->ptr) <= save->max_size);
    void* ptr = save->next;
    save->next += size;
    save->used_size += size;
    return ptr;
}

void* save_read_item(SaveArena* save, size_t data_size) {
    if ((size_t)(save->next + data_size - save->ptr) > save->max_size) {
        TraceLog(LOG_ERROR, "[LOAD] Unexpected EOF reading data\n");
        return NULL;
    }
    void* ptr = save->next;
    save->next += data_size;
    save->used_size += data_size;
    return ptr;
}

bool save_read_varint(SaveArena* save, unsigned int* out) {
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

void* save_read_array(SaveArena* save, size_t data_size, unsigned int* array_len) {
    if (!save_read_varint(save, array_len)) return NULL;
    return save_read_item(save, data_size * *array_len);
}

void save_add_item(SaveArena* save, const void* data, size_t data_size) {
    void* ptr = save_alloc(save, data_size);
    memcpy(ptr, data, data_size);
}

void save_add_varint(SaveArena* save, unsigned int data) {
    unsigned char varint = 0;
    do {
        varint = data & 0x7f;
        data >>= 7;
        varint |= (data == 0) << 7;
        save_add(save, varint);
    } while (data);
}

void save_add_array(SaveArena* save, const void* array, int array_size, size_t data_size) {
    save_add_varint(save, array_size);
    for (int i = 0; i < array_size; i++) save_add_item(save, array + data_size * i, data_size);
}

void free_save(SaveArena* save) {
    free(save->ptr);
    save->ptr = NULL;
    save->next = NULL;
    save->used_size = 0;
    save->max_size = 0;
}

void save_blockdef_input(SaveArena* save, ScrInput* input) {
    save_add_varint(save, input->type);
    switch (input->type) {
    case INPUT_TEXT_DISPLAY:
        save_add_array(save, input->data.stext.text, vector_size(input->data.stext.text), sizeof(input->data.stext.text[0]));
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

void save_blockdef(SaveArena* save, ScrBlockdef* blockdef) {
    save_add_array(save, blockdef->id, strlen(blockdef->id) + 1, sizeof(blockdef->id[0]));
    save_add(save, blockdef->color);
    save_add_varint(save, blockdef->type);
    save_add_varint(save, blockdef->arg_id);

    int input_count = vector_size(blockdef->inputs);
    save_add_varint(save, input_count);
    for (int i = 0; i < input_count; i++) save_blockdef_input(save, &blockdef->inputs[i]);
}

void save_block_arguments(SaveArena* save, ScrArgument* arg) {
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

void save_block(SaveArena* save, ScrBlock* block) {
    assert(block->blockdef->id != NULL);

    int arg_count = vector_size(block->arguments);

    int string_id = save_find_id(block->blockdef->id);
    assert(string_id != -1);
    save_add_varint(save, string_id);
    save_add_varint(save, arg_count);
    for (int i = 0; i < arg_count; i++) save_block_arguments(save, &block->arguments[i]);
}

void save_blockchain(SaveArena* save, ScrBlockChain* chain) {
    int blocks_count = vector_size(chain->blocks);

    save_add(save, chain->pos);
    save_add_varint(save, blocks_count);
    for (int i = 0; i < blocks_count; i++) save_block(save, &chain->blocks[i]);
}

void rename_blockdef(ScrBlockdef* blockdef, int id) {
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

void block_collect_ids(ScrBlock* block) {
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

void collect_all_code_ids(ScrBlockChain* code) {
    for (size_t i = 0; i < vector_size(code); i++) {
        ScrBlockChain* chain = &code[i];
        for (size_t j = 0; j < vector_size(chain->blocks); j++) {
            block_collect_ids(&chain->blocks[j]);
        }
    }
}

void save_code(const char* file_path, ScrBlockChain* code) {
    SaveArena save = new_save(32768);
    int save_ver = 1;
    int chains_count = vector_size(code);

    ScrBlockdef** blockdefs = vector_create();
    save_block_ids = vector_create();

    int id = 0;
    for (int i = 0; i < chains_count; i++) {
        ScrBlock* block = &code[i].blocks[0];
        for (size_t j = 0; j < vector_size(block->arguments); j++) {
            if (block->arguments[j].type != ARGUMENT_BLOCKDEF) continue;
            rename_blockdef(block->arguments[j].data.blockdef, id++);
            vector_add(&blockdefs, block->arguments[j].data.blockdef);
        }
    }

    collect_all_code_ids(code);

    save_add_varint(&save, save_ver);
    save_add_array(&save, scrap_ident, ARRLEN(scrap_ident), sizeof(scrap_ident[0]));

    save_add_varint(&save, vector_size(save_block_ids));
    for (size_t i = 0; i < vector_size(save_block_ids); i++) {
        save_add_array(&save, save_block_ids[i], strlen(save_block_ids[i]) + 1, sizeof(save_block_ids[i][0]));
    }

    save_add_varint(&save, id);
    for (size_t i = 0; i < vector_size(blockdefs); i++) save_blockdef(&save, blockdefs[i]);

    save_add_varint(&save, chains_count);
    for (int i = 0; i < chains_count; i++) save_blockchain(&save, &code[i]);

    SaveFileData(file_path, save.ptr, save.used_size);

    vector_free(save_block_ids);
    vector_free(blockdefs);
    free_save(&save);
}

ScrBlockdef* find_blockdef(ScrBlockdef** blockdefs, const char* id) {
    for (size_t i = 0; i < vector_size(blockdefs); i++) {
        if (!strcmp(id, blockdefs[i]->id)) return blockdefs[i];
    }
    return NULL;
}

bool load_blockdef_input(SaveArena* save, ScrInput* input) {
    ScrInputType type;
    if (!save_read_varint(save, (unsigned int*)&type)) return false;
    input->type = type;

    unsigned int text_len;
    ScrInputArgumentConstraint constr; 
    char* text;

    switch (input->type) {
    case INPUT_TEXT_DISPLAY:
        text = save_read_array(save, sizeof(char), &text_len);
        if (!text) return false;
        if (text[text_len - 1] != 0) return false;

        input->data.stext.text = vector_create();
        input->data.stext.ms = (ScrMeasurement) {0};
        input->data.stext.editor_ms = (ScrMeasurement) {0};

        for (char* str = text; *str; str++) vector_add(&input->data.stext.text, *str);
        vector_add(&input->data.stext.text, 0);
        break;
    case INPUT_ARGUMENT:
        if (!save_read_varint(save, (unsigned int*)&constr)) return false;

        ScrBlockdef* blockdef = load_blockdef(save);
        if (!blockdef) return false;

        input->data.arg.text = "";
        input->data.arg.constr = constr;
        input->data.arg.ms = (ScrMeasurement) {0};
        input->data.arg.blockdef = blockdef;
        input->data.arg.blockdef->ref_count++;
        input->data.arg.blockdef->func = block_custom_arg;
        vector_add(&save_blockdefs, input->data.arg.blockdef);
        break;
    default:
        TraceLog(LOG_ERROR, "[LOAD] Unimplemented input load\n");
        return false;
        break;
    }
    return true;
}

ScrBlockdef* load_blockdef(SaveArena* save) {
    unsigned int id_len;
    char* id = save_read_array(save, sizeof(char), &id_len);
    if (!id) return NULL;
    if (id_len == 0) return false;
    if (id[id_len - 1] != 0) return false;

    ScrColor* color = save_read_item(save, sizeof(ScrColor));
    if (!color) return NULL;

    ScrBlockdefType type;
    if (!save_read_varint(save, (unsigned int*)&type)) return NULL;

    int arg_id;
    if (!save_read_varint(save, (unsigned int*)&arg_id)) return NULL;

    unsigned int input_count;
    if (!save_read_varint(save, &input_count)) return NULL;

    ScrBlockdef* blockdef = malloc(sizeof(ScrBlockdef));
    blockdef->id = strcpy(malloc(id_len * sizeof(char)), id);
    blockdef->color = *color;
    blockdef->type = type;
    blockdef->ms = (ScrMeasurement) {0};
    blockdef->hidden = false;
    blockdef->ref_count = 0;
    blockdef->inputs = vector_create();
    blockdef->func = block_exec_custom;
    blockdef->chain = NULL;
    blockdef->arg_id = arg_id;

    for (unsigned int i = 0; i < input_count; i++) {
        ScrInput input;
        if (!load_blockdef_input(save, &input)) {
            blockdef_free(blockdef);
            return NULL;
        }
        vector_add(&blockdef->inputs, input);
    }

    return blockdef;
}

bool load_block_argument(SaveArena* save, ScrArgument* arg) {
    unsigned int input_id;
    if (!save_read_varint(save, &input_id)) return false;

    ScrArgumentType arg_type;
    if (!save_read_varint(save, (unsigned int*)&arg_type)) return false;

    arg->type = arg_type;
    arg->input_id = input_id;

    unsigned int text_id;
    ScrBlock block;
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
            TraceLog(LOG_ERROR, "[LOAD] Out of bounds read of save_block_id at %u\n", blockdef_id);
            return false;
        }

        ScrBlockdef* blockdef = find_blockdef(save_blockdefs, save_block_ids[blockdef_id]);
        if (!blockdef) return false;

        arg->data.blockdef = blockdef;
        arg->data.blockdef->ref_count++;
        break;
    default:
        TraceLog(LOG_ERROR, "[LOAD] Unimplemented argument load\n");
        return false;
    }
    return true;
}

bool load_block(SaveArena* save, ScrBlock* block) {
    unsigned int block_id;
    if (!save_read_varint(save, &block_id)) return false;

    ScrBlockdef* blockdef = NULL;
    blockdef = find_blockdef(save_blockdefs, save_block_ids[block_id]);
    if (!blockdef) {
        blockdef = find_blockdef(vm.blockdefs, save_block_ids[block_id]);
        if (!blockdef) {
            TraceLog(LOG_ERROR, "[LOAD] No blockdef matched id: %s\n", save_block_ids[block_id]);
            return false;
        }
    }

    unsigned int arg_count;
    if (!save_read_varint(save, &arg_count)) return false;

    block->blockdef = blockdef;
    block->arguments = vector_create();
    block->ms = (ScrMeasurement) {0};
    block->parent = NULL;
    blockdef->ref_count++;

    for (unsigned int i = 0; i < arg_count; i++) {
        ScrArgument arg;
        if (!load_block_argument(save, &arg)) {
            block_free(block);
            return false;
        }
        vector_add(&block->arguments, arg);
    }

    update_measurements(block, PLACEMENT_HORIZONTAL);
    return true;
}

bool load_blockchain(SaveArena* save, ScrBlockChain* chain) {
    ScrVec* pos = save_read_item(save, sizeof(ScrVec));
    if (!pos) return false;

    unsigned int blocks_count;
    if (!save_read_varint(save, &blocks_count)) return false;

    *chain = blockchain_new();
    chain->pos = *pos;

    for (unsigned int i = 0; i < blocks_count; i++) {
        ScrBlock block;
        if (!load_block(save, &block)) {
            blockchain_free(chain);
            return false;
        }
        blockchain_add_block(chain, block);
        block_update_all_links(&chain->blocks[vector_size(chain->blocks) - 1]);
    }

    return true;
}

ScrBlockChain* load_code(const char* file_path) {
    ScrBlockChain* code = vector_create();
    save_blockdefs = vector_create();
    save_block_ids = vector_create();

    int save_size;
    void* file_data = LoadFileData(file_path, &save_size);
    if (!file_data) goto load_fail;

    SaveArena save;
    save.ptr = file_data;
    save.next = file_data;
    save.max_size = save_size;
    save.used_size = 0;

    unsigned int ver;
    if (!save_read_varint(&save, &ver)) goto load_fail;
    if (ver != 1) {
        TraceLog(LOG_ERROR, "[LOAD] Unsupported version %d. Current scrap build expects save version 1\n", ver);
        goto load_fail;
    }

    unsigned int ident_len;
    char* ident = save_read_array(&save, sizeof(char), &ident_len);
    if (!ident) goto load_fail;
    if (ident_len == 0) goto load_fail;

    if (ident[ident_len - 1] != 0 || ident_len != sizeof(scrap_ident) || strncmp(ident, scrap_ident, sizeof(scrap_ident))) {
        TraceLog(LOG_ERROR, "[LOAD] Not valid scrap save\n");
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
        ScrBlockdef* blockdef = load_blockdef(&save);
        if (!blockdef) goto load_fail;
        vector_add(&save_blockdefs, blockdef);
    }

    unsigned int code_len;
    if (!save_read_varint(&save, &code_len)) goto load_fail;

    for (unsigned int i = 0; i < code_len; i++) {
        ScrBlockChain chain;
        if (!load_blockchain(&save, &chain)) goto load_fail;
        vector_add(&code, chain);
    }
    UnloadFileData(file_data);
    vector_free(save_block_ids);
    vector_free(save_blockdefs);
    return code;

load_fail:
    if (file_data) UnloadFileData(file_data);
    for (size_t i = 0; i < vector_size(code); i++) blockchain_free(&code[i]);
    vector_free(code);
    vector_free(save_block_ids);
    vector_free(save_blockdefs);
    return NULL;
}
