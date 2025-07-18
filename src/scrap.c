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

#define SCRVM_IMPLEMENTATION
#include "term.h"
#include "scrap.h"
#include "vec.h"

#include <math.h>
#include <libintl.h>
#include <locale.h>
#include <assert.h>
#include <string.h>

// Global Variables
Image logo_img;

Shader line_shader;
RenderTexture2D render_surface;
bool render_surface_needs_redraw = true;
int shader_time_loc;

Exec exec = {0};
char exec_compile_error[MAX_ERROR_LEN] = {0};
Block* exec_compile_error_block = NULL;

Vector2 camera_pos = {0};
Vector2 camera_click_pos = {0};

Config conf;
HoverInfo hover_info = {0};
Shader line_shader;

Font font_cond;
Font font_cond_shadow;
Font font_eb;
Font font_mono;

Texture2D run_tex;
Texture2D stop_tex;
Texture2D drop_tex;
Texture2D close_tex;
Texture2D logo_tex;
Texture2D warn_tex;
Texture2D edit_tex;
Texture2D close_tex;
Texture2D term_tex;
Texture2D add_arg_tex;
Texture2D del_arg_tex;
Texture2D add_text_tex;
Texture2D special_tex;
Texture2D list_tex;
Texture2D arrow_left_tex;
Texture2D arrow_right_tex;
Texture2D pi_symbol_tex;

Vm vm;
int start_vm_timeout = -1;
Vector2 camera_pos;
ActionBar actionbar;
BlockCode block_code = {0};
Dropdown dropdown = {0};
BlockPalette palette = {0};
BlockChain* editor_code = {0};
Block** search_list = NULL;
BlockChain mouse_blockchain = {0};
Gui* gui = NULL;
char* search_list_search = NULL;
int categories_scroll = 0;
int search_list_scroll = 0;
Vector2 search_list_pos = {0};

#ifdef RAM_OVERLOAD
int* overload;
pthread_t overload_thread;
#endif

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

// Recursively checks nested blocks for correct structure and connection with the parent block
void sanitize_block(Block* block) {
    for (vec_size_t i = 0; i < vector_size(block->arguments); i++) {
        if (block->arguments[i].type != ARGUMENT_BLOCK) continue;
        if (block->arguments[i].data.block.parent != block) {
            TraceLog(LOG_ERROR, "Block %p detached from parent %p! (Got %p)", &block->arguments[i].data.block, block, block->arguments[i].data.block.parent);
            assert(false);
            return;
        }
        sanitize_block(&block->arguments[i].data.block);
    }
}

// Checks the integrity and correctness of connections of all blocks of editor code and the mouse blockchain
void sanitize_links(void) {
    for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
        Block* blocks = editor_code[i].blocks;
        for (vec_size_t j = 0; j < vector_size(blocks); j++) {
            sanitize_block(&blocks[j]);
        }
    }

    for (vec_size_t i = 0; i < vector_size(mouse_blockchain.blocks); i++) {
        sanitize_block(&mouse_blockchain.blocks[i]);
    }
}

#ifdef RAM_OVERLOAD
void* overload_thread_entry(void* thread_val) {
    (void) thread_val;
    overload = vector_create();

    volatile int val = 0;
    while (1) vector_add(&overload, val++);
    return NULL;
}
#endif

Texture2D load_svg(const char* path) {
    Image svg_img = LoadImageSvg(path, conf.font_size, conf.font_size);
    Texture2D texture = LoadTextureFromImage(svg_img);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    UnloadImage(svg_img);
    return texture;
}

// Returns the absolute path to the font, converting the relative path to a path inside the data directory
const char* get_font_path(char* font_path) {
    return font_path[0] != '/' && font_path[1] != ':' ? into_data_path(font_path) : font_path;
}

GuiMeasurement scrap_gui_measure_image(void* image, unsigned short size) {
    Texture2D* img = image;
    return (GuiMeasurement) { img->width * ((float)size / (float)img->height), size };
}

int search_glyph(int codepoint) {
    // We assume that ASCII region is the first region, so this index should correspond to char '?' in the glyph table
    const int fallback = 31;
    for (int i = 0; i < CODEPOINT_REGION_COUNT; i++) {
        if (codepoint < codepoint_regions[i][0] || codepoint > codepoint_regions[i][1]) continue;
        return codepoint - codepoint_regions[i][0] + codepoint_start_ranges[i];
    }
    return fallback;
}

