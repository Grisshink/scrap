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
    BORDER_NORMAL,
    BORDER_CONTROL,
    BORDER_CONTROL_BODY,
    BORDER_END,
    BORDER_CONTROL_END,
    BORDER_NOTCHED,
} GuiBorderType;

typedef enum {
    RECT_NORMAL = 0,
    RECT_NOTCHED,
    RECT_TERMINAL, // Terminal rendering is handled specially as it needs to synchronize with its buffer
} GuiRectType;

typedef struct {
    unsigned int width;
    GuiBorderType type;
} GuiBorderData;

typedef enum {
    DRAWTYPE_UNKNOWN = 0,
    DRAWTYPE_RECT,
    DRAWTYPE_BORDER,
    DRAWTYPE_IMAGE,
    DRAWTYPE_TEXT,
    DRAWTYPE_SCISSOR_BEGIN,
    DRAWTYPE_SCISSOR_END,
    DRAWTYPE_SHADER_BEGIN,
    DRAWTYPE_SHADER_END,
} GuiDrawType;

typedef union {
    GuiTextData text;
    void* image;
    void* custom_data;
    void* shader;
    GuiBorderData border;
    GuiRectType rect_type;
} GuiDrawData;

typedef struct {
    unsigned char type; // DrawType
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
    int abs_x, abs_y;
    int cursor_x, cursor_y;
    unsigned short pad_w, pad_h;
    unsigned short gap;
    float scaling;
    float size_percentage;
    unsigned char draw_type; // DrawType
    GuiDrawData data;
    GuiColor color;
    // Sizing layout:
    // YYYYXXXX
    // Where:
    //   X - ElementSizing for element width
    //   Y - ElementSizing for element height
    unsigned char sizing;
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
    unsigned short element_count;
    struct GuiElement* parent_anchor;
    struct GuiElement* next;
};

typedef GuiMeasurement (*GuiMeasureTextSliceFunc)(void* font, const char* text, unsigned int text_size, unsigned short font_size);
typedef GuiMeasurement (*GuiMeasureImageFunc)(void* image, unsigned short size);

struct Gui {
    GuiDrawCommand command_stack[COMMAND_STACK_SIZE];
    size_t command_stack_len; size_t command_stack_iter;

    GuiDrawCommand rect_stack[AUX_STACK_SIZE];
    size_t rect_stack_len;

    GuiDrawCommand border_stack[AUX_STACK_SIZE];
    size_t border_stack_len;

    GuiDrawCommand image_stack[AUX_STACK_SIZE];
    size_t image_stack_len;

    GuiDrawCommand text_stack[AUX_STACK_SIZE];
    size_t text_stack_len;

    GuiElement element_stack[ELEMENT_STACK_SIZE];
    size_t element_stack_len;

    GuiElement* element_ptr_stack[ELEMENT_STACK_SIZE];
    size_t element_ptr_stack_len;

    void* state_stack[STATE_STACK_SIZE];
    size_t state_stack_len;

    GuiBounds scissor_stack[COMMAND_STACK_SIZE];
    size_t scissor_stack_len;

    GuiMeasureTextSliceFunc measure_text;
    GuiMeasureImageFunc measure_image;

    unsigned short win_w, win_h;
    short mouse_x, mouse_y;
    int mouse_scroll;
};

#define GUI_GET_COMMANDS(gui, command) while (gui->command_stack_iter < gui->command_stack_len && (command = &gui->command_stack[gui->command_stack_iter++]))
#define TRANSPARENT (GuiColor) {0}
#define NO_BORDER TRANSPARENT, 0
#define NO_COLOR TRANSPARENT, NO_BORDER

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
void gui_set_fit(Gui* gui);
void gui_set_grow(Gui* gui, GuiElementDirection diection);
void gui_set_percent_size(Gui* gui, float percentage, GuiElementDirection direction);
void gui_set_rect(Gui* gui, GuiColor color);
void gui_set_rect_type(Gui* gui, GuiRectType type);
void gui_set_direction(Gui* gui, GuiElementDirection direction);
void gui_set_border(Gui* gui, GuiColor color, unsigned int border_width);
void gui_set_border_type(Gui* gui, GuiBorderType type);
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
void gui_set_anchor(Gui* gui, GuiElement* anchor);
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
