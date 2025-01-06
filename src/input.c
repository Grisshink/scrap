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
#include "term.h"
#include "blocks.h"

#include <assert.h>
#include <math.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

void block_delete_blockdef(ScrBlock* block, ScrBlockdef* blockdef) {
    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        if (blockdef->ref_count <= 1) break;
        if (block->arguments[i].type != ARGUMENT_BLOCK) continue;
        if (block->arguments[i].data.block.blockdef == blockdef) {
            block_free(&block->arguments[i].data.block);
            argument_set_text(&block->arguments[i], "");
            continue;
        }
        block_delete_blockdef(&block->arguments[i].data.block, blockdef);
    }
    update_measurements(block, PLACEMENT_HORIZONTAL);
}

void blockchain_delete_blockdef(ScrBlockChain* chain, ScrBlockdef* blockdef) {
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        if (blockdef->ref_count <= 1) break;
        if (chain->blocks[i].blockdef == blockdef) {
            block_free(&chain->blocks[i]);
            vector_remove(chain->blocks, i);
            i--;
            continue;
        }
        block_delete_blockdef(&chain->blocks[i], blockdef);
    }
    blockchain_update_parent_links(chain);
}

void editor_code_remove_blockdef(ScrBlockdef* blockdef) {
    for (size_t i = 0; i < vector_size(editor_code); i++) {
        if (blockdef->ref_count <= 1) break;
        blockchain_delete_blockdef(&editor_code[i], blockdef);
        if (vector_size(editor_code[i].blocks) == 0) {
            blockchain_free(&editor_code[i]);
            blockcode_remove_blockchain(&block_code, i);
            i--;
        }
    }
}

bool edit_text(char** text) {
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (vector_size(*text) <= 1) return false;

        int remove_pos = vector_size(*text) - 2;
        int remove_size = 1;

        while (((unsigned char)(*text)[remove_pos] >> 6) == 2) { // This checks if we are in the middle of UTF-8 char
            remove_pos--;
            remove_size++;
        }

        vector_erase(*text, remove_pos, remove_size);
        return true;
    }

    bool typed = false;
    int char_val;
    while ((char_val = GetCharPressed())) {
        int utf_size = 0;
        const char* utf_char = CodepointToUTF8(char_val, &utf_size);
        for (int i = 0; i < utf_size; i++) {
            vector_insert(text, vector_size(*text) - 1, utf_char[i]);
        }
        typed = true;
    }
    return typed;
}

bool start_vm(void) {
    if (vm.is_running) return false;

    term_restart();
    exec = exec_new();
    exec_copy_code(&vm, &exec, editor_code);
    if (!exec_start(&vm, &exec)) {
        actionbar_show("Start failed!");
        return false;
    }

    actionbar_show("Started successfully!");
    if (current_tab != TAB_OUTPUT) {
        shader_time = 0.0;
        current_tab = TAB_OUTPUT;
    }
    return true;
}

bool stop_vm(void) {
    if (!vm.is_running) return false;
    printf("STOP\n");
    exec_stop(&vm, &exec);
    return true;
}

void deselect_all(void) {
    hover_info.select_argument = NULL;
    hover_info.select_input = NULL;
    hover_info.select_argument_pos.x = 0;
    hover_info.select_argument_pos.y = 0;
    dropdown.scroll_amount = 0;
}

bool block_button_update_collisions(Vector2 position, EditorHoverPart part) {
    Rectangle rect;
    rect.x = position.x + conf.font_size * 0.1;
    rect.y = position.y + conf.font_size * 0.1;
    rect.width = conf.font_size * 0.8;
    rect.height = conf.font_size * 0.8;

    if (CheckCollisionPointRec(GetMousePosition(), rect)) {
        hover_info.editor.part = part;
        return true;
    }

    return false;
}

