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

#ifndef SCRAP_H
#define SCRAP_H

#include "vm.h"
#include "raylib.h"
#include "config.h"
#include "scrap_gui.h"

typedef struct {
    int font_size;
    int side_bar_size;
    int fps_limit;
    int block_size_threshold;
    char font_path[FONT_PATH_MAX_SIZE];
    char font_bold_path[FONT_PATH_MAX_SIZE];
    char font_mono_path[FONT_PATH_MAX_SIZE];
} Config;

typedef enum {
    TOPBAR_TOP,
    TOPBAR_TABS,
    TOPBAR_RUN_BUTTON,
} TopBarType;

typedef struct {
    TopBarType type;
    Vector2 pos;
    int ind;
} TopBars;

typedef enum {
    EDITOR_NONE,
    EDITOR_BLOCKDEF,
    EDITOR_EDIT,
    EDITOR_ADD_ARG,
    EDITOR_DEL_ARG,
    EDITOR_ADD_TEXT,
} EditorHoverPart;

typedef struct {
    EditorHoverPart part;
    ScrBlockdef* edit_blockdef;
    ScrBlock* edit_block;
    ScrBlockdef* blockdef;
    size_t blockdef_input;
} EditorHoverInfo;

typedef struct {
    bool sidebar;
    bool drag_cancelled;

    ScrBlockChain* blockchain;
    //size_t blockchain_index;
    int blockchain_layer;

    ScrBlock* block;
    ScrArgument* argument;
    Vector2 argument_pos;
    ScrArgument* prev_argument;

    ScrBlock* select_block;
    ScrArgument* select_argument;
    Vector2 select_argument_pos;

    char** input;
    char** select_input;

    Vector2 last_mouse_pos;
    Vector2 mouse_click_pos;
    float time_at_last_pos;

    int dropdown_hover_ind;

    ScrBlockChain* exec_chain;
    size_t exec_ind;

    TopBars top_bars;
    EditorHoverInfo editor;
} HoverInfo;

typedef enum {
    TAB_CODE,
    TAB_OUTPUT,
} TabType;

typedef struct {
    ScrMeasurement ms;
    int scroll_amount;
} Dropdown;

typedef struct {
    float show_time;
    char text[ACTION_BAR_MAX_SIZE];
} ActionBar;

typedef struct {
    Vector2 min_pos;
    Vector2 max_pos;
} BlockCode;

typedef struct {
    int scroll_amount;
    int max_y;
    ScrBlock* blocks;
} Sidebar;

typedef enum {
    GUI_TYPE_SETTINGS,
    GUI_TYPE_ABOUT,
    GUI_TYPE_FILE,
} NuklearGuiType;

typedef struct {
    ScrVec pos;
    ScrBlock* block;
} DrawStack;

typedef struct {
    struct timespec start;
    const char* name;
} Timer;

extern Config conf;
extern HoverInfo hover_info;
extern Shader line_shader;

extern Font font_cond;
extern Font font_cond_shadow;
extern Font font_eb;
extern Font font_mono;

extern Texture2D run_tex;
extern Texture2D stop_tex;
extern Texture2D drop_tex;
extern Texture2D close_tex;
extern Texture2D logo_tex;
extern Texture2D warn_tex;
extern Texture2D edit_tex;
extern Texture2D close_tex;
extern Texture2D term_tex;
extern Texture2D add_arg_tex;
extern Texture2D del_arg_tex;
extern Texture2D add_text_tex;
extern Texture2D special_tex;
extern Texture2D list_tex;

extern TabType current_tab;
extern ScrVm vm;
extern Vector2 camera_pos;
extern ActionBar actionbar;
extern BlockCode block_code;
extern Dropdown dropdown;
extern Sidebar sidebar;
extern ScrBlockChain* editor_code;
extern DrawStack* draw_stack;
extern ScrBlockChain mouse_blockchain;
extern ScrExec exec;
extern Gui* gui;

extern Vector2 camera_click_pos;
extern Vector2 camera_pos;

extern float shader_time;
extern int blockchain_select_counter;

extern char* top_bar_buttons_text[3];
extern char* tab_bar_buttons_text[2];

extern const int codepoint_regions[CODEPOINT_REGION_COUNT][2];
extern int codepoint_start_ranges[CODEPOINT_REGION_COUNT];

// scrap.c
void blockcode_add_blockchain(BlockCode* blockcode, ScrBlockChain chain);
void blockcode_remove_blockchain(BlockCode* blockcode, size_t ind);
void sanitize_links(void);

// render.c
void sidebar_init(void);
void actionbar_show(const char* text);
void process_render(void);
void prerender_font_shadow(Font* font);
void scrap_gui_process_render(void);
void scrap_gui_process(void);

// input.c
void process_input(void);
void scrap_gui_process_input(void);

// util.c
ScrVec as_scr_vec(Vector2 vec);
Vector2 as_rl_vec(ScrVec vec);
Color as_rl_color(ScrColor color);
int leading_ones(unsigned char byte);
const char* into_data_path(const char* path);
ScrBlock block_new_ms(ScrBlockdef* blockdef);
Timer start_timer(const char* name);
void end_timer(Timer timer);

// measure.c
void blockdef_update_measurements(ScrBlockdef* blockdef, bool editing);
void update_measurements(ScrBlock* block, ScrPlacementStrategy placement);
void blockcode_update_measurments(BlockCode* blockcode);

// save.c
void set_default_config(Config* config);
void apply_config(Config* dst, Config* src);
void save_config(Config* config);
void load_config(Config* config);
void save_code(const char* file_path, ScrBlockChain* code);
ScrBlockChain* load_code(const char* file_path);

// gui.c
// void init_gui(void);
// void gui_show(NuklearGuiType type);
// void gui_hide_immediate(void);
// NuklearGuiType gui_get_type(void);
// bool gui_is_shown(void);
// void update_gui(void);
// void draw_gui(void);
// void gui_free(void);

// blocks.c
void load_blocks(ScrVm* vm);

#endif // SCRAP_H
