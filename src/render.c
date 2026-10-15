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

typedef enum {
    BORDER_NORMAL = 0,
    BORDER_CONTROL,
    BORDER_CONTROL_BODY,
    BORDER_END,
    BORDER_CONTROL_END,
    BORDER_NOTCHED,
} BorderType;

typedef enum {
    RECT_NORMAL = 0,
    RECT_NOTCHED,
    RECT_TERMINAL, // Terminal rendering is handled specially as it needs to synchronize with its buffer
} RectType;

typedef enum {
    IMAGE_NORMAL = 0,
    IMAGE_STRETCHED,
} ImageType;

GuiColor color_picker_default_colors[18] = {
    /* Black  */ { 0x00, 0x00, 0x00, 0xff }, /* Red    */ { 0xff, 0x00, 0x00, 0xff },
    /* Green  */ { 0x00, 0xff, 0x00, 0xff }, /* Yellow */ { 0xff, 0xff, 0x00, 0xff },
    /* Blue   */ { 0x00, 0x00, 0xff, 0xff }, /* Purple */ { 0xff, 0x00, 0xff, 0xff },
    /* Cyan   */ { 0x00, 0xff, 0xff, 0xff }, /* White  */ { 0xff, 0xff, 0xff, 0xff },
    CATEGORY_CONTROL_COLOR, CATEGORY_TERMINAL_COLOR,
    CATEGORY_MATH_COLOR,    CATEGORY_LOGIC_COLOR,
    CATEGORY_STRING_COLOR,  CATEGORY_MISC_COLOR,
    CATEGORY_DATA_COLOR,    CATEGORY_LIST_COLOR,
    CATEGORY_META_COLOR,    CATEGORY_NOTHING_COLOR,
};


static void code_draw(void);
static GuiElement* blockchain_draw(BlockChain* chain, bool ghost, bool show_previews, bool editable_arguments);
static void argument_on_hover(GuiElement* el);
static void argument_on_render(GuiElement* el);
static void block_preview_draw(void);

bool rl_vec_equal(Color lhs, Color rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

void actionbar_show(const char* text) {
    scrap_log(LOG_INFO, "[ACTION] %s", text);
    strncpy(editor.actionbar.text, text, sizeof(editor.actionbar.text) - 1);
    editor.actionbar.show_time = 3.0;
}

const char* sgettext(const char* msgid) {
    const char* msgval = gettext(msgid);
    if (msgval == msgid) msgval = strrchr(msgid, '|') + 1;
    if (msgval == (void*)1) msgval = msgid;
    return msgval;
}

static void dots_draw(float size, float spacing) {
    size    *= editor.camera.real_linear_zoom;
    spacing *= editor.camera.real_linear_zoom;

    if (size < 1) return;

    int win_width = GetScreenWidth();
    int win_height = GetScreenHeight();

    float offset_x = (0.0 - editor.camera.real_position.x) * editor.camera.real_linear_zoom
                   + ui.hover.panels.code_panel_bounds.width / 2.0
                   + ui.hover.panels.code_panel_bounds.x;

    float offset_y = (0.0 - editor.camera.real_position.y) * editor.camera.real_linear_zoom
                   + ui.hover.panels.code_panel_bounds.height / 2.0
                   + ui.hover.panels.code_panel_bounds.y;

    for (float y = fmodf(offset_y, spacing); y < win_height; y += spacing) {
        for (float x = fmodf(offset_x, spacing); x < win_width; x += spacing) {
            DrawRectangle(x, y, size, size, (Color) { 0x40, 0x40, 0x40, 0xff });
        }
    }

    if (ui.shader_time == 1.0) return;
    if (!IsShaderValid(assets.line_shader)) return;

    BeginShaderMode(assets.line_shader);
        for (float y = fmodf(offset_y, spacing); y < win_height; y += spacing) {
            DrawRectangle(0, y, win_width, size, (Color) { 0x40, 0x40, 0x40, 0xff });
        }
        for (float x = fmodf(offset_x, spacing); x < win_width; x += spacing) {
            DrawRectangle(x, 0, size, win_height, (Color) { 0x40, 0x40, 0x40, 0xff });
        }
    EndShaderMode();
}

static void term_draw(int x, int y) {
    if (term.char_w == 0 || term.char_h == 0) return;
    if (!term.buffer) return;

    Rectangle final_pos = { x, y, term.size.x, term.size.y };
    DrawRectangleRec(final_pos, BLACK);

    if (IsShaderValid(assets.line_shader)) {
        BeginShaderMode(assets.line_shader);
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
                DrawTextEx(assets.fonts.font_mono, buffer_char.ch, pos, term.font_size, 0.0, CONVERT_COLOR(buffer_char.fg_color, Color));
            }
            pos.x += term.char_size.x;
        }
        pos.y += term.font_size;
    }

    Vector2 cursor_pos = (Vector2) {
        final_pos.x + (term.cursor_pos % term.char_w) * term.char_size.x,
        final_pos.y + (term.cursor_pos / term.char_w) * term.font_size,
    };
    if (ui.hover.panels.panel && ui.hover.panels.panel->type == PANEL_TERM) {
        DrawRectangle(cursor_pos.x, cursor_pos.y, BLOCK_OUTLINE_SIZE, term.font_size, CONVERT_COLOR(term.cursor_fg_color, Color));
    } else {
        DrawRectangleLinesEx((Rectangle) { cursor_pos.x, cursor_pos.y, term.char_size.x, term.font_size }, BLOCK_OUTLINE_SIZE, CONVERT_COLOR(term.cursor_fg_color, Color));
    }
}

void font_prerender_shadow(Font* font) {
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
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    ui.hover.editor.part = EDITOR_BLOCKDEF;
    ui.hover.editor.blockdef = el->custom_data;
}

static void blockdef_input_on_hover(GuiElement* el) {
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    ui.hover.editor.blockchain = ui.hover.editor.prev_blockchain;
    if (el->draw_type != DRAWTYPE_UNKNOWN) return;
    el->draw_type = DRAWTYPE_BORDER;
    el->color = (GuiColor) { 0xa0, 0xa0, 0xa0, 0xff };
    el->data.border_width = BLOCK_OUTLINE_SIZE;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
}

static void blockdef_editor_del_button_on_hover(GuiElement* el) {
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (ui.hover.button.handler) return;
    el->draw_type = DRAWTYPE_RECT;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0xff, 0xff, 0xff, 0x80 };
    ui.hover.editor.blockdef_input = (size_t)el->custom_data;
    ui.hover.button.handler = blockdef_editor_del_arg_button_handle_click;
}

static void blockdef_editor_button_on_hover(GuiElement* el) {
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (ui.hover.button.handler) return;
    el->draw_type = DRAWTYPE_RECT;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0xff, 0xff, 0xff, 0x80 };
    ui.hover.button.handler = el->custom_data;
}

static void blockdef_editor_color_on_hover(GuiElement* el) {
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (ui.hover.button.handler) return;

    el->draw_type = DRAWTYPE_BORDER;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0xa0, 0xa0, 0xa0, 0xff };
    el->data.border_width = BLOCK_OUTLINE_SIZE;
    ui.hover.button.handler = blockdef_editor_color_button_handle_click;
}

static void blockdef_editor_return_type_switcher_on_hover(GuiElement* el) {
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (ui.hover.button.handler) return;

    el->draw_type = DRAWTYPE_RECT;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0xff, 0xff, 0xff, 0x80 };
    ui.hover.button.handler = blockdef_editor_return_type_switcher_button_handle_click;
}

static void blockdef_editor_type_switcher_on_hover(GuiElement* el) {
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (ui.hover.button.handler) return;

    el->draw_type = DRAWTYPE_RECT;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0xff, 0xff, 0xff, 0x80 };
    ui.hover.button.handler = blockdef_editor_type_switcher_button_handle_click;
    ui.hover.editor.blockdef_input = (size_t)el->custom_data;
}

static void blockdef_editor_button_draw(Texture2D* texture, ButtonClickHandler handler) {
    gui_element_begin(gui);
        gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });
        gui_on_hover(gui, blockdef_editor_button_on_hover);
        gui_set_custom_data(gui, handler);

        gui_image(gui, texture, BLOCK_IMAGE_SIZE, GUI_WHITE);
    gui_element_end(gui);
}

void input_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    if (ui.hover.is_panel_edit_mode) return;

    ui.hover.input_info = *(InputHoverInfo*)gui_get_state(el);
    ui.hover.input_info.rel_pos = (Vector2) {
        gui->mouse_x - el->abs_x - ui.hover.input_info.rel_pos.x,
        gui->mouse_y - el->abs_y - ui.hover.input_info.rel_pos.y,
    };
}

static const int input_cursor_scrolloff = 5;

void input_selection_on_render(GuiElement* el) {
    if (!el->parent) return;
    if (!el->parent->scroll_value) return;

    int cursor_pos = ui.hover.select_input_cursor > ui.hover.select_input_mark ? el->x + el->w : el->x;

    int right_off_x = cursor_pos + input_cursor_scrolloff - el->parent->w;
    int left_off_x  = -cursor_pos + input_cursor_scrolloff;

    if (left_off_x > 0) {
        *el->parent->scroll_value += left_off_x;
        ui.render_surface_redraw_next = true;
    } else if (right_off_x > 0) {
        *el->parent->scroll_value -= right_off_x;
        ui.render_surface_redraw_next = true;
    }
}

void input_cursor_on_render(GuiElement* el) {
    if (!el->parent) return;
    if (!el->parent->scroll_value) return;

    int right_off_x = el->x + el->w + input_cursor_scrolloff - el->parent->w;
    int left_off_x  = -el->x + input_cursor_scrolloff;

    if (left_off_x > 0) {
        *el->parent->scroll_value += left_off_x;
        ui.render_surface_redraw_next = true;
    } else if (right_off_x > 0) {
        *el->parent->scroll_value -= right_off_x;
        ui.render_surface_redraw_next = true;
    }
}