void blockdef_update_collisions(Vector2 position, ScrBlockdef* blockdef, bool editing) {
    Rectangle block_size;
    block_size.x = position.x;
    block_size.y = position.y;
    block_size.width = blockdef->ms.size.x;
    block_size.height = blockdef->ms.size.y;

    hover_info.editor.blockdef = blockdef;

    Vector2 cursor = position;
    cursor.x += BLOCK_PADDING;

    for (vec_size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (hover_info.input || hover_info.editor.part != EDITOR_BLOCKDEF) return;
        int width = 0;
        ScrInput* cur = &blockdef->inputs[i];
        Rectangle arg_size;

        switch (cur->type) {
        case INPUT_TEXT_DISPLAY:
            if (editing) {
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5;
                arg_size.width = cur->data.stext.editor_ms.size.x - conf.font_size - BLOCK_PADDING;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.input = &cur->data.stext.text;
                    break;
                }

                Vector2 arg_pos = (Vector2) { cursor.x + arg_size.width, cursor.y + block_size.height * 0.5 - conf.font_size * 0.5 };
                if (block_button_update_collisions(arg_pos, EDITOR_DEL_ARG)) {
                    hover_info.editor.blockdef_input = i;
                    break;
                }
            }
            width = cur->data.stext.editor_ms.size.x;
            break;
        case INPUT_IMAGE_DISPLAY:
            width = cur->data.simage.ms.size.x;
            break;
        case INPUT_ARGUMENT:
            width = cur->data.arg.blockdef->ms.size.x;

            Rectangle blockdef_rect;
            blockdef_rect.x = cursor.x;
            blockdef_rect.y = cursor.y + block_size.height * 0.5 - cur->data.arg.blockdef->ms.size.y * 0.5;
            blockdef_rect.width = cur->data.arg.blockdef->ms.size.x;
            blockdef_rect.height = cur->data.arg.blockdef->ms.size.y;
            if (CheckCollisionPointRec(GetMousePosition(), blockdef_rect)) {
                blockdef_update_collisions((Vector2) { blockdef_rect.x, blockdef_rect.y }, cur->data.arg.blockdef, editing);
                hover_info.editor.blockdef_input = i;
            }
            break;
        default:
            width = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0).x;
            break;
        }

        cursor.x += width + BLOCK_PADDING;
    }
}

void block_update_collisions(Vector2 position, ScrBlock* block) {
    if (hover_info.block && !block->parent) return;

    Rectangle block_size;
    block_size.x = position.x;
    block_size.y = position.y;
    block_size.width = block->ms.size.x;
    block_size.height = block->ms.size.y;
    
    if (!CheckCollisionPointRec(GetMousePosition(), block_size)) return;
    hover_info.block = block;

    Vector2 cursor = position;
    cursor.x += BLOCK_PADDING;
    if (block->ms.placement == PLACEMENT_VERTICAL) cursor.y += BLOCK_OUTLINE_SIZE * 2;

    int arg_id = 0;

    for (vec_size_t i = 0; i < vector_size(block->blockdef->inputs); i++) {
        if (hover_info.argument) return;
        int width = 0;
        int height = 0;
        ScrInput cur = block->blockdef->inputs[i];
        Rectangle arg_size;

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = cur.data.stext.ms.size.x;
            height = cur.data.stext.ms.size.y;
            break;
        case INPUT_IMAGE_DISPLAY:
            width = cur.data.simage.ms.size.x;
            height = cur.data.simage.ms.size.y;
            break;
        case INPUT_ARGUMENT:
            width = block->arguments[arg_id].ms.size.x;

            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5);
                arg_size.width = block->arguments[arg_id].ms.size.x;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;
                height = arg_size.height;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.argument = &block->arguments[arg_id];
                    hover_info.argument_pos = cursor;
                    hover_info.input = &block->arguments[arg_id].data.text;
                    break;
                }
                break;
            case ARGUMENT_BLOCK:
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height / 2 - block->arguments[arg_id].ms.size.y / 2); 
                arg_size.width = block->arguments[arg_id].ms.size.x;
                arg_size.height = block->arguments[arg_id].ms.size.y;
                height = arg_size.height;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.prev_argument = &block->arguments[arg_id];
                }
                
                block_update_collisions((Vector2) { arg_size.x, arg_size.y }, &block->arguments[arg_id].data.block);
                break;
            default:
                assert(false && "Unimplemented argument collision");
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            width = block->arguments[arg_id].ms.size.x;

            switch (block->arguments[arg_id].type) {
            case ARGUMENT_CONST_STRING:
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5);
                arg_size.width = block->arguments[arg_id].ms.size.x;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;
                height = arg_size.height;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.argument = &block->arguments[arg_id];
                    hover_info.argument_pos = cursor;
                    break;
                }
                break;
            case ARGUMENT_TEXT:
                assert(false && "Illegal argument type ARGUMENT_TEXT");
                break;
            case ARGUMENT_BLOCK:
                assert(false && "Illegal argument type ARGUMENT_BLOCK");
                break;
            default:
                assert(false && "Unimplemented argument collision");
                break;
            }
            arg_id++;
            break;
        case INPUT_BLOCKDEF_EDITOR:
            assert(block->arguments[arg_id].type == ARGUMENT_BLOCKDEF);
            width = block->arguments[arg_id].ms.size.x;
            height = block->arguments[arg_id].ms.size.y;

            Vector2 blockdef_size = as_rl_vec(block->arguments[arg_id].data.blockdef->ms.size);
            Vector2 arg_pos;
            arg_pos.x = cursor.x;
            arg_pos.y = cursor.y + (block->ms.placement == PLACEMENT_VERTICAL ? 0 : block_size.height * 0.5 - height * 0.5);
            if (!CheckCollisionPointRec(GetMousePosition(), (Rectangle) { arg_pos.x, arg_pos.y, width, height })) {
                arg_id++;
                break;
            }

            hover_info.argument = &block->arguments[arg_id];
            hover_info.argument_pos = cursor;

            Rectangle blockdef_rect;
            blockdef_rect.x = arg_pos.x + BLOCK_OUTLINE_SIZE;
            blockdef_rect.y = arg_pos.y + height * 0.5 - blockdef_size.y * 0.5;
            blockdef_rect.width = blockdef_size.x;
            blockdef_rect.height = blockdef_size.y;

            if (CheckCollisionPointRec(GetMousePosition(), blockdef_rect)) {
                hover_info.editor.part = EDITOR_BLOCKDEF;
                ScrBlockdef* editor_blockdef = block->arguments[arg_id].data.blockdef;
                blockdef_update_collisions((Vector2) { blockdef_rect.x, blockdef_rect.y }, editor_blockdef, hover_info.editor.edit_blockdef == editor_blockdef);
                arg_id++;
                break;
            }
            arg_pos.x += blockdef_size.x + BLOCK_PADDING * 0.5;
            
            if (hover_info.editor.edit_blockdef == block->arguments[arg_id].data.blockdef) {
                if (block_button_update_collisions((Vector2) {arg_pos.x, arg_pos.y + height * 0.5 - conf.font_size * 0.5 }, EDITOR_ADD_ARG)) {
                    arg_id++;
                    break;
                }
                arg_pos.x += conf.font_size + BLOCK_PADDING * 0.5;

                if (block_button_update_collisions((Vector2) {arg_pos.x, arg_pos.y + height * 0.5 - conf.font_size * 0.5 }, EDITOR_ADD_TEXT)) {
                    arg_id++;
                    break;
                }
                arg_pos.x += conf.font_size + BLOCK_PADDING * 0.5;
            }

            if (block_button_update_collisions((Vector2) {arg_pos.x, arg_pos.y + height * 0.5 - conf.font_size * 0.5 }, EDITOR_EDIT)) {
                arg_id++;
                break;
            }

            arg_id++;
            break;
        default: ; // This fixes gcc-9 error
            Vector2 size = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0);
            width = size.x;
            height = size.y;
            break;
        }
        if (block->ms.placement == PLACEMENT_VERTICAL) {
            cursor.y += height + BLOCK_OUTLINE_SIZE * 2;
        } else {
            cursor.x += width + BLOCK_PADDING;
        }
    }
}

