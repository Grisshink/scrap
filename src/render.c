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
#include <stdarg.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MOD(x, y) (((x) % (y) + (y)) % (y))
#define LERP(min, max, t) (((max) - (min)) * (t) + (min))
#define UNLERP(min, max, v) (((float)(v) - (float)(min)) / ((float)(max) - (float)(min)))
#define CONVERT_COLOR(color, type) (type) { color.r, color.g, color.b, color.a }

void scrap_gui_draw_code(void);

void sidebar_init(void) {
    sidebar.blocks = vector_create();
    for (vec_size_t i = 0; i < vector_size(vm.blockdefs); i++) {
        if (vm.blockdefs[i]->hidden) continue;
        vector_add(&sidebar.blocks, block_new_ms(vm.blockdefs[i]));
    }
}

void actionbar_show(const char* text) {
    TraceLog(LOG_INFO, "[ACTION] %s", text);
    strncpy(actionbar.text, text, sizeof(actionbar.text) - 1);
    actionbar.show_time = 3.0;
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

void draw_term(int x, int y) {
    pthread_mutex_lock(&term.lock);

    Rectangle final_pos = { term.size.x + x, term.size.y + y, term.size.width, term.size.height };
    DrawRectangleRec(final_pos, BLACK);
    BeginShaderMode(line_shader);
    DrawRectangleLinesEx(final_pos, 2.0, (Color) { 0x60, 0x60, 0x60, 0xff });
    EndShaderMode();

    if (term.buffer) {
        Vector2 pos = (Vector2) { final_pos.x, final_pos.y };
        for (int y = 0; y < term.char_h; y++) {
            pos.x = final_pos.x;
            for (int x = 0; x < term.char_w; x++) {
                DrawTextEx(font_mono, term.buffer[x + y*term.char_w], pos, TERM_CHAR_SIZE, 0.0, WHITE);
                pos.x += term.char_size.x;
            }
            pos.y += TERM_CHAR_SIZE;
        }
        if (fmod(GetTime(), 1.0) <= 0.5) {
            Vector2 cursor_pos = (Vector2) {
                final_pos.x + (term.cursor_pos % term.char_w) * term.char_size.x,
                final_pos.y + (term.cursor_pos / term.char_w) * TERM_CHAR_SIZE,
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

void blockdef_on_hover(FlexElement* el) {
    if (gui_window_is_shown()) return;
    hover_info.editor.part = EDITOR_BLOCKDEF;
    hover_info.editor.blockdef = el->custom_data;
}

void input_on_hover(FlexElement* el) {
    if (gui_window_is_shown()) return;
    hover_info.input = el->custom_data;
    hover_info.blockchain = hover_info.prev_blockchain;
    if (el->draw_type != DRAWTYPE_UNKNOWN) return;
    el->draw_type = DRAWTYPE_BORDER;
    el->color = (GuiColor) { 0xa0, 0xa0, 0xa0, 0xff };
    el->data.border.width = BLOCK_OUTLINE_SIZE;
    el->data.border.type = BORDER_NORMAL;
}

void editor_del_button_on_hover(FlexElement* el) {
    if (gui_window_is_shown()) return;
    if (hover_info.top_bars.handler) return;
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0xff, 0xff, 0xff, 0x80 };
    hover_info.editor.blockdef_input = (size_t)el->custom_data;
    hover_info.top_bars.handler = handle_editor_del_arg_button;
}

void editor_button_on_hover(FlexElement* el) {
    if (gui_window_is_shown()) return;
    if (hover_info.top_bars.handler) return;
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0xff, 0xff, 0xff, 0x80 };
    hover_info.top_bars.handler = el->custom_data;
}

void scrap_gui_draw_editor_button(Texture2D* texture, ButtonClickHandler handler) {
    gui_element_begin(gui);
        gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });
        gui_on_hover(gui, editor_button_on_hover);
        gui_set_custom_data(gui, handler);

        gui_image(gui, texture, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
    gui_element_end(gui);
}

void scrap_gui_draw_blockdef(ScrBlockdef* blockdef, bool editing) {
    bool collision = hover_info.editor.prev_blockdef == blockdef;
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
        ScrInput* input = &blockdef->inputs[i];

        if (hover_info.editor.edit_blockdef == blockdef) {
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
                        gui_set_align(gui, ALIGN_CENTER);
                        gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                        if (hover_info.select_input == &input->data.text) gui_set_border(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff }, BLOCK_OUTLINE_SIZE);
                        gui_set_custom_data(gui, &input->data.text);
                        gui_on_hover(gui, input_on_hover);

                        gui_element_begin(gui);
                            gui_set_direction(gui, DIRECTION_VERTICAL);
                            gui_set_align(gui, ALIGN_CENTER);
                            gui_set_grow(gui, DIRECTION_HORIZONTAL);

                            gui_text(gui, &font_cond, input->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0x00, 0x00, 0x00, 0xff });
                        gui_element_end(gui);
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
            scrap_gui_draw_blockdef(input->data.arg.blockdef, editing);
            break;
        default:
            gui_text(gui, &font_cond_shadow, "NODEF", BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            break;
        }

        if (hover_info.editor.edit_blockdef == blockdef) {
                gui_element_begin(gui);
                    gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0x40 });
                    gui_on_hover(gui, editor_del_button_on_hover);
                    gui_set_custom_data(gui, (void*)i);

                    gui_image(gui, &del_arg_tex, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_element_end(gui);
            gui_element_end(gui);
        }
    }

    gui_element_end(gui);
    gui_element_end(gui);
}

