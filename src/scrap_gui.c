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
#include <string.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

#define SIZING_X(el) (GuiElementSizing)(el->sizing & 0x0f)
#define SIZING_Y(el) (GuiElementSizing)((el->sizing >> 4) & 0x0f)
#define ANCHOR_X(el) (GuiAlignmentType)(el->anchor & 0x0f)
#define ANCHOR_Y(el) (GuiAlignmentType)((el->anchor >> 4) & 0x0f)
#define NEED_RESIZE(el) ((el->flags >> 5) & 1)
#define SCISSOR(el) ((el->flags >> 4) & 1)
#define FLOATING(el) ((el->flags >> 3) & 1)
#define ALIGN(el) (GuiAlignmentType)((el->flags >> 1) & 3)
#define DIRECTION(el) (GuiElementDirection)(el->flags & 1)

#define SET_SIZING_X(el, size) (el->sizing = (el->sizing & 0xf0) | size)
#define SET_SIZING_Y(el, size) (el->sizing = (el->sizing & 0x0f) | (size << 4))
#define SET_NEED_RESIZE(el, resize) (el->flags = (el->flags & ~(1 << 5)) | ((resize & 1) << 5))
#define SET_SCISSOR(el, scissor) (el->flags = (el->flags & ~(1 << 4)) | ((scissor & 1) << 4))
#define SET_FLOATING(el, floating) (el->flags = (el->flags & ~(1 << 3)) | ((floating & 1) << 3))
#define SET_ALIGN(el, ali) (el->flags = (el->flags & 0xf9) | (ali << 1))
#define SET_DIRECTION(el, dir) (el->flags = (el->flags & 0xfe) | dir)
#define SET_ANCHOR(el, x, y) (el->anchor = x | (y << 4))

static void gui_render(Gui* gui, GuiElement* el, float pos_x, float pos_y, float parent_scaling);
static void flush_aux_buffers(Gui* gui);

static bool inside_window(Gui* gui, GuiDrawCommand* command) {
    return (command->pos_x + command->width  > 0) && (command->pos_x < gui->win_w) &&
           (command->pos_y + command->height > 0) && (command->pos_y < gui->win_h);
}

static bool mouse_inside(Gui* gui, GuiBounds rect) {
    return ((gui->mouse_x > rect.x) && (gui->mouse_x < rect.x + rect.w) &&
            (gui->mouse_y > rect.y) && (gui->mouse_y < rect.y + rect.h));
}

void gui_init(Gui* gui) {
    gui->measure_text = NULL;
    gui->measure_image = NULL;
    gui->command_stack_len = 0;
    gui->element_stack_len = 0;
    gui->win_w = 0;
    gui->win_h = 0;
    gui->mouse_x = 0;
    gui->mouse_y = 0;
    gui->mouse_scroll = 0;
    gui->command_stack_iter = 0;
}

void gui_begin(Gui* gui) {
    gui->command_stack_len = 0;
    gui->command_stack_iter = 0;
    gui->rect_stack_len = 0;
    gui->border_stack_len = 0;
    gui->image_stack_len = 0;
    gui->text_stack_len = 0;
    gui->element_stack_len = 0;
    gui->element_ptr_stack_len = 0;
    gui->scissor_stack_len = 0;
    gui->state_stack_len = 0;
    gui_element_begin(gui);
    gui_set_fixed(gui, gui->win_w, gui->win_h);
}

void gui_end(Gui* gui) {
    gui_element_end(gui);
    gui_render(gui, &gui->element_stack[0], 0, 0, 1.0);
    flush_aux_buffers(gui);
}

void gui_set_measure_text_func(Gui* gui, GuiMeasureTextSliceFunc measure_text) {
    gui->measure_text = measure_text;
}

void gui_set_measure_image_func(Gui* gui, GuiMeasureImageFunc measure_image) {
    gui->measure_image = measure_image;
}

void gui_update_mouse_pos(Gui* gui, short mouse_x, short mouse_y) {
    gui->mouse_x = mouse_x;
    gui->mouse_y = mouse_y;
}

void gui_update_mouse_scroll(Gui* gui, int mouse_scroll) {
    gui->mouse_scroll = mouse_scroll;
}

