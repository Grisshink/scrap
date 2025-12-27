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

#include "raylib.h"
#include "scrap.h"
#include "vec.h"
#include "term.h"

#define NANOSVG_IMPLEMENTATION
#include "../external/nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "../external/nanosvgrast.h"

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <libintl.h>
#include <stdio.h>

static void draw_code(void);
static void draw_blockchain(BlockChain* chain, bool ghost, bool show_previews, bool editable_arguments);

bool rl_vec_equal(Color lhs, Color rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

void actionbar_show(const char* text) {
    TraceLog(LOG_INFO, "[ACTION] %s", text);
    strncpy(actionbar.text, text, sizeof(actionbar.text) - 1);
    actionbar.show_time = 3.0;
}

static void draw_dots(void) {
    int win_width = GetScreenWidth();
    int win_height = GetScreenHeight();

    for (int y = MOD(-(int)camera_pos.y, conf.font_size * 2); y < win_height; y += conf.font_size * 2) {
        for (int x = MOD(-(int)camera_pos.x, conf.font_size * 2); x < win_width; x += conf.font_size * 2) {
            DrawRectangle(x, y, 2, 2, (Color) { 0x40, 0x40, 0x40, 0xff });
        }
    }

    if (shader_time == 1.0) return;
    if (!IsShaderValid(line_shader)) return;

    BeginShaderMode(line_shader);
    for (int y = MOD(-(int)camera_pos.y, conf.font_size * 2); y < win_height; y += conf.font_size * 2) {
        DrawRectangle(0, y, win_width, 2, (Color) { 0x40, 0x40, 0x40, 0xff });
    }
    for (int x = MOD(-(int)camera_pos.x, conf.font_size * 2); x < win_width; x += conf.font_size * 2) {
        DrawRectangle(x, 0, 2, win_height, (Color) { 0x40, 0x40, 0x40, 0xff });
    }
    EndShaderMode();
}

static void draw_term(int x, int y) {
    mutex_lock(&term.lock);

    if (term.char_w == 0 || term.char_h == 0) goto unlock_term;
    if (!term.buffer) goto unlock_term;

    Rectangle final_pos = { x, y, term.size.x, term.size.y };
    DrawRectangleRec(final_pos, BLACK);

    if (IsShaderValid(line_shader)) {
        BeginShaderMode(line_shader);
        DrawRectangleLinesEx(final_pos, 2.0, (Color) { 0x60, 0x60, 0x60, 0xff });
        EndShaderMode();
    }

    Vector2 pos = (Vector2) { final_pos.x, final_pos.y };
    for (int y = 0; y < term.char_h; y++) {
        pos.x = final_pos.x;
        for (int x = 0; x < term.char_w; x++) {
            TerminalChar buffer_char = term.buffer[x + y*term.char_w];
            if (!rl_vec_equal(CONVERT_COLOR(buffer_char.bg_color, Color), BLACK)) {
                DrawRectangle(pos.x, pos.y, term.char_size.x, term.font_size, CONVERT_COLOR(buffer_char.bg_color, Color));
            }
            pos.x += term.char_size.x;
        }
        pos.y += term.font_size;
    }

    pos = (Vector2) { final_pos.x, final_pos.y };
    for (int y = 0; y < term.char_h; y++) {
        pos.x = final_pos.x;
        for (int x = 0; x < term.char_w; x++) {
            TerminalChar buffer_char = term.buffer[x + y*term.char_w];
            if (!rl_vec_equal(CONVERT_COLOR(buffer_char.fg_color, Color), CONVERT_COLOR(buffer_char.bg_color, Color))) {
                DrawTextEx(font_mono, buffer_char.ch, pos, term.font_size, 0.0, CONVERT_COLOR(buffer_char.fg_color, Color));
            }
            pos.x += term.char_size.x;
        }
        pos.y += term.font_size;
    }
    if (fmod(GetTime(), 1.0) <= 0.5) {
        Vector2 cursor_pos = (Vector2) {
            final_pos.x + (term.cursor_pos % term.char_w) * term.char_size.x,
            final_pos.y + (term.cursor_pos / term.char_w) * term.font_size,
        };
        DrawRectangle(cursor_pos.x, cursor_pos.y, BLOCK_OUTLINE_SIZE, term.font_size, CONVERT_COLOR(term.cursor_fg_color, Color));
    }

unlock_term:
    mutex_unlock(&term.lock);
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

    UnloadImage(font_img);
    UnloadImage(render_img);
}

static void blockdef_on_hover(GuiElement* el) {
    if (hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    hover.editor.part = EDITOR_BLOCKDEF;
    hover.editor.blockdef = el->custom_data;
}

static void blockdef_input_on_hover(GuiElement* el) {
    if (hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    hover.editor.blockchain = hover.editor.prev_blockchain;
    if (el->draw_type != DRAWTYPE_UNKNOWN) return;
    el->draw_type = DRAWTYPE_BORDER;
    el->color = (GuiColor) { 0xa0, 0xa0, 0xa0, 0xff };
    el->data.border.width = BLOCK_OUTLINE_SIZE;
    el->data.border.type = BORDER_NORMAL;
}

static void editor_del_button_on_hover(GuiElement* el) {
    if (hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (hover.button.handler) return;
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0xff, 0xff, 0xff, 0x80 };
    hover.editor.blockdef_input = (size_t)el->custom_data;
    hover.button.handler = handle_editor_del_arg_button;
}

static void editor_button_on_hover(GuiElement* el) {
    if (hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (hover.button.handler) return;
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0xff, 0xff, 0xff, 0x80 };
    hover.button.handler = el->custom_data;
}

static void draw_editor_button(Texture2D* texture, ButtonClickHandler handler) {
    gui_element_begin(gui);
        gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });
        gui_on_hover(gui, editor_button_on_hover);
        gui_set_custom_data(gui, handler);

        gui_image(gui, texture, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
    gui_element_end(gui);
}

static void input_on_hover(GuiElement* el) {
    if (hover.button.handler) return;
    if (hover.is_panel_edit_mode) return;

    unsigned short len;
    hover.input_info = *(InputHoverInfo*)gui_get_state(el, &len);
    hover.input_info.rel_pos = (Vector2) { gui->mouse_x - el->abs_x, gui->mouse_y - el->abs_y };
}

void draw_input(Font* font, char** input, const char* hint, unsigned short font_size, GuiColor font_color, bool editable) {
    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_VERTICAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_align(gui, ALIGN_CENTER);
        if (editable) {
            gui_on_hover(gui, input_on_hover);
            InputHoverInfo info = (InputHoverInfo) {
                .input = input,
                .rel_pos = (Vector2) {0},
                .font = font,
                .font_size = font_size,
            };
            gui_set_state(gui, &info, sizeof(info));
        }

        gui_element_begin(gui);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_CENTER);
            gui_set_grow(gui, DIRECTION_VERTICAL);

            if (hover.select_input == input) {
                if (hover.select_input_cursor == hover.select_input_mark) hover.select_input_mark = -1;

                if (hover.select_input_mark == -1) {
                    gui_text_slice(gui, font, *input, hover.select_input_cursor, font_size, font_color);
                    gui_element_begin(gui);
                        gui_set_rect(gui, font_color);
                        gui_set_min_size(gui, BLOCK_OUTLINE_SIZE, BLOCK_TEXT_SIZE);
                    gui_element_end(gui);

                    gui_text(gui, font, *input + hover.select_input_cursor, font_size, font_color);
                } else {
                    int select_start = MIN(hover.select_input_cursor, hover.select_input_mark),
                        select_end   = MAX(hover.select_input_cursor, hover.select_input_mark);
                    gui_text_slice(gui, font, *input, select_start, font_size, font_color);

                    gui_element_begin(gui);
                        gui_set_rect(gui, (GuiColor) TEXT_SELECTION_COLOR);
                        gui_text_slice(gui, font, *input + select_start, select_end - select_start, font_size, font_color);
                    gui_element_end(gui);

                    gui_text(gui, font, *input + select_end, font_size, font_color);
                }
            } else {
                if (**input == 0) {
                    gui_text(gui, font, hint, font_size, (GuiColor) { font_color.r, font_color.g, font_color.b, font_color.a * 0.3 });
                } else {
                    gui_text(gui, font, *input, font_size, font_color);
                }
            }
        gui_element_end(gui);
    gui_element_end(gui);
}

static void draw_blockdef(Blockdef* blockdef, bool editing) {
    bool collision = hover.editor.prev_blockdef == blockdef;
    Color color = CONVERT_COLOR(blockdef->color, Color);
    Color block_color = ColorBrightness(color, collision ? 0.3 : 0.0);
    Color dropdown_color = ColorBrightness(color, collision ? 0.0 : -0.3);
    Color outline_color = ColorBrightness(color, collision ? 0.5 : -0.2);

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
        gui_set_custom_data(gui, blockdef);
        gui_on_hover(gui, blockdef_on_hover);

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_border(gui, CONVERT_COLOR(outline_color, GuiColor), BLOCK_OUTLINE_SIZE);
        gui_set_align(gui, ALIGN_CENTER);
        gui_set_min_size(gui, 0, conf.font_size);
        gui_set_padding(gui, BLOCK_OUTLINE_SIZE * 2, BLOCK_OUTLINE_SIZE * 2);
        gui_set_gap(gui, BLOCK_PADDING);

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        Input* input = &blockdef->inputs[i];

        if (hover.editor.edit_blockdef == blockdef) {
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));
                gui_set_align(gui, ALIGN_CENTER);
                gui_set_padding(gui, BLOCK_PADDING, BLOCK_PADDING);
                gui_set_gap(gui, BLOCK_PADDING);
        }

        switch (input->type) {
        case INPUT_TEXT_DISPLAY:
            if (editing) {
                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0xff });

                    gui_element_begin(gui);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);
                        gui_set_min_size(gui, conf.font_size - BLOCK_OUTLINE_SIZE * 4, conf.font_size - BLOCK_OUTLINE_SIZE * 4);
                        gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                        if (hover.select_input == &input->data.text) gui_set_border(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff }, BLOCK_OUTLINE_SIZE);
                        gui_on_hover(gui, blockdef_input_on_hover);

                        draw_input(&font_cond, &input->data.text, "", BLOCK_TEXT_SIZE, (GuiColor) { 0x00, 0x00, 0x00, 0xff }, true);
                    gui_element_end(gui);
                gui_element_end(gui);
            } else {
                gui_text(gui, &font_cond_shadow, input->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            }
            break;
        case INPUT_IMAGE_DISPLAY:
            gui_image(gui, input->data.image.image_ptr, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            break;
        case INPUT_ARGUMENT:
            draw_blockdef(input->data.arg.blockdef, editing);
            break;
        default:
            gui_text(gui, &font_cond_shadow, "NODEF", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            break;
        }

        if (hover.editor.edit_blockdef == blockdef) {
                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });
                    gui_on_hover(gui, editor_del_button_on_hover);
                    gui_set_custom_data(gui, (void*)i);

                    gui_image(gui, &textures.button_del_arg, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_element_end(gui);
            gui_element_end(gui);
        }
    }

    gui_element_end(gui);
    gui_element_end(gui);
}

