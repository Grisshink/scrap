// TODO:
// - Better collision resolution
// - Swap blocks inside arguments?
// - Hat blocks
// - Move print output to window

#define LICENSE_URL "https://www.gnu.org/licenses/gpl-3.0.html"

#define SCRVM_IMPLEMENTATION
#define SCRVM_VEC_C
#include "vm.h"

#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#define RAYLIB_NUKLEAR_IMPLEMENTATION
#include "external/raylib-nuklear.h"

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MOD(x, y) (((x) % (y) + (y)) % (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))
#define LERP(min, max, t) (((max) - (min)) * (t) + (min))
#define UNLERP(min, max, v) (((float)(v) - (float)(min)) / ((float)(max) - (float)(min)))

#define ACTION_BAR_MAX_SIZE 128
#define FONT_PATH_MAX_SIZE 256
#define FONT_SYMBOLS_MAX_SIZE 1024
#define CONFIG_PATH "config.txt"

#define BLOCK_TEXT_SIZE (conf.font_size * 0.6)
#define DATA_PATH "data/"
#define BLOCK_PADDING (5.0 * (float)conf.font_size / 32.0)
#define BLOCK_OUTLINE_SIZE (2.0 * (float)conf.font_size / 32.0)
#define BLOCK_STRING_PADDING (10.0 * (float)conf.font_size / 32.0)
#define BLOCK_CONTROL_INDENT (16.0 * (float)conf.font_size / 32.0)
#define SIDE_BAR_PADDING (10.0 * (float)conf.font_size / 32.0)
#define DROP_TEX_WIDTH ((float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)drop_tex.height * (float)drop_tex.width)

typedef struct {
    int font_size;
    int side_bar_size;
    char font_symbols[FONT_SYMBOLS_MAX_SIZE];
    char font_path[FONT_PATH_MAX_SIZE];
    char font_bold_path[FONT_PATH_MAX_SIZE];
} Config;

typedef enum {
    TOPBAR_TOP,
    TOPBAR_TABS,
    TOPBAR_RUN_BUTTON,
} TopBarType;

typedef struct {
    TopBarType type;
    int ind;
} TopBars;

typedef struct {
    bool sidebar;
    ScrBlockChain* blockchain;
    vec_size_t blockchain_index;
    int blockchain_layer;
    ScrBlock* block;
    ScrBlockArgument* argument;
    Vector2 argument_pos;
    ScrBlockArgument* prev_argument;
    ScrBlock* select_block;
    ScrBlockArgument* select_argument;
    Vector2 select_argument_pos;
    Vector2 last_mouse_pos;
    Vector2 mouse_click_pos;
    float time_at_last_pos;
    int dropdown_hover_ind;
    bool drag_cancelled;
    TopBars top_bars;
    size_t exec_chain_ind;
    size_t exec_ind;
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
} NuklearGuiType;

typedef struct {
    bool shown;
    float animation_time;
    bool is_fading;
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

char* top_bar_buttons_text[] = {
    "File",
    "Settings",
    "About",
};

char* tab_bar_buttons_text[] = {
    "Code",
    "Output",
};

Config conf;
Config gui_conf;

Sound click_snd;

Texture2D run_tex;
Texture2D stop_tex;
Texture2D drop_tex;
Texture2D close_tex;
Texture2D logo_tex;
Texture2D warn_tex;
struct nk_image logo_tex_nuc;
struct nk_image warn_tex_nuc;

Font font_cond;
Font font_eb;
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

Vector2 camera_pos = {0};
Vector2 camera_click_pos = {0};
int blockchain_select_counter = -1;

char* keys_list[] = {
    "Space", "Enter",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", 
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", 
    "U", "V", "W", "X", "Y", "Z",
};

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

char** keys_accessor(ScrBlock* block, size_t* list_len) {
    (void)block;
    *list_len = ARRLEN(keys_list);
    return keys_list;
}

void save_config(Config* config);
void apply_config(Config* dst, Config* src);
void set_default_config(Config* config);
void update_measurements(ScrVm* vm, ScrBlock* block);

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

ScrBlock block_new_ms(ScrVm* vm, int id) {
    ScrBlock block = block_new(vm, id);
    update_measurements(vm, &block);
    return block;
}

void update_measurements(ScrVm* vm, ScrBlock* block) {
    ScrBlockdef blockdef = vm->blockdefs[block->id];

    block->ms.size.x = BLOCK_PADDING;
    block->ms.size.y = conf.font_size;

    int arg_id = 0;
    for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        ScrMeasurement ms;

        switch (blockdef.inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            ms = blockdef.inputs[i].data.stext.ms;
            break;
        case INPUT_ARGUMENT:
            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                ScrMeasurement string_ms;
                string_ms.size = as_scr_vec(MeasureTextEx(font_cond, block->arguments[arg_id].data.text, BLOCK_TEXT_SIZE, 0.0));
                string_ms.size.x += BLOCK_STRING_PADDING;
                string_ms.size.x = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, string_ms.size.x);

                block->arguments[arg_id].ms = string_ms;
                ms = string_ms;
                ms.size.y = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, ms.size.y);
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
            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
                ScrMeasurement string_ms;
                string_ms.size = as_scr_vec(MeasureTextEx(font_cond, block->arguments[arg_id].data.text, BLOCK_TEXT_SIZE, 0.0));
                string_ms.size.x += BLOCK_STRING_PADDING + DROP_TEX_WIDTH;
                string_ms.size.x = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, string_ms.size.x);

                block->arguments[arg_id].ms = string_ms;
                ms = string_ms;
                ms.size.y = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, ms.size.y);
                break;
            case ARGUMENT_TEXT:
            case ARGUMENT_BLOCK:
                assert(false && "Illegal argument type");
                break;
            default:
                assert(false && "Unimplemented argument measure");
                break;
            }
            arg_id++;
            break;
        case INPUT_IMAGE_DISPLAY:
            ms = blockdef.inputs[i].data.simage.ms;
            break;
        default:
            ms.size = as_scr_vec(MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0));
            break;
        }
        ms.size.x += BLOCK_PADDING;

        block->ms.size.x += ms.size.x;
        block->ms.size.y = MAX(block->ms.size.y, ms.size.y + BLOCK_OUTLINE_SIZE * 4);
    }

    if (block->parent) update_measurements(vm, block->parent);
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

    ScrBlockdef blockdef = vm.blockdefs[block->id];
    int arg_id = 0;

    for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        if (hover_info.argument) return;
        int width = 0;
        ScrBlockInput cur = blockdef.inputs[i];
        Rectangle arg_size;

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = cur.data.stext.ms.size.x;
            break;
        case INPUT_ARGUMENT:
            width = block->arguments[arg_id].ms.size.x;

            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5;
                arg_size.width = block->arguments[arg_id].ms.size.x;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.argument = &block->arguments[arg_id];
                    hover_info.argument_pos = cursor;
                    break;
                }
                break;
            case ARGUMENT_BLOCK:
                Vector2 block_pos;
                block_pos.x = cursor.x;
                block_pos.y = cursor.y + block_size.height / 2 - block->arguments[arg_id].ms.size.y / 2; 

                arg_size.x = block_pos.x;
                arg_size.y = block_pos.y;
                arg_size.width = block->arguments[arg_id].ms.size.x;
                arg_size.height = block->arguments[arg_id].ms.size.y;

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
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5;
                arg_size.width = block->arguments[arg_id].ms.size.x;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

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
        case INPUT_IMAGE_DISPLAY:
            width = cur.data.simage.ms.size.x;
            break;
        default:
            width = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0).x;
            break;
        }
        cursor.x += width + BLOCK_PADDING;
    }
}

