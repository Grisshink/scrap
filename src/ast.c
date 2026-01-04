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

#include "ast.h"
#include "vec.h"

#include <assert.h>
#include <libintl.h>
#include <string.h>

const char* type_to_str(DataType type) {
    switch (type) {
    case DATA_TYPE_NOTHING:
        return "nothing";
    case DATA_TYPE_INTEGER:
        return "integer";
    case DATA_TYPE_FLOAT:
        return "float";
    case DATA_TYPE_STRING:
        return "str";
    case DATA_TYPE_LITERAL:
        return "literal";
    case DATA_TYPE_BOOL:
        return "bool";
    case DATA_TYPE_LIST:
        return "list";
    case DATA_TYPE_ANY:
        return "any";
    case DATA_TYPE_BLOCKDEF:
        return "blockdef";
    case DATA_TYPE_UNKNOWN:
        return "unknown";
    }
    assert(false && "Unhandled type_to_str");
}

Block block_new(Blockdef* blockdef) {
    Block block;
    block.blockdef = blockdef;
    block.arguments = vector_create();
    block.parent = NULL;
    blockdef->ref_count++;

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        if (block.blockdef->inputs[i].type != INPUT_ARGUMENT &&
            block.blockdef->inputs[i].type != INPUT_DROPDOWN &&
            block.blockdef->inputs[i].type != INPUT_BLOCKDEF_EDITOR) continue;
        Argument* arg = vector_add_dst((Argument**)&block.arguments);
        arg->input_id = i;

        switch (blockdef->inputs[i].type) {
        case INPUT_ARGUMENT:
            switch (blockdef->inputs[i].data.arg.constr) {
            case BLOCKCONSTR_UNLIMITED:
                arg->type = ARGUMENT_TEXT;
                break;
            case BLOCKCONSTR_STRING:
                arg->type = ARGUMENT_CONST_STRING;
                break;
            default:
                assert(false && "Unimplemented argument constraint");
                break;
            }

            arg->data.text = vector_create();
            for (char* pos = blockdef->inputs[i].data.arg.text; *pos; pos++) {
                vector_add(&arg->data.text, *pos);
            }
            vector_add(&arg->data.text, 0);
            break;
        case INPUT_DROPDOWN:
            arg->type = ARGUMENT_CONST_STRING;

            size_t list_len = 0;
            char** list = blockdef->inputs[i].data.drop.list(&block, &list_len);
            if (!list || list_len == 0) break;

            arg->data.text = vector_create();
            for (char* pos = list[0]; *pos; pos++) {
                vector_add(&arg->data.text, *pos);
            }
            vector_add(&arg->data.text, 0);
            break;
        case INPUT_BLOCKDEF_EDITOR:
            arg->type = ARGUMENT_BLOCKDEF;
            arg->data.blockdef = blockdef_new("custom", BLOCKTYPE_NORMAL, blockdef->color, NULL);
            arg->data.blockdef->ref_count++;
            blockdef_add_text(arg->data.blockdef, gettext("My block"));
            break;
        default:
            assert(false && "Unreachable");
            break;
        }
    }
    return block;
}

Block block_copy(Block* block, Block* parent) {
    if (!block->arguments) return *block;

    Block new;
    new.blockdef = block->blockdef;
    new.parent = parent;
    new.arguments = vector_create();
    new.blockdef->ref_count++;

    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        Argument* arg = vector_add_dst((Argument**)&new.arguments);
        arg->type = block->arguments[i].type;
        arg->input_id = block->arguments[i].input_id;
        switch (block->arguments[i].type) {
        case ARGUMENT_CONST_STRING:
        case ARGUMENT_TEXT:
            arg->data.text = vector_copy(block->arguments[i].data.text);
            break;
        case ARGUMENT_BLOCK:
            arg->data.block = block_copy(&block->arguments[i].data.block, &new);
            break;
        case ARGUMENT_BLOCKDEF:
            arg->data.blockdef = blockdef_copy(block->arguments[i].data.blockdef);
            arg->data.blockdef->ref_count++;
            break;
        default:
            assert(false && "Unimplemented argument copy");
            break;
        }
    }

    for (size_t i = 0; i < vector_size(new.arguments); i++) {
        if (new.arguments[i].type != ARGUMENT_BLOCK) continue;
        block_update_parent_links(&new.arguments[i].data.block);
    }

    return new;
}

