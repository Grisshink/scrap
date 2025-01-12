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

#include "raylib.h"
#include "scrap.h"
#include "vec.h"
#include "term.h"

#include <assert.h>
#include <math.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MOD(x, y) (((x) % (y) + (y)) % (y))
#define LERP(min, max, t) (((max) - (min)) * (t) + (min))
#define UNLERP(min, max, v) (((float)(v) - (float)(min)) / ((float)(max) - (float)(min)))
#define CONVERT_COLOR(color, type) (type) { color.r, color.g, color.b, color.a }

void sidebar_init(void) {
    sidebar.blocks = vector_create();
    for (vec_size_t i = 0; i < vector_size(vm.blockdefs); i++) {
        if (vm.blockdefs[i]->hidden) continue;
        vector_add(&sidebar.blocks, block_new_ms(vm.blockdefs[i]));
    }

    sidebar.max_y = conf.font_size * 2.2 + SIDE_BAR_PADDING;
    for (vec_size_t i = 0; i < vector_size(sidebar.blocks); i++) {
        sidebar.max_y += sidebar.blocks[i].ms.size.y + SIDE_BAR_PADDING;
    }
}

void actionbar_show(const char* text) {
    TraceLog(LOG_INFO, "[ACTION] %s", text);
    strncpy(actionbar.text, text, sizeof(actionbar.text) - 1);
    actionbar.show_time = 3.0;
}

void draw_image(Vector2 position, ScrImage image, float size) {
    Texture2D* img = image.image_ptr;
    DrawTextureEx(*img, (Vector2) { position.x + SHADOW_DISTANCE, position.y + SHADOW_DISTANCE }, 0.0, size / (float)img->height, (Color) { 0x00, 0x00, 0x00, 0x80 });
    DrawTextureEx(*img, position, 0.0, size / (float)img->height, WHITE);
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
            DrawRectangleRoundedLinesEx(rect, 0.5, 5, BLOCK_OUTLINE_SIZE, selected ? selected_color : hovered_color);
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

    cursor.x += BLOCK_PADDING;

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
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
                DrawTextEx(font_cond_shadow, cur->data.stext.text, (Vector2) { floorf(arg_pos.x), floorf(arg_pos.y) }, BLOCK_TEXT_SIZE, 0.0, WHITE);
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
            width = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0).x;
            arg_pos.y += block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5;

            DrawTextEx(font_cond, "NODEF", arg_pos, BLOCK_TEXT_SIZE, 0.0, RED);
            break;
        }

        cursor.x += width + BLOCK_PADDING;
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

            DrawTextEx(font_cond_shadow, cur.data.stext.text, (Vector2) { floorf(arg_pos.x), floorf(arg_pos.y) }, BLOCK_TEXT_SIZE, 0.0, WHITE);
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
            case ARGUMENT_TEXT: draw_input_box(arg_pos, block->arguments[arg_id].ms, &block->arguments[arg_id].data.text, block->arguments[arg_id].type == ARGUMENT_CONST_STRING); break; case ARGUMENT_BLOCK:
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
                DrawRectangleRoundedLinesEx(arg_size, 0.5, 4, BLOCK_OUTLINE_SIZE, ColorBrightness(color, &block->arguments[arg_id] == hover_info.select_argument ? -0.5 : 0.5));
            }
            Vector2 ms = MeasureTextEx(font_cond, block->arguments[arg_id].data.text, BLOCK_TEXT_SIZE, 0);
            DrawTextEx(
                font_cond_shadow, 
                block->arguments[arg_id].data.text,
                (Vector2) { 
                    cursor.x + BLOCK_STRING_PADDING * 0.5, 
                    cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? BLOCK_OUTLINE_SIZE : block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5),
                },
                BLOCK_TEXT_SIZE,
                0.0,
                WHITE
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
        default: ; // This fixes gcc-9 error
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
//   +---------------+ 5
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
        DrawTextEx(font_cond_shadow, list[i], pos, BLOCK_TEXT_SIZE, 0, WHITE);
        pos.y += conf.font_size;
    }
}

