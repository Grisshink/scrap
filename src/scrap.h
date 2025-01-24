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

typedef struct PanelTree PanelTree;

typedef struct {
    int font_size;
    int side_bar_size;
    int fps_limit;
    int block_size_threshold;
    char* font_path;
    char* font_bold_path;
    char* font_mono_path;
} Config;

typedef bool (*ButtonClickHandler)(void);

typedef enum {
    SPLIT_SIDE_NONE = 0,
    SPLIT_SIDE_TOP,
    SPLIT_SIDE_BOTTOM,
    SPLIT_SIDE_LEFT,
    SPLIT_SIDE_RIGHT,
} SplitSide;

typedef struct {
    SplitSide side;
} SplitPreview;

typedef enum {
    PANEL_NONE = 0,
    PANEL_SPLIT,
    PANEL_SIDEBAR,
    PANEL_CODE,
    PANEL_TERM,
} PanelType;

struct PanelTree {
    PanelType type;
    GuiElementDirection direction;
    struct PanelTree* parent;
    float split_percent;
    struct PanelTree* left; // Becomes top when direction is DIRECTION_VERTICAL
    struct PanelTree* right; // Becomes bottom when direction is DIRECTION_VERTICAL
};

typedef struct {
    ButtonClickHandler handler;
    Vector2 pos;
} TopBars;

typedef enum {
    EDITOR_NONE,
    EDITOR_BLOCKDEF,
    EDITOR_EDIT,
    EDITOR_ADD_ARG,
    EDITOR_DEL_ARG,
    EDITOR_ADD_TEXT,
} EditorHoverPart;

typedef enum {
    LOCATION_NONE = 0,
    LOCATION_FILE_MENU,
    LOCATION_BLOCK_DROPDOWN,
} DropdownLocations;

typedef struct {
    EditorHoverPart part;
    ScrBlockdef* edit_blockdef;
    ScrBlock* edit_block;
    ScrBlockdef* prev_blockdef;
    ScrBlockdef* blockdef;
    size_t blockdef_input;
} EditorHoverInfo;

typedef struct {
    DropdownLocations location;
    GuiElement* element;
    ButtonClickHandler handler;
    char** list;
    int list_len;
    int select_ind;
    int scroll_amount;
} DropdownHoverInfo;

typedef struct {
    int min;
    int max;
    int* value;
    char value_str[16]; // Used to store value as string as gui does not store strings
} SliderHoverInfo;

typedef struct {
    bool is_panel_edit_mode;
    bool drag_cancelled;

    ScrBlockChain* prev_blockchain;
    ScrBlockChain* blockchain;

    ScrBlock* prev_block;
    ScrBlock* block;
    ScrArgument* argument;
    ScrArgument* prev_argument;

    ScrBlock* select_block;
    ScrArgument* select_argument;

    char** input;
    char** select_input;

    Vector2 mouse_click_pos;
    float time_at_last_pos;

    ScrBlockChain* exec_chain;
    size_t exec_ind;

    TopBars top_bars;
    EditorHoverInfo editor;

    int tab;

    DropdownHoverInfo dropdown;
    SliderHoverInfo hover_slider;
    SliderHoverInfo dragged_slider;
    int slider_last_val;

    PanelTree* panel;
    Rectangle panel_size;
    PanelTree* drag_panel;
    Rectangle drag_panel_size;
    PanelType mouse_panel;
    PanelTree* prev_panel;
    SplitSide panel_side;
} HoverInfo;

typedef struct {
    int scroll_amount;
} Dropdown;

typedef struct {
    char* name;
    PanelTree* root_panel;
} Tab;

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
    ScrBlock* blocks;
} Sidebar;

typedef enum {
    GUI_TYPE_SETTINGS,
    GUI_TYPE_ABOUT,
    GUI_TYPE_FILE,
} WindowGuiType;

typedef struct {
    struct timespec start;
    const char* name;
} Timer;

extern Config conf;
extern Config window_conf;
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
extern Texture2D arrow_left_tex;
extern Texture2D arrow_right_tex;

extern ScrVm vm;
extern Vector2 camera_pos;
extern ActionBar actionbar;
extern BlockCode block_code;
extern Dropdown dropdown;
extern Sidebar sidebar;
extern ScrBlockChain* editor_code;
extern ScrBlockChain mouse_blockchain;
extern ScrExec exec;
extern Gui* gui;

extern SplitPreview split_preview;
extern Tab* code_tabs;
extern int current_tab;

#ifdef DEBUG
extern double ui_time;
#endif

extern char debug_buffer[DEBUG_BUFFER_LINES][DEBUG_BUFFER_LINE_SIZE];

extern Vector2 camera_click_pos;
extern Vector2 camera_pos;

extern float shader_time;
extern int blockchain_select_counter;

extern char* top_bar_buttons_text[3];
extern char* tab_bar_buttons_text[2];

extern char project_name[1024];

extern const int codepoint_regions[CODEPOINT_REGION_COUNT][2];
extern int codepoint_start_ranges[CODEPOINT_REGION_COUNT];

// scrap.c
void sanitize_links(void);
GuiMeasurement custom_measure(Font font, const char *text, float font_size);
void panel_split(PanelTree* panel, SplitSide side, PanelType new_panel_type, float split_percent);
void delete_all_tabs(void);
size_t tab_new(char* name, PanelTree* root_panel);
void tab_delete(size_t tab);
void init_panels(void);
PanelTree* panel_new(PanelType type);
void panel_delete(PanelTree* panel);
void tab_insert(char* name, PanelTree* root_panel, size_t position);

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
bool handle_file_button_click(void);
bool handle_settings_button_click(void);
bool handle_about_button_click(void);
bool handle_run_button_click(void);
bool handle_stop_button_click(void);
bool handle_code_tab_click(void);
bool handle_output_tab_click(void);
bool handle_window_gui_close_button_click(void);
bool handle_settings_reset_button_click(void);
bool handle_settings_apply_button_click(void);
bool handle_about_license_button_click(void);
bool handle_dropdown_close(void);
bool handle_file_menu_click(void);
bool handle_editor_close_button(void);
bool handle_editor_edit_button(void);
bool handle_editor_add_arg_button(void);
bool handle_editor_add_text_button(void);
bool handle_editor_del_arg_button(void);
bool handle_settings_panel_editor_button_click(void);
bool handle_panel_editor_save_button(void);
bool handle_panel_editor_cancel_button(void);
bool handle_tab_button(void);
bool handle_add_tab_button(void);

// util.c
int leading_ones(unsigned char byte);
const char* into_data_path(const char* path);
ScrBlock block_new_ms(ScrBlockdef* blockdef);
Timer start_timer(const char* name);
double end_timer(Timer timer);

// save.c
void set_default_config(Config* config);
void apply_config(Config* dst, Config* src);
void save_config(Config* config);
void load_config(Config* config);
void save_code(const char* file_path, ScrBlockChain* code);
ScrBlockChain* load_code(const char* file_path);
void config_new(Config* config);
void config_free(Config* config);
void config_copy(Config* dst, Config* src);

// window.c
void init_gui_window(void);
void gui_window_show(WindowGuiType type);
void gui_window_hide(void);
void gui_window_hide_immediate(void);
WindowGuiType gui_window_get_type(void);
bool gui_window_is_shown(void);
void handle_window(void);

// blocks.c
void load_blocks(ScrVm* vm);

#endif // SCRAP_H
