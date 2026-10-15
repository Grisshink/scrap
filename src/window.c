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
#include "../external/tinyfiledialogs.h"

#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <libintl.h>
#include <string.h>

typedef struct {
    bool shown;
    float animation_time;
    float animation_ease;
    bool is_fading;
    bool is_hiding;
    WindowGuiRenderFunc render;
} WindowGui;

Config window_config;
static WindowGui window = {0};
static bool settings_tooltip = false;
static bool settings_applied = false;
static char** about_text_split = NULL;

static void button_draw(const char* label, Texture2D* icon, ButtonClickHandler handler, void* data);

// https://easings.net/#easeOutExpo
float ease_out_expo(float x) {
    return x == 1.0 ? 1.0 : 1 - powf(2.0, -10.0 * x);
}

static bool about_license_button_handle_click(void) {
    OpenURL(LICENSE_URL);
    return true;
}

static bool window_close_button_handle_click(void) {
    gui_window_hide();
    return true;
}

static void vector_append(char** vec, const char* str) {
    if (vector_size(*vec) > 0 && (*vec)[vector_size(*vec) - 1] == 0) vector_pop(*vec);
    for (size_t i = 0; str[i]; i++) vector_add(vec, str[i]);
    vector_add(vec, 0);
}

static bool settings_browse_button_handle_click(void) {
    char const* filters[] = { "*.ttf", "*.otf" };
    char** path_input = ui.hover.button.data;
    char* path = tinyfd_openFileDialog(NULL, *path_input, ARRLEN(filters), filters, "Font files", 0);
    if (!path) return true;

    vector_clear(*path_input);
    vector_append(path_input, path);

    ui.hover.select_input_cursor = 0;
    ui.hover.select_input_mark = -1;
    ui.render_surface_needs_redraw = true;

    return true;
}

static bool settings_left_slider_button_handle_click(void) {
    settings_applied = false;
    *ui.hover.hover_slider.value = MAX(*ui.hover.hover_slider.value - 1, ui.hover.hover_slider.min);
    return true;
}

static bool settings_right_slider_button_handle_click(void) {
    settings_applied = false;
    *ui.hover.hover_slider.value = MIN(*ui.hover.hover_slider.value + 1, ui.hover.hover_slider.max);
    return true;
}

static bool settings_dropdown_handle_click(void) {
    settings_applied = false;
    *(int*)ui.dropdown.ref_object = ui.dropdown.as.list.select_ind;
    return dropdown_handle_close();
}

static bool settings_dropdown_button_handle_click(void) {
    dropdown_show_list(ui.hover.settings_dropdown_data.list, ui.hover.settings_dropdown_data.list_len, ui.hover.settings_dropdown_data.value, settings_dropdown_handle_click);
    return true;
}

static bool settings_panel_editor_button_handle_click(void) {
    gui_window_hide();
    ui.hover.is_panel_edit_mode = true;
    ui.hover.select_input = NULL;
    ui.hover.editor.select_argument = NULL;
    ui.hover.editor.select_block = NULL;
    ui.hover.editor.select_blockchain = NULL;
    return true;
}

static bool settings_reset_button_handle_click(void) {
    config_set_default(&window_config);
    settings_applied = false;
    return true;
}

static bool settings_reset_panels_button_handle_click(void) {
    tabs_delete_all();
    panels_init();
    editor.current_tab = 0;
    settings_applied = false;
    return true;
}

static bool settings_apply_button_handle_click(void) {
    config_apply(&config, &window_config);
    config_save(&window_config);
    settings_applied = true;
    return true;
}

static bool settings_toggle_button_handle_click(void) {
    bool* toggle_value = ui.hover.button.data;
    *toggle_value = !*toggle_value;
    return true;
}

static bool save_confirmation_yes_button_handle_click(void) {
    if (project_save(NULL)) {
        editor.project_modified = false;
        ui.scrap_running = false;
    } else {
        gui_window_hide();
    }
    return true;
}

static bool save_confirmation_no_button_handle_click(void) {
    ui.scrap_running = false;
    return true;
}

static bool save_confirmation_cancel_button_handle_click(void) {
    gui_window_hide();
    return true;
}

void gui_window_init(void) {
    window.is_fading = true;
}

bool gui_window_is_shown(void) {
    return window.shown;
}

WindowGuiRenderFunc gui_window_get_render_func(void) {
    return window.render;
}