GuiMeasurement measure_slice(Font font, const char *text, unsigned int text_size, float font_size) {
    GuiMeasurement ms = {0};

    if ((font.texture.id == 0) || !text) return ms;

    int codepoint = 0; // Current character
    int index = 0; // Index position in sprite font

    for (unsigned int i = 0; i < text_size;) {
        int next = 0;
        codepoint = GetCodepointNext(&text[i], &next);
        index = search_glyph(codepoint);
        i += next;

        if (font.glyphs[index].advanceX != 0) {
            ms.w += font.glyphs[index].advanceX;
        } else {
            ms.w += font.recs[index].width + font.glyphs[index].offsetX;
        }
    }

    ms.w *= font_size / (float)font.baseSize;
    ms.h = font_size;
    return ms;
}

GuiMeasurement scrap_gui_measure_text(void* font, const char* text, unsigned int text_size, unsigned short font_size) {
    return measure_slice(*(Font*)font, text, text_size, font_size);
}

BlockCategory block_category_new(const char* name, Color color) {
    return (BlockCategory) {
        .name = name,
        .color = color,
        .blocks = vector_create(),
    };
}

void block_category_free(BlockCategory* category) {
    for (size_t i = 0; i < vector_size(category->blocks); i++) block_free(&category->blocks[i]);
    vector_free(category->blocks);
}

// Divides the panel into two parts along the specified side with the specified split percentage
void panel_split(PanelTree* panel, SplitSide side, PanelType new_panel_type, float split_percent) {
    if (panel->type == PANEL_SPLIT) return;

    PanelTree* old_panel = malloc(sizeof(PanelTree));
    old_panel->type = panel->type;
    old_panel->left = NULL;
    old_panel->right = NULL;
    old_panel->parent = panel;

    PanelTree* new_panel = malloc(sizeof(PanelTree));
    new_panel->type = new_panel_type;
    new_panel->left = NULL;
    new_panel->right = NULL;
    new_panel->parent = panel;

    panel->type = PANEL_SPLIT;

    switch (side) {
    case SPLIT_SIDE_TOP:
        panel->direction = DIRECTION_VERTICAL;
        panel->left = new_panel;
        panel->right = old_panel;
        panel->split_percent = split_percent;
        break;
    case SPLIT_SIDE_BOTTOM:
        panel->direction = DIRECTION_VERTICAL;
        panel->left = old_panel;
        panel->right = new_panel;
        panel->split_percent = 1.0 - split_percent;
        break;
    case SPLIT_SIDE_LEFT:
        panel->direction = DIRECTION_HORIZONTAL;
        panel->left = new_panel;
        panel->right = old_panel;
        panel->split_percent = split_percent;
        break;
    case SPLIT_SIDE_RIGHT:
        panel->direction = DIRECTION_HORIZONTAL;
        panel->left = old_panel;
        panel->right = new_panel;
        panel->split_percent = 1.0 - split_percent;
        break;
    case SPLIT_SIDE_NONE:
        assert(false && "Got SPLIT_SIDE_NONE");
        break;
    default:
        assert(false && "Got unknown split side");
        break;
    }
}

PanelTree* panel_new(PanelType type) {
    PanelTree* panel = malloc(sizeof(PanelTree));
    panel->type = type;
    panel->left = NULL;
    panel->right = NULL;
    panel->parent = NULL;
    return panel;
}

// Removes a panel and its child panels recursively, freeing memory
void panel_delete(PanelTree* panel) {
    assert(panel != NULL);

    if (panel->type == PANEL_SPLIT) {
        panel_delete(panel->left);
        panel_delete(panel->right);
        panel->left = NULL;
        panel->right = NULL;
    }

    panel->type = PANEL_NONE;
    free(panel);
}

// Removes a tab by index and frees its resources
void tab_delete(size_t tab) {
    assert(tab < vector_size(code_tabs));
    panel_delete(code_tabs[tab].root_panel);
    vector_free(code_tabs[tab].name);
    vector_remove(code_tabs, tab);
    if (current_tab >= (int)vector_size(code_tabs)) current_tab = vector_size(code_tabs) - 1;
}

void delete_all_tabs(void) {
    for (ssize_t i = vector_size(code_tabs) - 1; i >= 0; i--) tab_delete(i);
}