void blockchain_check_collisions(ScrBlockChain* chain, Vector2 camera_pos) {
    vector_clear(draw_stack);

    hover_info.blockchain = chain;
    hover_info.blockchain_layer = 0;
    Vector2 pos = as_rl_vec(hover_info.blockchain->pos);
    pos.x -= camera_pos.x;
    pos.y -= camera_pos.y;
    for (vec_size_t i = 0; i < vector_size(hover_info.blockchain->blocks); i++) {
        if (hover_info.block) break;
        hover_info.blockchain_layer = vector_size(draw_stack);
        hover_info.blockchain_index = i;

        ScrBlockdef* blockdef = chain->blocks[i].blockdef;
        if ((blockdef->type == BLOCKTYPE_END || blockdef->type == BLOCKTYPE_CONTROLEND) && vector_size(draw_stack) > 0) {
            pos.x -= BLOCK_CONTROL_INDENT;

            if (blockdef->type == BLOCKTYPE_END) {
                DrawStack prev_block = draw_stack[vector_size(draw_stack) - 1];
                Rectangle rect;
                rect.x = pos.x;
                rect.y = pos.y;
                rect.width = prev_block.block->ms.size.x;
                rect.height = conf.font_size;

                if (CheckCollisionPointRec(GetMousePosition(), rect)) {
                    hover_info.block = &hover_info.blockchain->blocks[i];
                }
            } else if (blockdef->type == BLOCKTYPE_CONTROLEND) {
                block_update_collisions(pos, &hover_info.blockchain->blocks[i]);
            }
            vector_pop(draw_stack);
        } else {
            block_update_collisions(pos, &hover_info.blockchain->blocks[i]);
        }

        if (blockdef->type == BLOCKTYPE_CONTROL || blockdef->type == BLOCKTYPE_CONTROLEND) {
            DrawStack stack_item;
            stack_item.pos = as_scr_vec(pos);
            stack_item.block = &chain->blocks[i];
            vector_add(&draw_stack, stack_item);
            pos.x += BLOCK_CONTROL_INDENT;
        }
        pos.y += hover_info.blockchain->blocks[i].ms.size.y;
    }
}

