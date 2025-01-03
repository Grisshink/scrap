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

#include "scrap.h"

#include <assert.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

ScrMeasurement measure_text(char* text) {
    ScrMeasurement ms = {0};
    ms.size = as_scr_vec(MeasureTextEx(font_cond, text, BLOCK_TEXT_SIZE, 0.0));
    ms.placement = PLACEMENT_HORIZONTAL;
    return ms;
}

ScrMeasurement measure_image(ScrImage image, float size) {
    Texture2D* texture = image.image_ptr; ScrMeasurement ms = {0};
    ms.size.x = size / (float)texture->height * (float)texture->width;
    ms.size.y = size; ms.placement = PLACEMENT_HORIZONTAL;
    return ms;
}

ScrMeasurement measure_input_box(const char* input) {
    ScrMeasurement ms;
    ms.size = as_scr_vec(MeasureTextEx(font_cond, input, BLOCK_TEXT_SIZE, 0.0));
    ms.size.x += BLOCK_STRING_PADDING;
    ms.size.x = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, ms.size.x);
    ms.size.y = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, ms.size.y);
    ms.placement = PLACEMENT_HORIZONTAL;
    return ms;
}

ScrMeasurement measure_block_button(void) {
    ScrMeasurement ms;
    ms.size.x = conf.font_size;
    ms.size.y = conf.font_size;
    ms.placement = PLACEMENT_HORIZONTAL;
    return ms;
}

ScrMeasurement measure_group(ScrMeasurement left, ScrMeasurement right, float padding) {
    ScrMeasurement ms = left; ms.size.x += right.size.x + padding; ms.size.y = MAX(left.size.y, right.size.y);
    return ms;
}

void blockdef_update_measurements(ScrBlockdef* blockdef, bool editing) {
    blockdef->ms.size.x = BLOCK_PADDING;
    blockdef->ms.placement = PLACEMENT_HORIZONTAL;
    blockdef->ms.size.y = conf.font_size;

    for (vec_size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        ScrMeasurement ms;
        ms.placement = PLACEMENT_HORIZONTAL;

        switch (blockdef->inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            if (editing) {
                ms = measure_input_box(blockdef->inputs[i].data.stext.text);
                ms = measure_group(ms, measure_block_button(), BLOCK_PADDING);
            } else {
                ms = measure_text(blockdef->inputs[i].data.stext.text);
            }
            blockdef->inputs[i].data.stext.editor_ms = ms;
            break;
        case INPUT_IMAGE_DISPLAY:
            ms = measure_image(blockdef->inputs[i].data.simage.image, BLOCK_IMAGE_SIZE);
            blockdef->inputs[i].data.simage.ms = ms;
            break;
        case INPUT_ARGUMENT:
            blockdef_update_measurements(blockdef->inputs[i].data.arg.blockdef, editing);
            ms = blockdef->inputs[i].data.arg.blockdef->ms;
            break;
        case INPUT_DROPDOWN:
            ms.size = as_scr_vec(MeasureTextEx(font_cond, "Dropdown", BLOCK_TEXT_SIZE, 0.0));
            break;
        case INPUT_BLOCKDEF_EDITOR:
            assert(false && "Unimplemented");
            break;
        default:
            ms.size = as_scr_vec(MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0));
            break;
        }
        ms.size.x += BLOCK_PADDING;

        blockdef->ms.size.x += ms.size.x;
        blockdef->ms.size.y = MAX(blockdef->ms.size.y, ms.size.y + BLOCK_OUTLINE_SIZE * 4);
    }
}

