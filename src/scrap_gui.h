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
#define COMMAND_STACK_SIZE 8192
typedef struct Gui Gui;
typedef struct FlexElement FlexElement;

typedef struct {
    int w, h;
} GuiMeasurement;

typedef struct {
    unsigned char r, g, b, a;
} GuiColor;

typedef struct {
    void* font;
    const char* text;
} GuiTextData;

typedef enum {
    DRAWTYPE_UNKNOWN = 0,
    DRAWTYPE_RECT,
    DRAWTYPE_TEXT,
    DRAWTYPE_BORDER,
    DRAWTYPE_IMAGE,
    DRAWTYPE_SCISSOR_BEGIN,
    DRAWTYPE_SCISSOR_END,
} DrawType;

typedef union {
    GuiTextData text;
    void* image;
    void* custom_data;
    int border_width;
} DrawData;

typedef struct {
    DrawType type;
    int pos_x, pos_y;
    int width, height;
    GuiColor color;
    DrawData data;
} DrawCommand;

typedef enum {
    ALIGN_LEFT = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT,
    ALIGN_TOP = 0,
    ALIGN_BOTTOM = 2,
} AlignmentType;

typedef enum {
    LAYOUT_STATIC,
    LAYOUT_VERTICAL,
    LAYOUT_HORIZONTAL,
    LAYOUT_FIXED,
} LayoutType;

typedef enum {
    SIZING_FIXED,
    SIZING_FIT,
    SIZING_GROW,
} ElementSizing;

typedef enum {
    DIRECTION_HORIZONTAL,
    DIRECTION_VERTICAL,
} FlexDirection;

typedef void (*HoverHandler)(FlexElement* el);

struct FlexElement {
    int x, y, w, h;
    int cursor_x, cursor_y;
    int pad_w, pad_h;
    int gap;
    DrawType draw_type;
    DrawData data;
    GuiColor color;
    ElementSizing sizing_x;
    ElementSizing sizing_y;
    FlexDirection direction;
    AlignmentType align;
    HoverHandler handle_hover;
    unsigned char is_floating;
    void* custom_data;
    int element_count;
    struct FlexElement* next;
};

typedef GuiMeasurement (*MeasureTextFunc)(void* font, const char* text, int size);
typedef GuiMeasurement (*MeasureImageFunc)(void* image, int size);

struct Gui {
    DrawCommand command_stack[COMMAND_STACK_SIZE];
    size_t command_stack_len;
    size_t command_stack_iter;

    FlexElement element_stack[ELEMENT_STACK_SIZE];
    size_t element_stack_len;

    FlexElement* element_ptr_stack[ELEMENT_STACK_SIZE];
    size_t element_ptr_stack_len;

    MeasureTextFunc measure_text;
    MeasureImageFunc measure_image;

    int win_w, win_h;
    int mouse_x, mouse_y;
};

#define GUI_GET_COMMANDS(gui, command) while (gui->command_stack_iter < gui->command_stack_len && (command = &gui->command_stack[gui->command_stack_iter++]))
#define TRANSPARENT (GuiColor) {0}
#define NO_BORDER TRANSPARENT, 0
#define NO_COLOR TRANSPARENT, NO_BORDER

void gui_init(Gui* gui);
void gui_begin(Gui* gui);
void gui_end(Gui* gui);
void gui_update_window_size(Gui* gui, int win_w, int win_h);
void gui_update_mouse_pos(Gui* gui, int mouse_x, int mouse_y);
void gui_set_measure_text_func(Gui* gui, MeasureTextFunc measure_text);
void gui_set_measure_image_func(Gui* gui, MeasureImageFunc measure_image);

FlexElement* gui_element_begin(Gui* gui);
void gui_element_end(Gui* gui);

void gui_set_fixed(Gui* gui, int w, int h);
void gui_set_fit(Gui* gui);
void gui_set_grow(Gui* gui, FlexDirection direction);
void gui_set_rect(Gui* gui, GuiColor color);
void gui_set_direction(Gui* gui, FlexDirection direction);
void gui_set_border(Gui* gui, GuiColor color, int border_width);
void gui_set_text(Gui* gui, void* font, const char* text, int size, GuiColor color);
void gui_set_image(Gui* gui, void* image, int size, GuiColor color);
void gui_set_min_size(Gui* gui, int min_w, int min_h);
void gui_set_align(Gui* gui, AlignmentType align);
void gui_set_padding(Gui* gui, int pad_w, int pad_h);
void gui_set_gap(Gui* gui, int gap);
void gui_set_custom_data(Gui* gui, void* custom_data);
void gui_set_floating(Gui* gui);
void gui_set_position(Gui* gui, int x, int y);

void gui_on_hover(Gui* gui, HoverHandler handler);

void gui_text(Gui* gui, void* font, const char* text, int size, GuiColor color);
void gui_image(Gui* gui, void* image, int size, GuiColor color);
void gui_grow(Gui* gui, FlexDirection direction);
void gui_spacer(Gui* gui, int w, int h);

#endif // SCRAP_GUI_H