void gui_window_show(WindowGuiRenderFunc func) {
    config_free(&window_config); // Drop old strings and replace with new
    config_copy(&window_config, &config);
    window.is_fading = false;
    window.render = func;
    ui.shader_time = -0.2;
    settings_applied = false;
}

void gui_window_hide(void) {
    ui.hover.select_input = NULL;
    window.is_fading = true;
}

void gui_window_hide_immediate(void) {
    gui_window_hide();
    window.is_hiding = true;
}

static void settings_button_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    ui.hover.button = *(ButtonHoverInfo*)gui_get_state(el);
}

static void close_button_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    if (el->draw_type == DRAWTYPE_RECT) return;
    el->draw_type = DRAWTYPE_RECT;
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    ui.hover.button.handler = window_close_button_handle_click;
}

static void window_on_hover(GuiElement* el) {
    (void) el;
    if (!ui.dropdown.shown) ui.hover.button.handler = NULL;
}

static void window_begin(const char* title, Texture2D* icon, int w, int h, float scaling) {
    ui.hover.button.handler = window_close_button_handle_click;
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
            gui_set_min_size(gui, 0, config.ui_size * 1.2);
            gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
            gui_set_gap(gui, ELEMENT_GAP/2);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            if (icon) gui_image(gui, icon, config.ui_size, GUI_WHITE);
            gui_text(gui, &assets.fonts.font_eb, title, config.ui_size * 0.8, GUI_WHITE);
            gui_grow(gui, DIRECTION_HORIZONTAL);
        gui_element_end(gui);

        gui_element_begin(gui);
            gui_set_direction(gui, DIRECTION_VERTICAL);
            gui_set_padding(gui, config.ui_size * 0.5, config.ui_size * 0.5);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_gap(gui, ELEMENT_GAP);
}

static void window_end(void) {
        gui_element_end(gui);

        GuiElement* el = gui_get_element(gui);

        gui_element_begin(gui);
            gui_set_floating(gui);
            if (IsShaderValid(assets.line_shader)) {
                gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
                gui_set_shader(gui, &assets.line_shader);
            }
            gui_set_position(gui, 0, 0);
            gui_set_fixed(gui, el->w, el->h);
        gui_element_end(gui);

        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_position(gui, el->w - config.ui_size * 1.2, 0);
            gui_set_fixed(gui, config.ui_size * 1.2, config.ui_size * 1.2);
            gui_set_align(gui, ALIGN_CENTER, ALIGN_CENTER);
            gui_on_hover(gui, close_button_on_hover);

            gui_image(gui, &assets.textures.button_close, config.ui_size * 0.8, GUI_WHITE);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void warning_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    (void) el;
    settings_tooltip = true;
}

static void setting_begin(const char* name, bool warning) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_gap(gui, ELEMENT_GAP);
        gui_set_min_size(gui, 0, config.ui_size);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_RIGHT, ALIGN_CENTER);

            gui_text(gui, &assets.fonts.font_cond, name, config.ui_size * 0.6, GUI_WHITE);
        gui_element_end(gui);

        if (warning) {
            gui_element_begin(gui);
                gui_set_image(gui, &assets.textures.icon_warning, config.ui_size, GUI_WHITE);
                gui_on_hover(gui, warning_on_hover);
            gui_element_end(gui);
        } else {
            gui_spacer(gui, config.ui_size, config.ui_size);
        }

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_gap(gui, ELEMENT_GAP);
            gui_set_min_size(gui, 0, config.ui_size);
}

static void slider_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    ui.hover.hover_slider = *(SliderHoverInfo*)gui_get_state(el);
    if (ui.hover.hover_slider.value == ui.hover.dragged_slider.value) {
        el->color = (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff };
        settings_applied = false;
    } else {
        el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    }
}

static void slider_button_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    el->draw_type = DRAWTYPE_RECT;
    el->color = (GuiColor) { 0x60, 0x60, 0x60, 0xff };
    el->draw_subtype = GUI_SUBTYPE_DEFAULT;
    ui.hover.button.handler = el->custom_data;
}