void update_measurements(ScrBlock* block, ScrPlacementStrategy placement) {
    block->ms.size.x = BLOCK_PADDING;
    block->ms.placement = placement;
    block->ms.size.y = placement == PLACEMENT_HORIZONTAL ? conf.font_size : BLOCK_OUTLINE_SIZE * 2;

    int arg_id = 0;
    for (vec_size_t i = 0; i < vector_size(block->blockdef->inputs); i++) {
        ScrMeasurement ms;

        switch (block->blockdef->inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            ms = measure_text(block->blockdef->inputs[i].data.stext.text);
            block->blockdef->inputs[i].data.stext.ms = ms;
            break;
        case INPUT_IMAGE_DISPLAY:
            ms = measure_image(block->blockdef->inputs[i].data.simage.image, BLOCK_IMAGE_SIZE);
            block->blockdef->inputs[i].data.simage.ms = ms;
            break;
        case INPUT_ARGUMENT:
            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                ScrMeasurement string_ms = measure_input_box(block->arguments[arg_id].data.text);
                block->arguments[arg_id].ms = string_ms;
                ms = string_ms;
                break;
            case ARGUMENT_BLOCK:
                block->arguments[arg_id].ms = block->arguments[arg_id].data.block.ms;
                ms = block->arguments[arg_id].ms;
                break;
            default:
                assert(false && "Unimplemented argument measure");
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            assert(block->arguments[arg_id].type == ARGUMENT_CONST_STRING);

            ScrMeasurement arg_ms = measure_input_box(block->arguments[arg_id].data.text);
            ScrMeasurement img_ms = measure_image((ScrImage) { .image_ptr = &drop_tex }, BLOCK_IMAGE_SIZE);
            ms = measure_group(arg_ms, img_ms, 0.0);
            block->arguments[arg_id].ms = ms;
            arg_id++;
            break;
        case INPUT_BLOCKDEF_EDITOR:
            ScrBlockdef* blockdef = block->arguments[arg_id].data.blockdef;
            blockdef_update_measurements(blockdef, hover_info.editor.edit_blockdef == blockdef);
            ScrMeasurement editor_ms = block->arguments[arg_id].data.blockdef->ms;
            ScrMeasurement button_ms = measure_block_button();
            if (hover_info.editor.edit_blockdef == block->arguments[arg_id].data.blockdef) {
                button_ms = measure_group(button_ms, measure_block_button(), BLOCK_PADDING);
                button_ms = measure_group(button_ms, measure_block_button(), BLOCK_PADDING);
            }

            ms = measure_group(editor_ms, button_ms, BLOCK_PADDING);
            block->arguments[arg_id].ms = ms;
            arg_id++;
            break;
        default:
            ms.size = as_scr_vec(MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0));
            break;
        }

        if (placement == PLACEMENT_VERTICAL) {
            ms.size.y += BLOCK_OUTLINE_SIZE * 2;
            block->ms.size.x = MAX(block->ms.size.x, ms.size.x + BLOCK_PADDING * 2);
            block->ms.size.y += ms.size.y;
        } else {
            ms.size.x += BLOCK_PADDING;
            block->ms.size.x += ms.size.x;
            block->ms.size.y = MAX(block->ms.size.y, ms.size.y + BLOCK_OUTLINE_SIZE * 4);
        }
    }

    if (block->ms.size.x > conf.block_size_threshold && block->ms.placement == PLACEMENT_HORIZONTAL) {
        update_measurements(block, PLACEMENT_VERTICAL);
        return;
    }

    if (block->parent) update_measurements(block->parent, PLACEMENT_HORIZONTAL);
}

void blockcode_update_measurments(BlockCode* blockcode) {
    blockcode->max_pos = (Vector2) { -1.0 / 0.0, -1.0 / 0.0 };
    blockcode->min_pos = (Vector2) { 1.0 / 0.0, 1.0 / 0.0 };

    for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
        blockcode->max_pos.x = MAX(blockcode->max_pos.x, editor_code[i].pos.x);
        blockcode->max_pos.y = MAX(blockcode->max_pos.y, editor_code[i].pos.y);
        blockcode->min_pos.x = MIN(blockcode->min_pos.x, editor_code[i].pos.x);
        blockcode->min_pos.y = MIN(blockcode->min_pos.y, editor_code[i].pos.y);
    }
}
