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
#define AUX_STACK_SIZE 4096
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
    DRAWTYPE_RECT,
    DRAWTYPE_BORDER,
    DRAWTYPE_IMAGE,
    DRAWTYPE_TEXT,
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
    // 00GSFAAD
    // Where:
    //   D - GuiElementDirection
    //   A - GuiAlignmentType
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
    unsigned short state_len;

    GuiElement* child_elements_begin;
    GuiElement* child_elements_end;

    GuiElement* parent;
    GuiElement* parent_anchor;

    GuiElement* prev;
    GuiElement* next;
};

typedef GuiMeasurement (*GuiMeasureTextSliceFunc)(void* font, const char* text, unsigned int text_size, unsigned short font_size);
typedef GuiMeasurement (*GuiMeasureImageFunc)(void* image, unsigned short size);

struct Gui {
    GuiDrawCommand command_list[COMMAND_STACK_SIZE];
    size_t command_list_len; size_t command_list_iter;

    GuiDrawCommand rect_stack[AUX_STACK_SIZE];
    size_t rect_stack_len;

    GuiDrawCommand border_stack[AUX_STACK_SIZE];
    size_t border_stack_len;

    GuiDrawCommand image_stack[AUX_STACK_SIZE];
    size_t image_stack_len;

    GuiDrawCommand text_stack[AUX_STACK_SIZE];
    size_t text_stack_len;

    GuiElement elements_arena[ELEMENT_STACK_SIZE];
    size_t elements_arena_len;

    void* state_arena[STATE_STACK_SIZE];
    size_t state_arena_len;

    GuiBounds scissor_stack[COMMAND_STACK_SIZE];
    size_t scissor_stack_len;

    GuiMeasureTextSliceFunc measure_text;
    GuiMeasureImageFunc measure_image;

    GuiElement* current_element;

    unsigned short win_w, win_h;
    short mouse_x, mouse_y;
    int mouse_scroll;
};

#define GUI_GET_COMMANDS(gui, command) while (gui->command_list_iter < gui->command_list_len && (command = &gui->command_list[gui->command_list_iter++]))
#define TRANSPARENT (GuiColor) {0}
#define NO_BORDER TRANSPARENT, 0
#define NO_COLOR TRANSPARENT, NO_BORDER
#define SUBTYPE_DEFAULT 0

void gui_init(Gui* gui);
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
void gui_set_align(Gui* gui, GuiAlignmentType align);
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
void* gui_get_state(GuiElement* el, unsigned short* state_len);
GuiElement* gui_get_element(Gui* gui);

void gui_on_hover(Gui* gui, GuiHandler handler);
void gui_on_render(Gui* gui, GuiHandler handler);

void gui_text_slice(Gui* gui, void* font, const char* text, unsigned int text_size, unsigned short font_size, GuiColor color);
void gui_text(Gui* gui, void* font, const char* text, unsigned short size, GuiColor color);
void gui_image(Gui* gui, void* image, unsigned short size, GuiColor color);
void gui_grow(Gui* gui, GuiElementDirection direction);
void gui_spacer(Gui* gui, unsigned short w, unsigned short h);

#endif // SCRAP_GUI_H