void input_text_draw(Font* font, char** input, const char* hint, unsigned short font_size, GuiColor font_color) {
    if (ui.hover.select_input == input) {
        if (ui.hover.select_input_cursor == ui.hover.select_input_mark) ui.hover.select_input_mark = -1;

        if (ui.hover.select_input_mark == -1) {
            gui_text_slice(gui, font, *input, ui.hover.select_input_cursor, font_size, font_color);
            gui_element_begin(gui);
                gui_set_rect(gui, font_color);
                gui_set_min_size(gui, BLOCK_OUTLINE_SIZE, BLOCK_TEXT_SIZE);
                gui_on_render(gui, input_cursor_on_render);
            gui_element_end(gui);

            gui_text(gui, font, *input + ui.hover.select_input_cursor, font_size, font_color);
        } else {
            int select_start = MIN(ui.hover.select_input_cursor, ui.hover.select_input_mark),
                select_end   = MAX(ui.hover.select_input_cursor, ui.hover.select_input_mark);
            gui_text_slice(gui, font, *input, select_start, font_size, font_color);

            gui_element_begin(gui);
                gui_set_rect(gui, (GuiColor) TEXT_SELECTION_COLOR);
                gui_text_slice(gui, font, *input + select_start, select_end - select_start, font_size, font_color);
                gui_on_render(gui, input_selection_on_render);
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
}

static void argument_input_on_hover(GuiElement* el) {
    if (el->custom_data) {
        argument_on_hover(el);
        Argument* arg = el->custom_data;
        if (arg->type != ARGUMENT_VALUE || arg->data.value.type == DATA_TYPE_STRING || arg->data.value.type == DATA_TYPE_ANY || arg->data.value.type == DATA_TYPE_INTEGER || arg->data.value.type == DATA_TYPE_FLOAT) {
            input_on_hover(el);
        }

        if (ui.hover.button.handler) return;
        if (arg->type == ARGUMENT_VALUE) {
            Value* value = &arg->data.value;
            if (value->type == DATA_TYPE_INTEGER || value->type == DATA_TYPE_FLOAT) {
                ui.hover.button.handler = input_numeric_handle_click;
                ui.hover.button.data = arg;
            } else if (value->type == DATA_TYPE_BOOL) {
                ui.hover.button.handler = input_bool_handle_click;
                ui.hover.button.data = arg;
            } else if (value->type == DATA_TYPE_COLOR) {
                ui.hover.button.handler = input_color_handle_click;
                ui.hover.button.data = arg;
            }
        }
    } else {
        blockdef_input_on_hover(el);
        input_on_hover(el);
    }
}

static void blockdef_argument_input_draw(char** input, const char* hint, bool can_hover, bool editable, GuiColor font_color, GuiColor bg_color) {
    gui_element_begin(gui);
        gui_set_rect(gui, bg_color);

        GuiElement* el = gui_element_begin(gui);
            if (editable) {
                if (ui.hover.select_input == input) {
                    gui_set_border(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff }, BLOCK_OUTLINE_SIZE);
                    gui_on_render(gui, argument_on_render);
                }
                InputHoverInfo info = (InputHoverInfo) {
                    .input = input,
                    .rel_pos = (Vector2) { BLOCK_STRING_PADDING / 2, 0 },
                    .font = &assets.fonts.font_cond_shadow,
                    .font_size = BLOCK_TEXT_SIZE * el->scaling,
                };
                gui_set_state(gui, &info, sizeof(info));
                if (can_hover) gui_on_hover(gui, argument_input_on_hover);
            }

            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_CENTER, ALIGN_CENTER);
            gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
            gui_set_min_size(gui, config.ui_size - BLOCK_OUTLINE_SIZE * 4, config.ui_size - BLOCK_OUTLINE_SIZE * 4);

            input_text_draw(&assets.fonts.font_cond, input, hint, BLOCK_TEXT_SIZE, font_color);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void blockdef_draw(Blockdef* blockdef, bool editing, bool in_editor) {
    bool collision = ui.hover.editor.prev_blockdef == blockdef;
    Color color = CONVERT_COLOR(blockdef->color, Color);
    Color block_color = ColorBrightness(color, collision ? 0.3 : 0.0);
    Color dropdown_color = ColorBrightness(color, collision ? 0.0 : -0.3);
    Color outline_color = ColorBrightness(color, collision ? 0.5 : -0.2);

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
        gui_set_custom_data(gui, blockdef);
        gui_on_hover(gui, blockdef_on_hover);
        if (ui.hover.editor.edit_blockdef == blockdef) gui_on_render(gui, argument_on_render);
        if (blockdef->type == BLOCKTYPE_HAT) gui_set_draw_subtype(gui, RECT_NOTCHED);

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_border(gui, CONVERT_COLOR(outline_color, GuiColor), BLOCK_OUTLINE_SIZE);
        gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
        gui_set_min_size(gui, 0, config.ui_size);
        gui_set_padding(gui, BLOCK_OUTLINE_SIZE * 2, BLOCK_OUTLINE_SIZE * 2);
        gui_set_gap(gui, BLOCK_PADDING);
        if (blockdef->type == BLOCKTYPE_HAT) gui_set_draw_subtype(gui, BORDER_NOTCHED);

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        Input* input = &blockdef->inputs[i];

        if (ui.hover.editor.edit_blockdef == blockdef) {
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));
                gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
                gui_set_padding(gui, BLOCK_PADDING, BLOCK_PADDING);
                gui_set_gap(gui, BLOCK_PADDING);
        }

        static_assert(INPUT_LAST == 6, "Exhaustive input type in blockdef_draw");
        switch (input->type) {
        case INPUT_TEXT_DISPLAY:
            if (editing && in_editor) {
                blockdef_argument_input_draw(&input->data.text, "", true, true, GUI_BLACK, GUI_WHITE);
            } else {
                gui_text(gui, &assets.fonts.font_cond_shadow, input->data.text, BLOCK_TEXT_SIZE, GUI_WHITE);
            }
            break;
        case INPUT_IMAGE_DISPLAY:
            gui_image(gui, input->data.image.image_ptr, BLOCK_IMAGE_SIZE, CONVERT_COLOR(input->data.image.image_color, GuiColor));
            break;
        case INPUT_ARGUMENT:
            if (in_editor) {
                input->data.arg.blockdef->color = blockdef->color;
                blockdef_draw(input->data.arg.blockdef, editing, in_editor);

                if (ui.hover.editor.edit_blockdef == blockdef) {
                    gui_element_begin(gui);
                        gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });
                        gui_set_custom_data(gui, (void*)i);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);
                        gui_set_align(gui, ALIGN_CENTER, ALIGN_CENTER);
                        gui_on_hover(gui, blockdef_editor_type_switcher_on_hover);

                        if (ui.dropdown.ref_object == input) {
                            ui.dropdown.element = gui_get_element(gui);
                        }

                        gui_spacer(gui, BLOCK_STRING_PADDING / 2, 0);
                        gui_text(gui, &assets.fonts.font_cond_shadow, gettext(type_to_str(input->data.arg.allowed_type)), BLOCK_TEXT_SIZE, GUI_WHITE);
                        gui_image(gui, &assets.textures.dropdown, BLOCK_IMAGE_SIZE, GUI_WHITE);
                    gui_element_end(gui);
                }
            } else {
                gui_element_begin(gui);
                    gui_set_fixed(gui, config.ui_size - BLOCK_OUTLINE_SIZE * 4, config.ui_size - BLOCK_OUTLINE_SIZE * 4);
                    gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));
                gui_element_end(gui);
            }
            break;
        default:
            gui_element_begin(gui);
                gui_set_fixed(gui, config.ui_size - BLOCK_OUTLINE_SIZE * 4, config.ui_size - BLOCK_OUTLINE_SIZE * 4);
                gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));
            gui_element_end(gui);
            break;
        }

        if (ui.hover.editor.edit_blockdef == blockdef) {
                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });
                    gui_on_hover(gui, blockdef_editor_del_button_on_hover);
                    gui_set_custom_data(gui, (void*)i);

                    gui_image(gui, &assets.textures.button_del_arg, BLOCK_IMAGE_SIZE, GUI_WHITE);
                gui_element_end(gui);
            gui_element_end(gui);
        }
    }

    gui_element_end(gui);
    gui_element_end(gui);
}

static void block_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    ui.hover.editor.block = el->custom_data;
    ui.hover.editor.blockchain = ui.hover.editor.prev_blockchain;
    ui.hover.editor.block_end = false;
    if (ui.hover.editor.block->parent.type != BLOCK_PARENT_ARGUMENT) ui.hover.editor.parent_argument = NULL;
}

static void block_end_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    block_on_hover(el);
    ui.hover.editor.block_end = true;
}

static void argument_on_render(GuiElement* el) {
    ui.hover.editor.select_block_pos = (Vector2) {
        el->abs_x + editor.camera.real_position.x - editor.camera.position.x,
        el->abs_y + editor.camera.real_position.y - editor.camera.position.y,
    };
}

static void block_on_render(GuiElement* el) {
    ui.hover.editor.select_block_pos = (Vector2) {
        el->abs_x + editor.camera.real_position.x - editor.camera.position.x,
        el->abs_y + editor.camera.real_position.y - editor.camera.position.y,
    };
    ui.hover.editor.select_valid = true;
}

static void block_argument_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    if (ui.hover.is_panel_edit_mode) return;
    ui.hover.editor.parent_argument = el->custom_data;
    ui.hover.editor.blockchain = ui.hover.editor.prev_blockchain;
}

static void argument_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    ui.hover.editor.argument = el->custom_data;
    ui.hover.editor.blockchain = ui.hover.editor.prev_blockchain;

    if (el->draw_type != DRAWTYPE_UNKNOWN) return;
    el->draw_type = DRAWTYPE_BORDER;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0xa0, 0xa0, 0xa0, 0xff };
    el->data.border_width = BLOCK_OUTLINE_SIZE;
}

static void argument_type_switcher_on_hover(GuiElement* el) {
    el->color = (GuiColor) { 0x50, 0x50, 0x50, 0x40 };
    ui.hover.button.handler = input_type_switcher_handle_click;
    ui.hover.button.data = el->custom_data;
}

static const char* value_to_text(Value* value) {
    static_assert(DATA_TYPE_LAST == 12, "Exhaustive data type in value_to_text");
    switch (value->type) {
    case DATA_TYPE_NOTHING: return gettext("nothing");
    case DATA_TYPE_STRING:
    case DATA_TYPE_ANY:
        return value->data.str_val;
    case DATA_TYPE_INTEGER: return gui_arena_sprintf(ui.scratch_arena, 32, "%ld", value->data.integer_val);
    case DATA_TYPE_FLOAT: return gui_arena_sprintf(ui.scratch_arena, 32, "%.16g", value->data.float_val);
    case DATA_TYPE_BOOL: return value->data.bool_val ? "true" : "false";
    case DATA_TYPE_LIST: return gettext("list");
    case DATA_TYPE_BLOCKDEF:
    case DATA_TYPE_COLOR:
    case DATA_TYPE_CHUNK:
    case DATA_TYPE_NULL:
    case DATA_TYPE_UNKNOWN:
    default:
        return NULL;
    }
}

