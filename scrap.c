// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024 Grisshink
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

#define LICENSE_URL "https://www.gnu.org/licenses/gpl-3.0.html"

#define SCRVM_IMPLEMENTATION
#define SCRVM_VEC_C
#include "vm.h"

#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <semaphore.h>
#include <unistd.h>
#include "raylib.h"
#define RAYLIB_NUKLEAR_IMPLEMENTATION
#include "external/raylib-nuklear.h"
#include "external/tinyfiledialogs.h"

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MOD(x, y) (((x) % (y) + (y)) % (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))
#define LERP(min, max, t) (((max) - (min)) * (t) + (min))
#define UNLERP(min, max, v) (((float)(v) - (float)(min)) / ((float)(max) - (float)(min)))

#define TERM_INPUT_BUF_SIZE 256
#define ACTION_BAR_MAX_SIZE 128
#define FONT_PATH_MAX_SIZE 256
#define FONT_SYMBOLS_MAX_SIZE 1024
#define CONFIG_PATH "config.txt"
#define DATA_PATH "data/"

#define BLOCK_TEXT_SIZE (conf.font_size * 0.6)
#define BLOCK_PADDING (5.0 * (float)conf.font_size / 32.0)
#define BLOCK_OUTLINE_SIZE (2.0 * (float)conf.font_size / 32.0)
#define BLOCK_IMAGE_SIZE (conf.font_size - BLOCK_OUTLINE_SIZE * 4)
#define BLOCK_STRING_PADDING (10.0 * (float)conf.font_size / 32.0)
#define BLOCK_CONTROL_INDENT (16.0 * (float)conf.font_size / 32.0)
#define SIDE_BAR_PADDING (10.0 * (float)conf.font_size / 32.0)
#define DROP_TEX_WIDTH ((float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)drop_tex.height * (float)drop_tex.width)
#define TERM_CHAR_SIZE (conf.font_size * 0.6)

typedef struct {
    int font_size;
    int side_bar_size;
    int fps_limit;
    int block_size_threshold;
    char font_symbols[FONT_SYMBOLS_MAX_SIZE];
    char font_path[FONT_PATH_MAX_SIZE];
    char font_bold_path[FONT_PATH_MAX_SIZE];
    char font_mono_path[FONT_PATH_MAX_SIZE];
} Config;

typedef enum {
    TOPBAR_TOP,
    TOPBAR_TABS,
    TOPBAR_RUN_BUTTON,
} TopBarType;

typedef struct {
    TopBarType type;
    Vector2 pos;
    int ind;
} TopBars;

typedef enum {
    EDITOR_NONE,
    EDITOR_BLOCKDEF,
    EDITOR_EDIT,
    EDITOR_ADD_ARG,
    EDITOR_DEL_ARG,
    EDITOR_ADD_TEXT,
} EditorHoverPart;

typedef struct {
    EditorHoverPart part;
    ScrBlockdef* edit_blockdef;
    ScrBlock* edit_block;
    ScrBlockdef* blockdef;
    size_t blockdef_input;
} EditorHoverInfo;

typedef struct {
    bool sidebar;
    bool drag_cancelled;

    ScrBlockChain* blockchain;
    vec_size_t blockchain_index;
    int blockchain_layer;

    ScrBlock* block;
    ScrArgument* argument;
    Vector2 argument_pos;
    ScrArgument* prev_argument;

    ScrBlock* select_block;
    ScrArgument* select_argument;
    Vector2 select_argument_pos;

    char** input;
    char** select_input;

    Vector2 last_mouse_pos;
    Vector2 mouse_click_pos;
    float time_at_last_pos;

    int dropdown_hover_ind;

    ScrBlockChain* exec_chain;
    size_t exec_ind;

    TopBars top_bars;
    EditorHoverInfo editor;
} HoverInfo;

typedef struct {
    ScrMeasurement ms;
    int scroll_amount;
} Dropdown;

typedef struct {
    float show_time;
    char text[ACTION_BAR_MAX_SIZE];
} ActionBar;

typedef enum {
    GUI_TYPE_SETTINGS,
    GUI_TYPE_ABOUT,
    GUI_TYPE_FILE,
} NuklearGuiType;

typedef struct {
    bool shown;
    float animation_time;
    bool is_fading;
    bool is_hiding;
    Vector2 pos;
    NuklearGuiType type;
    struct nk_context *ctx;
} NuklearGui;

typedef struct {
    Vector2 min_pos;
    Vector2 max_pos;
} BlockCode;

typedef struct {
    int scroll_amount;
    int max_y;
    ScrBlock* blocks;
} Sidebar;

typedef enum {
    TAB_CODE,
    TAB_OUTPUT,
} TabType;

typedef struct {
    ScrVec pos;
    ScrBlock* block;
} DrawStack;

typedef struct {
    pthread_mutex_t lock;
    Rectangle size;
    int char_w, char_h;
    int cursor_pos;
    Vector2 char_size;
    char (*buffer)[5];

    sem_t input_sem;
    char input_buf[TERM_INPUT_BUF_SIZE];
    int buf_start;
    int buf_end;
} OutputWindow;

typedef struct {
    void* ptr;
    void* next;
    size_t used_size;
    size_t max_size;
} SaveArena;

char* top_bar_buttons_text[] = {
    "File",
    "Settings",
    "About",
};

char* tab_bar_buttons_text[] = {
    "Code",
    "Output",
};

char scrap_ident[] = "SCRAP";
ScrBlockdef** save_blockdefs = NULL;
const char** save_block_ids = NULL;

Config conf;
Config gui_conf;

Sound click_snd;

Texture2D run_tex;
Texture2D stop_tex;
Texture2D drop_tex;
Texture2D close_tex;
Texture2D logo_tex;
Texture2D warn_tex;
Texture2D edit_tex;
Texture2D close_tex;
Texture2D term_tex;
Texture2D add_arg_tex;
Texture2D del_arg_tex;
Texture2D add_text_tex;
Texture2D special_tex;
Texture2D list_tex;
struct nk_image logo_tex_nuc;
struct nk_image warn_tex_nuc;

Font font_cond;
Font font_eb;
Font font_mono;
struct nk_user_font* font_eb_nuc = NULL;
struct nk_user_font* font_cond_nuc = NULL;

Shader line_shader;
float shader_time = 0.0;
int shader_time_loc;

TabType current_tab = TAB_CODE;

ScrVm vm;
ScrExec exec = {0};
ScrBlockChain mouse_blockchain = {0};
ScrBlockChain* editor_code = {0};

DrawStack* draw_stack = NULL;
HoverInfo hover_info = {0};
Sidebar sidebar = {0};
BlockCode block_code = {0};
Dropdown dropdown = {0};
ActionBar actionbar;
NuklearGui gui = {0};
OutputWindow out_win = {0};

Vector2 camera_pos = {0};
Vector2 camera_click_pos = {0};
int blockchain_select_counter = -1;

const char* line_shader_vertex =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec4 vertexColor;\n"
    "out vec2 fragCoord;\n"
    "out vec4 fragColor;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "    vec4 pos = mvp * vec4(vertexPosition, 1.0);\n"
    "    fragCoord = pos.xy;\n"
    "    fragColor = vertexColor;\n"
    "    gl_Position = pos;\n"
    "}";

const char* line_shader_fragment =
    "#version 330\n"
    "in vec2 fragCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform float time = 0.0;\n"
    "void main() {\n"
    "    vec2 coord = (fragCoord + 1.0) * 0.5;\n"
    "    coord.y = 1.0 - coord.y;\n"
    "    float pos = time * 4.0 - 1.0;\n"
    "    float diff = clamp(1.0 - abs(coord.x + coord.y - pos), 0.0, 1.0);\n"
    "    finalColor = vec4(fragColor.xyz, pow(diff, 2.0));\n"
    "}";

#define MATH_LIST_LEN 10
char* block_math_list[MATH_LIST_LEN] = {
    "sqrt", "round", "floor", "ceil",
    "sin", "cos", "tan",
    "asin", "acos", "atan",
};

void save_config(Config* config);
void apply_config(Config* dst, Config* src);
void set_default_config(Config* config);
void update_measurements(ScrBlock* block, ScrPlacementStrategy placement);
void term_input_put_char(char ch);
int term_print_str(const char* str);
void term_clear(void);
void save_code(const char* file_path, ScrBlockChain* code);
ScrBlockChain* load_code(const char* file_path);
ScrData block_exec_custom(ScrExec* exec, int argc, ScrData* argv);
ScrData block_custom_arg(ScrExec* exec, int argc, ScrData* argv);
void save_block(SaveArena* save, ScrBlock* block);
bool load_block(SaveArena* save, ScrBlock* block);
void save_blockdef(SaveArena* save, ScrBlockdef* blockdef);
ScrBlockdef* load_blockdef(SaveArena* save);
int save_find_id(const char* id);

char** math_list_access(ScrBlock* block, size_t* list_len) {
    (void) block;
    *list_len = MATH_LIST_LEN;
    return block_math_list;
}

ScrVec as_scr_vec(Vector2 vec) {
    return (ScrVec) { vec.x, vec.y };
}

Vector2 as_rl_vec(ScrVec vec) {
    return (Vector2) { vec.x, vec.y };
}

Color as_rl_color(ScrColor color) {
    return (Color) { color.r, color.g, color.b, color.a };
}

void actionbar_show(const char* text) {
    printf("[ACTION] %s\n", text);
    strncpy(actionbar.text, text, sizeof(actionbar.text) - 1);
    actionbar.show_time = 3.0;
}

void blockcode_update_measurments(BlockCode* blockcode) {
    blockcode->max_pos = (Vector2) { -1.0 / 0.0, -1.0 / 0.0 };
    blockcode->min_pos = (Vector2) { 1.0 / 0.0, 1.0 / 0.0 };

    for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
        blockcode->max_pos.x = MAX(blockcode->max_pos.x, editor_code[i].pos.x);
        blockcode->max_pos.y = MAX(blockcode->max_pos.y, editor_code[i].pos.y);
        blockcode->min_pos.x = MIN(blockcode->min_pos.x, editor_code[i].pos.x);
        blockcode->min_pos.y = MIN(blockcode->min_pos.y, editor_code[i].pos.y);
    }
}

void blockcode_add_blockchain(BlockCode* blockcode, ScrBlockChain chain) {
    vector_add(&editor_code, chain);
    blockcode_update_measurments(blockcode);
}

void blockcode_remove_blockchain(BlockCode* blockcode, size_t ind) {
    vector_remove(editor_code, ind);
    blockcode_update_measurments(blockcode);
}

ScrBlock block_new_ms(ScrBlockdef* blockdef) {
    ScrBlock block = block_new(blockdef);
    for (size_t i = 0; i < vector_size(block.arguments); i++) {
        if (block.arguments[i].type != ARGUMENT_BLOCKDEF) continue;
        block.arguments[i].data.blockdef->func = block_exec_custom;
    }
    update_measurements(&block, PLACEMENT_HORIZONTAL);
    return block;
}

void draw_text_shadow(Font font, const char *text, Vector2 position, float font_size, float spacing, Color tint, Color shadow) {
    DrawTextEx(font, text, (Vector2) { position.x + 1, position.y + 1 }, font_size, spacing, shadow);
    DrawTextEx(font, text, position, font_size, spacing, tint);
}

ScrMeasurement measure_text(char* text) {
    ScrMeasurement ms = {0};
    ms.size = as_scr_vec(MeasureTextEx(font_cond, text, BLOCK_TEXT_SIZE, 0.0));
    ms.placement = PLACEMENT_HORIZONTAL;
    return ms;
}

ScrMeasurement measure_image(ScrImage image, float size) {
    Texture2D* texture = image.image_ptr;
    ScrMeasurement ms = {0};
    ms.size.x = size / (float)texture->height * (float)texture->width;
    ms.size.y = size;
    ms.placement = PLACEMENT_HORIZONTAL;
    return ms;
}

ScrMeasurement measure_input_box(const char* input) {
    ScrMeasurement ms;
    ms.size = as_scr_vec(MeasureTextEx(font_cond, input, BLOCK_TEXT_SIZE, 0.0));
    ms.size.x += BLOCK_STRING_PADDING;
    ms.size.x = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, ms.size.x);
    ms.size.y = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, ms.size.y);
    ms.placement = PLACEMENT_HORIZONTAL;
    return ms;
}

ScrMeasurement measure_block_button(void) {
    ScrMeasurement ms;
    ms.size.x = conf.font_size;
    ms.size.y = conf.font_size;
    ms.placement = PLACEMENT_HORIZONTAL;
    return ms;
}

ScrMeasurement measure_group(ScrMeasurement left, ScrMeasurement right, float padding) {
    ScrMeasurement ms = left;
    ms.size.x += right.size.x + padding;
    ms.size.y = MAX(left.size.y, right.size.y);
    return ms;
}

void draw_image(Vector2 position, ScrImage image, float size) {
    Texture2D* img = image.image_ptr;
    DrawTextureEx(*img, (Vector2) { position.x + 1, position.y + 1 }, 0.0, size / (float)img->height, (Color) { 0x00, 0x00, 0x00, 0x88 });
    DrawTextureEx(*img, position, 0.0, size / (float)img->height, WHITE);
}

bool block_button_update_collisions(Vector2 position, EditorHoverPart part) {
    Rectangle rect;
    rect.x = position.x + conf.font_size * 0.1;
    rect.y = position.y + conf.font_size * 0.1;
    rect.width = conf.font_size * 0.8;
    rect.height = conf.font_size * 0.8;

    if (CheckCollisionPointRec(GetMousePosition(), rect)) {
        hover_info.editor.part = part;
        return true;
    }

    return false;
}

void draw_block_button(Vector2 position, Texture2D image, bool hovered) {
    Rectangle rect;
    rect.x = position.x + conf.font_size * 0.1;
    rect.y = position.y + conf.font_size * 0.1;
    rect.width = conf.font_size * 0.8;
    rect.height = conf.font_size * 0.8;
    DrawRectangleRec(rect, (Color) { 0xff, 0xff, 0xff, hovered ? 0x80 : 0x40 });
    DrawTextureEx(image, (Vector2) { rect.x, rect.y }, 0.0, 0.8, WHITE);
}

void draw_input_box(Vector2 position, ScrMeasurement ms, char** input, bool rounded) {
    Rectangle rect;
    rect.x = position.x;
    rect.y = position.y;
    rect.width = ms.size.x;
    rect.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

    bool hovered = input == hover_info.input;
    bool selected = input == hover_info.select_input;
    Color hovered_color = (Color) { 0x80, 0x80, 0x80, 0xff };
    Color selected_color = (Color) { 0x00, 0x00, 0x00, 0xff };

    if (rounded) {
        DrawRectangleRounded(rect, 0.5, 5, WHITE);
        if (hovered || selected) {
            DrawRectangleRoundedLines(rect, 0.5, 5, BLOCK_OUTLINE_SIZE, selected ? selected_color : hovered_color);
        }
    } else {
        DrawRectangleRec(rect, WHITE);
        if (hovered || selected) {
            DrawRectangleLinesEx(rect, BLOCK_OUTLINE_SIZE, selected ? selected_color : hovered_color);
        }
    }

    position.x += rect.width * 0.5 - MeasureTextEx(font_cond, *input, BLOCK_TEXT_SIZE, 0.0).x * 0.5;
    position.y += rect.height * 0.5 - BLOCK_TEXT_SIZE * 0.5;
    DrawTextEx(font_cond, *input, position, BLOCK_TEXT_SIZE, 0.0, BLACK);
}

void draw_block_base(Rectangle block_size, ScrBlockdef* blockdef, Color block_color, Color outline_color) {
    if (blockdef->type == BLOCKTYPE_HAT) {
        DrawRectangle(block_size.x, block_size.y, block_size.width - conf.font_size / 4.0, block_size.height, block_color);
        DrawRectangle(block_size.x, block_size.y + conf.font_size / 4.0, block_size.width, block_size.height - conf.font_size / 4.0, block_color);
        DrawTriangle(
            (Vector2) { block_size.x + block_size.width - conf.font_size / 4.0 - 1, block_size.y }, 
            (Vector2) { block_size.x + block_size.width - conf.font_size / 4.0 - 1, block_size.y + conf.font_size / 4.0 }, 
            (Vector2) { block_size.x + block_size.width, block_size.y + conf.font_size / 4.0 }, 
            block_color
        );
    } else {
        DrawRectangleRec(block_size, block_color);
    }

    if (blockdef->type == BLOCKTYPE_HAT) {
        DrawRectangle(block_size.x, block_size.y, block_size.width - conf.font_size / 4.0, BLOCK_OUTLINE_SIZE, outline_color);
        DrawRectangle(block_size.x, block_size.y, BLOCK_OUTLINE_SIZE, block_size.height, outline_color);
        DrawRectangle(block_size.x, block_size.y + block_size.height - BLOCK_OUTLINE_SIZE, block_size.width, BLOCK_OUTLINE_SIZE, outline_color);
        DrawRectangle(block_size.x + block_size.width - BLOCK_OUTLINE_SIZE, block_size.y + conf.font_size / 4.0, BLOCK_OUTLINE_SIZE, block_size.height - conf.font_size / 4.0, outline_color);
        DrawRectanglePro((Rectangle) {
            block_size.x + block_size.width - conf.font_size / 4.0,
            block_size.y,
            sqrtf((conf.font_size / 4.0 * conf.font_size / 4.0) * 2),
            BLOCK_OUTLINE_SIZE,
        }, (Vector2) {0}, 45.0, outline_color);
    } else {
        DrawRectangleLinesEx(block_size, BLOCK_OUTLINE_SIZE, outline_color);
    }
}

void blockdef_update_measurements(ScrBlockdef* blockdef, bool editing) {
    blockdef->ms.size.x = BLOCK_PADDING;
    blockdef->ms.placement = PLACEMENT_HORIZONTAL;
    blockdef->ms.size.y = conf.font_size;

    for (vec_size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        ScrMeasurement ms;
        ms.placement = PLACEMENT_HORIZONTAL;

        switch (blockdef->inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            if (editing) {
                ms = measure_input_box(blockdef->inputs[i].data.stext.text);
                ms = measure_group(ms, measure_block_button(), BLOCK_PADDING);
            } else {
                ms = measure_text(blockdef->inputs[i].data.stext.text);
            }
            blockdef->inputs[i].data.stext.editor_ms = ms;
            break;
        case INPUT_IMAGE_DISPLAY:
            ms = measure_image(blockdef->inputs[i].data.simage.image, BLOCK_IMAGE_SIZE);
            blockdef->inputs[i].data.simage.ms = ms;
            break;
        case INPUT_ARGUMENT:
            blockdef_update_measurements(blockdef->inputs[i].data.arg.blockdef, editing);
            ms = blockdef->inputs[i].data.arg.blockdef->ms;
            break;
        case INPUT_DROPDOWN:
            ms.size = as_scr_vec(MeasureTextEx(font_cond, "Dropdown", BLOCK_TEXT_SIZE, 0.0));
            break;
        case INPUT_BLOCKDEF_EDITOR:
            assert(false && "Unimplemented");
            break;
        default:
            ms.size = as_scr_vec(MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0));
            break;
        }
        ms.size.x += BLOCK_PADDING;

        blockdef->ms.size.x += ms.size.x;
        blockdef->ms.size.y = MAX(blockdef->ms.size.y, ms.size.y + BLOCK_OUTLINE_SIZE * 4);
    }
}

