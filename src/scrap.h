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

#ifndef SCRAP_H
#define SCRAP_H

#include <time.h>

#include "ast.h"
#include "raylib.h"
#include "config.h"
#include "scrap_gui.h"
#include "util.h"
#include "term.h"

typedef struct Vm Vm;

#ifdef USE_INTERPRETER
#include "interpreter.h"
#else
#include "compiler.h"
#endif

typedef struct PanelTree PanelTree;
typedef struct BlockCategory BlockCategory;

typedef enum {
    LANG_SYSTEM = 0,
    LANG_EN,
    LANG_RU,
    LANG_KK,
    LANG_UK,
} Language;

typedef struct {
    int ui_size;
    int fps_limit;
    int block_size_threshold;
    Language language;
    char* font_path;
    char* font_bold_path;
    char* font_mono_path;
    bool show_blockchain_previews;
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

typedef enum {
    DROPDOWN_LIST,
    DROPDOWN_COLOR_PICKER,
} DropdownType;

typedef struct {
    float hue, saturation, value;
} HSV;

typedef enum {
    COLOR_PICKER_NONE = 0,
    COLOR_PICKER_SV,
    COLOR_PICKER_SPECTRUM,
} ColorPickerPartType;

typedef struct {
    char** data;
    int len;

    int select_ind;
    int scroll;
} ListDropdown;

typedef struct {
    ColorPickerPartType hover_part, select_part;
    HSV color;
    Color* edit_color;
    char color_hex[10];
} ColorPickerDropdown;

typedef struct {
    bool shown;
    void* ref_object;
    GuiElement* element;
    ButtonClickHandler handler;

    DropdownType type;
    union {
        ListDropdown list;
        ColorPickerDropdown color_picker;
    } as;
} Dropdown;

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

typedef enum {
    CATEGORY_ITEM_CHAIN,
    CATEGORY_ITEM_LABEL,
} BlockCategoryItemType;

typedef struct {
    BlockCategoryItemType type;
    union {
        BlockChain chain;
        struct {
            const char* text;
            Color color;
        } label;
    } data;
} BlockCategoryItem;

struct BlockCategory {
    const char* name;
    Color color;
    BlockCategoryItem* items;

    BlockCategory* next;
    BlockCategory* prev;
};

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
    Texture2D icon_about;
    Texture2D icon_file;
    Texture2D icon_folder;
    Texture2D icon_list;
    Texture2D icon_logo;
    Texture2D icon_pi;
    Texture2D icon_settings;
    Texture2D icon_special;
    Texture2D icon_term;
    Texture2D icon_variable;
    Texture2D icon_warning;
    Texture2D spectrum;
} TextureList;

typedef struct {
    Font font_cond;
    Font font_cond_shadow;
    Font font_eb;
    Font font_mono;
} Fonts;

typedef struct {
    Fonts fonts;
    TextureList textures;
    Shader line_shader;
    Shader gradient_shader;
} Assets;

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
    BlockCategory* current_category;
    BlockCategory* categories_start;
    BlockCategory* categories_end;
} BlockPalette;

typedef void (*WindowGuiRenderFunc)(void);

typedef struct {
    char project_name[1024];
    bool project_modified;

    Tab* tabs;

    Vector2 camera_pos;
    Vector2 camera_click_pos;

    BlockChain* code;
    BlockPalette palette;

    char* search_list_search;
    Blockdef** search_list;
    Vector2 search_list_pos;

    ActionBar actionbar;
    BlockChain mouse_blockchain;
    SplitPreview split_preview;
    int* blockchain_render_layer_widths;
    int current_tab;
    int blockchain_select_counter;

    char debug_buffer[DEBUG_BUFFER_LINES][DEBUG_BUFFER_LINE_SIZE];
    bool show_debug;
} Editor;

typedef struct {
    bool scrap_running;

    RenderTexture2D render_surface;
    bool render_surface_needs_redraw;
    bool render_surface_redraw_next;

    int shader_time_loc;
    float shader_time;

    HoverInfo hover;

    int categories_scroll;
    int search_list_scroll;

    Dropdown dropdown;

#ifdef DEBUG
    double ui_time;
#endif
} UI;

struct Vm {
    Blockdef** blockdefs;
    size_t end_blockdef;

    Thread thread;