static void value_argument_draw(Argument* arg, bool can_hover, bool editable, bool ghost, GuiColor font_color, GuiColor bg_color) {
    if (arg->type != ARGUMENT_VALUE) return;
    Value* value = &arg->data.value;

    GuiElement* el = gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        if (editable) {
            if (ui.hover.editor.select_argument == arg) {
                gui_set_border(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff }, BLOCK_OUTLINE_SIZE);
                gui_on_render(gui, argument_on_render);
            }

            if (value->type == DATA_TYPE_STRING || value->type == DATA_TYPE_ANY || value->type == DATA_TYPE_INTEGER || value->type == DATA_TYPE_FLOAT) {
                InputHoverInfo info = (InputHoverInfo) {
                    .input = value->type == DATA_TYPE_STRING || value->type == DATA_TYPE_ANY ? &value->data.str_val : &ui.hover.editor.select_argument_scratch_input,
                    .rel_pos = (Vector2) { BLOCK_STRING_PADDING / 2, 0 },
                    .font = &assets.fonts.font_cond_shadow,
                    .font_size = BLOCK_TEXT_SIZE * el->scaling,
                };
                gui_set_state(gui, &info, sizeof(info));
            }
            gui_set_custom_data(gui, arg);
            if (can_hover) gui_on_hover(gui, argument_input_on_hover);
        }

        const int value_arg_size = config.ui_size - BLOCK_OUTLINE_SIZE * 4;

        if (value->type == DATA_TYPE_BOOL) {
            gui_element_begin(gui);
                gui_set_border(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x30 }, BLOCK_OUTLINE_SIZE);

                gui_element_begin(gui);
                    gui_set_min_size(gui, value_arg_size, value_arg_size);
                    gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                    gui_set_rect(gui, value->data.bool_val ? (GuiColor) { 0x30, 0xff, 0x30, ghost ? BLOCK_GHOST_OPACITY : 0xff } : (GuiColor) { 0xff, 0x30, 0x30, ghost ? BLOCK_GHOST_OPACITY : 0xff });
                    gui_set_align(gui, ALIGN_CENTER, ALIGN_CENTER);
                    gui_text(gui, &assets.fonts.font_cond_shadow, gettext(value->data.bool_val ? "True" : "False"), BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, ghost ? BLOCK_GHOST_OPACITY : 0xff });
                gui_element_end(gui);
            gui_element_end(gui);
        } else if (value->type == DATA_TYPE_COLOR) {
            gui_element_begin(gui);
                if (ui.dropdown.ref_object == &value->data.color_val) {
                    ui.dropdown.element = gui_get_element(gui);
                }

                gui_element_begin(gui);
                    gui_set_fixed(gui, BLOCK_IMAGE_SIZE, BLOCK_IMAGE_SIZE);

                    GuiColor color_val_color = CONVERT_COLOR(value->data.color_val, GuiColor);
                    if (ghost) color_val_color.a = BLOCK_GHOST_OPACITY;
                    gui_set_rect(gui, color_val_color);
                gui_element_end(gui);
            gui_element_end(gui);
        } else {
            gui_element_begin(gui);
                gui_set_rect(gui, bg_color);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_CENTER, ALIGN_CENTER);
                gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                gui_set_min_size(gui, config.ui_size - BLOCK_OUTLINE_SIZE * 4, config.ui_size - BLOCK_OUTLINE_SIZE * 4);

                if (value->type == DATA_TYPE_STRING || value->type == DATA_TYPE_ANY) {
                    input_text_draw(&assets.fonts.font_cond, &value->data.str_val, gettext(value->type == DATA_TYPE_STRING ? "Abc" : "any"), BLOCK_TEXT_SIZE, font_color);
                } else if (value->type == DATA_TYPE_INTEGER || value->type == DATA_TYPE_FLOAT) {
                    if (ui.hover.editor.select_argument == arg) {
                        input_text_draw(&assets.fonts.font_cond, &ui.hover.editor.select_argument_scratch_input, "", BLOCK_TEXT_SIZE, font_color);
                    } else {
                        gui_text(gui, &assets.fonts.font_cond, value_to_text(value), BLOCK_TEXT_SIZE, font_color);
                    }
                } else {
                    gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x50 });
                    gui_text(gui, &assets.fonts.font_cond, value_to_text(value), BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });
                }
            gui_element_end(gui);
        }

        if (ui.hover.editor.select_argument == arg) {
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_CENTER, ALIGN_CENTER);
                gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                gui_set_min_size(gui, config.ui_size - BLOCK_OUTLINE_SIZE * 4, config.ui_size - BLOCK_OUTLINE_SIZE * 4);
                gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x40 });
                gui_set_custom_data(gui, arg);
                gui_on_hover(gui, argument_type_switcher_on_hover);

                if (ui.dropdown.ref_object == arg) {
                    ui.dropdown.element = gui_get_element(gui);
                }

                gui_text(gui, &assets.fonts.font_cond_shadow, ": ", BLOCK_TEXT_SIZE, GUI_WHITE);
                gui_text(gui, &assets.fonts.font_cond_shadow, gettext(type_to_str(value->type)), BLOCK_TEXT_SIZE, GUI_WHITE);

                if (value->type == DATA_TYPE_ANY) {
                    gui_spacer(gui, BLOCK_STRING_PADDING/2, 0);
                    gui_text(gui, &assets.fonts.font_cond_shadow, "(", BLOCK_TEXT_SIZE, GUI_WHITE);
                    gui_text(gui, &assets.fonts.font_cond_shadow, gettext(type_to_str(ui.hover.editor.select_argument_type)), BLOCK_TEXT_SIZE, GUI_WHITE);
                    gui_text(gui, &assets.fonts.font_cond_shadow, ")", BLOCK_TEXT_SIZE, GUI_WHITE);
                }
                gui_image(gui, &assets.textures.dropdown, BLOCK_IMAGE_SIZE, GUI_WHITE);
            gui_element_end(gui);
        }
    gui_element_end(gui);
}

static void block_draw(Block* block, bool highlight, bool select, bool can_hover, bool ghost, bool editable, bool show_previews, bool darken) {
    bool collision = ui.hover.editor.prev_block == block || highlight;
    Color color = CONVERT_COLOR(block->blockdef->color, Color);
    if (!block->blockdef->func) color = (Color) UNIMPLEMENTED_BLOCK_COLOR;
    if (!thread_is_running(&vm.thread) && block == vm.compiler_error.block) {
        double animation = fmod(-GetTime(), 1.0) * 0.5 + 1.0;
        color = (Color) { 0xff * animation, 0x20 * animation, 0x20 * animation, 0xff };
    }
    if (ghost) color.a = BLOCK_GHOST_OPACITY;

    if (darken) color = ColorBrightness(color, -0.15);

    Color block_color = collision ? ColorBrightness(color, 0.3) : color;
    Color dropdown_color = collision ? color : ColorBrightness(color, -0.3);
    Color outline_color;
    select = ui.hover.editor.select_block == block || select;
    if (select) {
        outline_color = ColorBrightness(color, 0.7);
    } else {
        outline_color = ColorBrightness(color, collision ? 0.5 : -0.2);
    }

    GuiElement* block_element = gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
        gui_set_custom_data(gui, block);
        if (block->blockdef->type == BLOCKTYPE_HAT) gui_set_draw_subtype(gui, RECT_NOTCHED);
        if (can_hover) gui_on_hover(gui, block_on_hover);
        if (ui.hover.editor.select_block == block) gui_on_render(gui, block_on_render);

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_VERTICAL);
        gui_set_border(gui, CONVERT_COLOR(outline_color, GuiColor), BLOCK_OUTLINE_SIZE);
        gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
        gui_set_padding(gui, BLOCK_OUTLINE_SIZE * 2, BLOCK_OUTLINE_SIZE * 2);
        gui_set_min_size(gui, 0, config.ui_size);
        gui_set_gap(gui, BLOCK_PADDING);
        if (block->blockdef->type == BLOCKTYPE_CONTROL) {
            gui_set_draw_subtype(gui, BORDER_CONTROL);
        } else if (block->blockdef->type == BLOCKTYPE_CONTROLEND) {
            gui_set_draw_subtype(gui, BORDER_CONTROL_END);
        } else if (block->blockdef->type == BLOCKTYPE_HAT) {
            gui_set_draw_subtype(gui, BORDER_NOTCHED);
        }

    GuiElement* block_element_inner = gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_gap(gui, BLOCK_PADDING);
        gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
        gui_set_grow(gui, DIRECTION_VERTICAL);

    size_t arg_id = 0;
    Input* inputs = block->blockdef->inputs;
    size_t inputs_size = vector_size(inputs);

    Argument default_argument = {
        .input_id = 0,
        .data.value = {
            .type = DATA_TYPE_STRING,
            .data.str_val = "",
        },
    };

    for (size_t i = 0; i < inputs_size; i++) {
        Input* input = &inputs[i];
        Argument* arg = block->arguments ? &block->arguments[arg_id] : NULL;

        switch (input->type) {
        case INPUT_TEXT_DISPLAY:
            if (block_element_inner->w > 500) {
                gui_element_end(gui);
                block_element_inner = gui_element_begin(gui);
                    gui_set_direction(gui, DIRECTION_HORIZONTAL);
                    gui_set_gap(gui, BLOCK_PADDING);
                    gui_set_align(gui, ALIGN_RIGHT, ALIGN_CENTER);
                    gui_set_grow(gui, DIRECTION_HORIZONTAL);
            }

            gui_text(gui, &assets.fonts.font_cond_shadow, input->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, ghost ? BLOCK_GHOST_OPACITY : 0xff });
            break;
        case INPUT_IMAGE_DISPLAY: ;
            GuiColor img_color = CONVERT_COLOR(input->data.image.image_color, GuiColor);
            if (ghost) img_color.a = BLOCK_GHOST_OPACITY;
            gui_image(gui, input->data.image.image_ptr, BLOCK_IMAGE_SIZE, img_color);
            break;
        case INPUT_ARGUMENT:
            if (!arg) {
                arg = &default_argument;
                arg->type = ARGUMENT_VALUE;
            }

            switch (arg->type) {
            case ARGUMENT_BLOCK:
                gui_element_begin(gui);
                    if (can_hover) gui_on_hover(gui, block_argument_on_hover);
                    gui_set_custom_data(gui, arg);
                    if (arg->data.block->blockdef->type == BLOCKTYPE_CONTROL) gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));

                    if (arg->data.block->parent.as.arg != arg && editor.show_debug) {
                        gui_text(gui, &assets.fonts.font_cond_shadow, "Detached parent", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
                    }

                    block_draw(arg->data.block, highlight, false, can_hover, ghost, editable, show_previews, !memcmp(&arg->data.block->blockdef->color, &block->blockdef->color, sizeof(BlockdefColor)) ? !darken : false);
                gui_element_end(gui);
                break;
            case ARGUMENT_VALUE:
                value_argument_draw(
                    arg,
                    can_hover,
                    editable,
                    ghost,
                    (GuiColor) { 0x00, 0x00, 0x00, ghost ? BLOCK_GHOST_OPACITY : 0xff },
                    (GuiColor) { 0xff, 0xff, 0xff, ghost ? BLOCK_GHOST_OPACITY : BLOCK_ARG_OPACITY }
                );
                break;
            default:
                gui_text(gui, &assets.fonts.font_cond_shadow, "NODEF", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            if (!arg) {
                arg = &default_argument;
                arg->type = ARGUMENT_VALUE;
            }

            assert(arg->type == ARGUMENT_VALUE && arg->data.value.type == DATA_TYPE_STRING);
            gui_element_begin(gui);
                gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));

                if (ui.dropdown.ref_object == arg) {
                    ui.dropdown.element = gui_get_element(gui);
                }

                gui_element_begin(gui);
                    gui_set_min_size(gui, 0, config.ui_size - BLOCK_OUTLINE_SIZE * 4);
                    gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
                    gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                    gui_set_direction(gui, DIRECTION_HORIZONTAL);
                    if (editable) {
                        if (ui.hover.editor.select_argument == arg) {
                            gui_set_border(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff }, BLOCK_OUTLINE_SIZE);
                            gui_on_render(gui, argument_on_render);
                        }
                        if (can_hover) gui_on_hover(gui, argument_on_hover);
                        gui_set_custom_data(gui, arg);
                    }

                    gui_text(gui, &assets.fonts.font_cond_shadow, arg->data.value.data.str_val, BLOCK_TEXT_SIZE, GUI_WHITE);
                    gui_image(gui, &assets.textures.dropdown, BLOCK_IMAGE_SIZE, GUI_WHITE);

                gui_element_end(gui);
            gui_element_end(gui);
            arg_id++;
            break;
        case INPUT_BLOCKDEF_EDITOR:
            if (!arg) {
                arg_id++;
                break;
            }

            assert(arg->type == ARGUMENT_BLOCKDEF);
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));
                gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
                gui_set_gap(gui, BLOCK_PADDING);
                gui_set_custom_data(gui, arg);
                if (can_hover) gui_on_hover(gui, argument_on_hover);

                blockdef_draw(arg->data.blockdef, ui.hover.editor.edit_blockdef == arg->data.blockdef, true);

                if (editable) {
                    if (ui.hover.editor.edit_blockdef == arg->data.blockdef) {
                        blockdef_editor_button_draw(&assets.textures.button_add_arg, blockdef_editor_add_arg_button_handle_click);
                        blockdef_editor_button_draw(&assets.textures.button_add_text, blockdef_editor_add_text_button_handle_click);

                        gui_element_begin(gui);
                            if (can_hover) gui_on_hover(gui, blockdef_editor_color_on_hover);

                            if (ui.dropdown.ref_object == &arg->data.blockdef->color) {
                                ui.dropdown.element = gui_get_element(gui);
                                gui_set_border(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff }, BLOCK_OUTLINE_SIZE);
                                gui_on_render(gui, argument_on_render);
                            }

                            gui_element_begin(gui);
                                gui_set_fixed(gui, BLOCK_IMAGE_SIZE, BLOCK_IMAGE_SIZE);
                                gui_set_rect(gui, CONVERT_COLOR(arg->data.blockdef->color, GuiColor));
                            gui_element_end(gui);
                        gui_element_end(gui);

                        gui_element_begin(gui);
                            gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });
                            // gui_set_custom_data(gui, (void*)i);
                            gui_set_direction(gui, DIRECTION_HORIZONTAL);
                            gui_set_align(gui, ALIGN_CENTER, ALIGN_CENTER);
                            gui_on_hover(gui, blockdef_editor_return_type_switcher_on_hover);

                            if (ui.dropdown.ref_object == &arg->data.blockdef->return_type) {
                                ui.dropdown.element = gui_get_element(gui);
                            }

                            gui_spacer(gui, BLOCK_STRING_PADDING / 2, 0);
                            gui_text(gui, &assets.fonts.font_cond_shadow, gettext(type_to_str(arg->data.blockdef->return_type)), BLOCK_TEXT_SIZE, GUI_WHITE);
                            gui_image(gui, &assets.textures.dropdown, BLOCK_IMAGE_SIZE, GUI_WHITE);
                        gui_element_end(gui);

                        blockdef_editor_button_draw(&assets.textures.button_close, blockdef_editor_close_button_handle_click);
                    } else {
                        blockdef_editor_button_draw(&assets.textures.button_edit, blockdef_editor_edit_button_handle_click);
                    }
                    gui_spacer(gui, 0, 0);
                }

            gui_element_end(gui);
            arg_id++;
            break;
        default:
            gui_text(gui, &assets.fonts.font_cond_shadow, "NODEF", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
            break;
        }
    }

    gui_element_end(gui);
    gui_element_end(gui);
    gui_element_end(gui);

    if (block->blockdef->type == BLOCKTYPE_CONTROL || block->blockdef->type == BLOCKTYPE_CONTROLEND) {
        gui_element_begin(gui);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);

            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_min_size(gui, BLOCK_CONTROL_INDENT, config.ui_size / 2);
                gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
                gui_on_hover(gui, block_on_hover);
                gui_set_custom_data(gui, block);

                gui_element_begin(gui);
                    gui_set_grow(gui, DIRECTION_VERTICAL);
                    gui_set_grow(gui, DIRECTION_HORIZONTAL);
                    gui_set_border(gui, CONVERT_COLOR(outline_color, GuiColor), BLOCK_OUTLINE_SIZE);
                    gui_set_draw_subtype(gui, BORDER_CONTROL_BODY);
                gui_element_end(gui);
            gui_element_end(gui);

            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_VERTICAL);

                if (ui.hover.editor.prev_block == block && !ui.hover.editor.block_end && show_previews) {
                    block_preview_draw();
                }

                if (editor.show_debug && block->contents->parent != block) {
                    gui_text(gui, &assets.fonts.font_cond_shadow, "Detached parent", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
                }
                blockchain_draw(block->contents, ghost, show_previews, editable);
            gui_element_end(gui);
        gui_element_end(gui);

        if (block->blockdef->type == BLOCKTYPE_CONTROL) {
            if (editor.show_debug && block->controlend_contents->parent != block) {
                gui_text(gui, &assets.fonts.font_cond_shadow, "Detached parent", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
            }
            blockchain_draw(block->controlend_contents, ghost, show_previews, editable);
        }

        if (block->blockdef->type == BLOCKTYPE_CONTROL) {
            gui_element_begin(gui);
                gui_set_min_size(gui, block_element->w, config.ui_size);
                gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
                gui_on_hover(gui, block_end_on_hover);
                if (ui.hover.editor.select_block == block) gui_on_render(gui, block_on_render);
                gui_set_custom_data(gui, block);

                gui_element_begin(gui);
                    gui_set_grow(gui, DIRECTION_VERTICAL);
                    gui_set_grow(gui, DIRECTION_HORIZONTAL);
                    gui_set_border(gui, CONVERT_COLOR(outline_color, GuiColor), BLOCK_OUTLINE_SIZE);
                    gui_set_draw_subtype(gui, BORDER_END);
                gui_element_end(gui);
            gui_element_end(gui);
        }

        if (ui.hover.editor.prev_block == block && ui.hover.editor.block_end && show_previews && block->parent.type != BLOCK_PARENT_ARGUMENT) {
            block_preview_draw();
        }
    } else {
        if (ui.hover.editor.prev_block == block && show_previews && block->parent.type != BLOCK_PARENT_ARGUMENT) {
            block_preview_draw();
        }
    }
}