void blockdef_update_collisions(Vector2 position, ScrBlockdef* blockdef, bool editing) {
    Rectangle block_size;
    block_size.x = position.x;
    block_size.y = position.y;
    block_size.width = blockdef->ms.size.x;
    block_size.height = blockdef->ms.size.y;

    hover_info.editor.blockdef = blockdef;

    Vector2 cursor = position;
    cursor.x += BLOCK_PADDING;

    for (vec_size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (hover_info.input || hover_info.editor.part != EDITOR_BLOCKDEF) return;
        int width = 0;
        ScrInput* cur = &blockdef->inputs[i];
        Rectangle arg_size;

        switch (cur->type) {
        case INPUT_TEXT_DISPLAY:
            ScrMeasurement ms = cur->data.stext.editor_ms;
            if (editing) {
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5;
                arg_size.width = ms.size.x - conf.font_size - BLOCK_PADDING;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.input = &cur->data.stext.text;
                    break;
                }

                Vector2 arg_pos = (Vector2) { cursor.x + arg_size.width, cursor.y + block_size.height * 0.5 - conf.font_size * 0.5 };
                if (block_button_update_collisions(arg_pos, EDITOR_DEL_ARG)) {
                    hover_info.editor.blockdef_input = i;
                    break;
                }
            }
            width = ms.size.x;
            break;
        case INPUT_IMAGE_DISPLAY:
            width = cur->data.simage.ms.size.x;
            break;
        case INPUT_ARGUMENT:
            width = cur->data.arg.blockdef->ms.size.x;

            Rectangle blockdef_rect;
            blockdef_rect.x = cursor.x;
            blockdef_rect.y = cursor.y + block_size.height * 0.5 - cur->data.arg.blockdef->ms.size.y * 0.5;
            blockdef_rect.width = cur->data.arg.blockdef->ms.size.x;
            blockdef_rect.height = cur->data.arg.blockdef->ms.size.y;
            if (CheckCollisionPointRec(GetMousePosition(), blockdef_rect)) {
                blockdef_update_collisions((Vector2) { blockdef_rect.x, blockdef_rect.y }, cur->data.arg.blockdef, editing);
                hover_info.editor.blockdef_input = i;
            }
            break;
        default:
            Vector2 size = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0);
            width = size.x;
            break;
        }

        cursor.x += width + BLOCK_PADDING;
    }
}

void draw_blockdef(Vector2 position, ScrBlockdef* blockdef, bool editing) {
    bool collision = hover_info.editor.blockdef == blockdef;

    Color color = as_rl_color(blockdef->color);
    Color block_color = ColorBrightness(color, collision ? 0.3 : 0.0);
    Color outline_color = ColorBrightness(color, collision ? 0.5 : -0.2);

    Vector2 cursor = position;

    Rectangle block_size;
    block_size.x = position.x;
    block_size.y = position.y;
    block_size.width = blockdef->ms.size.x;
    block_size.height = blockdef->ms.size.y;

    if (!CheckCollisionRecs(block_size, (Rectangle) { 0, 0, GetScreenWidth(), GetScreenHeight() })) return;

    draw_block_base(block_size, blockdef, block_color, outline_color);
    //DrawTextEx(font_cond, TextFormat("%zu", blockdef->ref_count), (Vector2) { block_size.x, block_size.y - conf.font_size }, BLOCK_TEXT_SIZE, 0.0, WHITE);

    cursor.x += BLOCK_PADDING;

    for (vec_size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        int width = 0;
        ScrInput* cur = &blockdef->inputs[i];
        Vector2 arg_pos = cursor;

        switch (cur->type) {
        case INPUT_TEXT_DISPLAY:
            width = cur->data.stext.editor_ms.size.x;
            arg_pos.y += block_size.height * 0.5 - cur->data.stext.editor_ms.size.y * 0.5;

            if (editing) {
                ScrMeasurement input_ms = cur->data.stext.editor_ms;
                input_ms.size.x -= conf.font_size + BLOCK_PADDING;
                input_ms.size.y = conf.font_size - BLOCK_OUTLINE_SIZE * 4;
                draw_input_box((Vector2) { arg_pos.x, cursor.y + block_size.height * 0.5 - input_ms.size.y * 0.5}, input_ms, &cur->data.stext.text, false);
                arg_pos.x += input_ms.size.x + BLOCK_PADDING * 0.5;
                draw_block_button(arg_pos, del_arg_tex, hover_info.editor.part == EDITOR_DEL_ARG && hover_info.editor.blockdef == blockdef);
            } else {
                draw_text_shadow(font_cond, cur->data.stext.text, arg_pos, BLOCK_TEXT_SIZE, 0.0, WHITE, (Color) { 0x00, 0x00, 0x00, 0x88 });
            }
            break;
        case INPUT_IMAGE_DISPLAY:
            width = cur->data.simage.ms.size.x;
            arg_pos.y += block_size.height * 0.5 - cur->data.simage.ms.size.y * 0.5;

            draw_image(arg_pos, cur->data.simage.image, BLOCK_IMAGE_SIZE);
            break;
        case INPUT_ARGUMENT:
            width = cur->data.arg.blockdef->ms.size.x;
            arg_pos.y += block_size.height * 0.5 - cur->data.arg.blockdef->ms.size.y * 0.5;

            draw_blockdef(arg_pos, cur->data.arg.blockdef, editing);
            break;
        case INPUT_BLOCKDEF_EDITOR:
            assert(false && "Unimplemented");
            break;
        default:
            Vector2 size = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0);
            width = size.x;
            arg_pos.y += block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5;

            DrawTextEx(font_cond, "NODEF", arg_pos, BLOCK_TEXT_SIZE, 0.0, RED);
            break;
        }

        cursor.x += width + BLOCK_PADDING;
    }
}

void update_measurements(ScrBlock* block, ScrPlacementStrategy placement) {
    block->ms.size.x = BLOCK_PADDING;
    block->ms.placement = placement;
    block->ms.size.y = placement == PLACEMENT_HORIZONTAL ? conf.font_size : BLOCK_OUTLINE_SIZE * 2;

    int arg_id = 0;
    for (vec_size_t i = 0; i < vector_size(block->blockdef->inputs); i++) {
        ScrMeasurement ms;

        switch (block->blockdef->inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            ms = measure_text(block->blockdef->inputs[i].data.stext.text);
            block->blockdef->inputs[i].data.stext.ms = ms;
            break;
        case INPUT_IMAGE_DISPLAY:
            ms = measure_image(block->blockdef->inputs[i].data.simage.image, BLOCK_IMAGE_SIZE);
            block->blockdef->inputs[i].data.simage.ms = ms;
            break;
        case INPUT_ARGUMENT:
            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                ScrMeasurement string_ms = measure_input_box(block->arguments[arg_id].data.text);
                block->arguments[arg_id].ms = string_ms;
                ms = string_ms;
                break;
            case ARGUMENT_BLOCK:
                block->arguments[arg_id].ms = block->arguments[arg_id].data.block.ms;
                ms = block->arguments[arg_id].ms;
                break;
            default:
                assert(false && "Unimplemented argument measure");
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            assert(block->arguments[arg_id].type == ARGUMENT_CONST_STRING);

            ScrMeasurement arg_ms = measure_input_box(block->arguments[arg_id].data.text);
            ScrMeasurement img_ms = measure_image((ScrImage) { .image_ptr = &drop_tex }, BLOCK_IMAGE_SIZE);
            ms = measure_group(arg_ms, img_ms, 0.0);
            block->arguments[arg_id].ms = ms;
            arg_id++;
            break;
        case INPUT_BLOCKDEF_EDITOR:
            ScrBlockdef* blockdef = block->arguments[arg_id].data.blockdef;
            blockdef_update_measurements(blockdef, hover_info.editor.edit_blockdef == blockdef);
            ScrMeasurement editor_ms = block->arguments[arg_id].data.blockdef->ms;
            ScrMeasurement button_ms = measure_block_button();
            if (hover_info.editor.edit_blockdef == block->arguments[arg_id].data.blockdef) {
                button_ms = measure_group(button_ms, measure_block_button(), BLOCK_PADDING);
                button_ms = measure_group(button_ms, measure_block_button(), BLOCK_PADDING);
            }

            ms = measure_group(editor_ms, button_ms, BLOCK_PADDING);
            block->arguments[arg_id].ms = ms;
            arg_id++;
            break;
        default:
            ms.size = as_scr_vec(MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0));
            break;
        }

        if (placement == PLACEMENT_VERTICAL) {
            ms.size.y += BLOCK_OUTLINE_SIZE * 2;
            block->ms.size.x = MAX(block->ms.size.x, ms.size.x + BLOCK_PADDING * 2);
            block->ms.size.y += ms.size.y;
        } else {
            ms.size.x += BLOCK_PADDING;
            block->ms.size.x += ms.size.x;
            block->ms.size.y = MAX(block->ms.size.y, ms.size.y + BLOCK_OUTLINE_SIZE * 4);
        }
    }

    if (block->ms.size.x > conf.block_size_threshold && block->ms.placement == PLACEMENT_HORIZONTAL) {
        update_measurements(block, PLACEMENT_VERTICAL);
        return;
    }

    if (block->parent) update_measurements(block->parent, PLACEMENT_HORIZONTAL);
}

void block_update_collisions(Vector2 position, ScrBlock* block) {
    if (hover_info.block && !block->parent) return;

    Rectangle block_size;
    block_size.x = position.x;
    block_size.y = position.y;
    block_size.width = block->ms.size.x;
    block_size.height = block->ms.size.y;
    
    if (!CheckCollisionPointRec(GetMousePosition(), block_size)) return;
    hover_info.block = block;

    Vector2 cursor = position;
    cursor.x += BLOCK_PADDING;
    if (block->ms.placement == PLACEMENT_VERTICAL) cursor.y += BLOCK_OUTLINE_SIZE * 2;

    int arg_id = 0;

    for (vec_size_t i = 0; i < vector_size(block->blockdef->inputs); i++) {
        if (hover_info.argument) return;
        int width = 0;
        int height = 0;
        ScrInput cur = block->blockdef->inputs[i];
        Rectangle arg_size;

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = cur.data.stext.ms.size.x;
            height = cur.data.stext.ms.size.y;
            break;
        case INPUT_IMAGE_DISPLAY:
            width = cur.data.simage.ms.size.x;
            height = cur.data.simage.ms.size.y;
            break;
        case INPUT_ARGUMENT:
            width = block->arguments[arg_id].ms.size.x;

            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5);
                arg_size.width = block->arguments[arg_id].ms.size.x;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;
                height = arg_size.height;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.argument = &block->arguments[arg_id];
                    hover_info.argument_pos = cursor;
                    hover_info.input = &block->arguments[arg_id].data.text;
                    break;
                }
                break;
            case ARGUMENT_BLOCK:
                Vector2 block_pos;
                block_pos.x = cursor.x;
                block_pos.y = cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height / 2 - block->arguments[arg_id].ms.size.y / 2); 

                arg_size.x = block_pos.x;
                arg_size.y = block_pos.y;
                arg_size.width = block->arguments[arg_id].ms.size.x;
                arg_size.height = block->arguments[arg_id].ms.size.y;
                height = arg_size.height;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.prev_argument = &block->arguments[arg_id];
                }
                
                block_update_collisions(block_pos, &block->arguments[arg_id].data.block);
                break;
            default:
                assert(false && "Unimplemented argument collision");
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            width = block->arguments[arg_id].ms.size.x;

            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5);
                arg_size.width = block->arguments[arg_id].ms.size.x;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;
                height = arg_size.height;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.argument = &block->arguments[arg_id];
                    hover_info.argument_pos = cursor;
                    break;
                }
                break;
            case ARGUMENT_TEXT:
                assert(false && "Illegal argument type ARGUMENT_TEXT");
                break;
            case ARGUMENT_BLOCK:
                assert(false && "Illegal argument type ARGUMENT_BLOCK");
                break;
            default:
                assert(false && "Unimplemented argument collision");
                break;
            }
            arg_id++;
            break;
        case INPUT_BLOCKDEF_EDITOR:
            assert(block->arguments[arg_id].type == ARGUMENT_BLOCKDEF);
            width = block->arguments[arg_id].ms.size.x;
            height = block->arguments[arg_id].ms.size.y;

            Vector2 blockdef_size = as_rl_vec(block->arguments[arg_id].data.blockdef->ms.size);
            Vector2 arg_pos;
            arg_pos.x = cursor.x;
            arg_pos.y = cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - height * 0.5);
            if (!CheckCollisionPointRec(GetMousePosition(), (Rectangle) { arg_pos.x, arg_pos.y, width, height })) {
                arg_id++;
                break;
            }

            hover_info.argument = &block->arguments[arg_id];
            hover_info.argument_pos = cursor;

            Rectangle blockdef_rect;
            blockdef_rect.x = arg_pos.x + BLOCK_OUTLINE_SIZE;
            blockdef_rect.y = arg_pos.y + height * 0.5 - blockdef_size.y * 0.5;
            blockdef_rect.width = blockdef_size.x;
            blockdef_rect.height = blockdef_size.y;

            if (CheckCollisionPointRec(GetMousePosition(), blockdef_rect)) {
                hover_info.editor.part = EDITOR_BLOCKDEF;
                ScrBlockdef* editor_blockdef = block->arguments[arg_id].data.blockdef;
                blockdef_update_collisions((Vector2) { blockdef_rect.x, blockdef_rect.y }, editor_blockdef, hover_info.editor.edit_blockdef == editor_blockdef);
                arg_id++;
                break;
            }
            arg_pos.x += blockdef_size.x + BLOCK_PADDING * 0.5;
            
            if (hover_info.editor.edit_blockdef == block->arguments[arg_id].data.blockdef) {
                if (block_button_update_collisions((Vector2) {arg_pos.x, arg_pos.y + height * 0.5 - conf.font_size * 0.5 }, EDITOR_ADD_ARG)) {
                    arg_id++;
                    break;
                }
                arg_pos.x += conf.font_size + BLOCK_PADDING * 0.5;

                if (block_button_update_collisions((Vector2) {arg_pos.x, arg_pos.y + height * 0.5 - conf.font_size * 0.5 }, EDITOR_ADD_TEXT)) {
                    arg_id++;
                    break;
                }
                arg_pos.x += conf.font_size + BLOCK_PADDING * 0.5;
            }

            if (block_button_update_collisions((Vector2) {arg_pos.x, arg_pos.y + height * 0.5 - conf.font_size * 0.5 }, EDITOR_EDIT)) {
                arg_id++;
                break;
            }

            arg_id++;
            break;
        default:
            Vector2 size = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0);
            width = size.x;
            height = size.y;
            break;
        }
        if (block->ms.placement == PLACEMENT_VERTICAL) {
            cursor.y += height + BLOCK_OUTLINE_SIZE * 2;
        } else {
            cursor.x += width + BLOCK_PADDING;
        }
    }
}

void draw_block(Vector2 position, ScrBlock* block, bool force_outline, bool force_collision) {
    ScrBlockdef* blockdef = block->blockdef;
    bool collision = (hover_info.block == block && hover_info.editor.part == EDITOR_NONE) || force_collision;
    Color color = as_rl_color(blockdef->color);
    Color outline_color = force_collision ? YELLOW : ColorBrightness(color, collision ? 0.5 : -0.2);
    Color block_color = ColorBrightness(color, collision ? 0.3 : 0.0);

    Vector2 cursor = position;

    Rectangle block_size;
    block_size.x = position.x;
    block_size.y = position.y;
    block_size.width = block->ms.size.x;
    block_size.height = block->ms.size.y;

    if (!CheckCollisionRecs(block_size, (Rectangle) { 0, 0, GetScreenWidth(), GetScreenHeight() })) return;

    draw_block_base(block_size, blockdef, block_color, 
        force_outline || (blockdef->type != BLOCKTYPE_CONTROL && blockdef->type != BLOCKTYPE_CONTROLEND) ? outline_color : (Color) {0});
    //draw_text_shadow(font_cond, TextFormat("%d", blockdef->ref_count), (Vector2) { block_size.x, block_size.y }, BLOCK_TEXT_SIZE, 0.0, WHITE, BLACK);

    cursor.x += BLOCK_PADDING;
    if (block->ms.placement == PLACEMENT_VERTICAL) cursor.y += BLOCK_OUTLINE_SIZE * 2;

    int arg_id = 0;
    for (vec_size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        int width = 0;
        int height = 0;
        ScrInput cur = blockdef->inputs[i];
        Vector2 arg_pos = cursor;

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = cur.data.stext.ms.size.x;
            height = cur.data.stext.ms.size.y;
            arg_pos.y += block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5;

            draw_text_shadow(font_cond, cur.data.stext.text, arg_pos, BLOCK_TEXT_SIZE, 0.0, WHITE, (Color) { 0x00, 0x00, 0x00, 0x88 });
            break;
        case INPUT_IMAGE_DISPLAY:
            width = cur.data.simage.ms.size.x;
            height = cur.data.simage.ms.size.y;
            arg_pos.y += block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - cur.data.simage.ms.size.y * 0.5;

            draw_image(arg_pos, cur.data.simage.image, BLOCK_IMAGE_SIZE);
            break;
        case INPUT_ARGUMENT:
            width = block->arguments[arg_id].ms.size.x;
            height = block->arguments[arg_id].ms.size.y;
            arg_pos.y += block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - block->arguments[arg_id].ms.size.y * 0.5;

            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                draw_input_box(arg_pos, block->arguments[arg_id].ms, &block->arguments[arg_id].data.text, block->arguments[arg_id].type == ARGUMENT_CONST_STRING);
                break;
            case ARGUMENT_BLOCK:
                draw_block(arg_pos, &block->arguments[arg_id].data.block, true, force_collision);
                break;
            default:
                assert(false && "Unimplemented argument draw");
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            assert(block->arguments[arg_id].type == ARGUMENT_CONST_STRING);
            width = block->arguments[arg_id].ms.size.x;
            height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

            Rectangle arg_size;
            arg_size.x = cursor.x;
            arg_size.y = cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5);
            arg_size.width = width;
            arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

            DrawRectangleRounded(arg_size, 0.5, 4, ColorBrightness(color, collision ? 0.0 : -0.3));

            if (&block->arguments[arg_id] == hover_info.argument || &block->arguments[arg_id] == hover_info.select_argument) {
                DrawRectangleRoundedLines(arg_size, 0.5, 4, BLOCK_OUTLINE_SIZE, ColorBrightness(color, &block->arguments[arg_id] == hover_info.select_argument ? -0.5 : 0.5));
            }
            Vector2 ms = MeasureTextEx(font_cond, block->arguments[arg_id].data.text, BLOCK_TEXT_SIZE, 0);
            draw_text_shadow(
                font_cond, 
                block->arguments[arg_id].data.text,
                (Vector2) { 
                    cursor.x + BLOCK_STRING_PADDING * 0.5, 
                    cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? BLOCK_OUTLINE_SIZE : block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5),
                },
                BLOCK_TEXT_SIZE,
                0.0,
                WHITE,
                (Color) { 0x00, 0x00, 0x00, 0x88 }
            );

            draw_image(
                (Vector2) { 
                    cursor.x + ms.x + BLOCK_STRING_PADDING * 0.5,
                    cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? BLOCK_OUTLINE_SIZE : block_size.height * 0.5 - BLOCK_IMAGE_SIZE * 0.5),
                }, 
                (ScrImage) {
                    .image_ptr = &drop_tex,
                },
                BLOCK_IMAGE_SIZE
            );
            arg_id++;
            break;
        case INPUT_BLOCKDEF_EDITOR:
            assert(block->arguments[arg_id].type == ARGUMENT_BLOCKDEF);
            width = block->arguments[arg_id].ms.size.x;
            height = block->arguments[arg_id].ms.size.y;
            arg_pos.y += block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - height * 0.5;

            DrawRectangle(arg_pos.x, arg_pos.y, width, height, (Color) { 0x00, 0x00, 0x00, 0x40 });

            Vector2 blockdef_size = as_rl_vec(block->arguments[arg_id].data.blockdef->ms.size);
            ScrBlockdef* editor_blockdef = block->arguments[arg_id].data.blockdef;
            draw_blockdef(
                (Vector2) {
                    arg_pos.x, 
                    arg_pos.y + height * 0.5 - blockdef_size.y * 0.5,
                },
                editor_blockdef,
                hover_info.editor.edit_blockdef == editor_blockdef
            );
            arg_pos.x += blockdef_size.x + BLOCK_PADDING * 0.5;

            if (hover_info.editor.edit_blockdef == block->arguments[arg_id].data.blockdef) {
                draw_block_button(
                    (Vector2) {
                        arg_pos.x,
                        arg_pos.y + height * 0.5 - conf.font_size * 0.5,
                    }, 
                    add_arg_tex,
                    hover_info.editor.part == EDITOR_ADD_ARG && hover_info.block == block
                );
                arg_pos.x += conf.font_size + BLOCK_PADDING * 0.5;

                draw_block_button(
                    (Vector2) {
                        arg_pos.x,
                        arg_pos.y + height * 0.5 - conf.font_size * 0.5,
                    }, 
                    add_text_tex,
                    hover_info.editor.part == EDITOR_ADD_TEXT && hover_info.block == block
                );
                arg_pos.x += conf.font_size + BLOCK_PADDING * 0.5;
            }

            draw_block_button(
                (Vector2) {
                    arg_pos.x,
                    arg_pos.y + height * 0.5 - conf.font_size * 0.5,
                }, 
                hover_info.editor.edit_blockdef == block->arguments[arg_id].data.blockdef ? close_tex : edit_tex,
                hover_info.editor.part == EDITOR_EDIT && hover_info.block == block
            );

            arg_id++;
            break;
        default:
            Vector2 size = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0);
            width = size.x;
            height = size.y;
            DrawTextEx(
                font_cond, 
                "NODEF",
                (Vector2) { 
                    cursor.x, 
                    cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5), 
                },
                BLOCK_TEXT_SIZE, 
                0.0, 
                RED
            );
            break;
        }

        if (block->ms.placement == PLACEMENT_VERTICAL) {
            cursor.y += height + BLOCK_OUTLINE_SIZE * 2;
        } else {
            cursor.x += width + BLOCK_PADDING;
        }
    }
}

