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

#define SCRVM_IMPLEMENTATION
#include "term.h"
#include "scrap.h"
#include "vec.h"
#include "util.h"
#include "rlgl.h"

#include <math.h>
#include <libintl.h>
#include <locale.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

// Global Variables

Config config;
ProjectConfig project_config;

Assets assets;

Vm vm;
Gui* gui = NULL;

Editor editor;
UI ui;

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

const char* gradient_shader_vertex =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec2 vertexTexCoord;\n"
    "in vec4 vertexColor;\n"
    "out vec2 fragCoord;\n"
    "out vec4 fragColor;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "    vec4 pos = mvp * vec4(vertexPosition, 1.0);\n"
    "    fragCoord = vec2(vertexTexCoord.x, 1.0 - vertexTexCoord.y);\n"
    "    fragColor = vertexColor;\n"
    "    gl_Position = pos;\n"
    "}";

const char* gradient_shader_fragment =
    "#version 330\n"
    "in vec2 fragCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 left = mix(vec4(0.0, 0.0, 0.0, 1.0), vec4(1.0, 1.0, 1.0, 1.0), fragCoord.y);\n"
    "    vec4 right = mix(vec4(0.0, 0.0, 0.0, 1.0), fragColor, fragCoord.y);\n"
    "    finalColor = mix(left, right, fragCoord.x);\n"
    "}";

#define SHARED_DIR_BUF_LEN 512
#define LOCALE_DIR_BUF_LEN 768

const char* get_shared_dir_path(void) {
    static char out_path[SHARED_DIR_BUF_LEN] = {0};
    if (*out_path) return out_path;

#ifndef _WIN32
    snprintf(out_path, SHARED_DIR_BUF_LEN, "%sdata", GetApplicationDirectory());
    if (DirectoryExists(out_path)) {
        snprintf(out_path, SHARED_DIR_BUF_LEN, "%s", GetApplicationDirectory());
        goto end;
    }

    snprintf(out_path, SHARED_DIR_BUF_LEN, "%s../share/scrap/", GetApplicationDirectory());
    if (DirectoryExists(out_path)) goto end;

    snprintf(out_path, SHARED_DIR_BUF_LEN, "/usr/share/scrap/");
    if (DirectoryExists(out_path)) goto end;

    snprintf(out_path, SHARED_DIR_BUF_LEN, "/usr/local/share/scrap/");
    if (DirectoryExists(out_path)) goto end;
#endif

    snprintf(out_path, SHARED_DIR_BUF_LEN, "%s", GetApplicationDirectory());

end:
    scrap_log(LOG_INFO, "Using \"%s\" as shared directory path", out_path);
    return out_path;
}

const char* get_locale_path(void) {
    static char out_path[LOCALE_DIR_BUF_LEN] = {0};
    if (*out_path) return out_path;

#ifndef _WIN32
    snprintf(out_path, LOCALE_DIR_BUF_LEN, "%slocale", GetApplicationDirectory());
    if (DirectoryExists(out_path)) goto end;
#endif

    const char* shared_path = get_shared_dir_path();
    if (!strcmp(shared_path, GetApplicationDirectory())) {
        snprintf(out_path, LOCALE_DIR_BUF_LEN, "%slocale", shared_path);
    } else {
        snprintf(out_path, LOCALE_DIR_BUF_LEN, "%s../locale", shared_path);
    }

end:
    scrap_log(LOG_INFO, "Using \"%s\" as locale directory path", out_path);
    return out_path;
}

const char* into_shared_dir_path(const char* path) {
    return TextFormat("%s%s", get_shared_dir_path(), path);
}

// Returns the absolute path to the font, converting the relative path to a path inside the data directory
const char* get_font_path(char* font_path) {
    return font_path[0] != '/' && font_path[1] != ':' ? into_shared_dir_path(font_path) : font_path;
}