void block_on_hover(FlexElement* el) {
    if (gui_window_is_shown()) return;
    hover_info.block = el->custom_data;
    hover_info.blockchain = hover_info.prev_blockchain;
}

void block_argument_on_hover(FlexElement* el) {
    hover_info.prev_argument = el->custom_data;
    hover_info.blockchain = hover_info.prev_blockchain;
}

void argument_on_hover(FlexElement* el) {
    if (gui_window_is_shown()) return;
    hover_info.argument = el->custom_data;
    hover_info.input = &hover_info.argument->data.text;
    hover_info.blockchain = hover_info.prev_blockchain;
    if (el->draw_type != DRAWTYPE_UNKNOWN) return;
    el->draw_type = DRAWTYPE_BORDER;
    el->color = (GuiColor) { 0xa0, 0xa0, 0xa0, 0xff };
    el->data.border.width = BLOCK_OUTLINE_SIZE;
    el->data.border.type = BORDER_NORMAL;
}

void scrap_gui_draw_block(ScrBlock* block, bool highlight) {
    bool collision = hover_info.prev_block == block || highlight;
    Color color = CONVERT_COLOR(block->blockdef->color, Color);
    Color block_color = collision ? ColorBrightness(color, 0.3) : color;
    Color dropdown_color = collision ? color : ColorBrightness(color, -0.3);
    Color outline_color = highlight ? YELLOW : ColorBrightness(color, collision ? 0.5 : -0.2);

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
        gui_set_custom_data(gui, block);
        if (block->blockdef->type == BLOCKTYPE_HAT) gui_set_rect_type(gui, RECT_NOTCHED);
        gui_on_hover(gui, block_on_hover);

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
    ScrInput* inputs = block->blockdef->inputs;
    size_t inputs_size = vector_size(inputs);
    for (size_t i = 0; i < inputs_size; i++) {
        ScrInput* input = &inputs[i];
        ScrArgument* arg = &block->arguments[arg_id];

        switch (input->type) {
        case INPUT_TEXT_DISPLAY:
            gui_text(gui, &font_cond_shadow, input->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            break;
        case INPUT_IMAGE_DISPLAY:
            gui_image(gui, input->data.image.image_ptr, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            break;
        case INPUT_ARGUMENT:
            switch (arg->type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                gui_element_begin(gui);
                    if (arg->type == ARGUMENT_TEXT) {
                        gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                    } else {
                        gui_set_rect(gui, CONVERT_COLOR(dropdown_color, GuiColor));
                    }

                    gui_element_begin(gui);
                        gui_set_direction(gui, DIRECTION_HORIZONTAL);
                        gui_set_min_size(gui, conf.font_size - BLOCK_OUTLINE_SIZE * 4, conf.font_size - BLOCK_OUTLINE_SIZE * 4);
                        gui_set_align(gui, ALIGN_CENTER);
                        gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                        if (hover_info.select_argument == arg) gui_set_border(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff }, BLOCK_OUTLINE_SIZE);
                        gui_set_custom_data(gui, arg);
                        gui_on_hover(gui, argument_on_hover);

                        gui_element_begin(gui);
                            gui_set_direction(gui, DIRECTION_VERTICAL);
                            gui_set_align(gui, ALIGN_CENTER);
                            gui_set_grow(gui, DIRECTION_HORIZONTAL);

                            if (arg->type == ARGUMENT_TEXT) {
                                gui_text(gui, &font_cond, arg->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0x00, 0x00, 0x00, 0xff });
                            } else {
                                gui_text(gui, &font_cond_shadow, arg->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                            }
                        gui_element_end(gui);
                    gui_element_end(gui);
                gui_element_end(gui);
                break;
            case ARGUMENT_BLOCK:
                gui_element_begin(gui);
                    gui_on_hover(gui, block_argument_on_hover);
                    gui_set_custom_data(gui, arg);

                    scrap_gui_draw_block(&arg->data.block, highlight);
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

                if (hover_info.select_argument == arg && hover_info.dropdown.location == LOCATION_BLOCK_DROPDOWN) {
                    hover_info.dropdown.element = gui_get_element(gui);
                }

                gui_element_begin(gui);
                    gui_set_min_size(gui, 0, conf.font_size - BLOCK_OUTLINE_SIZE * 4);
                    gui_set_align(gui, ALIGN_CENTER);
                    gui_set_padding(gui, BLOCK_STRING_PADDING / 2, 0);
                    gui_set_direction(gui, DIRECTION_HORIZONTAL);
                    if (hover_info.select_argument == arg) gui_set_border(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff }, BLOCK_OUTLINE_SIZE);
                    gui_on_hover(gui, argument_on_hover);
                    gui_set_custom_data(gui, arg);

                    gui_text(gui, &font_cond_shadow, arg->data.text, BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                    gui_image(gui, &drop_tex, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
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
                gui_on_hover(gui, argument_on_hover);

                scrap_gui_draw_blockdef(arg->data.blockdef, hover_info.editor.edit_blockdef == arg->data.blockdef);

                if (hover_info.editor.edit_blockdef == arg->data.blockdef) {
                    scrap_gui_draw_editor_button(&add_arg_tex, handle_editor_add_arg_button);
                    scrap_gui_draw_editor_button(&add_text_tex, handle_editor_add_text_button);
                    scrap_gui_draw_editor_button(&close_tex, handle_editor_close_button);
                } else {
                    scrap_gui_draw_editor_button(&edit_tex, handle_editor_edit_button);
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

void button_on_hover(FlexElement* el) {
    if (gui_window_is_shown()) return;
    if (hover_info.top_bars.handler) return;
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    hover_info.top_bars.handler = el->custom_data;
}

FlexElement* scrap_gui_draw_button(const char* text, int size, bool selected, ButtonClickHandler handler) {
    FlexElement* el;
    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_align(gui, ALIGN_CENTER);
        gui_set_min_size(gui, 0, size);
        gui_set_padding(gui, conf.font_size * 0.3, 0);
        if (selected) gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        gui_on_hover(gui, button_on_hover);
        gui_set_custom_data(gui, handler);
        el = gui_get_element(gui);

        gui_text(gui, &font_cond, text, BLOCK_TEXT_SIZE, selected ? (GuiColor) { 0x00, 0x00, 0x00, 0xff } : (GuiColor) { 0xff, 0xff, 0xff, 0xff });
    gui_element_end(gui);
    return el;
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

        FlexElement* el = scrap_gui_draw_button("File", top_bar_size, false, handle_file_button_click);
        if (hover_info.dropdown.location == LOCATION_FILE_MENU) hover_info.dropdown.element = el;
        scrap_gui_draw_button("Settings", top_bar_size, false, handle_settings_button_click);
        scrap_gui_draw_button("About", top_bar_size, false, handle_about_button_click);
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

        //scrap_gui_draw_button("Code", tab_bar_size, current_tab == TAB_CODE, handle_code_tab_click);
        //scrap_gui_draw_button("Output", tab_bar_size, current_tab == TAB_OUTPUT, handle_output_tab_click);

        gui_grow(gui, DIRECTION_HORIZONTAL);
        gui_text(gui, &font_cond, project_name, BLOCK_TEXT_SIZE, (GuiColor) { 0x80, 0x80, 0x80, 0xff });
        gui_grow(gui, DIRECTION_HORIZONTAL);
        
        gui_element_begin(gui);
            gui_on_hover(gui, button_on_hover);
            gui_set_custom_data(gui, handle_stop_button_click);

            gui_image(gui, &stop_tex, tab_bar_size, CONVERT_COLOR(WHITE, GuiColor));
        gui_element_end(gui);
        gui_element_begin(gui);
            gui_on_hover(gui, button_on_hover);
            gui_set_custom_data(gui, handle_run_button_click);

            if (vm.is_running) {
                gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_image(gui, &run_tex, tab_bar_size, (GuiColor) { 0x00, 0x00, 0x00, 0xff });
            } else {
                gui_image(gui, &run_tex, tab_bar_size, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            }
        gui_element_end(gui);
    gui_element_end(gui);
}

void blockchain_on_hover(FlexElement* el) {
    hover_info.prev_blockchain = el->custom_data;
}

void scrap_gui_draw_blockchain(ScrBlockChain* chain) {
    int layer = 0;
    bool highlight = hover_info.exec_chain == chain;

    gui_element_begin(gui);
        gui_set_direction(gui, DIRECTION_VERTICAL);
        //gui_set_border(gui, CONVERT_COLOR(YELLOW, GuiColor), BLOCK_OUTLINE_SIZE);
        gui_on_hover(gui, blockchain_on_hover);
        gui_set_custom_data(gui, chain);
        gui_set_padding(gui, 5, 5);

    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        ScrBlockdef* blockdef = chain->blocks[i].blockdef;
        bool block_highlight = hover_info.exec_ind == i;

        if (blockdef->type == BLOCKTYPE_END) {
            gui_element_end(gui);
            gui_element_end(gui);

            FlexElement* el = gui_get_element(gui);

            ScrBlock* block = el->custom_data;

            bool collision = hover_info.prev_block == &chain->blocks[i] || (highlight && block_highlight);
            Color color = CONVERT_COLOR(block->blockdef->color, Color);
            Color block_color = ColorBrightness(color, collision ? 0.3 : 0.0);
            Color outline_color = highlight && block_highlight ? YELLOW : ColorBrightness(block_color, collision ? 0.5 : -0.2);

            gui_element_begin(gui);
                gui_set_min_size(gui, block->width, conf.font_size);
                gui_set_rect(gui, CONVERT_COLOR(block_color, GuiColor));
                gui_on_hover(gui, block_on_hover);
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

                scrap_gui_draw_block(&chain->blocks[i], highlight && block_highlight);
        } else {
            if (blockdef->type == BLOCKTYPE_CONTROL) {
                gui_element_begin(gui);
                    gui_set_direction(gui, DIRECTION_VERTICAL);
                    gui_set_custom_data(gui, &chain->blocks[i]);
            }
            scrap_gui_draw_block(&chain->blocks[i], highlight && block_highlight);
        }

        if (blockdef->type == BLOCKTYPE_CONTROL || blockdef->type == BLOCKTYPE_CONTROLEND) {
            layer++;

            FlexElement* el = gui_get_element(gui);
            chain->blocks[i].width = el->w;

            bool collision = hover_info.prev_block == &chain->blocks[i] || (highlight && block_highlight);
            Color color = CONVERT_COLOR(blockdef->color, Color);
            Color block_color = ColorBrightness(color, collision ? 0.3 : 0.0);
            Color outline_color = highlight && block_highlight ? YELLOW : ColorBrightness(block_color, collision ? 0.5 : -0.2);

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

void sidebar_on_hover(FlexElement* el) {
    (void) el;
    hover_info.sidebar = 1;
}

void scrap_gui_draw_sidebar(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x80 });
        gui_set_padding(gui, SIDE_BAR_PADDING, SIDE_BAR_PADDING);
        gui_set_gap(gui, SIDE_BAR_PADDING);
        gui_on_hover(gui, sidebar_on_hover);
        gui_set_scroll(gui, &sidebar.scroll_amount);
        gui_set_scroll_scaling(gui, conf.font_size * 4);
        gui_set_scissor(gui);

        for (size_t i = dropdown.scroll_amount; i < vector_size(sidebar.blocks); i++) {
            scrap_gui_draw_block(&sidebar.blocks[i], false);
        }
    gui_element_end(gui);
}

void scrap_gui_draw_code_area(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_direction(gui, DIRECTION_VERTICAL);
        gui_set_padding(gui, 0, conf.font_size * 2);
        gui_set_align(gui, ALIGN_CENTER);
        gui_set_scissor(gui);

        scrap_gui_draw_code();

        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, 0, 0);
            gui_set_padding(gui, conf.font_size * 0.2, conf.font_size * 0.2);

            for (int i = 0; i < DEBUG_BUFFER_LINES; i++) {
                gui_text(gui, &font_cond, debug_buffer[i], conf.font_size * 0.5, (GuiColor) { 0xff, 0xff, 0xff, 0x60 });
            }
        gui_element_end(gui);

        if (actionbar.show_time > 0) {
            Color color = YELLOW;
            color.a = actionbar.show_time / 3.0 * 255.0;
            gui_text(gui, &font_eb, actionbar.text, conf.font_size * 0.8, CONVERT_COLOR(color, GuiColor));
        }
    gui_element_end(gui);
}

void scrap_gui_draw_split_preview(void) {
    if (split_preview.side == SPLIT_SIDE_NONE) return;

    gui_element_begin(gui);
        gui_set_floating(gui);
        gui_set_position(gui, 0, 0);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);

        if (split_preview.side == SPLIT_SIDE_LEFT || split_preview.side == SPLIT_SIDE_RIGHT) gui_set_direction(gui, DIRECTION_HORIZONTAL);

        if (split_preview.side == SPLIT_SIDE_BOTTOM) gui_grow(gui, DIRECTION_VERTICAL);
        if (split_preview.side == SPLIT_SIDE_RIGHT) gui_grow(gui, DIRECTION_HORIZONTAL);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_rect(gui, (GuiColor) { 0x00, 0x80, 0xff, 0x80});
        gui_element_end(gui);

        if (split_preview.side == SPLIT_SIDE_TOP) gui_grow(gui, DIRECTION_VERTICAL);
        if (split_preview.side == SPLIT_SIDE_LEFT) gui_grow(gui, DIRECTION_HORIZONTAL);
    gui_element_end(gui);
}

void scrap_gui_draw_term_panel(void) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_padding(gui, conf.font_size * 0.5, conf.font_size * 0.5);
        gui_set_rect(gui, (GuiColor) { 0x20, 0x20, 0x20, 0xff });

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0xff });
            gui_set_rect_type(gui, RECT_TERMINAL);
        gui_element_end(gui);
    gui_element_end(gui);
}

void scrap_gui_draw_panel(PanelTree* panel) {
    switch (panel->type) {
    case PANEL_NONE:
        assert(false && "Attempt to render panel with type PANEL_NONE");
        break;
    case PANEL_SIDEBAR:
        scrap_gui_draw_sidebar();
        break;
    case PANEL_CODE:
        scrap_gui_draw_code_area();
        break;
    case PANEL_TERM:
        scrap_gui_draw_term_panel();
        break;
    case PANEL_SPLIT:
        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, panel->direction);

            gui_element_begin(gui);
                if (panel->direction == DIRECTION_VERTICAL) {
                    gui_set_percent_size(gui, panel->split_percent, DIRECTION_VERTICAL);
                    gui_set_grow(gui, DIRECTION_HORIZONTAL);
                } else {
                    gui_set_grow(gui, DIRECTION_VERTICAL);
                    gui_set_percent_size(gui, panel->split_percent, DIRECTION_HORIZONTAL);
                }

                scrap_gui_draw_panel(panel->left);
            gui_element_end(gui);
            
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);

                scrap_gui_draw_panel(panel->right);
            gui_element_end(gui);
        gui_element_end(gui);
        break;
    }
    if (panel->type != PANEL_SPLIT) scrap_gui_draw_split_preview();
}

void scrap_gui_draw_code(void) {
    for (size_t i = 0; i < vector_size(editor_code); i++) {
        Vector2 chain_pos = (Vector2) {
            editor_code[i].x - camera_pos.x, 
            editor_code[i].y - camera_pos.y,
        };
        // FIXME: code renderer does not properly check culling bounds
        if (chain_pos.x > gui->win_w || chain_pos.y > gui->win_h) continue;
        if (editor_code[i].width > 0 && editor_code[i].height > 0 && 
            (chain_pos.x + editor_code[i].width < 0 || chain_pos.y + editor_code[i].height < 0)) continue;
        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, chain_pos.x, chain_pos.y);

            scrap_gui_draw_blockchain(&editor_code[i]);
        gui_element_end(gui);
        FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len];
        editor_code[i].width = el->w;
        editor_code[i].height = el->h;
    }
}