// Draw order for draw_control_outline() and draw_controlend_outline()
//         1    12
//   +-----|---------+ 
//   |               | 2
//   |     +---------+
//   | 10  |    3
// 4 |     | 8
//   |-----|    7
//   |  9  +---------+
//   | 11            |
//   |               | 6
//   +---------------+
//         5

void draw_controlend_outline(DrawStack* block, Vector2 end_pos, Color color) {
    ScrBlockdefType blocktype = block->block->blockdef->type;
    Vector2 block_size = as_rl_vec(block->block->ms.size);
    
    if (blocktype == BLOCKTYPE_CONTROL) {
        /* 1 */ DrawRectangle(block->pos.x, block->pos.y, block_size.x, BLOCK_OUTLINE_SIZE, color);
    } else if (blocktype == BLOCKTYPE_CONTROLEND) {
        /* 12 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y, block_size.x - BLOCK_CONTROL_INDENT + BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE, color);
    }
    /* 2 */ DrawRectangle(block->pos.x + block_size.x - BLOCK_OUTLINE_SIZE, block->pos.y, BLOCK_OUTLINE_SIZE, block_size.y, color);
    /* 3 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y + block_size.y - BLOCK_OUTLINE_SIZE, block_size.x - BLOCK_CONTROL_INDENT + BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE, color);
    /* 8 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y + block_size.y, BLOCK_OUTLINE_SIZE, end_pos.y - (block->pos.y + block_size.y), color);
    /* 10 */ DrawRectangle(block->pos.x, block->pos.y, BLOCK_OUTLINE_SIZE, end_pos.y - block->pos.y, color);
}

void draw_control_outline(DrawStack* block, Vector2 end_pos, Color color, bool draw_end) {
    ScrBlockdefType blocktype = block->block->blockdef->type;
    Vector2 block_size = as_rl_vec(block->block->ms.size);

    if (blocktype == BLOCKTYPE_CONTROL) {
        /* 1 */ DrawRectangle(block->pos.x, block->pos.y, block_size.x, BLOCK_OUTLINE_SIZE, color);
    } else if (blocktype == BLOCKTYPE_CONTROLEND) {
        /* 12 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y, block_size.x - BLOCK_CONTROL_INDENT + BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE, color);
    }
    /* 2 */ DrawRectangle(block->pos.x + block_size.x - BLOCK_OUTLINE_SIZE, block->pos.y, BLOCK_OUTLINE_SIZE, block_size.y, color);
    /* 3 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y + block_size.y - BLOCK_OUTLINE_SIZE, block_size.x - BLOCK_CONTROL_INDENT + BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE, color);
    if (draw_end) {
        /* 4 */ DrawRectangle(block->pos.x, block->pos.y, BLOCK_OUTLINE_SIZE, end_pos.y + conf.font_size - block->pos.y, color);
        /* 5 */ DrawRectangle(end_pos.x, end_pos.y + conf.font_size - BLOCK_OUTLINE_SIZE, block_size.x, BLOCK_OUTLINE_SIZE, color);
        /* 6 */ DrawRectangle(end_pos.x + block_size.x - BLOCK_OUTLINE_SIZE, end_pos.y, BLOCK_OUTLINE_SIZE, conf.font_size, color);
        /* 7 */ DrawRectangle(end_pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, end_pos.y, block_size.x - BLOCK_CONTROL_INDENT + BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE, color);
    } else {
        /* 9 */ DrawRectangle(end_pos.x, end_pos.y - BLOCK_OUTLINE_SIZE, BLOCK_CONTROL_INDENT, BLOCK_OUTLINE_SIZE, color);
        /* 10 */ DrawRectangle(block->pos.x, block->pos.y, BLOCK_OUTLINE_SIZE, end_pos.y - block->pos.y, color);
    }
    /* 8 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y + block_size.y, BLOCK_OUTLINE_SIZE, end_pos.y - (block->pos.y + block_size.y), color);
}

void blockchain_check_collisions(ScrBlockChain* chain, Vector2 camera_pos) {
    vector_clear(draw_stack);

    hover_info.blockchain = chain;
    hover_info.blockchain_layer = 0;
    Vector2 pos = as_rl_vec(hover_info.blockchain->pos);
    pos.x -= camera_pos.x;
    pos.y -= camera_pos.y;
    for (vec_size_t i = 0; i < vector_size(hover_info.blockchain->blocks); i++) {
        if (hover_info.block) break;
        hover_info.blockchain_layer = vector_size(draw_stack);
        hover_info.blockchain_index = i;

        ScrBlockdef* blockdef = chain->blocks[i].blockdef;
        if ((blockdef->type == BLOCKTYPE_END || blockdef->type == BLOCKTYPE_CONTROLEND) && vector_size(draw_stack) > 0) {
            pos.x -= BLOCK_CONTROL_INDENT;

            if (blockdef->type == BLOCKTYPE_END) {
                DrawStack prev_block = draw_stack[vector_size(draw_stack) - 1];
                Rectangle rect;
                rect.x = pos.x;
                rect.y = pos.y;
                rect.width = prev_block.block->ms.size.x;
                rect.height = conf.font_size;

                if (CheckCollisionPointRec(GetMousePosition(), rect)) {
                    hover_info.block = &hover_info.blockchain->blocks[i];
                }
            } else if (blockdef->type == BLOCKTYPE_CONTROLEND) {
                block_update_collisions(pos, &hover_info.blockchain->blocks[i]);
            }
            vector_pop(draw_stack);
        } else {
            block_update_collisions(pos, &hover_info.blockchain->blocks[i]);
        }

        if (blockdef->type == BLOCKTYPE_CONTROL || blockdef->type == BLOCKTYPE_CONTROLEND) {
            DrawStack stack_item;
            stack_item.pos = as_scr_vec(pos);
            stack_item.block = &chain->blocks[i];
            vector_add(&draw_stack, stack_item);
            pos.x += BLOCK_CONTROL_INDENT;
        }
        pos.y += hover_info.blockchain->blocks[i].ms.size.y;
    }
}

void draw_block_chain(ScrBlockChain* chain, Vector2 camera_pos, bool chain_highlight) {
    vector_clear(draw_stack);

    Vector2 pos = as_rl_vec(chain->pos);
    pos.x -= camera_pos.x;
    pos.y -= camera_pos.y;
    for (vec_size_t i = 0; i < vector_size(chain->blocks); i++) {
        bool exec_highlight = hover_info.exec_ind == i && chain_highlight;
        ScrBlockdef* blockdef = chain->blocks[i].blockdef;

        if ((blockdef->type == BLOCKTYPE_END || blockdef->type == BLOCKTYPE_CONTROLEND) && vector_size(draw_stack) > 0) {
            pos.x -= BLOCK_CONTROL_INDENT;
            DrawStack prev_block = draw_stack[vector_size(draw_stack) - 1];
            ScrBlockdef* prev_blockdef = prev_block.block->blockdef;

            Rectangle rect;
            rect.x = prev_block.pos.x;
            rect.y = prev_block.pos.y + prev_block.block->ms.size.y;
            rect.width = BLOCK_CONTROL_INDENT;
            rect.height = pos.y - (prev_block.pos.y + prev_block.block->ms.size.y);
            DrawRectangleRec(rect, as_rl_color(prev_blockdef->color));

            bool touching_block = hover_info.block == &chain->blocks[i];
            Color outline_color = ColorBrightness(as_rl_color(prev_blockdef->color), hover_info.block == prev_block.block || touching_block ? 0.5 : -0.2);
            if (blockdef->type == BLOCKTYPE_END) {
                Color end_color = ColorBrightness(as_rl_color(prev_blockdef->color), exec_highlight || touching_block ? 0.3 : 0.0);
                DrawRectangle(pos.x, pos.y, prev_block.block->ms.size.x, conf.font_size, end_color);
                draw_control_outline(&prev_block, pos, outline_color, true);
            } else if (blockdef->type == BLOCKTYPE_CONTROLEND) {
                draw_block(pos, &chain->blocks[i], false, exec_highlight);
                draw_controlend_outline(&prev_block, pos, outline_color);
            }

            vector_pop(draw_stack);
        } else {
            draw_block(pos, &chain->blocks[i], false, exec_highlight);
        }
        if (blockdef->type == BLOCKTYPE_CONTROL || blockdef->type == BLOCKTYPE_CONTROLEND) {
            DrawStack stack_item;
            stack_item.pos = as_scr_vec(pos);
            stack_item.block = &chain->blocks[i];
            vector_add(&draw_stack, stack_item);
            pos.x += BLOCK_CONTROL_INDENT;
        }
        pos.y += chain->blocks[i].ms.size.y;
    }

    pos.y += conf.font_size;
    Rectangle rect;
    for (vec_size_t i = 0; i < vector_size(draw_stack); i++) {
        DrawStack prev_block = draw_stack[i];
        ScrBlockdef* prev_blockdef = prev_block.block->blockdef;

        pos.x = prev_block.pos.x;

        rect.x = prev_block.pos.x;
        rect.y = prev_block.pos.y + prev_block.block->ms.size.y;
        rect.width = BLOCK_CONTROL_INDENT;
        rect.height = pos.y - (prev_block.pos.y + prev_block.block->ms.size.y);

        DrawRectangleRec(rect, as_rl_color(prev_blockdef->color));
        draw_control_outline(&prev_block, pos, ColorBrightness(as_rl_color(prev_blockdef->color), hover_info.block == prev_block.block ? 0.5 : -0.2), false);
    }
}

bool button_check_collisions(Vector2* position, char* text, float button_scale, float side_padding, float side_margin) {
    side_padding *= conf.font_size;
    side_margin *= conf.font_size;

    int text_size = conf.font_size * 0.6;
    int text_width = text ? MeasureTextEx(font_cond, text, text_size, 0.0).x : 0;
    Rectangle rect = {
        .x = position->x,
        .y = position->y,
        .width = text_width + side_padding * 2,
        .height = conf.font_size * button_scale,
    };

    position->x += rect.width + side_margin;
    return CheckCollisionPointRec(GetMousePosition(), rect) && vector_size(mouse_blockchain.blocks) == 0;
}

void draw_button(Vector2* position, char* text, float button_scale, float side_padding, float side_margin, bool selected, bool hovered) {
    side_padding *= conf.font_size;
    side_margin *= conf.font_size;

    int text_size = conf.font_size * 0.6;
    int text_width = text ? MeasureTextEx(font_cond, text, text_size, 0.0).x : 0;
    Rectangle rect = {
        .x = position->x,
        .y = position->y,
        .width = text_width + side_padding * 2,
        .height = conf.font_size * button_scale,
    };

    if (selected || hovered) {
        Color select_color = selected ? (Color){ 0xFF, 0xFF, 0xFF, 0xFF } :
                                        (Color){ 0x40, 0x40, 0x40, 0xFF };
        DrawRectangleRec(rect, select_color);
    }
    if (text) {
        Color text_select_color = selected ? (Color){ 0x00, 0x00, 0x00, 0xFF } :
                                             (Color){ 0xFF, 0xFF, 0xFF, 0xFF };
        DrawTextEx(font_cond, text, (Vector2){ rect.x + side_padding, rect.y + rect.height * 0.5 - text_size * 0.5 }, text_size, 0.0, text_select_color);
    }

    position->x += rect.width + side_margin;
}

void bars_check_collisions(void) {
    Vector2 pos = (Vector2){ 0.0, conf.font_size * 1.2 };
    for (vec_size_t i = 0; i < ARRLEN(tab_bar_buttons_text); i++) {
        Vector2 last_pos = pos;
        if (button_check_collisions(&pos, tab_bar_buttons_text[i], 1.0, 0.3, 0)) {
            hover_info.top_bars.type = TOPBAR_TABS;
            hover_info.top_bars.pos = last_pos;
            hover_info.top_bars.ind = i;
            return;
        }
    }

    Vector2 run_pos = (Vector2){ GetScreenWidth() - conf.font_size * 2.0, conf.font_size * 1.2 };
    for (int i = 0; i < 2; i++) {
        Vector2 last_pos = run_pos;
        if (button_check_collisions(&run_pos, NULL, 1.0, 0.5, 0)) {
            hover_info.top_bars.type = TOPBAR_RUN_BUTTON;
            hover_info.top_bars.pos = last_pos;
            hover_info.top_bars.ind = i;
            return;
        }
    }

    int width = MeasureTextEx(font_eb, "Scrap", conf.font_size * 0.8, 0.0).x;
    pos = (Vector2){ 20 + conf.font_size + width, 0 };
    for (vec_size_t i = 0; i < ARRLEN(top_bar_buttons_text); i++) {
        Vector2 last_pos = pos;
        if (button_check_collisions(&pos, top_bar_buttons_text[i], 1.2, 0.3, 0)) {
            hover_info.top_bars.type = TOPBAR_TOP;
            hover_info.top_bars.pos = last_pos;
            hover_info.top_bars.ind = i;
            return;
        }
    }
}

#define COLLISION_AT(bar_type, index) (hover_info.top_bars.type == (bar_type) && hover_info.top_bars.ind == (int)(index))
void draw_tab_buttons(int sw) {
    Vector2 pos = (Vector2){ 0.0, conf.font_size * 1.2 };
    for (vec_size_t i = 0; i < ARRLEN(tab_bar_buttons_text); i++) {
        draw_button(&pos, tab_bar_buttons_text[i], 1.0, 0.3, 0, i == current_tab, COLLISION_AT(TOPBAR_TABS, i));
    }

    Vector2 run_pos = (Vector2){ sw - conf.font_size * 2.0, conf.font_size * 1.2 };
    Vector2 run_pos_copy = run_pos;
    draw_button(&run_pos_copy, NULL, 1.0, 0.5, 0, false, COLLISION_AT(TOPBAR_RUN_BUTTON, 0));
    draw_button(&run_pos_copy, NULL, 1.0, 0.5, 0, vm.is_running, COLLISION_AT(TOPBAR_RUN_BUTTON, 1));
    DrawTextureEx(stop_tex, run_pos, 0, (float)conf.font_size / (float)stop_tex.width, WHITE);
    run_pos.x += conf.font_size;
    DrawTextureEx(run_tex, run_pos, 0, (float)conf.font_size / (float)run_tex.width, vm.is_running ? BLACK : WHITE);
}

void draw_top_bar(void) {
    DrawTexture(logo_tex, 5, conf.font_size * 0.1, WHITE);

    int width = MeasureTextEx(font_eb, "Scrap", conf.font_size * 0.8, 0.0).x;
    DrawTextEx(font_eb, "Scrap", (Vector2){ 10 + conf.font_size, conf.font_size * 0.2 }, conf.font_size * 0.8, 0.0, WHITE);

    Vector2 pos = { 20 + conf.font_size + width, 0 };

    for (vec_size_t i = 0; i < ARRLEN(top_bar_buttons_text); i++) {
        draw_button(&pos, top_bar_buttons_text[i], 1.2, 0.3, 0, false, COLLISION_AT(TOPBAR_TOP, i));
    }
}
#undef COLLISION_AT

void draw_tooltip(void) {
    if (hover_info.time_at_last_pos < 0.5 || !hover_info.block) return;

    Vector2 pos = GetMousePosition();
    pos.x += 10.0;
    pos.y += 10.0;

    char* text = "Amog";
    Vector2 ms = MeasureTextEx(font_cond, text, conf.font_size * 0.5, 0);   
    DrawRectangle(pos.x - 5, pos.y - 5, ms.x + 10, ms.y + 10, (Color) { 0x00, 0x00, 0x00, 0x80 });
    DrawTextEx(font_cond, text, pos, conf.font_size * 0.5, 0, WHITE);
}

void draw_dropdown_list(void) {
    if (!hover_info.select_argument) return;

    ScrBlockdef* blockdef = hover_info.select_block->blockdef;
    ScrInput block_input = blockdef->inputs[hover_info.select_argument->input_id];

    if (block_input.type != INPUT_DROPDOWN) return;
    
    Vector2 pos;
    pos = hover_info.select_argument_pos;
    pos.y += hover_info.select_block->ms.size.y;

    DrawRectangle(pos.x, pos.y, dropdown.ms.size.x, dropdown.ms.size.y, ColorBrightness(as_rl_color(blockdef->color), -0.3));
    if (hover_info.dropdown_hover_ind != -1) {
        DrawRectangle(pos.x, pos.y + (hover_info.dropdown_hover_ind - dropdown.scroll_amount) * conf.font_size, dropdown.ms.size.x, conf.font_size, as_rl_color(blockdef->color));
    }

    pos.x += 5.0;
    pos.y += 5.0;

    size_t list_len = 0;
    char** list = block_input.data.drop.list(hover_info.select_block, &list_len);
    for (size_t i = dropdown.scroll_amount; i < list_len; i++) {
        if (pos.y > GetScreenHeight()) break;
        draw_text_shadow(font_cond, list[i], pos, BLOCK_TEXT_SIZE, 0, WHITE, (Color) { 0x00, 0x00, 0x00, 0x88 });
        pos.y += conf.font_size;
    }
}

void draw_dots(void) {
    int win_width = GetScreenWidth();
    int win_height = GetScreenHeight();

    for (int y = MOD(-(int)camera_pos.y, conf.font_size * 2); y < win_height; y += conf.font_size * 2) {
        for (int x = MOD(-(int)camera_pos.x, conf.font_size * 2); x < win_width; x += conf.font_size * 2) {
            DrawPixel(x, y, (Color) { 0x60, 0x60, 0x60, 0xff });
        }
    }

    BeginShaderMode(line_shader);
    for (int y = MOD(-(int)camera_pos.y, conf.font_size * 2); y < win_height; y += conf.font_size * 2) {
        DrawLine(0, y, win_width, y, (Color) { 0x40, 0x40, 0x40, 0xff });
    }
    for (int x = MOD(-(int)camera_pos.x, conf.font_size * 2); x < win_width; x += conf.font_size * 2) {
        DrawLine(x, 0, x, win_height, (Color) { 0x40, 0x40, 0x40, 0xff });
    }
    EndShaderMode();
}

void draw_action_bar(void) {
    if (actionbar.show_time <= 0) return;

    int width = MeasureTextEx(font_eb, actionbar.text, conf.font_size * 0.75, 0.0).x;
    Vector2 pos;
    pos.x = (GetScreenWidth() - conf.side_bar_size) / 2 - width / 2 + conf.side_bar_size;
    pos.y = (GetScreenHeight() - conf.font_size * 2.2) * 0.15 + conf.font_size * 2.2;
    Color color = YELLOW;
    color.a = actionbar.show_time / 3.0 * 255.0;

    DrawTextEx(font_eb, actionbar.text, pos, conf.font_size * 0.75, 0.0, color);
}

void draw_scrollbars(void) {
    float size = GetScreenWidth() / (block_code.max_pos.x - block_code.min_pos.x);
    if (size < 1) {
        size *= GetScreenWidth() - conf.side_bar_size;
        float t = UNLERP(block_code.min_pos.x, block_code.max_pos.x, camera_pos.x + GetScreenWidth() / 2);

        BeginScissorMode(conf.side_bar_size, GetScreenHeight() - conf.font_size / 6, GetScreenWidth() - conf.side_bar_size, conf.font_size / 6);
        DrawRectangle(
            LERP(conf.side_bar_size, GetScreenWidth() - size, t), 
            GetScreenHeight() - conf.font_size / 6, 
            size, 
            conf.font_size / 6, 
            (Color) { 0xff, 0xff, 0xff, 0x80 }
        );
        EndScissorMode();
    }

    size = GetScreenHeight() / (block_code.max_pos.y - block_code.min_pos.y);
    if (size < 1) {
        size *= GetScreenHeight() - conf.font_size * 2.2;
        float t = UNLERP(block_code.min_pos.y, block_code.max_pos.y, camera_pos.y + GetScreenHeight() / 2);

        BeginScissorMode(GetScreenWidth() - conf.font_size / 6, conf.font_size * 2.2, conf.font_size / 6, GetScreenHeight() - conf.font_size * 2.2);
        DrawRectangle(
            GetScreenWidth() - conf.font_size / 6, 
            LERP(conf.font_size * 2.2, GetScreenHeight() - size, t), 
            conf.font_size / 6, 
            size, 
            (Color) { 0xff, 0xff, 0xff, 0x80 }
        );
        EndScissorMode();
    }
}

void draw_sidebar(void) {
    BeginScissorMode(0, conf.font_size * 2.2, conf.side_bar_size, GetScreenHeight() - conf.font_size * 2.2);
    DrawRectangle(0, conf.font_size * 2.2, conf.side_bar_size, GetScreenHeight() - conf.font_size * 2.2, (Color){ 0, 0, 0, 0x60 });

    int pos_y = conf.font_size * 2.2 + SIDE_BAR_PADDING - sidebar.scroll_amount;
    for (vec_size_t i = 0; i < vector_size(sidebar.blocks); i++) {
        draw_block((Vector2){ SIDE_BAR_PADDING, pos_y }, &sidebar.blocks[i], true, false);
        pos_y += sidebar.blocks[i].ms.size.y + SIDE_BAR_PADDING;
    }

    if (sidebar.max_y > GetScreenHeight()) {
        float size = (GetScreenHeight() - conf.font_size * 2.2) / (sidebar.max_y - conf.font_size * 2.2);
        size *= GetScreenHeight() - conf.font_size * 2.2;
        float t = UNLERP(0, sidebar.max_y - GetScreenHeight(), sidebar.scroll_amount);

        DrawRectangle(
            conf.side_bar_size - conf.font_size / 6, 
            LERP(conf.font_size * 2.2, GetScreenHeight() - size, t), 
            conf.font_size / 6,
            size,
            (Color) { 0xff, 0xff, 0xff, 0x80 }
        );
    }
    EndScissorMode();
}

void draw_term(void) {
    pthread_mutex_lock(&out_win.lock);
    DrawRectangleRec(out_win.size, BLACK);
    BeginShaderMode(line_shader);
    DrawRectangleLinesEx(out_win.size, 2.0, (Color) { 0x60, 0x60, 0x60, 0xff });
    EndShaderMode();

    if (out_win.buffer) {
        Vector2 pos = (Vector2) { out_win.size.x, out_win.size.y };
        for (int y = 0; y < out_win.char_h; y++) {
            pos.x = out_win.size.x;
            for (int x = 0; x < out_win.char_w; x++) {
                DrawTextEx(font_mono, out_win.buffer[x + y*out_win.char_w], pos, TERM_CHAR_SIZE, 0.0, WHITE);
                pos.x += out_win.char_size.x;
            }
            pos.y += TERM_CHAR_SIZE;
        }
        if (fmod(GetTime(), 1.0) <= 0.5) {
            Vector2 cursor_pos = (Vector2) {
                out_win.size.x + (out_win.cursor_pos % out_win.char_w) * out_win.char_size.x,
                out_win.size.y + (out_win.cursor_pos / out_win.char_w) * TERM_CHAR_SIZE,
            };
            DrawRectangle(cursor_pos.x, cursor_pos.y, BLOCK_OUTLINE_SIZE, TERM_CHAR_SIZE, WHITE);
        }
    }

    pthread_mutex_unlock(&out_win.lock);
}

// https://easings.net/#easeOutExpo
float ease_out_expo(float x) {
    return x == 1.0 ? 1.0 : 1 - powf(2.0, -10.0 * x);
}

void nk_draw_rectangle(struct nk_context *ctx, struct nk_color color)
{
    struct nk_command_buffer *canvas;
    canvas = nk_window_get_canvas(ctx);

    struct nk_rect space;
    enum nk_widget_layout_states state;
    state = nk_widget(&space, ctx);
    if (!state) return;

    nk_fill_rect(canvas, space, 0, color);
}

void gui_show(NuklearGuiType type) {
    gui.is_fading = false;
    gui.type = type;
    gui.pos = hover_info.top_bars.pos;
    shader_time = -0.2;
}

void gui_hide(void) {
    gui.is_fading = true;
}

void gui_hide_immediate(void) {
    gui.is_fading = true;
    gui.is_hiding = true;
}

void gui_show_title(char* name) {
    nk_layout_space_begin(gui.ctx, NK_DYNAMIC, conf.font_size, 100);

    struct nk_rect layout_size = nk_layout_space_bounds(gui.ctx);

    nk_layout_space_push(gui.ctx, nk_rect(0.0, 0.0, 1.0, 1.0));
    nk_draw_rectangle(gui.ctx, nk_rgb(0x30, 0x30, 0x30));
    nk_layout_space_push(gui.ctx, nk_rect(0.0, 0.0, 1.0, 1.0));
    nk_style_set_font(gui.ctx, font_eb_nuc);
    nk_label(gui.ctx, name, NK_TEXT_CENTERED);
    nk_style_set_font(gui.ctx, font_cond_nuc);

    nk_layout_space_push(gui.ctx, nk_rect(1.0 - conf.font_size / layout_size.w, 0.0, conf.font_size / layout_size.w, 1.0));
    if (nk_button_label(gui.ctx, "X")) {
        gui_hide();
    }
    nk_layout_space_end(gui.ctx);
}

void gui_restart_warning(void) {
    struct nk_rect bounds = nk_widget_bounds(gui.ctx);
    nk_image(gui.ctx, warn_tex_nuc);
    if (nk_input_is_mouse_hovering_rect(&gui.ctx->input, bounds))
        // For some reason tooltip crops last char so we add additional char at the end
        nk_tooltip(gui.ctx, "Needs restart for changes to take effect ");
}

void handle_gui(void) {
    if (gui.is_hiding) {
        gui.shown = false;
        gui.is_hiding = false;
    }
    if (gui.is_fading) {
        gui.animation_time = MAX(gui.animation_time - GetFrameTime() * 2.0, 0.0);
        if (gui.animation_time == 0.0) gui.shown = false;
    } else {
        gui.shown = true;
        gui.animation_time = MIN(gui.animation_time + GetFrameTime() * 2.0, 1.0);
    }

    if (!gui.shown) return;

    float animation_ease = ease_out_expo(gui.animation_time);
    gui.ctx->style.window.spacing = nk_vec2(10, 10);
    gui.ctx->style.button.text_alignment = NK_TEXT_CENTERED;

    Vector2 gui_size;
    switch (gui.type) {
    case GUI_TYPE_SETTINGS:
        gui_size.x = 0.6 * GetScreenWidth() * animation_ease;
        gui_size.y = 0.8 * GetScreenHeight() * animation_ease;

        if (nk_begin(
                gui.ctx, 
                "Settings", 
                nk_rect(
                    GetScreenWidth() / 2 - gui_size.x / 2, 
                    GetScreenHeight() / 2 - gui_size.y / 2, 
                    gui_size.x, 
                    gui_size.y
                ), 
                NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)
        ) {
            gui_show_title("Settings");

            nk_layout_row_dynamic(gui.ctx, 10, 1);
            nk_spacer(gui.ctx);

            nk_layout_row_dynamic(gui.ctx, conf.font_size, 1);
            nk_style_set_font(gui.ctx, font_eb_nuc);
            nk_label(gui.ctx, "Interface", NK_TEXT_CENTERED);
            nk_style_set_font(gui.ctx, font_cond_nuc);

            nk_layout_row_template_begin(gui.ctx, conf.font_size);
            nk_layout_row_template_push_static(gui.ctx, 10);
            nk_layout_row_template_push_dynamic(gui.ctx);
            nk_layout_row_template_push_static(gui.ctx, conf.font_size);
            nk_layout_row_template_push_dynamic(gui.ctx);
            nk_layout_row_template_push_static(gui.ctx, 10);
            nk_layout_row_template_end(gui.ctx);

            nk_spacer(gui.ctx);
            nk_label(gui.ctx, "UI Size", NK_TEXT_RIGHT);
            gui_restart_warning();
            nk_property_int(gui.ctx, "#", 8, &gui_conf.font_size, 64, 1, 1.0);
            nk_spacer(gui.ctx);

            nk_spacer(gui.ctx);
            nk_label(gui.ctx, "Side bar size", NK_TEXT_RIGHT);
            nk_spacer(gui.ctx);
            nk_property_int(gui.ctx, "#", 10, &gui_conf.side_bar_size, 500, 1, 1.0);
            nk_spacer(gui.ctx);

            nk_spacer(gui.ctx);
            nk_label(gui.ctx, "FPS limit", NK_TEXT_RIGHT);
            nk_spacer(gui.ctx);
            nk_property_int(gui.ctx, "#", 10, &gui_conf.fps_limit, 240, 1, 1.0);
            nk_spacer(gui.ctx);

            nk_spacer(gui.ctx);
            nk_label(gui.ctx, "Block size threshold", NK_TEXT_RIGHT);
            nk_spacer(gui.ctx);
            nk_property_int(gui.ctx, "#", 200, &gui_conf.block_size_threshold, 8000, 10, 10.0);
            nk_spacer(gui.ctx);

            nk_spacer(gui.ctx);
            nk_label(gui.ctx, "Font path", NK_TEXT_RIGHT);
            gui_restart_warning();
            nk_edit_string_zero_terminated(gui.ctx, NK_EDIT_FIELD, gui_conf.font_path, FONT_PATH_MAX_SIZE, nk_filter_default);
            nk_spacer(gui.ctx);

            nk_spacer(gui.ctx);
            nk_label(gui.ctx, "Bold font path", NK_TEXT_RIGHT);
            gui_restart_warning();
            nk_edit_string_zero_terminated(gui.ctx, NK_EDIT_FIELD, gui_conf.font_bold_path, FONT_PATH_MAX_SIZE, nk_filter_default);
            nk_spacer(gui.ctx);

            nk_spacer(gui.ctx);
            nk_label(gui.ctx, "Monospaced font path", NK_TEXT_RIGHT);
            gui_restart_warning();
            nk_edit_string_zero_terminated(gui.ctx, NK_EDIT_FIELD, gui_conf.font_mono_path, FONT_PATH_MAX_SIZE, nk_filter_default);
            nk_spacer(gui.ctx);

            nk_layout_row_template_begin(gui.ctx, conf.font_size);
            nk_layout_row_template_push_dynamic(gui.ctx);
            nk_layout_row_template_push_static(gui.ctx, conf.font_size * 3);
            nk_layout_row_template_push_static(gui.ctx, conf.font_size * 3);
            nk_layout_row_template_push_static(gui.ctx, 10);
            nk_layout_row_template_end(gui.ctx);
            nk_spacer(gui.ctx);
            if (nk_button_label(gui.ctx, "Reset")) {
                set_default_config(&gui_conf);
            }
            if (nk_button_label(gui.ctx, "Apply")) {
                apply_config(&conf, &gui_conf);
                save_config(&gui_conf);
            }
            nk_spacer(gui.ctx);
        }
        nk_end(gui.ctx);
        break;
    case GUI_TYPE_ABOUT:
        gui_size.x = 500 * conf.font_size / 32.0 * animation_ease;
        gui_size.y = 250 * conf.font_size / 32.0 * animation_ease;

        if (nk_begin(
                gui.ctx, 
                "About", 
                nk_rect(
                    GetScreenWidth() / 2 - gui_size.x / 2, 
                    GetScreenHeight() / 2 - gui_size.y / 2, 
                    gui_size.x, 
                    gui_size.y
                ), 
                NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)
        ) {
            gui_show_title("About");

            nk_layout_row_dynamic(gui.ctx, 10 * conf.font_size / 32.0, 1);
            nk_spacer(gui.ctx);

            nk_layout_row_template_begin(gui.ctx, conf.font_size);
            nk_layout_row_template_push_static(gui.ctx, 10 * conf.font_size / 32.0);
            nk_layout_row_template_push_static(gui.ctx, conf.font_size);
            nk_layout_row_template_push_dynamic(gui.ctx);
            nk_layout_row_template_push_static(gui.ctx, 10 * conf.font_size / 32.0);
            nk_layout_row_template_end(gui.ctx);

            nk_spacer(gui.ctx);
            nk_image(gui.ctx, logo_tex_nuc);
            nk_style_set_font(gui.ctx, font_eb_nuc);
            nk_label(gui.ctx, "Scrap v" SCRAP_VERSION, NK_TEXT_LEFT);
            nk_style_set_font(gui.ctx, font_cond_nuc);
            nk_spacer(gui.ctx);

            nk_layout_row_template_begin(gui.ctx, conf.font_size * 1.9);
            nk_layout_row_template_push_static(gui.ctx, 10 * conf.font_size / 32.0);
            nk_layout_row_template_push_dynamic(gui.ctx);
            nk_layout_row_template_push_static(gui.ctx, 10 * conf.font_size / 32.0);
            nk_layout_row_template_end(gui.ctx);

            nk_spacer(gui.ctx);
            nk_label_wrap(gui.ctx, "Scrap is a project that allows anyone to build software using simple, block based interface.");
            nk_spacer(gui.ctx);

            nk_layout_row_template_begin(gui.ctx, conf.font_size);
            nk_layout_row_template_push_static(gui.ctx, 10 * conf.font_size / 32.0);
            nk_layout_row_template_push_static(gui.ctx, conf.font_size * 3);
            nk_layout_row_template_end(gui.ctx);
            nk_spacer(gui.ctx);
            if (nk_button_label(gui.ctx, "License")) {
                OpenURL(LICENSE_URL);
            }
        }
        nk_end(gui.ctx);
        break;
    case GUI_TYPE_FILE:
        gui_size.x = 150 * conf.font_size / 32.0;
        gui_size.y = conf.font_size * 2;
        gui.ctx->style.window.spacing = nk_vec2(0, 0);
        gui.ctx->style.button.text_alignment = NK_TEXT_LEFT;

        if (nk_begin(
                gui.ctx, 
                "File", 
                nk_rect(gui.pos.x, gui.pos.y + conf.font_size * 1.2, gui_size.x, gui_size.y), 
                NK_WINDOW_NO_SCROLLBAR)
        ) {
            nk_layout_row_dynamic(gui.ctx, conf.font_size, 1);
            if (nk_button_label(gui.ctx, "Save project")) {
                char const* filters[] = {"*.scrp"};
                char* save_path = tinyfd_saveFileDialog(NULL, "project.scrp", ARRLEN(filters), filters, "Scrap project files (.scrp)"); 
                if (save_path) save_code(save_path, editor_code);
            }
            if (nk_button_label(gui.ctx, "Load project")) {
                char const* filters[] = {"*.scrp"};
                char* files = tinyfd_openFileDialog(NULL, "project.scrp", ARRLEN(filters), filters, "Scrap project files (.scrp)", 0);

                if (files) {
                    ScrBlockChain* chain = load_code(files);
                    if (!chain) {
                        actionbar_show("File load failed :(");
                    } else {
                        for (size_t i = 0; i < vector_size(editor_code); i++) blockchain_free(&editor_code[i]);
                        vector_free(editor_code);
                        editor_code = chain;

                        blockchain_select_counter = 0;
                        camera_pos.x = editor_code[blockchain_select_counter].pos.x - ((GetScreenWidth() - conf.side_bar_size) / 2 + conf.side_bar_size);
                        camera_pos.y = editor_code[blockchain_select_counter].pos.y - ((GetScreenHeight() - conf.font_size * 2.2) / 2 + conf.font_size * 2.2);

                        actionbar_show("File load succeeded!");
                    }
                }
            }
        }
        nk_end(gui.ctx);
        break;
    default:
        break;
    }
}

bool handle_top_bar_click(void) {
    if (hover_info.top_bars.type == TOPBAR_TOP) {
        switch (hover_info.top_bars.ind) {
        case 0:
            if (!vm.is_running) gui_show(GUI_TYPE_FILE);
            break;
        case 1:
            gui_conf = conf;
            gui_show(GUI_TYPE_SETTINGS);
            break;
        case 2:
            gui_show(GUI_TYPE_ABOUT);
            break;
        default:
            break;
        }
    } else if (hover_info.top_bars.type == TOPBAR_TABS) {
        if (current_tab != (TabType)hover_info.top_bars.ind) {
            shader_time = 0.0;
            current_tab = hover_info.top_bars.ind;
        }
    } else if (hover_info.top_bars.type == TOPBAR_RUN_BUTTON) {
        if (hover_info.top_bars.ind == 1 && !vm.is_running) {
            sem_destroy(&out_win.input_sem);
            sem_init(&out_win.input_sem, 0, 0);
            out_win.buf_start = 0;
            out_win.buf_end = 0;
            term_clear();
            exec = exec_new();
            exec_copy_code(&vm, &exec, editor_code);
            if (exec_start(&vm, &exec)) {
                actionbar_show("Started successfully!");
                if (current_tab != TAB_OUTPUT) {
                    shader_time = 0.0;
                    current_tab = TAB_OUTPUT;
                }
            } else {
                actionbar_show("Start failed!");
            }
        } else if (hover_info.top_bars.ind == 0 && vm.is_running) {
            printf("STOP\n");
            exec_stop(&vm, &exec);
        }
    }
    return true;
}

void deselect_all(void) {
    hover_info.select_argument = NULL;
    hover_info.select_input = NULL;
    hover_info.select_argument_pos.x = 0;
    hover_info.select_argument_pos.y = 0;
    dropdown.scroll_amount = 0;
}

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
    update_measurements(block, PLACEMENT_HORIZONTAL);
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
            blockcode_remove_blockchain(&block_code, i);
            i--;
        }
    }
}

bool handle_sidebar_click(bool mouse_empty) {
    if (hover_info.select_argument) {
        deselect_all();
        return true;
    }
    if (mouse_empty && hover_info.block) {
        // Pickup block
        blockchain_add_block(&mouse_blockchain, block_new_ms(hover_info.block->blockdef));
        if (hover_info.block->blockdef->type == BLOCKTYPE_CONTROL && vm.end_blockdef) {
            blockchain_add_block(&mouse_blockchain, block_new_ms(vm.blockdefs[vm.end_blockdef]));
        }
        return true;
    } else if (!mouse_empty) {
        // Drop block
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
    ScrBlockdef* blockdef = hover_info.argument->data.blockdef;
    size_t last_input = vector_size(blockdef->inputs);
    char str[32];
    switch (hover_info.editor.part) {
    case EDITOR_ADD_ARG:
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
        blockdef_add_argument(hover_info.argument->data.blockdef, "", BLOCKCONSTR_UNLIMITED);
        sprintf(str, "arg%zu", last_input);
        ScrBlockdef* arg_blockdef = hover_info.argument->data.blockdef->inputs[last_input].data.arg.blockdef;
        blockdef_add_text(arg_blockdef, str);
        arg_blockdef->func = block_custom_arg;

        int arg_count = 0;
        for (size_t i = 0; i < vector_size(hover_info.argument->data.blockdef->inputs); i++) {
            if (hover_info.argument->data.blockdef->inputs[i].type == INPUT_ARGUMENT) arg_count++;
        }
        arg_blockdef->arg_id = arg_count - 1;

        update_measurements(hover_info.block, PLACEMENT_HORIZONTAL);
        deselect_all();
        return true;
    case EDITOR_ADD_TEXT:
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
        blockdef_add_text(hover_info.argument->data.blockdef, str);
        update_measurements(hover_info.block, PLACEMENT_HORIZONTAL);
        deselect_all();
        return true;
    case EDITOR_DEL_ARG:
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
            for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
                if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
                blockdef->inputs[i].data.arg.blockdef->arg_id--;
            }
        }

        update_measurements(hover_info.block, PLACEMENT_HORIZONTAL);
        deselect_all();
        return true;
    case EDITOR_EDIT:
        if (hover_info.editor.edit_blockdef == hover_info.argument->data.blockdef) {
            hover_info.editor.edit_blockdef = NULL;
            hover_info.editor.edit_block = NULL;
        } else {
            hover_info.editor.edit_blockdef = hover_info.argument->data.blockdef;
            if (hover_info.editor.edit_block) update_measurements(hover_info.editor.edit_block, PLACEMENT_HORIZONTAL);
            hover_info.editor.edit_block = hover_info.block;
        }
        update_measurements(hover_info.block, PLACEMENT_HORIZONTAL);
        deselect_all();
        return true;
    case EDITOR_BLOCKDEF:
        if (hover_info.editor.edit_blockdef == hover_info.argument->data.blockdef) return false;
        blockchain_add_block(&mouse_blockchain, block_new_ms(hover_info.editor.blockdef));
        deselect_all();
        return true;
    default:
        return false;
    }
}

bool handle_code_editor_click(bool mouse_empty) {
    if (!mouse_empty) {
        mouse_blockchain.pos = as_scr_vec(GetMousePosition());
        if (hover_info.argument || hover_info.prev_argument) {
            if (vector_size(mouse_blockchain.blocks) > 1) return true;
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_CONTROLEND) return true;
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return true;

            if (hover_info.argument) {
                // Attach to argument
                printf("Attach to argument\n");
                if (hover_info.argument->type != ARGUMENT_TEXT) return true;
                mouse_blockchain.blocks[0].parent = hover_info.block;
                argument_set_block(hover_info.argument, mouse_blockchain.blocks[0]);
                update_measurements(&hover_info.argument->data.block, PLACEMENT_HORIZONTAL);
                vector_clear(mouse_blockchain.blocks);
                PlaySound(click_snd);
            } else if (hover_info.prev_argument) {
                // Swap argument
                printf("Swap argument\n");
                if (hover_info.prev_argument->type != ARGUMENT_BLOCK) return true;
                mouse_blockchain.blocks[0].parent = hover_info.block->parent;
                ScrBlock temp = mouse_blockchain.blocks[0];
                mouse_blockchain.blocks[0] = *hover_info.block;
                mouse_blockchain.blocks[0].parent = NULL;
                block_update_parent_links(&mouse_blockchain.blocks[0]);
                argument_set_block(hover_info.prev_argument, temp);
                update_measurements(temp.parent, PLACEMENT_HORIZONTAL);
                PlaySound(click_snd);
            }
        } else if (
            hover_info.block && 
            hover_info.blockchain && 
            hover_info.block->parent == NULL
        ) {
            // Attach block
            printf("Attach block\n");
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return true;
            blockchain_insert(hover_info.blockchain, &mouse_blockchain, hover_info.blockchain_index);
            // Update block link to make valgrind happy
            hover_info.block = &hover_info.blockchain->blocks[hover_info.blockchain_index];
            PlaySound(click_snd);
        } else {
            // Put block
            printf("Put block\n");
            mouse_blockchain.pos.x += camera_pos.x;
            mouse_blockchain.pos.y += camera_pos.y;
            blockcode_add_blockchain(&block_code, mouse_blockchain);
            mouse_blockchain = blockchain_new();
        }
        return true;
    } else if (hover_info.block) {
        if (hover_info.block->parent) {
            if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                // Copy argument
                printf("Copy argument\n");
                blockchain_add_block(&mouse_blockchain, block_copy(hover_info.block, NULL));
            } else {
                // Detach argument
                printf("Detach argument\n");
                assert(hover_info.prev_argument != NULL);

                blockchain_add_block(&mouse_blockchain, *hover_info.block);
                mouse_blockchain.blocks[0].parent = NULL;

                ScrBlock* parent = hover_info.prev_argument->data.block.parent;
                argument_set_text(hover_info.prev_argument, "");
                hover_info.prev_argument->ms.size = as_scr_vec(MeasureTextEx(font_cond, "", BLOCK_TEXT_SIZE, 0.0));
                update_measurements(parent, PLACEMENT_HORIZONTAL);
            }
        } else if (hover_info.blockchain) {
            if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Copy block
                    printf("Copy block\n");
                    blockchain_free(&mouse_blockchain);
                    mouse_blockchain = blockchain_copy_single(hover_info.blockchain, hover_info.blockchain_index);
                } else {
                    // Copy chain
                    printf("Copy chain\n");
                    blockchain_free(&mouse_blockchain);
                    mouse_blockchain = blockchain_copy(hover_info.blockchain, hover_info.blockchain_index);
                }
            } else {
                hover_info.editor.edit_blockdef = NULL;
                if (hover_info.editor.edit_block) update_measurements(hover_info.editor.edit_block, PLACEMENT_HORIZONTAL);
                hover_info.editor.edit_block = NULL;
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Detach block
                    printf("Detach block\n");
                    blockchain_detach_single(&mouse_blockchain, hover_info.blockchain, hover_info.blockchain_index);
                    if (vector_size(hover_info.blockchain->blocks) == 0) {
                        blockchain_free(hover_info.blockchain);
                        blockcode_remove_blockchain(&block_code, hover_info.blockchain - editor_code);
                        hover_info.block = NULL;
                    }
                } else {
                    // Detach chain
                    printf("Detach chain\n");
                    blockchain_detach(&mouse_blockchain, hover_info.blockchain, hover_info.blockchain_index);
                    if (hover_info.blockchain_index == 0) {
                        blockchain_free(hover_info.blockchain);
                        blockcode_remove_blockchain(&block_code, hover_info.blockchain - editor_code);
                        hover_info.block = NULL;
                    }
                }
            }
        }
        return true;
    }
    return false;
}

// Return value indicates if we should cancel dragging
bool handle_mouse_click(void) {
    hover_info.mouse_click_pos = GetMousePosition();
    camera_click_pos = camera_pos;

    if (gui.shown) {
        if (gui.type == GUI_TYPE_FILE) gui_hide_immediate();
        return true;
    }
    if (hover_info.top_bars.ind != -1) return handle_top_bar_click();
    if (current_tab != TAB_CODE) return true;
    if (vm.is_running) return false;

    bool mouse_empty = vector_size(mouse_blockchain.blocks) == 0;

    if (hover_info.sidebar) return handle_sidebar_click(mouse_empty);

    if (mouse_empty && hover_info.argument && hover_info.argument->type == ARGUMENT_BLOCKDEF) {
        if (handle_blockdef_editor_click()) return true;
    }

    if (mouse_empty) {
        if (hover_info.dropdown_hover_ind != -1) {
            ScrInput block_input = hover_info.select_block->blockdef->inputs[hover_info.select_argument->input_id];
            assert(block_input.type == INPUT_DROPDOWN);
            
            size_t list_len = 0;
            char** list = block_input.data.drop.list(hover_info.select_block, &list_len);
            assert((size_t)hover_info.dropdown_hover_ind < list_len);

            argument_set_const_string(hover_info.select_argument, list[hover_info.dropdown_hover_ind]);
            hover_info.select_argument->ms.size = as_scr_vec(MeasureTextEx(font_cond, list[hover_info.dropdown_hover_ind], BLOCK_TEXT_SIZE, 0.0));
            update_measurements(hover_info.select_block, PLACEMENT_HORIZONTAL);
        }

        if (hover_info.block != hover_info.select_block) hover_info.select_block = hover_info.block;
        if (hover_info.input != hover_info.select_input) hover_info.select_input = hover_info.input;
        if (hover_info.argument != hover_info.select_argument) {
            hover_info.select_argument = hover_info.argument;
            hover_info.select_argument_pos = hover_info.argument_pos;
            dropdown.scroll_amount = 0;
            return true;
        }
        if (hover_info.select_argument) return true;
    }

    if (handle_code_editor_click(mouse_empty)) return true;
    return false;
}

bool edit_text(char** text) {
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (vector_size(*text) <= 1) return false;

        int remove_pos = vector_size(*text) - 2;
        int remove_size = 1;

        while (((unsigned char)(*text)[remove_pos] >> 6) == 2) { // This checks if we are in the middle of UTF-8 char
            remove_pos--;
            remove_size++;
        }

        vector_erase(*text, remove_pos, remove_size);
        return true;
    }

    bool typed = false;
    int char_val;
    while ((char_val = GetCharPressed())) {
        int utf_size = 0;
        const char* utf_char = CodepointToUTF8(char_val, &utf_size);
        for (int i = 0; i < utf_size; i++) {
            vector_insert(text, vector_size(*text) - 1, utf_char[i]);
        }
        typed = true;
    }
    return typed;
}

void handle_key_press(void) {
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

            camera_pos.x = editor_code[blockchain_select_counter].pos.x - ((GetScreenWidth() - conf.side_bar_size) / 2 + conf.side_bar_size);
            camera_pos.y = editor_code[blockchain_select_counter].pos.y - ((GetScreenHeight() - conf.font_size * 2.2) / 2 + conf.font_size * 2.2);
            actionbar_show(TextFormat("Jump to chain (%d/%d)", blockchain_select_counter + 1, vector_size(editor_code)));
            return;
        }
        return;
    };
    if (hover_info.select_block->blockdef->inputs[hover_info.select_argument->input_id].type == INPUT_DROPDOWN) return;

    if (edit_text(hover_info.select_input)) {
        update_measurements(hover_info.select_block, PLACEMENT_HORIZONTAL);
        return;
    }
}

void handle_mouse_wheel(void) {
    int wheel = (int)GetMouseWheelMove();

    dropdown.scroll_amount = MAX(dropdown.scroll_amount - wheel, 0);
    if (hover_info.sidebar) {
        sidebar.scroll_amount = MAX(sidebar.scroll_amount - wheel * (conf.font_size + SIDE_BAR_PADDING) * 4, 0);
    }
}

void handle_mouse_drag(void) {
    if (hover_info.drag_cancelled) return;

    Vector2 mouse_pos = GetMousePosition();

    camera_pos.x = camera_click_pos.x - (mouse_pos.x - hover_info.mouse_click_pos.x);
    camera_pos.y = camera_click_pos.y - (mouse_pos.y - hover_info.mouse_click_pos.y);
}

void dropdown_check_collisions(void) {
    if (!hover_info.select_argument) return;

    ScrInput block_input = hover_info.select_block->blockdef->inputs[hover_info.select_argument->input_id];

    if (block_input.type != INPUT_DROPDOWN) return;

    dropdown.ms.size.x = hover_info.select_argument->ms.size.x;
    dropdown.ms.size.y = 5.0;

    size_t list_len = 0;
    char** list = block_input.data.drop.list(hover_info.select_block, &list_len);

    Vector2 pos = hover_info.select_argument_pos;
    pos.y += hover_info.select_block->ms.size.y;
    for (size_t i = dropdown.scroll_amount; i < list_len; i++) {
        if (pos.y > GetScreenHeight()) break;
        Vector2 text_ms = MeasureTextEx(font_cond, list[i], BLOCK_TEXT_SIZE, 0);
        dropdown.ms.size.x = MAX(text_ms.x + 10, dropdown.ms.size.x);
        dropdown.ms.size.y += conf.font_size;
        pos.y += conf.font_size;
    }

    pos = hover_info.select_argument_pos;
    pos.y += hover_info.select_block->ms.size.y;

    for (size_t i = dropdown.scroll_amount; i < list_len; i++) {
        if (pos.y > GetScreenHeight()) break;
        Rectangle rect;
        rect.x = pos.x;
        rect.y = pos.y;
        rect.width = dropdown.ms.size.x;
        rect.height = conf.font_size;

        if (CheckCollisionPointRec(GetMousePosition(), rect)) {
            hover_info.dropdown_hover_ind = i;
            break;
        }
        pos.y += conf.font_size;
    }
}

void check_block_collisions(void) {
    if (current_tab != TAB_CODE) return;
    if (hover_info.sidebar) {
        int pos_y = conf.font_size * 2.2 + SIDE_BAR_PADDING - sidebar.scroll_amount;
        for (vec_size_t i = 0; i < vector_size(sidebar.blocks); i++) {
            if (hover_info.block) break;
            block_update_collisions((Vector2){ SIDE_BAR_PADDING, pos_y }, &sidebar.blocks[i]);
            pos_y += conf.font_size + SIDE_BAR_PADDING;
        }
    } else {
        for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
            if (hover_info.block) break;
            blockchain_check_collisions(&editor_code[i], camera_pos);
        }
    }
}

void term_input_put_char(char ch) {
    pthread_mutex_lock(&out_win.lock);
    out_win.input_buf[out_win.buf_end] = ch;
    out_win.buf_end = (out_win.buf_end + 1) % TERM_INPUT_BUF_SIZE;
    pthread_mutex_unlock(&out_win.lock);
    sem_post(&out_win.input_sem);
}

char term_input_get_char(void) {
    sem_wait(&out_win.input_sem);
    pthread_mutex_lock(&out_win.lock);
    int out = out_win.input_buf[out_win.buf_start];
    out_win.buf_start = (out_win.buf_start + 1) % TERM_INPUT_BUF_SIZE;
    pthread_mutex_unlock(&out_win.lock);
    return out;
}

int leading_ones(unsigned char byte) {
    int out = 0;
    while (byte & 0x80) {
        out++;
        byte <<= 1;
    }
    return out;
}

void term_scroll_down(void) {
    pthread_mutex_lock(&out_win.lock);
    memmove(out_win.buffer, out_win.buffer + out_win.char_w, out_win.char_w * (out_win.char_h - 1) * sizeof(*out_win.buffer));
    for (int i = out_win.char_w * (out_win.char_h - 1); i < out_win.char_w * out_win.char_h; i++) strncpy(out_win.buffer[i], " ", ARRLEN(*out_win.buffer));
    pthread_mutex_unlock(&out_win.lock);
}

int term_print_str(const char* str) {
    int len = 0;
    pthread_mutex_lock(&out_win.lock);
    while (*str) {
        if (out_win.cursor_pos >= out_win.char_w * out_win.char_h) {
            out_win.cursor_pos = out_win.char_w * out_win.char_h - out_win.char_w;
            term_scroll_down();
        }
        if (*str == '\n') {
            out_win.cursor_pos += out_win.char_w;
            str++;
            if (out_win.cursor_pos >= out_win.char_w * out_win.char_h) {
                out_win.cursor_pos -= out_win.char_w;
                term_scroll_down();
            }
            continue;
        }
        if (*str == '\r') {
            out_win.cursor_pos -= out_win.cursor_pos % out_win.char_w;
            str++;
            continue;
        }

        int mb_size = leading_ones(*str);
        if (mb_size == 0) mb_size = 1;
        int i = 0;
        for (; i < mb_size; i++) out_win.buffer[out_win.cursor_pos][i] = str[i];
        out_win.buffer[out_win.cursor_pos][i] = 0;

        str += mb_size;
        out_win.cursor_pos++;
        len++;
    }
    pthread_mutex_unlock(&out_win.lock);

    return len;
}

int term_print_int(int value) {
    char converted[12];
    snprintf(converted, 12, "%d", value);
    return term_print_str(converted);
}

int term_print_double(double value) {
    char converted[20];
    snprintf(converted, 20, "%f", value);
    return term_print_str(converted);
}

void term_clear(void) {
    pthread_mutex_lock(&out_win.lock);
    for (int i = 0; i < out_win.char_w * out_win.char_h; i++) strncpy(out_win.buffer[i], " ", ARRLEN(*out_win.buffer));
    out_win.cursor_pos = 0;
    pthread_mutex_unlock(&out_win.lock);
}

void term_resize(void) {
    pthread_mutex_lock(&out_win.lock);
    Vector2 screen_size = (Vector2) { GetScreenWidth() - 20, GetScreenHeight() - conf.font_size * 2.2 - 20 };
    out_win.size = (Rectangle) { 0, 0, 16, 9 };
    if (out_win.size.width / out_win.size.height > screen_size.x / screen_size.y) {
        out_win.size.height *= screen_size.x / out_win.size.width;
        out_win.size.width  *= screen_size.x / out_win.size.width;
        out_win.size.y = screen_size.y / 2 - out_win.size.height / 2;
    } else {
        out_win.size.width  *= screen_size.y / out_win.size.height;
        out_win.size.height *= screen_size.y / out_win.size.height;
        out_win.size.x = screen_size.x / 2 - out_win.size.width / 2;
    }
    out_win.size.x += 10;
    out_win.size.y += conf.font_size * 2.2 + 10;

    out_win.char_size = MeasureTextEx(font_mono, "A", TERM_CHAR_SIZE, 0.0);
    Vector2 new_buffer_size = { out_win.size.width / out_win.char_size.x, out_win.size.height / out_win.char_size.y };

    if (out_win.char_w != (int)new_buffer_size.x || out_win.char_h != (int)new_buffer_size.y) {
        out_win.char_w = new_buffer_size.x;
        out_win.char_h = new_buffer_size.y;

        if (out_win.buffer) free(out_win.buffer);
        int buf_size = out_win.char_w * out_win.char_h * sizeof(*out_win.buffer);
        out_win.buffer = malloc(buf_size);
        term_clear();
    }
    pthread_mutex_unlock(&out_win.lock);
}

void sanitize_block(ScrBlock* block) {
    for (vec_size_t i = 0; i < vector_size(block->arguments); i++) {
        if (block->arguments[i].type != ARGUMENT_BLOCK) continue;
        if (block->arguments[i].data.block.parent != block) {
            printf("ERROR: Block %p detached from parent %p! (Got %p)\n", &block->arguments[i].data.block, block, block->arguments[i].data.block.parent);
            assert(false);
            return;
        }
        sanitize_block(&block->arguments[i].data.block);
    }
}

void sanitize_links(void) {
    for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
        ScrBlock* blocks = editor_code[i].blocks;
        for (vec_size_t j = 0; j < vector_size(blocks); j++) {
            sanitize_block(&blocks[j]);
        }
    }

    for (vec_size_t i = 0; i < vector_size(mouse_blockchain.blocks); i++) {
        sanitize_block(&mouse_blockchain.blocks[i]);
    }
}

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
    dst->fps_limit = src->fps_limit;
    SetTargetFPS(dst->fps_limit);
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
        if (file[cursor] == '\0') has_lines = false;
        file[cursor++] = '\0';

        printf("Field = \"%s\" Value = \"%s\"\n", field, value);
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
            printf("Unknown key: %s\n", field);
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
        printf("[LOAD] Unexpected EOF reading data\n");
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

    switch (arg->type) {
    case ARGUMENT_TEXT:
    case ARGUMENT_CONST_STRING:
        save_add_varint(save, save_find_id(arg->data.text));
        break;
    case ARGUMENT_BLOCK:
        save_block(save, &arg->data.block);
        break;
    case ARGUMENT_BLOCKDEF:
        save_add_varint(save, save_find_id(arg->data.blockdef->id));
        break;
    default:
        assert(false && "Unimplemented argument save");
        break;
    }
}

void save_block(SaveArena* save, ScrBlock* block) {
    assert(block->blockdef->id != NULL);

    int arg_count = vector_size(block->arguments);

    save_add_varint(save, save_find_id(block->blockdef->id));
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
        default:
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

    switch (input->type) {
    case INPUT_TEXT_DISPLAY:
        unsigned int text_len;
        char* text = save_read_array(save, sizeof(char), &text_len);
        if (!text) return false;
        if (text[text_len - 1] != 0) return false;

        input->data.stext.text = vector_create();
        input->data.stext.ms = (ScrMeasurement) {0};
        input->data.stext.editor_ms = (ScrMeasurement) {0};

        for (char* str = text; *str; str++) vector_add(&input->data.stext.text, *str);
        vector_add(&input->data.stext.text, 0);
        break;
    case INPUT_ARGUMENT:
        ScrInputArgumentConstraint constr; 
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
        printf("[LOAD] Unimplemented input load\n");
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

    switch (arg_type) {
    case ARGUMENT_TEXT:
    case ARGUMENT_CONST_STRING:
        unsigned int text_id;
        if (!save_read_varint(save, &text_id)) return false;

        arg->data.text = vector_create();
        for (char* str = (char*)save_block_ids[text_id]; *str; str++) vector_add(&arg->data.text, *str);
        vector_add(&arg->data.text, 0);
        break;
    case ARGUMENT_BLOCK:
        ScrBlock block;
        if (!load_block(save, &block)) return false;
        
        arg->data.block = block;
        break;
    case ARGUMENT_BLOCKDEF:
        unsigned int blockdef_id;
        if (!save_read_varint(save, &blockdef_id)) return false;

        ScrBlockdef* blockdef = find_blockdef(save_blockdefs, save_block_ids[blockdef_id]);
        if (!blockdef) return false;

        arg->data.blockdef = blockdef;
        arg->data.blockdef->ref_count++;
        break;
    default:
        printf("[LOAD] Unimplemented argument load\n");
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
            printf("[LOAD] No blockdef matched id: %s\n", save_block_ids[block_id]);
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
        printf("[LOAD] Unsupported version %d. Current scrap build expects save version 1\n", ver);
        goto load_fail;
    }

    unsigned int ident_len;
    char* ident = save_read_array(&save, sizeof(char), &ident_len);
    if (!ident) goto load_fail;
    if (ident_len == 0) goto load_fail;

    if (ident[ident_len - 1] != 0 || ident_len != sizeof(scrap_ident) || strncmp(ident, scrap_ident, sizeof(scrap_ident))) {
        printf("[LOAD] Not valid scrap save\n");
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

ScrData block_noop(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_NOTHING;
}

ScrData block_loop(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_OMIT_ARGS;
    if (argv[0].type != DATA_CONTROL) RETURN_OMIT_ARGS;

    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        control_stack_pop_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
    }

    RETURN_OMIT_ARGS;
}

ScrData block_if(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_BOOL(1);
    if (argv[0].type != DATA_CONTROL) RETURN_BOOL(1);
    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (!data_to_bool(argv[1])) {
            exec_set_skip_block(exec);
            control_stack_push_data((int)0, int)
        } else {
            control_stack_push_data((int)1, int)
        }
        RETURN_OMIT_ARGS;
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        int is_success = 0;
        control_stack_pop_data(is_success, int)
        RETURN_BOOL(is_success);
    }
    RETURN_BOOL(1);
}

ScrData block_else_if(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_BOOL(1);
    if (argv[0].type != DATA_CONTROL) RETURN_BOOL(1);
    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (argc < 3 || data_to_bool(argv[1])) {
            exec_set_skip_block(exec);
            control_stack_push_data((int)1, int)
        } else {
            int condition = data_to_bool(argv[2]);
            if (!condition) exec_set_skip_block(exec);
            control_stack_push_data(condition, int)
        }
        RETURN_OMIT_ARGS;
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        int is_success = 0;
        control_stack_pop_data(is_success, int)
        RETURN_BOOL(is_success);
    }
    RETURN_BOOL(1);
}

ScrData block_else(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_BOOL(1);
    if (argv[0].type != DATA_CONTROL) RETURN_BOOL(1);
    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (argc < 2 || data_to_bool(argv[1])) {
            exec_set_skip_block(exec);
        }
        RETURN_OMIT_ARGS;
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        RETURN_BOOL(1);
    }
    RETURN_BOOL(1);
}

// Visualization of control stack (stack grows downwards):
// - loop block index
// - cycles left to loop
// - 1 <- indicator for end block to do looping
//
// If the loop should not loop then the stack will look like this:
// - 0 <- indicator for end block that it should stop immediately
ScrData block_repeat(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_OMIT_ARGS;
    if (argv[0].type != DATA_CONTROL) RETURN_OMIT_ARGS;

    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        int cycles = data_to_int(argv[1]);
        if (cycles <= 0) {
            exec_set_skip_block(exec);
            control_stack_push_data((int)0, int) // This indicates the end block that it should NOT loop
            RETURN_OMIT_ARGS;
        }
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
        control_stack_push_data(cycles - 1, int)
        control_stack_push_data((int)1, int) // This indicates the end block that it should loop
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        int should_loop = 0;
        control_stack_pop_data(should_loop, int)
        if (!should_loop) RETURN_BOOL(0);

        int left = -1;
        control_stack_pop_data(left, int)
        if (left <= 0) {
            size_t bin;
            control_stack_pop_data(bin, size_t)
            (void) bin; // Cleanup stack
            RETURN_BOOL(1);
        }

        control_stack_pop_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
        control_stack_push_data(left - 1, int)
        control_stack_push_data((int)1, int)
    }

    RETURN_OMIT_ARGS;
}

ScrData block_while(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 2) RETURN_BOOL(0);
    if (argv[0].type != DATA_CONTROL) RETURN_BOOL(0);

    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (!data_to_bool(argv[1])) {
            exec_set_skip_block(exec);
            RETURN_OMIT_ARGS;
        }
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        if (!data_to_bool(argv[1])) {
            size_t bin;
            control_stack_pop_data(bin, size_t)
            (void) bin; 
            RETURN_BOOL(1);
        }

        control_stack_pop_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
        control_stack_push_data(exec->chain_stack[exec->chain_stack_len - 1].running_ind, size_t)
    }

    RETURN_NOTHING;
}

ScrData block_sleep(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    int usecs = data_to_int(argv[0]);
    if (usecs < 0) RETURN_INT(0);
    if (usleep(usecs)) RETURN_INT(0);
    RETURN_INT(usecs);
}

ScrData block_declare_var(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 2) RETURN_NOTHING;
    if (argv[0].type != DATA_STR || argv[0].storage.type != DATA_STORAGE_STATIC) RETURN_NOTHING;

    ScrData var_value = data_copy(argv[1]);
    if (var_value.storage.type == DATA_STORAGE_MANAGED) var_value.storage.type = DATA_STORAGE_UNMANAGED;

    variable_stack_push_var(exec, argv[0].data.str_arg, var_value);
    return var_value;
}

ScrData block_get_var(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_NOTHING;
    ScrVariable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    return var->value;
}

ScrData block_set_var(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 2) RETURN_NOTHING;

    ScrVariable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;

    ScrData new_value = data_copy(argv[1]);
    if (new_value.storage.type == DATA_STORAGE_MANAGED) new_value.storage.type = DATA_STORAGE_UNMANAGED;

    if (var->value.storage.type == DATA_STORAGE_UNMANAGED) {
        data_free(var->value);
    }

    var->value = new_value;
    return var->value;
}

ScrData block_create_list(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;

    ScrData out;
    out.type = DATA_LIST;
    out.storage.type = DATA_STORAGE_MANAGED;
    out.storage.storage_len = 0;
    out.data.list_arg.items = NULL;
    out.data.list_arg.len = 0;
    return out;
}

ScrData block_list_add(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;

    ScrVariable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    if (var->value.type != DATA_LIST) RETURN_NOTHING;

    if (!var->value.data.list_arg.items) {
        var->value.data.list_arg.items = malloc(sizeof(ScrData));
        var->value.data.list_arg.len = 1;
    } else {
        var->value.data.list_arg.items = realloc(var->value.data.list_arg.items, ++var->value.data.list_arg.len * sizeof(ScrData));
    }
    var->value.storage.storage_len = var->value.data.list_arg.len * sizeof(ScrData);
    ScrData* list_item = &var->value.data.list_arg.items[var->value.data.list_arg.len - 1];
    if (argv[1].storage.type == DATA_STORAGE_MANAGED) {
        argv[1].storage.type = DATA_STORAGE_UNMANAGED;
        *list_item = argv[1];
    } else {
        *list_item = data_copy(argv[1]);
        if (list_item->storage.type == DATA_STORAGE_MANAGED) list_item->storage.type = DATA_STORAGE_UNMANAGED;
    }

    return *list_item;
}

ScrData block_list_get(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;

    ScrVariable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    if (var->value.type != DATA_LIST) RETURN_NOTHING;
    if (!var->value.data.list_arg.items || var->value.data.list_arg.len == 0) RETURN_NOTHING;
    int index = data_to_int(argv[1]);
    if (index < 0 || (size_t)index >= var->value.data.list_arg.len) RETURN_NOTHING;

    return var->value.data.list_arg.items[index];
}

ScrData block_list_set(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 3) RETURN_NOTHING;

    ScrVariable* var = variable_stack_get_variable(exec, data_to_str(argv[0]));
    if (!var) RETURN_NOTHING;
    if (var->value.type != DATA_LIST) RETURN_NOTHING;
    if (!var->value.data.list_arg.items || var->value.data.list_arg.len == 0) RETURN_NOTHING;
    int index = data_to_int(argv[1]);
    if (index < 0 || (size_t)index >= var->value.data.list_arg.len) RETURN_NOTHING;

    ScrData new_value = data_copy(argv[2]);
    if (new_value.storage.type == DATA_STORAGE_MANAGED) new_value.storage.type = DATA_STORAGE_UNMANAGED;

    if (var->value.data.list_arg.items[index].storage.type == DATA_STORAGE_UNMANAGED) {
        data_free(var->value.data.list_arg.items[index]);
    }
    var->value.data.list_arg.items[index] = new_value;
    return var->value.data.list_arg.items[index];
}

ScrData block_print(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc >= 1) {
        int bytes_sent = 0;
        switch (argv[0].type) {
        case DATA_INT:
            bytes_sent = term_print_int(argv[0].data.int_arg);
            break;
        case DATA_BOOL:
            bytes_sent = term_print_str(argv[0].data.int_arg ? "true" : "false");
            break;
        case DATA_STR:
            bytes_sent = term_print_str(argv[0].data.str_arg);
            break;
        case DATA_DOUBLE:
            bytes_sent = term_print_double(argv[0].data.double_arg);
            break;
        case DATA_LIST:
            bytes_sent += term_print_str("[");
            if (argv[0].data.list_arg.items && argv[0].data.list_arg.len) {
                for (size_t i = 0; i < argv[0].data.list_arg.len; i++) {
                    bytes_sent += block_print(exec, 1, &argv[0].data.list_arg.items[i]).data.int_arg;
                    bytes_sent += term_print_str(", ");
                }
            }
            bytes_sent += term_print_str("]");
            break;
        default:
            break;
        }
        RETURN_INT(bytes_sent);
    }
    RETURN_INT(0);
}

ScrData block_println(ScrExec* exec, int argc, ScrData* argv) {
    ScrData out = block_print(exec, argc, argv);
    term_print_str("\r\n");
    return out;
}

ScrData block_cursor_x(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&out_win.lock);
    int cur_x = out_win.cursor_pos % out_win.char_w;
    pthread_mutex_unlock(&out_win.lock);
    RETURN_INT(cur_x);
}

ScrData block_cursor_y(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&out_win.lock);
    int cur_y = out_win.cursor_pos / out_win.char_w;
    pthread_mutex_unlock(&out_win.lock);
    RETURN_INT(cur_y);
}

ScrData block_cursor_max_x(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&out_win.lock);
    int cur_max_x = out_win.char_w;
    pthread_mutex_unlock(&out_win.lock);
    RETURN_INT(cur_max_x);
}

ScrData block_cursor_max_y(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    pthread_mutex_lock(&out_win.lock);
    int cur_max_y = out_win.char_h;
    pthread_mutex_unlock(&out_win.lock);
    RETURN_INT(cur_max_y);
}

ScrData block_set_cursor(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;
    pthread_mutex_lock(&out_win.lock);
    int x = CLAMP(data_to_int(argv[0]), 0, out_win.char_w - 1);
    int y = CLAMP(data_to_int(argv[1]), 0, out_win.char_h - 1);
    out_win.cursor_pos = x + y * out_win.char_w;
    pthread_mutex_unlock(&out_win.lock);
    RETURN_NOTHING;
}

ScrData block_term_clear(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    term_clear();
    RETURN_NOTHING;
}

ScrData block_input(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;

    ScrString string = string_new(0);
    char input_char = 0;

    while (input_char != '\n') {
        char input[256];
        int i = 0;
        for (; i < 255 && input_char != '\n'; i++) input[i] = (input_char = term_input_get_char());
        if (input[i - 1] == '\n') input[i - 1] = 0;
        input[i] = 0;
        string_add(&string, input);
    }

    return string_make_managed(&string);
}

ScrData block_get_char(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argv;
    (void) argc;

    ScrString string = string_new(0);
    char input[10];
    input[0] = term_input_get_char();
    int mb_size = leading_ones(input[0]);
    if (mb_size == 0) mb_size = 1;
    for (int i = 1; i < mb_size; i++) input[i] = term_input_get_char();
    input[mb_size] = 0;
    string_add(&string, input);

    return string_make_managed(&string);
}

ScrData block_random(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    int min = data_to_int(argv[0]);
    int max = data_to_int(argv[1]);
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    int val = GetRandomValue(min, max);
    RETURN_INT(val);
}

ScrData block_join(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;

    ScrString string = string_new(0);
    string_add(&string, data_to_str(argv[0]));
    string_add(&string, data_to_str(argv[1]));
    return string_make_managed(&string);
}

ScrData block_length(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    if (argv[0].type == DATA_LIST) RETURN_INT(argv[0].data.list_arg.len);
    int len = 0;
    const char* str = data_to_str(argv[0]);
    while (*str) {
        int mb_size = leading_ones(*str);
        if (mb_size == 0) mb_size = 1;
        str += mb_size;
        len++;
    }
    RETURN_INT(len);
}

ScrData block_unix_time(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_INT(time(NULL));
}

ScrData block_convert_int(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    RETURN_INT(data_to_int(argv[0]));
}

ScrData block_convert_float(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_DOUBLE(0.0);
    RETURN_DOUBLE(data_to_double(argv[0]));
}

ScrData block_convert_str(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    ScrString string = string_new(0);
    if (argc < 1) return string_make_managed(&string);
    string_add(&string, data_to_str(argv[0]));
    return string_make_managed(&string);
}

ScrData block_convert_bool(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    RETURN_BOOL(data_to_bool(argv[0]));
}

ScrData block_plus(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg + data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) + data_to_int(argv[1]));
    }
}

ScrData block_minus(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg - data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) - data_to_int(argv[1]));
    }
}

ScrData block_mult(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg * data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) * data_to_int(argv[1]));
    }
}

ScrData block_div(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(argv[0].data.double_arg / data_to_double(argv[1]));
    } else {
        RETURN_INT(data_to_int(argv[0]) / data_to_int(argv[1]));
    }
}

ScrData block_pow(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) RETURN_DOUBLE(pow(argv[0].data.double_arg, data_to_double(argv[1])));

    int base = data_to_int(argv[0]);
    unsigned int exp = data_to_int(argv[1]);
    if (!exp) RETURN_INT(1);

    int result = 1;
    while (exp) {
        if (exp & 1) result *= base;
        exp >>= 1;
        base *= base;
    }
    RETURN_INT(result);
}

ScrData block_math(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_DOUBLE(0.0);
    if (argv[0].type != DATA_STR) RETURN_DOUBLE(0.0);

    if (!strcmp(argv[0].data.str_arg, "sin")) {
        RETURN_DOUBLE(sin(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "cos")) {
        RETURN_DOUBLE(cos(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "tan")) {
        RETURN_DOUBLE(tan(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "asin")) {
        RETURN_DOUBLE(asin(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "acos")) {
        RETURN_DOUBLE(acos(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "atan")) {
        RETURN_DOUBLE(atan(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "sqrt")) {
        RETURN_DOUBLE(sqrt(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "round")) {
        RETURN_DOUBLE(round(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "floor")) {
        RETURN_DOUBLE(floor(data_to_double(argv[1])));
    } else if (!strcmp(argv[0].data.str_arg, "ceil")) {
        RETURN_DOUBLE(ceil(data_to_double(argv[1])));
    } else {
        RETURN_DOUBLE(0.0);
    }
}

ScrData block_pi(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_DOUBLE(M_PI);
}

ScrData block_bit_not(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(~0);
    RETURN_INT(~data_to_int(argv[0]));
}

ScrData block_bit_and(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    RETURN_INT(data_to_int(argv[0]) & data_to_int(argv[1]));
}

ScrData block_bit_xor(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    if (argc < 2) RETURN_INT(data_to_int(argv[0]));
    RETURN_INT(data_to_int(argv[0]) ^ data_to_int(argv[1]));
}

ScrData block_bit_or(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_INT(0);
    if (argc < 2) RETURN_INT(data_to_int(argv[0]));
    RETURN_INT(data_to_int(argv[0]) | data_to_int(argv[1]));
}

ScrData block_rem(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_DOUBLE(fmod(argv[0].data.double_arg, data_to_double(argv[1])));
    } else {
        RETURN_INT(data_to_int(argv[0]) % data_to_int(argv[1]));
    }
}

ScrData block_less(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) < 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg < data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) < data_to_int(argv[1]));
    }
}

ScrData block_less_eq(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) <= 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg <= data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) <= data_to_int(argv[1]));
    }
}

ScrData block_more(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) > 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg > data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) > data_to_int(argv[1]));
    }
}

ScrData block_more_eq(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(data_to_int(argv[0]) >= 0);
    if (argv[0].type == DATA_DOUBLE) {
        RETURN_BOOL(argv[0].data.double_arg >= data_to_double(argv[1]));
    } else {
        RETURN_BOOL(data_to_int(argv[0]) >= data_to_int(argv[1]));
    }
}

ScrData block_not(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(1);
    RETURN_BOOL(!data_to_bool(argv[0]));
}

ScrData block_and(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_BOOL(0);
    RETURN_BOOL(data_to_bool(argv[0]) && data_to_bool(argv[1]));
}

ScrData block_or(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_BOOL(0);
    RETURN_BOOL(data_to_bool(argv[0]) || data_to_bool(argv[1]));
}

ScrData block_true(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_BOOL(1);
}

ScrData block_false(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_BOOL(0);
}

ScrData block_eq(ScrExec* exec, int argc, ScrData* argv) {
    (void) exec;
    if (argc < 2) RETURN_BOOL(0);
    if (argv[0].type != argv[1].type) RETURN_BOOL(0);

    switch (argv[0].type) {
    case DATA_BOOL:
    case DATA_INT:
        RETURN_BOOL(argv[0].data.int_arg == argv[1].data.int_arg);
    case DATA_DOUBLE:
        RETURN_BOOL(argv[0].data.double_arg == argv[1].data.double_arg);
    case DATA_STR:
        RETURN_BOOL(!strcmp(argv[0].data.str_arg, argv[1].data.str_arg));
    case DATA_NOTHING:
        RETURN_BOOL(1);
    default:
        RETURN_BOOL(0);
    }
}

ScrData block_not_eq(ScrExec* exec, int argc, ScrData* argv) {
    ScrData out = block_eq(exec, argc, argv);
    out.data.int_arg = !out.data.int_arg;
    return out;
}

ScrData block_exec_custom(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_NOTHING;
    if (argv[0].type != DATA_CHAIN) RETURN_NOTHING;
    ScrData return_val;
    exec_run_custom(exec, argv[0].data.chain_arg, argc - 1, argv + 1, &return_val);
    return return_val;
}

ScrData block_custom_arg(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_NOTHING;
    if (argv[0].type != DATA_INT) RETURN_NOTHING;
    if (argv[0].data.int_arg >= exec->chain_stack[exec->chain_stack_len - 1].custom_argc) RETURN_NOTHING;
    return data_copy(exec->chain_stack[exec->chain_stack_len - 1].custom_argv[argv[0].data.int_arg]);
}

ScrData block_return(ScrExec* exec, int argc, ScrData* argv) {
    if (argc < 1) RETURN_NOTHING;
    exec->chain_stack[exec->chain_stack_len - 1].return_arg = data_copy(argv[0]);
    exec->chain_stack[exec->chain_stack_len - 1].is_returning = true;
    RETURN_NOTHING;
}

Texture2D load_svg(const char* path) {
    Image svg_img = LoadImageSvg(path, conf.font_size, conf.font_size);
    Texture2D texture = LoadTextureFromImage(svg_img);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    UnloadImage(svg_img);
    return texture;
}

void setup(void) {
    run_tex = LoadTexture(DATA_PATH "run.png");
    SetTextureFilter(run_tex, TEXTURE_FILTER_BILINEAR);
    drop_tex = LoadTexture(DATA_PATH "drop.png");
    SetTextureFilter(drop_tex, TEXTURE_FILTER_BILINEAR);
    close_tex = LoadTexture(DATA_PATH "close.png");
    SetTextureFilter(close_tex, TEXTURE_FILTER_BILINEAR);

    logo_tex = load_svg(DATA_PATH "logo.svg");
    warn_tex = load_svg(DATA_PATH "warning.svg");
    stop_tex = load_svg(DATA_PATH "stop.svg");
    edit_tex = load_svg(DATA_PATH "edit.svg");
    close_tex = load_svg(DATA_PATH "close.svg");
    term_tex = load_svg(DATA_PATH "term.svg");
    add_arg_tex = load_svg(DATA_PATH "add_arg.svg");
    del_arg_tex = load_svg(DATA_PATH "del_arg.svg");
    add_text_tex = load_svg(DATA_PATH "add_text.svg");
    special_tex = load_svg(DATA_PATH "special.svg");
    list_tex = load_svg(DATA_PATH "list.svg");
    logo_tex_nuc = TextureToNuklear(logo_tex);
    warn_tex_nuc = TextureToNuklear(warn_tex);

    int codepoints_count;
    int *codepoints = LoadCodepoints(conf.font_symbols, &codepoints_count);
    font_cond = LoadFontEx(conf.font_path, conf.font_size, codepoints, codepoints_count);
    font_eb = LoadFontEx(conf.font_bold_path, conf.font_size, codepoints, codepoints_count);
    font_mono = LoadFontEx(conf.font_mono_path, conf.font_size, codepoints, codepoints_count);
    UnloadCodepoints(codepoints);

    SetTextureFilter(font_cond.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(font_eb.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(font_mono.texture, TEXTURE_FILTER_BILINEAR);

    click_snd = LoadSound(DATA_PATH "click.wav");

    line_shader = LoadShaderFromMemory(line_shader_vertex, line_shader_fragment);
    shader_time_loc = GetShaderLocation(line_shader, "time");

    vm = vm_new();

    ScrBlockdef* on_start = blockdef_new("on_start", BLOCKTYPE_HAT, (ScrColor) { 0xff, 0x77, 0x00, 0xFF }, block_noop);
    blockdef_add_text(on_start, "When");
    blockdef_add_image(on_start, (ScrImage) { .image_ptr = &run_tex });
    blockdef_add_text(on_start, "clicked");
    blockdef_register(&vm, on_start);

    ScrBlockdef* sc_input = blockdef_new("input", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xff }, block_input);
    blockdef_add_image(sc_input, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_input, "Get input");
    blockdef_register(&vm, sc_input);

    ScrBlockdef* sc_char = blockdef_new("get_char", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xff }, block_get_char);
    blockdef_add_image(sc_char, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_char, "Get char");
    blockdef_register(&vm, sc_char);

    ScrBlockdef* sc_print = blockdef_new("print", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_print);
    blockdef_add_image(sc_print, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_print, "Print");
    blockdef_add_argument(sc_print, "Привет, мусороид!", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_print);

    ScrBlockdef* sc_println = blockdef_new("println", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_println);
    blockdef_add_image(sc_println, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_println, "Print line");
    blockdef_add_argument(sc_println, "Привет, мусороид!", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_println);

    ScrBlockdef* sc_cursor_x = blockdef_new("cursor_x", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_cursor_x);
    blockdef_add_image(sc_cursor_x, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_x, "Cursor X");
    blockdef_register(&vm, sc_cursor_x);

    ScrBlockdef* sc_cursor_y = blockdef_new("cursor_y", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_cursor_y);
    blockdef_add_image(sc_cursor_y, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_y, "Cursor Y");
    blockdef_register(&vm, sc_cursor_y);

    ScrBlockdef* sc_cursor_max_x = blockdef_new("cursor_max_x", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_cursor_max_x);
    blockdef_add_image(sc_cursor_max_x, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_max_x, "Terminal width");
    blockdef_register(&vm, sc_cursor_max_x);

    ScrBlockdef* sc_cursor_max_y = blockdef_new("cursor_max_y", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_cursor_max_y);
    blockdef_add_image(sc_cursor_max_y, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_cursor_max_y, "Terminal height");
    blockdef_register(&vm, sc_cursor_max_y);

    ScrBlockdef* sc_set_cursor = blockdef_new("set_cursor", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_set_cursor);
    blockdef_add_image(sc_set_cursor, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_set_cursor, "Set cursor X:");
    blockdef_add_argument(sc_set_cursor, "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_set_cursor, "Y:");
    blockdef_add_argument(sc_set_cursor, "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_set_cursor);

    ScrBlockdef* sc_term_clear = blockdef_new("term_clear", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_term_clear);
    blockdef_add_image(sc_term_clear, (ScrImage) { .image_ptr = &term_tex });
    blockdef_add_text(sc_term_clear, "Clear terminal");
    blockdef_register(&vm, sc_term_clear);

    ScrBlockdef* sc_loop = blockdef_new("loop", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_loop);
    blockdef_add_text(sc_loop, "Loop");
    blockdef_register(&vm, sc_loop);

    ScrBlockdef* sc_repeat = blockdef_new("repeat", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_repeat);
    blockdef_add_text(sc_repeat, "Repeat");
    blockdef_add_argument(sc_repeat, "10", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_repeat, "times");
    blockdef_register(&vm, sc_repeat);

    ScrBlockdef* sc_while = blockdef_new("while", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_while);
    blockdef_add_text(sc_while, "While");
    blockdef_add_argument(sc_while, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_while);

    ScrBlockdef* sc_if = blockdef_new("if", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_if);
    blockdef_add_text(sc_if, "If");
    blockdef_add_argument(sc_if, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_if, ", then");
    blockdef_register(&vm, sc_if);

    ScrBlockdef* sc_else_if = blockdef_new("else_if", BLOCKTYPE_CONTROLEND, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_else_if);
    blockdef_add_text(sc_else_if, "Else if");
    blockdef_add_argument(sc_else_if, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_else_if, ", then");
    blockdef_register(&vm, sc_else_if);

    ScrBlockdef* sc_else = blockdef_new("else", BLOCKTYPE_CONTROLEND, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_else);
    blockdef_add_text(sc_else, "Else");
    blockdef_register(&vm, sc_else);

    ScrBlockdef* sc_do_nothing = blockdef_new("do_nothing", BLOCKTYPE_CONTROL, (ScrColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_do_nothing, "Do nothing");
    blockdef_register(&vm, sc_do_nothing);

    ScrBlockdef* sc_sleep = blockdef_new("sleep", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_sleep);
    blockdef_add_text(sc_sleep, "Sleep");
    blockdef_add_argument(sc_sleep, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_sleep, "us");
    blockdef_register(&vm, sc_sleep);

    ScrBlockdef* sc_end = blockdef_new("end", BLOCKTYPE_END, (ScrColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_end, "End");
    blockdef_register(&vm, sc_end);

    ScrBlockdef* sc_plus = blockdef_new("plus", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_plus);
    blockdef_add_argument(sc_plus, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_plus, "+");
    blockdef_add_argument(sc_plus, "10", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_plus);

    ScrBlockdef* sc_minus = blockdef_new("minus", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_minus);
    blockdef_add_argument(sc_minus, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_minus, "-");
    blockdef_add_argument(sc_minus, "10", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_minus);

    ScrBlockdef* sc_mult = blockdef_new("mult", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_mult);
    blockdef_add_argument(sc_mult, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_mult, "*");
    blockdef_add_argument(sc_mult, "10", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_mult);

    ScrBlockdef* sc_div = blockdef_new("div", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_div);
    blockdef_add_argument(sc_div, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_div, "/");
    blockdef_add_argument(sc_div, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_div);

    ScrBlockdef* sc_pow = blockdef_new("pow", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_pow);
    blockdef_add_text(sc_pow, "Pow");
    blockdef_add_argument(sc_pow, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_add_argument(sc_pow, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_pow);

    ScrBlockdef* sc_math = blockdef_new("math", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xff }, block_math);
    blockdef_add_dropdown(sc_math, DROPDOWN_SOURCE_LISTREF, math_list_access);
    blockdef_add_argument(sc_math, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_math);

    ScrBlockdef* sc_pi = blockdef_new("pi", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xff }, block_pi);
    blockdef_add_text(sc_pi, "Pi");
    blockdef_register(&vm, sc_pi);

    ScrBlockdef* sc_bit_not = blockdef_new("bit_not", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_bit_not);
    blockdef_add_text(sc_bit_not, "~");
    blockdef_add_argument(sc_bit_not, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_bit_not);

    ScrBlockdef* sc_bit_and = blockdef_new("bit_and", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_bit_and);
    blockdef_add_argument(sc_bit_and, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_and, "&");
    blockdef_add_argument(sc_bit_and, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_bit_and);

    ScrBlockdef* sc_bit_or = blockdef_new("bit_or", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_bit_or);
    blockdef_add_argument(sc_bit_or, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_or, "|");
    blockdef_add_argument(sc_bit_or, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_bit_or);

    ScrBlockdef* sc_bit_xor = blockdef_new("bit_xor", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_bit_xor);
    blockdef_add_argument(sc_bit_xor, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_bit_xor, "^");
    blockdef_add_argument(sc_bit_xor, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_bit_xor);

    ScrBlockdef* sc_rem = blockdef_new("rem", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_rem);
    blockdef_add_argument(sc_rem, "39", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_rem, "%");
    blockdef_add_argument(sc_rem, "5", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_rem);

    ScrBlockdef* sc_less = blockdef_new("less", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_less);
    blockdef_add_argument(sc_less, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_less, "<");
    blockdef_add_argument(sc_less, "11", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_less);

    ScrBlockdef* sc_less_eq = blockdef_new("less_eq", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_less_eq);
    blockdef_add_argument(sc_less_eq, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_less_eq, "<=");
    blockdef_add_argument(sc_less_eq, "11", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_less_eq);

    ScrBlockdef* sc_eq = blockdef_new("eq", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_eq);
    blockdef_add_argument(sc_eq, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_eq, "=");
    blockdef_add_argument(sc_eq, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_eq);

    ScrBlockdef* sc_not_eq = blockdef_new("not_eq", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_not_eq);
    blockdef_add_argument(sc_not_eq, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_not_eq, "!=");
    blockdef_add_argument(sc_not_eq, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_not_eq);

    ScrBlockdef* sc_more_eq = blockdef_new("more_eq", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_more_eq);
    blockdef_add_argument(sc_more_eq, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_more_eq, ">=");
    blockdef_add_argument(sc_more_eq, "11", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_more_eq);

    ScrBlockdef* sc_more = blockdef_new("more", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_more);
    blockdef_add_argument(sc_more, "9", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_more, ">");
    blockdef_add_argument(sc_more, "11", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_more);

    ScrBlockdef* sc_not = blockdef_new("not", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_not);
    blockdef_add_text(sc_not, "Not");
    blockdef_add_argument(sc_not, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_not);

    ScrBlockdef* sc_and = blockdef_new("and", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_and);
    blockdef_add_argument(sc_and, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_and, "and");
    blockdef_add_argument(sc_and, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_and);

    ScrBlockdef* sc_or = blockdef_new("or", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_or);
    blockdef_add_argument(sc_or, "", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_or, "or");
    blockdef_add_argument(sc_or, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_or);

    ScrBlockdef* sc_true = blockdef_new("true", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_true);
    blockdef_add_text(sc_true, "True");
    blockdef_register(&vm, sc_true);

    ScrBlockdef* sc_false = blockdef_new("false", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_false);
    blockdef_add_text(sc_false, "False");
    blockdef_register(&vm, sc_false);

    ScrBlockdef* sc_random = blockdef_new("random", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_random);
    blockdef_add_text(sc_random, "Random");
    blockdef_add_argument(sc_random, "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_random, "to");
    blockdef_add_argument(sc_random, "10", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_random);

    ScrBlockdef* sc_join = blockdef_new("join", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_join);
    blockdef_add_text(sc_join, "Join");
    blockdef_add_argument(sc_join, "абоба ", BLOCKCONSTR_UNLIMITED);
    blockdef_add_argument(sc_join, "мусор", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_join);

    ScrBlockdef* sc_length = blockdef_new("length", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_length);
    blockdef_add_text(sc_length, "Length");
    blockdef_add_argument(sc_length, "мусорка", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_length);

    ScrBlockdef* sc_unix_time = blockdef_new("unix_time", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0x99, 0xff, 0xff }, block_unix_time);
    blockdef_add_text(sc_unix_time, "Time since 1970");
    blockdef_register(&vm, sc_unix_time);

    ScrBlockdef* sc_int = blockdef_new("convert_int", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0x99, 0xff, 0xff }, block_convert_int);
    blockdef_add_text(sc_int, "Int");
    blockdef_add_argument(sc_int, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_int);

    ScrBlockdef* sc_float = blockdef_new("convert_float", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0x99, 0xff, 0xff }, block_convert_float);
    blockdef_add_text(sc_float, "Float");
    blockdef_add_argument(sc_float, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_float);

    ScrBlockdef* sc_str = blockdef_new("convert_str", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0x99, 0xff, 0xff }, block_convert_str);
    blockdef_add_text(sc_str, "Str");
    blockdef_add_argument(sc_str, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_str);

    ScrBlockdef* sc_bool = blockdef_new("convert_bool", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0x99, 0xff, 0xff }, block_convert_bool);
    blockdef_add_text(sc_bool, "Bool");
    blockdef_add_argument(sc_bool, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_bool);

    ScrBlockdef* sc_nothing = blockdef_new("nothing", BLOCKTYPE_NORMAL, (ScrColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_nothing, "Nothing");
    blockdef_register(&vm, sc_nothing);

    ScrBlockdef* sc_comment = blockdef_new("comment", BLOCKTYPE_NORMAL, (ScrColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    blockdef_add_text(sc_comment, "//");
    blockdef_add_argument(sc_comment, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_comment);

    ScrBlockdef* sc_decl_var = blockdef_new("decl_var", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x77, 0x00, 0xff }, block_declare_var);
    blockdef_add_text(sc_decl_var, "Declare");
    blockdef_add_argument(sc_decl_var, "my variable", BLOCKCONSTR_STRING);
    blockdef_add_text(sc_decl_var, "=");
    blockdef_add_argument(sc_decl_var, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_decl_var);

    ScrBlockdef* sc_get_var = blockdef_new("get_var", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x77, 0x00, 0xff }, block_get_var);
    blockdef_add_text(sc_get_var, "Get");
    blockdef_add_argument(sc_get_var, "my variable", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_get_var);

    ScrBlockdef* sc_set_var = blockdef_new("set_var", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x77, 0x00, 0xff }, block_set_var);
    blockdef_add_text(sc_set_var, "Set");
    blockdef_add_argument(sc_set_var, "my variable", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_set_var, "=");
    blockdef_add_argument(sc_set_var, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_set_var);

    ScrBlockdef* sc_create_list = blockdef_new("create_list", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x44, 0x00, 0xff }, block_create_list);
    blockdef_add_image(sc_create_list, (ScrImage) { .image_ptr = &list_tex });
    blockdef_add_text(sc_create_list, "Empty list");
    blockdef_register(&vm, sc_create_list);

    ScrBlockdef* sc_list_add = blockdef_new("list_add", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x44, 0x00, 0xff }, block_list_add);
    blockdef_add_image(sc_list_add, (ScrImage) { .image_ptr = &list_tex });
    blockdef_add_text(sc_list_add, "Add");
    blockdef_add_argument(sc_list_add, "my variable", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_add, "value");
    blockdef_add_argument(sc_list_add, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_list_add);

    ScrBlockdef* sc_list_get = blockdef_new("list_get", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x44, 0x00, 0xff }, block_list_get);
    blockdef_add_image(sc_list_get, (ScrImage) { .image_ptr = &list_tex });
    blockdef_add_argument(sc_list_get, "my variable", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_get, "get at");
    blockdef_add_argument(sc_list_get, "0", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_list_get);

    ScrBlockdef* sc_list_set = blockdef_new("list_set", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x44, 0x00, 0xff }, block_list_set);
    blockdef_add_image(sc_list_set, (ScrImage) { .image_ptr = &list_tex });
    blockdef_add_argument(sc_list_set, "my variable", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_set, "set at");
    blockdef_add_argument(sc_list_set, "0", BLOCKCONSTR_UNLIMITED);
    blockdef_add_text(sc_list_set, "=");
    blockdef_add_argument(sc_list_set, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_list_set);

    ScrBlockdef* sc_define_block = blockdef_new("define_block", BLOCKTYPE_HAT, (ScrColor) { 0x99, 0x00, 0xff, 0xff }, block_noop);
    blockdef_add_image(sc_define_block, (ScrImage) { .image_ptr = &special_tex });
    blockdef_add_text(sc_define_block, "Define");
    blockdef_add_blockdef_editor(sc_define_block);
    blockdef_register(&vm, sc_define_block);

    ScrBlockdef* sc_return = blockdef_new("return", BLOCKTYPE_NORMAL, (ScrColor) { 0x99, 0x00, 0xff, 0xff }, block_return);
    blockdef_add_image(sc_return, (ScrImage) { .image_ptr = &special_tex });
    blockdef_add_text(sc_return, "Return");
    blockdef_add_argument(sc_return, "", BLOCKCONSTR_UNLIMITED);
    blockdef_register(&vm, sc_return);

    mouse_blockchain = blockchain_new();
    draw_stack = vector_create();
    editor_code = vector_create();

    sem_init(&out_win.input_sem, 0, 0);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&out_win.lock, &attr);
    pthread_mutexattr_destroy(&attr);
    term_resize();

    sidebar.blocks = vector_create();
    for (vec_size_t i = 0; i < vector_size(vm.blockdefs); i++) {
        if (vm.blockdefs[i]->hidden) continue;
        vector_add(&sidebar.blocks, block_new_ms(vm.blockdefs[i]));
    }

    sidebar.max_y = conf.font_size * 2.2 + SIDE_BAR_PADDING;
    for (vec_size_t i = 0; i < vector_size(sidebar.blocks); i++) {
        sidebar.max_y += sidebar.blocks[i].ms.size.y + SIDE_BAR_PADDING;
    }

    font_eb_nuc = LoadFontIntoNuklear(font_eb, conf.font_size);
    font_cond_nuc = LoadFontIntoNuklear(font_cond, conf.font_size * 0.6);
    gui.is_fading = true;
    gui.ctx = InitNuklearEx(font_cond_nuc, &line_shader);

    gui.ctx->style.text.color = nk_rgb(0xff, 0xff, 0xff);

    gui.ctx->style.window.fixed_background.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.window.fixed_background.data.color = nk_rgb(0x20, 0x20, 0x20);
    gui.ctx->style.window.background = nk_rgb(0x20, 0x20, 0x20);
    gui.ctx->style.window.border_color = nk_rgb(0x60, 0x60, 0x60);
    gui.ctx->style.window.padding = nk_vec2(0, 0);

    gui.ctx->style.button.text_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.button.text_hover = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.button.text_active = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.button.rounding = 0.0;
    gui.ctx->style.button.border = 1.0;
    gui.ctx->style.button.border_color = nk_rgb(0x60, 0x60, 0x60);
    gui.ctx->style.button.normal.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.button.normal.data.color = nk_rgb(0x30, 0x30, 0x30);
    gui.ctx->style.button.hover.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.button.hover.data.color = nk_rgb(0x40, 0x40, 0x40);
    gui.ctx->style.button.active.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.button.active.data.color = nk_rgb(0x20, 0x20, 0x20);

    gui.ctx->style.slider.bar_normal = nk_rgb(0x30, 0x30, 0x30);
    gui.ctx->style.slider.bar_hover = nk_rgb(0x30, 0x30, 0x30);
    gui.ctx->style.slider.bar_active = nk_rgb(0x30, 0x30, 0x30);
    gui.ctx->style.slider.bar_filled = nk_rgb(0xaa, 0xaa, 0xaa);
    gui.ctx->style.slider.cursor_normal.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.slider.cursor_normal.data.color = nk_rgb(0xaa, 0xaa, 0xaa);
    gui.ctx->style.slider.cursor_hover.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.slider.cursor_hover.data.color = nk_rgb(0xdd, 0xdd, 0xdd);
    gui.ctx->style.slider.cursor_active.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.slider.cursor_active.data.color = nk_rgb(0xff, 0xff, 0xff);

    gui.ctx->style.edit.normal.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.edit.normal.data.color = nk_rgb(0x30, 0x30, 0x30);
    gui.ctx->style.edit.hover.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.edit.hover.data.color = nk_rgb(0x40, 0x40, 0x40);
    gui.ctx->style.edit.active.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.edit.active.data.color = nk_rgb(0x28, 0x28, 0x28);
    gui.ctx->style.edit.rounding = 0.0;
    gui.ctx->style.edit.border = 1.0;
    gui.ctx->style.edit.border_color = nk_rgb(0x60, 0x60, 0x60);
    gui.ctx->style.edit.text_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.edit.text_hover = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.edit.text_active = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.edit.selected_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.edit.selected_hover = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.edit.selected_text_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.edit.selected_text_hover = nk_rgb(0x20, 0x20, 0x20);
    gui.ctx->style.edit.cursor_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.edit.cursor_hover = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.edit.cursor_text_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.edit.cursor_text_hover = nk_rgb(0x20, 0x20, 0x20);

    gui.ctx->style.property.normal.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.property.normal.data.color = nk_rgb(0x30, 0x30, 0x30);
    gui.ctx->style.property.hover.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.property.hover.data.color = nk_rgb(0x40, 0x40, 0x40);
    gui.ctx->style.property.active.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.property.active.data.color = nk_rgb(0x40, 0x40, 0x40);
    gui.ctx->style.property.label_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.label_hover = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.label_active = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.rounding = 0.0;
    gui.ctx->style.property.border = 1.0;
    gui.ctx->style.property.border_color = nk_rgb(0x60, 0x60, 0x60);

    gui.ctx->style.property.inc_button.normal.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.property.inc_button.normal.data.color = nk_rgba(0x00, 0x00, 0x00, 0x00);
    gui.ctx->style.property.inc_button.text_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.inc_button.text_hover = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.inc_button.text_active = nk_rgb(0xff, 0xff, 0xff);

    gui.ctx->style.property.dec_button.normal.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.property.dec_button.normal.data.color = nk_rgba(0x00, 0x00, 0x00, 0x00);
    gui.ctx->style.property.dec_button.text_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.dec_button.text_hover = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.dec_button.text_active = nk_rgb(0xff, 0xff, 0xff);

    gui.ctx->style.property.edit.rounding = 0.0;
    gui.ctx->style.property.edit.normal.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.property.edit.normal.data.color = nk_rgba(0x00, 0x00, 0x00, 0x00);
    gui.ctx->style.property.edit.hover.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.property.edit.hover.data.color = nk_rgba(0x00, 0x00, 0x00, 0x00);
    gui.ctx->style.property.edit.active.type = NK_STYLE_ITEM_COLOR;
    gui.ctx->style.property.edit.active.data.color = nk_rgba(0x00, 0x00, 0x00, 0x00);
    gui.ctx->style.property.edit.text_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.edit.text_hover = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.edit.text_active = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.edit.selected_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.edit.selected_hover = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.edit.selected_text_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.edit.selected_text_hover = nk_rgb(0x20, 0x20, 0x20);
    gui.ctx->style.property.edit.cursor_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.edit.cursor_hover = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.edit.cursor_text_normal = nk_rgb(0xff, 0xff, 0xff);
    gui.ctx->style.property.edit.cursor_text_hover = nk_rgb(0x20, 0x20, 0x20);
}

int main(void) {
    set_default_config(&conf);
    load_config(&conf);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(800, 600, "Scrap");
    InitAudioDevice();
    SetTargetFPS(conf.fps_limit);
    SetWindowState(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);

    setup();

    while (!WindowShouldClose()) {
        hover_info.sidebar = GetMouseX() < conf.side_bar_size && GetMouseY() > conf.font_size * 2.2;
        hover_info.block = NULL;
        hover_info.argument = NULL;
        hover_info.input = NULL;
        hover_info.argument_pos.x = 0;
        hover_info.argument_pos.y = 0;
        hover_info.prev_argument = NULL;
        hover_info.blockchain = NULL;
        hover_info.blockchain_index = -1;
        hover_info.blockchain_layer = 0;
        hover_info.dropdown_hover_ind = -1;
        hover_info.top_bars.ind = -1;
        hover_info.exec_ind = -1;
        hover_info.exec_chain = NULL;
        hover_info.editor.part = EDITOR_NONE;
        hover_info.editor.blockdef = NULL;
        hover_info.editor.blockdef_input = -1;

        Vector2 mouse_pos = GetMousePosition();
        if ((int)hover_info.last_mouse_pos.x == (int)mouse_pos.x && (int)hover_info.last_mouse_pos.y == (int)mouse_pos.y) {
            hover_info.time_at_last_pos += GetFrameTime();
        } else {
            hover_info.last_mouse_pos = mouse_pos;
            hover_info.time_at_last_pos = 0;
        }

        dropdown_check_collisions();
        if (!gui.shown) {
            check_block_collisions();
            bars_check_collisions();
        }

        if (gui.shown) UpdateNuklear(gui.ctx);
        handle_gui();

        if (GetMouseWheelMove() != 0.0) {
            handle_mouse_wheel();
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
            handle_key_press();
        }

        if (IsWindowResized()) {
            shader_time = 0.0;
            term_resize();
        }

        if (sidebar.max_y > GetScreenHeight()) {
            sidebar.scroll_amount = MIN(sidebar.scroll_amount, sidebar.max_y - GetScreenHeight());
        } else {
            sidebar.scroll_amount = 0;
        }

        mouse_blockchain.pos = as_scr_vec(GetMousePosition());

        actionbar.show_time -= GetFrameTime();
        if (actionbar.show_time < 0) actionbar.show_time = 0;

        if (shader_time_loc != -1) SetShaderValue(line_shader, shader_time_loc, &shader_time, SHADER_UNIFORM_FLOAT);
        shader_time += GetFrameTime() / 2.0;
        if (shader_time >= 1.0) shader_time = 1.0;

        // I have no idea why, but this code may occasionally crash X server, so it is turned off for now
        /*if (hover_info.argument || hover_info.select_argument) {
            SetMouseCursor(MOUSE_CURSOR_IBEAM);
        } else if (hover_info.block) {
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        } else {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }*/

        size_t vm_return = -1;
        if (exec_try_join(&vm, &exec, &vm_return)) {
            if (vm_return == 1) {
                actionbar_show("Vm executed successfully");
            } else if (vm_return == (size_t)PTHREAD_CANCELED) {
                actionbar_show("Vm stopped >:(");
            } else {
                actionbar_show("Vm shitted and died :(");
            }
            exec_free(&exec);
        } else if (vm.is_running) {
            hover_info.exec_chain = exec.running_chain;
            hover_info.exec_ind = exec.chain_stack[exec.chain_stack_len - 1].running_ind;
        }

        BeginDrawing();
        ClearBackground(GetColor(0x202020ff));

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        DrawRectangle(0, 0, sw, conf.font_size * 1.2, (Color){ 0x30, 0x30, 0x30, 0xFF });
        DrawRectangle(0, conf.font_size * 1.2, sw, conf.font_size, (Color){ 0x2B, 0x2B, 0x2B, 0xFF });
        draw_tab_buttons(sw);
        draw_top_bar();

        if (current_tab == TAB_CODE) {
            BeginScissorMode(0, conf.font_size * 2.2, sw, sh - conf.font_size * 2.2);
                draw_dots();
                for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
                    draw_block_chain(&editor_code[i], camera_pos, hover_info.exec_chain == &editor_code[i]);
                }
            EndScissorMode();

            draw_scrollbars();

            draw_sidebar();

            BeginScissorMode(0, conf.font_size * 2.2, sw, sh - conf.font_size * 2.2);
                draw_block_chain(&mouse_blockchain, (Vector2) {0}, false);
            EndScissorMode();

            draw_action_bar();

#ifdef DEBUG
            DrawTextEx(
                font_cond, 
                TextFormat(
                    "BlockChain: %p, Ind: %d, Layer: %d\n"
                    "Block: %p, Parent: %p\n"
                    "Argument: %p, Pos: (%.3f, %.3f)\n"
                    "Prev argument: %p\n"
                    "Select block: %p\n"
                    "Select arg: %p, Pos: (%.3f, %.3f)\n"
                    "Sidebar: %d\n"
                    "Mouse: %p, Time: %.3f, Pos: (%d, %d), Click: (%d, %d)\n"
                    "Camera: (%.3f, %.3f), Click: (%.3f, %.3f)\n"
                    "Dropdown ind: %d, Scroll: %d\n"
                    "Drag cancelled: %d\n"
                    "Bar: %d, Ind: %d\n"
                    "Min: (%.3f, %.3f), Max: (%.3f, %.3f)\n"
                    "Sidebar scroll: %d, Max: %d\n"
                    "Editor: %d, Editing: %p, Blockdef: %p, input: %zu\n",
                    hover_info.blockchain,
                    hover_info.blockchain_index,
                    hover_info.blockchain_layer,
                    hover_info.block,
                    hover_info.block ? hover_info.block->parent : NULL,
                    hover_info.argument, hover_info.argument_pos.x, hover_info.argument_pos.y, 
                    hover_info.prev_argument,
                    hover_info.select_block,
                    hover_info.select_argument, hover_info.select_argument_pos.x, hover_info.select_argument_pos.y, 
                    hover_info.sidebar,
                    mouse_blockchain.blocks,
                    hover_info.time_at_last_pos,
                    (int)mouse_pos.x, (int)mouse_pos.y,
                    (int)hover_info.mouse_click_pos.x, (int)hover_info.mouse_click_pos.y,
                    camera_pos.x, camera_pos.y, camera_click_pos.x, camera_click_pos.y,
                    hover_info.dropdown_hover_ind, dropdown.scroll_amount,
                    hover_info.drag_cancelled,
                    hover_info.top_bars.type, hover_info.top_bars.ind,
                    block_code.min_pos.x, block_code.min_pos.y, block_code.max_pos.x, block_code.max_pos.y,
                    sidebar.scroll_amount, sidebar.max_y,
                    hover_info.editor.part, hover_info.editor.edit_blockdef, hover_info.editor.blockdef, hover_info.editor.blockdef_input
                ), 
                (Vector2){ 
                    conf.side_bar_size + 5, 
                    conf.font_size * 2.2 + 5
                }, 
                conf.font_size * 0.5,
                0.0, 
                GRAY
            );
#else
            Vector2 debug_pos = (Vector2) {
                conf.side_bar_size + 5 * conf.font_size / 32.0, 
                conf.font_size * 2.2 + 5 * conf.font_size / 32.0,
            };
            DrawTextEx(font_cond, "Scrap v" SCRAP_VERSION, debug_pos, conf.font_size * 0.5, 0.0, (Color) { 0xff, 0xff, 0xff, 0x40 });
            debug_pos.y += conf.font_size * 0.5;
            DrawTextEx(font_cond, TextFormat("FPS: %d, Frame time: %.3f", GetFPS(), GetFrameTime()), debug_pos, conf.font_size * 0.5, 0.0, (Color) { 0xff, 0xff, 0xff, 0x40 });
#endif
        } else if (current_tab == TAB_OUTPUT) {
            draw_term();
        }

        if (gui.shown) {
            float animation_ease = ease_out_expo(gui.animation_time);
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 0x00, 0x00, 0x00, 0x44 * animation_ease });
            DrawNuklear(gui.ctx);
        }

        draw_dropdown_list();
        draw_tooltip();

        EndDrawing();
    }

    if (vm.is_running) {
        exec_stop(&vm, &exec);
        size_t bin;
        exec_join(&vm, &exec, &bin);
        exec_free(&exec);
    }
    pthread_mutex_destroy(&out_win.lock);
    sem_destroy(&out_win.input_sem);
    vector_free(draw_stack);
    UnloadNuklear(gui.ctx);
    blockchain_free(&mouse_blockchain);
    for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
        blockchain_free(&editor_code[i]);
    }
    vector_free(editor_code);
    
    for (vec_size_t i = 0; i < vector_size(sidebar.blocks); i++) {
        block_free(&sidebar.blocks[i]);
    }
    vector_free(sidebar.blocks);
    vm_free(&vm);
    CloseWindow();

    return 0;
}
