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
#define ALIGN_X(el) (GuiAlignmentType)((el->flags >> 6) & 3)
#define ALIGN_Y(el) (GuiAlignmentType)((el->flags >> 1) & 3)
#define DIRECTION(el) (GuiElementDirection)(el->flags & 1)

#define SET_SIZING_X(el, size) (el->sizing = (el->sizing & 0xf0) | size)
#define SET_SIZING_Y(el, size) (el->sizing = (el->sizing & 0x0f) | (size << 4))
#define SET_NEED_RESIZE(el, resize) (el->flags = (el->flags & ~(1 << 5)) | ((resize & 1) << 5))
#define SET_SCISSOR(el, scissor) (el->flags = (el->flags & ~(1 << 4)) | ((scissor & 1) << 4))
#define SET_FLOATING(el, floating) (el->flags = (el->flags & ~(1 << 3)) | ((floating & 1) << 3))
#define SET_ALIGN_X(el, ali) (el->flags = (el->flags & 0x3f) | (ali << 6))
#define SET_ALIGN_Y(el, ali) (el->flags = (el->flags & 0xf9) | (ali << 1))
#define SET_DIRECTION(el, dir) (el->flags = (el->flags & 0xfe) | dir)
#define SET_ANCHOR(el, x, y) (el->anchor = x | (y << 4))

static void gui_render(Gui* gui, GuiElement* el);
static void flush_command_batch(Gui* gui);

static bool inside_window(Gui* gui, GuiDrawBounds rect) {
    return (rect.x + rect.w > 0) && (rect.x < gui->win_w) &&
           (rect.y + rect.h > 0) && (rect.y < gui->win_h);
}

static bool mouse_inside(Gui* gui, GuiBounds rect) {
    return ((gui->mouse_x > rect.x) && (gui->mouse_x < rect.x + rect.w) &&
            (gui->mouse_y > rect.y) && (gui->mouse_y < rect.y + rect.h));
}

void gui_init(Gui* gui) {
    gui->measure_text = NULL;
    gui->measure_image = NULL;
    gui->command_list_len = 0;
    gui->elements_arena_len = 0;
    gui->win_w = 0;
    gui->win_h = 0;
    gui->mouse_x = 0;
    gui->mouse_y = 0;
    gui->mouse_scroll = 0;
    gui->command_list_iter = 0;
}

void gui_begin(Gui* gui) {
    gui->command_list_len = 0;
    gui->command_list_iter = 0;
    gui->elements_arena_len = 0;
    gui->scissor_stack_len = 0;
    gui->state_arena_len = 0;
    gui->current_element = NULL;
    gui->command_list_last_batch = 0;

    gui_element_begin(gui);
    gui_set_fixed(gui, gui->win_w, gui->win_h);
}