void draw_dots(void) {
    int win_width = GetScreenWidth();
    int win_height = GetScreenHeight();

    for (int y = MOD(-(int)camera_pos.y, conf.font_size * 2); y < win_height; y += conf.font_size * 2) {
        for (int x = MOD(-(int)camera_pos.x, conf.font_size * 2); x < win_width; x += conf.font_size * 2) {
            DrawRectangle(x, y, 2, 2, (Color) { 0x40, 0x40, 0x40, 0xff });
        }
    }

    if (shader_time == 1.0) return;
    BeginShaderMode(line_shader);
    for (int y = MOD(-(int)camera_pos.y, conf.font_size * 2); y < win_height; y += conf.font_size * 2) {
        DrawRectangle(0, y, win_width, 2, (Color) { 0x40, 0x40, 0x40, 0xff });
    }
    for (int x = MOD(-(int)camera_pos.x, conf.font_size * 2); x < win_width; x += conf.font_size * 2) {
        DrawRectangle(x, 0, 2, win_height, (Color) { 0x40, 0x40, 0x40, 0xff });
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
    pthread_mutex_lock(&term.lock);
    DrawRectangleRec(term.size, BLACK);
    BeginShaderMode(line_shader);
    DrawRectangleLinesEx(term.size, 2.0, (Color) { 0x60, 0x60, 0x60, 0xff });
    EndShaderMode();

    if (term.buffer) {
        Vector2 pos = (Vector2) { term.size.x, term.size.y };
        for (int y = 0; y < term.char_h; y++) {
            pos.x = term.size.x;
            for (int x = 0; x < term.char_w; x++) {
                DrawTextEx(font_mono, term.buffer[x + y*term.char_w], pos, TERM_CHAR_SIZE, 0.0, WHITE);
                pos.x += term.char_size.x;
            }
            pos.y += TERM_CHAR_SIZE;
        }
        if (fmod(GetTime(), 1.0) <= 0.5) {
            Vector2 cursor_pos = (Vector2) {
                term.size.x + (term.cursor_pos % term.char_w) * term.char_size.x,
                term.size.y + (term.cursor_pos / term.char_w) * TERM_CHAR_SIZE,
            };
            DrawRectangle(cursor_pos.x, cursor_pos.y, BLOCK_OUTLINE_SIZE, TERM_CHAR_SIZE, WHITE);
        }
    }

    pthread_mutex_unlock(&term.lock);
}

void prerender_font_shadow(Font* font) {
    SetTextureFilter(font->texture, TEXTURE_FILTER_POINT);
    Image font_img = LoadImageFromTexture(font->texture);
    Image render_img = ImageCopy(font_img);
    ImageClearBackground(&render_img, BLANK);
    ImageDraw(
        &render_img, 
        font_img, 
        (Rectangle) { 0, 0, font_img.width, font_img.height }, 
        (Rectangle) { SHADOW_DISTANCE, SHADOW_DISTANCE, font_img.width, font_img.height }, 
        (Color) { 0x00, 0x00, 0x00, 0x88 }
    );
    ImageDraw(
        &render_img, 
        font_img, 
        (Rectangle) { 0, 0, font_img.width, font_img.height }, 
        (Rectangle) { 0, 0, font_img.width, font_img.height }, 
        WHITE
    );
    UnloadTexture(font->texture);
    font->texture = LoadTextureFromImage(render_img);
    SetTextureFilter(font->texture, TEXTURE_FILTER_BILINEAR);
}

void scrap_gui_draw_blockdef(ScrBlockdef* blockdef) {
    bool collision = false;
    Color block_color = CONVERT_COLOR(blockdef->color, Color);
    Color outline_color = ColorBrightness(block_color, collision ? 0.5 : -0.2);

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_border(gui, CONVERT_COLOR(outline_color, GuiColor), BLOCK_OUTLINE_SIZE);
        gui_set_align(gui, ALIGN_CENTER);
        gui_set_min_size(gui, 0, conf.font_size);
        gui_set_padding(gui, BLOCK_OUTLINE_SIZE * 2, BLOCK_OUTLINE_SIZE * 2);
        gui_set_gap(gui, BLOCK_PADDING);

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        ScrInput* input = &blockdef->inputs[i];

        switch (input->type) {
        case INPUT_TEXT_DISPLAY:
            gui_text(gui, &font_cond_shadow, input->data.stext.text, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            break;
        case INPUT_IMAGE_DISPLAY:
            gui_image(gui, input->data.simage.image.image_ptr, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            break;
        case INPUT_ARGUMENT:
            scrap_gui_draw_blockdef(input->data.arg.blockdef);
            break;
        default:
            gui_text(gui, &font_cond_shadow, "NODEF", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            break;
        }
    }

    gui_element_end(gui);
    gui_element_end(gui);
}

void block_border_on_hover(FlexElement* el) {
    Color new_col = ColorBrightness(CONVERT_COLOR(el->color, Color), 0.5);
    el->color = CONVERT_COLOR(new_col, GuiColor);
}

void block_on_hover(FlexElement* el) {
    Color new_col = ColorBrightness(CONVERT_COLOR(el->color, Color), 0.3);
    el->color = CONVERT_COLOR(new_col, GuiColor);
    hover_info.block = el->custom_data;
}

void block_argument_on_hover(FlexElement* el) {
    hover_info.prev_argument = el->custom_data;
}

void argument_on_hover(FlexElement* el) {
    el->draw_type = DRAWTYPE_BORDER;
    el->color = (GuiColor) { 0xa0, 0xa0, 0xa0, 0xff };
    el->data.border_width = BLOCK_OUTLINE_SIZE;
    hover_info.argument = el->custom_data;
}

void scrap_gui_draw_block(ScrBlock* block) {
    bool collision = false;
    Color block_color = CONVERT_COLOR(block->blockdef->color, Color);
    Color dropdown_color = ColorBrightness(block_color, collision ? 0.0 : -0.3);
    Color outline_color = ColorBrightness(block_color, collision ? 0.5 : -0.2);

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
        gui_set_custom_data(gui, block);
        gui_on_hover(gui, block_on_hover);

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_border(gui, CONVERT_COLOR(outline_color, GuiColor), BLOCK_OUTLINE_SIZE);
        gui_set_align(gui, ALIGN_CENTER);
        gui_set_min_size(gui, 0, conf.font_size);
        gui_set_padding(gui, BLOCK_OUTLINE_SIZE * 2, BLOCK_OUTLINE_SIZE * 2);
        gui_set_gap(gui, BLOCK_PADDING);
        gui_on_hover(gui, block_border_on_hover);
    
    size_t arg_id = 0;
    for (size_t i = 0; i < vector_size(block->blockdef->inputs); i++) {
        ScrInput* input = &block->blockdef->inputs[i];
        ScrArgument* arg = &block->arguments[arg_id];

        switch (input->type) {
        case INPUT_TEXT_DISPLAY:
            gui_text(gui, &font_cond_shadow, input->data.stext.text, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            break;
        case INPUT_IMAGE_DISPLAY:
            gui_image(gui, input->data.simage.image.image_ptr, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            break;
        case INPUT_ARGUMENT:
            switch (arg->type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0xff });

                    gui_element_begin(gui);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);
                        gui_set_min_size(gui, 0, conf.font_size - BLOCK_OUTLINE_SIZE * 4);
                        gui_set_align(gui, ALIGN_CENTER);
                        gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                        gui_set_custom_data(gui, arg);
                        gui_on_hover(gui, argument_on_hover);

                        gui_text(gui, &font_cond, arg->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0x00, 0x00, 0x00, 0xff });
                    gui_element_end(gui);
                gui_element_end(gui);
                break;
            case ARGUMENT_BLOCK:
                gui_element_begin(gui);
                    gui_on_hover(gui, block_argument_on_hover);
                    gui_set_custom_data(gui, arg);

                    scrap_gui_draw_block(&arg->data.block);
                gui_element_end(gui);
                break;
            default:
                gui_text(gui, &font_cond_shadow, "NODEF", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            assert(arg->type == ARGUMENT_CONST_STRING);
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_min_size(gui, 0, conf.font_size - BLOCK_OUTLINE_SIZE * 4);
                gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                gui_set_align(gui, ALIGN_CENTER);
                gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));

                gui_text(gui, &font_cond_shadow, arg->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_image(gui, &drop_tex, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_element_end(gui);
            arg_id++;
            break;
        case INPUT_BLOCKDEF_EDITOR:
            assert(arg->type == ARGUMENT_BLOCKDEF);
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_padding(gui, BLOCK_OUTLINE_SIZE * 2, BLOCK_OUTLINE_SIZE * 2);
                gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));
                gui_set_align(gui, ALIGN_CENTER);
                gui_set_gap(gui, BLOCK_PADDING);

                scrap_gui_draw_blockdef(arg->data.blockdef);

                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });

                    gui_image(gui, &edit_tex, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_element_end(gui);

                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });

                    gui_image(gui, &close_tex, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_element_end(gui);
            gui_element_end(gui);
            arg_id++;
            break;
        default:
            gui_text(gui, &font_cond_shadow, "NODEF", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
            break;
        }
    }

    gui_element_end(gui);
    gui_element_end(gui);
}

