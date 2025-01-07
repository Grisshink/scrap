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
#define LAYOUT_STACK_SIZE 4096

typedef struct {
    int w, h;
} GuiMeasurement;

typedef struct {
    unsigned char r, g, b, a;
} GuiColor;

typedef enum {
    DRAWTYPE_UNKNOWN = 0,
    DRAWTYPE_RECT,
    DRAWTYPE_TEXT,
    DRAWTYPE_IMAGE,
} DrawType;

typedef union {
    const char* text;
    void* image;
    void* custom_data;
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
    LAYOUT_STATIC,
    LAYOUT_VERTICAL,
    LAYOUT_HORIZONTAL,
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
    Element background;
    void (*advance)(struct Layout* layout, GuiMeasurement size);
} Layout;

typedef GuiMeasurement (*MeasureTextFunc)(const char* text, int size);
typedef GuiMeasurement (*MeasureImageFunc)(void* image, int size);

typedef struct {
    Layout layout_stack[LAYOUT_STACK_SIZE];
    size_t layout_stack_len;

    DrawCommand command_stack[COMMAND_STACK_SIZE];
    size_t command_stack_len;
    size_t command_stack_iter;

    MeasureTextFunc measure_text;
    MeasureImageFunc measure_image;
} Gui;

#define GUI_GET_COMMANDS(gui, command) while (gui->command_stack_iter < gui->command_stack_len && (command = &gui->command_stack[gui->command_stack_iter++]))

void gui_begin(Gui* gui, int pos_x, int pos_y);
void gui_init(Gui* gui);
void gui_set_measure_text_func(Gui* gui, MeasureTextFunc measure_text);
void gui_set_measure_image_func(Gui* gui, MeasureImageFunc measure_image);
void gui_end(Gui* gui);

void gui_layout_begin_static(Gui* gui, int pad_x, int pad_y, GuiColor rect_color);
void gui_layout_end_static(Gui* gui);
void gui_layout_begin_vertical(Gui* gui, int gap, GuiColor rect_color);
void gui_layout_end_vertical(Gui* gui);
void gui_layout_begin_horizontal(Gui* gui, int gap, GuiColor rect_color);
void gui_layout_end_horizontal(Gui* gui);
void gui_layout_set_min_size(Gui* gui, int width, int height);

void gui_draw_rect(Gui* gui, int size_x, int size_y, GuiColor color);
void gui_draw_text(Gui* gui, const char* text, int size, GuiColor color);
void gui_draw_image(Gui* gui, void* image, int size, GuiColor color);

Element gui_begin_element(Gui* gui);
void gui_end_element(Gui* gui, Element element);

#endif // SCRAP_GUI_H
