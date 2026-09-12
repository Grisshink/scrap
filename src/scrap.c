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
#include "std.h"

#include <math.h>
#include <libintl.h>
#include <locale.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#define KiB(n) ((size_t)(n) << 10)
#define MiB(n) ((size_t)(n) << 20)
#define GiB(n) ((size_t)(n) << 30)

// Global Variables

Config config;

Assets assets;

Vm vm;
Gui* gui = NULL;
Gui gui_val;

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

Image setup(void* save_data, size_t save_size) {
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
        &assets.textures.icon_c,             "c_icon.svg",
        &assets.textures.icon_error,         "error.svg",
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

    reload_fonts();

    assets.line_shader = LoadShaderFromMemory(line_shader_vertex, line_shader_fragment);
    ui.shader_time_loc = GetShaderLocation(assets.line_shader, "time");

    assets.gradient_shader = LoadShaderFromMemory(gradient_shader_vertex, gradient_shader_fragment);

    strncpy(editor.project_name, EDITOR_DEFAULT_PROJECT_NAME, 1024);
    editor.blockchain_select_counter = -1;

    ui.render_surface_needs_redraw = true;

    ui.scratch_arena = gui_arena_new(GiB(1), MiB(1));
    ui.hover.editor.select_argument_scratch_input = vector_create();
    vector_add(&ui.hover.editor.select_argument_scratch_input, 0);

    vm = vm_new();
    register_blocks(&vm);

    editor.show_debug = true;
    editor.mouse_blockchains = vector_create();
    editor.code = save_data && save_size ? load_code(save_data, save_size, false) : vector_create();

    if (vector_size(editor.code) > 0) {
        editor.camera_pos.x = editor.code[0].x - 50;
        editor.camera_pos.y = editor.code[0].y - 50;
    }

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

    gui_val = gui_new(GiB(1));
    gui = &gui_val;
    gui_set_measure_text_func(gui, scrap_gui_measure_text);
    gui_set_measure_image_func(gui, scrap_gui_measure_image);
    gui_update_window_size(gui, GetScreenWidth(), GetScreenHeight());
    init_gui_window();

    editor.blockchain_render_layer_widths = vector_create();

    return window_icon;
}

void cleanup(void) {
    for (size_t i = 0; i < vector_size(editor.mouse_blockchains); i++) blockchain_free(editor.mouse_blockchains[i].chain);
    vector_free(editor.mouse_blockchains);
    for (size_t i = 0; i < vector_size(editor.code); i++) blockchain_free(editor.code[i].chain);
    vector_free(editor.code);
    vm_free(&vm);

    // Free the terminal after vm as vm still can reference the terminal
    term_free();

    vector_free(editor.blockchain_render_layer_widths);
    gui_free(gui);

    gui_arena_free(ui.scratch_arena);
    vector_free(ui.hover.editor.select_argument_scratch_input);

    delete_all_tabs();
    vector_free(editor.tabs);

    vector_free(editor.search_list_search);
    vector_free(editor.search_list);

    unregister_categories();

    config_free(&config);
    config_free(&window_config);

    CloseWindow();
}

void start_editor(void* save_data, size_t save_size, bool daemonize) {
#ifdef _WIN32
    (void) daemonize;
#else
    if (daemonize) {
        printf("Opening scrap in background...\n");
        daemon(1, 0);
    }
#endif

    SetTraceLogCallback(scrap_log_va);
    config_new(&config);
    config_new(&window_config);

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

    Image icon = setup(save_data, save_size);
    if (save_data) free(save_data);
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
}

int start_runtime(void* bc_data, size_t bc_size) {
    assert(bc_data != NULL);
    assert(bc_size > 0);

    // When starting the editor, GLFW internally sets LC_CTYPE locale to make %lc format options work properly, 
    // so we need to set it here explicitly
    setlocale(LC_CTYPE, "");

    IrMemArena* arena = ir_arena_new(GiB(1), KiB(512));
    IrBytecodePool* pool = bytecode_pool_new(arena);
    IrBytecode bc;

    bool success = bytecode_load(pool, &bc, bc_data, bc_size);
    free(bc_data);
    if (!success) {
        printf("Bytecode load error\n");
        bytecode_pool_free(pool);
        return 1;
    }
    bc.name = "main";

    std_init();

    IrExec exec = exec_new(MiB(1), GiB(1));
    if (exec.last_error[0] != 0) {
        printf("Exec create error: %s\n", exec.last_error);
        bytecode_pool_free(pool);
        return 1;
    }

    exec_set_run_function_resolver(&exec, std_resolve_function);
    exec_add_bytecode(&exec, bc);

    if (!bytecode_find_label(&bc, "entry")) {
        bytecode_pool_free(pool);
        exec_free(&exec);
        return 0;
    }

    if (!exec_run(&exec, "main", "entry")) {
        printf("Runtime error: %s\n", exec.last_error);
        bytecode_pool_free(pool);
        exec_free(&exec);
        return 1;
    }

    bytecode_pool_free(pool);
    exec_free(&exec);
    return 0;
}

void usage(char* exe_name) {
    init_console();

    printf("Usage %s [OPTIONS] [FILE]\n\n", exe_name);
    printf("OPTIONS:\n");
#ifndef _WIN32
    printf("    -d, --no-daemon -- Do not run scrap as background process, log everything into console (Linux only)\n");
#endif
    printf("    -h, --help      -- Show help\n");
#ifdef _WIN32
    printf("Press enter to close");
    getchar();
#endif
    exit(1);
}

int main(int argc, char** argv) {
    struct {
        bool help;
        bool no_daemonize;
        char* file_path;
    } flags = {0};

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            flags.help = true;
        } else if (!strcmp(argv[i], "--help")) {
            flags.help = true;
#ifndef _WIN32
        } else if (!strcmp(argv[i], "--no-daemon")) {
            flags.no_daemonize = true;
        } else if (!strcmp(argv[i], "-d")) {
            flags.no_daemonize = true;
#endif
        } else {
            if (argv[i][0] == '-') {
                printf("Error: Unknown flag \"%s\"\n", argv[i]);
                usage(argv[0]);
            }

            if (flags.file_path) {
                printf("Error: Multiple file paths provided\n");
                usage(argv[0]);
            }
            flags.file_path = argv[i];
        }
    }

    if (flags.help) usage(argv[0]);

    if (flags.file_path) {
        FILE* f = fopen(flags.file_path, "rb");
        if (!f) {
            printf("Cannot load file at path \"%s\": %s\n", flags.file_path, strerror(errno));
            return 1;
        }

        fseek(f, 0, SEEK_END);
        size_t file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        void* file_data = malloc(file_size);
        file_size = fread(file_data, 1, file_size, f);
        fclose(f);

        if (bytecode_load(NULL, NULL, file_data, file_size)) {
            return start_runtime(file_data, file_size);
        } else if (load_code(file_data, file_size, true)) {
            start_editor(file_data, file_size, !flags.no_daemonize);
            return 0;
        } else {
            scrap_log(LOG_ERROR, "Specified file at path \"%s\" does not look like a scrap project nor scrap bytecode, aborting", flags.file_path);
            free(file_data);
            return 1;
        }
    }

    start_editor(NULL, 0, !flags.no_daemonize);
    return 0;
}