void button_on_hover(FlexElement* el) {
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
}

void scrap_gui_draw_button(const char* text, int size, bool selected) {
    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_align(gui, ALIGN_CENTER);
        gui_set_min_size(gui, 0, size);
        gui_set_padding(gui, conf.font_size * 0.3, 0);
        if (selected) gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        gui_on_hover(gui, button_on_hover);

        gui_text(gui, &font_cond, text, BLOCK_TEXT_SIZE, selected ? (GuiColor) { 0x00, 0x00, 0x00, 0xff } : (GuiColor) { 0xff, 0xff, 0xff, 0xff });
    gui_element_end(gui);
}

void scrap_gui_draw_top_bar(void) {
    const int top_bar_size = conf.font_size * 1.2;
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
        gui_set_min_size(gui, 0, top_bar_size);
        gui_set_align(gui, ALIGN_CENTER);

        gui_spacer(gui, 5, 0);
        gui_image(gui, &logo_tex, conf.font_size, CONVERT_COLOR(WHITE, GuiColor));
        gui_spacer(gui, 10, 0);
        gui_text(gui, &font_eb, "Scrap", conf.font_size * 0.8, CONVERT_COLOR(WHITE, GuiColor));
        gui_spacer(gui, 10, 0);

        scrap_gui_draw_button("File", top_bar_size, false);
        scrap_gui_draw_button("Settings", top_bar_size, false);
        scrap_gui_draw_button("About", top_bar_size, false);
    gui_element_end(gui);
}

