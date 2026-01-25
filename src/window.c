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

#include "raylib.h"
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

static void draw_button(const char* label, ButtonClickHandler handler, void* data);

// https://easings.net/#easeOutExpo
float ease_out_expo(float x) {
    return x == 1.0 ? 1.0 : 1 - powf(2.0, -10.0 * x);
}

static bool about_on_license_button_click(void) {
    OpenURL(LICENSE_URL);
    return true;
}

static bool window_on_close_button_click(void) {
    gui_window_hide();
    return true;
}

static void vector_append(char** vec, const char* str) {
    if (vector_size(*vec) > 0 && (*vec)[vector_size(*vec) - 1] == 0) vector_pop(*vec);
    for (size_t i = 0; str[i]; i++) vector_add(vec, str[i]);
    vector_add(vec, 0);
}

static bool settings_on_browse_button_click(void) {
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

static bool settings_on_left_slider_button_click(void) {
    settings_applied = false;
    *ui.hover.hover_slider.value = MAX(*ui.hover.hover_slider.value - 1, ui.hover.hover_slider.min);
    return true;
}

static bool settings_on_right_slider_button_click(void) {
    settings_applied = false;
    *ui.hover.hover_slider.value = MIN(*ui.hover.hover_slider.value + 1, ui.hover.hover_slider.max);
    return true;
}

static bool settings_on_dropdown_button_click(void) {
    settings_applied = false;
    *(int*)ui.dropdown.ref_object = ui.dropdown.as.list.select_ind;
    return handle_dropdown_close();
}

static bool settings_on_dropdown_click(void) {
    show_list_dropdown(ui.hover.settings_dropdown_data.list, ui.hover.settings_dropdown_data.list_len, ui.hover.settings_dropdown_data.value, settings_on_dropdown_button_click);
    return true;
}

static bool settings_on_panel_editor_button_click(void) {
    gui_window_hide();
    ui.hover.is_panel_edit_mode = true;
    ui.hover.select_input = NULL;
    ui.hover.editor.select_argument = NULL;
    ui.hover.editor.select_block = NULL;
    ui.hover.editor.select_blockchain = NULL;
    return true;
}

static bool settings_on_reset_button_click(void) {
    set_default_config(&window_config);
    settings_applied = false;
    return true;
}

static bool settings_on_reset_panels_button_click(void) {
    delete_all_tabs();
    init_panels();
    editor.current_tab = 0;
    settings_applied = false;
    return true;
}

static bool settings_on_apply_button_click(void) {
    apply_config(&config, &window_config);
    save_config(&window_config);
    settings_applied = true;
    return true;
}

static bool project_settings_on_build_button_click(void) {
#ifdef USE_INTERPRETER
    vm_start();
#else
    vm_start(COMPILER_MODE_BUILD);
#endif
    gui_window_hide();
    return true;
}

static bool save_confirmation_on_yes_button_click(void) {
    if (save_project()) {
        ui.scrap_running = false;
    } else {
        gui_window_hide();
    }
    return true;
}

static bool save_confirmation_on_no_button_click(void) {
    ui.scrap_running = false;
    return true;
}

static bool save_confirmation_on_cancel_button_click(void) {
    gui_window_hide();
    return true;
}

void init_gui_window(void) {
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
    ui.hover.button.handler = window_on_close_button_click;
}

static void window_on_hover(GuiElement* el) {
    (void) el;
    if (!ui.dropdown.shown) ui.hover.button.handler = NULL;
}

static void begin_window(const char* title, int w, int h, float scaling) {
    ui.hover.button.handler = window_on_close_button_click;
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

            gui_grow(gui, DIRECTION_HORIZONTAL);
            gui_text(gui, &assets.fonts.font_eb, title, config.ui_size * 0.8, GUI_WHITE);
            gui_grow(gui, DIRECTION_HORIZONTAL);
        gui_element_end(gui);

        gui_element_begin(gui);
            gui_set_direction(gui, DIRECTION_VERTICAL);
            gui_set_padding(gui, config.ui_size * 0.5, config.ui_size * 0.5);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_grow(gui, DIRECTION_VERTICAL);
            gui_set_gap(gui, WINDOW_ELEMENT_PADDING);
}

static void end_window(void) {
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

            gui_text(gui, &assets.fonts.font_cond, "X", config.ui_size * 0.8, GUI_WHITE);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void warning_on_hover(GuiElement* el) {
    if (ui.hover.button.handler) return;
    (void) el;
    settings_tooltip = true;
}

static void begin_setting(const char* name, bool warning) {
    gui_element_begin(gui);
        gui_set_grow(gui, DIRECTION_HORIZONTAL);
        gui_set_direction(gui, DIRECTION_HORIZONTAL);
        gui_set_gap(gui, WINDOW_ELEMENT_PADDING);
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
            gui_set_gap(gui, WINDOW_ELEMENT_PADDING);
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
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
            if (IsShaderValid(assets.line_shader)) {
                gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
                gui_set_shader(gui, &assets.line_shader);
            }

            snprintf(state->value_str, 16, "%d", *state->value);

            gui_element_begin(gui);
                gui_on_hover(gui, slider_button_on_hover);
                gui_set_custom_data(gui, settings_on_left_slider_button_click);
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
                gui_set_custom_data(gui, settings_on_right_slider_button_click);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);
                gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);

                gui_image(gui, &assets.textures.button_arrow_right, BLOCK_IMAGE_SIZE, GUI_WHITE);
            gui_element_end(gui);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void end_setting(void) {
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
    ui.hover.button.handler = settings_on_dropdown_click;
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
            gui_set_padding(gui, WINDOW_ELEMENT_PADDING, 0);
            gui_set_scissor(gui);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            gui_text(gui, &assets.fonts.font_cond, sgettext(list[*value]), config.ui_size * 0.6, GUI_WHITE);
            gui_grow(gui, DIRECTION_HORIZONTAL);
            gui_image(gui, &assets.textures.dropdown, BLOCK_IMAGE_SIZE, GUI_WHITE);
        gui_element_end(gui);
    gui_element_end(gui);
}

static void draw_text_input(char** input, const char* hint, int* scroll, bool editable, bool path_input) {
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
            gui_set_padding(gui, WINDOW_ELEMENT_PADDING, 0);
            gui_set_scroll(gui, scroll);
            gui_set_scissor(gui);

            if (editable) {
                InputHoverInfo info = (InputHoverInfo) {
                    .input = input,
                    .rel_pos = (Vector2) { WINDOW_ELEMENT_PADDING + *scroll, 0 },
                    .font = &assets.fonts.font_cond,
                    .font_size = config.ui_size * 0.6,
                };
                gui_set_state(gui, &info, sizeof(info));
                gui_on_hover(gui, input_on_hover);
            }

            draw_input_text(&assets.fonts.font_cond, input, hint, config.ui_size * 0.6, GUI_WHITE);
        gui_element_end(gui);
    gui_element_end(gui);

    if (path_input) draw_button("Browse", settings_on_browse_button_click, input);
}

static void draw_button(const char* label, ButtonClickHandler handler, void* data) {
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
            gui_set_padding(gui, WINDOW_ELEMENT_PADDING, 0);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
            if (IsShaderValid(assets.line_shader)) {
                gui_set_border(gui, (GuiColor) { 0x60, 0x60, 0x60, 0xff }, 2);
                gui_set_shader(gui, &assets.line_shader);
            }

            gui_text(gui, &assets.fonts.font_cond, label, config.ui_size * 0.6, GUI_WHITE);
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

void draw_settings_window(void) {
    static int font_path_scroll = 0;
    static int font_bold_path_scroll = 0;
    static int font_mono_path_scroll = 0;

    begin_window(gettext("Settings"), MIN(600, gui->win_w - config.ui_size), 0, window.animation_ease);
        begin_setting(gettext("Language"), true);
            draw_dropdown_input((int*)&window_config.language, language_list, ARRLEN(language_list));
        end_setting();

        begin_setting(gettext("UI size"), true);
            draw_slider(8, 64, &window_config.ui_size);
        end_setting();

        begin_setting(gettext("FPS limit"), false);
            draw_slider(0, 240, &window_config.fps_limit);
        end_setting();

        begin_setting(gettext("Font path"), true);
            draw_text_input(&window_config.font_path, gettext("path"), &font_path_scroll, true, true);
        end_setting();

        begin_setting(gettext("Bold font path"), true);
            draw_text_input(&window_config.font_bold_path, gettext("path"), &font_bold_path_scroll, true, true);
        end_setting();

        begin_setting(gettext("Monospaced font path"), true);
            draw_text_input(&window_config.font_mono_path, gettext("path"), &font_mono_path_scroll, true, true);
        end_setting();

        begin_setting(gettext("Panel editor"), false);
            gui_element_begin(gui);
                gui_set_grow(gui, DIRECTION_HORIZONTAL);
                gui_set_grow(gui, DIRECTION_VERTICAL);
                gui_set_direction(gui, DIRECTION_HORIZONTAL);

                draw_button(gettext("Open"), settings_on_panel_editor_button_click, NULL);
            gui_element_end(gui);
        end_setting();

        gui_grow(gui, DIRECTION_VERTICAL);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_min_size(gui, 0, config.ui_size * 0.6);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            if (settings_applied) gui_text(gui, &assets.fonts.font_cond, "Settings applied", config.ui_size * 0.6, GUI_WHITE);
        gui_element_end(gui);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_gap(gui, WINDOW_ELEMENT_PADDING);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            draw_button(gettext("Reset panels"), settings_on_reset_panels_button_click, NULL);
            draw_button(gettext("Reset"), settings_on_reset_button_click, NULL);
            draw_button(gettext("Apply"), settings_on_apply_button_click, NULL);
        gui_element_end(gui);
    end_window();

    if (settings_tooltip) {
        gui_element_begin(gui);
            gui_set_floating(gui);
            gui_set_rect(gui, (GuiColor) { 0x00, 0x00, 0x00, 0x80 });
            gui_set_position(gui, gui->mouse_x + 10, gui->mouse_y + 10);
            gui_set_padding(gui, WINDOW_ELEMENT_PADDING * 0.5, WINDOW_ELEMENT_PADDING * 0.5);

            gui_text(gui, &assets.fonts.font_cond, gettext("Needs restart for changes to take effect"), config.ui_size * 0.6, GUI_WHITE);
        gui_element_end(gui);
    }

    settings_tooltip = false;
}

void draw_project_settings_window(void) {
    static int executable_name_scroll = 0;
    static int linker_name_scroll = 0;

    begin_window(gettext("Build settings"), MIN(600, gui->win_w - config.ui_size), 0, window.animation_ease);
        begin_setting(gettext("Executable name"), false);
            draw_text_input(&project_config.executable_name, gettext("name"), &executable_name_scroll, true, false);
        end_setting();

        begin_setting(gettext("Linker name (Linux only)"), false);
            draw_text_input(&project_config.linker_name, gettext("name"), &linker_name_scroll, true, false);
        end_setting();

        gui_grow(gui, DIRECTION_VERTICAL);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_gap(gui, WINDOW_ELEMENT_PADDING);

            gui_grow(gui, DIRECTION_HORIZONTAL);

            draw_button(gettext("Build!"), project_settings_on_build_button_click, NULL);
        gui_element_end(gui);
    end_window();
}

void draw_about_window(void) {
    if (!about_text_split) {
        TraceLog(LOG_INFO, "Split about text");
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

    begin_window(gettext("About"), 500 * config.ui_size / 32.0, 0, window.animation_ease);
        gui_element_begin(gui);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_align(gui, ALIGN_LEFT, ALIGN_CENTER);
            gui_set_gap(gui, WINDOW_ELEMENT_PADDING);

            gui_image(gui, &assets.textures.icon_logo, config.ui_size, GUI_WHITE);
            gui_text(gui, &assets.fonts.font_eb, "Scrap v" SCRAP_VERSION, config.ui_size * 0.8, GUI_WHITE);
        gui_element_end(gui);

        gui_element_begin(gui);
            if (about_text_split) {
                for (size_t i = 0; i < vector_size(about_text_split); i++) {
                    gui_text_slice(gui, &assets.fonts.font_cond, about_text_split[i], vector_size(about_text_split[i]), config.ui_size * 0.6, GUI_WHITE);
                }
            } else {
                gui_text(gui, &assets.fonts.font_cond, "ERROR", config.ui_size * 0.6, (GuiColor) { 0xff, 0x20, 0x20, 0xff });
            }
        gui_element_end(gui);

        gui_grow(gui, DIRECTION_VERTICAL);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_gap(gui, WINDOW_ELEMENT_PADDING);

            gui_grow(gui, DIRECTION_HORIZONTAL);
            draw_button(gettext("License"), about_on_license_button_click, NULL);
        gui_element_end(gui);
    end_window();
}

void draw_save_confirmation_window(void) {
    begin_window(gettext("Confirm save"), 500 * config.ui_size / 32.0, 0, window.animation_ease);
        gui_text(gui, &assets.fonts.font_cond, gettext("Project is modified. Save the changes and quit?"), config.ui_size * 0.6, GUI_WHITE);

        gui_grow(gui, DIRECTION_VERTICAL);

        gui_element_begin(gui);
            gui_set_grow(gui, DIRECTION_HORIZONTAL);
            gui_set_direction(gui, DIRECTION_HORIZONTAL);
            gui_set_gap(gui, WINDOW_ELEMENT_PADDING);

            gui_grow(gui, DIRECTION_HORIZONTAL);

            draw_button(gettext("Yes"),    save_confirmation_on_yes_button_click, NULL);
            draw_button(gettext("No"),     save_confirmation_on_no_button_click, NULL);
            draw_button(gettext("Cancel"), save_confirmation_on_cancel_button_click, NULL);
        gui_element_end(gui);
    end_window();
}

void draw_window(void) {
    if (!window.shown) return;

    window.animation_ease = ease_out_expo(window.animation_time);
    window.render();
}