Image setup(void) {
    SetExitKey(KEY_NULL);

    ui.render_surface = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    SetTextureWrap(ui.render_surface.texture, TEXTURE_WRAP_MIRROR_REPEAT);

    assets.textures.dropdown = LoadTexture(into_shared_dir_path(DATA_PATH "drop.png"));
    SetTextureFilter(assets.textures.dropdown, TEXTURE_FILTER_BILINEAR);

    assets.textures.spectrum = LoadTexture(into_shared_dir_path(DATA_PATH "spectrum.png"));
    SetTextureFilter(assets.textures.spectrum, TEXTURE_FILTER_BILINEAR);

    Image window_icon;
    svg_load(into_shared_dir_path(DATA_PATH "logo.svg"), config.ui_size, config.ui_size, &window_icon);
    assets.textures.icon_logo = LoadTextureFromImage(window_icon);
    SetTextureFilter(assets.textures.icon_logo, TEXTURE_FILTER_BILINEAR);

    void* image_load_paths[] = {
        &assets.textures.button_add_arg,     "add_arg.svg",
        &assets.textures.button_add_text,    "add_text.svg",
        &assets.textures.button_arrow_left,  "arrow_left.svg",
        &assets.textures.button_arrow_right, "arrow_right.svg",
        &assets.textures.button_build,       "build.svg",
        &assets.textures.button_close,       "close.svg",
        &assets.textures.button_del_arg,     "del_arg.svg",
        &assets.textures.button_edit,        "edit.svg",
        &assets.textures.button_run,         "run.svg",
        &assets.textures.button_stop,        "stop.svg",
        &assets.textures.icon_about,         "about.svg",
        &assets.textures.icon_file,          "file.svg",
        &assets.textures.icon_folder,        "folder.svg",
        &assets.textures.icon_list,          "list.svg",
        &assets.textures.icon_pi,            "pi_symbol.svg",
        &assets.textures.icon_settings,      "settings.svg",
        &assets.textures.icon_special,       "special.svg",
        &assets.textures.icon_term,          "term.svg",
        &assets.textures.icon_variable,      "variable_symbol.svg",
        &assets.textures.icon_warning,       "warning.svg",
        NULL,
    };

    for (int i = 0; image_load_paths[i]; i += 2) {
        Image svg_img;
        if (!svg_load(TextFormat("%s" DATA_PATH "%s", get_shared_dir_path(), image_load_paths[i + 1]), config.ui_size, config.ui_size, &svg_img)) {
            continue;
        }

        Texture2D* texture = image_load_paths[i];
        *texture = LoadTextureFromImage(svg_img);
        SetTextureFilter(*texture, TEXTURE_FILTER_BILINEAR);
        UnloadImage(svg_img);
    }

    int* codepoints = vector_create();
    for (int i = 0; i < CODEPOINT_REGION_COUNT; i++) {
        codepoint_start_ranges[i] = vector_size(codepoints);
        for (int j = codepoint_regions[i][0]; j <= codepoint_regions[i][1]; j++) {
            vector_add(&codepoints, j);
        }
    }
    int codepoints_count = vector_size(codepoints);

    assets.fonts.font_cond = LoadFontEx(get_font_path(config.font_path), config.ui_size, codepoints, codepoints_count);
    assets.fonts.font_cond_shadow = LoadFontEx(get_font_path(config.font_path), BLOCK_TEXT_SIZE, codepoints, codepoints_count);
    assets.fonts.font_eb = LoadFontEx(get_font_path(config.font_bold_path), config.ui_size * 0.8, codepoints, codepoints_count);
    assets.fonts.font_mono = LoadFontEx(get_font_path(config.font_mono_path), config.ui_size, codepoints, codepoints_count);
    vector_free(codepoints);

    SetTextureFilter(assets.fonts.font_cond.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(assets.fonts.font_cond_shadow.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(assets.fonts.font_eb.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(assets.fonts.font_mono.texture, TEXTURE_FILTER_BILINEAR);

    prerender_font_shadow(&assets.fonts.font_cond_shadow);

    assets.line_shader = LoadShaderFromMemory(line_shader_vertex, line_shader_fragment);
    ui.shader_time_loc = GetShaderLocation(assets.line_shader, "time");

    assets.gradient_shader = LoadShaderFromMemory(gradient_shader_vertex, gradient_shader_fragment);

    strncpy(editor.project_name, EDITOR_DEFAULT_PROJECT_NAME, 1024);
    editor.blockchain_select_counter = -1;

    ui.render_surface_needs_redraw = true;

    vm = vm_new();
    register_blocks(&vm);

    editor.show_debug = true;
    editor.mouse_blockchain = blockchain_new();
    editor.code = vector_create();

    editor.search_list = vector_create();
    editor.search_list_search = vector_create();
    vector_add(&editor.search_list_search, 0);
    update_search();

    term_init(term_measure_text, &assets.fonts.font_mono, config.ui_size * 0.6);

    // This fixes incorrect texture coordinates in gradient shader
    Texture2D texture = {
        .id = rlGetTextureIdDefault(),
        .width = 1,
        .height = 1,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    SetShapesTexture(texture, (Rectangle){ 0.0f, 0.0f, 1.0f, 1.0f });

    gui = malloc(sizeof(Gui));
    gui_init(gui);
    gui_set_measure_text_func(gui, scrap_gui_measure_text);
    gui_set_measure_image_func(gui, scrap_gui_measure_image);
    gui_update_window_size(gui, GetScreenWidth(), GetScreenHeight());
    scrap_log(LOG_INFO, "Allocated %.2f KiB for gui", (float)sizeof(Gui) / 1024.0f);
    init_gui_window();

    editor.blockchain_render_layer_widths = vector_create();

    return window_icon;
}

void cleanup(void) {
    term_free();

    blockchain_free(&editor.mouse_blockchain);
    for (vec_size_t i = 0; i < vector_size(editor.code); i++) blockchain_free(&editor.code[i]);
    vector_free(editor.code);
    vm_free(&vm);

    vector_free(editor.blockchain_render_layer_widths);
    free(gui);

    delete_all_tabs();
    vector_free(editor.tabs);

    vector_free(editor.search_list_search);
    vector_free(editor.search_list);
    vector_free(vm.compile_error);

    unregister_categories();

    project_config_free(&project_config);
    config_free(&config);
    config_free(&window_config);

    CloseWindow();
}

// Main function: Initializes configurations, sets up window, processes input, renders GUI, and cleans up resources on exit
int main(void) {
    SetTraceLogCallback(scrap_log_va);
    config_new(&config);
    config_new(&window_config);
    project_config_new(&project_config);
    project_config_set_default(&project_config);

    editor.tabs = vector_create();
    set_default_config(&config);
    load_config(&config);

    if (config.language != LANG_SYSTEM) {
#ifdef _WIN32
        scrap_set_env("LANG", language_to_code(config.language));
#else
        scrap_set_env("LANGUAGE", language_to_code(config.language));
#endif
    }
    setlocale(LC_MESSAGES, "");
    textdomain("scrap");
    bindtextdomain("scrap", get_locale_path());
#ifdef _WIN32
    bind_textdomain_codeset("scrap", "UTF-8");
#endif

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Scrap");
    //SetWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(config.fps_limit);

    Image icon = setup();
    SetWindowIcon(icon);
    // SetWindowIcon() copies the icon so we can safely unload it
    UnloadImage(icon);

    ui.scrap_running = true;
    while (ui.scrap_running) {
        if (WindowShouldClose()) {
            if (!editor.project_modified) {
                ui.scrap_running = false;
                break;
            } else {
                gui_window_show(draw_save_confirmation_window);
            }
        }

        vm_handle_running_thread();

        scrap_gui_process_ui();

        BeginTextureMode(ui.render_surface);
            scrap_gui_process_render();
        EndTextureMode();

        BeginDrawing();
            Texture2D texture = ui.render_surface.texture;

            DrawTexturePro(
                texture,
                (Rectangle) {
#ifdef ARABIC_MODE
                    // Flip texture upside down and also mirror it ;)
                    texture.width, texture.height,
#else
                    // Render everything just below the texture. This texture has wrapping mode set to TEXTURE_WRAP_MIRROR_REPEAT,
                    // so this will have the effect of flipping the texture upside down
                    0, texture.height,
#endif
                    texture.width, texture.height,
                },
                (Rectangle) {
                    0, 0,
                    texture.width, texture.height,
                },
                (Vector2) {0}, // Origin at 0,0
                0.0, // No rotation
                WHITE // No tint
            );
        EndDrawing();
    }

    cleanup();
    return 0;
}