void gui_update_window_size(Gui* gui, unsigned short win_w, unsigned short win_h) {
    gui->win_w = win_w;
    gui->win_h = win_h;
}

static int partition_image(GuiDrawCommand* array, int begin, int end) {
    GuiDrawCommand pivot = array[(begin + end) / 2];
    int left = begin - 1;
    int right = end + 1;

    for (;;) {
        do left++; while (array[left].data.image < pivot.data.image);
        do right--; while (array[right].data.image > pivot.data.image);
        if (left >= right) return right;
        GuiDrawCommand temp = array[left];
        array[left] = array[right];
        array[right] = temp;
    }
}

static int partition_font(GuiDrawCommand* array, int begin, int end) {
    GuiDrawCommand pivot = array[(begin + end) / 2];
    int left = begin - 1;
    int right = end + 1;

    for (;;) {
        do left++; while (array[left].data.text.font < pivot.data.text.font);
        do right--; while (array[right].data.text.font > pivot.data.text.font);
        if (left >= right) return right;
        GuiDrawCommand temp = array[left];
        array[left] = array[right];
        array[right] = temp;
    }
}

static void sort_commands(GuiDrawCommand* array, int begin, int end, bool is_font) {
    if (begin >= end || begin < 0 || end < 0) return;
    int pos = is_font ? partition_font(array, begin, end) : partition_image(array, begin, end);
    sort_commands(array, begin, pos, is_font);
    sort_commands(array, pos + 1, end, is_font);
}

static void flush_aux_buffers(Gui* gui) {
    sort_commands(gui->text_stack, 0, gui->text_stack_len - 1, true);
    sort_commands(gui->image_stack, 0, gui->image_stack_len - 1, false);

    for (size_t i = 0; i < gui->rect_stack_len; i++)   gui->command_stack[gui->command_stack_len++] = gui->rect_stack[i];
    for (size_t i = 0; i < gui->border_stack_len; i++) gui->command_stack[gui->command_stack_len++] = gui->border_stack[i];
    for (size_t i = 0; i < gui->image_stack_len; i++)  gui->command_stack[gui->command_stack_len++] = gui->image_stack[i];
    for (size_t i = 0; i < gui->text_stack_len; i++)   gui->command_stack[gui->command_stack_len++] = gui->text_stack[i];

    gui->rect_stack_len = 0;
    gui->border_stack_len = 0;
    gui->image_stack_len = 0;
    gui->text_stack_len = 0;
}

static GuiDrawCommand* new_draw_command(Gui* gui, GuiDrawBounds bounds, GuiDrawType draw_type, GuiDrawData data, GuiColor color) {
    GuiDrawCommand* command = &gui->command_stack[gui->command_stack_len++];
    command->pos_x = bounds.x;
    command->pos_y = bounds.y;
    command->width = bounds.w;
    command->height = bounds.h;
    command->type = draw_type;
    command->data = data;
    command->color = color;
    return command;
}

static GuiBounds scissor_rect(GuiBounds rect, GuiBounds scissor) {
    if (rect.x < scissor.x) {
        rect.w = MAX(0, rect.w - (scissor.x - rect.x));
        rect.x = scissor.x;
    }
    if (rect.y < scissor.y) {
        rect.h = MAX(0, rect.h - (scissor.y - rect.y));
        rect.y = scissor.y;
    }
    if (rect.x + rect.w > scissor.x + scissor.w) {
        rect.w = MAX(0, rect.w - ((rect.x + rect.w) - (scissor.x + scissor.w)));
    }
    if (rect.y + rect.h > scissor.y + scissor.h) {
        rect.h = MAX(0, rect.h - ((rect.y + rect.h) - (scissor.y + scissor.h)));
    }

    return rect;
}

