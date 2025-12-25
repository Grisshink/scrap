// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-2025 Grisshink
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

#ifndef SCRAP_H
#define SCRAP_H

#include <time.h>

#include "ast.h"
#include "raylib.h"
#include "config.h"
#include "scrap_gui.h"
#include "util.h"

typedef struct Vm Vm;

typedef enum {
    EXEC_STATE_NOT_RUNNING = 0,
    EXEC_STATE_STARTING,
    EXEC_STATE_RUNNING,
    EXEC_STATE_DONE,
} ExecState;

#ifdef USE_INTERPRETER
#include "interpreter.h"
#else
#include "compiler.h"
#endif

typedef struct PanelTree PanelTree;

typedef enum {
    LANG_SYSTEM = 0,
    LANG_EN,
    LANG_RU,
    LANG_KK,
    LANG_UK,
} Language;

typedef struct {
    int font_size;
    int fps_limit;
    int block_size_threshold;
    Language language;
    char* font_path;
    char* font_bold_path;
    char* font_mono_path;
} Config;

typedef struct {
    char* executable_name;
    char* linker_name;
} ProjectConfig;

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
    PANEL_BLOCK_PALETTE,
    PANEL_CODE,
    PANEL_TERM,
    PANEL_BLOCK_CATEGORIES,
} PanelType;

struct PanelTree {
    PanelType type;
    GuiElementDirection direction;
    struct PanelTree* parent;
    float split_percent;
    struct PanelTree* left; // Becomes top when direction is DIRECTION_VERTICAL
    struct PanelTree* right; // Becomes bottom when direction is DIRECTION_VERTICAL
};

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
    LOCATION_SETTINGS,
} DropdownLocations;

typedef struct {
    BlockChain* prev_blockchain;
    BlockChain* blockchain;

    Block* prev_block;
    Block* block;
    Argument* argument;
    Argument* prev_argument;
    Argument* parent_argument;

    Block* select_block;
    Argument* select_argument;
    BlockChain* select_blockchain;
    Vector2 select_block_pos;
    bool select_valid;

    EditorHoverPart part;
    Blockdef* edit_blockdef;
    Block* edit_block;
    Blockdef* prev_blockdef;
    Blockdef* blockdef;
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
    int* value;
    char** list;
    int list_len;
} DropdownData;

typedef struct {
    int min;
    int max;
    int* value;
    char value_str[16]; // Used to store value as string as gui does not store strings
} SliderHoverInfo;

typedef struct {
    char** input;
    Vector2 rel_pos;
    Font* font;
    float font_size;
} InputHoverInfo;

typedef struct {
    const char* name;
    Color color;
    BlockChain* chains;
} BlockCategory;

typedef struct {
    PanelTree* panel;
    Rectangle panel_size;

    PanelTree* drag_panel;
    Rectangle drag_panel_size;

    PanelType mouse_panel;
    PanelTree* prev_panel;
    SplitSide panel_side;

    Rectangle code_panel_bounds;
} PanelHoverInfo;

typedef struct {
    ButtonClickHandler handler;
    void* data;
} ButtonHoverInfo;

typedef struct {
    bool is_panel_edit_mode;
    bool drag_cancelled;

    BlockCategory* category;

    InputHoverInfo input_info;
    char** select_input;
    int select_input_cursor;
    int select_input_mark;

    Vector2 mouse_click_pos;
    float time_at_last_pos;

    EditorHoverInfo editor;
    PanelHoverInfo panels;
    ButtonHoverInfo button;

    DropdownHoverInfo dropdown;
    SliderHoverInfo hover_slider;
    SliderHoverInfo dragged_slider;
    int slider_last_val;

    DropdownData settings_dropdown_data;
    int* select_settings_dropdown_value;
} HoverInfo;

typedef struct {
    Texture2D button_add_arg;
    Texture2D button_add_text;
    Texture2D button_arrow_left;
    Texture2D button_arrow_right;
    Texture2D button_build;
    Texture2D button_close;
    Texture2D button_del_arg;
    Texture2D button_edit;
    Texture2D button_run;
    Texture2D button_stop;
    Texture2D dropdown;
    Texture2D icon_list;
    Texture2D icon_logo;
    Texture2D icon_pi;
    Texture2D icon_special;
    Texture2D icon_term;
    Texture2D icon_variable;
    Texture2D icon_warning;
} TextureList;

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
    int current_category;
    BlockCategory* categories;
} BlockPalette;

typedef enum {
    GUI_TYPE_SETTINGS,
    GUI_TYPE_ABOUT,
    GUI_TYPE_FILE,
    GUI_TYPE_PROJECT_SETTINGS,
} WindowGuiType;

struct Vm {
    Blockdef** blockdefs;
    // TODO: Maybe remove end_blockdef from here
    size_t end_blockdef;
    bool is_running;

    BlockChain* exec_chain;
    size_t exec_ind;
    BlockChain* prev_exec_chain;
    size_t prev_exec_ind;
};

extern Config conf;
extern Config window_conf;
extern ProjectConfig project_conf;
extern HoverInfo hover;
extern Shader line_shader;
extern RenderTexture2D render_surface;
extern bool render_surface_needs_redraw;

extern Font font_cond;
extern Font font_cond_shadow;
extern Font font_eb;
extern Font font_mono;

extern TextureList textures;