static void tab_button_add_on_hover(GuiElement* el) {
    if (gui_window_is_shown()) return;
    if (ui.hover.button.handler) return;
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    ui.hover.button.handler = tab_add_button_handle_click;
    ui.hover.button.data = el->custom_data;
}

static void tab_button_on_hover(GuiElement* el) {
    if (gui_window_is_shown()) return;
    if (ui.hover.button.handler) return;
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    ui.hover.button.handler = tab_button_handle_click;
    ui.hover.button.data = el->custom_data;
}

static void button_on_hover(GuiElement* el) {
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (ui.hover.button.handler) return;
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    ui.hover.button.handler = el->custom_data;
}

static void panel_editor_button_on_hover(GuiElement* el) {
    if (!ui.hover.is_panel_edit_mode) return;
    if (ui.hover.button.handler) return;

    Color color = ColorBrightness(CONVERT_COLOR(el->color, Color), -0.13);
    el->color = CONVERT_COLOR(color, GuiColor);
    ui.hover.button.handler = el->custom_data;
}

static void panel_editor_button_draw(const char* text, int size, GuiColor color, ButtonClickHandler handler) {
    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
        gui_set_min_size(gui, 0, size);
        gui_set_padding(gui, config.ui_size * 0.3, 0);
        gui_set_rect(gui, color);
        gui_on_hover(gui, panel_editor_button_on_hover);
        gui_set_custom_data(gui, handler);

        gui_text(gui, &assets.fonts.font_cond, text, BLOCK_TEXT_SIZE, GUI_BLACK);
    gui_element_end(gui);
}

static GuiElement* button_draw(const char* text, Texture2D* icon, int size, bool selected, GuiHandler on_hover, void* custom_data) {
    GuiElement* el;
    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
        gui_set_min_size(gui, 0, size);
        gui_set_padding(gui, config.ui_size * 0.3, 0);
        gui_set_gap(gui, ELEMENT_GAP/2);
        if (selected) gui_set_rect(gui, GUI_WHITE);
        gui_on_hover(gui, on_hover);
        gui_set_custom_data(gui, custom_data);
        el = gui_get_element(gui);

        if (icon) gui_image(gui, icon, BLOCK_IMAGE_SIZE, selected ? GUI_BLACK : GUI_WHITE);
        if (text) gui_text(gui, &assets.fonts.font_cond, text, BLOCK_TEXT_SIZE, selected ? GUI_BLACK : GUI_WHITE);
    gui_element_end(gui);
    return el;
}

static void top_bar_draw(void) {
    const int top_bar_size = config.ui_size * 1.2;
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
        gui_set_min_size(gui, 0, top_bar_size);
        gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);

        gui_spacer(gui, 5, 0);
        gui_image(gui, &assets.textures.icon_logo, config.ui_size, CONVERT_COLOR(WHITE, GuiColor));
        gui_spacer(gui, 10, 0);
        gui_text(gui, &assets.fonts.font_eb, gettext("Scrap"), config.ui_size * 0.8, CONVERT_COLOR(WHITE, GuiColor));
        gui_spacer(gui, 10, 0);

        GuiElement* el = button_draw(gettext("File"), &assets.textures.icon_file, top_bar_size, false, button_on_hover, file_button_handle_click);
        if (ui.dropdown.handler == file_dropdown_handle_click) ui.dropdown.element = el;
        button_draw(gettext("Settings"), &assets.textures.icon_settings, top_bar_size, false, button_on_hover, settings_button_handle_click);
        button_draw(gettext("About"), &assets.textures.icon_about, top_bar_size, false, button_on_hover, about_button_handle_click);
    gui_element_end(gui);
}

static void tab_bar_draw(void) {
    const int tab_bar_size = config.ui_size;
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
        gui_set_min_size(gui, 0, tab_bar_size);
        gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);

        if (ui.hover.is_panel_edit_mode && ui.hover.panels.mouse_panel != PANEL_NONE) {
            button_draw("+", NULL, tab_bar_size, false, tab_button_add_on_hover, (void*)0);
        }
        for (size_t i = 0; i < vector_size(editor.tabs); i++) {
            button_draw(gettext(editor.tabs[i].name), NULL, tab_bar_size, editor.current_tab == (int)i, tab_button_on_hover, (void*)i);
            if (ui.hover.is_panel_edit_mode && ui.hover.panels.mouse_panel != PANEL_NONE) {
                button_draw("+", NULL, tab_bar_size, false, tab_button_add_on_hover, (void*)(i + 1));
            }
        }

        gui_grow(gui, DIRECTION_HORIZONTAL);
        gui_text(gui, &assets.fonts.font_cond, editor.project_name, BLOCK_TEXT_SIZE, (GuiColor) { 0x80, 0x80, 0x80, 0xff });
        if (editor.project_modified) gui_text(gui, &assets.fonts.font_cond, "*", BLOCK_TEXT_SIZE, (GuiColor) { 0x80, 0x80, 0x80, 0xff });
        gui_grow(gui, DIRECTION_HORIZONTAL);

        // TODO: Build project
        // gui_element_begin(gui);
        //     gui_on_hover(gui, button_on_hover);
        //     gui_set_custom_data(gui, handle_build_button_click);

        //     gui_image(gui, &assets.textures.button_build, tab_bar_size, (GuiColor) { 0xff, 0x99, 0x00, 0xff });
        // gui_element_end(gui);

        // gui_spacer(gui, config.ui_size * 0.2, 0);

        gui_element_begin(gui);
            gui_on_hover(gui, button_on_hover);
            gui_set_custom_data(gui, stop_button_handle_click);

            if (vm.thread.state == THREAD_STATE_STOPPING) {
                gui_set_rect(gui, GUI_WHITE);
                gui_image(gui, &assets.textures.button_stop, tab_bar_size, GUI_BLACK);
            } else {
                gui_image(gui, &assets.textures.button_stop, tab_bar_size, (GuiColor) { 0xff, 0x40, 0x30, 0xff });
            }
        gui_element_end(gui);

        gui_spacer(gui, config.ui_size * 0.2, 0);

        gui_element_begin(gui);
            gui_on_hover(gui, button_on_hover);
            gui_set_custom_data(gui, run_button_handle_click);

            if (thread_is_running(&vm.thread)) {
                gui_set_rect(gui, GUI_WHITE);
                gui_image(gui, &assets.textures.button_run, tab_bar_size, GUI_BLACK);
            } else {
                gui_image(gui, &assets.textures.button_run, tab_bar_size, (GuiColor) { 0x60, 0xff, 0x00, 0xff });
            }
        gui_element_end(gui);
    gui_element_end(gui);
}