static void block_on_hover(GuiElement* el) {
    if (hover.button.handler) return;
    if (hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    hover.editor.block = el->custom_data;
    hover.editor.blockchain = hover.editor.prev_blockchain;
    if (!hover.editor.block->parent) hover.editor.parent_argument = NULL;
}

static void argument_on_render(GuiElement* el) {
    hover.editor.select_block_pos = (Vector2) { el->abs_x, el->abs_y };
}

static void block_on_render(GuiElement* el) {
    hover.editor.select_block_pos = (Vector2) { el->abs_x, el->abs_y };
    hover.editor.select_valid = true;
}

static void block_argument_on_hover(GuiElement* el) {
    if (hover.button.handler) return;
    if (hover.is_panel_edit_mode) return;
    hover.editor.parent_argument = el->custom_data;
    hover.editor.blockchain = hover.editor.prev_blockchain;
}

static void argument_on_hover(GuiElement* el) {
    if (hover.button.handler) return;
    if (hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    hover.editor.argument = el->custom_data;
    hover.editor.blockchain = hover.editor.prev_blockchain;
    if (el->draw_type != DRAWTYPE_UNKNOWN) return;
    el->draw_type = DRAWTYPE_BORDER;
    el->color = (GuiColor) { 0xa0, 0xa0, 0xa0, 0xff };
    el->data.border.width = BLOCK_OUTLINE_SIZE;
    el->data.border.type = BORDER_NORMAL;
}

static void draw_block(Block* block, bool highlight, bool can_hover, bool ghost, bool editable) {
    bool collision = hover.editor.prev_block == block || highlight;
    Color color = CONVERT_COLOR(block->blockdef->color, Color);
    if (!block->blockdef->func) color = (Color) UNIMPLEMENTED_BLOCK_COLOR;
    if (!thread_is_running(&vm.thread) && block == exec_compile_error_block) {
        double animation = fmod(-GetTime(), 1.0) * 0.5 + 1.0;
        color = (Color) { 0xff * animation, 0x20 * animation, 0x20 * animation, 0xff };
    }
    if (ghost) color.a = BLOCK_GHOST_OPACITY;

    Color block_color = collision ? ColorBrightness(color, 0.3) : color;
    Color dropdown_color = collision ? color : ColorBrightness(color, -0.3);
    Color outline_color;
    if (highlight) {
        outline_color = YELLOW;
    } else if (hover.editor.select_block == block) {
        outline_color = ColorBrightness(color, 0.7);
    } else {
        outline_color = ColorBrightness(color, collision ? 0.5 : -0.2);
    }

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
        gui_set_custom_data(gui, block);
        if (block->blockdef->type == BLOCKTYPE_HAT) gui_set_rect_type(gui, RECT_NOTCHED);
        if (can_hover) gui_on_hover(gui, block_on_hover);
        if (hover.editor.select_block == block) gui_on_render(gui, block_on_render);

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_border(gui, CONVERT_COLOR(outline_color, GuiColor), BLOCK_OUTLINE_SIZE);
        gui_set_align(gui, ALIGN_CENTER);
        gui_set_min_size(gui, 0, conf.font_size);
        gui_set_padding(gui, BLOCK_OUTLINE_SIZE * 2, BLOCK_OUTLINE_SIZE * 2);
        gui_set_gap(gui, BLOCK_PADDING);
        if (block->blockdef->type == BLOCKTYPE_CONTROL) {
            gui_set_border_type(gui, BORDER_CONTROL);
        } else if (block->blockdef->type == BLOCKTYPE_CONTROLEND) {
            gui_set_border_type(gui, BORDER_CONTROL_END);
        } else if (block->blockdef->type == BLOCKTYPE_HAT) {
            gui_set_border_type(gui, BORDER_NOTCHED);
        }

    size_t arg_id = 0;
    Input* inputs = block->blockdef->inputs;
    size_t inputs_size = vector_size(inputs);
    for (size_t i = 0; i < inputs_size; i++) {
        Input* input = &inputs[i];
        Argument* arg = &block->arguments[arg_id];

        switch (input->type) {
        case INPUT_TEXT_DISPLAY:
            gui_text(gui, &font_cond_shadow, input->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, ghost ? BLOCK_GHOST_OPACITY : 0xff });
            break;
        case INPUT_IMAGE_DISPLAY: ;
            GuiColor img_color = CONVERT_COLOR(input->data.image.image_color, GuiColor);
            if (ghost) img_color.a = BLOCK_GHOST_OPACITY;
            gui_image(gui, input->data.image.image_ptr, BLOCK_IMAGE_SIZE, img_color);
            break;
        case INPUT_ARGUMENT:
            switch (arg->type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                gui_element_begin(gui);
                    if (arg->type == ARGUMENT_TEXT) {
                        gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, ghost ? BLOCK_GHOST_OPACITY : BLOCK_ARG_OPACITY });
                    } else {
                        gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));
                    }

                    gui_element_begin(gui);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);
                        gui_set_min_size(gui, conf.font_size - BLOCK_OUTLINE_SIZE * 4, conf.font_size - BLOCK_OUTLINE_SIZE * 4);
                        gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                        if (editable) {
                            if (hover.editor.select_argument == arg) {
                                gui_set_border(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff }, BLOCK_OUTLINE_SIZE);
                                gui_on_render(gui, argument_on_render);
                            }
                            gui_set_custom_data(gui, arg);
                            if (can_hover) gui_on_hover(gui, argument_on_hover);
                        }

                        if (arg->type == ARGUMENT_TEXT) {
                            if (can_hover) {
                                draw_input(
                                    &font_cond, 
                                    &arg->data.text, 
                                    input->data.arg.hint_text, 
                                    BLOCK_TEXT_SIZE, 
                                    (GuiColor) { 0x00, 0x00, 0x00, ghost ? BLOCK_GHOST_OPACITY : 0xff }, 
                                    editable
                                );
                            }
                        } else {
                            if (can_hover) {
                                draw_input(
                                    &font_cond_shadow, 
                                    &arg->data.text, 
                                    input->data.arg.hint_text, 
                                    BLOCK_TEXT_SIZE, 
                                    (GuiColor) { 0xff, 0xff, 0xff, ghost ? BLOCK_GHOST_OPACITY : 0xff }, 
                                    editable
                                );
                            }
                        }
                    gui_element_end(gui);
                gui_element_end(gui);
                break;
            case ARGUMENT_BLOCK:
                gui_element_begin(gui);
                    if (can_hover) gui_on_hover(gui, block_argument_on_hover);
                    gui_set_custom_data(gui, arg);

                    draw_block(&arg->data.block, highlight, can_hover, ghost, editable);
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
                gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));

                if (hover.editor.select_argument == arg && hover.dropdown.location == LOCATION_BLOCK_DROPDOWN) {
                    hover.dropdown.element = gui_get_element(gui);
                }

                gui_element_begin(gui);
                    gui_set_min_size(gui, 0, conf.font_size - BLOCK_OUTLINE_SIZE * 4);
                    gui_set_align(gui, ALIGN_CENTER);
                    gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                    gui_set_direction(gui, DIRECTION_HORIZONTAL);
                    if (editable) {
                        if (hover.editor.select_argument == arg) gui_set_border(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff }, BLOCK_OUTLINE_SIZE);
                        if (can_hover) gui_on_hover(gui, argument_on_hover);
                        gui_set_custom_data(gui, arg);
                    }

                    gui_text(gui, &font_cond_shadow, arg->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                    gui_image(gui, &textures.dropdown, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_element_end(gui);
            gui_element_end(gui);
            arg_id++;
            break;
        case INPUT_BLOCKDEF_EDITOR:
            assert(arg->type == ARGUMENT_BLOCKDEF);
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));
                gui_set_align(gui, ALIGN_CENTER);
                gui_set_gap(gui, BLOCK_PADDING);
                gui_set_custom_data(gui, arg);
                if (can_hover) gui_on_hover(gui, argument_on_hover);

                draw_blockdef(arg->data.blockdef, hover.editor.edit_blockdef == arg->data.blockdef);

                if (hover.editor.edit_blockdef == arg->data.blockdef) {
                    draw_editor_button(&textures.button_add_arg, handle_editor_add_arg_button);
                    draw_editor_button(&textures.button_add_text, handle_editor_add_text_button);
                    draw_editor_button(&textures.button_close, handle_editor_close_button);
                } else {
                    draw_editor_button(&textures.button_edit, handle_editor_edit_button);
                }

                gui_spacer(gui, 0, 0);
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