void block_free(Block* block) {
    blockdef_free(block->blockdef);
    if (block->arguments) {
        for (size_t i = 0; i < vector_size(block->arguments); i++) {
            switch (block->arguments[i].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                vector_free(block->arguments[i].data.text);
                break;
            case ARGUMENT_BLOCK:
                block_free(&block->arguments[i].data.block);
                break;
            case ARGUMENT_BLOCKDEF:
                blockdef_free(block->arguments[i].data.blockdef);
                break;
            default:
                assert(false && "Unimplemented argument free");
                break;
            }
        }
        vector_free((Argument*)block->arguments);
    }
}

void block_update_all_links(Block* block) {
    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        if (block->arguments[i].type != ARGUMENT_BLOCK) continue;
        block->arguments[i].data.block.parent = block;
        block_update_all_links(&block->arguments[i].data.block);
    }
}

void block_update_parent_links(Block* block) {
    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        if (block->arguments[i].type != ARGUMENT_BLOCK) continue;
        block->arguments[i].data.block.parent = block;
    }
}

BlockChain blockchain_new(void) {
    BlockChain chain;
    chain.x = 0;
    chain.y = 0;
    chain.blocks = vector_create();
    chain.width = 0;
    chain.height = 0;

    return chain;
}

BlockChain blockchain_copy_single(BlockChain* chain, size_t pos) {
    assert(pos < vector_size(chain->blocks) || pos == 0);

    BlockChain new;
    new.x = chain->x;
    new.y = chain->y;
    new.blocks = vector_create();

    BlockdefType block_type = chain->blocks[pos].blockdef->type;
    if (block_type == BLOCKTYPE_END) return new;
    if (block_type != BLOCKTYPE_CONTROL) {
        vector_add(&new.blocks, block_copy(&chain->blocks[pos], NULL));
        blockchain_update_parent_links(&new);
        return new;
    }

    int layer = 0;
    for (size_t i = pos; i < vector_size(chain->blocks) && layer >= 0; i++) {
        block_type = chain->blocks[i].blockdef->type;
        vector_add(&new.blocks, block_copy(&chain->blocks[i], NULL));
        if (block_type == BLOCKTYPE_CONTROL && i != pos) {
            layer++;
        } else if (block_type == BLOCKTYPE_END) {
            layer--;
        }
    }

    blockchain_update_parent_links(&new);
    return new;
}

BlockChain blockchain_copy(BlockChain* chain, size_t pos) {
    assert(pos < vector_size(chain->blocks) || pos == 0);

    BlockChain new;
    new.x = chain->x;
    new.y = chain->y;
    new.blocks = vector_create();

    int pos_layer = 0;
    for (size_t i = 0; i < pos; i++) {
        BlockdefType block_type = chain->blocks[i].blockdef->type;
        if (block_type == BLOCKTYPE_CONTROL) {
            pos_layer++;
        } else if (block_type == BLOCKTYPE_END) {
            pos_layer--;
            if (pos_layer < 0) pos_layer = 0;
        }
    }
    int current_layer = pos_layer;

    vector_reserve(&new.blocks, vector_size(chain->blocks) - pos);
    for (vec_size_t i = pos; i < vector_size(chain->blocks); i++) {
        BlockdefType block_type = chain->blocks[i].blockdef->type;
        if ((block_type == BLOCKTYPE_END || (block_type == BLOCKTYPE_CONTROLEND && i != pos)) &&
            pos_layer == current_layer &&
            current_layer != 0) break;

        vector_add(&new.blocks, block_copy(&chain->blocks[i], NULL));
        block_update_parent_links(&new.blocks[vector_size(new.blocks) - 1]);

        if (block_type == BLOCKTYPE_CONTROL) {
            current_layer++;
        } else if (block_type == BLOCKTYPE_END) {
            current_layer--;
        }
    }

    return new;
}

void blockchain_update_parent_links(BlockChain* chain) {
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        block_update_parent_links(&chain->blocks[i]);
    }
}

void blockchain_add_block(BlockChain* chain, Block block) {
    vector_add(&chain->blocks, block);
    blockchain_update_parent_links(chain);
}

void blockchain_clear_blocks(BlockChain* chain) {
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        block_free(&chain->blocks[i]);
    }
    vector_clear(chain->blocks);
}

void blockchain_insert(BlockChain* dst, BlockChain* src, size_t pos) {
    assert(pos < vector_size(dst->blocks));

    vector_reserve(&dst->blocks, vector_size(dst->blocks) + vector_size(src->blocks));
    for (ssize_t i = (ssize_t)vector_size(src->blocks) - 1; i >= 0; i--) {
        vector_insert(&dst->blocks, pos + 1, src->blocks[i]);
    }
    blockchain_update_parent_links(dst);
    vector_clear(src->blocks);
}

