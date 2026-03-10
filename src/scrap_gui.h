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

#ifndef SCRAP_GUI_H
#define SCRAP_GUI_H

#include <stddef.h>

#define ELEMENT_STACK_SIZE 32768
#define COMMAND_STACK_SIZE 4096
#define STATE_STACK_SIZE 32768
typedef struct Gui Gui;
typedef struct GuiElement GuiElement;

typedef struct {
    unsigned short w, h;
} GuiMeasurement;

typedef struct {
    int x, y;
    unsigned short w, h;
} GuiBounds;

typedef struct {
    float x, y, w, h;
} GuiDrawBounds;

typedef struct {
    unsigned char r, g, b, a;
} GuiColor;

typedef struct {
    void* font;
    const char* text;
    unsigned int text_size;
} GuiTextData;

typedef enum {
    DRAWTYPE_UNKNOWN = 0,
    DRAWTYPE_RECT = 1,
    DRAWTYPE_BORDER = 2,
    DRAWTYPE_IMAGE = 3,
    DRAWTYPE_TEXT = 4,
    DRAWTYPE_SCISSOR_SET,
    DRAWTYPE_SCISSOR_RESET,
    DRAWTYPE_SHADER_BEGIN,
    DRAWTYPE_SHADER_END,
} GuiDrawType;

typedef union {
    GuiTextData text;
    void* image;
    void* custom_data;
    void* shader;
    unsigned int border_width;
} GuiDrawData;

typedef struct {
    unsigned char type; // DrawType
    unsigned char subtype;
    float pos_x, pos_y;
    float width, height;
    GuiColor color;
    GuiDrawData data;
} GuiDrawCommand;

typedef enum {
    ALIGN_LEFT = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT,
    ALIGN_TOP = 0,
    ALIGN_BOTTOM = 2,
} GuiAlignmentType;

typedef enum {
    SIZING_FIT = 0,
    SIZING_FIXED,
    SIZING_GROW,
    SIZING_PERCENT,
} GuiElementSizing;

typedef enum {
    DIRECTION_VERTICAL = 0,
    DIRECTION_HORIZONTAL,
} GuiElementDirection;

typedef void (*GuiHandler)(GuiElement* el);

struct GuiElement {
    int x, y;
    unsigned short w, h;
    unsigned short min_w, min_h;
    float abs_x, abs_y;
    int cursor_x, cursor_y;
    unsigned short pad_w, pad_h;
    unsigned short gap;
    float scaling;
    float size_percentage;
    unsigned char draw_type; // DrawType
    // Custom draw type, interpretation of this is defined by user
    // NOTE: Scrap gui always interprets 0 as no subtype, so don't do custom
    // rendering with this subtype value to not break things in unexpected way
    unsigned char draw_subtype;
    GuiDrawData data;
    GuiColor color;
    // Sizing layout:
    // YYYYXXXX
    // Where:
    //   X - ElementSizing for element width
    //   Y - ElementSizing for element height
    unsigned char sizing;
    // Anchor layout:
    // YYYYXXXX
    // Where:
    //   X - GuiAlignmentType for horizontal alignment
    //   Y - GuiAlignmentType for vertical alignment
    unsigned char anchor;
    // Flags layout:
    // XXGSFYYD
    // Where:
    //   D - GuiElementDirection
    //   X - GuiAlignmentType for horizontal alignment
    //   Y - GuiAlignmentType for vertical alignment
    //   F - Is floating element
    //   S - Is scissoring on
    //   G - Does this element need to be resized? (internal flag)
    //   0 - Unused
    unsigned char flags;
    GuiHandler handle_hover;
    GuiHandler handle_pre_render;
    int* scroll_value;
    short scroll_scaling;
    void* custom_data;
    void* custom_state;
    void* shader;

    GuiElement* child_elements_begin;
    GuiElement* child_elements_end;

    GuiElement* parent;
    GuiElement* parent_anchor;

    GuiElement* prev;
    GuiElement* next;
};

typedef GuiMeasurement (*GuiMeasureTextSliceFunc)(void* font, const char* text, unsigned int text_size, unsigned short font_size);
typedef GuiMeasurement (*GuiMeasureImageFunc)(void* image, unsigned short size);

typedef struct {
    size_t reserve_size, commit_size,
           pos, commit_pos;
} GuiMemArena;

typedef struct {
    GuiDrawCommand* items;
    size_t size, capacity;
} GuiDrawCommandList;

typedef struct {
    GuiBounds* items;
    size_t size, capacity;
} GuiScissorStack;

struct Gui {
    GuiMemArena* arena;

    size_t elements_count;

    GuiDrawCommandList command_list;
    GuiDrawCommandList aux_command_list;
    size_t command_list_iter,
           command_list_last_batch;

    GuiScissorStack scissor_stack;

    GuiMeasureTextSliceFunc measure_text;
    GuiMeasureImageFunc measure_image;

    GuiElement *root_element,
               *current_element;

    unsigned short win_w, win_h;
    short mouse_x, mouse_y;
    int mouse_scroll;
};

