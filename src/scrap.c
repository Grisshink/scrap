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

#include <math.h>
#include <libintl.h>
#include <locale.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

// Global Variables

Shader line_shader;
RenderTexture2D render_surface;
bool render_surface_needs_redraw = true;
int shader_time_loc;

Exec exec = {0};
char** exec_compile_error = NULL;
Block* exec_compile_error_block = NULL;
BlockChain* exec_compile_error_blockchain = NULL;

Vector2 camera_pos = {0};
Vector2 camera_click_pos = {0};

Config conf;
ProjectConfig project_conf;
HoverInfo hover = {0};
Shader line_shader;

Font font_cond;
Font font_cond_shadow;
Font font_eb;
Font font_mono;

TextureList textures;

Vm vm;
int start_vm_timeout = -1;
#ifndef USE_INTERPRETER
CompilerMode start_vm_mode = COMPILER_MODE_JIT;
#endif
Vector2 camera_pos;
ActionBar actionbar;
BlockCode block_code = {0};
Dropdown dropdown = {0};
BlockPalette palette = {0};
BlockChain* editor_code = {0};
Blockdef** search_list = NULL;
BlockChain mouse_blockchain = {0};
Gui* gui = NULL;
char* search_list_search = NULL;
int categories_scroll = 0;
int search_list_scroll = 0;
Vector2 search_list_pos = {0};
int* blockchain_render_layer_widths;

SplitPreview split_preview = {0};
Tab* code_tabs = NULL;
int current_tab = 0;

char project_name[1024] = "project.scrp";
char debug_buffer[DEBUG_BUFFER_LINES][DEBUG_BUFFER_LINE_SIZE] = {0};

#ifdef DEBUG
double ui_time = 0.0;
#endif

float shader_time = 0.0;
int blockchain_select_counter = -1;

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

// Fragment shader code for line rendering with time-based effects and color modulation
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

// End-stage brain Winlator
char* top_bar_buttons_text[3] = {
    "File",
    "Settings",
    "About",
};

char* tab_bar_buttons_text[2] = {
    "Code",
    "Output",
};

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
    TraceLog(LOG_INFO, "Using \"%s\" as shared directory path", out_path);
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
    TraceLog(LOG_INFO, "Using \"%s\" as locale directory path", out_path);
    return out_path;
}

const char* into_shared_dir_path(const char* path) {
    return TextFormat("%s%s", get_shared_dir_path(), path);
}

// Returns the absolute path to the font, converting the relative path to a path inside the data directory
const char* get_font_path(char* font_path) {
    return font_path[0] != '/' && font_path[1] != ':' ? into_shared_dir_path(font_path) : font_path;
}

static Vm vm_new(void) {
    Vm vm = (Vm) {
        .blockdefs = vector_create(),
        .end_blockdef = -1,
        .thread = thread_new(exec_run, exec_cleanup),
        .exec_chain = NULL,
        .exec_ind = 0,
        .prev_exec_chain = NULL,
        .prev_exec_ind = 0,
    };
    return vm;
}

static void vm_free(Vm* vm) {
    for (ssize_t i = (ssize_t)vector_size(vm->blockdefs) - 1; i >= 0 ; i--) {
        blockdef_unregister(vm, i);
    }
    vector_free(vm->blockdefs);
}

static BlockChain* find_blockchain(Block* block) {
    if (!block) return NULL;
    while (block->parent) block = block->parent;
    for (size_t i = 0; i < vector_size(editor_code); i++) {
        if (block >= editor_code[i].blocks && block < editor_code[i].blocks + vector_size(editor_code[i].blocks)) {
            return &editor_code[i];
        }
    }
    return NULL;
}