void draw_text_shadow(Font font, const char *text, Vector2 position, float font_size, float spacing, Color tint, Color shadow) {
    DrawTextEx(font, text, (Vector2) { position.x + 1, position.y + 1 }, font_size, spacing, shadow);
    DrawTextEx(font, text, position, font_size, spacing, tint);
}

void draw_block(Vector2 position, ScrBlock* block, bool force_outline, bool force_collision) {
    bool collision = hover_info.block == block || force_collision;
    ScrBlockdef blockdef = vm.blockdefs[block->id];
    Color color = as_rl_color(blockdef.color);
    Color outline_color = force_collision ? YELLOW : ColorBrightness(color, collision ? 0.5 : -0.2);

    Vector2 cursor = position;

    Rectangle block_size;
    block_size.x = position.x;
    block_size.y = position.y;
    block_size.width = block->ms.size.x;
    block_size.height = block->ms.size.y;

    DrawRectangleRec(block_size, ColorBrightness(color, collision ? 0.3 : 0.0));
    if (force_outline || (blockdef.type != BLOCKTYPE_CONTROL && blockdef.type != BLOCKTYPE_CONTROLEND)) {
        DrawRectangleLinesEx(block_size, BLOCK_OUTLINE_SIZE, outline_color);
    }
    cursor.x += BLOCK_PADDING;

    int arg_id = 0;
    for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        int width = 0;
        ScrBlockInput cur = blockdef.inputs[i];

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = cur.data.stext.ms.size.x;
            draw_text_shadow(
                font_cond, 
                cur.data.stext.text, 
                (Vector2) { 
                    cursor.x, 
                    cursor.y + block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5, 
                },
                BLOCK_TEXT_SIZE,
                0.0,
                WHITE,
                (Color) { 0x00, 0x00, 0x00, 0x88 }
            );
            break;
        case INPUT_ARGUMENT:
            width = block->arguments[arg_id].ms.size.x;

            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                Rectangle arg_size;
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5;
                arg_size.width = width;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

                bool hovered = &block->arguments[arg_id] == hover_info.argument;
                bool selected = &block->arguments[arg_id] == hover_info.select_argument;

                if (block->arguments[arg_id].type == ARGUMENT_CONST_STRING) {
                    DrawRectangleRounded(arg_size, 0.5, 5, WHITE);
                    if (hovered || selected) {
                        DrawRectangleRoundedLines(arg_size, 0.5, 5, BLOCK_OUTLINE_SIZE, ColorBrightness(color, selected ? -0.5 : 0.5));
                    }
                } else if (block->arguments[arg_id].type == ARGUMENT_TEXT) {
                    DrawRectangleRec(arg_size, WHITE);
                    if (hovered || selected) {
                        DrawRectangleLinesEx(arg_size, BLOCK_OUTLINE_SIZE, ColorBrightness(color, selected ? -0.5 : 0.2));
                    }
                } 
                DrawTextEx(
                    font_cond, 
                    block->arguments[arg_id].data.text,
                    (Vector2) { 
                        cursor.x + BLOCK_STRING_PADDING * 0.5, 
                        cursor.y + block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5, 
                    },
                    BLOCK_TEXT_SIZE,
                    0.0,
                    BLACK
                );
                break;
            case ARGUMENT_BLOCK:
                Vector2 block_pos;
                block_pos.x = cursor.x;
                block_pos.y = cursor.y + block_size.height * 0.5 - block->arguments[arg_id].ms.size.y * 0.5;

                draw_block(block_pos, &block->arguments[arg_id].data.block, true, force_collision);
                break;
            default:
                assert(false && "Unimplemented argument draw");
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            width = block->arguments[arg_id].ms.size.x;

            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
                Rectangle arg_size;
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5;
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
                        cursor.y + block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5, 
                    },
                    BLOCK_TEXT_SIZE,
                    0.0,
                    WHITE,
                    (Color) { 0x00, 0x00, 0x00, 0x88 }
                );
                DrawTextureEx(
                    drop_tex,
                    (Vector2) { 
                        cursor.x + ms.x + BLOCK_STRING_PADDING * 0.5 + 1,
                        cursor.y + BLOCK_OUTLINE_SIZE * 2 + 1,
                    }, 
                    0.0, 
                    (float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)drop_tex.height, 
                    (Color) { 0x00, 0x00, 0x00, 0x88 }
                );

                DrawTextureEx(
                    drop_tex,
                    (Vector2) { 
                        cursor.x + ms.x + BLOCK_STRING_PADDING * 0.5,
                        cursor.y + BLOCK_OUTLINE_SIZE * 2,
                    }, 
                    0.0, 
                    (float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)drop_tex.height, 
                    WHITE
                );

                break;
            case ARGUMENT_TEXT:
                assert(false && "Illegal argument type ARGUMENT_TEXT");
                break;
            case ARGUMENT_BLOCK:
                assert(false && "Illegal argument type ARGUMENT_BLOCK");
                break;
            default:
                assert(false && "Unimplemented argument draw");
                break;
            }
            arg_id++;
            break;
        case INPUT_IMAGE_DISPLAY:
            Texture2D* image = cur.data.simage.image.image_ptr;
            width = cur.data.simage.ms.size.x;
            DrawTextureEx(
                *image, 
                (Vector2) { 
                    cursor.x + 1, 
                    cursor.y + BLOCK_OUTLINE_SIZE * 2 + 1,
                }, 
                0.0, 
                (float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)image->height, 
                (Color) { 0x00, 0x00, 0x00, 0x88 }
            );
            DrawTextureEx(
                *image, 
                (Vector2) { 
                    cursor.x, 
                    cursor.y + BLOCK_OUTLINE_SIZE * 2,
                }, 
                0.0, 
                (float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)image->height, 
                WHITE
            );
            break;
        default:
            width = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0).x;
            DrawTextEx(
                font_cond, 
                "NODEF",
                (Vector2) { 
                    cursor.x, 
                    cursor.y + block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5, 
                },
                BLOCK_TEXT_SIZE, 
                0.0, 
                RED
            );
            break;
        }

        cursor.x += width + BLOCK_PADDING;
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
    ScrBlockType blocktype = vm.blockdefs[block->block->id].type;
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
    ScrBlockType blocktype = vm.blockdefs[block->block->id].type;
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

        ScrBlockdef blockdef = vm.blockdefs[chain->blocks[i].id];
        if ((blockdef.type == BLOCKTYPE_END || blockdef.type == BLOCKTYPE_CONTROLEND) && vector_size(draw_stack) > 0) {
            pos.x -= BLOCK_CONTROL_INDENT;

            if (blockdef.type == BLOCKTYPE_END) {
                DrawStack prev_block = draw_stack[vector_size(draw_stack) - 1];
                Rectangle rect;
                rect.x = pos.x;
                rect.y = pos.y;
                rect.width = prev_block.block->ms.size.x;
                rect.height = conf.font_size;

                if (CheckCollisionPointRec(GetMousePosition(), rect)) {
                    hover_info.block = &hover_info.blockchain->blocks[i];
                }
            } else if (blockdef.type == BLOCKTYPE_CONTROLEND) {
                block_update_collisions(pos, &hover_info.blockchain->blocks[i]);
            }
            vector_pop(draw_stack);
        } else {
            block_update_collisions(pos, &hover_info.blockchain->blocks[i]);
        }

        if (blockdef.type == BLOCKTYPE_CONTROL || blockdef.type == BLOCKTYPE_CONTROLEND) {
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
        ScrBlockdef blockdef = vm.blockdefs[chain->blocks[i].id];

        if ((blockdef.type == BLOCKTYPE_END || blockdef.type == BLOCKTYPE_CONTROLEND) && vector_size(draw_stack) > 0) {
            pos.x -= BLOCK_CONTROL_INDENT;
            DrawStack prev_block = draw_stack[vector_size(draw_stack) - 1];
            ScrBlockdef prev_blockdef = vm.blockdefs[prev_block.block->id];

            Rectangle rect;
            rect.x = prev_block.pos.x;
            rect.y = prev_block.pos.y + prev_block.block->ms.size.y;
            rect.width = BLOCK_CONTROL_INDENT;
            rect.height = pos.y - (prev_block.pos.y + prev_block.block->ms.size.y);
            DrawRectangleRec(rect, as_rl_color(prev_blockdef.color));

            bool touching_block = hover_info.block == &chain->blocks[i];
            Color outline_color = ColorBrightness(as_rl_color(prev_blockdef.color), hover_info.block == prev_block.block || touching_block ? 0.5 : -0.2);
            if (blockdef.type == BLOCKTYPE_END) {
                Color end_color = ColorBrightness(as_rl_color(prev_blockdef.color), exec_highlight || touching_block ? 0.3 : 0.0);
                DrawRectangle(pos.x, pos.y, prev_block.block->ms.size.x, conf.font_size, end_color);
                draw_control_outline(&prev_block, pos, outline_color, true);
            } else if (blockdef.type == BLOCKTYPE_CONTROLEND) {
                draw_block(pos, &chain->blocks[i], false, exec_highlight);
                draw_controlend_outline(&prev_block, pos, outline_color);
            }

            vector_pop(draw_stack);
        } else {
            draw_block(pos, &chain->blocks[i], false, exec_highlight);
        }
        if (blockdef.type == BLOCKTYPE_CONTROL || blockdef.type == BLOCKTYPE_CONTROLEND) {
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
        ScrBlockdef prev_blockdef = vm.blockdefs[prev_block.block->id];

        pos.x = prev_block.pos.x;

        rect.x = prev_block.pos.x;
        rect.y = prev_block.pos.y + prev_block.block->ms.size.y;
        rect.width = BLOCK_CONTROL_INDENT;
        rect.height = pos.y - (prev_block.pos.y + prev_block.block->ms.size.y);

        DrawRectangleRec(rect, as_rl_color(prev_blockdef.color));
        draw_control_outline(&prev_block, pos, ColorBrightness(as_rl_color(prev_blockdef.color), hover_info.block == prev_block.block ? 0.5 : -0.2), false);
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
        if (button_check_collisions(&pos, tab_bar_buttons_text[i], 1.0, 0.3, 0)) {
            hover_info.top_bars.type = TOPBAR_TABS;
            hover_info.top_bars.ind = i;
            return;
        }
    }

    Vector2 run_pos = (Vector2){ GetScreenWidth() - conf.font_size * 2.0, conf.font_size * 1.2 };
    for (int i = 0; i < 2; i++) {
        if (button_check_collisions(&run_pos, NULL, 1.0, 0.5, 0)) {
            hover_info.top_bars.type = TOPBAR_RUN_BUTTON;
            hover_info.top_bars.ind = i;
            return;
        }
    }

    int width = MeasureTextEx(font_eb, "Scrap", conf.font_size * 0.8, 0.0).x;
    pos = (Vector2){ 20 + conf.font_size + width, 0 };
    for (vec_size_t i = 0; i < ARRLEN(top_bar_buttons_text); i++) {
        if (button_check_collisions(&pos, top_bar_buttons_text[i], 1.2, 0.3, 0)) {
            hover_info.top_bars.type = TOPBAR_TOP;
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
    DrawTextureEx(stop_tex, run_pos, 0, (float)conf.font_size / (float)run_tex.width, WHITE);
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

    ScrBlockdef blockdef = vm.blockdefs[hover_info.select_block->id];
    ScrBlockInput block_input = blockdef.inputs[hover_info.select_argument->input_id];

    if (block_input.type != INPUT_DROPDOWN) return;
    
    Vector2 pos;
    pos = hover_info.select_argument_pos;
    pos.y += hover_info.select_block->ms.size.y;

    DrawRectangle(pos.x, pos.y, dropdown.ms.size.x, dropdown.ms.size.y, ColorBrightness(as_rl_color(blockdef.color), -0.3));
    if (hover_info.dropdown_hover_ind != -1) {
        DrawRectangle(pos.x, pos.y + (hover_info.dropdown_hover_ind - dropdown.scroll_amount) * conf.font_size, dropdown.ms.size.x, conf.font_size, as_rl_color(blockdef.color));
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
        pos_y += conf.font_size + SIDE_BAR_PADDING;
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

void draw_output_box(void) {
    Vector2 screen_size = (Vector2) { GetScreenWidth() - 20, GetScreenHeight() - conf.font_size * 2.2 - 20 };
    Rectangle rect = (Rectangle) { 0, 0, 16, 9 };
    if (rect.width / rect.height > screen_size.x / screen_size.y) {
        rect.height *= screen_size.x / rect.width;
        rect.width  *= screen_size.x / rect.width;
        rect.y = screen_size.y / 2 - rect.height / 2;
    } else {
        rect.width  *= screen_size.y / rect.height;
        rect.height *= screen_size.y / rect.height;
        rect.x = screen_size.x / 2 - rect.width / 2;
    }
    rect.x += 10;
    rect.y += conf.font_size * 2.2 + 10;

    DrawRectangleRec(rect, BLACK);
    BeginShaderMode(line_shader);
    DrawRectangleLinesEx(rect, 2.0, (Color) { 0x60, 0x60, 0x60, 0xff });
    EndShaderMode();
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
    shader_time = -0.2;
}

void gui_hide(void) {
    gui.is_fading = true;
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
    if (gui.is_fading) {
        gui.animation_time = MAX(gui.animation_time - GetFrameTime() * 2.0, 0.0);
        if (gui.animation_time == 0.0) gui.shown = false;
    } else {
        gui.shown = true;
        gui.animation_time = MIN(gui.animation_time + GetFrameTime() * 2.0, 1.0);
    }

    if (!gui.shown) return;

    float animation_ease = ease_out_expo(gui.animation_time);

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
            nk_label(gui.ctx, "Font path", NK_TEXT_RIGHT);
            gui_restart_warning();
            nk_edit_string_zero_terminated(gui.ctx, NK_EDIT_FIELD, gui_conf.font_path, FONT_PATH_MAX_SIZE, nk_filter_default);
            nk_spacer(gui.ctx);

            nk_spacer(gui.ctx);
            nk_label(gui.ctx, "Bold font path", NK_TEXT_RIGHT);
            gui_restart_warning();
            nk_edit_string_zero_terminated(gui.ctx, NK_EDIT_FIELD, gui_conf.font_bold_path, FONT_PATH_MAX_SIZE, nk_filter_default);
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
            nk_label(gui.ctx, "Scrap v0.1", NK_TEXT_LEFT);
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
    default:
        break;
    }
}

// Return value indicates if we should cancel dragging
bool handle_mouse_click(void) {
    hover_info.mouse_click_pos = GetMousePosition();
    camera_click_pos = camera_pos;

    if (gui.shown) {
        return true;
    }

    if (hover_info.top_bars.ind != -1) {
        if (hover_info.top_bars.type == TOPBAR_TOP) {
            switch (hover_info.top_bars.ind) {
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
                exec = exec_new(&vm);
                exec_copy_code(&vm, &exec, editor_code);
                if (exec_start(&vm, &exec)) {
                    actionbar_show("Started successfully!");
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

    if (current_tab != TAB_CODE) {
        return true;
    }

    if (vm.is_running) return false;

    bool mouse_empty = vector_size(mouse_blockchain.blocks) == 0;

    if (hover_info.sidebar) {
        if (hover_info.select_argument) {
            hover_info.select_argument = NULL;
            hover_info.select_argument_pos.x = 0;
            hover_info.select_argument_pos.y = 0;
            dropdown.scroll_amount = 0;
            return true;
        }
        if (mouse_empty && hover_info.block) {
            // Pickup block
            blockchain_add_block(&mouse_blockchain, block_new_ms(&vm, hover_info.block->id));
            if (vm.blockdefs[hover_info.block->id].type == BLOCKTYPE_CONTROL && vm.end_block_id != (size_t)-1) {
                blockchain_add_block(&mouse_blockchain, block_new_ms(&vm, vm.end_block_id));
            }
            return true;
        } else if (!mouse_empty) {
            // Drop block
            blockchain_clear_blocks(&mouse_blockchain);
            return true;
        }
        return true;
    }

    if (mouse_empty) {
        if (hover_info.dropdown_hover_ind != -1) {
            ScrBlockdef blockdef = vm.blockdefs[hover_info.select_block->id];
            ScrBlockInput block_input = blockdef.inputs[hover_info.select_argument->input_id];
            assert(block_input.type == INPUT_DROPDOWN);
            
            size_t list_len = 0;
            char** list = block_input.data.drop.list(hover_info.select_block, &list_len);
            assert((size_t)hover_info.dropdown_hover_ind < list_len);

            argument_set_const_string(hover_info.select_argument, list[hover_info.dropdown_hover_ind]);
            hover_info.select_argument->ms.size = as_scr_vec(MeasureTextEx(font_cond, list[hover_info.dropdown_hover_ind], BLOCK_TEXT_SIZE, 0.0));
            update_measurements(&vm, hover_info.select_block);
        }

        if (hover_info.block != hover_info.select_block) {
            hover_info.select_block = hover_info.block;
        }

        if (hover_info.argument != hover_info.select_argument) {
            hover_info.select_argument = hover_info.argument;
            hover_info.select_argument_pos = hover_info.argument_pos;
            dropdown.scroll_amount = 0;
            return true;
        }

        if (hover_info.select_argument) {
            return true;
        }
    }

    if (!mouse_empty) {
        mouse_blockchain.pos = as_scr_vec(GetMousePosition());
        if (hover_info.argument) {
            // Attach to argument
            printf("Attach to argument\n");
            if (vector_size(mouse_blockchain.blocks) > 1) return true;
            if (vm.blockdefs[mouse_blockchain.blocks[0].id].type == BLOCKTYPE_CONTROLEND) return true;
            if (hover_info.argument->type != ARGUMENT_TEXT) return true;
            mouse_blockchain.blocks[0].parent = hover_info.block;
            argument_set_block(hover_info.argument, mouse_blockchain.blocks[0]);
            update_measurements(&vm, &hover_info.argument->data.block);
            vector_clear(mouse_blockchain.blocks);
            PlaySound(click_snd);
        } else if (
            hover_info.block && 
            hover_info.blockchain && 
            hover_info.block->parent == NULL
        ) {
            // Attach block
            printf("Attach block\n");
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
            // Detach argument
            printf("Detach argument\n");
            assert(hover_info.prev_argument != NULL);

            blockchain_add_block(&mouse_blockchain, *hover_info.block);
            mouse_blockchain.blocks[0].parent = NULL;

            ScrBlock* parent = hover_info.prev_argument->data.block.parent;
            argument_set_text(hover_info.prev_argument, "");
            hover_info.prev_argument->ms.size = as_scr_vec(MeasureTextEx(font_cond, "", BLOCK_TEXT_SIZE, 0.0));
            update_measurements(&vm, parent);
        } else if (hover_info.blockchain) {
            // Detach block
            printf("Detach block\n");
            blockchain_detach(&vm, &mouse_blockchain, hover_info.blockchain, hover_info.blockchain_index);
            if (hover_info.blockchain_index == 0) {
                blockchain_free(hover_info.blockchain);
                blockcode_remove_blockchain(&block_code, hover_info.blockchain - editor_code);
                hover_info.block = NULL;
            }
        }
        return true;
    }
    return false;
}

void handle_key_press(void) {
    if (!hover_info.select_argument) {
        if (IsKeyPressed(KEY_SPACE) && vector_size(editor_code) > 0) {
            blockchain_select_counter++;
            if ((vec_size_t)blockchain_select_counter >= vector_size(editor_code)) blockchain_select_counter = 0;

            camera_pos.x = editor_code[blockchain_select_counter].pos.x - ((GetScreenWidth() - conf.side_bar_size) / 2 + conf.side_bar_size);
            camera_pos.y = editor_code[blockchain_select_counter].pos.y - ((GetScreenHeight() - conf.font_size * 2.2) / 2 + conf.font_size * 2.2);
            actionbar_show(TextFormat("Jump to chain (%d/%d)", blockchain_select_counter + 1, vector_size(editor_code)));
        }
        return;
    };
    assert(hover_info.select_argument->type == ARGUMENT_TEXT || hover_info.select_argument->type == ARGUMENT_CONST_STRING);
    if (vm.blockdefs[hover_info.select_block->id].inputs[hover_info.select_argument->input_id].type == INPUT_DROPDOWN) return;

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        char* arg_text = hover_info.select_argument->data.text;
        if (vector_size(arg_text) <= 1) return;

        int remove_pos = vector_size(arg_text) - 2;
        int remove_size = 1;

        while (((unsigned char)arg_text[remove_pos] >> 6) == 2) { // This checks if we are in the middle of UTF-8 char
            remove_pos--;
            remove_size++;
        }

        vector_erase(arg_text, remove_pos, remove_size);
        update_measurements(&vm, hover_info.select_block);
        return;
    }

    int char_val;
    while ((char_val = GetCharPressed())) {
        char** arg_text = &hover_info.select_argument->data.text;
        
        int utf_size = 0;
        const char* utf_char = CodepointToUTF8(char_val, &utf_size);
        for (int i = 0; i < utf_size; i++) {
            vector_insert(arg_text, vector_size(*arg_text) - 1, utf_char[i]);
        }
        update_measurements(&vm, hover_info.select_block);
    }
}

void handle_mouse_wheel(void) {
    int wheel = (int)GetMouseWheelMove();

    dropdown.scroll_amount = MAX(dropdown.scroll_amount - wheel, 0);
    if (hover_info.sidebar) {
        sidebar.scroll_amount = MAX(sidebar.scroll_amount - wheel * (conf.font_size + SIDE_BAR_PADDING) * 2, 0);
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

    ScrBlockdef blockdef = vm.blockdefs[hover_info.select_block->id];
    ScrBlockInput block_input = blockdef.inputs[hover_info.select_argument->input_id];

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
    strncpy(config->font_symbols, "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNMйцукенгшщзхъфывапролджэячсмитьбюёЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮЁ ,./;'\\[]=-0987654321`~!@#$%^&*()_+{}:\"|<>?", sizeof(config->font_symbols) - 1);
    strncpy(config->font_path, DATA_PATH "nk57-cond.otf", sizeof(config->font_path) - 1);
    strncpy(config->font_bold_path, DATA_PATH "nk57-eb.otf", sizeof(config->font_bold_path) - 1);
}

void apply_config(Config* dst, Config* src) {
    dst->side_bar_size = src->side_bar_size;
}

void save_config(Config* config) {
    int file_size = 1;
    // ARRLEN also includes \0 into size, but we are using this size to put = sign instead
    file_size += ARRLEN("UI_SIZE") + 10 + 1;
    file_size += ARRLEN("SIDE_BAR_SIZE") + 10 + 1;
    file_size += ARRLEN("FONT_SYMBOLS") + strlen(config->font_symbols) + 1;
    file_size += ARRLEN("FONT_PATH") + strlen(config->font_path) + 1;
    file_size += ARRLEN("FONT_BOLD_PATH") + strlen(config->font_bold_path) + 1;
    
    char* file_str = malloc(sizeof(char) * file_size);
    int cursor = 0;

    cursor += sprintf(file_str + cursor, "UI_SIZE=%u\n", config->font_size);
    cursor += sprintf(file_str + cursor, "SIDE_BAR_SIZE=%u\n", config->side_bar_size);
    cursor += sprintf(file_str + cursor, "FONT_SYMBOLS=%s\n", config->font_symbols);
    cursor += sprintf(file_str + cursor, "FONT_PATH=%s\n", config->font_path);
    cursor += sprintf(file_str + cursor, "FONT_BOLD_PATH=%s\n", config->font_bold_path);

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
        } else if (!strcmp(field, "FONT_SYMBOLS")) {
            strncpy(config->font_symbols, value, sizeof(config->font_symbols) - 1);
        } else if (!strcmp(field, "FONT_PATH")) {
            strncpy(config->font_path, value, sizeof(config->font_path) - 1);
        } else if (!strcmp(field, "FONT_BOLD_PATH")) {
            strncpy(config->font_bold_path, value, sizeof(config->font_bold_path) - 1);
        } else {
            printf("Unknown key: %s\n", field);
        }
    }

    UnloadFileText(file);
}

ScrMeasurement measure_text(char* text) {
    ScrMeasurement ms = {0};
    ms.size = as_scr_vec(MeasureTextEx(font_cond, text, BLOCK_TEXT_SIZE, 0.0));
    return ms;
}

ScrMeasurement measure_argument(char* text) {
    ScrMeasurement ms = {0};
    ms.size = as_scr_vec(MeasureTextEx(font_cond, text, BLOCK_TEXT_SIZE, 0.0));
    ms.size.x += BLOCK_STRING_PADDING;
    ms.size.x = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, ms.size.x);
    return ms;
}

ScrMeasurement measure_image(ScrImage image) {
    Texture2D* texture = image.image_ptr;
    ScrMeasurement ms = {0};
    ms.size.x = (float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)texture->height * (float)texture->width;
    ms.size.y = conf.font_size - BLOCK_OUTLINE_SIZE * 4;
    return ms;
}

ScrFuncArg block_noop(ScrExec* exec, int argc, ScrFuncArg* argv) {
    (void) exec;
    (void) argc;
    (void) argv;
    RETURN_NOTHING;
}

ScrFuncArg block_loop(ScrExec* exec, int argc, ScrFuncArg* argv) {
    if (argc < 1) RETURN_OMIT_ARGS;
    if (argv[0].type != FUNC_ARG_CONTROL) RETURN_OMIT_ARGS;

    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        control_stack_push_data(exec->running_ind, size_t)
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        control_stack_pop_data(exec->running_ind, size_t)
        control_stack_push_data(exec->running_ind, size_t)
    }

    RETURN_OMIT_ARGS;
}

ScrFuncArg block_if(ScrExec* exec, int argc, ScrFuncArg* argv) {
    if (argc < 1) RETURN_BOOL(1);
    if (argv[0].type != FUNC_ARG_CONTROL) RETURN_BOOL(1);
    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (!func_arg_to_bool(argv[1])) {
            exec->skip_block = true;
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

ScrFuncArg block_else_if(ScrExec* exec, int argc, ScrFuncArg* argv) {
    if (argc < 1) RETURN_BOOL(1);
    if (argv[0].type != FUNC_ARG_CONTROL) RETURN_BOOL(1);
    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (argc < 3 || func_arg_to_bool(argv[1])) {
            exec->skip_block = true;
            control_stack_push_data((int)1, int)
        } else {
            int condition = func_arg_to_bool(argv[2]);
            if (!condition) exec->skip_block = true;
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


ScrFuncArg block_else(ScrExec* exec, int argc, ScrFuncArg* argv) {
    if (argc < 1) RETURN_BOOL(1);
    if (argv[0].type != FUNC_ARG_CONTROL) RETURN_BOOL(1);
    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (argc < 2 || func_arg_to_bool(argv[1])) {
            exec->skip_block = true;
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
ScrFuncArg block_repeat(ScrExec* exec, int argc, ScrFuncArg* argv) {
    if (argc < 1) RETURN_OMIT_ARGS;
    if (argv[0].type != FUNC_ARG_CONTROL) RETURN_OMIT_ARGS;

    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        int cycles = func_arg_to_int(argv[1]);
        if (cycles <= 0) {
            exec->skip_block = true;
            control_stack_push_data((int)0, int) // This indicates the end block that it should NOT loop
            RETURN_OMIT_ARGS;
        }
        control_stack_push_data(exec->running_ind, size_t)
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

        control_stack_pop_data(exec->running_ind, size_t)
        control_stack_push_data(exec->running_ind, size_t)
        control_stack_push_data(left - 1, int)
        control_stack_push_data((int)1, int)
    }

    RETURN_OMIT_ARGS;
}

ScrFuncArg block_while(ScrExec* exec, int argc, ScrFuncArg* argv) {
    if (argc < 2) RETURN_BOOL(0);
    if (argv[0].type != FUNC_ARG_CONTROL) RETURN_BOOL(0);

    if (argv[0].data.control_arg == CONTROL_ARG_BEGIN) {
        if (!func_arg_to_bool(argv[1])) {
            exec->skip_block = true;
            RETURN_OMIT_ARGS;
        }
        control_stack_push_data(exec->running_ind, size_t)
    } else if (argv[0].data.control_arg == CONTROL_ARG_END) {
        if (!func_arg_to_bool(argv[1])) {
            size_t bin;
            control_stack_pop_data(bin, size_t)
            (void) bin; // Cleanup stack
            RETURN_BOOL(1);
        }

        control_stack_pop_data(exec->running_ind, size_t)
        control_stack_push_data(exec->running_ind, size_t)
    }

    RETURN_NOTHING;
}

ScrFuncArg block_declare_var(ScrExec* exec, int argc, ScrFuncArg* argv) {
    if (argc < 2) RETURN_NOTHING;
    if (argv[0].type != FUNC_ARG_STATIC_STR) RETURN_NOTHING;

    ScrFuncArg var_value;

    switch (argv[1].type) {
    case FUNC_ARG_UNMANAGED_STR:
    case FUNC_ARG_MANAGED_STR:
        ScrString string = string_new(0);
        string_add(&string, argv[1].data.str_arg);
        var_value.type = FUNC_ARG_UNMANAGED_STR;
        var_value.data.str_arg = string.str;
        break;
    default:
        var_value = argv[1];
        break;
    }

    variable_stack_push_var(exec, argv[0].data.str_arg, var_value);
    return var_value;
}

ScrFuncArg block_get_var(ScrExec* exec, int argc, ScrFuncArg* argv) {
    if (argc < 1) RETURN_NOTHING;
    if (argv[0].type != FUNC_ARG_STATIC_STR) RETURN_NOTHING;

    ScrVariable* var = variable_stack_get_variable(exec, argv[0].data.str_arg);
    if (!var) RETURN_NOTHING;
    return var->value;
}

ScrFuncArg block_set_var(ScrExec* exec, int argc, ScrFuncArg* argv) {
    if (argc < 2) RETURN_NOTHING;
    if (argv[0].type != FUNC_ARG_STATIC_STR) RETURN_NOTHING;

    ScrVariable* var = variable_stack_get_variable(exec, argv[0].data.str_arg);
    if (!var) RETURN_NOTHING;

    ScrFuncArg new_value;
    switch (argv[1].type) {
    case FUNC_ARG_UNMANAGED_STR:
    case FUNC_ARG_MANAGED_STR:
        ScrString string = string_new(0);
        string_add(&string, argv[1].data.str_arg);
        new_value.type = FUNC_ARG_UNMANAGED_STR;
        new_value.data.str_arg = string.str;
        break;
    default:
        new_value = argv[1];
        break;
    }
    if (var->value.type == FUNC_ARG_UNMANAGED_STR) {
        free((char*)var->value.data.str_arg);
    }

    var->value = new_value;
    return var->value;
}

ScrFuncArg block_print(ScrExec* exec, int argc, ScrFuncArg* argv) {
    (void) exec;
    if (argc >= 1) {
        int bytes_sent = 0;
        switch (argv[0].type) {
        case FUNC_ARG_INT:
            bytes_sent = printf("%d\n", argv[0].data.int_arg) - 1;
            break;
        case FUNC_ARG_BOOL:
            bytes_sent = printf("%s\n", argv[0].data.int_arg ? "true" : "false") - 1;
            break;
        case FUNC_ARG_UNMANAGED_STR:
        case FUNC_ARG_MANAGED_STR:
        case FUNC_ARG_STATIC_STR:
            bytes_sent = printf("%s\n", argv[0].data.str_arg) - 1;
            break;
        default:
            break;
        }
        RETURN_INT(bytes_sent);
    }
    RETURN_INT(0);
}

ScrFuncArg block_input(ScrExec* exec, int argc, ScrFuncArg* argv) {
    (void) exec;
    (void) argv;
    (void) argc;
    printf("Input int: ");
    fflush(stdout);

    char input[256];
    fgets(input, 256, stdin);
    int val = atoi(input);
    RETURN_INT(val);
}

ScrFuncArg block_random(ScrExec* exec, int argc, ScrFuncArg* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    int min = func_arg_to_int(argv[0]);
    int max = func_arg_to_int(argv[1]);
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    int val = rand() % (max - min + 1) + min;
    RETURN_INT(val);
}

ScrFuncArg block_join(ScrExec* exec, int argc, ScrFuncArg* argv) {
    (void) exec;
    if (argc < 2) RETURN_NOTHING;
    const char* left;
    const char* right;

    char left_buf[16];
    switch (argv[0].type) {
    case FUNC_ARG_UNMANAGED_STR:
    case FUNC_ARG_MANAGED_STR:
    case FUNC_ARG_STATIC_STR:
        left = argv[0].data.str_arg;
        break;
    case FUNC_ARG_BOOL:
        left = argv[0].data.int_arg ? "true" : "false";
        break;
    case FUNC_ARG_INT:
        snprintf(left_buf, 16, "%d", argv[0].data.int_arg);
        left = left_buf;
        break;
    default:
        left = "";
        break;
    }

    char right_buf[16];
    switch (argv[1].type) {
    case FUNC_ARG_UNMANAGED_STR:
    case FUNC_ARG_MANAGED_STR:
    case FUNC_ARG_STATIC_STR:
        right = argv[1].data.str_arg;
        break;
    case FUNC_ARG_BOOL:
        right = argv[1].data.int_arg ? "true" : "false";
        break;
    case FUNC_ARG_INT:
        snprintf(right_buf, 16, "%d", argv[1].data.int_arg);
        right = right_buf;
        break;
    default:
        right = "";
        break;
    }
    ScrString string = string_new(0);
    string_add(&string, left);
    string_add(&string, right);

    ScrFuncArg arg;
    arg.type = FUNC_ARG_MANAGED_STR;
    arg.data.str_arg = string.str;
    return arg;
}

ScrFuncArg block_plus(ScrExec* exec, int argc, ScrFuncArg* argv) {
    (void) exec;
    if (argc < 2) RETURN_INT(0);
    RETURN_INT(func_arg_to_int(argv[0]) + func_arg_to_int(argv[1]));
}

ScrFuncArg block_less(ScrExec* exec, int argc, ScrFuncArg* argv) {
    (void) exec;
    if (argc < 1) RETURN_BOOL(0);
    if (argc < 2) RETURN_BOOL(func_arg_to_bool(argv[0]) < 0);
    RETURN_BOOL(func_arg_to_int(argv[0]) < func_arg_to_int(argv[1]));
}

ScrFuncArg block_eq(ScrExec* exec, int argc, ScrFuncArg* argv) {
    (void) exec;
    if (argc < 2) RETURN_BOOL(0);
    if (argv[0].type != argv[1].type) RETURN_BOOL(0);

    switch (argv[0].type) {
    case FUNC_ARG_BOOL:
    case FUNC_ARG_INT:
        RETURN_BOOL(argv[0].data.int_arg == argv[1].data.int_arg);
    case FUNC_ARG_UNMANAGED_STR:
    case FUNC_ARG_MANAGED_STR:
    case FUNC_ARG_STATIC_STR:
        RETURN_BOOL(!strcmp(argv[0].data.str_arg, argv[1].data.str_arg));
    case FUNC_ARG_NOTHING:
        RETURN_BOOL(1);
    default:
        RETURN_BOOL(0);
    }
}

ScrFuncArg block_not_eq(ScrExec* exec, int argc, ScrFuncArg* argv) {
    (void) exec;
    if (argc < 2) RETURN_BOOL(1);
    if (argv[0].type != argv[1].type) RETURN_BOOL(1);

    switch (argv[0].type) {
    case FUNC_ARG_BOOL:
    case FUNC_ARG_INT:
        RETURN_BOOL(argv[0].data.int_arg != argv[1].data.int_arg);
    case FUNC_ARG_UNMANAGED_STR:
    case FUNC_ARG_MANAGED_STR:
    case FUNC_ARG_STATIC_STR:
        RETURN_BOOL(strcmp(argv[0].data.str_arg, argv[1].data.str_arg) != 0);
    case FUNC_ARG_NOTHING:
        RETURN_BOOL(0);
    default:
        RETURN_BOOL(1);
    }
}

Texture2D load_svg(const char* path) {
    Image svg_img = LoadImageSvg(path, conf.font_size, conf.font_size);
    Texture2D texture = LoadTextureFromImage(svg_img);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    UnloadImage(svg_img);
    return texture;
}

void setup(void) {
    srand(time(NULL)); // God seed 🙏🙏🙏

    run_tex = LoadTexture(DATA_PATH "run.png");
    SetTextureFilter(run_tex, TEXTURE_FILTER_BILINEAR);
    drop_tex = LoadTexture(DATA_PATH "drop.png");
    SetTextureFilter(drop_tex, TEXTURE_FILTER_BILINEAR);
    close_tex = LoadTexture(DATA_PATH "close.png");
    SetTextureFilter(close_tex, TEXTURE_FILTER_BILINEAR);

    logo_tex = load_svg(DATA_PATH "logo.svg");
    warn_tex = load_svg(DATA_PATH "warning.svg");
    stop_tex = load_svg(DATA_PATH "stop.svg");
    logo_tex_nuc = TextureToNuklear(logo_tex);
    warn_tex_nuc = TextureToNuklear(warn_tex);

    int codepoints_count;
    int *codepoints = LoadCodepoints(conf.font_symbols, &codepoints_count);
    font_cond = LoadFontEx(conf.font_path, conf.font_size, codepoints, codepoints_count);
    font_eb = LoadFontEx(conf.font_bold_path, conf.font_size, codepoints, codepoints_count);
    UnloadCodepoints(codepoints);

    SetTextureFilter(font_cond.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(font_eb.texture, TEXTURE_FILTER_BILINEAR);

    click_snd = LoadSound(DATA_PATH "click.wav");

    line_shader = LoadShaderFromMemory(line_shader_vertex, line_shader_fragment);
    shader_time_loc = GetShaderLocation(line_shader, "time");

    vm = vm_new(measure_text, measure_argument, measure_image);

    int on_start = block_register(&vm, "on_start", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x77, 0x00, 0xFF }, block_noop);
    block_add_text(&vm, on_start, "When");
    block_add_image(&vm, on_start, (ScrImage) { .image_ptr = &run_tex });
    block_add_text(&vm, on_start, "clicked");

    int sc_input = block_register(&vm, "input", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xff }, block_input);
    block_add_text(&vm, sc_input, "Get int");

    //int on_key_press = block_register(&vm, "on_key_press", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x77, 0x00, 0xFF }, NULL);
    //block_add_text(&vm, on_key_press, "When");
    //block_add_dropdown(&vm, on_key_press, DROPDOWN_SOURCE_LISTREF, &keys_accessor);
    //block_add_text(&vm, on_key_press, "pressed");

    int sc_print = block_register(&vm, "print", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xaa, 0x44, 0xFF }, block_print);
    block_add_text(&vm, sc_print, "Print");
    block_add_argument(&vm, sc_print, "Привет, мусороид!", BLOCKCONSTR_UNLIMITED);

    int sc_loop = block_register(&vm, "loop", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_loop);
    block_add_text(&vm, sc_loop, "Loop");

    int sc_repeat = block_register(&vm, "repeat", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_repeat);
    block_add_text(&vm, sc_repeat, "Repeat");
    block_add_argument(&vm, sc_repeat, "10", BLOCKCONSTR_UNLIMITED);
    block_add_text(&vm, sc_repeat, "times");

    int sc_while = block_register(&vm, "while", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_while);
    block_add_text(&vm, sc_while, "While");
    block_add_argument(&vm, sc_while, "", BLOCKCONSTR_UNLIMITED);

    int sc_if = block_register(&vm, "if", BLOCKTYPE_CONTROL, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_if);
    block_add_text(&vm, sc_if, "If");
    block_add_argument(&vm, sc_if, "", BLOCKCONSTR_UNLIMITED);
    block_add_text(&vm, sc_if, ", then");

    int sc_else_if = block_register(&vm, "else_if", BLOCKTYPE_CONTROLEND, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_else_if);
    block_add_text(&vm, sc_else_if, "Else if");
    block_add_argument(&vm, sc_else_if, "", BLOCKCONSTR_UNLIMITED);
    block_add_text(&vm, sc_else_if, ", then");

    int sc_else = block_register(&vm, "else", BLOCKTYPE_CONTROLEND, (ScrColor) { 0xff, 0x99, 0x00, 0xff }, block_else);
    block_add_text(&vm, sc_else, "Else");

    int sc_end = block_register(&vm, "end", BLOCKTYPE_END, (ScrColor) { 0x77, 0x77, 0x77, 0xff }, block_noop);
    block_add_text(&vm, sc_end, "End");

    int sc_plus = block_register(&vm, "plus", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_plus);
    block_add_argument(&vm, sc_plus, "9", BLOCKCONSTR_UNLIMITED);
    block_add_text(&vm, sc_plus, "+");
    block_add_argument(&vm, sc_plus, "10", BLOCKCONSTR_UNLIMITED);

    int sc_less = block_register(&vm, "less", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_less);
    block_add_argument(&vm, sc_less, "9", BLOCKCONSTR_UNLIMITED);
    block_add_text(&vm, sc_less, "<");
    block_add_argument(&vm, sc_less, "11", BLOCKCONSTR_UNLIMITED);

    int sc_eq = block_register(&vm, "eq", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_eq);
    block_add_argument(&vm, sc_eq, "", BLOCKCONSTR_UNLIMITED);
    block_add_text(&vm, sc_eq, "=");
    block_add_argument(&vm, sc_eq, "", BLOCKCONSTR_UNLIMITED);

    int sc_not_eq = block_register(&vm, "not_eq", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_not_eq);
    block_add_argument(&vm, sc_not_eq, "", BLOCKCONSTR_UNLIMITED);
    block_add_text(&vm, sc_not_eq, "!=");
    block_add_argument(&vm, sc_not_eq, "", BLOCKCONSTR_UNLIMITED);

    int sc_random = block_register(&vm, "random", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_random);
    block_add_text(&vm, sc_random, "Random");
    block_add_argument(&vm, sc_random, "0", BLOCKCONSTR_UNLIMITED);
    block_add_text(&vm, sc_random, "to");
    block_add_argument(&vm, sc_random, "10", BLOCKCONSTR_UNLIMITED);

    int sc_join = block_register(&vm, "join", BLOCKTYPE_NORMAL, (ScrColor) { 0x00, 0xcc, 0x77, 0xFF }, block_join);
    block_add_text(&vm, sc_join, "Join");
    block_add_argument(&vm, sc_join, "абоба ", BLOCKCONSTR_UNLIMITED);
    block_add_argument(&vm, sc_join, "мусор", BLOCKCONSTR_UNLIMITED);

    int sc_decl_var = block_register(&vm, "decl_var", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x66, 0x00, 0xff }, block_declare_var);
    block_add_text(&vm, sc_decl_var, "Declare");
    block_add_argument(&vm, sc_decl_var, "my variable", BLOCKCONSTR_STRING);
    block_add_text(&vm, sc_decl_var, "=");
    block_add_argument(&vm, sc_decl_var, "", BLOCKCONSTR_UNLIMITED);

    int sc_get_var = block_register(&vm, "get_var", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x66, 0x00, 0xff }, block_get_var);
    block_add_text(&vm, sc_get_var, "Get");
    block_add_argument(&vm, sc_get_var, "my variable", BLOCKCONSTR_STRING);

    int sc_set_var = block_register(&vm, "set_var", BLOCKTYPE_NORMAL, (ScrColor) { 0xff, 0x66, 0x00, 0xff }, block_set_var);
    block_add_text(&vm, sc_set_var, "Set");
    block_add_argument(&vm, sc_set_var, "my variable", BLOCKCONSTR_STRING);
    block_add_text(&vm, sc_set_var, "=");
    block_add_argument(&vm, sc_set_var, "", BLOCKCONSTR_UNLIMITED);

    mouse_blockchain = blockchain_new();
    draw_stack = vector_create();
    editor_code = vector_create();

    sidebar.blocks = vector_create();
    for (vec_size_t i = 0; i < vector_size(vm.blockdefs); i++) {
        if (vm.blockdefs[i].hidden) continue;
        vector_add(&sidebar.blocks, block_new_ms(&vm, i));
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
    gui.ctx->style.window.spacing = nk_vec2(10, 10);

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
    SetTargetFPS(60);
    //EnableEventWaiting();
    SetWindowState(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);

    setup();

    while (!WindowShouldClose()) {
        hover_info.sidebar = GetMouseX() < conf.side_bar_size && GetMouseY() > conf.font_size * 2.2;
        hover_info.block = NULL;
        hover_info.argument = NULL;
        hover_info.argument_pos.x = 0;
        hover_info.argument_pos.y = 0;
        hover_info.prev_argument = NULL;
        hover_info.blockchain = NULL;
        hover_info.blockchain_index = -1;
        hover_info.blockchain_layer = 0;
        hover_info.dropdown_hover_ind = -1;
        hover_info.top_bars.ind = -1;
        hover_info.exec_ind = -1;
        hover_info.exec_chain_ind = -1;

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

        if (IsWindowResized()) shader_time = 0.0;

        sidebar.max_y = conf.font_size * 2.2 + SIDE_BAR_PADDING + (conf.font_size + SIDE_BAR_PADDING) * vector_size(sidebar.blocks);
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
            hover_info.exec_chain_ind = exec.running_chain_ind;
            hover_info.exec_ind = exec.running_ind;
            //actionbar_show(TextFormat("chain: %zu, ind: %zu", hover_info.exec_chain_ind, hover_info.exec_ind));
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
                    draw_block_chain(&editor_code[i], camera_pos, hover_info.exec_chain_ind == i);
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
                    "Sidebar scroll: %d, Max: %d",
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
                    sidebar.scroll_amount, sidebar.max_y
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
            DrawTextEx(
                font_cond, 
                TextFormat("FPS: %d\nFrame time: %.3f", GetFPS(), GetFrameTime()), 
                (Vector2){ 
                    conf.side_bar_size + 5, 
                    conf.font_size * 2.2 + 5
                }, 
                conf.font_size * 0.5,
                0.0, 
                GRAY
            );
#endif
        } else if (current_tab == TAB_OUTPUT) {
            draw_output_box();
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
    vector_free(draw_stack);
    UnloadNuklear(gui.ctx);
    blockchain_free(&mouse_blockchain);
    for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
        blockchain_free(&editor_code[i]);
    }
    
    for (vec_size_t i = 0; i < vector_size(sidebar.blocks); i++) {
        block_free(&sidebar.blocks[i]);
    }
    vector_free(sidebar.blocks);
    vm_free(&vm);
    CloseWindow();

    return 0;
}
