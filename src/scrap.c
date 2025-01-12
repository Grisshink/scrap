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
#include "blocks.h"

#include <math.h>

Image logo_img;

Shader line_shader;
int shader_time_loc;

ScrExec exec = {0};

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

TabType current_tab = TAB_CODE;
ScrVm vm;
Vector2 camera_pos;
ActionBar actionbar;
BlockCode block_code = {0};
Dropdown dropdown = {0};
Sidebar sidebar = {0};
ScrBlockChain* editor_code = {0};
DrawStack* draw_stack = NULL;
ScrBlockChain mouse_blockchain = {0};
Gui* gui = NULL;

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

char* top_bar_buttons_text[3] = {
    "File",
    "Settings",
    "About",
};

char* tab_bar_buttons_text[2] = {
    "Code",
    "Output",
};

void blockcode_add_blockchain(BlockCode* blockcode, ScrBlockChain chain) {
    vector_add(&editor_code, chain);
    blockcode_update_measurments(blockcode);
}

void blockcode_remove_blockchain(BlockCode* blockcode, size_t ind) {
    vector_remove(editor_code, ind);
    blockcode_update_measurments(blockcode);
}

void sanitize_block(ScrBlock* block) {
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

void sanitize_links(void) {
    for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
        ScrBlock* blocks = editor_code[i].blocks;
        for (vec_size_t j = 0; j < vector_size(blocks); j++) {
            sanitize_block(&blocks[j]);
        }
    }

    for (vec_size_t i = 0; i < vector_size(mouse_blockchain.blocks); i++) {
        sanitize_block(&mouse_blockchain.blocks[i]);
    }
}

Texture2D load_svg(const char* path) {
    Image svg_img = LoadImageSvg(path, conf.font_size, conf.font_size);
    Texture2D texture = LoadTextureFromImage(svg_img);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    UnloadImage(svg_img);
    return texture;
}

const char* get_font_path(char* font_path) {
    return font_path[0] != '/' && font_path[1] != ':' ? into_data_path(font_path) : font_path;
}

GuiMeasurement scrap_gui_measure_image(void* image, int size) {
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

GuiMeasurement custom_measure(Font font, const char *text, float font_size) {
    GuiMeasurement ms = {0};

    if ((font.texture.id == 0) || !text) return ms;

    int size = TextLength(text);
    int codepoint = 0; // Current character
    int index = 0; // Index position in sprite font

    for (int i = 0; i < size;) {
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

GuiMeasurement scrap_gui_measure_text(void* font, const char* text, int size) {
    return custom_measure(*(Font*)font, text, size);
}

GuiColor as_gui_color(Color color) {
    return (GuiColor) { color.r, color.g, color.b, color.a };
}

Color as_gui_rl_color(GuiColor color) {
    return (Color) { color.r, color.g, color.b, color.a };
}

void setup(void) {
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
    load_blocks(&vm);

    mouse_blockchain = blockchain_new();
    draw_stack = vector_create();
    editor_code = vector_create();

    sidebar_init();

    term_init();

    gui = malloc(sizeof(Gui));
    gui_init(gui);
    gui_set_measure_text_func(gui, scrap_gui_measure_text);
    gui_set_measure_image_func(gui, scrap_gui_measure_image);
    gui_update_window_size(gui, GetScreenWidth(), GetScreenHeight());
    TraceLog(LOG_INFO, "Allocated %.2f KiB for gui", (float)sizeof(Gui) / 1024.0f);
}

int main(void) {
    set_default_config(&conf);
    load_config(&conf);

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Scrap");
    SetWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(conf.fps_limit);

    setup();
    SetWindowIcon(logo_img);

    while (!WindowShouldClose()) {
        scrap_gui_process_input();

        actionbar.show_time -= GetFrameTime();
        if (actionbar.show_time < 0) actionbar.show_time = 0;

        if (shader_time_loc != -1) SetShaderValue(line_shader, shader_time_loc, &shader_time, SHADER_UNIFORM_FLOAT);
        shader_time += GetFrameTime() / 2.0;
        if (shader_time >= 1.0) shader_time = 1.0;

        size_t vm_return = -1;
        if (exec_try_join(&vm, &exec, &vm_return)) {
            if (vm_return == 1) {
                actionbar_show("Vm executed successfully");
            } else if (vm_return == (size_t)PTHREAD_CANCELED) {
                actionbar_show("Vm stopped >:(");
            } else {
                actionbar_show("Vm shitted and died :(");
            }
            exec_free(&exec);
        } else if (vm.is_running) {
            hover_info.exec_chain = exec.running_chain;
            hover_info.exec_ind = exec.chain_stack[exec.chain_stack_len - 1].running_ind;
        }

        BeginDrawing();
        scrap_gui_process_render();
        EndDrawing();
    }

    if (vm.is_running) {
        exec_stop(&vm, &exec);
        size_t bin;
        exec_join(&vm, &exec, &bin);
        exec_free(&exec);
    }
    term_free();
    vector_free(draw_stack);
    blockchain_free(&mouse_blockchain);
    for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
        blockchain_free(&editor_code[i]);
    }
    vector_free(editor_code);
    for (vec_size_t i = 0; i < vector_size(sidebar.blocks); i++) {
        block_free(&sidebar.blocks[i]);
    }
    vector_free(sidebar.blocks);
    vm_free(&vm);
    free(gui);
    CloseWindow();

    return 0;
}