static void tab_button_add_on_hover(GuiElement* el) {
    if (gui_window_is_shown()) return;
    if (hover.button.handler) return;
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    hover.button.handler = handle_add_tab_button;
    hover.button.data = el->custom_data;
}

static void tab_button_on_hover(GuiElement* el) {
    if (gui_window_is_shown()) return;
    if (hover.button.handler) return;
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    hover.button.handler = handle_tab_button;
    hover.button.data = el->custom_data;
}

static void button_on_hover(GuiElement* el) {
    if (hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (hover.button.handler) return;
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    hover.button.handler = el->custom_data;
}

static void panel_editor_button_on_hover(GuiElement* el) {
    if (!hover.is_panel_edit_mode) return;
    if (hover.button.handler) return;

    Color color = ColorBrightness(CONVERT_COLOR(el->color, Color), -0.13);
    el->color = CONVERT_COLOR(color, GuiColor);
    hover.button.handler = el->custom_data;
}

static void draw_panel_editor_button(const char* text, int size, GuiColor color, ButtonClickHandler handler) {
    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_align(gui, ALIGN_CENTER);
        gui_set_min_size(gui, 0, size);
        gui_set_padding(gui, conf.font_size * 0.3, 0);
        gui_set_rect(gui, color);
        gui_on_hover(gui, panel_editor_button_on_hover);
        gui_set_custom_data(gui, handler);

        gui_text(gui, &font_cond, text, BLOCK_TEXT_SIZE, (GuiColor) { 0x00, 0x00, 0x00, 0xff });
    gui_element_end(gui);
}

static GuiElement* draw_button(const char* text, int size, bool selected, GuiHandler on_hover, void* custom_data) {
    GuiElement* el;
    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_align(gui, ALIGN_CENTER);
        gui_set_min_size(gui, 0, size);
        gui_set_padding(gui, conf.font_size * 0.3, 0);
        if (selected) gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        gui_on_hover(gui, on_hover);
        gui_set_custom_data(gui, custom_data);
        el = gui_get_element(gui);

        gui_text(gui, &font_cond, text, BLOCK_TEXT_SIZE, selected ? (GuiColor) { 0x00, 0x00, 0x00, 0xff } : (GuiColor) { 0xff, 0xff, 0xff, 0xff });
    gui_element_end(gui);
    return el;
}

static void draw_top_bar(void) {
    const int top_bar_size = conf.font_size * 1.2;
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
        gui_set_min_size(gui, 0, top_bar_size);
        gui_set_align(gui, ALIGN_CENTER);

        gui_spacer(gui, 5, 0);
        gui_image(gui, &textures.icon_logo, conf.font_size, CONVERT_COLOR(WHITE, GuiColor));
        gui_spacer(gui, 10, 0);
        gui_text(gui, &font_eb, gettext("Scrap"), conf.font_size * 0.8, CONVERT_COLOR(WHITE, GuiColor));
        gui_spacer(gui, 10, 0);

        GuiElement* el = draw_button(gettext("File"), top_bar_size, false, button_on_hover, handle_file_button_click);
        if (hover.dropdown.location == LOCATION_FILE_MENU) hover.dropdown.element = el;
        draw_button(gettext("Settings"), top_bar_size, false, button_on_hover, handle_settings_button_click);
        draw_button(gettext("About"), top_bar_size, false, button_on_hover, handle_about_button_click);
    gui_element_end(gui);
}

static void draw_tab_bar(void) {
    const int tab_bar_size = conf.font_size;
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
        gui_set_min_size(gui, 0, tab_bar_size);
        gui_set_align(gui, ALIGN_CENTER);

        if (hover.is_panel_edit_mode && hover.panels.mouse_panel != PANEL_NONE) {
            draw_button("+", tab_bar_size, false, tab_button_add_on_hover, (void*)0);
        }
        for (size_t i = 0; i < vector_size(code_tabs); i++) {
            draw_button(gettext(code_tabs[i].name), tab_bar_size, current_tab == (int)i, tab_button_on_hover, (void*)i);
            if (hover.is_panel_edit_mode && hover.panels.mouse_panel != PANEL_NONE) {
                draw_button("+", tab_bar_size, false, tab_button_add_on_hover, (void*)(i + 1));
            }
        }

        gui_grow(gui, DIRECTION_HORIZONTAL);
        gui_text(gui, &font_cond, project_name, BLOCK_TEXT_SIZE, (GuiColor) { 0x80, 0x80, 0x80, 0xff });
        gui_grow(gui, DIRECTION_HORIZONTAL);

#ifndef USE_INTERPRETER
        gui_element_begin(gui);
            gui_on_hover(gui, button_on_hover);
            gui_set_custom_data(gui, handle_build_button_click);

            gui_image(gui, &textures.button_build, tab_bar_size, (GuiColor) { 0xff, 0x99, 0x00, 0xff });
        gui_element_end(gui);

        gui_spacer(gui, conf.font_size * 0.2, 0);
#endif

        gui_element_begin(gui);
            gui_on_hover(gui, button_on_hover);
            gui_set_custom_data(gui, handle_stop_button_click);

            if (vm.thread.state == THREAD_STATE_STOPPING) {
                gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_image(gui, &textures.button_stop, tab_bar_size, (GuiColor) { 0x00, 0x00, 0x00, 0xff });
            } else {
                gui_image(gui, &textures.button_stop, tab_bar_size, (GuiColor) { 0xff, 0x40, 0x30, 0xff });
            }
        gui_element_end(gui);

        gui_spacer(gui, conf.font_size * 0.2, 0);

        gui_element_begin(gui);
            gui_on_hover(gui, button_on_hover);
            gui_set_custom_data(gui, handle_run_button_click);

            if (thread_is_running(&vm.thread)) {
                gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_image(gui, &textures.button_run, tab_bar_size, (GuiColor) { 0x00, 0x00, 0x00, 0xff });
            } else {
                gui_image(gui, &textures.button_run, tab_bar_size, (GuiColor) { 0x60, 0xff, 0x00, 0xff });
            }
        gui_element_end(gui);
    gui_element_end(gui);
}

static void blockchain_on_hover(GuiElement* el) {
    if (hover.is_panel_edit_mode) return;
    hover.editor.prev_blockchain = el->custom_data;
}

static void draw_block_preview(BlockChain* chain) {
    if (chain == &mouse_blockchain) return;
    if (vector_size(mouse_blockchain.blocks) == 0) return;
    if (hover.editor.prev_argument != NULL) return;
    if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return;

    draw_blockchain(&mouse_blockchain, true, false, false);
}

static void draw_blockchain(BlockChain* chain, bool ghost, bool show_previews, bool editable_arguments) {
    int layer = 0;
    bool highlight = vm.exec_chain == chain;

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_VERTICAL);
        gui_on_hover(gui, blockchain_on_hover);
        gui_set_custom_data(gui, chain);

    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        Blockdef* blockdef = chain->blocks[i].blockdef;
        bool block_highlight = vm.exec_ind == i;

        if (blockdef->type == BLOCKTYPE_END) {
            gui_element_end(gui);
            gui_element_end(gui);

            GuiElement* el = gui_get_element(gui);

            Block* block = el->custom_data;

            bool collision = hover.editor.prev_block == &chain->blocks[i] || (highlight && block_highlight);
            Color color = CONVERT_COLOR(block->blockdef->color, Color);
            if (ghost) color.a = BLOCK_GHOST_OPACITY;
            Color block_color = ColorBrightness(color, collision ? 0.3 : 0.0);
            Color outline_color;
            if (highlight && block_highlight) {
                outline_color = YELLOW;
            } else if (hover.editor.select_block == &chain->blocks[i]) {
                outline_color = ColorBrightness(color, 0.7);
            } else {
                outline_color = ColorBrightness(color, collision ? 0.5 : -0.2);
            }

            gui_element_begin(gui);
                gui_set_min_size(gui, block->width, conf.font_size);
                gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
                gui_on_hover(gui, block_on_hover);
                if (hover.editor.select_block == &chain->blocks[i]) gui_on_render(gui, block_on_render);
                gui_set_custom_data(gui, &chain->blocks[i]);

                gui_element_begin(gui);
                    gui_set_grow(gui, DIRECTION_VERTICAL);
                    gui_set_grow(gui, DIRECTION_HORIZONTAL);
                    gui_set_border(gui, CONVERT_COLOR(outline_color, GuiColor), BLOCK_OUTLINE_SIZE);
                    gui_set_border_type(gui, BORDER_END);
                gui_element_end(gui);
            gui_element_end(gui);

            layer--;
            gui_element_end(gui);
        } else if (blockdef->type == BLOCKTYPE_CONTROLEND) {
            if (layer > 0) {
                gui_element_end(gui);
                gui_element_end(gui);
                gui_element_end(gui);
                layer--;
            }
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_VERTICAL);
                gui_set_custom_data(gui, &chain->blocks[i]);
        } else if (blockdef->type == BLOCKTYPE_CONTROL) {
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_VERTICAL);
                gui_set_custom_data(gui, &chain->blocks[i]);
        }

        if (blockdef->type != BLOCKTYPE_END) {
            draw_block(&chain->blocks[i], highlight && block_highlight, true, ghost, editable_arguments);
        }

        if (blockdef->type == BLOCKTYPE_CONTROL || blockdef->type == BLOCKTYPE_CONTROLEND) {
            layer++;

            GuiElement* el = gui_get_element(gui);
            chain->blocks[i].width = el->w;

            bool collision = hover.editor.prev_block == &chain->blocks[i] || (highlight && block_highlight);
            Color color = CONVERT_COLOR(blockdef->color, Color);
            if (ghost) color.a = BLOCK_GHOST_OPACITY;
            Color block_color = ColorBrightness(color, collision ? 0.3 : 0.0);
            Color outline_color;
            if (highlight && block_highlight) {
                outline_color = YELLOW;
            } else if (hover.editor.select_block == &chain->blocks[i]) {
                outline_color = ColorBrightness(color, 0.7);
            } else {
                outline_color = ColorBrightness(color, collision ? 0.5 : -0.2);
            }

            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);

                gui_element_begin(gui);
                    gui_set_grow(gui, DIRECTION_VERTICAL);
                    gui_set_min_size(gui, BLOCK_CONTROL_INDENT, conf.font_size / 2);
                    gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
                    gui_on_hover(gui, block_on_hover);
                    gui_set_custom_data(gui, &chain->blocks[i]);

                    gui_element_begin(gui);
                        gui_set_grow(gui, DIRECTION_VERTICAL);
                        gui_set_grow(gui, DIRECTION_HORIZONTAL);
                        gui_set_border(gui, CONVERT_COLOR(outline_color, GuiColor), BLOCK_OUTLINE_SIZE);
                        gui_set_border_type(gui, BORDER_CONTROL_BODY);
                    gui_element_end(gui);
                gui_element_end(gui);

                gui_element_begin(gui);
                    gui_set_direction(gui, DIRECTION_VERTICAL);

                    if (hover.editor.prev_block == &chain->blocks[i] && show_previews) {
                        draw_block_preview(chain);
                    }
        } else {
            if (hover.editor.prev_block == &chain->blocks[i] && show_previews) {
                draw_block_preview(chain);
            }
        }
    }

    while (layer > 0) {
        gui_element_end(gui);
        gui_element_end(gui);
        gui_element_end(gui);
        layer--;
    }

    gui_element_end(gui);
}