#define GUI_GET_COMMANDS(gui, command) for ( \
    gui->command_list_iter = 0; \
    command = gui->command_list.items[gui->command_list_iter], gui->command_list_iter < gui->command_list.size; \
    gui->command_list_iter++ \
)

#define GUI_BLACK (GuiColor) { 0x00, 0x00, 0x00, 0xff }
#define GUI_WHITE (GuiColor) { 0xff, 0xff, 0xff, 0xff }
#define GUI_SUBTYPE_DEFAULT 0

Gui gui_new(size_t arena_size);
void gui_free(Gui* gui);
void gui_begin(Gui* gui);
void gui_end(Gui* gui);
void gui_update_window_size(Gui* gui, unsigned short win_w, unsigned short win_h);
void gui_update_mouse_pos(Gui* gui, short mouse_x, short mouse_y);
void gui_update_mouse_scroll(Gui* gui, int mouse_scroll);
void gui_set_measure_text_func(Gui* gui, GuiMeasureTextSliceFunc measure_text);
void gui_set_measure_image_func(Gui* gui, GuiMeasureImageFunc measure_image);

GuiElement* gui_element_begin(Gui* gui);
void gui_element_end(Gui* gui);

void gui_set_fixed(Gui* gui, unsigned short w, unsigned short h);
void gui_set_fit(Gui* gui, GuiElementDirection direction);
void gui_set_grow(Gui* gui, GuiElementDirection diection);
void gui_set_percent_size(Gui* gui, float percentage, GuiElementDirection direction);
void gui_set_draw_subtype(Gui* gui, unsigned char subtype);
void gui_set_rect(Gui* gui, GuiColor color);
void gui_set_direction(Gui* gui, GuiElementDirection direction);
void gui_set_border(Gui* gui, GuiColor color, unsigned int border_width);
void gui_set_text_slice(Gui* gui, void* font, const char* text, unsigned int text_size, unsigned short font_size, GuiColor color);
void gui_set_text(Gui* gui, void* font, const char* text, unsigned short size, GuiColor color);
void gui_set_image(Gui* gui, void* image, unsigned short size, GuiColor color);
void gui_set_min_size(Gui* gui, unsigned short min_w, unsigned short min_h);
void gui_set_align(Gui* gui, GuiAlignmentType align_x, GuiAlignmentType align_y);
void gui_set_padding(Gui* gui, unsigned short pad_w, unsigned short pad_h);
void gui_set_gap(Gui* gui, unsigned short gap);
void gui_set_custom_data(Gui* gui, void* custom_data);
void gui_set_floating(Gui* gui);
void gui_set_scissor(Gui* gui);
void gui_set_position(Gui* gui, int x, int y);
void gui_set_anchor(Gui* gui, GuiAlignmentType anchor_x, GuiAlignmentType anchor_y);
void gui_set_parent_anchor(Gui* gui, GuiElement* parent_anchor);
void gui_set_scroll(Gui* gui, int* scroll_value);
void gui_set_scroll_scaling(Gui* gui, int scroll_scaling);
void gui_set_shader(Gui* gui, void* shader);
void gui_scale_element(Gui* gui, float scaling);
void* gui_set_state(Gui* gui, void* state, unsigned short state_len);
void* gui_get_state(GuiElement* el);
GuiElement* gui_get_element(Gui* gui);

void gui_on_hover(Gui* gui, GuiHandler handler);
void gui_on_render(Gui* gui, GuiHandler handler);

void gui_text_slice(Gui* gui, void* font, const char* text, unsigned int text_size, unsigned short font_size, GuiColor color);
void gui_text(Gui* gui, void* font, const char* text, unsigned short size, GuiColor color);
void gui_image(Gui* gui, void* image, unsigned short size, GuiColor color);
void gui_grow(Gui* gui, GuiElementDirection direction);
void gui_spacer(Gui* gui, unsigned short w, unsigned short h);

#define gui_arena_append(_arena, _list, _val) do { \
    if ((_list).size >= (_list).capacity) { \
        size_t _old_cap = (_list).capacity * sizeof(*(_list).items); \
        if ((_list).capacity == 0) (_list).capacity = 32; \
        else (_list).capacity *= 2; \
        (_list).items = gui_arena_realloc(_arena, (_list).items, _old_cap, (_list).capacity * sizeof(*(_list).items)); \
    } \
    (_list).items[(_list).size++] = (_val); \
} while (0)

GuiMemArena* gui_arena_new(size_t reserve_size, size_t commit_size);
void gui_arena_free(GuiMemArena* arena);
void* gui_arena_alloc(GuiMemArena* arena, size_t size);
void* gui_arena_realloc(GuiMemArena* arena, void* ptr, size_t old_size, size_t new_size);
const char* gui_arena_sprintf(GuiMemArena* arena, size_t max_size, const char* fmt, ...);
void gui_arena_pop(GuiMemArena* arena, size_t size);
void gui_arena_pop_to(GuiMemArena* arena, size_t pos);
void gui_arena_clear(GuiMemArena* arena);

#endif // SCRAP_GUI_H