static void blockchain_on_hover(GuiElement* el) {
    if (ui.hover.is_panel_edit_mode) return;
    ui.hover.editor.prev_blockchain = el->custom_data;
}

static void block_preview_draw(void) {
    if (vector_size(editor.mouse_blockchains) == 0) return;
    if (ui.hover.editor.prev_argument != NULL) return;
    if (editor.mouse_blockchains[0].chain->start->blockdef->type == BLOCKTYPE_HAT) return;

    blockchain_draw(editor.mouse_blockchains[0].chain, true, false, false);
}

static GuiElement* blockchain_draw(BlockChain* chain, bool ghost, bool show_previews, bool editable_arguments) {
    GuiElement* el = gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_VERTICAL);
        gui_on_hover(gui, blockchain_on_hover);
        gui_set_custom_data(gui, chain);

        if (editor.show_debug) {
            BlockChain* ch = chain;

            if (CHAIN_EMPTY(ch) && !ch->parent) {
                gui_text(gui, &assets.fonts.font_cond_shadow, "Empty chain", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
            }

            if (!ch->start != !ch->end) {
                gui_text(gui, &assets.fonts.font_cond_shadow, !ch->start ? "missing start" : "missing end", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
            }

            if (ch->start) {
                if (ch->start->parent.as.chain != ch) {
                    gui_text(gui, &assets.fonts.font_cond_shadow, "start does not match", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
                }
            }

            if (ch->end) {
                if (ch->end->parent.as.chain != ch) {
                    gui_text(gui, &assets.fonts.font_cond_shadow, "end does not match", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0x00, 0x00, 0xff });
                }
            }
        }

        for (Block* iter = chain->start; iter; iter = iter->next) {
            if (!thread_is_running(&vm.thread) && iter == vm.compiler_error.block) vm.compiler_error.blockchain = chain;
            block_draw(iter, false, false, true, ghost, editable_arguments, show_previews, false);
        }
    gui_element_end(gui);
    return el;
}

static void block_category_on_hover(GuiElement* el) {
    if (ui.hover.is_panel_edit_mode) return;
    if (gui_window_is_shown()) return;
    if (ui.hover.button.handler) return;

    el->color.a = 0x80;
    ui.hover.button.handler = block_category_handle_click;
    ui.hover.category = el->custom_data;
}

static void block_category_draw(BlockCategory* category) {
    GuiColor color = CONVERT_COLOR(category->color, GuiColor);
    color.a = 0x40;

    gui_element_begin(gui);
        gui_set_scissor(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, color);
        gui_on_hover(gui, block_category_on_hover);
        gui_set_custom_data(gui, category);

        color.a = 0xff;
        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            if (category == editor.palette.current_category) gui_set_border(gui, color, BLOCK_OUTLINE_SIZE);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
             gui_set_padding(gui, category->image ? ELEMENT_GAP / 2 : ELEMENT_GAP, 0);
            gui_set_min_size(gui, 0, config.ui_size);
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
            gui_set_gap(gui, category->image ? ELEMENT_GAP / 2 : ELEMENT_GAP);

            if (category->image) {
                gui_image(gui, category->image, BLOCK_IMAGE_SIZE, color);
            } else {
                gui_element_begin(gui);
                    gui_set_min_size(gui, ELEMENT_GAP * 2, ELEMENT_GAP * 2);
                    gui_set_rect(gui, color);
                gui_element_end(gui);
            }

            gui_text(gui, &assets.fonts.font_cond_shadow, category->name, BLOCK_TEXT_SIZE, GUI_WHITE);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void block_categories_draw(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) PANEL_BACKGROUND_COLOR);
        gui_set_padding(gui, ELEMENT_GAP, ELEMENT_GAP);
        gui_set_gap(gui, ELEMENT_GAP);
        gui_set_scroll(gui, &ui.categories_scroll);
        gui_set_scissor(gui);

        BlockCategory* cat = editor.palette.categories_start;
        while (cat) {
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_gap(gui, ELEMENT_GAP);

                for (int i = 0; i < config.categories_column_count; i++) {
                    if (!cat) {
                        gui_grow(gui, DIRECTION_HORIZONTAL);
                        continue;
                    }
                    block_category_draw(cat);
                    cat = cat->next;
                }
            gui_element_end(gui);
        }
    gui_element_end(gui);
}

static void block_pallete_draw(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) PANEL_BACKGROUND_COLOR);
        gui_set_padding(gui, ELEMENT_GAP, ELEMENT_GAP);
        gui_set_gap(gui, ELEMENT_GAP);
        gui_set_scroll(gui, &editor.palette.scroll_amount);
        gui_set_scroll_scaling(gui, config.ui_size * 4);
        gui_set_scissor(gui);

        BlockCategory* cat = editor.palette.current_category;
        if (cat) {
            for (size_t i = 0; i < vector_size(cat->items); i++) {
                switch (cat->items[i].type) {
                case CATEGORY_ITEM_CHAIN:
                    blockchain_draw(cat->items[i].data.chain, false, false, false);
                    break;
                case CATEGORY_ITEM_LABEL:
                    if (i != 0) gui_spacer(gui, 0, config.ui_size * 0.1);
                    gui_element_begin(gui);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);
                        gui_set_gap(gui, BLOCK_OUTLINE_SIZE * 4);
                        gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
                        // gui_set_min_size(gui, 0, config.ui_size);

                        gui_element_begin(gui);
                            gui_set_fixed(gui, BLOCK_OUTLINE_SIZE * 2, config.ui_size * 0.75);
                            gui_set_rect(gui, CONVERT_COLOR(cat->items[i].data.label.color, GuiColor));
                        gui_element_end(gui);

                        gui_text(gui, &assets.fonts.font_cond_shadow, cat->items[i].data.label.text, BLOCK_TEXT_SIZE, GUI_WHITE);
                    gui_element_end(gui);
                    break;
                }
            }
        } else {
            gui_set_align(gui, ALIGN_CENTER, ALIGN_CENTER);
            gui_text(gui, &assets.fonts.font_cond_shadow, "No category currently selected", BLOCK_TEXT_SIZE, GUI_WHITE);
        }
    gui_element_end(gui);
}

static void spectrum_on_hover(GuiElement* el) {
    (void) el;
    ui.dropdown.as.color_picker.hover_part = COLOR_PICKER_SPECTRUM;
}

static void spectrum_on_render(GuiElement* el) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ui.dropdown.as.color_picker.select_part == COLOR_PICKER_SPECTRUM) {
        ui.dropdown.as.color_picker.color.hue = CLAMP((gui->mouse_y - el->parent->abs_y) / el->parent->h * 360.0, 0.0, 360.0);
        project_mark_modified();
    }
    el->y = (ui.dropdown.as.color_picker.color.hue / 360.0) * el->parent->h - el->h / 2.0;
}

static void color_picker_on_hover(GuiElement* el) {
    (void) el;
    ui.hover.button.handler = color_picker_handle_click;
}

static void color_picker_sv_on_hover(GuiElement* el) {
    (void) el;
    ui.dropdown.as.color_picker.hover_part = COLOR_PICKER_SV;
}

static void color_picker_default_colors_on_hover(GuiElement* el) {
    el->color = GUI_WHITE;
    ui.hover.button.handler = color_picker_default_color_handle_click;
    ui.hover.button.data = el->custom_data;
}

static void color_picker_sv_on_render(GuiElement* el) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ui.dropdown.as.color_picker.select_part == COLOR_PICKER_SV) {
        ui.dropdown.as.color_picker.color.saturation = CLAMP((gui->mouse_x - el->parent->abs_x) / el->parent->w, 0.0, 1.0);
        ui.dropdown.as.color_picker.color.value = CLAMP(1.0 - (gui->mouse_y - el->parent->abs_y) / el->parent->h, 0.0, 1.0);
        project_mark_modified();
    }

    el->x = ui.dropdown.as.color_picker.color.saturation * el->parent->w - el->w / 2.0;
    el->y = (1 - ui.dropdown.as.color_picker.color.value) * el->parent->h - el->h / 2.0;
}

static void color_picker_draw(void) {
    Color col = ColorFromHSV(
        ui.dropdown.as.color_picker.color.hue,
        ui.dropdown.as.color_picker.color.saturation,
        ui.dropdown.as.color_picker.color.value
    );

    Color col_hue = ColorFromHSV(
        ui.dropdown.as.color_picker.color.hue,
        1.0,
        1.0
    );

    *ui.dropdown.as.color_picker.edit_color = col;

    gui_element_begin(gui);
        gui_set_rect(gui, (GuiColor) { 0x20, 0x20, 0x20, 0xff });
        gui_on_hover(gui, color_picker_on_hover);

        gui_element_begin(gui);
            gui_set_border(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff }, 2);
            gui_set_gap(gui, config.ui_size * 0.25);
            gui_set_padding(gui, config.ui_size * 0.25, config.ui_size * 0.25);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);

            gui_element_begin(gui);
                gui_set_rect(gui, CONVERT_COLOR(col_hue, GuiColor));
                gui_set_fixed(gui, config.ui_size * 8.0, config.ui_size * 8.0);
                gui_set_shader(gui, &assets.gradient_shader);
                gui_on_hover(gui, color_picker_sv_on_hover);

                gui_element_begin(gui);
                    gui_set_border(gui, (GuiColor) { 0xff - col.r, 0xff - col.g, 0xff - col.b, 0xff }, 2);
                    gui_set_floating(gui);
                    gui_set_position(gui, -config.ui_size * 0.125, -config.ui_size * 0.125);
                    gui_set_fixed(gui, config.ui_size * 0.25, config.ui_size * 0.25);
                    gui_on_render(gui, color_picker_sv_on_render);
                gui_element_end(gui);
            gui_element_end(gui);

            gui_element_begin(gui);
                gui_set_image(gui, &assets.textures.spectrum, 0, GUI_WHITE);
                gui_set_min_size(gui, config.ui_size * 0.75, 0);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_draw_subtype(gui, IMAGE_STRETCHED);
                gui_on_hover(gui, spectrum_on_hover);

                gui_element_begin(gui);
                    gui_set_border(gui, (GuiColor) { 0xff - col_hue.r, 0xff - col_hue.g, 0xff - col_hue.b, 0xff }, 2);
                    gui_set_floating(gui);
                    gui_set_position(gui, -config.ui_size * 0.125, -config.ui_size * 0.125);
                    gui_set_fixed(gui, config.ui_size, config.ui_size * 0.25);
                    gui_on_render(gui, spectrum_on_render);
                gui_element_end(gui);
            gui_element_end(gui);

            gui_element_begin(gui);
                gui_set_gap(gui, config.ui_size * 0.25);

                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
                    gui_set_padding(gui, config.ui_size * 0.2, config.ui_size * 0.2);

                    snprintf(ui.dropdown.as.color_picker.color_hex, 10, "#%02x%02x%02x%02x", col.r, col.g, col.b, col.a);
                    gui_text(gui, &assets.fonts.font_mono, ui.dropdown.as.color_picker.color_hex, BLOCK_TEXT_SIZE, GUI_WHITE);
                gui_element_end(gui);

                gui_element_begin(gui);
                    gui_set_border(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff }, 2);
                    gui_element_begin(gui);
                        gui_set_fixed(gui, config.ui_size, config.ui_size);
                        gui_set_rect(gui, CONVERT_COLOR(col, GuiColor));
                    gui_element_end(gui);
                gui_element_end(gui);

                for (size_t i = 0; i < ARRLEN(color_picker_default_colors) / 4 + 1; i++) {
                    gui_element_begin(gui);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);
                        gui_set_gap(gui, config.ui_size * 0.25);

                        for (size_t j = 0; j < 4; j++) {
                            size_t ind = i*4 + j;
                            if (ind >= ARRLEN(color_picker_default_colors)) break;

                            gui_element_begin(gui);
                                gui_set_border(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff }, 2);
                                gui_on_hover(gui, color_picker_default_colors_on_hover);
                                gui_set_custom_data(gui, (void*)ind);

                                gui_element_begin(gui);
                                    gui_set_fixed(gui, config.ui_size * 0.8, config.ui_size * 0.8);
                                    gui_set_rect(gui, color_picker_default_colors[ind]);
                                gui_element_end(gui);
                            gui_element_end(gui);
                        }
                    gui_element_end(gui);
                }
            gui_element_end(gui);

        gui_element_end(gui);

    gui_element_end(gui);
}