static void gui_render(Gui* gui, GuiElement* el, float pos_x, float pos_y, float parent_scaling) {
    GuiBounds scissor = gui->scissor_stack_len > 0 ? gui->scissor_stack[gui->scissor_stack_len - 1] : (GuiBounds) { 0, 0, gui->win_w, gui->win_h };
    bool hover = false;

    if (el->parent_anchor && el->parent_anchor < el) {
        pos_x = el->parent_anchor->abs_x;
        pos_y = el->parent_anchor->abs_y;
    }

    float anchor_x = 0, 
          anchor_y = 0;

    switch (ANCHOR_X(el)) {
    case ALIGN_CENTER: anchor_x = el->w * el->scaling / 2; break;
    case ALIGN_RIGHT: anchor_x = el->w * el->scaling; break;
    default: break;
    }
    switch (ANCHOR_Y(el)) {
    case ALIGN_CENTER: anchor_y = el->h * el->scaling / 2; break;
    case ALIGN_RIGHT: anchor_y = el->h * el->scaling; break;
    default: break;
    }

    el->abs_x = (el->x - anchor_x) * parent_scaling + pos_x;
    el->abs_y = (el->y - anchor_y) * parent_scaling + pos_y;

    if (mouse_inside(gui, scissor_rect((GuiBounds) {
        (el->x - anchor_x) * parent_scaling + pos_x,
        (el->y - anchor_y) * parent_scaling + pos_y,
        el->w * el->scaling,
        el->h * el->scaling }, scissor)))
    {
        if (el->handle_hover) el->handle_hover(el);
        hover = true;
    }
    if (el->handle_pre_render) el->handle_pre_render(el);

    GuiDrawBounds el_bounds = (GuiDrawBounds) {
        pos_x + ((float)el->x - anchor_x) * parent_scaling,
        pos_y + ((float)el->y - anchor_y) * parent_scaling,
        (float)el->w * el->scaling,
        (float)el->h * el->scaling,
    };

    if (SCISSOR(el) || FLOATING(el) || el->shader) flush_aux_buffers(gui);

    if (SCISSOR(el)) {
        new_draw_command(gui, el_bounds, DRAWTYPE_SCISSOR_BEGIN, (GuiDrawData) {0}, (GuiColor) {0});
        gui->scissor_stack[gui->scissor_stack_len++] = (GuiBounds) { el_bounds.x, el_bounds.y, el_bounds.w, el_bounds.h };
    }
    if (el->shader) new_draw_command(gui, el_bounds, DRAWTYPE_SHADER_BEGIN, (GuiDrawData) { .shader = el->shader }, (GuiColor) {0});

    if (el->draw_type != DRAWTYPE_UNKNOWN) {
        GuiDrawCommand command;
        command.pos_x = el_bounds.x;
        command.pos_y = el_bounds.y;
        command.width = el_bounds.w;
        command.height = el_bounds.h;
        command.type = el->draw_type;
        command.color = el->color;
        command.data = el->data;

        if (inside_window(gui, &command)) {
            switch (el->draw_type) {
            case DRAWTYPE_RECT:
                gui->rect_stack[gui->rect_stack_len++] = command;
                break;
            case DRAWTYPE_BORDER:
                gui->border_stack[gui->border_stack_len++] = command;
                break;
            case DRAWTYPE_IMAGE:
                gui->image_stack[gui->image_stack_len++] = command;
                break;
            case DRAWTYPE_TEXT:
                gui->text_stack[gui->text_stack_len++] = command;
                break;
            default:
                assert(false && "Unhandled render draw type");
                break;
            }
        }
    }

    if (el->shader) {
        flush_aux_buffers(gui);
        new_draw_command(gui, el_bounds, DRAWTYPE_SHADER_END, (GuiDrawData) { .shader = el->shader }, (GuiColor) {0});
    }

    GuiElement* iter = el + 1;
    for (int i = 0; i < el->element_count; i++) {
        gui_render(gui, iter, pos_x + ((float)el->x - anchor_x) * parent_scaling, pos_y + ((float)el->y - anchor_y) * parent_scaling, el->scaling);
        iter = iter->next;
    }

    if (el->scroll_value) {
        int el_size = DIRECTION(el) == DIRECTION_HORIZONTAL ? el->w : el->h;
        int content_size = DIRECTION(el) == DIRECTION_HORIZONTAL ? el->cursor_x : el->cursor_y;
        int max = content_size - el_size;

        if (max > 0) {
            flush_aux_buffers(gui);
            GuiDrawCommand* command = &gui->command_stack[gui->command_stack_len++];
            command->type = DRAWTYPE_RECT;
            command->data.rect_type = RECT_NORMAL;
            command->color = (GuiColor) { 0xff, 0xff, 0xff, 0x80 };

            float scroll_size = (float)el_size / ((float)content_size / (float)el_size);
            float scroll_pos = (-(float)*el->scroll_value / (float)max) * ((float)el_size - scroll_size);
            if (DIRECTION(el) == DIRECTION_HORIZONTAL) {
                command->width = scroll_size * el->scaling;
                command->height = 5 * el->scaling;
                command->pos_x = el_bounds.x + scroll_pos * parent_scaling;
                command->pos_y = el_bounds.y + el_bounds.h - command->height;
            } else {
                command->width = 5 * el->scaling;
                command->height = scroll_size * el->scaling;
                command->pos_x = el_bounds.x + el_bounds.w - command->width;
                command->pos_y = el_bounds.y + scroll_pos * parent_scaling;
            }
        }

        if (hover) *el->scroll_value += gui->mouse_scroll * el->scroll_scaling;
        if (*el->scroll_value < -max) *el->scroll_value = -max;
        if (*el->scroll_value > 0) *el->scroll_value = 0;
    }

    if (FLOATING(el) || SCISSOR(el)) flush_aux_buffers(gui);

    if (SCISSOR(el)) {
        new_draw_command(gui, el_bounds, DRAWTYPE_SCISSOR_END, (GuiDrawData) {0}, (GuiColor) {0});
        gui->scissor_stack_len--;
    }
}