void dropdown_on_hover(FlexElement* el) {
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    // Double cast to avoid warning. In our case this operation is safe because el->custom_data currently stores a value of type int
    hover_info.dropdown.select_ind = (int)(size_t)el->custom_data;

    hover_info.top_bars.handler = hover_info.dropdown.handler;
}

void scrap_gui_draw_dropdown(void) {
    const int max_list_size = 10;

    if (!hover_info.dropdown.location) return;
    hover_info.top_bars.handler = handle_dropdown_close;
    gui_element_begin(gui);
        gui_set_floating(gui);
        gui_set_rect(gui, (GuiColor) { 0x40, 0x40, 0x40, 0xff });
        gui_set_gap(gui, 2);
        gui_set_padding(gui, 2, 2);
        gui_set_anchor(gui, hover_info.dropdown.element);
        gui_set_position(gui, 0, hover_info.dropdown.element->h);
        if (hover_info.dropdown.list_len > max_list_size) {
            gui_set_scissor(gui);
            gui_set_fixed(gui, hover_info.dropdown.element->w + 5, max_list_size * (conf.font_size + 2) + 4);
            gui_set_scroll(gui, &hover_info.dropdown.scroll_amount);
            gui_set_scroll_scaling(gui, (conf.font_size + 2) * 2);
        } else {
            gui_set_min_size(gui, hover_info.dropdown.element->w, 0);
        }

        for (int i = 0; i < hover_info.dropdown.list_len; i++) {
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_CENTER);
                gui_set_min_size(gui, 0, conf.font_size);
                gui_set_padding(gui, conf.font_size * 0.3, 0);
                gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
                gui_on_hover(gui, dropdown_on_hover);
                gui_set_custom_data(gui, (void*)(size_t)i);

                gui_text(gui, &font_cond, hover_info.dropdown.list[i], BLOCK_TEXT_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_element_end(gui);
        }
    gui_element_end(gui);
}