static void slider_draw(int min, int max, int* value) {
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
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
            if (IsShaderValid(assets.line_shader)) {
                gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
                gui_set_shader(gui, &assets.line_shader);
            }

            snprintf(state->value_str, 16, "%d", *state->value);

            gui_element_begin(gui);
                gui_on_hover(gui, slider_button_on_hover);
                gui_set_custom_data(gui, settings_left_slider_button_handle_click);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);

                gui_image(gui, &assets.textures.button_arrow_left, BLOCK_IMAGE_SIZE, GUI_WHITE);
            gui_element_end(gui);

            gui_grow(gui, DIRECTION_HORIZONTAL);

            gui_text(gui, &assets.fonts.font_cond, state->value_str, config.ui_size * 0.6, GUI_WHITE);

            gui_grow(gui, DIRECTION_HORIZONTAL);

            gui_element_begin(gui);
                gui_on_hover(gui, slider_button_on_hover);
                gui_set_custom_data(gui, settings_right_slider_button_handle_click);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);

                gui_image(gui, &assets.textures.button_arrow_right, BLOCK_IMAGE_SIZE, GUI_WHITE);
            gui_element_end(gui);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void setting_end(void) {
    gui_element_end(gui);
    gui_element_end(gui);
}

static void text_input_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
}

static void dropdown_input_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    ui.hover.settings_dropdown_data = *(DropdownData*)gui_get_state(el);
    ui.hover.button.handler = settings_dropdown_button_handle_click;
    if (el->color.r == 0x30) el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
}

static void dropdown_input_draw(int* value, char** list, int list_len) {
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

        if ((int*)ui.dropdown.ref_object == value) {
            ui.dropdown.element = gui_get_element(gui);
            gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });
        }

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
            if (IsShaderValid(assets.line_shader)) {
                gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
                gui_set_shader(gui, &assets.line_shader);
            }
            gui_set_padding(gui, ELEMENT_GAP, 0);
            gui_set_scissor(gui);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            gui_text(gui, &assets.fonts.font_cond, sgettext(list[*value]), config.ui_size * 0.6, GUI_WHITE);
            gui_grow(gui, DIRECTION_HORIZONTAL);
            gui_image(gui, &assets.textures.dropdown, BLOCK_IMAGE_SIZE, GUI_WHITE);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void text_input_draw(char** input, const char* hint, int* scroll, bool editable, bool path_input) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_grow(gui, DIRECTION_VERTICAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
        gui_on_hover(gui, text_input_on_hover);
        gui_set_custom_data(gui, input);

        if (input == ui.hover.select_input) gui_set_rect(gui, (GuiColor) { 0x2b, 0x2b, 0x2b, 0xff });

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
            if (IsShaderValid(assets.line_shader)) {
                gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
                gui_set_shader(gui, &assets.line_shader);
            }
            gui_set_padding(gui, ELEMENT_GAP, 0);
            gui_set_scroll(gui, scroll);
            gui_set_scissor(gui);

            if (editable) {
                InputHoverInfo info = (InputHoverInfo) {
                    .input = input,
                    .rel_pos = (Vector2) { ELEMENT_GAP + *scroll, 0 },
                    .font = &assets.fonts.font_cond,
                    .font_size = config.ui_size * 0.6,
                };
                gui_set_state(gui, &info, sizeof(info));
                gui_on_hover(gui, input_on_hover);
            }

            input_text_draw(&assets.fonts.font_cond, input, hint, config.ui_size * 0.6, GUI_WHITE);
        gui_element_end(gui);
    gui_element_end(gui);

    if (path_input) button_draw(NULL, &assets.textures.icon_folder, settings_browse_button_handle_click, input);
}