GuiElement* gui_element_begin(Gui* gui) {
    assert(gui->element_stack_len < ELEMENT_STACK_SIZE);
    assert(gui->element_ptr_stack_len < ELEMENT_STACK_SIZE);

    GuiElement* prev = gui->element_ptr_stack_len > 0 ? gui->element_ptr_stack[gui->element_ptr_stack_len - 1] : NULL;
    GuiElement* el = &gui->element_stack[gui->element_stack_len++];
    gui->element_ptr_stack[gui->element_ptr_stack_len++] = el;
    el->draw_type = DRAWTYPE_UNKNOWN;
    el->data = (GuiDrawData) {0};
    el->x = prev ? prev->cursor_x : 0;
    el->y = prev ? prev->cursor_y : 0;
    el->abs_x = 0;
    el->abs_y = 0;
    el->scaling = prev ? prev->scaling : 1.0;
    el->element_count = 0;
    el->cursor_x = 0;
    el->cursor_y = 0;
    el->sizing = 0; // sizing_x = SIZING_FIT, sizing_y = SIZING_FIT
    el->pad_w = 0;
    el->pad_h = 0;
    el->gap = 0;
    el->w = 0;
    el->h = 0;
    el->flags = 0; // direction = DIRECTION_VERTICAL, align = ALIGN_TOP | ALIGN_LEFT, is_floating = false, needs_resize = false
    el->anchor = 0; // Top left anchor
    el->next = NULL;
    el->parent_anchor = NULL;
    el->handle_hover = NULL;
    el->handle_pre_render = NULL;
    el->custom_data = NULL;
    el->custom_state = NULL;
    el->scroll_value = NULL;
    el->size_percentage = 1.0;
    el->shader = NULL;
    el->scroll_scaling = 64;
    el->state_len = 0;
    return el;
}

static void gui_element_offset(GuiElement* el, int offset_x, int offset_y) {
    GuiElement *iter = el + 1;
    for (int i = 0; i < el->element_count; i++) {
        iter->x += offset_x;
        iter->y += offset_y;
        iter = iter->next;
    }
}

static void gui_element_realign(GuiElement* el) {
    if (ALIGN(el) == ALIGN_TOP) return;

    int align_div = ALIGN(el) == ALIGN_CENTER ? 2 : 1;
    GuiElement *iter = el + 1;
    for (int i = 0; i < el->element_count; i++) {
        if (!FLOATING(iter)) {
            if (DIRECTION(el) == DIRECTION_VERTICAL) {
                iter->x = (el->w - iter->w) / align_div;
            } else {
                iter->y = (el->h - iter->h) / align_div;
            }
        }
        iter = iter->next;
    }
}