void scrap_gui_process(void) {
    // Gui
    gui_begin(gui);
        scrap_gui_draw_top_bar();
        scrap_gui_draw_tab_bar();
        if (current_tab == TAB_CODE) {
            scrap_gui_draw_panel(root_panel);
        } else if (current_tab == TAB_OUTPUT) {
        }
        handle_window();
        if (current_tab == TAB_CODE) {
            gui_element_begin(gui);
                gui_set_floating(gui);
                gui_set_position(gui, gui->mouse_x, gui->mouse_y);

                scrap_gui_draw_blockchain(&mouse_blockchain);
            gui_element_end(gui);
        }

        scrap_gui_draw_dropdown();
    gui_end(gui);
}


// Draw order for scrap_gui_render_border_control()
//
//           1
//   +---------------+ 
// 4 |               | 2
//   +     +---------+
//             3
//
void scrap_gui_render_border_control(DrawCommand* cmd) {
    unsigned short border_w = cmd->data.border.width;
    Color color = CONVERT_COLOR(cmd->color, Color);

    /* 1 */ DrawRectangle(cmd->pos_x, cmd->pos_y, cmd->width, border_w, color);
    /* 2 */ DrawRectangle(cmd->pos_x + cmd->width - border_w, cmd->pos_y, border_w, cmd->height, color);
    /* 3 */ DrawRectangle(cmd->pos_x + BLOCK_CONTROL_INDENT - border_w, cmd->pos_y + cmd->height - border_w, cmd->width - BLOCK_CONTROL_INDENT, border_w, color);
    /* 4 */ DrawRectangle(cmd->pos_x, cmd->pos_y, border_w, cmd->height, color);
}