// Initializes resources and settings by loading textures, fonts, and configurations, and sets up GUI and panel interface
Image setup(void) {
    SetExitKey(KEY_NULL);
    render_surface = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    SetTextureWrap(render_surface.texture, TEXTURE_WRAP_MIRROR_REPEAT);

    textures.dropdown = LoadTexture(into_shared_dir_path(DATA_PATH "drop.png"));
    SetTextureFilter(textures.dropdown, TEXTURE_FILTER_BILINEAR);

    Image window_icon;
    svg_load(into_shared_dir_path(DATA_PATH "logo.svg"), conf.font_size, conf.font_size, &window_icon);
    textures.icon_logo = LoadTextureFromImage(window_icon);
    SetTextureFilter(textures.icon_logo, TEXTURE_FILTER_BILINEAR);

    void* image_load_paths[] = {
        &textures.button_add_arg,     "add_arg.svg",
        &textures.button_add_text,    "add_text.svg",
        &textures.button_arrow_left,  "arrow_left.svg",
        &textures.button_arrow_right, "arrow_right.svg",
        &textures.button_build,       "build.svg",
        &textures.button_close,       "close.svg",
        &textures.button_del_arg,     "del_arg.svg",
        &textures.button_edit,        "edit.svg",
        &textures.button_run,         "run.svg",
        &textures.button_stop,        "stop.svg",
        &textures.icon_list,          "list.svg",
        &textures.icon_pi,            "pi_symbol.svg",
        &textures.icon_special,       "special.svg",
        &textures.icon_term,          "term.svg",
        &textures.icon_variable,      "variable_symbol.svg",
        &textures.icon_warning,       "warning.svg",
        NULL,
    };

    for (int i = 0; image_load_paths[i]; i += 2) {
        Image svg_img;
        if (!svg_load(TextFormat("%s" DATA_PATH "%s", get_shared_dir_path(), image_load_paths[i + 1]), conf.font_size, conf.font_size, &svg_img)) {
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

    font_cond = LoadFontEx(get_font_path(conf.font_path), conf.font_size, codepoints, codepoints_count);
    font_cond_shadow = LoadFontEx(get_font_path(conf.font_path), BLOCK_TEXT_SIZE, codepoints, codepoints_count);
    font_eb = LoadFontEx(get_font_path(conf.font_bold_path), conf.font_size * 0.8, codepoints, codepoints_count);
    font_mono = LoadFontEx(get_font_path(conf.font_mono_path), conf.font_size, codepoints, codepoints_count);
    vector_free(codepoints);

    SetTextureFilter(font_cond.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(font_cond_shadow.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(font_eb.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(font_mono.texture, TEXTURE_FILTER_BILINEAR);

    prerender_font_shadow(&font_cond_shadow);

    line_shader = LoadShaderFromMemory(line_shader_vertex, line_shader_fragment);
    shader_time_loc = GetShaderLocation(line_shader, "time");

    exec_compile_error = vector_create();

    vm = vm_new();
    register_blocks(&vm);

    mouse_blockchain = blockchain_new();
    editor_code = vector_create();

    search_list = vector_create();
    search_list_search = vector_create();
    vector_add(&search_list_search, 0);
    update_search();

    term_init(term_measure_text, &font_mono, conf.font_size * 0.6);

    gui = malloc(sizeof(Gui));
    gui_init(gui);
    gui_set_measure_text_func(gui, scrap_gui_measure_text);
    gui_set_measure_image_func(gui, scrap_gui_measure_image);
    gui_update_window_size(gui, GetScreenWidth(), GetScreenHeight());
    TraceLog(LOG_INFO, "Allocated %.2f KiB for gui", (float)sizeof(Gui) / 1024.0f);
    init_gui_window();

    blockchain_render_layer_widths = vector_create();

    return window_icon;
}

// Main function: Initializes configurations, sets up window, processes input, renders GUI, and cleans up resources on exit
int main(void) {
    SetTraceLogCallback(scrap_log);
    config_new(&conf);
    config_new(&window_conf);
    project_config_new(&project_conf);
    project_config_set_default(&project_conf);

    code_tabs = vector_create();
    set_default_config(&conf);
    load_config(&conf);

    if (conf.language != LANG_SYSTEM) {
#ifdef _WIN32
        scrap_set_env("LANG", language_to_code(conf.language));
#else
        scrap_set_env("LANGUAGE", language_to_code(conf.language));
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
    SetTargetFPS(conf.fps_limit);

    Image icon = setup();
    SetWindowIcon(icon);
    // SetWindowIcon() copies the icon so we can safely unload it
    UnloadImage(icon);

    while (!WindowShouldClose()) {
        vm.exec_ind = -1;
        vm.exec_chain = NULL;

        ThreadReturnCode thread_return = thread_try_join(&vm.thread);
        if (thread_return != THREAD_RETURN_RUNNING) {
            switch (thread_return) {
            case THREAD_RETURN_SUCCESS:
                actionbar_show(gettext("Vm executed successfully"));
                break;
            case THREAD_RETURN_FAILURE:
                actionbar_show(gettext("Vm shitted and died :("));
                break;
            case THREAD_RETURN_STOPPED:
                actionbar_show(gettext("Vm stopped >:("));
                break;
            default:
                break;
            }

            size_t i = 0;
            while (exec.current_error[i]) {
                vector_add(&exec_compile_error, vector_create());
                size_t line_len = 0;
                while (line_len < 50 && exec.current_error[i]) {
                    if (((unsigned char)exec.current_error[i] >> 6) != 2) line_len++;
                    if (line_len >= 50) break;
                    vector_add(&exec_compile_error[vector_size(exec_compile_error) - 1], exec.current_error[i++]);
                }
                vector_add(&exec_compile_error[vector_size(exec_compile_error) - 1], 0);
            }
            exec_compile_error_block = exec.current_error_block;
            exec_compile_error_blockchain = find_blockchain(exec_compile_error_block);
            exec_free(&exec);
            render_surface_needs_redraw = true;
        } else if (thread_is_running(&vm.thread)) {
#ifdef USE_INTERPRETER
            vm.exec_chain = exec.running_chain;
            vm.exec_ind = exec.chain_stack[exec.chain_stack_len - 1].running_ind;
#else
            vm.exec_chain = NULL;
            vm.exec_ind = 0;
#endif
            if (vm.prev_exec_chain != vm.exec_chain) render_surface_needs_redraw = true;
            if (vm.prev_exec_ind != vm.exec_ind) render_surface_needs_redraw = true;

            vm.prev_exec_chain = vm.exec_chain;
            vm.prev_exec_ind = vm.exec_ind;

            mutex_lock(&term.lock);
            if (find_panel(code_tabs[current_tab].root_panel, PANEL_TERM) && term.is_buffer_dirty) {
                render_surface_needs_redraw = true;
                term.is_buffer_dirty = false;
            }
            mutex_unlock(&term.lock);
       } else {
            if (vector_size(exec_compile_error) > 0) render_surface_needs_redraw = true;
       }

        actionbar.show_time -= GetFrameTime();
        if (actionbar.show_time < 0) {
            actionbar.show_time = 0;
        } else {
            render_surface_needs_redraw = true;
        }

        if (shader_time_loc != -1) SetShaderValue(line_shader, shader_time_loc, &shader_time, SHADER_UNIFORM_FLOAT);
        shader_time += GetFrameTime() / 2.0;
        if (shader_time >= 1.0) {
            shader_time = 1.0;
        } else {
            render_surface_needs_redraw = true;
        }

        scrap_gui_process_input();

        if (render_surface_needs_redraw) {
            BeginTextureMode(render_surface);
                scrap_gui_process_render();
            EndTextureMode();
            render_surface_needs_redraw = false;
        }

        BeginDrawing();
            DrawTexturePro(
                render_surface.texture,
#ifdef ARABIC_MODE
                (Rectangle) { render_surface.texture.width, render_surface.texture.height, render_surface.texture.width, render_surface.texture.height },
#else
                (Rectangle) { 0, render_surface.texture.height, render_surface.texture.width, render_surface.texture.height },
#endif
                (Rectangle) { 0, 0, render_surface.texture.width, render_surface.texture.height },
                (Vector2) {0},
                0.0,
                WHITE
            );
        EndDrawing();
    }

    if (thread_is_running(&vm.thread)) {
        thread_stop(&vm.thread);
        thread_join(&vm.thread);
        exec_free(&exec);
    }
    term_free();
    blockchain_free(&mouse_blockchain);
    for (vec_size_t i = 0; i < vector_size(editor_code); i++) blockchain_free(&editor_code[i]);
    vector_free(editor_code);
    vm_free(&vm);
    vector_free(blockchain_render_layer_widths);
    free(gui);
    delete_all_tabs();
    vector_free(search_list_search);
    vector_free(search_list);
    vector_free(code_tabs);
    vector_free(exec_compile_error);
    unregister_categories();
    project_config_free(&project_conf);
    config_free(&conf);
    config_free(&window_conf);
    CloseWindow();

    return 0;
}