static void gui_element_resize(Gui* gui, GuiElement* el, unsigned short new_w, unsigned short new_h) {
    el->w = new_w;
    el->h = new_h;

    int left_w = el->w - el->pad_w * 2 + el->gap;
    int left_h = el->h - el->pad_h * 2 + el->gap;
    int grow_elements = 0;

    GuiElement* iter = el + 1;
    for (int i = 0; i < el->element_count; i++) {
        if (!FLOATING(iter)) {
            if (DIRECTION(el) == DIRECTION_VERTICAL) {
                if (SIZING_Y(iter) == SIZING_GROW) {
                    grow_elements++;
                } else if (SIZING_Y(iter) == SIZING_PERCENT) {
                    left_h -= el->h * iter->size_percentage;
                } else {
                    left_h -= iter->h;
                }
                left_h -= el->gap;
            } else {
                if (SIZING_X(iter) == SIZING_GROW) {
                    grow_elements++;
                } else if (SIZING_X(iter) == SIZING_PERCENT) {
                    left_w -= el->w * iter->size_percentage;
                } else {
                    left_w -= iter->w;
                }
                left_w -= el->gap;
            }
        }
        iter = iter->next;
    }

    el->cursor_x = el->pad_w;
    el->cursor_y = el->pad_h;

    iter = el + 1;
    for (int i = 0; i < el->element_count; i++) {
        bool is_floating = FLOATING(iter);
        if (!is_floating) {
            iter->x = el->cursor_x;
            iter->y = el->cursor_y;
        }

        int size_w = iter->w;
        int size_h = iter->h;
        GuiElementSizing sizing_x = SIZING_X(iter);
        GuiElementSizing sizing_y = SIZING_Y(iter);
        if (sizing_x == SIZING_PERCENT) size_w = el->w * iter->size_percentage;
        if (sizing_y == SIZING_PERCENT) size_h = el->h * iter->size_percentage;

        if (DIRECTION(el) == DIRECTION_VERTICAL) {
            if (sizing_x == SIZING_GROW) size_w = el->w - el->pad_w * 2;
            if (sizing_y == SIZING_GROW) size_h = left_h / grow_elements;
            if (sizing_x == SIZING_GROW || sizing_y == SIZING_GROW || sizing_x == SIZING_PERCENT || sizing_y == SIZING_PERCENT) gui_element_resize(gui, iter, size_w, size_h);
            if (!is_floating) el->cursor_y += iter->h + el->gap;
        } else {
            if (sizing_x == SIZING_GROW) size_w = left_w / grow_elements;
            if (sizing_y == SIZING_GROW) size_h = el->h - el->pad_h * 2;
            if (sizing_x == SIZING_GROW || sizing_y == SIZING_GROW || sizing_x == SIZING_PERCENT || sizing_y == SIZING_PERCENT) gui_element_resize(gui, iter, size_w, size_h);
            if (!is_floating) el->cursor_x += iter->w + el->gap;
        }
        iter = iter->next;
    }
    if (DIRECTION(el) == DIRECTION_HORIZONTAL) {
        el->cursor_x += el->pad_w - el->gap;
    } else {
        el->cursor_y += el->pad_h - el->gap;
    }

    gui_element_realign(el);
    if (el->scroll_value) {
        if (DIRECTION(el) == DIRECTION_HORIZONTAL) {
            gui_element_offset(el, *el->scroll_value, 0);
        } else {
            gui_element_offset(el, 0, *el->scroll_value);
        }
    }
}

static void gui_element_advance(GuiElement* el, GuiMeasurement ms) {
    if (!el) return;

    if (DIRECTION(el) == DIRECTION_HORIZONTAL) {
        el->cursor_x += ms.w + el->gap;
        if (SIZING_X(el) != SIZING_FIXED) el->w = MAX(el->w, el->cursor_x + el->pad_w);
        if (SIZING_Y(el) != SIZING_FIXED) el->h = MAX(el->h, ms.h + el->pad_h * 2);
    } else {
        el->cursor_y += ms.h + el->gap;
        if (SIZING_X(el) != SIZING_FIXED) el->w = MAX(el->w, ms.w + el->pad_w * 2);
        if (SIZING_Y(el) != SIZING_FIXED) el->h = MAX(el->h, el->cursor_y + el->pad_h);
    }
}

