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

#include "scrap_gui.h"

#include <assert.h>
#include <stdbool.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

static void layout_adv_static(Gui* gui, Layout* layout, GuiMeasurement size) {
    (void) gui;
    layout->size.w = MAX(size.w + layout->data.padding.w * 2, layout->size.w);
    layout->size.h = MAX(size.h + layout->data.padding.h * 2, layout->size.h);
}

static void layout_adv_fixed(Gui* gui, Layout* layout, GuiMeasurement size) {
    (void) gui;
    (void) layout;
    (void) size;
}

static void layout_adv_vertical(Gui* gui, Layout* layout, GuiMeasurement size) {
    int prev_w = layout->size.w;
    size_t prev_command_end = layout->command_end;

    layout->size.w = MAX(size.w, layout->size.w);
    layout->size.h += size.h + layout->data.gap;
    layout->cursor_y += size.h + layout->data.gap;
    layout->command_end = gui->command_stack_len;

    if (layout->align != ALIGN_LEFT) {
        if (prev_w < layout->size.w) {
            int offset = (layout->size.w - prev_w) / (layout->align == ALIGN_CENTER ? 2 : 1);
            for (size_t i = layout->command_start; i < prev_command_end; i++) gui->command_stack[i].pos_x += offset;
        } else {
            int offset = (layout->size.w - size.w /*gui->command_stack[prev_command_end].width*/) / (layout->align == ALIGN_CENTER ? 2 : 1);
            for (size_t i = prev_command_end; i < layout->command_end; i++) gui->command_stack[i].pos_x += offset;
        }
    }
}

static void layout_adv_horizontal(Gui* gui, Layout* layout, GuiMeasurement size) {
    int prev_h = layout->size.h;
    size_t prev_command_end = layout->command_end;

    layout->size.w += size.w + layout->data.gap;
    layout->size.h = MAX(size.h, layout->size.h);
    layout->cursor_x += size.w + layout->data.gap;
    layout->command_end = gui->command_stack_len;

    if (layout->align != ALIGN_TOP) {
        if (prev_h < layout->size.h) {
            int offset = (layout->size.h - prev_h) / (layout->align == ALIGN_CENTER ? 2 : 1);
            for (size_t i = layout->command_start; i < prev_command_end; i++) gui->command_stack[i].pos_y += offset;
        } else {
            int offset = (layout->size.h - size.h /*gui->command_stack[prev_command_end].height*/) / (layout->align == ALIGN_CENTER ? 2 : 1);
            for (size_t i = prev_command_end; i < layout->command_end; i++) gui->command_stack[i].pos_y += offset;
        }
    }
}

static bool inside_window(Gui* gui, Element el) {
    return (el->pos_x + el->width  > 0) && (el->pos_x < gui->win_w) && 
           (el->pos_y + el->height > 0) && (el->pos_y < gui->win_h);
}

static Layout* gui_layout_begin(Gui* gui, GuiColor rect_color, GuiColor border_color, int border_width) {
    assert(gui->layout_stack_len < LAYOUT_STACK_SIZE);

    Layout* layout = &gui->layout_stack[gui->layout_stack_len++];
    if (gui->layout_stack_len <= 1) {
        layout->cursor_x = 0;
        layout->cursor_y = 0;
    } else {
        layout->cursor_x = gui->layout_stack[gui->layout_stack_len - 2].cursor_x;
        layout->cursor_y = gui->layout_stack[gui->layout_stack_len - 2].cursor_y;
    }
    if (rect_color.a == 0) {
        layout->background = NULL;
    } else {
        layout->background = gui_begin_element(gui);
        layout->background->type = DRAWTYPE_RECT;
        layout->background->color = rect_color;
    }
    if (border_color.a == 0) {
        layout->background_border = NULL;
    } else {
        layout->background_border = gui_begin_element(gui);
        layout->background_border->type = DRAWTYPE_BORDER;
        layout->background_border->data.border_width = border_width;
        layout->background_border->color = border_color;
    }
    return layout;
}