static void code_editor_on_render(GuiElement* el) {
    ui.hover.panels.code_panel_bounds = (Rectangle) { el->abs_x, el->abs_y, el->w, el->h };
}

static void code_editor_draw(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_direction(gui, DIRECTION_VERTICAL);
        gui_set_padding(gui, 0, 0);
        gui_set_align(gui, ALIGN_RIGHT, ALIGN_TOP);
        gui_set_scissor(gui);
        gui_on_render(gui, code_editor_on_render);

        code_draw();

        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, 0, 0);
            gui_set_padding(gui, config.ui_size * 0.2, config.ui_size * 0.2);
            for (int i = 0; i < DEBUG_BUFFER_LINES; i++) {
                if (*editor.debug_buffer[i]) gui_text(gui, &assets.fonts.font_cond, editor.debug_buffer[i], config.ui_size * 0.5, (GuiColor) { 0xff, 0xff, 0xff, 0x60 });
            }

            gui_spacer(gui, 0, config.ui_size * 0.5);
        gui_element_end(gui);

        if (!thread_is_running(&vm.thread) && vector_size(vm.error_lines) > 0) {
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_gap(gui, config.ui_size * 0.5);
                gui_set_padding(gui, config.ui_size * 0.4, config.ui_size * 0.4);
                gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x80 });

                double animation = (fmod(-GetTime(), 1.0) * 0.5 + 1.0) * 255.0;
                gui_element_begin(gui);
                    gui_set_padding(gui, 0, config.ui_size * 0.2);
                    gui_image(gui, &assets.textures.icon_error, config.ui_size, (GuiColor) { 0xff, 0x20, 0x20, animation });
                gui_element_end(gui);

                gui_element_begin(gui);
                    gui_set_direction(gui, DIRECTION_VERTICAL);

                    gui_text(gui, &assets.fonts.font_cond, gettext("Got compiler error!"), config.ui_size * 0.6, (GuiColor) { 0xff, 0x33, 0x33, 0xff });
                    for (size_t i = 0; i < vector_size(vm.error_lines); i++) {
                        gui_text(gui, &assets.fonts.font_cond, vm.error_lines[i], config.ui_size * 0.6, GUI_WHITE);
                    }

                    gui_spacer(gui, 0, config.ui_size * 0.5);

                    gui_element_begin(gui);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);
                        gui_set_gap(gui, config.ui_size * 0.5);

                        if (vm.compiler_error.block) {
                            gui_element_begin(gui);
                                gui_set_border(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff }, BLOCK_OUTLINE_SIZE);
                                button_draw(gettext("Jump to block"), NULL, config.ui_size, false, button_on_hover, error_jump_to_block_button_handle_click);
                            gui_element_end(gui);
                        }

                        gui_element_begin(gui);
                            gui_set_border(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff }, BLOCK_OUTLINE_SIZE);
                            button_draw(gettext("Close"), NULL, config.ui_size, false, button_on_hover, error_close_button_handle_click);
                        gui_element_end(gui);
                    gui_element_end(gui);
                gui_element_end(gui);
            gui_element_end(gui);
        } else {
            gui_spacer(gui, 0, config.ui_size * 1.5);
        }

        if (editor.actionbar.show_time > 0) {
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_direction(gui, DIRECTION_VERTICAL);
                gui_set_align(gui, ALIGN_CENTER, ALIGN_TOP);

                Color color = YELLOW;
                color.a = editor.actionbar.show_time / 3.0 * 255.0;
                gui_text(gui, &assets.fonts.font_eb, editor.actionbar.text, config.ui_size * 0.8, CONVERT_COLOR(color, GuiColor));
            gui_element_end(gui);
        }
    gui_element_end(gui);
}

static void panel_editor_split_preview_draw(PanelTree* panel) {
    if (!ui.hover.is_panel_edit_mode) return;
    if (ui.hover.panels.prev_panel != panel) return;

    if (ui.hover.panels.mouse_panel == PANEL_NONE) {
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

    if (ui.hover.panels.panel_side == SPLIT_SIDE_NONE) return;

    gui_element_begin(gui);
        gui_set_floating(gui);
        gui_set_position(gui, 0, 0);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);

        if (ui.hover.panels.panel_side == SPLIT_SIDE_LEFT || ui.hover.panels.panel_side == SPLIT_SIDE_RIGHT) gui_set_direction(gui, DIRECTION_HORIZONTAL);

        if (ui.hover.panels.panel_side == SPLIT_SIDE_BOTTOM) gui_grow(gui, DIRECTION_VERTICAL);
        if (ui.hover.panels.panel_side == SPLIT_SIDE_RIGHT) gui_grow(gui, DIRECTION_HORIZONTAL);

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

        if (ui.hover.panels.panel_side == SPLIT_SIDE_TOP) gui_grow(gui, DIRECTION_VERTICAL);
        if (ui.hover.panels.panel_side == SPLIT_SIDE_LEFT) gui_grow(gui, DIRECTION_HORIZONTAL);
    gui_element_end(gui);
}

static void terminal_draw(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_padding(gui, ELEMENT_GAP, ELEMENT_GAP);
        gui_set_rect(gui, (GuiColor) PANEL_BACKGROUND_COLOR);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_rect(gui, GUI_WHITE);
            gui_set_draw_subtype(gui, RECT_TERMINAL);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void panel_on_hover(GuiElement* el) {
    ui.hover.panels.panel = el->custom_data;
    ui.hover.panels.panel_size = (Rectangle) { el->abs_x, el->abs_y, el->w, el->h };

    if (ui.hover.panels.panel->type == PANEL_SPLIT) return;

    int mouse_x = gui->mouse_x - el->abs_x;
    int mouse_y = gui->mouse_y - el->abs_y;

    bool is_top_right = mouse_y < ((float)el->h / (float)el->w) * mouse_x;
    bool is_top_left = mouse_y < -((float)el->h / (float)el->w * mouse_x) + el->h;

    if (is_top_right) {
        if (is_top_left) {
            ui.hover.panels.panel_side = SPLIT_SIDE_TOP;
        } else {
            ui.hover.panels.panel_side = SPLIT_SIDE_RIGHT;
        }
    } else {
        if (is_top_left) {
            ui.hover.panels.panel_side = SPLIT_SIDE_LEFT;
        } else {
            ui.hover.panels.panel_side = SPLIT_SIDE_BOTTOM;
        }
    }
}

static void panel_draw(PanelTree* panel) {
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
        block_pallete_draw();
        break;
    case PANEL_CODE:
        code_editor_draw();
        break;
    case PANEL_TERM:
        terminal_draw();
        break;
    case PANEL_BLOCK_CATEGORIES:
        block_categories_draw();
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

                panel_draw(panel->left);
            gui_element_end(gui);

            if (ui.hover.is_panel_edit_mode) {
                gui_element_begin(gui);
                    if (panel->direction == DIRECTION_HORIZONTAL) {
                        gui_set_grow(gui, DIRECTION_VERTICAL);
                    } else {
                        gui_set_grow(gui, DIRECTION_HORIZONTAL);
                    }
                    gui_set_min_size(gui, 10, 10);
                    gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, ui.hover.panels.drag_panel == panel ? 0x20 : ui.hover.panels.prev_panel == panel ? 0x80 : 0x40 });
                gui_element_end(gui);
            }

            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);

                if (panel->right->type != PANEL_SPLIT) {
                    gui_on_hover(gui, panel_on_hover);
                    gui_set_custom_data(gui, panel->right);
                }

                panel_draw(panel->right);
            gui_element_end(gui);
        gui_element_end(gui);
        break;
    }
    if (panel->type != PANEL_SPLIT) panel_editor_split_preview_draw(panel);
    if (panel->type != PANEL_SPLIT && !panel->parent) gui_element_end(gui);
}

static void code_draw(void) {
    for (size_t i = 0; i < vector_size(editor.code); i++) {
        Vector2 chain_pos = (Vector2) {
            (editor.code[i].x * config.ui_size / 32.0 - editor.camera.real_position.x) * editor.camera.real_linear_zoom + ui.hover.panels.code_panel_bounds.width / 2,
            (editor.code[i].y * config.ui_size / 32.0 - editor.camera.real_position.y) * editor.camera.real_linear_zoom + ui.hover.panels.code_panel_bounds.height / 2,
        };
        Rectangle code_size = ui.hover.panels.code_panel_bounds;
        if (editor.code[i].chain != ui.hover.editor.select_blockchain) {
            if (chain_pos.x > code_size.width || chain_pos.y > code_size.height) continue;

            if (editor.code[i].width > 0 && editor.code[i].height > 0 &&
                (chain_pos.x + editor.code[i].width  * editor.camera.real_linear_zoom < 0 ||
                 chain_pos.y + editor.code[i].height * editor.camera.real_linear_zoom < 0))
            {
                continue;
            }
        }
        GuiElement* el = gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, chain_pos.x, chain_pos.y);
            gui_scale_element(gui, editor.camera.real_linear_zoom);

            blockchain_draw(editor.code[i].chain, false, config.show_blockchain_previews, true);
        gui_element_end(gui);
        editor.code[i].width = el->w;
        editor.code[i].height = el->h;
    }
}

static void list_dropdown_on_hover(GuiElement* el) {
    assert(ui.dropdown.type == DROPDOWN_LIST);

    el->draw_type = DRAWTYPE_RECT;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    // Double cast to avoid warning. In our case this operation is safe because el->custom_data currently stores a value of type int
    ui.dropdown.as.list.select_ind = (int)(size_t)el->custom_data;

    ui.hover.button.handler = ui.dropdown.handler;
}