    Exec exec;
    char** compile_error;
    Block* compile_error_block;
    BlockChain* compile_error_blockchain;

    int start_timeout; // = -1;
#ifndef USE_INTERPRETER
    CompilerMode start_mode; // = COMPILER_MODE_JIT;
#endif
};

extern Config config;
extern Config window_config;
extern ProjectConfig project_config;

extern Assets assets;

extern Vm vm;
extern Gui* gui;

extern Editor editor;
extern UI ui;

extern char* language_list[5];
extern const int codepoint_regions[CODEPOINT_REGION_COUNT][2];
extern int codepoint_start_ranges[CODEPOINT_REGION_COUNT];

// scrap.c
// Nothing...

// render.c
void actionbar_show(const char* text);
void process_render(void);
void prerender_font_shadow(Font* font);
void scrap_gui_process_render(void);
void scrap_gui_process(void);
bool svg_load(const char* file_name, size_t width, size_t height, Image* out_image);
const char* sgettext(const char* msgid);
void input_on_hover(GuiElement* el);
void draw_input_text(Font* font, char** input, const char* hint, unsigned short font_size, GuiColor font_color);

// input.c
void scrap_gui_process_ui(void);

PanelTree* find_panel(PanelTree* root, PanelType panel);
void update_search(void);
void show_list_dropdown(char** list, int list_len, void* ref_object, ButtonClickHandler handler);

GuiMeasurement scrap_gui_measure_image(void* image, unsigned short size);
GuiMeasurement scrap_gui_measure_text(void* font, const char* text, unsigned int text_size, unsigned short font_size);
TermVec term_measure_text(void* font, const char* text, unsigned int text_size, unsigned short font_size);
int search_glyph(int codepoint);

size_t tab_new(char* name, PanelTree* root_panel);
void delete_all_tabs(void);

void init_panels(void);
PanelTree* panel_new(PanelType type);
void panel_split(PanelTree* panel, SplitSide side, PanelType new_panel_type, float split_percent);
void panel_delete(PanelTree* panel);

bool save_project(void);

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
bool handle_color_picker_click(void);
bool handle_editor_color_button(void);

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

const char* get_locale_path(void);
const char* get_shared_dir_path(void);
const char* into_shared_dir_path(const char* path);

void reload_fonts(void);

// window.c
void init_gui_window(void);
void gui_window_show(WindowGuiRenderFunc func);
void gui_window_hide(void);
void gui_window_hide_immediate(void);
WindowGuiRenderFunc gui_window_get_render_func(void);
bool gui_window_is_shown(void);
void handle_window(void);
void draw_window(void);

void draw_settings_window(void);
void draw_project_settings_window(void);
void draw_about_window(void);
void draw_save_confirmation_window(void);

// blocks.c
void register_blocks(Vm* vm);

#ifdef USE_INTERPRETER
bool block_custom_arg(Exec* exec, Block* block, int argc, AnyValue* argv, AnyValue* return_val, ControlState control_state);
bool block_exec_custom(Exec* exec, Block* block, int argc, AnyValue* argv, AnyValue* return_val, ControlState control_state);
#else
bool block_custom_arg(Exec* exec, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state);
bool block_exec_custom(Exec* exec, Block* block, int argc, FuncArg* argv, FuncArg* return_val, ControlState control_state);
#endif

// vm.c
BlockCategory block_category_new(const char* name, Color color);
BlockCategory* block_category_register(BlockCategory category);
void block_category_add_blockdef(BlockCategory* category, Blockdef* blockdef);
void block_category_add_label(BlockCategory* category, const char* label, Color color);

size_t blockdef_register(Vm* vm, Blockdef* blockdef);
void blockdef_unregister(Vm* vm, size_t block_id);

void unregister_categories(void);

Vm vm_new(void);
void vm_free(Vm* vm);
#ifdef USE_INTERPRETER
bool vm_start(void);
#else
bool vm_start(CompilerMode mode);
#endif
bool vm_stop(void);
void vm_handle_running_thread(void);

void clear_compile_error(void);
Block block_new_ms(Blockdef* blockdef);

// platform.c
void scrap_set_env(const char* name, const char* value);

#ifndef USE_INTERPRETER
bool spawn_process(char* command, char* error, size_t error_len);
#endif

#endif // SCRAP_H