static void category_on_hover(GuiElement* el) {
    if (hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (hover.button.handler) return;

    el->color.a = 0x80;
    hover.button.handler = handle_category_click;
    hover.category = el->custom_data;
}

static void draw_category(BlockCategory* category, bool selected) {
    GuiColor color = CONVERT_COLOR(category->color, GuiColor);
    color.a = 0x40;

    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, color);
        gui_on_hover(gui, category_on_hover);
        gui_set_custom_data(gui, category);

        color.a = 0xff;
        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            if (selected) gui_set_border(gui, color, BLOCK_OUTLINE_SIZE);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_padding(gui, BLOCK_PADDING, BLOCK_PADDING);
            gui_set_min_size(gui, 0, conf.font_size);
            gui_set_align(gui, ALIGN_CENTER);
            gui_set_gap(gui, BLOCK_PADDING);

            gui_element_begin(gui);
                gui_set_min_size(gui, conf.font_size * 0.5, conf.font_size * 0.5);
                gui_set_rect(gui, color);
            gui_element_end(gui);

            gui_text(gui, &font_cond_shadow, category->name, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        gui_element_end(gui);
    gui_element_end(gui);
}

static void draw_block_categories(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) PANEL_BACKGROUND_COLOR);
        gui_set_padding(gui, SIDE_BAR_PADDING, SIDE_BAR_PADDING);
        gui_set_gap(gui, SIDE_BAR_PADDING);
        gui_set_scroll(gui, &categories_scroll);
        gui_set_scissor(gui);

        for (size_t i = 0; i < vector_size(palette.categories); i += 2) {
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_gap(gui, SIDE_BAR_PADDING);

                draw_category(&palette.categories[i], (int)i == palette.current_category);
                if (i + 1 < vector_size(palette.categories)) {
                    draw_category(&palette.categories[i + 1], (int)i + 1 == palette.current_category);
                } else {
                    gui_grow(gui, DIRECTION_HORIZONTAL);
                }
            gui_element_end(gui);
        }
    gui_element_end(gui);
}