static void list_dropdown_draw(void) {
    const int max_list_size = 10;

    gui_element_begin(gui);
        gui_set_rect(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff });
        gui_set_gap(gui, 2);
        gui_set_padding(gui, 2, 2);
        if (ui.dropdown.as.list.len > max_list_size) {
            gui_set_scissor(gui);
            gui_set_fixed(gui, ui.dropdown.element->w * ui.dropdown.element->scaling + 5, max_list_size * (config.ui_size + 2) + 4);
            gui_set_scroll(gui, &ui.dropdown.as.list.scroll);
            gui_set_scroll_scaling(gui, (config.ui_size + 2) * 2);
        } else {
            gui_set_min_size(gui, ui.dropdown.element->w * ui.dropdown.element->scaling, 0);
        }

        for (int i = 0; i < ui.dropdown.as.list.len; i++) {
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
                gui_set_min_size(gui, 0, config.ui_size);
                gui_set_padding(gui, config.ui_size * 0.3, 0);
                gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
                gui_on_hover(gui, list_dropdown_on_hover);
                gui_set_custom_data(gui, (void*)(size_t)i);

                const char* list_value = sgettext(ui.dropdown.as.list.data[i]);
                gui_text(gui, &assets.fonts.font_cond, list_value, BLOCK_TEXT_SIZE, GUI_WHITE);
            gui_element_end(gui);
        }
    gui_element_end(gui);
}

static void dropdown_on_render(GuiElement* el) {
    int off_x = MAX(el->abs_x + el->w - gui->win_w, 0);
    int off_y = MAX(el->abs_y + el->h - gui->win_h, 0);
    el->x -= off_x;
    el->abs_x -= off_x;

    el->y -= off_y;
    el->abs_y -= off_y;
}

static void dropdown_draw(void) {
    if (!ui.dropdown.shown) return;
    ui.hover.button.handler = dropdown_handle_close;

    if (!ui.dropdown.element) {
        scrap_log(LOG_WARNING, "[DROPDOWN] Anchor is not set or gone");
        dropdown_handle_close();
        return;
    }

    gui_element_begin(gui);
        gui_set_floating(gui);
        gui_set_parent_anchor(gui, ui.dropdown.element);
        gui_set_position(gui, 0, ui.dropdown.element->h * ui.dropdown.element->scaling);
        gui_on_render(gui, dropdown_on_render);

        switch (ui.dropdown.type) {
        case DROPDOWN_COLOR_PICKER:
            color_picker_draw();
            break;
        case DROPDOWN_LIST:
            list_dropdown_draw();
            break;
        default:
            assert(false && "Unhandled dropdown type");
            break;
        }
    gui_element_end(gui);
}

static void blockdef_searcher_on_hover(GuiElement* el) {
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    ui.hover.editor.blockdef = el->custom_data;
}

static void blockdef_searcher_draw(void) {
    gui_element_begin(gui);
        gui_set_floating(gui);
        gui_set_position(gui, editor.search_list_pos.x, editor.search_list_pos.y);
        gui_set_rect(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff });
        gui_set_gap(gui, BLOCK_OUTLINE_SIZE);
        gui_set_padding(gui, BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE);

        gui_element_begin(gui);
            gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_padding(gui, BLOCK_OUTLINE_SIZE, 0);
            gui_set_min_size(gui, 0, config.ui_size);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);

            input_text_draw(&assets.fonts.font_cond, &editor.search_list_search, "Search...", BLOCK_TEXT_SIZE, GUI_WHITE);
        gui_element_end(gui);

        gui_element_begin(gui);
            gui_set_fixed(gui, 0, config.ui_size * 5);
            gui_set_fit(gui, DIRECTION_HORIZONTAL);
            gui_set_gap(gui, BLOCK_OUTLINE_SIZE);
            gui_set_scissor(gui);
            gui_set_scroll(gui, &ui.search_list_scroll);

            for (size_t i = 0; i < vector_size(editor.search_list); i++) {
                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
                    gui_set_grow(gui, DIRECTION_HORIZONTAL);
                    gui_on_hover(gui, blockdef_searcher_on_hover);
                    gui_set_custom_data(gui, editor.search_list[i]);

                    blockdef_draw(editor.search_list[i], false, false);
                gui_element_end(gui);
            }

        gui_element_end(gui);
    gui_element_end(gui);
}

static void panel_editor_on_hover(GuiElement* el) {
    (void) el;
    if (!ui.hover.is_panel_edit_mode) return;
    ui.hover.panels.panel = NULL;
}

void scrap_gui_process(void) {
    gui_begin(gui);
        top_bar_draw();
        tab_bar_draw();
        GuiElement* tab_bar_anchor = NULL;

        if (ui.hover.is_panel_edit_mode) {
            gui_element_begin(gui);
                tab_bar_anchor = gui_get_element(gui);
            gui_element_end(gui);
        }

        panel_draw(editor.tabs[editor.current_tab].root_panel);
        window_draw();

        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, gui->mouse_x, gui->mouse_y);
            gui_set_gap(gui, config.ui_size);
            gui_scale_element(gui, editor.camera.real_linear_zoom);

            int x_i = 0,
                x_i_max = ceil(sqrt(vector_size(editor.mouse_blockchains))),
                y_max = 0;

            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_gap(gui, config.ui_size);

                for (size_t i = 0; i < vector_size(editor.mouse_blockchains); i++) {
                    GuiElement* el = blockchain_draw(editor.mouse_blockchains[i].chain, false, false, true);
                    editor.mouse_blockchains[i].width  = el->w;
                    editor.mouse_blockchains[i].height = el->h;

                    x_i++;
                    y_max = MAX(y_max, editor.mouse_blockchains[i].height);

                    if (x_i >= x_i_max) {
                        gui_element_end(gui);
                        gui_element_begin(gui);
                            gui_set_direction(gui, DIRECTION_HORIZONTAL);
                            gui_set_gap(gui, config.ui_size);
                        y_max = 0;
                        x_i = 0;
                    }
                }
            gui_element_end(gui);
        gui_element_end(gui);

        if (ui.hover.select_input == &editor.search_list_search) {
            blockdef_searcher_draw();
        } else {
            editor.search_list_pos = (Vector2) { gui->mouse_x, gui->mouse_y };
        }

        if (ui.hover.is_panel_edit_mode) {
            if (ui.hover.panels.mouse_panel != PANEL_NONE) {
                gui_element_begin(gui);
                    gui_set_floating(gui);
                    gui_set_fixed(gui, gui->win_w * 0.3, gui->win_h * 0.3);
                    gui_set_position(gui, gui->mouse_x, gui->mouse_y);

                    PanelTree panel = (PanelTree) {
                        .type = ui.hover.panels.mouse_panel,
                        .parent = NULL,
                        .left = NULL,
                        .right = NULL,
                    };
                    panel_draw(&panel);
                gui_element_end(gui);
            }

            gui_element_begin(gui);
                gui_set_floating(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_position(gui, 0, 0);
                gui_set_parent_anchor(gui, tab_bar_anchor);
                gui_set_align(gui, ALIGN_CENTER, ALIGN_TOP);
                gui_set_padding(gui, 0, config.ui_size);

                gui_element_begin(gui);
                    gui_set_padding(gui, config.ui_size * 0.3, config.ui_size * 0.3);
                    gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x80 });
                    gui_set_align(gui, ALIGN_CENTER, ALIGN_TOP);
                    gui_on_hover(gui, panel_editor_on_hover);

                    gui_text(gui, &assets.fonts.font_eb, gettext("Panel edit mode"), config.ui_size * 0.8, GUI_WHITE);

                    gui_spacer(gui, 0, config.ui_size * 0.25);

                    gui_text(gui, &assets.fonts.font_cond_shadow, gettext("Click on panels to reposition them"), BLOCK_TEXT_SIZE, GUI_WHITE);
                    gui_text(gui, &assets.fonts.font_cond_shadow, gettext("Drag panel edges to resize them"), BLOCK_TEXT_SIZE, GUI_WHITE);

                    gui_spacer(gui, 0, config.ui_size * 0.25);

                    gui_element_begin(gui);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);
                        gui_set_gap(gui, config.ui_size * 0.25);

                        panel_editor_button_draw(gettext("Save"), config.ui_size, (GuiColor) { 0x40, 0xff, 0x40, 0xff }, panel_editor_save_button_handle_click);
                        panel_editor_button_draw(gettext("Done"), config.ui_size, (GuiColor) { 0x80, 0x80, 0x80, 0xff }, panel_editor_cancel_button_handle_click);
                    gui_element_end(gui);
                gui_element_end(gui);
            gui_element_end(gui);
        }

        dropdown_draw();
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
        scrap_log(LOG_WARNING, "[SVG] Could not load \"%s\"", file_name);
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

// Draw order for border_control_render()
//
//           1
//   +---------------+
// 4 |               | 2
//   +     +---------+
//             3
//
static void border_control_render(GuiDrawCommand cmd) {
    unsigned short border_w = cmd.data.border_width;
    Color color = CONVERT_COLOR(cmd.color, Color);

    /* 1 */ DrawRectangle(cmd.pos_x, cmd.pos_y, cmd.width, border_w, color);
    /* 2 */ DrawRectangle(cmd.pos_x + cmd.width - border_w, cmd.pos_y, border_w, cmd.height, color);
    /* 3 */ DrawRectangle(cmd.pos_x + BLOCK_CONTROL_INDENT * cmd.scaling - border_w, cmd.pos_y + cmd.height - border_w, cmd.width - BLOCK_CONTROL_INDENT * cmd.scaling, border_w, color);
    /* 4 */ DrawRectangle(cmd.pos_x, cmd.pos_y, border_w, cmd.height, color);
}

// Draw order for border_control_body_render()
//
//   +     +
// 1 |     | 2
//   +     +
//
static void border_control_body_render(GuiDrawCommand cmd) {
    unsigned short border_w = cmd.data.border_width;
    Color color = CONVERT_COLOR(cmd.color, Color);

    /* 1 */ DrawRectangle(cmd.pos_x, cmd.pos_y, border_w, cmd.height, color);
    /* 2 */ DrawRectangle(cmd.pos_x + cmd.width - border_w, cmd.pos_y, border_w, cmd.height, color);
}

// Draw order for border_control_end_render()
//
//              1
//   +     +---------+
// 4 |               | 2
//   +     +---------+
//              3
//
static void border_control_end_render(GuiDrawCommand cmd) {
    unsigned short border_w = cmd.data.border_width;
    Color color = CONVERT_COLOR(cmd.color, Color);

    /* 1 */ DrawRectangle(cmd.pos_x + BLOCK_CONTROL_INDENT * cmd.scaling - border_w, cmd.pos_y, cmd.width - BLOCK_CONTROL_INDENT * cmd.scaling, border_w, color);
    /* 2 */ DrawRectangle(cmd.pos_x + cmd.width - border_w, cmd.pos_y, border_w, cmd.height, color);
    /* 3 */ DrawRectangle(cmd.pos_x + BLOCK_CONTROL_INDENT * cmd.scaling - border_w, cmd.pos_y + cmd.height - border_w, cmd.width - BLOCK_CONTROL_INDENT * cmd.scaling, border_w, color);
    /* 4 */ DrawRectangle(cmd.pos_x, cmd.pos_y, border_w, cmd.height, color);
}

// Draw order for border_end_render()
//
//              1
//   +     +---------+
// 4 |               | 2
//   +---------------+
//           3
static void border_end_render(GuiDrawCommand cmd) {
    unsigned short border_w = cmd.data.border_width;
    Color color = CONVERT_COLOR(cmd.color, Color);

    /* 1 */ DrawRectangle(cmd.pos_x + BLOCK_CONTROL_INDENT * cmd.scaling - border_w, cmd.pos_y, cmd.width - BLOCK_CONTROL_INDENT * cmd.scaling, border_w, color);
    /* 2 */ DrawRectangle(cmd.pos_x + cmd.width - border_w, cmd.pos_y, border_w, cmd.height, color);
    /* 3 */ DrawRectangle(cmd.pos_x, cmd.pos_y + cmd.height - border_w, cmd.width, border_w, color);
    /* 4 */ DrawRectangle(cmd.pos_x, cmd.pos_y, border_w, cmd.height, color);
}