static void gui_layout_end(Gui* gui) {
    assert(gui->layout_stack_len > 0);

    Layout* layout = &gui->layout_stack[--gui->layout_stack_len];

    if (layout->background) {
        layout->background->width = layout->size.w;
        layout->background->height = layout->size.h;
    }
    if (layout->background_border) {
        layout->background_border->width = layout->size.w;
        layout->background_border->height = layout->size.h;
    }

    if (layout->background_border) {
        gui_end_element(gui, layout->background_border);
        if (!inside_window(gui, layout->background_border) && (size_t)(layout->background_border - gui->command_stack) == gui->command_stack_len - 1) {
            gui->command_stack_len--;
        }
    } else if (layout->background) {
        gui_end_element(gui, layout->background);
        if (!inside_window(gui, layout->background) && (size_t)(layout->background - gui->command_stack) == gui->command_stack_len - 1) {
            gui->command_stack_len--;
        }
    } else {
        Layout* prev = &gui->layout_stack[gui->layout_stack_len - 1];
        prev->advance(gui, prev, layout->size);
    }
}

void gui_layout_begin_static(Gui* gui, int pad_x, int pad_y, GuiColor rect_color, GuiColor border_color, int border_width) {
    Layout* layout = gui_layout_begin(gui, rect_color, border_color, border_width);
    layout->advance = layout_adv_static;
    layout->type = LAYOUT_STATIC;
    layout->size = (GuiMeasurement) { pad_x * 2, pad_y * 2 };
    layout->cursor_x += pad_x;
    layout->cursor_y += pad_y;
    layout->data.padding.w = pad_x;
    layout->data.padding.h = pad_y;
}

void gui_layout_end_static(Gui* gui) {
    gui_layout_end(gui);
}

void gui_layout_begin_vertical(Gui* gui, int gap, AlignmentType align, GuiColor rect_color, GuiColor border_color, int border_width) {
    Layout* layout = gui_layout_begin(gui, rect_color, border_color, border_width);
    layout->advance = layout_adv_vertical;
    layout->type = LAYOUT_VERTICAL;
    layout->size = (GuiMeasurement) {0};
    layout->align = align;
    layout->command_start = gui->command_stack_len;
    layout->command_end = layout->command_start;
    layout->data.gap = gap;
}

void gui_layout_end_vertical(Gui* gui) {
    Layout* layout = &gui->layout_stack[gui->layout_stack_len - 1];
    layout->size.h -= layout->data.gap;
    gui_layout_end(gui);
}

void gui_layout_begin_horizontal(Gui* gui, int gap, AlignmentType align, GuiColor rect_color, GuiColor border_color, int border_width) {
    Layout* layout = gui_layout_begin(gui, rect_color, border_color, border_width);
    layout->advance = layout_adv_horizontal;
    layout->type = LAYOUT_HORIZONTAL;
    layout->size = (GuiMeasurement) {0};
    layout->align = align;
    layout->command_start = gui->command_stack_len;
    layout->command_end = layout->command_start;
    layout->data.gap = gap;
}

void gui_layout_end_horizontal(Gui* gui) {
    Layout* layout = &gui->layout_stack[gui->layout_stack_len - 1];
    layout->size.w -= layout->data.gap;
    gui_layout_end(gui);
}

void gui_layout_begin_fixed(Gui* gui, int size_x, int size_y, GuiColor rect_color, GuiColor border_color, int border_width) {
    Layout* layout = gui_layout_begin(gui, rect_color, border_color, border_width);
    layout->advance = layout_adv_fixed;
    layout->type = LAYOUT_FIXED;
    layout->size = (GuiMeasurement) { size_x, size_y };

    gui_begin_scissor(gui, size_x, size_y);
}

void gui_layout_end_fixed(Gui* gui) {
    gui_end_scissor(gui);
    gui_layout_end(gui);
}

void gui_init(Gui* gui) {
    gui->measure_text = NULL;
    gui->measure_image = NULL;
}

void gui_begin(Gui* gui, int pos_x, int pos_y) {
    gui->layout_stack_len = 0;
    gui->command_stack_len = 0;
    gui->command_stack_iter = 0;
    gui_layout_begin_static(gui, 0, 0, NO_COLOR);
    gui->layout_stack[0].cursor_x = pos_x;
    gui->layout_stack[0].cursor_y = pos_y;
}