extern Exec exec;
extern char** exec_compile_error;
extern Block* exec_compile_error_block;
extern BlockChain* exec_compile_error_blockchain;

extern Vm vm;
extern int start_vm_timeout;
#ifndef USE_INTERPRETER
extern CompilerMode start_vm_mode;
#endif
extern Vector2 camera_pos;
extern ActionBar actionbar;
extern BlockCode block_code;
extern Dropdown dropdown;
extern BlockPalette palette;
extern BlockChain* editor_code;
extern Block** search_list;
extern BlockChain mouse_blockchain;
extern Gui* gui;
extern char* search_list_search;
extern int categories_scroll;
extern int search_list_scroll;
extern Vector2 search_list_pos;

#ifdef RAM_OVERLOAD
extern int* overload;
extern pthread_t overload_thread;
#endif

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
extern char* language_list[5];

extern const int codepoint_regions[CODEPOINT_REGION_COUNT][2];
extern int codepoint_start_ranges[CODEPOINT_REGION_COUNT];

// scrap.c
void sanitize_links(void);
GuiMeasurement measure_slice(Font font, const char *text, unsigned int text_size, float font_size);
int search_glyph(int codepoint);
void panel_split(PanelTree* panel, SplitSide side, PanelType new_panel_type, float split_percent);
void delete_all_tabs(void);
size_t tab_new(char* name, PanelTree* root_panel);
void tab_delete(size_t tab);
void init_panels(void);
PanelTree* panel_new(PanelType type);
void panel_delete(PanelTree* panel);
void tab_insert(char* name, PanelTree* root_panel, size_t position);
BlockCategory block_category_new(const char* name, Color color);
void block_category_free(BlockCategory* category);
Vm vm_new(void);
void vm_free(Vm* vm);
size_t blockdef_register(Vm* vm, Blockdef* blockdef);
void blockdef_unregister(Vm* vm, size_t id);
void clear_compile_error(void);

// render.c
void actionbar_show(const char* text);
void process_render(void);
void prerender_font_shadow(Font* font);
void scrap_gui_process_render(void);
void scrap_gui_process(void);
void draw_input(Font* font, char** input, const char* hint, unsigned short font_size, GuiColor font_color, bool editable);
bool svg_load(const char* file_name, size_t width, size_t height, Image* out_image);

// input.c
void process_input(void);
void scrap_gui_process_input(void);
bool handle_file_button_click(void);
bool handle_settings_button_click(void);
bool handle_about_button_click(void);
bool handle_run_button_click(void);
bool handle_build_button_click(void);
bool handle_stop_button_click(void);
bool handle_code_tab_click(void);
bool handle_output_tab_click(void);
bool handle_dropdown_close(void);
bool handle_file_menu_click(void);
bool handle_editor_close_button(void);
bool handle_editor_edit_button(void);
bool handle_editor_add_arg_button(void);
bool handle_editor_add_text_button(void);
bool handle_editor_del_arg_button(void);
bool handle_panel_editor_save_button(void);
bool handle_panel_editor_cancel_button(void);
bool handle_tab_button(void);
bool handle_add_tab_button(void);
bool handle_category_click(void);
bool handle_jump_to_block_button_click(void);
bool handle_error_window_close_button_click(void);
PanelTree* find_panel(PanelTree* root, PanelType panel);
void update_search(void);
Block block_new_ms(Blockdef* blockdef);
void show_dropdown(DropdownLocations location, char** list, int list_len, ButtonClickHandler handler);
#ifdef USE_INTERPRETER
bool start_vm(void);
#else
bool start_vm(CompilerMode mode);
#endif

// save.c
void config_new(Config* config);
void config_free(Config* config);
void set_default_config(Config* config);
void apply_config(Config* dst, Config* src);
void save_config(Config* config);
void load_config(Config* config);
void config_copy(Config* dst, Config* src);

void save_code(const char* file_path, ProjectConfig* config, BlockChain* code);
BlockChain* load_code(const char* file_path, ProjectConfig* out_config);

void project_config_new(ProjectConfig* config);
void project_config_free(ProjectConfig* config);
void project_config_set_default(ProjectConfig* config);

const char* language_to_code(Language lang);
Language code_to_language(const char* code);

// window.c
void init_gui_window(void);
void gui_window_show(WindowGuiType type);
void gui_window_hide(void);
void gui_window_hide_immediate(void);
WindowGuiType gui_window_get_type(void);
bool gui_window_is_shown(void);
void handle_window(void);
void draw_window(void);

// blocks.c
void register_blocks(Vm* vm);
void register_categories(void);
void unregister_categories(void);

#ifdef USE_INTERPRETER
bool block_custom_arg(Exec* exec, Block* block, int argc, AnyValue* argv, AnyValue* return_val, ControlState control_state);
bool block_exec_custom(Exec* exec, Block* block, int argc, AnyValue* argv, AnyValue* return_val, ControlState control_state);
#else
bool block_custom_arg(Exec* exec, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state);
bool block_exec_custom(Exec* exec, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state);
#endif

// platform.c
void scrap_set_env(const char* name, const char* value);

#if defined(RAM_OVERLOAD) && defined(_WIN32)
bool should_do_ram_overload(void);
#endif

#ifndef USE_INTERPRETER
bool spawn_process(char* command, char* error, size_t error_len);
#endif

#endif // SCRAP_H