// Draw order for scrap_gui_render_border_control_body()
//
//   +     +
// 1 |     | 2
//   +     +
//
void scrap_gui_render_border_control_body(DrawCommand* cmd) {
    unsigned short border_w = cmd->data.border.width;
    Color color = CONVERT_COLOR(cmd->color, Color);

    /* 1 */ DrawRectangle(cmd->pos_x, cmd->pos_y, border_w, cmd->height, color);
    /* 2 */ DrawRectangle(cmd->pos_x + cmd->width - border_w, cmd->pos_y, border_w, cmd->height, color);
}

// Draw order for scrap_gui_render_border_control_end()
//
//              1
//   +     +---------+
// 4 |               | 2
//   +     +---------+
//              3
//
void scrap_gui_render_border_control_end(DrawCommand* cmd) {
    unsigned short border_w = cmd->data.border.width;
    Color color = CONVERT_COLOR(cmd->color, Color);

    /* 1 */ DrawRectangle(cmd->pos_x + BLOCK_CONTROL_INDENT - border_w, cmd->pos_y, cmd->width - BLOCK_CONTROL_INDENT, border_w, color);
    /* 2 */ DrawRectangle(cmd->pos_x + cmd->width - border_w, cmd->pos_y, border_w, cmd->height, color);
    /* 3 */ DrawRectangle(cmd->pos_x + BLOCK_CONTROL_INDENT - border_w, cmd->pos_y + cmd->height - border_w, cmd->width - BLOCK_CONTROL_INDENT, border_w, color);
    /* 4 */ DrawRectangle(cmd->pos_x, cmd->pos_y, border_w, cmd->height, color);
}