void gui_end(Gui* gui) {
    gui_element_end(gui);
    gui_render(gui, &gui->elements_arena[0]);
    flush_command_batch(gui);
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

static bool is_command_lesseq(GuiDrawCommand* left, GuiDrawCommand* right) {
    if (left->type != right->type) return left->type <= right->type;
    
    switch (left->type) {
    case DRAWTYPE_IMAGE:
        return left->data.image <= right->data.image;
    case DRAWTYPE_TEXT:
        return left->data.text.font <= right->data.text.font;
    default:
        return true; // Equal
    }
}

static void merge(GuiDrawCommand* list, GuiDrawCommand* aux_list, int start, int middle, int end) {
    int left_pos  = start,
        right_pos = middle;

    for (int i = start; i < end; i++) {
        if (left_pos < middle && (right_pos >= end || is_command_lesseq(&list[left_pos], &list[right_pos]))) {
            aux_list[i] = list[left_pos++];
        } else {
            aux_list[i] = list[right_pos++];
        }
    }
}

static void split_and_merge_commands(GuiDrawCommand* list, GuiDrawCommand* aux_list, int start, int end) {
    if (end - start <= 1) return;
    int middle = (start + end) / 2;

    split_and_merge_commands(aux_list, list, start,  middle);
    split_and_merge_commands(aux_list, list, middle, end);
    merge(list, aux_list, start, middle, end);
}

static void sort_commands(GuiDrawCommand* list, GuiDrawCommand* aux_list, int start, int end) {
    for (int i = start; i < end; i++) aux_list[i] = list[i];
    split_and_merge_commands(aux_list, list, start, end);
}

static void flush_command_batch(Gui* gui) {
    if (gui->command_list_last_batch >= gui->command_list_len) return;
    // Skip sorting unwanted elements
    while (gui->command_list[gui->command_list_last_batch].type > 4) gui->command_list_last_batch++;

    sort_commands(gui->command_list, gui->aux_command_list, gui->command_list_last_batch, gui->command_list_len);
    gui->command_list_last_batch = gui->command_list_len;
}

static GuiDrawCommand* new_draw_command(Gui* gui, GuiDrawBounds bounds, GuiDrawType draw_type, unsigned char draw_subtype, GuiDrawData data, GuiColor color) {
    GuiDrawCommand* command = &gui->command_list[gui->command_list_len++];
    command->pos_x = bounds.x;
    command->pos_y = bounds.y;
    command->width = bounds.w;
    command->height = bounds.h;
    command->type = draw_type;
    command->data = data;
    command->color = color;
    command->subtype = draw_subtype;
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

static void gui_get_anchor_pos(GuiElement* el, float* anchor_x, float* anchor_y) {
    switch (ANCHOR_X(el)) {
    case ALIGN_CENTER: *anchor_x = (float)el->w * el->scaling / 2; break;
    case ALIGN_RIGHT:  *anchor_x = (float)el->w * el->scaling; break;
    default: break;
    }
    switch (ANCHOR_Y(el)) {
    case ALIGN_CENTER: *anchor_y = (float)el->h * el->scaling / 2; break;
    case ALIGN_RIGHT:  *anchor_y = (float)el->h * el->scaling; break;
    default: break;
    }
}

static void gui_render(Gui* gui, GuiElement* el) {
    GuiBounds scissor = gui->scissor_stack_len > 0 ? gui->scissor_stack[gui->scissor_stack_len - 1] : (GuiBounds) { 0, 0, gui->win_w, gui->win_h };
    bool hover = false;

    float parent_pos_x   = 0,
          parent_pos_y   = 0,
          parent_scaling = 1.0;
    if (el->parent) {
        if (el->parent_anchor) {
            parent_pos_x = el->parent_anchor->abs_x;
            parent_pos_y = el->parent_anchor->abs_y;
        } else {
            parent_pos_x = el->parent->abs_x;
            parent_pos_y = el->parent->abs_y;
        }
        parent_scaling = el->parent->scaling;
    }

    float anchor_x = 0,
          anchor_y = 0;
    gui_get_anchor_pos(el, &anchor_x, &anchor_y);

    el->abs_x = ((float)el->x - anchor_x) * parent_scaling + parent_pos_x;
    el->abs_y = ((float)el->y - anchor_y) * parent_scaling + parent_pos_y;

    if (mouse_inside(gui, scissor_rect((GuiBounds) {
        (el->x - anchor_x) * parent_scaling + parent_pos_x,
        (el->y - anchor_y) * parent_scaling + parent_pos_y,
        el->w * el->scaling,
        el->h * el->scaling }, scissor)))
    {
        if (el->handle_hover) el->handle_hover(el);
        hover = true;
    }
    if (el->handle_pre_render) el->handle_pre_render(el);

    GuiDrawBounds el_bounds = (GuiDrawBounds) {
        parent_pos_x + ((float)el->x - anchor_x) * parent_scaling,
        parent_pos_y + ((float)el->y - anchor_y) * parent_scaling,
        (float)el->w * el->scaling,
        (float)el->h * el->scaling,
    };

    if (SCISSOR(el) || FLOATING(el) || el->shader) flush_command_batch(gui);

    if (SCISSOR(el)) {
        new_draw_command(gui, el_bounds, DRAWTYPE_SCISSOR_SET, GUI_SUBTYPE_DEFAULT, (GuiDrawData) {0}, (GuiColor) {0});
        gui->scissor_stack[gui->scissor_stack_len++] = (GuiBounds) { el_bounds.x, el_bounds.y, el_bounds.w, el_bounds.h };
    }
    if (el->shader) new_draw_command(gui, el_bounds, DRAWTYPE_SHADER_BEGIN, GUI_SUBTYPE_DEFAULT, (GuiDrawData) { .shader = el->shader }, (GuiColor) {0});

    if (el->draw_type != DRAWTYPE_UNKNOWN && inside_window(gui, el_bounds)) {
        new_draw_command(gui, el_bounds, el->draw_type, el->draw_subtype, el->data, el->color);
    }

    if (el->shader) {
        flush_command_batch(gui);
        new_draw_command(gui, el_bounds, DRAWTYPE_SHADER_END, GUI_SUBTYPE_DEFAULT, (GuiDrawData) { .shader = el->shader }, (GuiColor) {0});
    }

    for (GuiElement* iter = el->child_elements_begin; iter; iter = iter->next) {
        gui_render(gui, iter);
    }

    if (el->scroll_value) {
        int el_size = DIRECTION(el) == DIRECTION_HORIZONTAL ? el->w : el->h;
        int content_size = DIRECTION(el) == DIRECTION_HORIZONTAL ? el->cursor_x : el->cursor_y;
        int max = content_size - el_size;

        if (max > 0) {
            flush_command_batch(gui);
            GuiDrawCommand* command = &gui->command_list[gui->command_list_len++];
            command->type = DRAWTYPE_RECT;
            command->subtype = GUI_SUBTYPE_DEFAULT;
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

    if (FLOATING(el) || SCISSOR(el)) flush_command_batch(gui);

    if (SCISSOR(el)) {
        gui->scissor_stack_len--;
        if (gui->scissor_stack_len == 0) {
            new_draw_command(gui, el_bounds, DRAWTYPE_SCISSOR_RESET, GUI_SUBTYPE_DEFAULT, (GuiDrawData) {0}, (GuiColor) {0});
        } else {
            new_draw_command(gui, el_bounds, DRAWTYPE_SCISSOR_SET, GUI_SUBTYPE_DEFAULT, (GuiDrawData) {0}, (GuiColor) {0});
        }
    }
}

static GuiElement* gui_element_new(Gui* gui) {
    assert(gui->elements_arena_len < ELEMENT_STACK_SIZE);

    GuiElement* el = &gui->elements_arena[gui->elements_arena_len++];
    memset(el, 0, sizeof(*el));

    el->scaling = 1.0;
    el->size_percentage = 1.0;
    el->scroll_scaling = 64;
    return el;
}

GuiElement* gui_element_begin(Gui* gui) {
    GuiElement* parent = gui->current_element;
    GuiElement* el = gui_element_new(gui);
    gui->current_element = el;
    el->parent = parent;

    if (parent) {
        if (!parent->child_elements_end) {
            parent->child_elements_begin = el;
            parent->child_elements_end = el;
        } else {
            parent->child_elements_end->next = el;
            el->prev = parent->child_elements_end;
            parent->child_elements_end = el;
        }
        el->scaling = parent->scaling;
        el->x = parent->cursor_x;
        el->y = parent->cursor_y;
    }
    return el;
}

static void gui_element_offset(GuiElement* el, int offset_x, int offset_y) {
    for (GuiElement* iter = el->child_elements_begin; iter; iter = iter->next) {
        iter->x += offset_x;
        iter->y += offset_y;
    }
}

static void gui_element_realign(GuiElement* el) {
    int align_div;

    if (ALIGN_X(el) != ALIGN_TOP) {
        align_div = ALIGN_X(el) == ALIGN_CENTER ? 2 : 1;
        for (GuiElement* iter = el->child_elements_begin; iter; iter = iter->next) {
            if (FLOATING(iter)) continue;
            if (DIRECTION(el) == DIRECTION_VERTICAL) {
                iter->x = (el->w - iter->w) / align_div;
            } else {
                iter->x += MAX(0, (el->w - el->pad_w + el->gap - el->cursor_x) / align_div);
            }
        }
    }

    if (ALIGN_Y(el) != ALIGN_TOP) {
        align_div = ALIGN_Y(el) == ALIGN_CENTER ? 2 : 1;
        for (GuiElement* iter = el->child_elements_begin; iter; iter = iter->next) {
            if (FLOATING(iter)) continue;
            if (DIRECTION(el) == DIRECTION_VERTICAL) {
                iter->y += MAX(0, (el->h - el->pad_h + el->gap - el->cursor_y) / align_div);
            } else {
                iter->y = (el->h - iter->h) / align_div;
            }
        }
    }

    if (el->scroll_value) {
        if (DIRECTION(el) == DIRECTION_HORIZONTAL) {
            gui_element_offset(el, *el->scroll_value, 0);
        } else {
            gui_element_offset(el, 0, *el->scroll_value);
        }
    }
}

static void gui_element_resize(Gui* gui, GuiElement* el, unsigned short new_w, unsigned short new_h) {
    el->w = new_w;
    el->h = new_h;

    int left_w = el->w - el->pad_w * 2 + el->gap;
    int left_h = el->h - el->pad_h * 2 + el->gap;
    int grow_elements = 0;

    for (GuiElement* iter = el->child_elements_begin; iter; iter = iter->next) {
        if (FLOATING(iter)) continue;

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

    if (left_w < 0) left_w = 0;
    if (left_h < 0) left_h = 0;

    el->cursor_x = el->pad_w;
    el->cursor_y = el->pad_h;

    for (GuiElement* iter = el->child_elements_begin; iter; iter = iter->next) {
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
    }
    if (DIRECTION(el) == DIRECTION_HORIZONTAL) {
        el->cursor_x += el->pad_w - el->gap;
    } else {
        el->cursor_y += el->pad_h - el->gap;
    }

    gui_element_realign(el);
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
    GuiElement* el     = gui->current_element;
    GuiElement* parent = gui->current_element->parent;

    if (DIRECTION(el) == DIRECTION_VERTICAL) {
        el->h -= el->gap;
    } else {
        el->w -= el->gap;
    }

    if (!FLOATING(el)) gui_element_advance(parent, (GuiMeasurement) { el->w, el->h });

    GuiElementSizing sizing_x = SIZING_X(el),
                     sizing_y = SIZING_Y(el);
    bool has_defined_size = sizing_x != SIZING_GROW && sizing_x != SIZING_PERCENT &&
                            sizing_y != SIZING_GROW && sizing_y != SIZING_PERCENT;

    if (!has_defined_size) SET_NEED_RESIZE(parent, 1);

    if (has_defined_size && NEED_RESIZE(el) && (sizing_x == SIZING_FIXED || sizing_y == SIZING_FIXED || sizing_x == SIZING_FIT || sizing_y == SIZING_FIT)) {
        gui_element_resize(gui, el, el->w, el->h);
    } else {
        gui_element_realign(el);
    }

    gui->current_element = parent;
}

GuiElement* gui_get_element(Gui* gui) {
    return gui->current_element;
}

void gui_on_hover(Gui* gui, GuiHandler handler) {
    GuiElement* el = gui->current_element;
    el->handle_hover = handler;
}

void gui_on_render(Gui* gui, GuiHandler handler) {
    GuiElement* el = gui->current_element;
    el->handle_pre_render = handler;
}

void gui_set_anchor(Gui* gui, GuiAlignmentType anchor_x, GuiAlignmentType anchor_y) {
    GuiElement* el = gui->current_element;
    SET_ANCHOR(el, anchor_x, anchor_y);
}

void gui_set_parent_anchor(Gui* gui, GuiElement* anchor) {
    GuiElement* el = gui->current_element;
    el->parent_anchor = anchor;
}

void gui_set_shader(Gui* gui, void* shader) {
    GuiElement* el = gui->current_element;
    el->shader = shader;
}

void gui_set_scroll_scaling(Gui* gui, int scroll_scaling) {
    GuiElement* el = gui->current_element;
    el->scroll_scaling = scroll_scaling;
}

void gui_set_scroll(Gui* gui, int* scroll_value) {
    GuiElement* el = gui->current_element;
    el->scroll_value = scroll_value;
}

void gui_set_scissor(Gui* gui) {
    GuiElement* el = gui->current_element;
    SET_SCISSOR(el, 1);
}

void gui_scale_element(Gui* gui, float scaling) {
    GuiElement* el = gui->current_element;
    el->scaling *= scaling;
}

void* gui_set_state(Gui* gui, void* state, unsigned short state_len) {
    GuiElement* el = gui->current_element;
    if (el->custom_state) return el->custom_state;
    assert(gui->state_arena_len + state_len <= STATE_STACK_SIZE);

    memcpy(&gui->state_arena[gui->state_arena_len], state, state_len);
    el->custom_state = &gui->state_arena[gui->state_arena_len];
    gui->state_arena_len += state_len;
    return el->custom_state;
}

void* gui_get_state(GuiElement* el, unsigned short* state_len) {
    *state_len = el->state_len;
    return el->custom_state;
}

void gui_set_floating(Gui* gui) {
    GuiElement* el = gui->current_element;
    SET_FLOATING(el, 1);
}

void gui_set_position(Gui* gui, int x, int y) {
    GuiElement* el = gui->current_element;
    el->x = x;
    el->y = y;
}

void gui_set_custom_data(Gui* gui, void* custom_data) {
    GuiElement* el = gui->current_element;
    el->custom_data = custom_data;
}

void gui_set_fixed(Gui* gui, unsigned short w, unsigned short h) {
    GuiElement* el = gui->current_element;
    el->sizing = SIZING_FIXED | (SIZING_FIXED << 4); // This sets both dimensions to SIZING_FIXED
    el->w = w;
    el->h = h;
}

void gui_set_fit(Gui* gui, GuiElementDirection direction) {
    GuiElement* el = gui->current_element;
    if (direction == DIRECTION_VERTICAL) {
        SET_SIZING_Y(el, SIZING_FIT);
    } else {
        SET_SIZING_X(el, SIZING_FIT);
    }
}

void gui_set_padding(Gui* gui, unsigned short pad_w, unsigned short pad_h) {
    GuiElement* el = gui->current_element;
    el->pad_w = pad_w;
    el->pad_h = pad_h;
    el->w = MAX(el->w, el->pad_w * 2);
    el->h = MAX(el->h, el->pad_h * 2);
    el->cursor_x = pad_w;
    el->cursor_y = pad_h;
}

void gui_set_gap(Gui* gui, unsigned short gap) {
    GuiElement* el = gui->current_element;
    el->gap = gap;
}

void gui_set_grow(Gui* gui, GuiElementDirection direction) {
    GuiElement* el = gui->current_element;
    if (direction == DIRECTION_VERTICAL) {
        SET_SIZING_Y(el, SIZING_GROW);
        el->h = 0;
    } else {
        SET_SIZING_X(el, SIZING_GROW);
        el->w = 0;
    }
}

void gui_set_percent_size(Gui* gui, float percentage, GuiElementDirection direction) {
    GuiElement* el = gui->current_element;
    el->size_percentage = percentage;
    if (direction == DIRECTION_VERTICAL) {
        SET_SIZING_Y(el, SIZING_PERCENT);
        el->h = 0;
    } else {
        SET_SIZING_X(el, SIZING_PERCENT);
        el->w = 0;
    }
}

void gui_set_draw_subtype(Gui* gui, unsigned char subtype) {
    GuiElement* el = gui->current_element;
    el->draw_subtype = subtype;
}

void gui_set_direction(Gui* gui, GuiElementDirection direction) {
    GuiElement* el = gui->current_element;
    SET_DIRECTION(el, direction);
}

void gui_set_rect(Gui* gui, GuiColor color) {
    GuiElement* el = gui->current_element;
    el->draw_type = DRAWTYPE_RECT;
    el->color = color;
}

void gui_set_border(Gui* gui, GuiColor color, unsigned int border_width) {
    GuiElement* el = gui->current_element;
    el->draw_type = DRAWTYPE_BORDER;
    el->color = color;
    el->data.border_width = border_width;
}

void gui_set_text_slice(Gui* gui, void* font, const char* text, unsigned int text_size, unsigned short font_size, GuiColor color) {
    if (text_size == 0) return;
    GuiElement* el = gui->current_element;
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
    GuiElement* el = gui->current_element;
    GuiMeasurement image_size = gui->measure_image(image, size);
    el->draw_type = DRAWTYPE_IMAGE;
    el->color = color;
    el->data.image = image;
    el->w = image_size.w;
    el->h = image_size.h;
}

void gui_set_align(Gui* gui, GuiAlignmentType align_x, GuiAlignmentType align_y) {
    GuiElement* el = gui->current_element;
    SET_ALIGN_X(el, align_x);
    SET_ALIGN_Y(el, align_y);
}

void gui_set_min_size(Gui* gui, unsigned short min_w, unsigned short min_h) {
    GuiElement* el = gui->current_element;
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