static void draw_block_palette(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) PANEL_BACKGROUND_COLOR);
        gui_set_padding(gui, SIDE_BAR_PADDING, SIDE_BAR_PADDING);
        gui_set_gap(gui, SIDE_BAR_PADDING);
        gui_set_scroll(gui, &palette.scroll_amount);
        gui_set_scroll_scaling(gui, conf.font_size * 4);
        gui_set_scissor(gui);

        for (size_t i = 0; i < vector_size(palette.categories[palette.current_category].chains); i++) {
            draw_blockchain(&palette.categories[palette.current_category].chains[i], false, false, false);
        }
    gui_element_end(gui);
}

static void code_area_on_render(GuiElement* el) {
    hover.panels.code_panel_bounds = (Rectangle) { el->abs_x, el->abs_y, el->w, el->h };
}

static void draw_code_area(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_direction(gui, DIRECTION_VERTICAL);
        gui_set_padding(gui, 0, 0);
        gui_set_align(gui, ALIGN_RIGHT);
        gui_set_scissor(gui);
        gui_on_render(gui, code_area_on_render);

        draw_code();

        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, 0, 0);
            gui_set_padding(gui, conf.font_size * 0.2, conf.font_size * 0.2);
            for (int i = 0; i < DEBUG_BUFFER_LINES; i++) {
                if (*debug_buffer[i]) gui_text(gui, &font_cond, debug_buffer[i], conf.font_size * 0.5, (GuiColor) { 0xff, 0xff, 0xff, 0x60 });
            }
        gui_element_end(gui);

        if (!thread_is_running(&vm.thread) && vector_size(exec_compile_error) > 0) {
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_CENTER);
                gui_set_gap(gui, conf.font_size * 0.5);
                gui_set_padding(gui, conf.font_size * 0.4, conf.font_size * 0.4);
                gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x80 });

                double animation = (fmod(-GetTime(), 1.0) * 0.5 + 1.0) * 255.0;
                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0xff, 0x20, 0x20, animation });
                    gui_set_fixed(gui, conf.font_size, conf.font_size);
                    gui_set_direction(gui, DIRECTION_VERTICAL);
                    gui_set_align(gui, ALIGN_CENTER);

                    gui_text(gui, &font_eb, "!", conf.font_size, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_element_end(gui);

                gui_element_begin(gui);
                    gui_set_direction(gui, DIRECTION_VERTICAL);

                    gui_text(gui, &font_cond, gettext("Got compiler error!"), conf.font_size * 0.6, (GuiColor) { 0xff, 0x33, 0x33, 0xff });
                    for (size_t i = 0; i < vector_size(exec_compile_error); i++) {
                        gui_text(gui, &font_cond, exec_compile_error[i], conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                    }

                    gui_spacer(gui, 0, conf.font_size * 0.5);

                    gui_element_begin(gui);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);   
                        gui_set_gap(gui, conf.font_size * 0.5);

                        if (exec_compile_error_block) {
                            gui_element_begin(gui);
                                gui_set_border(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff }, BLOCK_OUTLINE_SIZE);
                                draw_button(gettext("Jump to block"), conf.font_size, false, button_on_hover, handle_jump_to_block_button_click);
                            gui_element_end(gui);
                        }

                        gui_element_begin(gui);
                            gui_set_border(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff }, BLOCK_OUTLINE_SIZE);
                            draw_button(gettext("Close"), conf.font_size, false, button_on_hover, handle_error_window_close_button_click);
                        gui_element_end(gui);
                    gui_element_end(gui);
                gui_element_end(gui);
            gui_element_end(gui);
        } else {
            gui_spacer(gui, 0, conf.font_size * 1.5);
        }

        if (actionbar.show_time > 0) {
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_direction(gui, DIRECTION_VERTICAL);
                gui_set_align(gui, ALIGN_CENTER);
                
                Color color = YELLOW;
                color.a = actionbar.show_time / 3.0 * 255.0;
                gui_text(gui, &font_eb, actionbar.text, conf.font_size * 0.8, CONVERT_COLOR(color, GuiColor));
            gui_element_end(gui);
        }
    gui_element_end(gui);
}

static void draw_split_preview(PanelTree* panel) {
    if (!hover.is_panel_edit_mode) return;
    if (hover.panels.prev_panel != panel) return;

    if (hover.panels.mouse_panel == PANEL_NONE) {
        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, 0, 0);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_rect(gui, (GuiColor) { 0x00, 0xff, 0xff, 0x20 });

            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_border(gui, (GuiColor) { 0x00, 0xff, 0xff, 0x80 }, BLOCK_OUTLINE_SIZE);
            gui_element_end(gui);
        gui_element_end(gui);
        return;
    }

    if (hover.panels.panel_side == SPLIT_SIDE_NONE) return;

    gui_element_begin(gui);
        gui_set_floating(gui);
        gui_set_position(gui, 0, 0);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);

        if (hover.panels.panel_side == SPLIT_SIDE_LEFT || hover.panels.panel_side == SPLIT_SIDE_RIGHT) gui_set_direction(gui, DIRECTION_HORIZONTAL);

        if (hover.panels.panel_side == SPLIT_SIDE_BOTTOM) gui_grow(gui, DIRECTION_VERTICAL);
        if (hover.panels.panel_side == SPLIT_SIDE_RIGHT) gui_grow(gui, DIRECTION_HORIZONTAL);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_rect(gui, (GuiColor) { 0x00, 0xff, 0xff, 0x20 });

            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_border(gui, (GuiColor) { 0x00, 0xff, 0xff, 0x80 }, BLOCK_OUTLINE_SIZE);
            gui_element_end(gui);
        gui_element_end(gui);

        if (hover.panels.panel_side == SPLIT_SIDE_TOP) gui_grow(gui, DIRECTION_VERTICAL);
        if (hover.panels.panel_side == SPLIT_SIDE_LEFT) gui_grow(gui, DIRECTION_HORIZONTAL);
    gui_element_end(gui);
}

static void draw_term_panel(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_padding(gui, conf.font_size * 0.5, conf.font_size * 0.5);
        gui_set_rect(gui, (GuiColor) PANEL_BACKGROUND_COLOR);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0xff });
            gui_set_rect_type(gui, RECT_TERMINAL);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void panel_on_hover(GuiElement* el) {
    hover.panels.panel = el->custom_data;
    hover.panels.panel_size = (Rectangle) { el->abs_x, el->abs_y, el->w, el->h };

    if (hover.panels.panel->type == PANEL_SPLIT) return;

    int mouse_x = gui->mouse_x - el->abs_x;
    int mouse_y = gui->mouse_y - el->abs_y;

    bool is_top_right = mouse_y < ((float)el->h / (float)el->w) * mouse_x;
    bool is_top_left = mouse_y < -((float)el->h / (float)el->w * mouse_x) + el->h;

    if (is_top_right) {
        if (is_top_left) {
            hover.panels.panel_side = SPLIT_SIDE_TOP;
        } else {
            hover.panels.panel_side = SPLIT_SIDE_RIGHT;
        }
    } else {
        if (is_top_left) {
            hover.panels.panel_side = SPLIT_SIDE_LEFT;
        } else {
            hover.panels.panel_side = SPLIT_SIDE_BOTTOM;
        }
    }
}