bool button_check_collisions(Vector2* position, char* text, float button_scale, float side_padding, float side_margin) {
    side_padding *= conf.font_size;
    side_margin *= conf.font_size;

    int text_size = conf.font_size * 0.6;
    int text_width = text ? MeasureTextEx(font_cond, text, text_size, 0.0).x : 0;
    Rectangle rect = {
        .x = position->x,
        .y = position->y,
        .width = text_width + side_padding * 2,
        .height = conf.font_size * button_scale,
    };

    position->x += rect.width + side_margin;
    return CheckCollisionPointRec(GetMousePosition(), rect) && vector_size(mouse_blockchain.blocks) == 0;
}

void bars_check_collisions(void) {
    Vector2 pos = (Vector2){ 0.0, conf.font_size * 1.2 };
    for (vec_size_t i = 0; i < ARRLEN(tab_bar_buttons_text); i++) {
        Vector2 last_pos = pos;
        if (button_check_collisions(&pos, tab_bar_buttons_text[i], 1.0, 0.3, 0)) {
            hover_info.top_bars.type = TOPBAR_TABS;
            hover_info.top_bars.pos = last_pos;
            hover_info.top_bars.ind = i;
            return;
        }
    }

    Vector2 run_pos = (Vector2){ GetScreenWidth() - conf.font_size * 2.0, conf.font_size * 1.2 };
    for (int i = 0; i < 2; i++) {
        Vector2 last_pos = run_pos;
        if (button_check_collisions(&run_pos, NULL, 1.0, 0.5, 0)) {
            hover_info.top_bars.type = TOPBAR_RUN_BUTTON;
            hover_info.top_bars.pos = last_pos;
            hover_info.top_bars.ind = i;
            return;
        }
    }

    int width = MeasureTextEx(font_eb, "Scrap", conf.font_size * 0.8, 0.0).x;
    pos = (Vector2){ 20 + conf.font_size + width, 0 };
    for (vec_size_t i = 0; i < ARRLEN(top_bar_buttons_text); i++) {
        Vector2 last_pos = pos;
        if (button_check_collisions(&pos, top_bar_buttons_text[i], 1.2, 0.3, 0)) {
            hover_info.top_bars.type = TOPBAR_TOP;
            hover_info.top_bars.pos = last_pos;
            hover_info.top_bars.ind = i;
            return;
        }
    }
}

void dropdown_check_collisions(void) {
    if (!hover_info.select_argument) return;

    ScrInput block_input = hover_info.select_block->blockdef->inputs[hover_info.select_argument->input_id];

    if (block_input.type != INPUT_DROPDOWN) return;

    dropdown.ms.size.x = hover_info.select_argument->ms.size.x;
    dropdown.ms.size.y = 5.0;

    size_t list_len = 0;
    char** list = block_input.data.drop.list(hover_info.select_block, &list_len);

    Vector2 pos = hover_info.select_argument_pos;
    pos.y += hover_info.select_block->ms.size.y;
    for (size_t i = dropdown.scroll_amount; i < list_len; i++) {
        if (pos.y > GetScreenHeight()) break;
        Vector2 text_ms = MeasureTextEx(font_cond, list[i], BLOCK_TEXT_SIZE, 0);
        dropdown.ms.size.x = MAX(text_ms.x + 10, dropdown.ms.size.x);
        dropdown.ms.size.y += conf.font_size;
        pos.y += conf.font_size;
    }

    pos = hover_info.select_argument_pos;
    pos.y += hover_info.select_block->ms.size.y;

    for (size_t i = dropdown.scroll_amount; i < list_len; i++) {
        if (pos.y > GetScreenHeight()) break;
        Rectangle rect;
        rect.x = pos.x;
        rect.y = pos.y;
        rect.width = dropdown.ms.size.x;
        rect.height = conf.font_size;

        if (CheckCollisionPointRec(GetMousePosition(), rect)) {
            hover_info.dropdown_hover_ind = i;
            break;
        }
        pos.y += conf.font_size;
    }
}

void check_block_collisions(void) {
    if (current_tab != TAB_CODE) return;
    if (hover_info.sidebar) {
        int pos_y = conf.font_size * 2.2 + SIDE_BAR_PADDING - sidebar.scroll_amount;
        for (vec_size_t i = 0; i < vector_size(sidebar.blocks); i++) {
            if (hover_info.block) break;
            block_update_collisions((Vector2){ SIDE_BAR_PADDING, pos_y }, &sidebar.blocks[i]);
            pos_y += conf.font_size + SIDE_BAR_PADDING;
        }
    } else {
        for (vec_size_t i = 0; i < vector_size(editor_code); i++) {
            if (hover_info.block) break;
            blockchain_check_collisions(&editor_code[i], camera_pos);
        }
    }
}

