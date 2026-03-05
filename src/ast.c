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
    static_assert(DATA_TYPE_LAST == 13, "Exhaustive data type in type_to_str");
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
    case DATA_TYPE_COLOR:
        return "color";
    case DATA_TYPE_ANY:
        return "any";
    case DATA_TYPE_BLOCKDEF:
        return "blockdef";
    case DATA_TYPE_UNKNOWN:
        return "unknown";
    case DATA_TYPE_CHUNK:
        return "chunk";
    case DATA_TYPE_NULL:
        return "null";
    default:
        assert(false && "Unhandled type_to_str");
    }
}

Block* block_new(Blockdef* blockdef) {
    Block* block = malloc(sizeof(Block));
    block->blockdef = blockdef;
    block->arguments = vector_create();
    block->contents = blockchain_new();
    block->contents->parent = block;
    block->controlend_contents = blockchain_new();
    block->controlend_contents->parent = block;
    block->parent = (BlockParent) {0};
    block->prev = NULL;
    block->next = NULL;
    blockdef->ref_count++;

    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        Argument* arg;

        static_assert(INPUT_LAST == 6, "Exhaustive input type in block_new");
        switch (blockdef->inputs[i].type) {
        case INPUT_ARGUMENT:
            arg = vector_add_dst(&block->arguments);
            arg->block = block;
            arg->input_id = i;

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
            arg = vector_add_dst(&block->arguments);
            arg->block = block;
            arg->input_id = i;
            arg->type = ARGUMENT_CONST_STRING;

            size_t list_len = 0;
            char** list = blockdef->inputs[i].data.drop.list(block, &list_len);
            if (!list || list_len == 0) break;

            arg->data.text = vector_create();
            for (char* pos = list[0]; *pos; pos++) {
                vector_add(&arg->data.text, *pos);
            }
            vector_add(&arg->data.text, 0);
            break;
        case INPUT_BLOCKDEF_EDITOR:
            arg = vector_add_dst(&block->arguments);
            arg->block = block;
            arg->input_id = i;
            arg->type = ARGUMENT_BLOCKDEF;
            arg->data.blockdef = blockdef_new("custom", BLOCKTYPE_NORMAL, blockdef->color, NULL);
            arg->data.blockdef->ref_count++;
            blockdef_add_text(arg->data.blockdef, gettext("My block"));
            break;
        case INPUT_COLOR:
            arg = vector_add_dst(&block->arguments);
            arg->block = block;
            arg->input_id = i;
            arg->type = ARGUMENT_COLOR;
            arg->data.color = blockdef->inputs[i].data.color;
            break;
        case INPUT_TEXT_DISPLAY:
        case INPUT_IMAGE_DISPLAY:
            break;
        default:
            assert(false && "Unhandled add input argument");
            break;
        }
    }
    return block;
}

Block* block_copy(Block* block, BlockParent parent) {
    if (!block) return NULL;
    if (!block->arguments) return NULL;

    Block* new = malloc(sizeof(Block));
    new->blockdef = block->blockdef;
    new->parent = parent;
    new->arguments = vector_create();

    new->contents = blockchain_copy(block->contents, NULL);
    new->contents->parent = new;
    new->controlend_contents = blockchain_copy(block->controlend_contents, NULL);
    new->controlend_contents->parent = new;

    new->blockdef->ref_count++;
    new->prev = NULL;
    new->next = NULL;

    vector_reserve(&new->arguments, vector_size(block->arguments));
    for (size_t i = 0; i < vector_size(block->arguments); i++) {
        Argument* arg = vector_add_dst(&new->arguments);
        arg->block = new;
        arg->type = block->arguments[i].type;
        arg->input_id = block->arguments[i].input_id;
        static_assert(ARGUMENT_LAST == 5, "Exhaustive argument type in block_copy");
        switch (block->arguments[i].type) {
        case ARGUMENT_CONST_STRING:
        case ARGUMENT_TEXT:
            arg->data.text = vector_copy(block->arguments[i].data.text);
            break;
        case ARGUMENT_BLOCK: ;
            BlockParent parent_arg;
            parent_arg.type = BLOCK_PARENT_ARGUMENT;
            parent_arg.as.arg = arg;
            arg->data.block = block_copy(block->arguments[i].data.block, parent_arg);
            break;
        case ARGUMENT_BLOCKDEF:
            arg->data.blockdef = blockdef_copy(block->arguments[i].data.blockdef);
            arg->data.blockdef->ref_count++;
            break;
        case ARGUMENT_COLOR:
            arg->data.color = block->arguments[i].data.color;
            break;
        default:
            assert(false && "Unimplemented argument copy");
            break;
        }
    }

    return new;
}