// Creates a new tab with the given name and panel, adding it to the list of tabs
size_t tab_new(char* name, PanelTree* root_panel) {
    if (!root_panel) {
        TraceLog(LOG_WARNING, "Got root_panel == NULL, not adding");
        return -1;
    }

    Tab* tab = vector_add_dst(&code_tabs);
    tab->name = vector_create();
    for (char* str = name; *str; str++) vector_add(&tab->name, *str);
    vector_add(&tab->name, 0);
    tab->root_panel = root_panel;

    return vector_size(code_tabs) - 1;
}

// Inserts a new tab with the given name and panel at the specified position in the list of tabs
void tab_insert(char* name, PanelTree* root_panel, size_t position) {
    if (!root_panel) {
        TraceLog(LOG_WARNING, "Got root_panel == NULL, not adding");
        return;
    }

    Tab* tab = vector_insert_dst(&code_tabs, position);
    tab->name = vector_create();
    for (char* str = name; *str; str++) vector_add(&tab->name, *str);
    vector_add(&tab->name, 0);
    tab->root_panel = root_panel;
}

// Initializes codespace, using a default panel layout
void init_panels(void) {
    PanelTree* code_panel = panel_new(PANEL_CODE);
    panel_split(code_panel, SPLIT_SIDE_LEFT, PANEL_BLOCK_PALETTE, 0.3);
    panel_split(code_panel->left, SPLIT_SIDE_TOP, PANEL_BLOCK_CATEGORIES, 0.35);
    tab_new("Code", code_panel);
    tab_new("Output", panel_new(PANEL_TERM));
}

size_t blockdef_register(Vm* vm, Blockdef* blockdef) {
    if (!blockdef->func) TraceLog(LOG_WARNING, "[VM] Block \"%s\" has not defined its implementation!", blockdef->id);

    vector_add(&vm->blockdefs, blockdef);
    blockdef->ref_count++;
    if (blockdef->type == BLOCKTYPE_END && vm->end_blockdef == (size_t)-1) {
        vm->end_blockdef = vector_size(vm->blockdefs) - 1;
    }

    return vector_size(vm->blockdefs) - 1;
}

void blockdef_unregister(Vm* vm, size_t block_id) {
    blockdef_free(vm->blockdefs[block_id]);
    vector_remove(vm->blockdefs, block_id);
}

Vm vm_new(void) {
    Vm vm = (Vm) {
        .blockdefs = vector_create(),
        .end_blockdef = -1,
        .is_running = false,
    };
    return vm;
}

void vm_free(Vm* vm) {
    for (ssize_t i = (ssize_t)vector_size(vm->blockdefs) - 1; i >= 0 ; i--) {
        blockdef_unregister(vm, i);
    }
    vector_free(vm->blockdefs);
}

// Initializes resources and settings by loading textures, fonts, and configurations, and sets up GUI and panel interface
void setup(void) {
    SetExitKey(KEY_NULL);
    render_surface = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    SetTextureWrap(render_surface.texture, TEXTURE_WRAP_MIRROR_REPEAT);

    run_tex = LoadTexture(into_data_path(DATA_PATH "run.png"));
    SetTextureFilter(run_tex, TEXTURE_FILTER_BILINEAR);
    drop_tex = LoadTexture(into_data_path(DATA_PATH "drop.png"));
    SetTextureFilter(drop_tex, TEXTURE_FILTER_BILINEAR);
    close_tex = LoadTexture(into_data_path(DATA_PATH "close.png"));
    SetTextureFilter(close_tex, TEXTURE_FILTER_BILINEAR);

    logo_img = LoadImageSvg(into_data_path(DATA_PATH "logo.svg"), conf.font_size, conf.font_size);
    logo_tex = LoadTextureFromImage(logo_img);
    SetTextureFilter(logo_tex, TEXTURE_FILTER_BILINEAR);

    warn_tex = load_svg(into_data_path(DATA_PATH "warning.svg"));
    stop_tex = load_svg(into_data_path(DATA_PATH "stop.svg"));
    edit_tex = load_svg(into_data_path(DATA_PATH "edit.svg"));
    close_tex = load_svg(into_data_path(DATA_PATH "close.svg"));
    term_tex = load_svg(into_data_path(DATA_PATH "term.svg"));
    add_arg_tex = load_svg(into_data_path(DATA_PATH "add_arg.svg"));
    del_arg_tex = load_svg(into_data_path(DATA_PATH "del_arg.svg"));
    add_text_tex = load_svg(into_data_path(DATA_PATH "add_text.svg"));
    special_tex = load_svg(into_data_path(DATA_PATH "special.svg"));
    list_tex = load_svg(into_data_path(DATA_PATH "list.svg"));
    arrow_left_tex = load_svg(into_data_path(DATA_PATH "arrow_left.svg"));
    arrow_right_tex = load_svg(into_data_path(DATA_PATH "arrow_right.svg"));
    pi_symbol_tex = load_svg(into_data_path(DATA_PATH "pi_symbol.svg"));

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

    vm = vm_new();
    register_categories();
    register_blocks(&vm);

    mouse_blockchain = blockchain_new();
    editor_code = vector_create();

    search_list = vector_create();
    search_list_search = vector_create();
    vector_add(&search_list_search, 0);
    update_search();

    term_init();

#if defined(RAM_OVERLOAD) && defined(_WIN32)
    if (should_do_ram_overload()) {
        pthread_create(&overload_thread, NULL, overload_thread_entry, NULL);
    }
#endif

    gui = malloc(sizeof(Gui));
    gui_init(gui);
    gui_set_measure_text_func(gui, scrap_gui_measure_text);
    gui_set_measure_image_func(gui, scrap_gui_measure_image);
    gui_update_window_size(gui, GetScreenWidth(), GetScreenHeight());
    TraceLog(LOG_INFO, "Allocated %.2f KiB for gui", (float)sizeof(Gui) / 1024.0f);
    init_gui_window();
}