void gui_element_end(Gui* gui) {
    GuiElement* el = gui->element_ptr_stack[--gui->element_ptr_stack_len];
    GuiElement* prev = gui->element_ptr_stack_len > 0 ? gui->element_ptr_stack[gui->element_ptr_stack_len - 1] : NULL;
    if (DIRECTION(el) == DIRECTION_VERTICAL) {
        el->h -= el->gap;
    } else {
        el->w -= el->gap;
    }
    el->next = &gui->element_stack[gui->element_stack_len];
    if (prev) {
        prev->element_count++;
    }

    if (!FLOATING(el)) gui_element_advance(prev, (GuiMeasurement) { el->w, el->h });
    GuiElementSizing sizing_x = SIZING_X(el);
    GuiElementSizing sizing_y = SIZING_Y(el);
    bool has_defined_size = sizing_x != SIZING_GROW && sizing_x != SIZING_PERCENT &&
                            sizing_y != SIZING_GROW && sizing_y != SIZING_PERCENT;

    if (!has_defined_size) SET_NEED_RESIZE(prev, 1);

    if (has_defined_size && NEED_RESIZE(el) && (sizing_x == SIZING_FIXED || sizing_y == SIZING_FIXED || sizing_x == SIZING_FIT || sizing_y == SIZING_FIT)) {
        gui_element_resize(gui, el, el->w, el->h);
    } else {
        gui_element_realign(el);
        if (el->scroll_value) {
            if (DIRECTION(el) == DIRECTION_HORIZONTAL) {
                gui_element_offset(el, *el->scroll_value, 0);
            } else {
                gui_element_offset(el, 0, *el->scroll_value);
            }
        }
    }
}

GuiElement* gui_get_element(Gui* gui) {
    return gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
}

void gui_on_hover(Gui* gui, GuiHandler handler) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->handle_hover = handler;
}

void gui_on_render(Gui* gui, GuiHandler handler) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->handle_pre_render = handler;
}

void gui_set_anchor(Gui* gui, GuiAlignmentType anchor_x, GuiAlignmentType anchor_y) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    SET_ANCHOR(el, anchor_x, anchor_y);
}

void gui_set_parent_anchor(Gui* gui, GuiElement* anchor) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->parent_anchor = anchor;
}

void gui_set_shader(Gui* gui, void* shader) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->shader = shader;
}

void gui_set_scroll_scaling(Gui* gui, int scroll_scaling) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->scroll_scaling = scroll_scaling;
}

void gui_set_scroll(Gui* gui, int* scroll_value) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->scroll_value = scroll_value;
}

void gui_set_scissor(Gui* gui) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    SET_SCISSOR(el, 1);
}

void gui_scale_element(Gui* gui, float scaling) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->scaling *= scaling;
}

void* gui_set_state(Gui* gui, void* state, unsigned short state_len) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    if (el->custom_state) return el->custom_state;
    assert(gui->state_stack_len + state_len <= STATE_STACK_SIZE);

    memcpy(&gui->state_stack[gui->state_stack_len], state, state_len);
    el->custom_state = &gui->state_stack[gui->state_stack_len];
    gui->state_stack_len += state_len;
    return el->custom_state;
}

void* gui_get_state(GuiElement* el, unsigned short* state_len) {
    *state_len = el->state_len;
    return el->custom_state;
}

void gui_set_floating(Gui* gui) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    SET_FLOATING(el, 1);
}

void gui_set_position(Gui* gui, int x, int y) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->x = x;
    el->y = y;
}

void gui_set_custom_data(Gui* gui, void* custom_data) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->custom_data = custom_data;
}

void gui_set_fixed(Gui* gui, unsigned short w, unsigned short h) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->sizing = SIZING_FIXED | (SIZING_FIXED << 4); // This sets both dimensions to SIZING_FIXED
    el->w = w;
    el->h = h;
}

void gui_set_fit(Gui* gui, GuiElementDirection direction) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    if (direction == DIRECTION_VERTICAL) {
        SET_SIZING_Y(el, SIZING_FIT);
    } else {
        SET_SIZING_X(el, SIZING_FIT);
    }
}

void gui_set_padding(Gui* gui, unsigned short pad_w, unsigned short pad_h) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->pad_w = pad_w;
    el->pad_h = pad_h;
    el->w = MAX(el->w, el->pad_w * 2);
    el->h = MAX(el->h, el->pad_h * 2);
    el->cursor_x = pad_w;
    el->cursor_y = pad_h;
}