void gui_end(Gui* gui) {
    (void) gui;
}

void gui_set_measure_text_func(Gui* gui, MeasureTextFunc measure_text) {
    gui->measure_text = measure_text;
}

void gui_set_measure_image_func(Gui* gui, MeasureImageFunc measure_image) {
    gui->measure_image = measure_image;
}

void gui_update_window_size(Gui* gui, int win_w, int win_h) {
    gui->win_w = win_w;
    gui->win_h = win_h;
}

Element gui_begin_element(Gui* gui) {
    assert(gui->command_stack_len < COMMAND_STACK_SIZE);

    Element el = &gui->command_stack[gui->command_stack_len++];
    el->type = DRAWTYPE_UNKNOWN;
    el->pos_x = gui->layout_stack[gui->layout_stack_len - 1].cursor_x;
    el->pos_y = gui->layout_stack[gui->layout_stack_len - 1].cursor_y;
    el->width = 0;
    el->height = 0;
    el->color = (GuiColor) {0};
    el->data.custom_data = NULL;
    return el;
}

void gui_end_element(Gui* gui, Element element) {
    assert(gui->layout_stack_len > 0);
    assert(element != NULL);
    Layout* lay = &gui->layout_stack[gui->layout_stack_len - 1];
    lay->advance(gui, lay, (GuiMeasurement) { element->width, element->height });
}

void gui_layout_set_min_size(Gui* gui, int width, int height) {
    assert(gui->layout_stack_len > 0);
    Layout* lay = &gui->layout_stack[gui->layout_stack_len - 1];
    lay->size.w = MAX(width, lay->size.w);
    lay->size.h = MAX(height, lay->size.h);
}

void gui_begin_scissor(Gui* gui, int size_x, int size_y) {
    Element el = gui_begin_element(gui);
    el->type = DRAWTYPE_SCISSOR_BEGIN;
    el->width = size_x;
    el->height = size_y;
    gui_end_element(gui, el);
    //if (!inside_window(gui, el)) gui->command_stack_len--;
}

void gui_end_scissor(Gui* gui) {
    Element el = gui_begin_element(gui);
    el->type = DRAWTYPE_SCISSOR_END;
    gui_end_element(gui, el);
    //if (!inside_window(gui, el)) gui->command_stack_len--;
}

void gui_draw_rect(Gui* gui, int size_x, int size_y, GuiColor color) {
    Element el = gui_begin_element(gui);
    el->type = DRAWTYPE_RECT;
    el->width = size_x;
    el->height = size_y;
    el->color = color;
    gui_end_element(gui, el);
    if (!inside_window(gui, el)) gui->command_stack_len--;
}

void gui_draw_border(Gui* gui, int size_x, int size_y, int border_width, GuiColor color) {
    Element el = gui_begin_element(gui);
    el->type = DRAWTYPE_BORDER;
    el->width = size_x;
    el->height = size_y;
    el->color = color;
    el->data.border_width = border_width;
    gui_end_element(gui, el);
    if (!inside_window(gui, el)) gui->command_stack_len--;
}

void gui_draw_text(Gui* gui, void* font, const char* text, int size, GuiColor color) {
    assert(gui->measure_text != NULL);
    Element el = gui_begin_element(gui);
    GuiMeasurement text_size = gui->measure_text(font, text, size);
    el->type = DRAWTYPE_TEXT;
    el->width = text_size.w;
    el->height = text_size.h;
    el->color = color;
    el->data.text.text = text;
    el->data.text.font = font;
    gui_end_element(gui, el);
    if (!inside_window(gui, el)) gui->command_stack_len--;
}

void gui_draw_image(Gui* gui, void* image, int size, GuiColor color) {
    assert(gui->measure_image != NULL);
    Element el = gui_begin_element(gui);
    GuiMeasurement image_size = gui->measure_image(image, size);
    el->type = DRAWTYPE_IMAGE;
    el->width = image_size.w;
    el->height = image_size.h;
    el->color = color;
    el->data.image = image;
    gui_end_element(gui, el);
    if (!inside_window(gui, el)) gui->command_stack_len--;
}