bool handle_top_bar_click(void) {
    if (hover_info.top_bars.type == TOPBAR_TOP) {
        switch (hover_info.top_bars.ind) {
        case 0:
            if (!vm.is_running) gui_show(GUI_TYPE_FILE);
            break;
        case 1:
            gui_show(GUI_TYPE_SETTINGS);
            break;
        case 2:
            gui_show(GUI_TYPE_ABOUT);
            break;
        default:
            break;
        }
    } else if (hover_info.top_bars.type == TOPBAR_TABS) {
        if (current_tab != (TabType)hover_info.top_bars.ind) {
            shader_time = 0.0;
            current_tab = hover_info.top_bars.ind;
        }
    } else if (hover_info.top_bars.type == TOPBAR_RUN_BUTTON) {
        if (hover_info.top_bars.ind == 1) {
            start_vm();
        } else if (hover_info.top_bars.ind == 0) {
            stop_vm();
        }
    }
    return true;
}

bool handle_sidebar_click(bool mouse_empty) {
    if (hover_info.select_argument) {
        deselect_all();
        return true;
    }
    if (mouse_empty && hover_info.block) {
        // Pickup block
        blockchain_add_block(&mouse_blockchain, block_new_ms(hover_info.block->blockdef));
        if (hover_info.block->blockdef->type == BLOCKTYPE_CONTROL && vm.end_blockdef) {
            blockchain_add_block(&mouse_blockchain, block_new_ms(vm.blockdefs[vm.end_blockdef]));
        }
        return true;
    } else if (!mouse_empty) {
        // Drop block
        for (size_t i = 0; i < vector_size(mouse_blockchain.blocks); i++) {
            for (size_t j = 0; j < vector_size(mouse_blockchain.blocks[i].arguments); j++) {
                ScrArgument* arg = &mouse_blockchain.blocks[i].arguments[j];
                if (arg->type != ARGUMENT_BLOCKDEF) continue;
                if (arg->data.blockdef->ref_count > 1) editor_code_remove_blockdef(arg->data.blockdef);
                for (size_t k = 0; k < vector_size(arg->data.blockdef->inputs); k++) {
                    ScrInput* input = &arg->data.blockdef->inputs[k];
                    if (input->type != INPUT_ARGUMENT) continue;
                    if (input->data.arg.blockdef->ref_count > 1) editor_code_remove_blockdef(input->data.arg.blockdef);
                }
            }
        }
        blockchain_clear_blocks(&mouse_blockchain);
        return true;
    }
    return true;
}

bool handle_blockdef_editor_click(void) {
    ScrBlockdef* blockdef = hover_info.argument->data.blockdef;
    size_t last_input = vector_size(blockdef->inputs);
    char str[32];
    switch (hover_info.editor.part) {
    case EDITOR_ADD_ARG:
        // TODO: Update block arguments when new argument is added
        if (blockdef->ref_count > 1) {
            deselect_all();
            return true;
        }
        for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
            if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
            if (blockdef->inputs[i].data.arg.blockdef->ref_count > 1) {
                deselect_all();
                return true;
            }
        }
        blockdef_add_argument(hover_info.argument->data.blockdef, "", BLOCKCONSTR_UNLIMITED);
        sprintf(str, "arg%zu", last_input);
        ScrBlockdef* arg_blockdef = hover_info.argument->data.blockdef->inputs[last_input].data.arg.blockdef;
        blockdef_add_text(arg_blockdef, str);
        arg_blockdef->func = block_custom_arg;

        int arg_count = 0;
        for (size_t i = 0; i < vector_size(hover_info.argument->data.blockdef->inputs); i++) {
            if (hover_info.argument->data.blockdef->inputs[i].type == INPUT_ARGUMENT) arg_count++;
        }
        arg_blockdef->arg_id = arg_count - 1;

        update_measurements(hover_info.block, PLACEMENT_HORIZONTAL);
        deselect_all();
        return true;
    case EDITOR_ADD_TEXT:
        // TODO: Update block arguments when new argument is added
        if (blockdef->ref_count > 1) {
            deselect_all();
            return true;
        }
        for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
            if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
            if (blockdef->inputs[i].data.arg.blockdef->ref_count > 1) {
                deselect_all();
                return true;
            }
        }
        sprintf(str, "text%zu", last_input);
        blockdef_add_text(hover_info.argument->data.blockdef, str);
        update_measurements(hover_info.block, PLACEMENT_HORIZONTAL);
        deselect_all();
        return true;
    case EDITOR_DEL_ARG:
        assert(hover_info.editor.blockdef_input != (size_t)-1);
        if (blockdef->ref_count > 1) {
            deselect_all();
            return true;
        }
        for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
            if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
            if (blockdef->inputs[i].data.arg.blockdef->ref_count > 1) {
                deselect_all();
                return true;
            }
        }

        bool is_arg = blockdef->inputs[hover_info.editor.blockdef_input].type == INPUT_ARGUMENT;
        blockdef_delete_input(blockdef, hover_info.editor.blockdef_input);
        if (is_arg) {
            for (size_t i = hover_info.editor.blockdef_input; i < vector_size(blockdef->inputs); i++) {
                if (blockdef->inputs[i].type != INPUT_ARGUMENT) continue;
                blockdef->inputs[i].data.arg.blockdef->arg_id--;
            }
        }

        update_measurements(hover_info.block, PLACEMENT_HORIZONTAL);
        deselect_all();
        return true;
    case EDITOR_EDIT:
        if (hover_info.editor.edit_blockdef == hover_info.argument->data.blockdef) {
            hover_info.editor.edit_blockdef = NULL;
            hover_info.editor.edit_block = NULL;
        } else {
            hover_info.editor.edit_blockdef = hover_info.argument->data.blockdef;
            if (hover_info.editor.edit_block) update_measurements(hover_info.editor.edit_block, PLACEMENT_HORIZONTAL);
            hover_info.editor.edit_block = hover_info.block;
        }
        update_measurements(hover_info.block, PLACEMENT_HORIZONTAL);
        deselect_all();
        return true;
    case EDITOR_BLOCKDEF:
        if (hover_info.editor.edit_blockdef == hover_info.argument->data.blockdef) return false;
        blockchain_add_block(&mouse_blockchain, block_new_ms(hover_info.editor.blockdef));
        deselect_all();
        return true;
    default:
        return false;
    }
}

