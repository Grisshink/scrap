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

#define COMMAND_STACK_SIZE 65536
#define LAYOUT_STACK_SIZE 8192
typedef struct Gui Gui;

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

typedef DrawCommand* Element;

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

typedef union {
    int gap;
    GuiMeasurement padding;
} LayoutData;

typedef struct Layout {
    LayoutType type;
    LayoutData data;
    GuiMeasurement size;
    int cursor_x, cursor_y;
    size_t command_start;
    size_t command_end;
    AlignmentType align;
    Element background;
    Element background_border;
    void (*advance)(Gui* gui, struct Layout* layout, GuiMeasurement size);
} Layout;

typedef GuiMeasurement (*MeasureTextFunc)(void* font, const char* text, int size);
typedef GuiMeasurement (*MeasureImageFunc)(void* image, int size);

struct Gui {
    Layout layout_stack[LAYOUT_STACK_SIZE];
    size_t layout_stack_len;

    DrawCommand command_stack[COMMAND_STACK_SIZE];
    size_t command_stack_len;
    size_t command_stack_iter;
    MeasureTextFunc measure_text;
    MeasureImageFunc measure_image;

    int win_w, win_h;
};

#define GUI_GET_COMMANDS(gui, command) while (gui->command_stack_iter < gui->command_stack_len && (command = &gui->command_stack[gui->command_stack_iter++]))
#define TRANSPARENT (GuiColor) {0}
#define NO_BORDER TRANSPARENT, 0
#define NO_COLOR TRANSPARENT, NO_BORDER

void gui_begin(Gui* gui, int pos_x, int pos_y);
void gui_update_window_size(Gui* gui, int win_w, int win_h);
void gui_init(Gui* gui);
void gui_set_measure_text_func(Gui* gui, MeasureTextFunc measure_text);
void gui_set_measure_image_func(Gui* gui, MeasureImageFunc measure_image);
void gui_end(Gui* gui);

void gui_layout_begin_static(Gui* gui, int pad_x, int pad_y);
void gui_layout_end_static(Gui* gui);

void gui_layout_begin_vertical(Gui* gui, int gap, AlignmentType align);
void gui_layout_end_vertical(Gui* gui);

void gui_layout_begin_horizontal(Gui* gui, int gap, AlignmentType align);
void gui_layout_end_horizontal(Gui* gui);

void gui_layout_begin_fixed(Gui* gui, int size_x, int size_y);
void gui_layout_end_fixed(Gui* gui);

void gui_layout_set_min_size(Gui* gui, int width, int height);

void gui_layout_draw_rect(Gui* gui, GuiColor rect_color);
void gui_layout_draw_border(Gui* gui, GuiColor border_color, int border_width);

void gui_draw_rect(Gui* gui, int size_x, int size_y, GuiColor color);
void gui_draw_border(Gui* gui, int size_x, int size_y, int border_width, GuiColor color);
void gui_draw_text(Gui* gui, void* font, const char* text, int size, GuiColor color);
void gui_draw_image(Gui* gui, void* image, int size, GuiColor color);

void gui_begin_scissor(Gui* gui, int size_x, int size_y);
void gui_end_scissor(Gui* gui);

Element gui_begin_element(Gui* gui);
void gui_end_element(Gui* gui, Element element);

#endif // SCRAP_GUI_H