void blockchain_detach_single(BlockChain* dst, BlockChain* src, size_t pos) {
    assert(pos < vector_size(src->blocks));

    BlockdefType block_type = src->blocks[pos].blockdef->type;
    if (block_type == BLOCKTYPE_END) return;
    if (block_type != BLOCKTYPE_CONTROL) {
        vector_add(&dst->blocks, src->blocks[pos]);
        blockchain_update_parent_links(dst);
        vector_remove(src->blocks, pos);
        for (size_t i = pos; i < vector_size(src->blocks); i++) block_update_parent_links(&src->blocks[i]);
        return;
    }

    int size = 0;
    int layer = 0;
    for (size_t i = pos; i < vector_size(src->blocks) && layer >= 0; i++) {
        BlockdefType block_type = src->blocks[i].blockdef->type;
        vector_add(&dst->blocks, src->blocks[i]);
        if (block_type == BLOCKTYPE_CONTROL && i != pos) {
            layer++;
        } else if (block_type == BLOCKTYPE_END) {
            layer--;
        }
        size++;
    }

    blockchain_update_parent_links(dst);
    vector_erase(src->blocks, pos, size);
    for (size_t i = pos; i < vector_size(src->blocks); i++) block_update_parent_links(&src->blocks[i]);
}

void blockchain_detach(BlockChain* dst, BlockChain* src, size_t pos) {
    assert(pos < vector_size(src->blocks));

    int pos_layer = 0;
    for (size_t i = 0; i < pos; i++) {
        BlockdefType block_type = src->blocks[i].blockdef->type;
        if (block_type == BLOCKTYPE_CONTROL) {
            pos_layer++;
        } else if (block_type == BLOCKTYPE_END) {
            pos_layer--;
            if (pos_layer < 0) pos_layer = 0;
        }
    }

    int current_layer = pos_layer;
    int layer_size = 0;

    vector_reserve(&dst->blocks, vector_size(dst->blocks) + vector_size(src->blocks) - pos);
    for (size_t i = pos; i < vector_size(src->blocks); i++) {
        BlockdefType block_type = src->blocks[i].blockdef->type;
        if ((block_type == BLOCKTYPE_END || (block_type == BLOCKTYPE_CONTROLEND && i != pos)) && pos_layer == current_layer && current_layer != 0) break;
        vector_add(&dst->blocks, src->blocks[i]);
        if (block_type == BLOCKTYPE_CONTROL) {
            current_layer++;
        } else if (block_type == BLOCKTYPE_END) {
            current_layer--;
        }
        layer_size++;
    }
    blockchain_update_parent_links(dst);
    vector_erase(src->blocks, pos, layer_size);
    blockchain_update_parent_links(src);
}

void blockchain_free(BlockChain* chain) {
    blockchain_clear_blocks(chain);
    vector_free(chain->blocks);
}

void argument_set_block(Argument* block_arg, Block block) {
    if (block_arg->type == ARGUMENT_TEXT || block_arg->type == ARGUMENT_CONST_STRING) vector_free(block_arg->data.text);
    block_arg->type = ARGUMENT_BLOCK;
    block_arg->data.block = block;

    block_update_parent_links(&block_arg->data.block);
}

void argument_set_const_string(Argument* block_arg, char* text) {
    assert(block_arg->type == ARGUMENT_CONST_STRING);

    block_arg->type = ARGUMENT_CONST_STRING;
    vector_clear(block_arg->data.text);

    for (char* pos = text; *pos; pos++) {
        vector_add(&block_arg->data.text, *pos);
    }
    vector_add(&block_arg->data.text, 0);
}

void argument_set_text(Argument* block_arg, char* text) {
    assert(block_arg->type == ARGUMENT_BLOCK);
    assert(block_arg->data.block.parent != NULL);

    block_arg->type = ARGUMENT_TEXT;
    block_arg->data.text = vector_create();

    for (char* pos = text; *pos; pos++) {
        vector_add(&block_arg->data.text, *pos);
    }
    vector_add(&block_arg->data.text, 0);
}

Blockdef* blockdef_new(const char* id, BlockdefType type, BlockdefColor color, void* func) {
    assert(id != NULL);
    Blockdef* blockdef = malloc(sizeof(Blockdef));
    blockdef->id = strcpy(malloc((strlen(id) + 1) * sizeof(char)), id);
    blockdef->color = color;
    blockdef->type = type;
    blockdef->ref_count = 0;
    blockdef->inputs = vector_create();
    blockdef->func = func;

    return blockdef;
}

