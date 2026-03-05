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

#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdbool.h>

#define CHAIN_EMPTY(_chain) (!(_chain)->start && !(_chain)->end)

typedef struct BlockdefColor BlockdefColor;
typedef struct BlockdefImage BlockdefImage;

typedef enum ArgumentType ArgumentType;
typedef union ArgumentData ArgumentData;
typedef struct Argument Argument;
typedef enum BlockParentType BlockParentType;
typedef struct BlockParent BlockParent;
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
typedef struct RootBlockChain RootBlockChain;

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
    DATA_TYPE_COLOR,
    DATA_TYPE_CHUNK, // A chunk of compiled code. Can get merged with other chunks that will result in final compiler output
    DATA_TYPE_NULL, // No type, used in chunks to indicate that chunk did not modify the stack
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
    BlockdefColor color;
};

enum InputType {
    INPUT_TEXT_DISPLAY,
    INPUT_ARGUMENT,
    INPUT_DROPDOWN,
    INPUT_BLOCKDEF_EDITOR,
    INPUT_IMAGE_DISPLAY,
    INPUT_COLOR,
    // Must be last in enum
    INPUT_LAST,
};

struct Input {
    InputType type;
    InputData data;
};

enum BlockdefType {
    BLOCKTYPE_NORMAL,
    BLOCKTYPE_CONTROL,
    BLOCKTYPE_CONTROLEND,
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

enum BlockParentType {
    BLOCK_PARENT_NONE = 0, // No parent
    BLOCK_PARENT_ARGUMENT,
    BLOCK_PARENT_BLOCKCHAIN,
    BLOCK_PARENT_BLOCK,
};

struct BlockParent {
    BlockParentType type;
    union {
        Argument* arg;
        BlockChain* chain;
        Block* block;
    } as;
};

struct Block {
    Blockdef* blockdef;
    Argument* arguments;
    BlockChain* contents;
    BlockChain* controlend_contents;
    BlockParent parent;
    Block* prev;
    Block* next;
};

union ArgumentData {
    char* text;
    BlockdefColor color;
    Block* block;
    Blockdef* blockdef;
};

enum ArgumentType {
    ARGUMENT_TEXT = 0,
    ARGUMENT_BLOCK,
    ARGUMENT_CONST_STRING,
    ARGUMENT_BLOCKDEF,
    ARGUMENT_COLOR,
    // Must be last in enum
    ARGUMENT_LAST,
};

struct Argument {
    Block* block;
    int input_id;
    ArgumentType type;
    ArgumentData data;
};

struct BlockChain {
    Block* parent;
    Block* start;
    Block* end;
};

struct RootBlockChain {
    int x, y;
    int width, height;
    BlockChain* chain;
};

Blockdef* blockdef_new(const char* id, BlockdefType type, BlockdefColor color, void* func);
Blockdef* blockdef_copy(Blockdef* blockdef);
void blockdef_add_text(Blockdef* blockdef, const char* text);
void blockdef_add_argument(Blockdef* blockdef, char* defualt_data, const char* hint_text, InputArgumentConstraint constraint);
void blockdef_add_dropdown(Blockdef* blockdef, InputDropdownSource dropdown_source, ListAccessor accessor);
void blockdef_add_color_input(Blockdef* blockdef, BlockdefColor color);
void blockdef_add_image(Blockdef* blockdef, BlockdefImage image);
void blockdef_add_blockdef_editor(Blockdef* blockdef);
void blockdef_delete_input(Blockdef* blockdef, size_t input);
void blockdef_set_id(Blockdef* blockdef, const char* new_id);
// Removes all function pointers from blockdef, rendering it unusable. 
// User is expected to manually fix these blockdefs by putting appropriate newly created blockdef
void blockdef_abandon(Blockdef* blockdef);
void blockdef_free(Blockdef* blockdef);

BlockChain* blockchain_new(void);
BlockChain* blockchain_copy(BlockChain* chain, Block* pos);
void blockchain_insert(BlockChain* dst, BlockChain* src, Block* pos);
BlockChain* blockchain_detach(BlockChain* chain, Block* start, Block* end);
void blockchain_free(BlockChain* chain);

Block* block_new(Blockdef* blockdef);
Block* block_copy(Block* block, BlockParent parent);
void block_free(Block* block);

void argument_set_block(Argument* block_arg, Block* block);
void argument_set_const_string(Argument* block_arg, char* text);
void argument_set_text(Argument* block_arg, char* text);
void argument_set_color(Argument* block_arg, BlockdefColor color);

const char* type_to_str(DataType type);

#endif // AST_H