static void button_draw(const char* label, Texture2D* icon, ButtonClickHandler handler, void* data) {
    gui_element_begin(gui);
        gui_set_min_size(gui, 0, config.ui_size);
        gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
        gui_on_hover(gui, settings_button_on_hover);
        ButtonHoverInfo info = (ButtonHoverInfo) {
            .handler = handler,
            .data = data,
        };
        gui_set_state(gui, &info, sizeof(info));

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_padding(gui, ELEMENT_GAP, 0);
            gui_set_gap(gui, ELEMENT_GAP/2);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
            if (IsShaderValid(assets.line_shader)) {
                gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
                gui_set_shader(gui, &assets.line_shader);
            }

            if (icon) gui_image(gui, icon, BLOCK_IMAGE_SIZE, GUI_WHITE);
            if (label) gui_text(gui, &assets.fonts.font_cond, label, config.ui_size * 0.6, GUI_WHITE);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void toggle_on_hover(GuiElement* el) {
    el->color = (GuiColor) { 0x40, 0x40, 0x40, 0xff };
    ui.hover.button.handler = settings_toggle_button_handle_click;
    ui.hover.button.data = el->custom_data;
}

static void toggle_draw(bool* value) {
    gui_element_begin(gui);
        gui_set_rect(gui, (GuiColor) { 0x30, 0x30, 0x30, 0xff });
        gui_on_hover(gui, toggle_on_hover);
        gui_set_custom_data(gui, value);

        gui_element_begin(gui);
            gui_set_fixed(gui, config.ui_size * 2, config.ui_size);
            if (IsShaderValid(assets.line_shader)) {
                gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
                gui_set_shader(gui, &assets.line_shader);
            }
            gui_set_direction(gui, DIRECTION_HORIZONTAL);

            if (*value) gui_spacer(gui, config.ui_size, 0);

            gui_element_begin(gui);
                gui_set_padding(gui, config.ui_size * 0.125, config.ui_size * 0.125);

                gui_element_begin(gui);
                    gui_set_fixed(gui, config.ui_size * 0.75, config.ui_size * 0.75);
                    gui_set_rect(gui, *value ? (GuiColor) { 0x30, 0xdd, 0x30, 0xff } : (GuiColor) { 0xdd, 0x30, 0x30, 0xff });
                gui_element_end(gui);
            gui_element_end(gui);
        gui_element_end(gui);
    gui_element_end(gui);
}

void window_handle(void) {
    if (window.is_hiding) {
        window.shown = false;
        window.is_hiding = false;
    }
    if (window.is_fading) {
        window.animation_time -= GetFrameTime() * 2.0;
        if (window.animation_time < 0.0) {
            window.animation_time = 0.0;
            if (window.shown) ui.render_surface_needs_redraw = true;
            window.shown = false;
            if (about_text_split) {
                for (size_t i = 0; i < vector_size(about_text_split); i++) {
                    vector_free(about_text_split[i]);
                }
                vector_free(about_text_split);
                about_text_split = NULL;
            }
        } else {
            ui.render_surface_needs_redraw = true;
        }
    } else {
        window.shown = true;
        window.animation_time += GetFrameTime() * 2.0;
        if (window.animation_time > 1.0) {
            window.animation_time = 1.0;
        } else {
            ui.render_surface_needs_redraw = true;
        }
    }
}

void settings_window_draw(void) {
    static int font_path_scroll = 0;
    static int font_bold_path_scroll = 0;
    static int font_mono_path_scroll = 0;

    window_begin(gettext("Settings"), &assets.textures.icon_settings, MIN(700 * config.ui_size / 32.0, gui->win_w - config.ui_size), 0, window.animation_ease);
        setting_begin(gettext("Language"), true);
            dropdown_input_draw((int*)&window_config.language, language_list, ARRLEN(language_list));
        setting_end();

        setting_begin(gettext("UI size"), false);
            slider_draw(8, 64, &window_config.ui_size);
        setting_end();

        setting_begin(gettext("FPS limit"), false);
            slider_draw(0, 240, &window_config.fps_limit);
        setting_end();

        setting_begin(gettext("Code panel smoothness"), false);
            slider_draw(0, 32, &window_config.code_area_smoothness);
        setting_end();

        setting_begin(gettext("Categories column count"), false);
            slider_draw(1, 8, &window_config.categories_column_count);
        setting_end();

        setting_begin(gettext("Font path"), false);
            text_input_draw(&window_config.font_path, gettext("path"), &font_path_scroll, true, true);
        setting_end();

        setting_begin(gettext("Bold font path"), false);
            text_input_draw(&window_config.font_bold_path, gettext("path"), &font_bold_path_scroll, true, true);
        setting_end();

        setting_begin(gettext("Monospaced font path"), false);
            text_input_draw(&window_config.font_mono_path, gettext("path"), &font_mono_path_scroll, true, true);
        setting_end();

        setting_begin(gettext("Panel layout"), false);
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);

                button_draw(gettext("Edit"), &assets.textures.button_edit, settings_panel_editor_button_handle_click, NULL);
            gui_element_end(gui);
        setting_end();

        setting_begin(gettext("Show block previews"), false);
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);

                toggle_draw(&window_config.show_blockchain_previews);
            gui_element_end(gui);
        setting_end();

        setting_begin(gettext("Show debug info"), false);
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);

                toggle_draw(&editor.show_debug);
            gui_element_end(gui);
        setting_end();

        gui_grow(gui, DIRECTION_VERTICAL);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_min_size(gui, 0, config.ui_size * 0.6);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            if (settings_applied) gui_text(gui, &assets.fonts.font_cond, gettext("Settings applied"), config.ui_size * 0.6, GUI_WHITE);
        gui_element_end(gui);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_gap(gui, ELEMENT_GAP);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            button_draw(gettext("Reset panels"), NULL, settings_reset_panels_button_handle_click, NULL);
            button_draw(gettext("Reset"), NULL, settings_reset_button_handle_click, NULL);
            button_draw(gettext("Apply"), NULL, settings_apply_button_handle_click, NULL);
        gui_element_end(gui);
    window_end();

    if (settings_tooltip) {
        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x80 });
            gui_set_position(gui, gui->mouse_x + 10, gui->mouse_y + 10);
            gui_set_padding(gui, ELEMENT_GAP * 0.5, ELEMENT_GAP * 0.5);

            gui_text(gui, &assets.fonts.font_cond, gettext("Needs restart for changes to take effect"), config.ui_size * 0.6, GUI_WHITE);
        gui_element_end(gui);
    }

    settings_tooltip = false;
}

