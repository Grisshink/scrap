#include "raylib.h"
#define RAYLIB_NUKLEAR_IMPLEMENTATION
#include "../external/raylib-nuklear.h"
#include "../external/tinyfiledialogs.h"
#include "scrap.h"

#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#define LICENSE_URL "https://www.gnu.org/licenses/gpl-3.0.html"

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

struct nk_user_font* font_eb_nuc = NULL;
struct nk_user_font* font_cond_nuc = NULL;
struct nk_image logo_tex_nuc;
struct nk_image warn_tex_nuc;

Config gui_conf;

typedef struct {
    bool shown;
    float animation_time;
    bool is_fading;
    bool is_hiding;
    Vector2 pos;
    NuklearGuiType type;
    struct nk_context *ctx;
} NuklearGui;

NuklearGui gui = {0};

void handle_gui(void);
void apply_styles(void);

// https://easings.net/#easeOutExpo
float ease_out_expo(float x) {
    return x == 1.0 ? 1.0 : 1 - powf(2.0, -10.0 * x);
}

void init_gui(void) {
    gui.is_fading = true;
    logo_tex_nuc = TextureToNuklear(logo_tex);
    warn_tex_nuc = TextureToNuklear(warn_tex);
    font_eb_nuc = LoadFontIntoNuklear(font_eb, conf.font_size);
    font_cond_nuc = LoadFontIntoNuklear(font_cond, conf.font_size * 0.6);
    gui.ctx = InitNuklearEx(font_cond_nuc, &line_shader);
    apply_styles();
}

bool gui_is_shown(void) {
    return gui.shown;
}

NuklearGuiType gui_get_type(void) {
    return gui.type;
}

void update_gui(void) {
    if (gui_is_shown()) UpdateNuklear(gui.ctx);
    handle_gui();
}

void draw_gui(void) {
    if (!gui.shown) return;

    float animation_ease = ease_out_expo(gui.animation_time);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 0x00, 0x00, 0x00, 0x44 * animation_ease });
    DrawNuklear(gui.ctx);
}

void gui_free(void) {
    UnloadNuklear(gui.ctx);
}

void apply_styles(void) {
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
    gui_conf = conf;
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
            nk_property_int(gui.ctx, "#", 0, &gui_conf.fps_limit, 240, 1, 1.0);
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
