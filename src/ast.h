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

#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdbool.h>

typedef struct BlockdefColor BlockdefColor;
typedef struct BlockdefImage BlockdefImage;

typedef enum ArgumentType ArgumentType;
typedef union ArgumentData ArgumentData;
typedef struct Argument Argument;
typedef struct Block Block;

typedef enum InputArgumentConstraint InputArgumentConstraint;
typedef enum InputDropdownSource InputDropdownSource;
typedef struct InputArgument InputArgument;
typedef struct InputDropdown InputDropdown;
typedef enum InputType InputType;
typedef union InputData InputData;
typedef struct Input Input;

typedef enum BlockdefType BlockdefType;
typedef struct Blockdef Blockdef;

typedef struct BlockChain BlockChain;

typedef char** (*ListAccessor)(Block* block, size_t* list_len);

typedef enum {
    DATA_TYPE_UNKNOWN = 0,
    DATA_TYPE_NOTHING,
    DATA_TYPE_INTEGER,
    DATA_TYPE_FLOAT,
    DATA_TYPE_LITERAL, // Literal string, stored in global memory
    DATA_TYPE_STRING, // Pointer to a string type, managed by the current memory allocator (GC)
    DATA_TYPE_BOOL,
    DATA_TYPE_LIST,
    DATA_TYPE_ANY,
    DATA_TYPE_BLOCKDEF,
} DataType;

struct BlockdefColor {
    unsigned char r, g, b, a;
};

struct BlockdefImage {
    void* image_ptr;
    BlockdefColor image_color;
};

enum InputArgumentConstraint {
    BLOCKCONSTR_UNLIMITED, // Can put anything as argument
    BLOCKCONSTR_STRING, // Can only put strings as argument
};

struct InputArgument {
    Blockdef* blockdef;
    InputArgumentConstraint constr;
    char* text;
    const char* hint_text;
};

enum InputDropdownSource {
    DROPDOWN_SOURCE_LISTREF,
};

struct InputDropdown {
    InputDropdownSource source;
    ListAccessor list;
};

union InputData {
    char* text;
    BlockdefImage image;
    InputArgument arg;
    InputDropdown drop;
};

enum InputType {
    INPUT_TEXT_DISPLAY,
    INPUT_ARGUMENT,
    INPUT_DROPDOWN,
    INPUT_BLOCKDEF_EDITOR,
    INPUT_IMAGE_DISPLAY,
};

struct Input {
    InputType type;
    InputData data;
};

enum BlockdefType {
    BLOCKTYPE_NORMAL,
    BLOCKTYPE_CONTROL,
    BLOCKTYPE_CONTROLEND,
    BLOCKTYPE_END,
    BLOCKTYPE_HAT,
};

struct Blockdef {
    const char* id;
    int ref_count;
    BlockdefColor color;
    BlockdefType type;
    Input* inputs;
    void* func;
};

struct Block {
    Blockdef* blockdef;
    struct Argument* arguments;
    int width;
    struct Block* parent;
};

union ArgumentData {
    char* text;
    Block block;
    Blockdef* blockdef;
};

enum ArgumentType {
    ARGUMENT_TEXT = 0,
    ARGUMENT_BLOCK,
    ARGUMENT_CONST_STRING,
    ARGUMENT_BLOCKDEF,
};

struct Argument {
    int input_id;
    ArgumentType type;
    ArgumentData data;
};

struct BlockChain {
    int x, y;
    int width, height;
    Block* blocks;
};

Blockdef* blockdef_new(const char* id, BlockdefType type, BlockdefColor color, void* func);
Blockdef* blockdef_copy(Blockdef* blockdef);
void blockdef_add_text(Blockdef* blockdef, char* text);
void blockdef_add_argument(Blockdef* blockdef, char* defualt_data, const char* hint_text, InputArgumentConstraint constraint);
void blockdef_add_dropdown(Blockdef* blockdef, InputDropdownSource dropdown_source, ListAccessor accessor);
void blockdef_add_image(Blockdef* blockdef, BlockdefImage image);
void blockdef_add_blockdef_editor(Blockdef* blockdef);
void blockdef_delete_input(Blockdef* blockdef, size_t input);
void blockdef_set_id(Blockdef* blockdef, const char* new_id);
void blockdef_free(Blockdef* blockdef);

BlockChain blockchain_new(void);
BlockChain blockchain_copy(BlockChain* chain, size_t ind);
BlockChain blockchain_copy_single(BlockChain* chain, size_t pos);
void blockchain_add_block(BlockChain* chain, Block block);
void blockchain_clear_blocks(BlockChain* chain);
void blockchain_insert(BlockChain* dst, BlockChain* src, size_t pos);
void blockchain_update_parent_links(BlockChain* chain);
// Splits off blockchain src in two at specified pos, placing lower half into blockchain dst
void blockchain_detach(BlockChain* dst, BlockChain* src, size_t pos);
void blockchain_detach_single(BlockChain* dst, BlockChain* src, size_t pos);
void blockchain_free(BlockChain* chain);

Block block_new(Blockdef* blockdef);
Block block_copy(Block* block, Block* parent);
void block_update_parent_links(Block* block);
void block_update_all_links(Block* block);
void block_free(Block* block);

void argument_set_block(Argument* block_arg, Block block);
void argument_set_const_string(Argument* block_arg, char* text);
void argument_set_text(Argument* block_arg, char* text);

const char* type_to_str(DataType type);

#endif // AST_H