static void draw_panel(PanelTree* panel) {
    if (panel->type != PANEL_SPLIT && !panel->parent) {
        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_on_hover(gui, panel_on_hover);
            gui_set_custom_data(gui, panel);
    }

    switch (panel->type) {
    case PANEL_NONE:
        assert(false && "Attempt to render panel with type PANEL_NONE");
        break;
    case PANEL_BLOCK_PALETTE:
        draw_block_palette();
        break;
    case PANEL_CODE:
        draw_code_area();
        break;
    case PANEL_TERM:
        draw_term_panel();
        break;
    case PANEL_BLOCK_CATEGORIES:
        draw_block_categories();
        break;
    case PANEL_SPLIT:
        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, panel->direction);
            gui_on_hover(gui, panel_on_hover);
            gui_set_custom_data(gui, panel);

            gui_element_begin(gui);
                if (panel->direction == DIRECTION_VERTICAL) {
                    gui_set_percent_size(gui, panel->split_percent, DIRECTION_VERTICAL);
                    gui_set_grow(gui, DIRECTION_HORIZONTAL);
                } else {
                    gui_set_grow(gui, DIRECTION_VERTICAL);
                    gui_set_percent_size(gui, panel->split_percent, DIRECTION_HORIZONTAL);
                }

                if (panel->left->type != PANEL_SPLIT) {
                    gui_on_hover(gui, panel_on_hover);
                    gui_set_custom_data(gui, panel->left);
                }

                draw_panel(panel->left);
            gui_element_end(gui);

            if (hover.is_panel_edit_mode) {
                gui_element_begin(gui);
                    if (panel->direction == DIRECTION_HORIZONTAL) {
                        gui_set_grow(gui, DIRECTION_VERTICAL);
                    } else {
                        gui_set_grow(gui, DIRECTION_HORIZONTAL);
                    }
                    gui_set_min_size(gui, 10, 10);
                    gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, hover.panels.drag_panel == panel ? 0x20 : hover.panels.prev_panel == panel ? 0x80 : 0x40 });
                gui_element_end(gui);
            }

            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);

                if (panel->right->type != PANEL_SPLIT) {
                    gui_on_hover(gui, panel_on_hover);
                    gui_set_custom_data(gui, panel->right);
                }

                draw_panel(panel->right);
            gui_element_end(gui);
        gui_element_end(gui);
        break;
    }
    if (panel->type != PANEL_SPLIT) draw_split_preview(panel);
    if (panel->type != PANEL_SPLIT && !panel->parent) gui_element_end(gui);
}

static void draw_code(void) {
    for (size_t i = 0; i < vector_size(editor_code); i++) {
        Vector2 chain_pos = (Vector2) {
            editor_code[i].x - camera_pos.x,
            editor_code[i].y - camera_pos.y,
        };
        Rectangle code_size = hover.panels.code_panel_bounds;
        if (&editor_code[i] != hover.editor.select_blockchain) {
            if (chain_pos.x > code_size.width || chain_pos.y > code_size.height) continue;
            if (editor_code[i].width > 0 && editor_code[i].height > 0 &&
                (chain_pos.x + editor_code[i].width < 0 || chain_pos.y + editor_code[i].height < 0)) continue;
        }
        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, chain_pos.x, chain_pos.y);

            draw_blockchain(&editor_code[i], false, true, true);
        gui_element_end(gui);
        GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len];
        editor_code[i].width = el->w;
        editor_code[i].height = el->h;
    }
}

static void dropdown_on_hover(GuiElement* el) {
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    // Double cast to avoid warning. In our case this operation is safe because el->custom_data currently stores a value of type int
    hover.dropdown.select_ind = (int)(size_t)el->custom_data;

    hover.button.handler = hover.dropdown.handler;
}

static void draw_dropdown(void) {
    const int max_list_size = 10;

    if (!hover.dropdown.location) return;
    hover.button.handler = handle_dropdown_close;
    gui_element_begin(gui);
        gui_set_floating(gui);
        gui_set_rect(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff });
        gui_set_gap(gui, 2);
        gui_set_padding(gui, 2, 2);
        gui_set_parent_anchor(gui, hover.dropdown.element);
        gui_set_position(gui, 0, hover.dropdown.element->h);
        if (hover.dropdown.list_len > max_list_size) {
            gui_set_scissor(gui);
            gui_set_fixed(gui, hover.dropdown.element->w + 5, max_list_size * (conf.font_size + 2) + 4);
            gui_set_scroll(gui, &hover.dropdown.scroll_amount);
            gui_set_scroll_scaling(gui, (conf.font_size + 2) * 2);
        } else {
            gui_set_min_size(gui, hover.dropdown.element->w, 0);
        }

        for (int i = 0; i < hover.dropdown.list_len; i++) {
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_CENTER);
                gui_set_min_size(gui, 0, conf.font_size);
                gui_set_padding(gui, conf.font_size * 0.3, 0);
                gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
                gui_on_hover(gui, dropdown_on_hover);
                gui_set_custom_data(gui, (void*)(size_t)i);

                const char* list_value = hover.dropdown.location != LOCATION_BLOCK_DROPDOWN ?
                                         gettext(hover.dropdown.list[i]) :
                                         hover.dropdown.list[i];
                gui_text(gui, &font_cond, list_value, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_element_end(gui);
        }
    gui_element_end(gui);
}

static void search_on_hover(GuiElement* el) {
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    hover.editor.block = el->custom_data;
}

static void draw_search_list(void) {
    gui_element_begin(gui);
        gui_set_floating(gui);
        gui_set_position(gui, search_list_pos.x, search_list_pos.y);
        gui_set_rect(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff });
        gui_set_gap(gui, BLOCK_OUTLINE_SIZE);
        gui_set_padding(gui, BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE);

        gui_element_begin(gui);
            gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_padding(gui, BLOCK_OUTLINE_SIZE, 0);
            gui_set_min_size(gui, 0, conf.font_size);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_CENTER);

            gui_element_begin(gui);
                draw_input(&font_cond, &search_list_search, "Search...", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff }, true);
            gui_element_end(gui);
        gui_element_end(gui);

        gui_element_begin(gui);
            gui_set_fixed(gui, 0, conf.font_size * 5);
            gui_set_fit(gui, DIRECTION_HORIZONTAL);
            gui_set_gap(gui, BLOCK_OUTLINE_SIZE);
            gui_set_scissor(gui);
            gui_set_scroll(gui, &search_list_scroll);

            for (size_t i = 0; i < vector_size(search_list); i++) {
                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
                    gui_set_grow(gui, DIRECTION_HORIZONTAL);
                    gui_on_hover(gui, search_on_hover);
                    gui_set_custom_data(gui, search_list[i]);

                    draw_block(search_list[i], false, false, false, false);
                gui_element_end(gui);
            }

        gui_element_end(gui);
    gui_element_end(gui);
}

static void panel_editor_on_hover(GuiElement* el) {
    (void) el;
    if (!hover.is_panel_edit_mode) return;
    hover.panels.panel = NULL;
}

