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
#include "../external/tinyfiledialogs.h"
#include "scrap.h"

#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

typedef struct {
    bool shown;
    float animation_time;
    bool is_fading;
    bool is_hiding;
    Vector2 pos;
    WindowGuiType type;
} WindowGui;

Config window_conf;
static WindowGui window = {0};
static bool settings_tooltip = false;

// https://easings.net/#easeOutExpo
float ease_out_expo(float x) {
    return x == 1.0 ? 1.0 : 1 - powf(2.0, -10.0 * x);
}

void init_gui_window(void) {
    window.is_fading = true;
}

bool gui_window_is_shown(void) {
    return window.shown;
}

WindowGuiType gui_window_get_type(void) {
    return window.type;
}

void gui_window_show(WindowGuiType type) {
    config_free(&window_conf); // Drop old strings and replace with new
    config_copy(&window_conf, &conf);
    window.is_fading = false;
    window.type = type;
    window.pos = hover_info.top_bars.pos;
    shader_time = -0.2;
}

void gui_window_hide(void) {
    window.is_fading = true;
}

void gui_window_hide_immediate(void) {
    window.is_fading = true;
    window.is_hiding = true;
}

static void settings_button_on_hover(FlexElement* el) {
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    hover_info.top_bars.handler = el->custom_data;
}

static void close_button_on_hover(FlexElement* el) {
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    hover_info.top_bars.handler = handle_window_gui_close_button_click;
}

static void window_on_hover(FlexElement* el) {
    (void) el;
    hover_info.top_bars.handler = NULL;
}

static void scrap_gui_begin_window(const char* title, int w, int h, float scaling) {
    hover_info.top_bars.handler = handle_window_gui_close_button_click;
    gui_element_begin(gui);
        gui_set_floating(gui);
        gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x40 * scaling });
        gui_set_position(gui, 0, 0);
        gui_set_fixed(gui, gui->win_w, gui->win_h);
    gui_element_end(gui);

    gui_element_begin(gui);
        gui_scale_element(gui, scaling);
        gui_set_floating(gui);
        gui_set_position(gui, (gui->win_w / 2) / scaling - w / 2, (gui->win_h / 2) / scaling - h / 2);
        gui_set_fixed(gui, w, h);
        gui_set_rect(gui, (GuiColor) { 0x20, 0x20, 0x20, 0xff });
        gui_set_direction(gui, DIRECTION_VERTICAL);
        gui_on_hover(gui, window_on_hover);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_min_size(gui, 0, conf.font_size * 1.2);
            gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_CENTER);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            gui_text(gui, &font_eb, title, conf.font_size * 0.8, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_grow(gui, DIRECTION_HORIZONTAL);
        gui_element_end(gui);

        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, w - conf.font_size * 1.2, 0);
            gui_set_fixed(gui, conf.font_size * 1.2, conf.font_size * 1.2);
            gui_set_align(gui, ALIGN_CENTER);
            gui_on_hover(gui, close_button_on_hover);
            
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_CENTER);

                gui_text(gui, &font_cond, "X", conf.font_size * 0.8, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_element_end(gui);
        gui_element_end(gui);

        gui_element_begin(gui);
            gui_set_direction(gui, DIRECTION_VERTICAL);
            gui_set_padding(gui, conf.font_size * 0.5, conf.font_size * 0.5);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_gap(gui, WINDOW_ELEMENT_PADDING);
}

static void scrap_gui_end_window(void) {
    gui_element_end(gui);
    gui_element_end(gui);
}

static void warning_on_hover(FlexElement* el) {
    (void) el;
    settings_tooltip = true;
}

static void scrap_gui_begin_setting(const char* name, bool warning) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_gap(gui, WINDOW_ELEMENT_PADDING);
        gui_set_min_size(gui, 0, conf.font_size);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_CENTER);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            gui_text(gui, &font_cond, name, conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        gui_element_end(gui);

        if (warning) {
            gui_element_begin(gui);
                gui_set_image(gui, &warn_tex, conf.font_size, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_on_hover(gui, warning_on_hover);
            gui_element_end(gui);
        } else {
            gui_spacer(gui, conf.font_size, conf.font_size);
        }

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_CENTER);
            gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
}

static void slider_on_hover(FlexElement* el) {
    unsigned int len;
    hover_info.hover_slider = *(SliderHoverInfo*)gui_get_state(el, &len);
    el->color = hover_info.hover_slider.value == hover_info.dragged_slider.value ? (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff } : (GuiColor) { 0x40, 0x40, 0x40, 0xff };
}