void block_free(Block* block) {
    blockdef_free(block->blockdef);

    if (block->arguments) {
        for (size_t i = 0; i < vector_size(block->arguments); i++) {
            static_assert(ARGUMENT_LAST == 5, "Exhaustive argument type in block_free");
            switch (block->arguments[i].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                vector_free(block->arguments[i].data.text);
                break;
            case ARGUMENT_BLOCK:
                block_free(block->arguments[i].data.block);
                break;
            case ARGUMENT_BLOCKDEF: ;
                blockdef_abandon(block->arguments[i].data.blockdef);
                blockdef_free(block->arguments[i].data.blockdef);
                break;
            case ARGUMENT_COLOR:
                break;
            default:
                assert(false && "Unimplemented argument free");
                break;
            }
        }
        vector_free(block->arguments);
    }

    blockchain_free(block->contents);
    blockchain_free(block->controlend_contents);
    free(block);
}

BlockChain* blockchain_new(void) {
    BlockChain* chain = malloc(sizeof(BlockChain));
    memset(chain, 0, sizeof(BlockChain));
    return chain;
}

BlockChain* blockchain_copy(BlockChain* chain, Block* pos) {
    assert(chain != NULL);

    BlockChain* new_chain = blockchain_new();
    if (CHAIN_EMPTY(chain)) return new_chain;

    for (Block* iter = pos ? pos : chain->start; iter; iter = iter->next) {
        Block* copy = block_copy(iter, (BlockParent) {0});
        copy->prev = new_chain->end;
        if (new_chain->end) new_chain->end->next = copy;
        new_chain->end = copy;
        if (!new_chain->start) new_chain->start = new_chain->end;
    }

    BlockParent parent;
    parent.type = BLOCK_PARENT_BLOCKCHAIN;
    parent.as.chain = new_chain;

    new_chain->start->parent = parent;
    new_chain->end->parent = parent;

    return new_chain;
}

void blockchain_insert(BlockChain* dst, BlockChain* src, Block* pos) {
    assert(dst != NULL);
    assert(src != NULL);

    if (CHAIN_EMPTY(src)) {
        free(src);
        return;
    }

    Block* next_pos = pos ? pos->next : dst->start;

    if (pos) pos->next = src->start;
    src->start->prev = pos;

    if (next_pos) next_pos->prev = src->end;
    src->end->next = next_pos;

    if (!pos) {
        dst->start = src->start;
        dst->start->parent.type = BLOCK_PARENT_BLOCKCHAIN;
        dst->start->parent.as.chain = dst;
    }

    if (!next_pos) {
        dst->end = src->end;
        dst->end->parent.type = BLOCK_PARENT_BLOCKCHAIN;
        dst->end->parent.as.chain = dst;
    }

    free(src);
}