Blockdef* blockdef_copy(Blockdef* blockdef) {
    Blockdef* new = malloc(sizeof(Blockdef));
    new->id = strcpy(malloc((strlen(blockdef->id) + 1) * sizeof(char)), blockdef->id);
    new->color = blockdef->color;
    new->type = blockdef->type;
    new->ref_count = 0;
    new->inputs = vector_create();
    new->func = blockdef->func;

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        Input* input = vector_add_dst(&new->inputs);
        input->type = blockdef->inputs[i].type;
        switch (blockdef->inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            input->data = (InputData) {
                .text = vector_copy(blockdef->inputs[i].data.text),
            };
            break;
        case INPUT_ARGUMENT:
            input->data = (InputData) {
                .arg = {
                    .blockdef = blockdef_copy(blockdef->inputs[i].data.arg.blockdef),
                    .text = blockdef->inputs[i].data.arg.text,
                    .constr = blockdef->inputs[i].data.arg.constr,
                },
            };
            break;
        case INPUT_IMAGE_DISPLAY:
            input->data = (InputData) {
                .image = blockdef->inputs[i].data.image,
            };
            break;
        case INPUT_DROPDOWN:
            input->data = (InputData) {
                .drop = {
                    .source = blockdef->inputs[i].data.drop.source,
                    .list = blockdef->inputs[i].data.drop.list,
                },
            };
            break;
        case INPUT_BLOCKDEF_EDITOR:
            input->data = (InputData) {0};
            break;
        default:
            assert(false && "Unimplemented input copy");
            break;
        }
    }

    return new;
}

void blockdef_add_text(Blockdef* blockdef, const char* text) {
    Input* input = vector_add_dst(&blockdef->inputs);
    input->type = INPUT_TEXT_DISPLAY;
    input->data = (InputData) {
        .text = vector_create(),
    };

    for (size_t i = 0; text[i]; i++) vector_add(&input->data.text, text[i]);
    vector_add(&input->data.text, 0);
}

void blockdef_add_argument(Blockdef* blockdef, char* defualt_data, const char* hint_text, InputArgumentConstraint constraint) {
    Input* input = vector_add_dst(&blockdef->inputs);
    input->type = INPUT_ARGUMENT;
    input->data = (InputData) {
        .arg = {
            .blockdef = blockdef_new("custom_arg", BLOCKTYPE_NORMAL, blockdef->color, NULL),
            .text = defualt_data,
            .constr = constraint,
            .hint_text = hint_text,
        },
    };
    input->data.arg.blockdef->ref_count++;
}

void blockdef_add_blockdef_editor(Blockdef* blockdef) {
    Input* input = vector_add_dst(&blockdef->inputs);
    input->type = INPUT_BLOCKDEF_EDITOR;
    input->data = (InputData) {0};
}

void blockdef_add_dropdown(Blockdef* blockdef, InputDropdownSource dropdown_source, ListAccessor accessor) {
    Input* input = vector_add_dst(&blockdef->inputs);
    input->type = INPUT_DROPDOWN;
    input->data = (InputData) {
        .drop = {
            .source = dropdown_source,
            .list = accessor,
        },
    };
}

void blockdef_add_image(Blockdef* blockdef, BlockdefImage image) {
    Input* input = vector_add_dst(&blockdef->inputs);
    input->type = INPUT_IMAGE_DISPLAY;
    input->data = (InputData) {
        .image = image,
    };
}

void blockdef_set_id(Blockdef* blockdef, const char* new_id) {
    free((void*)blockdef->id);
    blockdef->id = strcpy(malloc((strlen(new_id) + 1) * sizeof(char)), new_id);
}

void blockdef_delete_input(Blockdef* blockdef, size_t input) {
    assert(input < vector_size(blockdef->inputs));

    switch (blockdef->inputs[input].type) {
    case INPUT_TEXT_DISPLAY:
        vector_free(blockdef->inputs[input].data.text);
        break;
    case INPUT_ARGUMENT:
        blockdef_free(blockdef->inputs[input].data.arg.blockdef);
        break;
    default:
        assert(false && "Unimplemented input delete");
        break;
    }
    vector_remove(blockdef->inputs, input);
}

void blockdef_free(Blockdef* blockdef) {
    blockdef->ref_count--;
    if (blockdef->ref_count > 0) return;
    for (vec_size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        switch (blockdef->inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            vector_free(blockdef->inputs[i].data.text);
            break;
        case INPUT_ARGUMENT:
            blockdef_free(blockdef->inputs[i].data.arg.blockdef);
            break;
        default:
            break;
        }
    }
    vector_free(blockdef->inputs);
    free((void*)blockdef->id);
    free(blockdef);
}