void about_window_draw(void) {
    if (!about_text_split) {
        scrap_log(LOG_INFO, "Split about text");
        about_text_split = vector_create();
        const char* about_text = gettext("Scrap is a project that allows anyone to build\n"
                                         "software using simple, block based interface.");
        size_t about_text_len = strlen(about_text);

        char* current_text = vector_create();
        for (size_t i = 0; i < about_text_len; i++) {
            if (about_text[i] == '\n') {
                vector_add(&about_text_split, current_text);
                current_text = vector_create();
                continue;
            }
            vector_add(&current_text, about_text[i]);
        }
        vector_add(&about_text_split, current_text);
    }

    window_begin(gettext("About"), &assets.textures.icon_about, 500 * config.ui_size / 32.0, 0, window.animation_ease);
        gui_element_begin(gui);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
            gui_set_gap(gui, ELEMENT_GAP);

            gui_image(gui, &assets.textures.icon_logo, config.ui_size, GUI_WHITE);
            gui_text(gui, &assets.fonts.font_eb, "Scrap " SCRAP_VERSION, config.ui_size * 0.8, GUI_WHITE);
        gui_element_end(gui);

        gui_element_begin(gui);
            if (about_text_split) {
                for (size_t i = 0; i < vector_size(about_text_split); i++) {
                    gui_text_slice(gui, &assets.fonts.font_cond, about_text_split[i], vector_size(about_text_split[i]), config.ui_size * 0.6, GUI_WHITE);
                }
            } else {
                gui_text(gui, &assets.fonts.font_cond, "ERROR", config.ui_size * 0.6, (GuiColor) { 0xff, 0x20, 0x20, 0xff });
            }
            gui_spacer(gui, 0, config.ui_size * 0.3);
            gui_text(gui, &assets.fonts.font_cond, "Raylib v" RAYLIB_VERSION, config.ui_size * 0.6, (GuiColor) { 0xff, 0xff, 0xff, 0x80 });
        gui_element_end(gui);

        gui_grow(gui, DIRECTION_VERTICAL);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_gap(gui, ELEMENT_GAP);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            button_draw(gettext("License"), &assets.textures.icon_file, about_license_button_handle_click, NULL);
        gui_element_end(gui);
    window_end();
}

void save_confirmation_window_draw(void) {
    window_begin(gettext("Confirm save"), &assets.textures.icon_about, 500 * config.ui_size / 32.0, 0, window.animation_ease);
        gui_text(gui, &assets.fonts.font_cond, gettext("Project is modified. Save the changes before quitting?"), config.ui_size * 0.6, GUI_WHITE);

        gui_grow(gui, DIRECTION_VERTICAL);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_gap(gui, ELEMENT_GAP);

            gui_grow(gui, DIRECTION_HORIZONTAL);

            button_draw(gettext("Yes"),    NULL, save_confirmation_yes_button_handle_click, NULL);
            button_draw(gettext("No"),     NULL, save_confirmation_no_button_handle_click, NULL);
            button_draw(gettext("Cancel"), NULL, save_confirmation_cancel_button_handle_click, NULL);
        gui_element_end(gui);
    window_end();
}

void window_draw(void) {
    if (!window.shown) return;

    window.animation_ease = ease_out_expo(window.animation_time);
    window.render();
}