static void scrap_gui_slider(int min, int max, int* value) {
    SliderHoverInfo info = (SliderHoverInfo) {
        .min = min,
        .max = max,
        .value = value,
    };
    gui_on_hover(gui, slider_on_hover);
    SliderHoverInfo* state = gui_set_state(gui, &info, sizeof(info));
    snprintf(state->value_str, 16, "%d", *state->value);

    gui_image(gui, &arrow_left_tex, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
    gui_grow(gui, DIRECTION_HORIZONTAL);
    gui_text(gui, &font_cond, state->value_str, conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
    gui_grow(gui, DIRECTION_HORIZONTAL);
    gui_image(gui, &arrow_right_tex, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
}

static void scrap_gui_end_setting(void) {
    gui_element_end(gui);
    gui_element_end(gui);
}

void text_input_on_hover(FlexElement* el) {
    hover_info.input = el->custom_data;
    if (hover_info.input == hover_info.select_input) return;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
}

void scrap_gui_text_input(char** input) {
    gui_on_hover(gui, text_input_on_hover);
    gui_set_custom_data(gui, input);
    if (input == hover_info.select_input) gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });

    gui_spacer(gui, WINDOW_ELEMENT_PADDING, 0);
    gui_text(gui, &font_cond, *input, conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
    if (input == hover_info.select_input) {
        gui_element_begin(gui);
            gui_set_min_size(gui, 2, conf.font_size * 0.6);
            gui_set_rect(gui, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        gui_element_end(gui);
    }
}

void handle_gui(void) {
    if (window.is_hiding) {
        window.shown = false;
        window.is_hiding = false;
    }
    if (window.is_fading) {
        window.animation_time = MAX(window.animation_time - GetFrameTime() * 2.0, 0.0);
        if (window.animation_time == 0.0) window.shown = false;
    } else {
        window.shown = true;
        window.animation_time = MIN(window.animation_time + GetFrameTime() * 2.0, 1.0);
    }

    if (!window.shown) return;

    float animation_ease = ease_out_expo(window.animation_time);

    switch (window.type) {
    case GUI_TYPE_SETTINGS:
        scrap_gui_begin_window("Settings", 0.6 * gui->win_w, 0.8 * gui->win_h, animation_ease);
            scrap_gui_begin_setting("UI size", true);
                scrap_gui_slider(8, 64, &window_conf.font_size);
            scrap_gui_end_setting();

            scrap_gui_begin_setting("Side bar size", false);
                scrap_gui_slider(10, 500, &window_conf.side_bar_size);
            scrap_gui_end_setting();

            scrap_gui_begin_setting("FPS Limit", false);
                scrap_gui_slider(0, 240, &window_conf.fps_limit);
            scrap_gui_end_setting();

            scrap_gui_begin_setting("Font path", true);
                scrap_gui_text_input(&window_conf.font_path);
            scrap_gui_end_setting();

            scrap_gui_begin_setting("Bold font path", true);
                scrap_gui_text_input(&window_conf.font_bold_path);
            scrap_gui_end_setting();

            scrap_gui_begin_setting("Monospaced font path", true);
                scrap_gui_text_input(&window_conf.font_mono_path);
            scrap_gui_end_setting();

            gui_grow(gui, DIRECTION_VERTICAL);

            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_gap(gui, WINDOW_ELEMENT_PADDING);

                gui_grow(gui, DIRECTION_HORIZONTAL);

                gui_element_begin(gui);
                    gui_set_min_size(gui, 0, conf.font_size);
                    gui_set_padding(gui, WINDOW_ELEMENT_PADDING, WINDOW_ELEMENT_PADDING);
                    gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
                    gui_set_align(gui, ALIGN_CENTER);
                    gui_set_direction(gui, DIRECTION_HORIZONTAL);
                    gui_on_hover(gui, settings_button_on_hover);
                    gui_set_custom_data(gui, handle_settings_reset_button_click);

                    gui_text(gui, &font_cond, "Reset", conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_element_end(gui);

                gui_element_begin(gui);
                    gui_set_min_size(gui, 0, conf.font_size);
                    gui_set_padding(gui, WINDOW_ELEMENT_PADDING, WINDOW_ELEMENT_PADDING);
                    gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
                    gui_set_align(gui, ALIGN_CENTER);
                    gui_set_direction(gui, DIRECTION_HORIZONTAL);
                    gui_on_hover(gui, settings_button_on_hover);
                    gui_set_custom_data(gui, handle_settings_apply_button_click);

                    gui_text(gui, &font_cond, "Apply", conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_element_end(gui);
            gui_element_end(gui);
        scrap_gui_end_window();
        break;
    case GUI_TYPE_ABOUT:
        scrap_gui_begin_window("About", 500 * conf.font_size / 32.0, 250 * conf.font_size / 32.0, animation_ease);
            gui_text(gui, &font_cond, "About page", conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        scrap_gui_end_window();
        break;
    default:
        assert(false && "Unhandled window draw");
        break;
    }

    if (settings_tooltip) {
        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x80 });
            gui_set_position(gui, gui->mouse_x + 10, gui->mouse_y + 10);
            gui_set_padding(gui, WINDOW_ELEMENT_PADDING * 0.5, WINDOW_ELEMENT_PADDING * 0.5);

            gui_text(gui, &font_cond, "Needs restart for changes to take effect", conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        gui_element_end(gui);
    }

    settings_tooltip = false;

    /*case GUI_TYPE_FILE:
        gui_size.x = 150 * conf.font_size / 32.0;
        gui_size.y = conf.font_size * 2;
        window.ctx->style.window.spacing = nk_vec2(0, 0);
        window.ctx->style.button.text_alignment = NK_TEXT_LEFT;

        if (nk_begin(
                window.ctx, 
                "File", 
                nk_rect(window.pos.x, window.pos.y + conf.font_size * 1.2, gui_size.x, gui_size.y), 
                NK_WINDOW_NO_SCROLLBAR)
        ) {
            nk_layout_row_dynamic(window.ctx, conf.font_size, 1);
            if (nk_button_label(window.ctx, "Save project")) {
                char const* filters[] = {"*.scrp"};
                char* save_path = tinyfd_saveFileDialog(NULL, "project.scrp", ARRLEN(filters), filters, "Scrap project files (.scrp)"); 
                if (save_path) save_code(save_path, editor_code);
            }
            if (nk_button_label(window.ctx, "Load project")) {
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
        nk_end(window.ctx);
        break;
    default:
        break;
    }*/
}