// Draw order for border_notched_render() and rect_notched_render()
//
//           1
//   +--------------+ 2
//   |               +
// 5 |               | 3
//   +---------------+
//           4
static void border_notched_render(GuiDrawCommand cmd) {
    unsigned short border_w = cmd.data.border_width;
    Color color = CONVERT_COLOR(cmd.color, Color);
    int notch_size = config.ui_size / 4 * cmd.scaling;

    /* 1 */ DrawRectangle(cmd.pos_x, cmd.pos_y, cmd.width - notch_size, border_w, color);
    /* 2 */ DrawRectanglePro((Rectangle) {
        cmd.pos_x + cmd.width - notch_size,
        cmd.pos_y,
        sqrtf((notch_size * notch_size) * 2),
        border_w,
    }, (Vector2) {0}, 45.0, color);
    /* 3 */ DrawRectangle(cmd.pos_x + cmd.width - border_w, cmd.pos_y + notch_size, border_w, cmd.height - notch_size, color);
    /* 4 */ DrawRectangle(cmd.pos_x, cmd.pos_y + cmd.height - border_w, cmd.width, border_w, color);
    /* 5 */ DrawRectangle(cmd.pos_x, cmd.pos_y, border_w, cmd.height, color);
}

static void rect_notched_render(GuiDrawCommand cmd) {
    Color color = CONVERT_COLOR(cmd.color, Color);
    int notch_size = config.ui_size / 4 * cmd.scaling;

    DrawRectangle(cmd.pos_x, cmd.pos_y, cmd.width - notch_size, cmd.height, color);
    DrawRectangle(cmd.pos_x, cmd.pos_y + notch_size, cmd.width, cmd.height - notch_size, color);
    DrawTriangle(
        (Vector2) { cmd.pos_x + cmd.width - notch_size - 1, cmd.pos_y },
        (Vector2) { cmd.pos_x + cmd.width - notch_size - 1, cmd.pos_y + notch_size },
        (Vector2) { cmd.pos_x + cmd.width, cmd.pos_y + notch_size },
        color
    );
}

static void text_slice_draw(Font font, const char *text, float pos_x, float pos_y, unsigned int text_size, float font_size, Color color) {
    if (font.texture.id == 0) return;

    Vector2 pos = (Vector2) { pos_x, pos_y };
    int codepoint, index;
    float scale_factor = font_size / font.baseSize;

    for (unsigned int i = 0; i < text_size;) {
        if (!text[i]) break;
        int next = 0;
        codepoint = GetCodepointNext(&text[i], &next);
        index = search_glyph(font, codepoint);
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
    GuiDrawCommand command;
    GUI_GET_COMMANDS(gui, command) {
        Texture2D* image = command.data.image;

        switch (command.type) {
        case DRAWTYPE_UNKNOWN:
            assert(false && "Got unknown draw type");
            break;
        case DRAWTYPE_BORDER:
            command.width = ceilf(command.width);
            command.height = ceilf(command.height);
            switch (command.subtype) {
            case BORDER_NORMAL:
                DrawRectangleLinesEx(
                    (Rectangle) { floor(command.pos_x), floor(command.pos_y), command.width, command.height },
                    command.data.border_width,
                    CONVERT_COLOR(command.color, Color)
                );
                break;
            case BORDER_CONTROL:
                border_control_render(command);
                break;
            case BORDER_CONTROL_BODY:
                border_control_body_render(command);
                break;
            case BORDER_END:
                border_end_render(command);
                break;
            case BORDER_CONTROL_END:
                border_control_end_render(command);
                break;
            case BORDER_NOTCHED:
                border_notched_render(command);
                break;
            default:
                assert(false && "Unhandled draw border type");
                break;
            }
            break;
        case DRAWTYPE_RECT:
            command.width = ceilf(command.width);
            command.height = ceilf(command.height);
            switch (command.subtype) {
            case RECT_NORMAL:
                DrawRectangle(command.pos_x, command.pos_y, command.width, command.height, CONVERT_COLOR(command.color, Color));
                break;
            case RECT_NOTCHED:
                rect_notched_render(command);
                break;
            case RECT_TERMINAL:
                term_resize(command.width, command.height);
                term_draw(command.pos_x, command.pos_y);
                break;
            default:
                assert(false && "Unhandled draw rect type");
                break;
            }
            break;
        case DRAWTYPE_TEXT:
            text_slice_draw(
                *(Font*)command.data.text.font,
                command.data.text.text,
                command.pos_x,
                command.pos_y,
                command.data.text.text_size,
                command.height,
                CONVERT_COLOR(command.color, Color)
            );
            break;
        case DRAWTYPE_IMAGE:
            switch (command.subtype) {
            case IMAGE_NORMAL:
                DrawTextureEx(
                    *image,
                    (Vector2) { command.pos_x + SHADOW_DISTANCE, command.pos_y + SHADOW_DISTANCE },
                    0.0,
                    (float)command.height / (float)image->height,
                    (Color) { 0x00, 0x00, 0x00, 0x80 }
                );
                DrawTextureEx(
                    *image,
                    (Vector2) { command.pos_x, command.pos_y},
                    0.0,
                    (float)command.height / (float)image->height,
                    CONVERT_COLOR(command.color, Color)
                );
                break;
            case IMAGE_STRETCHED:
                DrawTexturePro(
                    *image,
                    (Rectangle) { 0, 0, image->width, image->height },
                    (Rectangle) { command.pos_x, command.pos_y, command.width, command.height },
                    (Vector2) {0},
                    0.0,
                    CONVERT_COLOR(command.color, Color)
                );
                break;
            }
            break;
        case DRAWTYPE_SCISSOR_SET:
            BeginScissorMode(command.pos_x, command.pos_y, command.width, command.height);
            break;
        case DRAWTYPE_SCISSOR_RESET:
            EndScissorMode();
            break;
        case DRAWTYPE_SHADER_BEGIN:
            BeginShaderMode(*(Shader*)command.data.shader);
            break;
        case DRAWTYPE_SHADER_END:
            EndShaderMode();
            break;
        default:
            assert(false && "Unimplemented command render");
            break;
        }
#ifdef DEBUG
        if (show_bounds) DrawRectangleLinesEx((Rectangle) { command.pos_x, command.pos_y, command.width, command.height }, 1.0, (Color) { 0xff, 0x00, 0xff, 0x40 });
#endif
    }
}

static void debug_print(int* num, char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vsnprintf(editor.debug_buffer[(*num)++], DEBUG_BUFFER_LINE_SIZE, fmt, va);
    va_end(va);
}

static void debug_write_buffer(void) {
    int i = 0;
    debug_print(&i, "Scrap " SCRAP_VERSION);
    debug_print(&i, "FPS: %d, Frame time: %.3f", GetFPS(), GetFrameTime());

    if (!editor.show_debug) return;
    debug_print(&i, "UI time: %.6g us", ui.ui_time);
    debug_print(&i, "Elements: %zu, Drawn: %zu", gui->elements_count, gui->command_list.size);
    debug_print(&i, "Mem: %.3f/%.3f MiB", (double)gui->arena->pos / (1024.0 * 1024.0), (double)gui->arena->reserve_size / (1024.0 * 1024.0));
    debug_print(&i, " ");

    debug_print(&i, "Button handler: %p", ui.hover.button.handler);
    debug_print(&i, "Block: %p, Parent: %p, Parent Arg: %p", ui.hover.editor.block, ui.hover.editor.block ? ui.hover.editor.block->parent.as.arg : NULL, ui.hover.editor.parent_argument);
    debug_print(&i, "Block end: %s", ui.hover.editor.block_end ? "true" : "false");
    debug_print(&i, "Argument: %p", ui.hover.editor.argument);
    debug_print(&i, "BlockChain: %p", ui.hover.editor.blockchain);
    debug_print(&i, "Select block: %p, arg: %p, chain: %p", ui.hover.editor.select_block, ui.hover.editor.select_argument, ui.hover.editor.select_blockchain);
    debug_print(&i, "Select block pos: (%.3f, %.3f)", ui.hover.editor.select_block_pos.x, ui.hover.editor.select_block_pos.y);
    debug_print(&i, "Select block bounds Pos: (%.3f, %.3f), Size: (%.3f, %.3f)", ui.hover.panels.code_panel_bounds.x, ui.hover.panels.code_panel_bounds.y, ui.hover.panels.code_panel_bounds.width, ui.hover.panels.code_panel_bounds.height);
    debug_print(&i, "Category: %p", ui.hover.category);
    debug_print(&i, "Mouse chains: %zu, Time: %.3f, Pos: (%d, %d), Click: (%d, %d)", vector_size(editor.mouse_blockchains), ui.hover.time_at_last_pos, GetMouseX(), GetMouseY(), (int)ui.hover.mouse_click_pos.x, (int)ui.hover.mouse_click_pos.y);
    debug_print(&i, "Camera: (%.3f, %.3f), Click: (%.3f, %.3f), Zoom: %.3f", editor.camera.position.x, editor.camera.position.y, editor.camera.click_position.x, editor.camera.click_position.y, editor.camera.zoom);
    debug_print(&i, "Drag cancelled: %d", ui.hover.drag_cancelled);
    debug_print(&i, "Editor: %d, Editing: %p, Blockdef: %p, input: %zu", ui.hover.editor.part, ui.hover.editor.edit_blockdef, ui.hover.editor.blockdef, ui.hover.editor.blockdef_input);
    debug_print(&i, "Slider: %p, min: %d, max: %d", ui.hover.hover_slider.value, ui.hover.hover_slider.min, ui.hover.hover_slider.max);
    debug_print(&i, "Input: %p, Select: %p, Pos: (%.3f, %.3f), ind: (%d, %d)", ui.hover.input_info.input, ui.hover.select_input, ui.hover.input_info.rel_pos.x, ui.hover.input_info.rel_pos.y, ui.hover.select_input_cursor, ui.hover.select_input_mark);
    debug_print(&i, "Panel: %p, side: %d", ui.hover.panels.panel, ui.hover.panels.panel_side);
    debug_print(&i, "Part: %d, Select: %d", ui.dropdown.as.color_picker.hover_part, ui.dropdown.as.color_picker.select_part);
    debug_print(&i, "Anchor: %p, Ref: %p", ui.dropdown.element, ui.dropdown.ref_object);
    debug_print(&i, "Error root: %p, chain: %p, block: %p", vm.compiler_error.root_blockchain, vm.compiler_error.blockchain, vm.compiler_error.block);
    debug_print(&i, "Last save time: %.3g s", editor.project_modified && !editor.project_autosaved ? end_timer(editor.project_time_since_save) / 1e+6 : 0.0);
}

void scrap_gui_process_render(void) {
    if (!ui.render_surface_needs_redraw) return;
    ui.render_surface_needs_redraw = false;

    ClearBackground(GetColor(0x202020ff));
    dots_draw(2, config.ui_size * 2);

    for (int i = 0; i < DEBUG_BUFFER_LINES; i++) editor.debug_buffer[i][0] = 0;
    debug_write_buffer();
    scrap_gui_render();

    if (vm.start_timeout == 0) {
        term_restart();
        compile_error_clear();
        vm.code = editor.code;
        if (!thread_start(&vm.thread, &vm)) {
            actionbar_show(gettext("Start failed!"));
        } else {
            actionbar_show(gettext("Started successfully!"));
        }
    }
}