bool handle_code_editor_click(bool mouse_empty) {
    if (!mouse_empty) {
        mouse_blockchain.pos = as_scr_vec(GetMousePosition());
        if (hover_info.argument || hover_info.prev_argument) {
            if (vector_size(mouse_blockchain.blocks) > 1) return true;
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_CONTROLEND) return true;
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return true;

            if (hover_info.argument) {
                // Attach to argument
                printf("Attach to argument\n");
                if (hover_info.argument->type != ARGUMENT_TEXT) return true;
                mouse_blockchain.blocks[0].parent = hover_info.block;
                argument_set_block(hover_info.argument, mouse_blockchain.blocks[0]);
                update_measurements(&hover_info.argument->data.block, PLACEMENT_HORIZONTAL);
                vector_clear(mouse_blockchain.blocks);
            } else if (hover_info.prev_argument) {
                // Swap argument
                printf("Swap argument\n");
                if (hover_info.prev_argument->type != ARGUMENT_BLOCK) return true;
                mouse_blockchain.blocks[0].parent = hover_info.block->parent;
                ScrBlock temp = mouse_blockchain.blocks[0];
                mouse_blockchain.blocks[0] = *hover_info.block;
                mouse_blockchain.blocks[0].parent = NULL;
                block_update_parent_links(&mouse_blockchain.blocks[0]);
                argument_set_block(hover_info.prev_argument, temp);
                update_measurements(temp.parent, PLACEMENT_HORIZONTAL);
            }
        } else if (
            hover_info.block && 
            hover_info.blockchain && 
            hover_info.block->parent == NULL
        ) {
            // Attach block
            printf("Attach block\n");
            if (mouse_blockchain.blocks[0].blockdef->type == BLOCKTYPE_HAT) return true;
            blockchain_insert(hover_info.blockchain, &mouse_blockchain, hover_info.blockchain_index);
            // Update block link to make valgrind happy
            hover_info.block = &hover_info.blockchain->blocks[hover_info.blockchain_index];
        } else {
            // Put block
            printf("Put block\n");
            mouse_blockchain.pos.x += camera_pos.x;
            mouse_blockchain.pos.y += camera_pos.y;
            blockcode_add_blockchain(&block_code, mouse_blockchain);
            mouse_blockchain = blockchain_new();
        }
        return true;
    } else if (hover_info.block) {
        if (hover_info.block->parent) {
            if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                // Copy argument
                printf("Copy argument\n");
                blockchain_add_block(&mouse_blockchain, block_copy(hover_info.block, NULL));
            } else {
                // Detach argument
                printf("Detach argument\n");
                assert(hover_info.prev_argument != NULL);

                blockchain_add_block(&mouse_blockchain, *hover_info.block);
                mouse_blockchain.blocks[0].parent = NULL;

                ScrBlock* parent = hover_info.prev_argument->data.block.parent;
                argument_set_text(hover_info.prev_argument, "");
                hover_info.prev_argument->ms.size = as_scr_vec(MeasureTextEx(font_cond, "", BLOCK_TEXT_SIZE, 0.0));
                update_measurements(parent, PLACEMENT_HORIZONTAL);
            }
        } else if (hover_info.blockchain) {
            if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Copy block
                    printf("Copy block\n");
                    blockchain_free(&mouse_blockchain);
                    mouse_blockchain = blockchain_copy_single(hover_info.blockchain, hover_info.blockchain_index);
                } else {
                    // Copy chain
                    printf("Copy chain\n");
                    blockchain_free(&mouse_blockchain);
                    mouse_blockchain = blockchain_copy(hover_info.blockchain, hover_info.blockchain_index);
                }
            } else {
                hover_info.editor.edit_blockdef = NULL;
                if (hover_info.editor.edit_block) update_measurements(hover_info.editor.edit_block, PLACEMENT_HORIZONTAL);
                hover_info.editor.edit_block = NULL;
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Detach block
                    printf("Detach block\n");
                    blockchain_detach_single(&mouse_blockchain, hover_info.blockchain, hover_info.blockchain_index);
                    if (vector_size(hover_info.blockchain->blocks) == 0) {
                        blockchain_free(hover_info.blockchain);
                        blockcode_remove_blockchain(&block_code, hover_info.blockchain - editor_code);
                        hover_info.block = NULL;
                    }
                } else {
                    // Detach chain
                    printf("Detach chain\n");
                    blockchain_detach(&mouse_blockchain, hover_info.blockchain, hover_info.blockchain_index);
                    if (hover_info.blockchain_index == 0) {
                        blockchain_free(hover_info.blockchain);
                        blockcode_remove_blockchain(&block_code, hover_info.blockchain - editor_code);
                        hover_info.block = NULL;
                    }
                }
            }
        }
        return true;
    }
    return false;
}