void gui_set_gap(Gui* gui, unsigned short gap) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->gap = gap;
}

void gui_set_grow(Gui* gui, GuiElementDirection direction) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    if (direction == DIRECTION_VERTICAL) {
        SET_SIZING_Y(el, SIZING_GROW);
        el->h = 0;
    } else {
        SET_SIZING_X(el, SIZING_GROW);
        el->w = 0;
    }
}

void gui_set_percent_size(Gui* gui, float percentage, GuiElementDirection direction) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->size_percentage = percentage;
    if (direction == DIRECTION_VERTICAL) {
        SET_SIZING_Y(el, SIZING_PERCENT);
        el->h = 0;
    } else {
        SET_SIZING_X(el, SIZING_PERCENT);
        el->w = 0;
    }
}

void gui_set_direction(Gui* gui, GuiElementDirection direction) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    SET_DIRECTION(el, direction);
}

void gui_set_rect(Gui* gui, GuiColor color) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->draw_type = DRAWTYPE_RECT;
    el->color = color;
    el->data.rect_type = RECT_NORMAL;
}

void gui_set_rect_type(Gui* gui, GuiRectType type) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    if (el->draw_type != DRAWTYPE_RECT) return;
    el->data.rect_type = type;
}

void gui_set_border(Gui* gui, GuiColor color, unsigned int border_width) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->draw_type = DRAWTYPE_BORDER;
    el->color = color;
    el->data.border.width = border_width;
    el->data.border.type = BORDER_NORMAL;
}

void gui_set_border_type(Gui* gui, GuiBorderType type) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    if (el->draw_type != DRAWTYPE_BORDER) return;
    el->data.border.type = type;
}

void gui_set_text_slice(Gui* gui, void* font, const char* text, unsigned int text_size, unsigned short font_size, GuiColor color) {
    if (text_size == 0) return;
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    GuiMeasurement text_bounds = gui->measure_text(font, text, text_size, font_size);
    el->draw_type = DRAWTYPE_TEXT;
    el->color = color;
    el->data.text.text = text;
    el->data.text.font = font;
    el->data.text.text_size = text_size;
    el->w = text_bounds.w;
    el->h = text_bounds.h;
}

inline void gui_set_text(Gui* gui, void* font, const char* text, unsigned short font_size, GuiColor color) {
    gui_set_text_slice(gui, font, text, strlen(text), font_size, color);
}

void gui_set_image(Gui* gui, void* image, unsigned short size, GuiColor color) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    GuiMeasurement image_size = gui->measure_image(image, size);
    el->draw_type = DRAWTYPE_IMAGE;
    el->color = color;
    el->data.image = image;
    el->w = image_size.w;
    el->h = image_size.h;
}

void gui_set_align(Gui* gui, GuiAlignmentType align) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    SET_ALIGN(el, align);
}

void gui_set_min_size(Gui* gui, unsigned short min_w, unsigned short min_h) {
    GuiElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->w = MAX(el->w, min_w);
    el->h = MAX(el->h, min_h);
}

inline void gui_text_slice(Gui* gui, void* font, const char* text, unsigned int text_size, unsigned short font_size, GuiColor color) {
    if (text_size == 0) return;
    gui_element_begin(gui);
    gui_set_text_slice(gui, font, text, text_size, font_size, color);
    gui_element_end(gui);
}

inline void gui_text(Gui* gui, void* font, const char* text, unsigned short size, GuiColor color) {
    gui_element_begin(gui);
    gui_set_text(gui, font, text, size, color);
    gui_element_end(gui);
}

inline void gui_image(Gui* gui, void* image, unsigned short size, GuiColor color) {
    gui_element_begin(gui);
    gui_set_image(gui, image, size, color);
    gui_element_end(gui);
}

inline void gui_grow(Gui* gui, GuiElementDirection direction) {
    gui_element_begin(gui);
    gui_set_grow(gui, direction);
    gui_element_end(gui);
}

inline void gui_spacer(Gui* gui, unsigned short w, unsigned short h) {
    gui_element_begin(gui);
    gui_set_min_size(gui, w, h);
    gui_element_end(gui);
}