void scrap_gui_process(void) {
    gui_begin(gui);
        draw_top_bar();
        draw_tab_bar();
        GuiElement* tab_bar_anchor = NULL;

        if (hover.is_panel_edit_mode) {
            gui_element_begin(gui);
                tab_bar_anchor = gui_get_element(gui);
            gui_element_end(gui);
        }

        draw_panel(code_tabs[current_tab].root_panel);
        draw_window();

        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, gui->mouse_x + 5, gui->mouse_y + 5);

            draw_blockchain(&mouse_blockchain, false, false, false);
        gui_element_end(gui);

        if (hover.select_input == &search_list_search) {
            draw_search_list();
        } else {
            search_list_pos = (Vector2) { gui->mouse_x, gui->mouse_y };
        }

        if (hover.is_panel_edit_mode) {
            if (hover.panels.mouse_panel != PANEL_NONE) {
                gui_element_begin(gui);
                    gui_set_floating(gui);
                    gui_set_fixed(gui, gui->win_w * 0.3, gui->win_h * 0.3);
                    gui_set_position(gui, gui->mouse_x, gui->mouse_y);

                    PanelTree panel = (PanelTree) {
                        .type = hover.panels.mouse_panel,
                        .parent = NULL,
                        .left = NULL,
                        .right = NULL,
                    };
                    draw_panel(&panel);
                gui_element_end(gui);
            }

            gui_element_begin(gui);
                gui_set_floating(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_position(gui, 0, 0);
                gui_set_parent_anchor(gui, tab_bar_anchor);
                gui_set_align(gui, ALIGN_CENTER);
                gui_set_padding(gui, 0, conf.font_size);

                gui_element_begin(gui);
                    gui_set_padding(gui, conf.font_size * 0.3, conf.font_size * 0.3);
                    gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x80 });
                    gui_set_align(gui, ALIGN_CENTER);
                    gui_on_hover(gui, panel_editor_on_hover);

                    gui_text(gui, &font_eb, gettext("Panel edit mode"), conf.font_size * 0.8, (GuiColor) { 0xff, 0xff, 0xff, 0xff });

                    gui_spacer(gui, 0, conf.font_size * 0.25);

                    gui_text(gui, &font_cond_shadow, gettext("Click on panels to reposition them"), BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                    gui_text(gui, &font_cond_shadow, gettext("Drag panel edges to resize them"), BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });

                    gui_spacer(gui, 0, conf.font_size * 0.25);

                    gui_element_begin(gui);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);
                        gui_set_gap(gui, conf.font_size * 0.25);

                        draw_panel_editor_button(gettext("Save"), conf.font_size, (GuiColor) { 0x40, 0xff, 0x40, 0xff }, handle_panel_editor_save_button);
                        draw_panel_editor_button(gettext("Done"), conf.font_size, (GuiColor) { 0x80, 0x80, 0x80, 0xff }, handle_panel_editor_cancel_button);
                    gui_element_end(gui);
                gui_element_end(gui);
            gui_element_end(gui);
        }

        draw_dropdown();
    gui_end(gui);
}

// Adopted from Raylib 5.0
bool svg_load(const char* file_name, size_t width, size_t height, Image* out_image) {
    if (!file_name) return false;

    // Bug in Raylib 5.0:
    // LoadFileData() does not return null-terminated string which nsvgParse expects, so
    // i am using nsvgParseFromFile() here instead
    NSVGimage* svg = nsvgParseFromFile(file_name, "px", 96.0);
    if (!svg) {
        TraceLog(LOG_WARNING, "[SVG] Could not load \"%s\"", file_name);
        return false;
    }
    unsigned char* image_data = malloc(width * height * 4);

    float scale_width  = width  / svg->width,
          scale_height = height / svg->height,
          scale        = MAX(scale_width, scale_height);

    int offset_x = 0,
        offset_y = 0;

    if (scale_height > scale_width) {
        offset_y = (height - svg->height * scale) / 2;
    } else {
        offset_x = (width - svg->width * scale) / 2;
    }

    NSVGrasterizer *rast = nsvgCreateRasterizer();
    nsvgRasterize(rast, svg, offset_x, offset_y, scale, image_data, width, height, width*4);

    out_image->data    = image_data;
    out_image->width   = width;
    out_image->height  = height;
    out_image->mipmaps = 1;
    out_image->format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    nsvgDeleteRasterizer(rast);
    nsvgDelete(svg);
    return true;
}

// Draw order for render_border_control()
//
//           1
//   +---------------+
// 4 |               | 2
//   +     +---------+
//             3
//
static void render_border_control(GuiDrawCommand* cmd) {
    unsigned short border_w = cmd->data.border.width;
    Color color = CONVERT_COLOR(cmd->color, Color);

    /* 1 */ DrawRectangle(cmd->pos_x, cmd->pos_y, cmd->width, border_w, color);
    /* 2 */ DrawRectangle(cmd->pos_x + cmd->width - border_w, cmd->pos_y, border_w, cmd->height, color);
    /* 3 */ DrawRectangle(cmd->pos_x + BLOCK_CONTROL_INDENT - border_w, cmd->pos_y + cmd->height - border_w, cmd->width - BLOCK_CONTROL_INDENT, border_w, color);
    /* 4 */ DrawRectangle(cmd->pos_x, cmd->pos_y, border_w, cmd->height, color);
}

// Draw order for render_border_control_body()
//
//   +     +
// 1 |     | 2
//   +     +
//
static void render_border_control_body(GuiDrawCommand* cmd) {
    unsigned short border_w = cmd->data.border.width;
    Color color = CONVERT_COLOR(cmd->color, Color);

    /* 1 */ DrawRectangle(cmd->pos_x, cmd->pos_y, border_w, cmd->height, color);
    /* 2 */ DrawRectangle(cmd->pos_x + cmd->width - border_w, cmd->pos_y, border_w, cmd->height, color);
}

// Draw order for render_border_control_end()
//
//              1
//   +     +---------+
// 4 |               | 2
//   +     +---------+
//              3
//
static void render_border_control_end(GuiDrawCommand* cmd) {
    unsigned short border_w = cmd->data.border.width;
    Color color = CONVERT_COLOR(cmd->color, Color);

    /* 1 */ DrawRectangle(cmd->pos_x + BLOCK_CONTROL_INDENT - border_w, cmd->pos_y, cmd->width - BLOCK_CONTROL_INDENT, border_w, color);
    /* 2 */ DrawRectangle(cmd->pos_x + cmd->width - border_w, cmd->pos_y, border_w, cmd->height, color);
    /* 3 */ DrawRectangle(cmd->pos_x + BLOCK_CONTROL_INDENT - border_w, cmd->pos_y + cmd->height - border_w, cmd->width - BLOCK_CONTROL_INDENT, border_w, color);
    /* 4 */ DrawRectangle(cmd->pos_x, cmd->pos_y, border_w, cmd->height, color);
}

// Draw order for render_border_end()
//
//              1
//   +     +---------+
// 4 |               | 2
//   +---------------+
//           3
static void render_border_end(GuiDrawCommand* cmd) {
    unsigned short border_w = cmd->data.border.width;
    Color color = CONVERT_COLOR(cmd->color, Color);

    /* 1 */ DrawRectangle(cmd->pos_x + BLOCK_CONTROL_INDENT - border_w, cmd->pos_y, cmd->width - BLOCK_CONTROL_INDENT, border_w, color);
    /* 2 */ DrawRectangle(cmd->pos_x + cmd->width - border_w, cmd->pos_y, border_w, cmd->height, color);
    /* 3 */ DrawRectangle(cmd->pos_x, cmd->pos_y + cmd->height - border_w, cmd->width, border_w, color);
    /* 4 */ DrawRectangle(cmd->pos_x, cmd->pos_y, border_w, cmd->height, color);
}

// Draw order for render_border_notched() and render_rect_notched()
//
//           1
//   +--------------+ 2
//   |               +
// 5 |               | 3
//   +---------------+
//           4
static void render_border_notched(GuiDrawCommand* cmd) {
    unsigned short border_w = cmd->data.border.width;
    Color color = CONVERT_COLOR(cmd->color, Color);
    int notch_size = conf.font_size / 4;

    /* 1 */ DrawRectangle(cmd->pos_x, cmd->pos_y, cmd->width - notch_size, border_w, color);
    /* 2 */ DrawRectanglePro((Rectangle) {
        cmd->pos_x + cmd->width - notch_size,
        cmd->pos_y,
        sqrtf((notch_size * notch_size) * 2),
        border_w,
    }, (Vector2) {0}, 45.0, color);
    /* 3 */ DrawRectangle(cmd->pos_x + cmd->width - border_w, cmd->pos_y + notch_size, border_w, cmd->height - notch_size, color);
    /* 4 */ DrawRectangle(cmd->pos_x, cmd->pos_y + cmd->height - border_w, cmd->width, border_w, color);
    /* 5 */ DrawRectangle(cmd->pos_x, cmd->pos_y, border_w, cmd->height, color);
}

