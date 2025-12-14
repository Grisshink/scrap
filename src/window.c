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

#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <libintl.h>

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
    hover_info.select_input = NULL;
    window.is_fading = true;
}

void gui_window_hide_immediate(void) {
    hover_info.select_input = NULL;
    window.is_fading = true;
    window.is_hiding = true;
}

static void settings_button_on_hover(GuiElement* el) {
    if (hover_info.top_bars.handler) return;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    hover_info.top_bars.handler = el->custom_data;
}

static void close_button_on_hover(GuiElement* el) {
    if (hover_info.top_bars.handler) return;
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->data.rect_type = RECT_NORMAL;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    hover_info.top_bars.handler = handle_window_gui_close_button_click;
}

static void window_on_hover(GuiElement* el) {
    (void) el;
    if (!hover_info.dropdown.location) hover_info.top_bars.handler = NULL;
}

static void begin_window(const char* title, int w, int h, float scaling) {
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
        gui_set_position(gui, gui->win_w / 2, gui->win_h / 2);
        gui_set_anchor(gui, ALIGN_CENTER, ALIGN_CENTER);
        gui_set_fixed(gui, w, h);
        if (w == 0) gui_set_fit(gui, DIRECTION_HORIZONTAL);
        if (h == 0) gui_set_fit(gui, DIRECTION_VERTICAL);
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
            gui_set_direction(gui, DIRECTION_VERTICAL);
            gui_set_padding(gui, conf.font_size * 0.5, conf.font_size * 0.5);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_gap(gui, WINDOW_ELEMENT_PADDING);
}

static void end_window(void) {
        gui_element_end(gui);

        GuiElement* el = gui_get_element(gui);

        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
            gui_set_position(gui, 0, 0);
            gui_set_fixed(gui, el->w, el->h);
            gui_set_shader(gui, &line_shader);
        gui_element_end(gui);

        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, el->w - conf.font_size * 1.2, 0);
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
    gui_element_end(gui);
}

static void warning_on_hover(GuiElement* el) {
    if (hover_info.top_bars.handler) return;
    (void) el;
    settings_tooltip = true;
}

static void begin_setting(const char* name, bool warning) {
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

}

static void slider_on_hover(GuiElement* el) {
    if (hover_info.top_bars.handler) return;
    unsigned short len;
    hover_info.hover_slider = *(SliderHoverInfo*)gui_get_state(el, &len);
    el->color = hover_info.hover_slider.value == hover_info.dragged_slider.value ? (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff } : (GuiColor) { 0x40, 0x40, 0x40, 0xff };
}

static void slider_button_on_hover(GuiElement* el) {
    if (hover_info.top_bars.handler) return;
    el->draw_type = DRAWTYPE_RECT;
    el->color = (GuiColor) { 0x60, 0x60, 0x60, 0xff };
    el->data.rect_type = RECT_NORMAL;
    hover_info.top_bars.handler = el->custom_data;
}