// Main function: Initializes configurations, sets up window, processes input, renders GUI, and cleans up resources on exit
int main(void) {
    SetTraceLogCallback(scrap_log);
    config_new(&conf);
    config_new(&window_conf);
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
    bindtextdomain("scrap", into_data_path(LOCALE_PATH));
#ifdef _WIN32
    bind_textdomain_codeset("scrap", "UTF-8");
#endif

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Scrap");
    //SetWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(conf.fps_limit);

    setup();
    SetWindowIcon(logo_img);

    while (!WindowShouldClose()) {
        hover_info.exec_ind = -1;
        hover_info.exec_chain = NULL;

        size_t vm_return = -1;
        if (exec_try_join(&vm, &exec, &vm_return)) {
            if (vm_return == 1) {
                actionbar_show(gettext("Vm executed successfully"));
            } else if (vm_return == (size_t)PTHREAD_CANCELED) {
                actionbar_show(gettext("Vm stopped >:("));
            } else {
                actionbar_show(gettext("Vm shitted and died :("));
            }
            strncpy(exec_compile_error, exec.current_error, MAX_ERROR_LEN);
            exec_compile_error_block = exec.current_error_block;
            exec_free(&exec);
            render_surface_needs_redraw = true;
        } else if (vm.is_running) {
#ifdef USE_INTERPRETER
            hover_info.exec_chain = exec.running_chain;
            hover_info.exec_ind = exec.chain_stack[exec.chain_stack_len - 1].running_ind;
#else
            hover_info.exec_chain = NULL;
            hover_info.exec_ind = 0;
#endif
            if (hover_info.prev_exec_chain != hover_info.exec_chain) render_surface_needs_redraw = true;
            if (hover_info.prev_exec_ind != hover_info.exec_ind) render_surface_needs_redraw = true;

            hover_info.prev_exec_chain = hover_info.exec_chain;
            hover_info.prev_exec_ind = hover_info.exec_ind;

            pthread_mutex_lock(&term.lock);
            if (find_panel(code_tabs[current_tab].root_panel, PANEL_TERM) && term.is_buffer_dirty) {
                render_surface_needs_redraw = true;
                term.is_buffer_dirty = false;
            }
            pthread_mutex_unlock(&term.lock);
       } else {
            if (exec_compile_error[0]) render_surface_needs_redraw = true;
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

    if (vm.is_running) {
        exec_stop(&vm, &exec);
        size_t bin;
        exec_join(&vm, &exec, &bin);
        exec_free(&exec);
    }
    term_free();
    blockchain_free(&mouse_blockchain);
    for (vec_size_t i = 0; i < vector_size(editor_code); i++) blockchain_free(&editor_code[i]);
    vector_free(editor_code);
    vm_free(&vm);
    free(gui);
    delete_all_tabs();
    vector_free(search_list_search);
    vector_free(search_list);
    vector_free(code_tabs);
    unregister_categories();
    config_free(&conf);
    config_free(&window_conf);
    CloseWindow();

    return 0;
}