// Return value indicates if we should cancel dragging
bool handle_mouse_click(void) {
    hover_info.mouse_click_pos = GetMousePosition();
    camera_click_pos = camera_pos;

    if (gui_is_shown()) {
        if (gui_get_type() == GUI_TYPE_FILE) gui_hide_immediate();
        return true;
    }
    if (hover_info.top_bars.ind != -1) return handle_top_bar_click();
    if (current_tab != TAB_CODE) return true;
    if (vm.is_running) return false;

    bool mouse_empty = vector_size(mouse_blockchain.blocks) == 0;

    if (hover_info.sidebar) return handle_sidebar_click(mouse_empty);

    if (mouse_empty && hover_info.argument && hover_info.argument->type == ARGUMENT_BLOCKDEF) {
        if (handle_blockdef_editor_click()) return true;
    }

    if (mouse_empty) {
        if (hover_info.dropdown_hover_ind != -1) {
            ScrInput block_input = hover_info.select_block->blockdef->inputs[hover_info.select_argument->input_id];
            assert(block_input.type == INPUT_DROPDOWN);
            
            size_t list_len = 0;
            char** list = block_input.data.drop.list(hover_info.select_block, &list_len);
            assert((size_t)hover_info.dropdown_hover_ind < list_len);

            argument_set_const_string(hover_info.select_argument, list[hover_info.dropdown_hover_ind]);
            hover_info.select_argument->ms.size = as_scr_vec(MeasureTextEx(font_cond, list[hover_info.dropdown_hover_ind], BLOCK_TEXT_SIZE, 0.0));
            update_measurements(hover_info.select_block, PLACEMENT_HORIZONTAL);
        }

        if (hover_info.block != hover_info.select_block) hover_info.select_block = hover_info.block;
        if (hover_info.input != hover_info.select_input) hover_info.select_input = hover_info.input;
        if (hover_info.argument != hover_info.select_argument) {
            hover_info.select_argument = hover_info.argument;
            hover_info.select_argument_pos = hover_info.argument_pos;
            dropdown.scroll_amount = 0;
            return true;
        }
        if (hover_info.select_argument) return true;
    }

    if (handle_code_editor_click(mouse_empty)) return true;
    return false;
}