void scrap_gui_draw_tab_bar(void) {
    const int tab_bar_size = conf.font_size;
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
        gui_set_min_size(gui, 0, tab_bar_size);
        gui_set_align(gui, ALIGN_CENTER);

        scrap_gui_draw_button("Code", tab_bar_size, current_tab == TAB_CODE);
        scrap_gui_draw_button("Output", tab_bar_size, current_tab == TAB_OUTPUT);

        gui_grow(gui, DIRECTION_HORIZONTAL);
        gui_text(gui, &font_cond, "Project.scrp", BLOCK_TEXT_SIZE, (GuiColor) { 0x80, 0x80, 0x80, 0xff });
        gui_grow(gui, DIRECTION_HORIZONTAL);
        
        gui_element_begin(gui);
            gui_on_hover(gui, button_on_hover);
            gui_image(gui, &stop_tex, tab_bar_size, CONVERT_COLOR(WHITE, GuiColor));
        gui_element_end(gui);
        gui_element_begin(gui);
            gui_on_hover(gui, button_on_hover);
            gui_image(gui, &run_tex, tab_bar_size, CONVERT_COLOR(WHITE, GuiColor));
        gui_element_end(gui);
    gui_element_end(gui);
}

void blockchain_on_hover(FlexElement* el) {
    hover_info.blockchain = el->custom_data;
}

void scrap_gui_draw_blockchain(ScrBlockChain* chain) {
    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_VERTICAL);
        gui_set_border(gui, CONVERT_COLOR(YELLOW, GuiColor), BLOCK_OUTLINE_SIZE);
        gui_on_hover(gui, blockchain_on_hover);
        gui_set_custom_data(gui, chain);
        gui_set_padding(gui, 5, 5);

    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        scrap_gui_draw_block(&chain->blocks[i]);
    }

    gui_element_end(gui);
}

void sidebar_on_hover(FlexElement* el) {
    (void) el;
    hover_info.sidebar = 1;
}

void scrap_gui_draw_sidebar(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);

        gui_element_begin(gui);
            gui_set_fixed(gui, conf.side_bar_size, 0);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x80 });
            gui_set_padding(gui, SIDE_BAR_PADDING, SIDE_BAR_PADDING);
            gui_set_gap(gui, SIDE_BAR_PADDING);
            gui_on_hover(gui, sidebar_on_hover);

            for (size_t i = dropdown.scroll_amount; i < vector_size(sidebar.blocks); i++) {
                scrap_gui_draw_block(&sidebar.blocks[i]);
            }
        gui_element_end(gui);
    gui_element_end(gui);
}

void scrap_gui_process(void) {
    // Gui
    gui_begin(gui);
        for (size_t i = 0; i < vector_size(editor_code); i++) {
            gui_element_begin(gui);
                gui_set_floating(gui);
                gui_set_position(gui, editor_code[i].pos.x - camera_pos.x, editor_code[i].pos.y - camera_pos.y);

                scrap_gui_draw_blockchain(&editor_code[i]);
            gui_element_end(gui);
        }
        scrap_gui_draw_top_bar();
        scrap_gui_draw_tab_bar();
        scrap_gui_draw_sidebar();
        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, gui->mouse_x, gui->mouse_y);

            scrap_gui_draw_blockchain(&mouse_blockchain);
        gui_element_end(gui);
    gui_end(gui);
}