static void render_rect_notched(GuiDrawCommand* cmd) {
    Color color = CONVERT_COLOR(cmd->color, Color);
    int notch_size = conf.font_size / 4;

    DrawRectangle(cmd->pos_x, cmd->pos_y, cmd->width - notch_size, cmd->height, color);
    DrawRectangle(cmd->pos_x, cmd->pos_y + notch_size, cmd->width, cmd->height - notch_size, color);
    DrawTriangle(
        (Vector2) { cmd->pos_x + cmd->width - notch_size - 1, cmd->pos_y },
        (Vector2) { cmd->pos_x + cmd->width - notch_size - 1, cmd->pos_y + notch_size },
        (Vector2) { cmd->pos_x + cmd->width, cmd->pos_y + notch_size },
        color
    );
}

static void draw_text_slice(Font font, const char *text, float pos_x, float pos_y, unsigned int text_size, float font_size, Color color) {
    if (font.texture.id == 0) return;

    Vector2 pos = (Vector2) { pos_x, pos_y };
    int codepoint, index;
    float scale_factor = font_size / font.baseSize;

    for (unsigned int i = 0; i < text_size;) {
        int next = 0;
        codepoint = GetCodepointNext(&text[i], &next);
        index = search_glyph(codepoint);
        i += next;

        if (codepoint != ' ') DrawTextCodepoint(font, codepoint, pos, font_size, color);

        if (font.glyphs[index].advanceX != 0) {
            pos.x += font.glyphs[index].advanceX * scale_factor;
        } else {
            pos.y += font.recs[index].width * scale_factor + font.glyphs[index].offsetX;
        }
    }
}

static void scrap_gui_render(void) {
#ifdef DEBUG
    bool show_bounds = IsKeyDown(KEY_F4);
#endif
    GuiDrawCommand* command;
    GUI_GET_COMMANDS(gui, command) {
        Texture2D* image = command->data.image;

        switch (command->type) {
        case DRAWTYPE_UNKNOWN:
            assert(false && "Got unknown draw type");
            break;
        case DRAWTYPE_BORDER:
            switch (command->data.border.type) {
            case BORDER_NORMAL:
                DrawRectangleLinesEx(
                    (Rectangle) { command->pos_x, command->pos_y, command->width, command->height },
                    command->data.border.width,
                    CONVERT_COLOR(command->color, Color)
                );
                break;
            case BORDER_CONTROL:
                render_border_control(command);
                break;
            case BORDER_CONTROL_BODY:
                render_border_control_body(command);
                break;
            case BORDER_END:
                render_border_end(command);
                break;
            case BORDER_CONTROL_END:
                render_border_control_end(command);
                break;
            case BORDER_NOTCHED:
                render_border_notched(command);
                break;
            default:
                assert(false && "Unhandled draw border type");
                break;
            }
            break;
        case DRAWTYPE_RECT:
            switch (command->data.rect_type) {
            case RECT_NORMAL:
                DrawRectangle(command->pos_x, command->pos_y, command->width, command->height, CONVERT_COLOR(command->color, Color));
                break;
            case RECT_NOTCHED:
                render_rect_notched(command);
                break;
            case RECT_TERMINAL:
                term_resize(command->width, command->height);
                draw_term(command->pos_x, command->pos_y);
                break;
            default:
                assert(false && "Unhandled draw rect type");
                break;
            }
            break;
        case DRAWTYPE_TEXT:
            draw_text_slice(
                *(Font*)command->data.text.font,
                command->data.text.text,
                command->pos_x,
                command->pos_y,
                command->data.text.text_size,
                command->height,
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
        case DRAWTYPE_SHADER_BEGIN:
            BeginShaderMode(*(Shader*)command->data.shader);
            break;
        case DRAWTYPE_SHADER_END:
            EndShaderMode();
            break;
        default:
            assert(false && "Unimplemented command render");
            break;
        }
#ifdef DEBUG
        if (show_bounds) DrawRectangleLinesEx((Rectangle) { command->pos_x, command->pos_y, command->width, command->height }, 1.0, (Color) { 0xff, 0x00, 0xff, 0x40 });
#endif
    }
}

static void print_debug(int* num, char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vsnprintf(debug_buffer[(*num)++], DEBUG_BUFFER_LINE_SIZE, fmt, va);
    va_end(va);
}

static void write_debug_buffer(void) {
    int i = 0;
#ifdef DEBUG
    print_debug(&i, "Block: %p, Parent: %p, Parent Arg: %p", hover.editor.block, hover.editor.block ? hover.editor.block->parent : NULL, hover.editor.parent_argument);
    print_debug(&i, "Argument: %p", hover.editor.argument);
    print_debug(&i, "BlockChain: %p", hover.editor.blockchain);
    print_debug(&i, "Select block: %p, arg: %p, chain: %p", hover.editor.select_block, hover.editor.select_argument, hover.editor.select_blockchain);
    print_debug(&i, "Select block pos: (%.3f, %.3f)", hover.editor.select_block_pos.x, hover.editor.select_block_pos.y);
    print_debug(&i, "Select block bounds Pos: (%.3f, %.3f), Size: (%.3f, %.3f)", hover.panels.code_panel_bounds.x, hover.panels.code_panel_bounds.y, hover.panels.code_panel_bounds.width, hover.panels.code_panel_bounds.height);
    print_debug(&i, "Category: %p", hover.category);
    print_debug(&i, "Mouse: %p, Time: %.3f, Pos: (%d, %d), Click: (%d, %d)", mouse_blockchain.blocks, hover.time_at_last_pos, GetMouseX(), GetMouseY(), (int)hover.mouse_click_pos.x, (int)hover.mouse_click_pos.y);
    print_debug(&i, "Camera: (%.3f, %.3f), Click: (%.3f, %.3f)", camera_pos.x, camera_pos.y, camera_click_pos.x, camera_click_pos.y);
    print_debug(&i, "Dropdown scroll: %d", dropdown.scroll_amount);
    print_debug(&i, "Drag cancelled: %d", hover.drag_cancelled);
    print_debug(&i, "Min: (%.3f, %.3f), Max: (%.3f, %.3f)", block_code.min_pos.x, block_code.min_pos.y, block_code.max_pos.x, block_code.max_pos.y);
    print_debug(&i, "Palette scroll: %d", palette.scroll_amount);
    print_debug(&i, "Editor: %d, Editing: %p, Blockdef: %p, input: %zu", hover.editor.part, hover.editor.edit_blockdef, hover.editor.blockdef, hover.editor.blockdef_input);
    print_debug(&i, "Elements: %zu/%zu, Draw: %zu/%zu", gui->element_stack_len, ELEMENT_STACK_SIZE, gui->command_stack_len, COMMAND_STACK_SIZE);
    print_debug(&i, "Slider: %p, min: %d, max: %d", hover.hover_slider.value, hover.hover_slider.min, hover.hover_slider.max);
    print_debug(&i, "Input: %p, Select: %p, Pos: (%.3f, %.3f), ind: (%d, %d)", hover.input_info.input, hover.select_input, hover.input_info.rel_pos.x, hover.input_info.rel_pos.y, hover.select_input_cursor, hover.select_input_mark);
    print_debug(&i, "Exec chain: %p, ind: %zu", vm.exec_chain, vm.exec_ind);
    print_debug(&i, "UI time: %.3f", ui_time);
    print_debug(&i, "FPS: %d, Frame time: %.3f", GetFPS(), GetFrameTime());
    print_debug(&i, "Panel: %p, side: %d", hover.panels.panel, hover.panels.panel_side);
#else
    print_debug(&i, "Scrap v" SCRAP_VERSION);
    print_debug(&i, "FPS: %d, Frame time: %.3f", GetFPS(), GetFrameTime());
#endif
}

void scrap_gui_process_render(void) {
    ClearBackground(GetColor(0x202020ff));
    draw_dots();

    write_debug_buffer();
    scrap_gui_render();

    if (start_vm_timeout == 0) {
        term_restart();
        clear_compile_error();
#ifdef USE_INTERPRETER
        exec = exec_new(&vm.thread);
#else
        exec = exec_new(&vm.thread, start_vm_mode);
#endif
        exec.code = editor_code;
        if (!thread_start(exec.thread, &exec)) {
            actionbar_show(gettext("Start failed!"));
        } else {
            actionbar_show(gettext("Started successfully!"));
        }
    }
}