static void draw_slider(int min, int max, int* value) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
        gui_on_hover(gui, slider_on_hover);

        SliderHoverInfo info = (SliderHoverInfo) {
            .min = min,
            .max = max,
            .value = value,
        };
        SliderHoverInfo* state = gui_set_state(gui, &info, sizeof(info));

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_CENTER);
            gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
            gui_set_shader(gui, &line_shader);

            snprintf(state->value_str, 16, "%d", *state->value);

            gui_element_begin(gui);
                gui_on_hover(gui, slider_button_on_hover);
                gui_set_custom_data(gui, handle_left_slider_button_click);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_CENTER);

                gui_image(gui, &arrow_left_tex, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_element_end(gui);

            gui_grow(gui, DIRECTION_HORIZONTAL);

            gui_text(gui, &font_cond, state->value_str, conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });

            gui_grow(gui, DIRECTION_HORIZONTAL);

            gui_element_begin(gui);
                gui_on_hover(gui, slider_button_on_hover);
                gui_set_custom_data(gui, handle_right_slider_button_click);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_CENTER);

                gui_image(gui, &arrow_right_tex, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_element_end(gui);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void end_setting(void) {
    gui_element_end(gui);
}

static void text_input_on_hover(GuiElement* el) {
    if (hover_info.top_bars.handler) return;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
}

static void dropdown_input_on_hover(GuiElement* el) {
    if (hover_info.top_bars.handler) return;
    unsigned short len;
    hover_info.settings_dropdown_data = *(DropdownData*)gui_get_state(el, &len);
    hover_info.top_bars.handler = handle_settings_dropdown_click;
    if (el->color.r == 0x30) el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
}

static void draw_dropdown_input(int* value, char** list, int list_len) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
        gui_on_hover(gui, dropdown_input_on_hover);

        DropdownData data = (DropdownData) {
            .value = value,
            .list = list,
            .list_len = list_len,
        };
        gui_set_state(gui, &data, sizeof(data));

        if (hover_info.select_settings_dropdown_value == value && hover_info.dropdown.location == LOCATION_SETTINGS) {
            hover_info.dropdown.element = gui_get_element(gui);
            gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
        }

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_CENTER);
            gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
            gui_set_shader(gui, &line_shader);
            gui_set_padding(gui, WINDOW_ELEMENT_PADDING, 0);
            gui_set_scissor(gui);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            gui_text(gui, &font_cond, gettext(list[*value]), conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_grow(gui, DIRECTION_HORIZONTAL);
            gui_image(gui, &drop_tex, BLOCK_IMAGE_SIZE, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        gui_element_end(gui);
    gui_element_end(gui);
}

static void draw_text_input(char** input, const char* hint, int* scroll) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
        gui_on_hover(gui, text_input_on_hover);
        gui_set_custom_data(gui, input);

        if (input == hover_info.select_input) gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_CENTER);
            gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
            gui_set_shader(gui, &line_shader);
            gui_set_padding(gui, WINDOW_ELEMENT_PADDING, 0);
            gui_set_scroll(gui, scroll);
            gui_set_scissor(gui);

            gui_element_begin(gui);
                draw_input(&font_cond, input, hint, conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_element_end(gui);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void draw_button(const char* label, ButtonClickHandler handler) {
    gui_element_begin(gui);
        gui_set_min_size(gui, 0, conf.font_size);
        gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
        gui_on_hover(gui, settings_button_on_hover);
        gui_set_custom_data(gui, handler);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_padding(gui, WINDOW_ELEMENT_PADDING, 0);
            gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_CENTER);
            gui_set_shader(gui, &line_shader);

            gui_text(gui, &font_cond, label, conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        gui_element_end(gui);
    gui_element_end(gui);
}

void handle_window(void) {
    if (window.is_hiding) {
        window.shown = false;
        window.is_hiding = false;
    }
    if (window.is_fading) {
        window.animation_time -= GetFrameTime() * 2.0;
        if (window.animation_time < 0.0) {
            window.animation_time = 0.0;
            if (window.shown) render_surface_needs_redraw = true;
            window.shown = false;
        } else {
            render_surface_needs_redraw = true;
        }
    } else {
        window.shown = true;
        window.animation_time += GetFrameTime() * 2.0;
        if (window.animation_time > 1.0) {
            window.animation_time = 1.0;
        } else {
            render_surface_needs_redraw = true;
        }
    }
}

void draw_window(void) {
    if (!window.shown) return;

    float animation_ease = ease_out_expo(window.animation_time);

    static int font_path_scroll = 0;
    static int font_bold_path_scroll = 0;
    static int font_mono_path_scroll = 0;
    static int executable_name_scroll = 0;
    static int linker_name_scroll = 0;

    switch (window.type) {
    case GUI_TYPE_SETTINGS:
        begin_window(gettext("Settings"), MIN(600, gui->win_w - conf.font_size), 0, animation_ease);
            begin_setting(gettext("Language"), true);
                draw_dropdown_input((int*)&window_conf.language, language_list, ARRLEN(language_list));
            end_setting();

            begin_setting(gettext("UI size"), true);
                draw_slider(8, 64, &window_conf.font_size);
            end_setting();

            begin_setting(gettext("FPS limit"), false);
                draw_slider(0, 240, &window_conf.fps_limit);
            end_setting();

            begin_setting(gettext("Font path"), true);
                draw_text_input(&window_conf.font_path, gettext("path"), &font_path_scroll);
            end_setting();

            begin_setting(gettext("Bold font path"), true);
                draw_text_input(&window_conf.font_bold_path, gettext("path"), &font_bold_path_scroll);
            end_setting();

            begin_setting(gettext("Monospaced font path"), true);
                draw_text_input(&window_conf.font_mono_path, gettext("path"), &font_mono_path_scroll);
            end_setting();

            begin_setting(gettext("Panel editor"), false);
                gui_element_begin(gui);
                    gui_set_grow(gui, DIRECTION_HORIZONTAL);
                    gui_set_grow(gui, DIRECTION_VERTICAL);
                    gui_set_direction(gui, DIRECTION_HORIZONTAL);

                    draw_button(gettext("Open"), handle_settings_panel_editor_button_click);
                gui_element_end(gui);
            end_setting();

            gui_grow(gui, DIRECTION_VERTICAL);

            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_gap(gui, WINDOW_ELEMENT_PADDING);

                gui_grow(gui, DIRECTION_HORIZONTAL);
                draw_button(gettext("Reset panels"), handle_settings_reset_panels_button_click);
                draw_button(gettext("Reset"), handle_settings_reset_button_click);
                draw_button(gettext("Apply"), handle_settings_apply_button_click);
            gui_element_end(gui);
        end_window();
        break;
    case GUI_TYPE_PROJECT_SETTINGS:
        begin_window(gettext("Build settings"), MIN(600, gui->win_w - conf.font_size), 0, animation_ease);
            begin_setting(gettext("Executable name"), false);
                draw_text_input(&project_conf.executable_name, gettext("name"), &executable_name_scroll);
            end_setting();

            begin_setting(gettext("Linker name (Linux only)"), false);
                draw_text_input(&project_conf.linker_name, gettext("name"), &linker_name_scroll);
            end_setting();

            gui_grow(gui, DIRECTION_VERTICAL);

            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_gap(gui, WINDOW_ELEMENT_PADDING);

                gui_grow(gui, DIRECTION_HORIZONTAL);

                draw_button(gettext("Build!"), handle_project_settings_build_button_click);
            gui_element_end(gui);
        end_window();
        break;
    case GUI_TYPE_ABOUT:
        begin_window(gettext("About"), 500 * conf.font_size / 32.0, 0, animation_ease);
            gui_element_begin(gui);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_CENTER);
                gui_set_gap(gui, WINDOW_ELEMENT_PADDING);

                gui_image(gui, &logo_tex, conf.font_size, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_text(gui, &font_eb, "Scrap v" SCRAP_VERSION, conf.font_size * 0.8, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_element_end(gui);

            gui_element_begin(gui);
                gui_text(gui, &font_cond, gettext("Scrap is a project that allows anyone to build"), conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
                gui_text(gui, &font_cond, gettext("software using simple, block based interface."), conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
            gui_element_end(gui);

            gui_grow(gui, DIRECTION_VERTICAL);

            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_gap(gui, WINDOW_ELEMENT_PADDING);

                gui_grow(gui, DIRECTION_HORIZONTAL);
                draw_button(gettext("License"), handle_about_license_button_click);
            gui_element_end(gui);
        end_window();
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

            gui_text(gui, &font_cond, gettext("Needs restart for changes to take effect"), conf.font_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0xff });
        gui_element_end(gui);
    }

    settings_tooltip = false;
}