// Draw order for scrap_gui_render_border_end()
//
//              1
//   +     +---------+
// 4 |               | 2
//   +---------------+ 
//           3
void scrap_gui_render_border_end(DrawCommand* cmd) {
    unsigned short border_w = cmd->data.border.width;
    Color color = CONVERT_COLOR(cmd->color, Color);

    /* 1 */ DrawRectangle(cmd->pos_x + BLOCK_CONTROL_INDENT - border_w, cmd->pos_y, cmd->width - BLOCK_CONTROL_INDENT, border_w, color);
    /* 2 */ DrawRectangle(cmd->pos_x + cmd->width - border_w, cmd->pos_y, border_w, cmd->height, color);
    /* 3 */ DrawRectangle(cmd->pos_x, cmd->pos_y + cmd->height - border_w, cmd->width, border_w, color);
    /* 4 */ DrawRectangle(cmd->pos_x, cmd->pos_y, border_w, cmd->height, color);
}

// Draw order for scrap_gui_render_border_notched() and scrap_gui_render_rect_notched()
//
//           1
//   +--------------+ 2
//   |               +
// 5 |               | 3
//   +---------------+ 
//           4
void scrap_gui_render_border_notched(DrawCommand* cmd) {
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

void scrap_gui_render_rect_notched(DrawCommand* cmd) {
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

void scrap_gui_render(void) {
#ifdef DEBUG
    bool show_bounds = IsKeyDown(KEY_F4);
#endif
    DrawCommand* command;
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
                scrap_gui_render_border_control(command);
                break;
            case BORDER_CONTROL_BODY:
                scrap_gui_render_border_control_body(command);
                break;
            case BORDER_END:
                scrap_gui_render_border_end(command);
                break;
            case BORDER_CONTROL_END:
                scrap_gui_render_border_control_end(command);
                break;
            case BORDER_NOTCHED:
                scrap_gui_render_border_notched(command);
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
                scrap_gui_render_rect_notched(command);
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

void print_debug(int* num, char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vsnprintf(debug_buffer[(*num)++], DEBUG_BUFFER_LINE_SIZE, fmt, va);
    va_end(va);
}

void write_debug_buffer(void) {
    int i = 0;
#ifdef DEBUG
    print_debug(&i, "Block: %p, Parent: %p", hover_info.block, hover_info.block ? hover_info.block->parent : NULL);
    print_debug(&i, "Argument: %p", hover_info.argument);
    print_debug(&i, "BlockChain: %p", hover_info.blockchain);
    print_debug(&i, "Prev argument: %p", hover_info.prev_argument);
    print_debug(&i, "Select block: %p", hover_info.select_block);
    print_debug(&i, "Select arg: %p", hover_info.select_argument);
    print_debug(&i, "Sidebar: %d", hover_info.sidebar);
    print_debug(&i, "Mouse: %p, Time: %.3f, Pos: (%d, %d), Click: (%d, %d)", mouse_blockchain.blocks, hover_info.time_at_last_pos, GetMouseX(), GetMouseY(), (int)hover_info.mouse_click_pos.x, (int)hover_info.mouse_click_pos.y);
    print_debug(&i, "Camera: (%.3f, %.3f), Click: (%.3f, %.3f)", camera_pos.x, camera_pos.y, camera_click_pos.x, camera_click_pos.y);
    print_debug(&i, "Dropdown scroll: %d", dropdown.scroll_amount);
    print_debug(&i, "Drag cancelled: %d", hover_info.drag_cancelled);
    print_debug(&i, "Min: (%.3f, %.3f), Max: (%.3f, %.3f)", block_code.min_pos.x, block_code.min_pos.y, block_code.max_pos.x, block_code.max_pos.y);
    print_debug(&i, "Sidebar scroll: %d", sidebar.scroll_amount);
    print_debug(&i, "Editor: %d, Editing: %p, Blockdef: %p, input: %zu", hover_info.editor.part, hover_info.editor.edit_blockdef, hover_info.editor.blockdef, hover_info.editor.blockdef_input);
    print_debug(&i, "Elements: %zu/%zu, Draw: %zu/%zu", gui->element_stack_len, ELEMENT_STACK_SIZE, gui->command_stack_len, COMMAND_STACK_SIZE);
    print_debug(&i, "Slider: %p, min: %d, max: %d", hover_info.hover_slider.value, hover_info.hover_slider.min, hover_info.hover_slider.max);
    print_debug(&i, "Input: %p, Select: %p", hover_info.input, hover_info.select_input);
    print_debug(&i, "Exec chain: %p, ind: %zu", hover_info.exec_chain, hover_info.exec_ind);
    print_debug(&i, "UI time: %.3f", ui_time);
    print_debug(&i, "FPS: %d, Frame time: %.3f", GetFPS(), GetFrameTime());
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
}