void scrap_gui_render(void) {
    DrawCommand* command;
    GUI_GET_COMMANDS(gui, command) {
        Texture2D* image = command->data.image;

        switch (command->type) {
        case DRAWTYPE_UNKNOWN:
            assert(false && "Got unknown draw type");
            break;
        case DRAWTYPE_BORDER:
            DrawRectangleLinesEx((Rectangle) { command->pos_x, command->pos_y, command->width, command->height }, command->data.border_width, CONVERT_COLOR(command->color, Color));
            break;
        case DRAWTYPE_RECT:
            DrawRectangle(command->pos_x, command->pos_y, command->width, command->height, CONVERT_COLOR(command->color, Color));
            break;
        case DRAWTYPE_TEXT:
            DrawTextEx(
                *(Font*)command->data.text.font, 
                command->data.text.text, 
                (Vector2) { command->pos_x, command->pos_y }, 
                command->height, 
                0.0, 
                CONVERT_COLOR(command->color, Color)
            );
            break;
        case DRAWTYPE_IMAGE:
            DrawTextureEx(
                *image, 
                (Vector2) { command->pos_x + SHADOW_DISTANCE, command->pos_y + SHADOW_DISTANCE },
                0.0,
                (float)command->height / (float)image->height,
                (Color) { 0x00, 0x00, 0x00, 0x80 }
            );
            DrawTextureEx(
                *image, 
                (Vector2) { command->pos_x, command->pos_y},
                0.0,
                (float)command->height / (float)image->height,
                CONVERT_COLOR(command->color, Color)
            );
            break;
        case DRAWTYPE_SCISSOR_BEGIN:
            BeginScissorMode(command->pos_x, command->pos_y, command->width, command->height);
            break;
        case DRAWTYPE_SCISSOR_END:
            EndScissorMode();
            break;
        default:
            assert(false && "Unimplemented command render");
            break;
        }
#ifdef DEBUG
        DrawRectangleLinesEx((Rectangle) { command->pos_x, command->pos_y, command->width, command->height }, 1.0, (Color) { 0xff, 0x00, 0xff, 0x40 });
#endif
    }
}

void scrap_gui_process_render(void) {
    ClearBackground(GetColor(0x202020ff));
    draw_dots();
    scrap_gui_render();

#ifdef DEBUG
        DrawTextEx(
            font_cond, 
            TextFormat(
                "BlockChain: %p, Layer: %d\n"
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
                "Editor: %d, Editing: %p, Blockdef: %p, input: %zu\n"
                "Elements: %zu/%zu, Draw: %zu/%zu",
                hover_info.blockchain,
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
                GetMouseX(), GetMouseY(),
                (int)hover_info.mouse_click_pos.x, (int)hover_info.mouse_click_pos.y,
                camera_pos.x, camera_pos.y, camera_click_pos.x, camera_click_pos.y,
                hover_info.dropdown_hover_ind, dropdown.scroll_amount,
                hover_info.drag_cancelled,
                hover_info.top_bars.type, hover_info.top_bars.ind,
                block_code.min_pos.x, block_code.min_pos.y, block_code.max_pos.x, block_code.max_pos.y,
                sidebar.scroll_amount, sidebar.max_y,
                hover_info.editor.part, hover_info.editor.edit_blockdef, hover_info.editor.blockdef, hover_info.editor.blockdef_input,
                gui->element_stack_len, ELEMENT_STACK_SIZE, gui->command_stack_len, COMMAND_STACK_SIZE
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
        DrawTextEx(
            font_cond, 
            TextFormat(
                "FPS: %d, Frame time: %.3f\nCommand count: %zu/%zu\nElement count: %zu/%zu", 
                GetFPS(), 
                GetFrameTime(), 
                gui->command_stack_len, ELEMENT_STACK_SIZE,
                gui->element_stack_len, ELEMENT_STACK_SIZE
            ), 
            debug_pos, 
            conf.font_size * 0.5, 
            0.0, 
            (Color) { 0xff, 0xff, 0xff, 0x40 }
        );
#endif

}

void process_render(void) {
    ClearBackground(GetColor(0x202020ff));

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    if (current_tab == TAB_CODE) {
        BeginScissorMode(0, conf.font_size * 2.2, sw, sh - conf.font_size * 2.2);
            for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
                draw_block_chain(&editor_code[i], camera_pos, hover_info.exec_chain == &editor_code[i]);
            }
        EndScissorMode();

        draw_scrollbars();

        BeginScissorMode(0, conf.font_size * 2.2, sw, sh - conf.font_size * 2.2);
            draw_block_chain(&mouse_blockchain, (Vector2) {0}, false);
        EndScissorMode();

        draw_action_bar();
    } else if (current_tab == TAB_OUTPUT) {
        draw_term();
    }

    scrap_gui_render();

    draw_dropdown_list();
    draw_tooltip();
}