void handle_key_press(void) {
    if (IsKeyPressed(KEY_F5)) {
        start_vm();
        return;
    }
    if (IsKeyPressed(KEY_F6)) {
        stop_vm();
        return;
    }

    if (current_tab == TAB_OUTPUT) {
        if (!vm.is_running) return;
        if (IsKeyPressed(KEY_ENTER)) {
            term_input_put_char('\n');
            term_print_str("\r\n");
            return;
        }

        int char_val;
        while ((char_val = GetCharPressed())) {
            int utf_size = 0;
            const char* utf_char = CodepointToUTF8(char_val, &utf_size);
            for (int i = 0; i < utf_size; i++) {
                term_input_put_char(utf_char[i]);
            }
            // CodepointToUTF8() returns an array, not a null terminated string, so we copy it to satisfy constraints
            char utf_str[7];
            memcpy(utf_str, utf_char, utf_size);
            utf_str[utf_size] = 0;
            term_print_str(utf_str);
        }
        return;
    }

    if (!hover_info.select_input) {
        if (IsKeyPressed(KEY_SPACE) && vector_size(editor_code) > 0) {
            blockchain_select_counter++;
            if ((vec_size_t)blockchain_select_counter >= vector_size(editor_code)) blockchain_select_counter = 0;

            camera_pos.x = editor_code[blockchain_select_counter].pos.x - ((GetScreenWidth() - conf.side_bar_size) / 2 + conf.side_bar_size);
            camera_pos.y = editor_code[blockchain_select_counter].pos.y - ((GetScreenHeight() - conf.font_size * 2.2) / 2 + conf.font_size * 2.2);
            actionbar_show(TextFormat("Jump to chain (%d/%d)", blockchain_select_counter + 1, vector_size(editor_code)));
            return;
        }
        return;
    };
    if (hover_info.select_block->blockdef->inputs[hover_info.select_argument->input_id].type == INPUT_DROPDOWN) return;

    if (edit_text(hover_info.select_input)) {
        update_measurements(hover_info.select_block, PLACEMENT_HORIZONTAL);
        return;
    }
}

void handle_mouse_wheel(void) {
    if (current_tab != TAB_CODE) return;

    Vector2 wheel = GetMouseWheelMoveV();

    dropdown.scroll_amount = MAX(dropdown.scroll_amount - wheel.y, 0);
    if (hover_info.sidebar) {
        sidebar.scroll_amount = MAX(sidebar.scroll_amount - wheel.y * (conf.font_size + SIDE_BAR_PADDING) * 6, 0);
    } else {
        if (hover_info.select_argument) return;
        camera_pos.x -= wheel.x * conf.font_size * 2;
        camera_pos.y -= wheel.y * conf.font_size * 2;
    }
}

void handle_mouse_drag(void) {
    if (hover_info.drag_cancelled) return;

    Vector2 mouse_pos = GetMousePosition();

    camera_pos.x = camera_click_pos.x - (mouse_pos.x - hover_info.mouse_click_pos.x);
    camera_pos.y = camera_click_pos.y - (mouse_pos.y - hover_info.mouse_click_pos.y);
}

void process_input(void) {
    hover_info.sidebar = GetMouseX() < conf.side_bar_size && GetMouseY() > conf.font_size * 2.2;
    hover_info.block = NULL;
    hover_info.argument = NULL;
    hover_info.input = NULL;
    hover_info.argument_pos.x = 0;
    hover_info.argument_pos.y = 0;
    hover_info.prev_argument = NULL;
    hover_info.blockchain = NULL;
    hover_info.blockchain_index = -1;
    hover_info.blockchain_layer = 0;
    hover_info.dropdown_hover_ind = -1;
    hover_info.top_bars.ind = -1;
    hover_info.exec_ind = -1;
    hover_info.exec_chain = NULL;
    hover_info.editor.part = EDITOR_NONE;
    hover_info.editor.blockdef = NULL;
    hover_info.editor.blockdef_input = -1;

    Vector2 mouse_pos = GetMousePosition();
    if ((int)hover_info.last_mouse_pos.x == (int)mouse_pos.x && (int)hover_info.last_mouse_pos.y == (int)mouse_pos.y) {
        hover_info.time_at_last_pos += GetFrameTime();
    } else {
        hover_info.last_mouse_pos = mouse_pos;
        hover_info.time_at_last_pos = 0;
    }

    dropdown_check_collisions();
    if (!gui_is_shown()) {
        check_block_collisions();
        bars_check_collisions();
    }

    update_gui();

    if (GetMouseWheelMove() != 0.0) {
        handle_mouse_wheel();
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        hover_info.drag_cancelled = handle_mouse_click();
#ifdef DEBUG
        // This will traverse through all blocks in codebase, which is expensive in large codebase.
        // Ideally all functions should not be broken in the first place. This helps with debugging invalid states
        sanitize_links();
#endif
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        hover_info.mouse_click_pos = GetMousePosition();
        camera_click_pos = camera_pos;
    } else if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        handle_mouse_drag();
    } else {
        hover_info.drag_cancelled = false;
        handle_key_press();
    }

    if (IsWindowResized()) {
        shader_time = 0.0;
        term_resize();
    }

    if (sidebar.max_y > GetScreenHeight()) {
        sidebar.scroll_amount = MIN(sidebar.scroll_amount, sidebar.max_y - GetScreenHeight());
    } else {
        sidebar.scroll_amount = 0;
    }

    mouse_blockchain.pos = as_scr_vec(GetMousePosition());
}