BlockChain* blockchain_detach(BlockChain* chain, Block* start, Block* end) {
    assert(chain != NULL);
    assert(!start == !end && "Invalid chain, either start or end cannot be NULL");

    BlockChain* new_chain = blockchain_new();
    new_chain->start = start;
    new_chain->end = end;

    if (!new_chain->start && !new_chain->end) return new_chain;

    BlockParent parent;
    parent.type = BLOCK_PARENT_BLOCKCHAIN;
    parent.as.chain = new_chain;

    new_chain->start->parent = parent;
    new_chain->end->parent = parent;

    if (chain->start == start && chain->end == end) {
        chain->start = NULL;
        chain->end = NULL;
        return new_chain;
    }

    if (start->prev) {
        start->prev->next = end->next;
    } else {
        chain->start = end->next;
        chain->start->parent.type = BLOCK_PARENT_BLOCKCHAIN;
        chain->start->parent.as.chain = chain;
    }

    if (end->next) {
        end->next->prev = start->prev;
    } else {
        chain->end = start->prev;
        chain->end->parent.type = BLOCK_PARENT_BLOCKCHAIN;
        chain->end->parent.as.chain = chain;
    }

    start->prev = NULL;
    end->next = NULL;

    return new_chain;
}

void blockchain_free(BlockChain* chain) {
    assert(chain != NULL);

    Block* iter = chain->start;
    while (iter) {
        Block* next = iter->next;
        block_free(iter);
        iter = next;
    }
    chain->start = NULL;
    chain->end = NULL;
    free(chain);
}

void argument_set_block(Argument* arg, Block* block) {
    if (arg->type == ARGUMENT_TEXT || arg->type == ARGUMENT_CONST_STRING) vector_free(arg->data.text);
    arg->type = ARGUMENT_BLOCK;
    arg->data.block = block;
}

void argument_set_const_string(Argument* arg, char* text) {
    assert(arg->type == ARGUMENT_CONST_STRING);

    arg->type = ARGUMENT_CONST_STRING;
    vector_clear(arg->data.text);

    for (char* pos = text; *pos; pos++) vector_add(&arg->data.text, *pos);
    vector_add(&arg->data.text, 0);
}

void argument_set_text(Argument* arg, char* text) {
    arg->type = ARGUMENT_TEXT;
    arg->data.text = vector_create();

    for (char* pos = text; *pos; pos++) vector_add(&arg->data.text, *pos);
    vector_add(&arg->data.text, 0);
}

void argument_set_color(Argument* arg, BlockdefColor color) {
    arg->type = ARGUMENT_COLOR;
    arg->data.color = color;
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

        static_assert(INPUT_LAST == 6, "Exhaustive input type in blockdef_copy");
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
                    .hint_text = blockdef->inputs[i].data.arg.hint_text,
                },
            };
            input->data.arg.blockdef->ref_count++;
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
        case INPUT_COLOR:
            input->data = (InputData) {
                .color = blockdef->inputs[i].data.color,
            };
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

void blockdef_add_color_input(Blockdef* blockdef, BlockdefColor color) {
    Input* input = vector_add_dst(&blockdef->inputs);
    input->type = INPUT_COLOR;
    input->data = (InputData) {
        .color = color,
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
    
    static_assert(INPUT_LAST == 6, "Exhaustive input type in blockdef_delete_input");
    switch (blockdef->inputs[input].type) {
    case INPUT_TEXT_DISPLAY:
        vector_free(blockdef->inputs[input].data.text);
        break;
    case INPUT_ARGUMENT:
        blockdef_free(blockdef->inputs[input].data.arg.blockdef);
        break;
    case INPUT_DROPDOWN:
    case INPUT_BLOCKDEF_EDITOR:
    case INPUT_IMAGE_DISPLAY:
    case INPUT_COLOR:
        assert(false && "TODO");
        break;
    default:
        assert(false && "Unimplemented input delete");
        break;
    }
    vector_remove(blockdef->inputs, input);
}

void blockdef_abandon(Blockdef* blockdef) {
    blockdef->func = NULL;
    for (size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        Input* input = &blockdef->inputs[i];
        if (input->type != INPUT_ARGUMENT) continue;
        input->data.arg.blockdef->func = NULL;
    }
}

void blockdef_free(Blockdef* blockdef) {
    blockdef->ref_count--;
    if (blockdef->ref_count > 0) return;
    for (vec_size_t i = 0; i < vector_size(blockdef->inputs); i++) {
        static_assert(INPUT_LAST == 6, "Exhaustive input type in blockdef_free");
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
