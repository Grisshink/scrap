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

#ifndef SCRAP_IR_H
#define SCRAP_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#define IR_LAST_ERROR_SIZE 512

#ifdef DEBUG
#define IR_ASSERT(val) assert(val)
#else
#define IR_ASSERT(val)
#endif

typedef struct IrExec IrExec;
typedef struct IrValue IrValue;
typedef struct IrConstValue IrConstValue;

typedef size_t ConstId;
typedef int IrLabelID;
typedef size_t IrInstructionID;
typedef bool (*IrRunFunction)(IrExec* exec);
typedef IrRunFunction (*IrRunFunctionResolver)(IrExec* exec, const char* hint);

typedef enum {
    IR_ILLEGAL = 0, // Illegal instruction
    IR_PUSHN = 1,   // Push nothing
    IR_PUSHI,       // Push int
    IR_PUSHF,       // Push float
    IR_PUSHB,       // Push bool
    IR_PUSHL,       // Push list
    IR_PUSHA,       // Push string
    IR_PUSHLB,      // Push label
    IR_PUSHFN,      // Push func
    IR_POP,         // Pop value
    IR_POPC,        // Pop multiple values
    IR_DUP,         // Duplicate last value
    IR_LOAD,        // Load variable into stack
    IR_STORE,       // Store variable from stack
    IR_GLOAD,       // Load global variable into stack
    IR_GSTORE,      // Store global variable from stack

    // Integer arithmetic
    IR_ADDI,
    IR_SUBI,
    IR_MULI,
    IR_DIVI,
    IR_MODI,
    IR_POWI,

    // Bitwise math
    IR_NOTI,
    IR_ANDI,
    IR_ORI,
    IR_XORI,

    // Float arithmetic
    IR_ADDF,
    IR_SUBF,
    IR_MULF,
    IR_DIVF,
    IR_MODF,
    IR_POWF,

    // Boolean algebra
    IR_NOT,
    IR_AND,
    IR_OR,
    IR_XOR,

    // Comparisons
    IR_LESSI,
    IR_MOREI,

    IR_LESSF,
    IR_MOREF,

    IR_LESSEQI,
    IR_MOREEQI,

    IR_LESSEQF,
    IR_MOREEQF,

    IR_EQ,
    IR_NEQ,

    // Type conversion
    IR_ITOF, // int to float
    IR_ITOB, // int to bool
    IR_ITOA, // int to string in ascii format

    IR_FTOI, // float to int
    IR_FTOB, // float to bool
    IR_FTOA, // float to string in ascii format

    IR_BTOI, // bool to int
    IR_BTOF, // bool to float
    IR_BTOA, // bool to string in ascii format

    IR_ATOI, // string to int
    IR_ATOF, // string to float
    IR_ATOB, // string to bool

    IR_LTOA, // list to string in ascii format
    IR_NTOA, // nothing to string in ascii format

    IR_TOI, // any to int
    IR_TOF, // any to float
    IR_TOB, // any to bool
    IR_TOA, // any to string
    IR_TOL, // any to list. Does nothing and it's only used for checking for valid type

    IR_TYPEOF, // Get type of value as string

    // List manipulation
    IR_ADDL,    // Add value to list
    IR_INDEXL,  // Get value from list
    IR_SETL,    // Replace value in list
    IR_INSERTL, // Insert value into list
    IR_DELL,    // Delete value from list
    IR_LENL,    // Get list length

    // Branching
    IR_JMP,  // Jump to label
    IR_IF,   // Jump to label if input bool is true
    IR_IFNOT, // Jump to label if input bool is false
    IR_CALL, // Call label as function, return with IR_RET opcode
    IR_RUN,  // Run native function
    IR_DYNJMP, // Same as IR_JMP, but take label from stack
    IR_DYNIF, // Same as IR_IF, but take label from stack
    IR_DYNCALL, // Same as IR_CALL, but take label from stack
    IR_DYNRUN, // Same as IR_RUN, but take func from stack
    IR_RET,  // Return from function

    IR_LAST,
} IrOpcode;

typedef struct {
    IrRunFunction ptr;
    const char* hint;
} IrFunction;

typedef struct {
    size_t pos;
    const char* name;
} IrLabel;

typedef struct {
    IrValue* items;
    size_t size, capacity;
    bool owned;
} IrList;

typedef enum {
    IR_TYPE_NOTHING, // Nothing, similar to NULL in other languages
    IR_TYPE_BYTE,    // 8-bit integer, useful for representing binary data or UTF-8 sequences
    IR_TYPE_INT,     // 64-bit integer
    IR_TYPE_FLOAT,   // 64-bit float
    IR_TYPE_BOOL,    // Boolean, only can contain true or false
    IR_TYPE_LIST,    // Dynamic list, can contain values with different types
    IR_TYPE_STRING,  // String type. Functionally identical to lists, except conversion instructions treat list as a string type
    IR_TYPE_FUNC,    // Pointer to native function
    IR_TYPE_LABEL,   // Pointer to label within bytecode
} IrValueType;

struct IrConstValue {
    IrValueType type;
    union {
        uint8_t byte_val;
        int64_t int_val;
        double float_val;
        bool bool_val;
        IrFunction func_val;
        IrLabel label_val;
        IrList* list_val;
    } as;
};

struct IrValue {
    IrValueType type;
    union {
        uint8_t byte_val;
        int64_t int_val;
        double float_val;
        bool bool_val;
        IrRunFunction func_val;
        size_t label_val;
        IrList* list_val;
    } as;
};

typedef struct {
    IrConstValue* items;
    size_t size, capacity;
} IrConstValueList;

typedef struct {
    IrValue* items;
    size_t size, capacity;
} IrValueList;

typedef struct {
    IrValueList* items;
    size_t size, capacity;
} IrVariableFrameList;

typedef struct {
    ConstId* items;
    size_t size, capacity;
} IrLabelList;

typedef struct {
    unsigned char* items;
    size_t size, capacity;
} IrOpcodes;

typedef struct {
    size_t reserve_size,
           commit_size,
           pos,
           commit_pos;
} IrMemArena;

typedef struct {
    struct {
        ConstId* items;
        size_t size, capacity;
    } hash_set;
    IrConstValueList list; // List of all constants that bytecode chunks can reference
    IrMemArena* arena; // Arena for all bytecode allocations
} IrBytecodePool;

typedef struct {
    const char* name;
    unsigned int version;
    IrOpcodes code;
    IrBytecodePool* pool;
    IrLabelList labels;
} IrBytecode;

typedef struct {
    IrBytecode* items;
    size_t size, capacity;
} IrBytecodeChunks;

typedef struct {
    void* copy_ptr;
    size_t size;
    unsigned char data[];
} IrHeapChunk;

typedef struct {
    IrMemArena* mem;
    size_t chunks_count, mem_max;
} IrHeap;

struct IrExec {
    IrBytecodeChunks chunks;
    IrValueList stack;
    IrValueList globals;
    IrVariableFrameList variables;
    char last_error[IR_LAST_ERROR_SIZE];
    IrRunFunctionResolver resolve_run_function;

    IrHeap heap;
    IrHeap second_heap;
};

// Allocate new bytecode pool.
// This will hold all constants used by bytecode chunks as well as the arena for all bytecode allocations.
// Note that bytecode_pool_free will also free the passed arena
// Pass NULL to arena to let bytecode pool create it automatically
IrBytecodePool* bytecode_pool_new(IrMemArena* arena);

// Frees the pool with all bytecode chunks and constants.
void bytecode_pool_free(IrBytecodePool* pool);

// Get constant index in list of constants in bytecode pool.
// Returns (size_t)-1 if the constant does not exist.
size_t bytecode_pool_get(IrBytecodePool* pool, IrConstValue value);

// Add constant to constant list in bytecode pool and return its index in the list.
// If constant already exists, then it will return the index of already existing constant and free the value.
size_t bytecode_pool_insert(IrBytecodePool* pool, IrConstValue value);

// Create new bytecode chunk with associated bytecode pool.
// All data allocated using the bytecode will be freed upon freeing the associated bytecode pool.
// If the name argument is NULL, then the bytecode name will default to *Unnamed*.
IrBytecode bytecode_new(const char* name, IrBytecodePool* pool);

// Adds all code from src bytecode to the end of dst bytecode.
// The src bytecode should not be used after calling this function.
void bytecode_join(IrBytecode* dst, IrBytecode* src);

// Print the bytecode contents to stdout.
void bytecode_print(IrBytecode* bc);

// Appends named label to the end of bytecode.
// The returned ConstId can be used to reference the label in other bytecode functions.
ConstId bytecode_push_label(IrBytecode* bc, const char* name);

// Appends the instruction to the end of bytecode.
// The returned IrInstructionID can be used to reference the instruction in other bytecode functions.
IrInstructionID bytecode_push_op(IrBytecode* bc, IrOpcode op);

// These functions append the instruction with constant to the end of bytecode.
IrInstructionID bytecode_push_op_int(IrBytecode* bc, IrOpcode op, int64_t int_val);
IrInstructionID bytecode_push_op_float(IrBytecode* bc, IrOpcode op, double float_val);
IrInstructionID bytecode_push_op_bool(IrBytecode* bc, IrOpcode op, bool bool_val);
IrInstructionID bytecode_push_op_func(IrBytecode* bc, IrOpcode op, IrFunction func_val);
IrInstructionID bytecode_push_op_label(IrBytecode* bc, IrOpcode op, ConstId label_id);
IrInstructionID bytecode_push_op_list(IrBytecode* bc, IrOpcode op, IrList* list_val);
IrInstructionID bytecode_push_op_list_string(IrBytecode* bc, IrOpcode op, IrList* list_val);

// These functions replace the constant in bytecode at instruction instr_id.
void bytecode_set_op_int(IrBytecode* bc, IrInstructionID instr_id, int int_val);
void bytecode_set_op_float(IrBytecode* bc, IrInstructionID instr_id, double float_val);
void bytecode_set_op_bool(IrBytecode* bc, IrInstructionID instr_id, bool bool_val);
void bytecode_set_op_func(IrBytecode* bc, IrInstructionID instr_id, IrFunction func_val);
void bytecode_set_op_list(IrBytecode* bc, IrInstructionID instr_id, IrList* list_val);

// Allocate new immutable list type that can be used in bytecode functions that operate on lists.
IrList* bytecode_const_list_new(IrBytecodePool* pool);

// Add value to immutable list.
void bytecode_const_list_append(IrBytecodePool* pool, IrList* list, IrValue val);

// Save bytecode into file.
void bytecode_save(IrBytecode* bc, const char* filepath);

// Load bytecode from file.
bool bytecode_load(IrBytecodePool* pool, IrBytecode* bc, const char* filepath);

// Create a function value that needs to be resolved at runtime using hint string.
// The exact hint string that needs to be passed depends on current runtime function resolver,
// which can be defined in exec using exec_set_run_function_resolver function.
IrFunction ir_func_by_hint(const char* hint);

// Create a function value with resolved function pointer.
IrFunction ir_func_by_ptr(IrRunFunction func);

// Allocate new execution engine that will execute the bytecode chunks.
// See exec_add_bytecode and exec_run for getting your code to run.
IrExec exec_new(size_t memory_min, size_t memory_max);

// Free the execution engine.
void exec_free(IrExec* exec);

// Sets the run function resolver for this execution engine.
// By default the function resolver is set to NULL upon exec creation which means
// that all IR_RUN instructions will fail with runtime error when executed.
void exec_set_run_function_resolver(IrExec* exec, IrRunFunctionResolver resolver);

// Add bytecode chunk into exec for running the bytecode using exec_run function.
void exec_add_bytecode(IrExec* exec, IrBytecode bc);

// Run the named bytecode chunk from specified label_name.
// This functions returns true upon executing IR_RET instruction at top level,
// and false if the running bytecode caused a runtime error which you can get by accessing exec.last_error property.
bool exec_run(IrExec* exec, const char* bc_name, const char* label_name);

// Raw version of exec_run function.
// Accepts the pos index in bytecode at which to start executing the code.
bool exec_run_bytecode(IrExec* exec, IrBytecode* bc, size_t pos);

// Prints the contents of exec stack for debugging purposes.
void exec_print_stack(IrExec* exec);

// Prints the contents of exec variable frames for debugging purposes.
void exec_print_variables(IrExec* exec);

// Triggers the garbage collection event in exec for debugging purposes.
void exec_collect(IrExec* exec);

// Sets last error value in exec. Used when raising errors from run functions.
void exec_set_error(IrExec* exec, const char* fmt, ...);

// Allocate memory using exec's garbage collector.
// Note that any ir value that is neither in the exec stack nor in the exec variable stack
// can get garbage collected after calling these functions.
void* exec_malloc(IrExec* exec, size_t size);
void* exec_realloc(IrExec* exec, void* ptr, size_t new_size);

// Create new dynamic list. Note: This calls exec_malloc and can change pointers to gc allocated values
IrList* exec_list_new(IrExec* exec);

// Pushes IrValue to exec stack.
void exec_push_value(IrExec* exec, IrValue value);

// Pops IrValue from exec stack and returns it. Produces undefined behavior when stack underflow occurs.
IrValue exec_pop_value(IrExec* exec);

// Same as exec_pop_value, but with multiple values.
void exec_pop_multiple(IrExec* exec, size_t count);

// Similar to exec_pop_value, except it does not remove value from stack.
IrValue exec_get_value(IrExec* exec);

// Duplicates the last value in stack.
void exec_dup_value(IrExec* exec);

// Convenience functions for pushing values to exec stack of specific type.
void exec_push_int(IrExec* exec, int64_t int_val);
void exec_push_float(IrExec* exec, double float_val);
void exec_push_bool(IrExec* exec, bool bool_val);
void exec_push_func(IrExec* exec, IrRunFunction func_val);
void exec_push_label(IrExec* exec, size_t label_val);
void exec_push_list(IrExec* exec, IrList* list_val);
void exec_push_list_string(IrExec* exec, IrList* list_val);
void exec_push_nothing(IrExec* exec);

// Convenience functions for getting values from exec stack of specific type.
int64_t exec_get_int(IrExec* exec);
double exec_get_float(IrExec* exec);
bool exec_get_bool(IrExec* exec);
IrRunFunction exec_get_func(IrExec* exec);
size_t exec_get_label(IrExec* exec);
IrList* exec_get_list(IrExec* exec);
IrList* exec_get_list_string(IrExec* exec);

// Convenience functions for popping values from exec stack of specific type.
int64_t exec_pop_int(IrExec* exec);
double exec_pop_float(IrExec* exec);
bool exec_pop_bool(IrExec* exec);
IrRunFunction exec_pop_func(IrExec* exec);
size_t exec_pop_label(IrExec* exec);
IrList* exec_pop_list(IrExec* exec);
IrList* exec_pop_list_string(IrExec* exec);

// Converts ascii string to list and pushes to stack
bool exec_push_string(IrExec* exec, const char* str);

// Gets ascii buffer from string value
void exec_get_string(IrList* string, char* buf, size_t buf_len);

// Pops ascii string from stack
void exec_pop_string(IrExec* exec, char* buf, size_t buf_len);

// Arena management functions
#define ir_arena_append(arena, list, val) do { \
    if ((list).size >= (list).capacity) { \
        size_t _old_cap = (list).capacity * sizeof(*(list).items); \
        if ((list).capacity == 0) (list).capacity = 32; \
        else (list).capacity *= 2; \
        (list).items = ir_arena_realloc(arena, (list).items, _old_cap, (list).capacity * sizeof(*(list).items)); \
    } \
    (list).items[(list).size++] = (val); \
} while (0)

IrMemArena* ir_arena_new(size_t reserve_size, size_t commit_size);
void ir_arena_free(IrMemArena* arena);
void* ir_arena_alloc(IrMemArena* arena, size_t size);
void* ir_arena_alloc_packed(IrMemArena* arena, size_t size);
void* ir_arena_realloc(IrMemArena* arena, void* ptr, size_t old_size, size_t new_size);
void ir_arena_pop(IrMemArena* arena, size_t size);
void ir_arena_pop_to(IrMemArena* arena, size_t pos);
void ir_arena_clear(IrMemArena* arena);
char* ir_arena_sprintf(IrMemArena* arena, size_t max_size, const char* fmt, ...);

#endif // SCRAP_IR_H

#ifdef SCRAP_IR_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>

#define IR_SAVE_MIN_VERSION 1
#define IR_SAVE_MAX_VERSION 1
#define IR_SAVE_IDENT "SCRAP_IR"

#define KiB(n) ((size_t)(n) << 10)
#define MiB(n) ((size_t)(n) << 20)
#define GiB(n) ((size_t)(n) << 30)

#define ir_list_append(list, val) do { \
    if ((list).size >= (list).capacity) { \
        if ((list).capacity == 0) (list).capacity = 32; \
        else (list).capacity *= 2; \
        (list).items = realloc((list).items, (list).capacity * sizeof(*(list).items)); \
    } \
    (list).items[(list).size++] = (val); \
} while (0)

#define ir_list_free(list) do { \
    if ((list).items) { \
        free((list).items); \
        (list).items = NULL; \
        (list).size = 0; \
        (list).capacity = 0; \
    } \
} while (0)

#define CHECK_IMMEDIATE \
    if (i + 2 >= bc->code.size) { \
        printf("    inval\n"); \
        return; \
    }

#define CODE_IMMEDIATE ((bc->code.items[i + 1] << 16) | (bc->code.items[i + 2] << 8) | bc->code.items[i + 3])

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define IR_ALIGN_UP_POW2(n, p) (((size_t)(n) + ((size_t)(p) - 1)) & (~((size_t)(p) - 1)))

#define IR_ARENA_BASE_POS (sizeof(IrMemArena))
#define IR_ARENA_ALIGN (sizeof(void*))

typedef struct {
    void* ptr;
    size_t pos;
    size_t size;
} IrSave;

unsigned long ir_plat_get_pagesize(void);
void* ir_plat_mem_reserve(size_t size);
bool ir_plat_mem_commit(void* ptr, size_t size);
bool ir_plat_mem_decommit(void* ptr, size_t size);
bool ir_plat_mem_release(void* ptr, size_t size);

size_t hash_value(IrConstValue value) {
    size_t hash = 0;

    switch (value.type) {
    case IR_TYPE_NOTHING: hash = 0xdeadbeef; break;
    case IR_TYPE_BYTE: hash = value.as.byte_val; break;
    case IR_TYPE_INT: hash = value.as.int_val; break;
    case IR_TYPE_FLOAT: hash = *(size_t*)&value.as.float_val; break;
    case IR_TYPE_BOOL: hash = value.as.bool_val; break;
    case IR_TYPE_LIST: ;
        IrList* list = value.as.list_val;
        if (!list) {
            hash = 0x12345678;
            break;
        }
        IrConstValue const_val;
        for (size_t i = 0; i < list->size; i++) {
            IrValue val = list->items[i];
            memset(&const_val.as, 0, sizeof(const_val.as));
            memcpy(&const_val, &val, sizeof(val));
            hash = (hash << 1) ^ hash_value(const_val);
        }
        break;
    case IR_TYPE_FUNC:
        if (value.as.func_val.hint) {
            size_t hint_size = strlen(value.as.func_val.hint);
            for (size_t i = 0; i < hint_size; i++) {
                hash = (hash << 1) ^ value.as.func_val.hint[i];
            }
        }
        hash ^= (size_t)value.as.func_val.ptr;
        break;
    case IR_TYPE_LABEL: ;
        size_t label_size = strlen(value.as.label_val.name);
        for (size_t i = 0; i < label_size; i++) {
            hash = (hash << 1) ^ value.as.label_val.name[i];
        }
        break;
    default:
        break;
    }

    return hash;
}

bool value_equals(IrValue left, IrValue right) {
    if (left.type != right.type) return false;

    switch (left.type) {
    case IR_TYPE_NOTHING: return true;
    case IR_TYPE_BYTE: return left.as.byte_val == right.as.byte_val;
    case IR_TYPE_INT: return left.as.int_val == right.as.int_val;
    case IR_TYPE_FLOAT: return left.as.float_val == right.as.float_val;
    case IR_TYPE_BOOL: return left.as.bool_val == right.as.bool_val;
    case IR_TYPE_STRING:
    case IR_TYPE_LIST: ;
        IrList* left_list = left.as.list_val;
        IrList* right_list = right.as.list_val;

        if (!left_list && !right_list) return true;
        if (!left_list || !right_list) return false;
        if (left_list->size != right_list->size) return false;
        for (size_t i = 0; i < left_list->size; i++) {
            if (!value_equals(left_list->items[i], right_list->items[i])) return false;
        }
        return true;
    case IR_TYPE_FUNC: return left.as.func_val == right.as.func_val;
    case IR_TYPE_LABEL: return left.as.label_val == right.as.label_val;
    }
    return true;
}

bool const_value_equals(IrConstValue left, IrConstValue right) {
    if (left.type != right.type) return false;

    switch (left.type) {
    case IR_TYPE_NOTHING: return true;
    case IR_TYPE_BYTE: return left.as.byte_val == right.as.byte_val;
    case IR_TYPE_INT: return left.as.int_val == right.as.int_val;
    case IR_TYPE_FLOAT: return left.as.float_val == right.as.float_val;
    case IR_TYPE_BOOL: return left.as.bool_val == right.as.bool_val;
    case IR_TYPE_STRING:
    case IR_TYPE_LIST: ;
        IrList* left_list = left.as.list_val;
        IrList* right_list = right.as.list_val;

        if (!left_list && !right_list) return true;
        if (!left_list || !right_list) return false;
        if (left_list->size != right_list->size) return false;
        for (size_t i = 0; i < left_list->size; i++) {
            if (!value_equals(left_list->items[i], right_list->items[i])) return false;
        }
        return true;
    case IR_TYPE_FUNC:
        if (left.as.func_val.ptr == right.as.func_val.ptr && left.as.func_val.ptr && right.as.func_val.ptr) return true;
        if ((left.as.func_val.hint != NULL) != (right.as.func_val.hint != NULL)) return false;
        if (left.as.func_val.hint) {
            return !strcmp(left.as.func_val.hint, right.as.func_val.hint);
        }
        return true;
    case IR_TYPE_LABEL:
        return !strcmp(left.as.label_val.name, right.as.label_val.name);
    }
    return true;
}

IrBytecodePool* bytecode_pool_new(IrMemArena* arena) {
    if (!arena) arena = ir_arena_new(GiB(2), MiB(1));

    IrBytecodePool* pool = ir_arena_alloc(arena, sizeof(IrBytecodePool));
    memset(pool, 0, sizeof(IrBytecodePool));
    pool->arena = arena;
    return pool;
}

void bytecode_pool_free(IrBytecodePool* pool) {
    ir_list_free(pool->hash_set);
    ir_list_free(pool->list);
    ir_arena_free(pool->arena);
}

size_t bytecode_pool_get(IrBytecodePool* pool, IrConstValue value) {
    if (pool->hash_set.capacity == 0) return (size_t)-1;

    size_t hash = hash_value(value) % pool->hash_set.capacity;
    size_t idx = pool->hash_set.items[hash];
    if (idx == (size_t)-1) return idx;
    while (!const_value_equals(pool->list.items[idx], value)) {
        hash++;
        if (hash >= pool->hash_set.capacity) hash = 0;
        idx = pool->hash_set.items[hash];
        if (idx == (size_t)-1) return idx;
    }
    return idx;
}

size_t bytecode_pool_insert(IrBytecodePool* pool, IrConstValue value) {
    if ((float)pool->hash_set.size / (float)pool->hash_set.capacity > 0.6 || pool->hash_set.capacity == 0) {
        if (pool->hash_set.capacity == 0) pool->hash_set.capacity = 1024;
        else pool->hash_set.capacity *= 2;

        pool->hash_set.items = realloc(pool->hash_set.items, sizeof(*pool->hash_set.items) * pool->hash_set.capacity);
        // This sets all buckets in hash set to -1 (empty)
        memset(pool->hash_set.items, 0xff, sizeof(*pool->hash_set.items) * pool->hash_set.capacity);

        for (size_t i = 0; i < pool->list.size; i++) {
            size_t hash = hash_value(pool->list.items[i]) % pool->hash_set.capacity;
            size_t idx = pool->hash_set.items[hash];
            while (idx != (size_t)-1) {
                hash++;
                if (hash >= pool->hash_set.capacity) hash = 0;
                idx = pool->hash_set.items[hash];
            }
            pool->hash_set.items[hash] = i;
        }
    }

    size_t hash = hash_value(value) % pool->hash_set.capacity;
    size_t idx = pool->hash_set.items[hash];

    while (idx != (size_t)-1) {
        if (const_value_equals(pool->list.items[idx], value)) return idx;

        hash++;
        if (hash >= pool->hash_set.capacity) hash = 0;
        idx = pool->hash_set.items[hash];
    }

    // Insert value
    idx = pool->list.size;
    pool->hash_set.items[hash] = idx;
    pool->hash_set.size++;
    ir_list_append(pool->list, value);
    return idx;
}

IrBytecode bytecode_new(const char* name, IrBytecodePool* pool) {
    return (IrBytecode) {
        .name = name,
        .version = IR_SAVE_MAX_VERSION,
        .code = (IrOpcodes) {0},
        .pool = pool,
        .labels = (IrLabelList) {0},
    };
}

void bytecode_join(IrBytecode* dst, IrBytecode* src) {
    IR_ASSERT(src->pool == dst->pool);

    size_t dst_size = dst->code.size;

    IrMemArena* arena = dst->pool->arena;

    if (dst->code.size == 0) {
        dst->code = src->code;
    } else {
        for (size_t i = 0; i < src->code.size; i++) {
            ir_arena_append(arena, dst->code, src->code.items[i]);
        }
    }

    if (dst->labels.size == 0) {
        dst->labels = src->labels;
        if (dst->code.size != 0) {
            for (size_t i = 0; i < dst->labels.size; i++) {
                dst->pool->list.items[dst->labels.items[i]].as.label_val.pos += dst_size;
            }
        }
    } else {
        for (size_t i = 0; i < src->labels.size; i++) {
            src->pool->list.items[src->labels.items[i]].as.label_val.pos += dst_size;
            ir_arena_append(arena, dst->labels, src->labels.items[i]);
        }
    }
}

IrInstructionID bytecode_push_op(IrBytecode* bc, IrOpcode op) {
    IrInstructionID id = bc->code.size;
    ir_arena_append(bc->pool->arena, bc->code, op);
    return id;
}

IrInstructionID bytecode_push_op_const(IrBytecode* bc, IrOpcode op, ConstId const_id) {
    IrInstructionID id = bc->code.size;
    ir_arena_append(bc->pool->arena, bc->code, op);
    ir_arena_append(bc->pool->arena, bc->code, (const_id >> 16) & 255);
    ir_arena_append(bc->pool->arena, bc->code, (const_id >> 8) & 255);
    ir_arena_append(bc->pool->arena, bc->code, const_id & 255);
    return id;
}

ConstId bytecode_push_label(IrBytecode* bc, const char* name) {
    IrConstValue val;
    val.type = IR_TYPE_LABEL;
    val.as.label_val.name = name;
    val.as.label_val.pos = bc->code.size;

    assert(bytecode_pool_get(bc->pool, val) == (size_t)-1);
    ConstId label = bytecode_pool_insert(bc->pool, val);
    ir_arena_append(bc->pool->arena, bc->labels, label);
    return label;
}

ConstId bytecode_push_constant(IrBytecode* bc, IrConstValue constant) {
    ConstId id = bytecode_pool_insert(bc->pool, constant);
    IR_ASSERT(id < 0x1000000);
    return id;
}

#define _ir_make_bc_push_op(_name, _type, _valname, _irtype) \
    IrInstructionID _name(IrBytecode* bc, IrOpcode op, _type _valname) { \
        IrConstValue constant; \
        constant.type = _irtype; \
        constant.as._valname = _valname; \
        return bytecode_push_op_const(bc, op, bytecode_push_constant(bc, constant)); \
    }

_ir_make_bc_push_op(bytecode_push_op_int, int64_t, int_val, IR_TYPE_INT)
_ir_make_bc_push_op(bytecode_push_op_float, double, float_val, IR_TYPE_FLOAT)
_ir_make_bc_push_op(bytecode_push_op_bool, bool, bool_val, IR_TYPE_BOOL)
_ir_make_bc_push_op(bytecode_push_op_func, IrFunction, func_val, IR_TYPE_FUNC)
_ir_make_bc_push_op(bytecode_push_op_list, IrList*, list_val, IR_TYPE_LIST)
_ir_make_bc_push_op(bytecode_push_op_list_string, IrList*, list_val, IR_TYPE_STRING)

#undef _ir_make_bc_push_op

void bytecode_set_op(IrBytecode* bc, IrInstructionID instr_id, IrOpcode op) {
    bc->code.items[instr_id] = op;
}

void bytecode_set_op_const(IrBytecode* bc, IrInstructionID instr_id, ConstId const_id) {
    bc->code.items[instr_id + 1] = (const_id >> 16) & 255;
    bc->code.items[instr_id + 2] = (const_id >> 8) & 255;
    bc->code.items[instr_id + 3] = const_id & 255;
}

#define _ir_make_bc_set_op(_name, _type, _valname, _irtype) \
    void _name(IrBytecode* bc, IrInstructionID instr_id, _type _valname) { \
        IrConstValue constant; \
        constant.type = _irtype; \
        constant.as._valname = _valname; \
        bytecode_set_op_const(bc, instr_id, bytecode_push_constant(bc, constant)); \
    }

_ir_make_bc_set_op(bytecode_set_op_int, int, int_val, IR_TYPE_INT)
_ir_make_bc_set_op(bytecode_set_op_float, double, float_val, IR_TYPE_FLOAT)
_ir_make_bc_set_op(bytecode_set_op_bool, bool, bool_val, IR_TYPE_BOOL)
_ir_make_bc_set_op(bytecode_set_op_func, IrFunction, func_val, IR_TYPE_FUNC)
_ir_make_bc_set_op(bytecode_set_op_list, IrList*, list_val, IR_TYPE_LIST)

#undef _ir_make_bc_set_op

IrList* bytecode_const_list_new(IrBytecodePool* pool) {
    IrList* list = ir_arena_alloc(pool->arena, sizeof(IrList));
    memset(list, 0, sizeof(IrList));
    return list;
}

void bytecode_const_list_append(IrBytecodePool* pool, IrList* list, IrValue val) {
    ir_arena_append(pool->arena, *list, val);
}

IrInstructionID bytecode_push_op_label(IrBytecode* bc, IrOpcode op, ConstId label_id) {
    IR_ASSERT(bc->pool->list.items[label_id].type == IR_TYPE_LABEL);
    return bytecode_push_op_const(bc, op, label_id);
}

void* bytecode_load_raw(IrSave* save, size_t data_size) {
    if (save->pos + data_size > save->size) return NULL;
    void* ptr = save->ptr + save->pos;
    save->pos += data_size;
    return ptr;
}

bool bytecode_load_varint(IrSave* save, uint64_t* varint) {
    *varint = 0;

    unsigned char* chunk = NULL;
    do {
        chunk = bytecode_load_raw(save, sizeof(unsigned char));
        if (!chunk) return false;
        *varint <<= 7;
        *varint |= *chunk & 0x7f;
    } while ((*chunk & 0x80) == 0);
    return true;
}

bool bytecode_load_array(IrSave* save, void** data, size_t data_size, size_t* count) {
    if (!bytecode_load_varint(save, count)) return false;

    void* arr = NULL;

    if (count != 0) {
        arr = bytecode_load_raw(save, data_size * (*count));
        if (!arr) return false;
    }

    *data = arr;
    return true;
}

size_t* bytecode_load_varint_array(IrSave* save, IrBytecodePool* pool, size_t* count) {
    if (!bytecode_load_varint(save, count)) return NULL;

    size_t* arr = ir_arena_alloc(pool->arena, sizeof(size_t) * (*count));

    for (size_t i = 0; i < *count; i++) {
        if (!bytecode_load_varint(save, arr + i)) return NULL;
    }

    return arr;
}

bool bytecode_load_value(IrSave* save, IrBytecodePool* pool, IrValue* value) {
    uint64_t type_val;
    if (!bytecode_load_varint(save, &type_val)) return false;
    IrValueType type = type_val;

    value->type = type;

    IrList* list;
    size_t list_size;

    switch (type) {
    case IR_TYPE_NOTHING: break;
    case IR_TYPE_BYTE:
        uint64_t byte_val;
        if (!bytecode_load_varint(save, &byte_val)) return false;
        value->as.byte_val = byte_val;
        break;
    case IR_TYPE_INT:
        uint64_t int_val;
        if (!bytecode_load_varint(save, &int_val)) return false;
        value->as.int_val = *(int64_t*)&int_val;
        break;
    case IR_TYPE_FLOAT:
        double* float_val = bytecode_load_raw(save, sizeof(double));
        if (!float_val) return false;
        value->as.float_val = *float_val;
        break;
    case IR_TYPE_BOOL:
        uint64_t bool_val;
        if (!bytecode_load_varint(save, &bool_val)) return false;
        value->as.bool_val = bool_val;
        break;
    case IR_TYPE_LIST:
        list = bytecode_const_list_new(pool);

        if (!bytecode_load_varint(save, &list_size)) return false;
        for (size_t i = 0; i < list_size; i++) {
            IrValue val;
            if (!bytecode_load_value(save, pool, &val)) return false;
            bytecode_const_list_append(pool, list, val);
        }
        value->as.list_val = list;
        break;
    case IR_TYPE_STRING:
        list = bytecode_const_list_new(pool);

        if (!bytecode_load_varint(save, &list_size)) return false;
        for (size_t i = 0; i < list_size; i++) {
            uint64_t val;
            if (!bytecode_load_varint(save, &val)) return false;
            bytecode_const_list_append(pool, list, (IrValue) {
                .type = IR_TYPE_INT,
                .as.int_val = *(int64_t*)&val,
            });
        }
        value->as.list_val = list;
        break;
    case IR_TYPE_FUNC:
    case IR_TYPE_LABEL:
        assert(false && "TODO");
        break;
    }

    return true;
}

bool bytecode_load_const_value(IrSave* save, IrBytecodePool* pool, IrConstValue* value) {
    uint64_t type_val;
    if (!bytecode_load_varint(save, &type_val)) return false;
    IrValueType type = type_val;

    value->type = type;

    IrList* list;
    size_t list_size;

    switch (type) {
    case IR_TYPE_NOTHING: break;
    case IR_TYPE_BYTE:
        uint64_t byte_val;
        if (!bytecode_load_varint(save, &byte_val)) return false;
        value->as.byte_val = byte_val;
        break;
    case IR_TYPE_INT:
        uint64_t int_val;
        if (!bytecode_load_varint(save, &int_val)) return false;
        value->as.int_val = *(int64_t*)&int_val;
        break;
    case IR_TYPE_FLOAT:
        double* float_val = bytecode_load_raw(save, sizeof(double));
        if (!float_val) return false;
        value->as.float_val = *float_val;
        break;
    case IR_TYPE_BOOL:
        uint64_t bool_val;
        if (!bytecode_load_varint(save, &bool_val)) return false;
        value->as.bool_val = bool_val;
        break;
    case IR_TYPE_LIST:
        list = bytecode_const_list_new(pool);

        if (!bytecode_load_varint(save, &list_size)) return false;
        for (size_t i = 0; i < list_size; i++) {
            IrValue val;
            if (!bytecode_load_value(save, pool, &val)) return false;
            bytecode_const_list_append(pool, list, val);
        }
        value->as.list_val = list;
        break;
    case IR_TYPE_STRING:
        list = bytecode_const_list_new(pool);

        if (!bytecode_load_varint(save, &list_size)) return false;
        for (size_t i = 0; i < list_size; i++) {
            uint64_t val;
            if (!bytecode_load_varint(save, &val)) return false;
            bytecode_const_list_append(pool, list, (IrValue) {
                .type = IR_TYPE_INT,
                .as.int_val = *(int64_t*)&val,
            });
        }
        value->as.list_val = list;
        break;
    case IR_TYPE_FUNC:
        size_t hint_size;
        char* hint;

        if (!bytecode_load_array(save, (void**)&hint, sizeof(char), &hint_size)) return false;
        char* hint_str = ir_arena_alloc(pool->arena, hint_size + 1);
        memcpy(hint_str, hint, hint_size);
        hint_str[hint_size] = 0;

        value->as.func_val.hint = hint_str;
        value->as.func_val.ptr = NULL;
        break;
    case IR_TYPE_LABEL:
        size_t label_size;
        char* label;

        if (!bytecode_load_array(save, (void**)&label, sizeof(char), &label_size)) return false;
        char* label_str = ir_arena_alloc(pool->arena, label_size + 1);
        memcpy(label_str, label, label_size);
        label_str[label_size] = 0;

        size_t label_pos;
        if (!bytecode_load_varint(save, &label_pos)) return false;

        value->as.label_val.name = label_str;
        value->as.label_val.pos = label_pos;
        break;
    }

    return true;
}

#define IR_LOAD_FAIL do { \
    return_val = false; \
    goto load_return; \
} while (0)

bool bytecode_load(IrBytecodePool* pool, IrBytecode* bc, const char* filepath) {
    bool return_val = true;

    if (pool->list.size > 0) return false;

    FILE* f = fopen(filepath, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* data = malloc(file_size);
    file_size = fread(data, 1, file_size, f);

    IrSave save = {
        .ptr = data,
        .pos = 0,
        .size = file_size,
    };

    size_t ident_size;
    char* ident;

    if (!bytecode_load_array(&save, (void**)&ident, sizeof(char), &ident_size)) IR_LOAD_FAIL;

    if (strncmp(ident, IR_SAVE_IDENT, ident_size)) {
        printf("Invalid ident: %.*s\n", (int)ident_size, ident);
        IR_LOAD_FAIL;
    }

    uint64_t version;
    if (!bytecode_load_varint(&save, &version)) IR_LOAD_FAIL;

    if (version < IR_SAVE_MIN_VERSION || version > IR_SAVE_MAX_VERSION) {
        printf("Invalid version: %lu. Supported bytecode versions: %d-%d\n", version, IR_SAVE_MIN_VERSION, IR_SAVE_MAX_VERSION);
        IR_LOAD_FAIL;
    }

    size_t pool_size;
    if (!bytecode_load_varint(&save, &pool_size)) IR_LOAD_FAIL;

    for (size_t i = 0; i < pool_size; i++) {
        IrConstValue value;
        if (!bytecode_load_const_value(&save, pool, &value)) IR_LOAD_FAIL;
        ir_list_append(pool->list, value);
    }

    size_t code_size;
    void* code;
    if (!bytecode_load_array(&save, &code, sizeof(unsigned char), &code_size)) IR_LOAD_FAIL;

    void* code_ptr = ir_arena_alloc(pool->arena, code_size * sizeof(unsigned char));
    memcpy(code_ptr, code, code_size * sizeof(unsigned char));

    size_t labels_size;
    size_t* labels = bytecode_load_varint_array(&save, pool, &labels_size);
    if (!labels) IR_LOAD_FAIL;

    *bc = bytecode_new(NULL, pool);

    bc->version = version;

    bc->code.size = code_size;
    bc->code.capacity = code_size;
    bc->code.items = code_ptr;

    bc->labels.size = labels_size;
    bc->labels.capacity = labels_size;
    bc->labels.items = labels;

load_return:
    free(data);
    fclose(f);
    return return_val;
}

void bytecode_save_varint(IrMemArena* save, uint64_t val) {
    uint8_t buf[16];
    size_t buf_size = 0;

    do {
        buf[15 - buf_size++] = val & 0x7f;
        val >>= 7;
    } while (val);

    buf[15] |= 1 << 7;

    uint8_t* ptr = ir_arena_alloc_packed(save, sizeof(uint8_t) * buf_size);
    memcpy(ptr, buf + 16 - buf_size, sizeof(uint8_t) * buf_size);
}

void bytecode_save_raw(IrMemArena* save, const void* data, size_t data_size) {
    assert(data_size != 0);
    void* ptr = ir_arena_alloc_packed(save, data_size);
    memcpy(ptr, data, data_size);
}

void bytecode_save_array(IrMemArena* save, const void* data, size_t data_size, size_t count) {
    assert(data_size != 0);
    bytecode_save_varint(save, count);

    if (count != 0) {
        assert(data != NULL);
        void* ptr = ir_arena_alloc_packed(save, count * data_size);
        memcpy(ptr, data, count * data_size);
    }
}

void bytecode_save_varint_array(IrMemArena* save, const size_t* data, size_t count) {
    bytecode_save_varint(save, count);

    if (count == 0) return;

    assert(data != NULL);
    for (size_t i = 0; i < count; i++) {
        bytecode_save_varint(save, data[i]);
    }
}

void bytecode_save_value(IrMemArena* save, IrValue value) {
    bytecode_save_varint(save, value.type);

    IrList* list;

    switch (value.type) {
    case IR_TYPE_NOTHING: break;
    case IR_TYPE_BYTE: bytecode_save_varint(save, value.as.byte_val); break;
    case IR_TYPE_INT: bytecode_save_varint(save, *(uint64_t*)&value.as.int_val); break;
    case IR_TYPE_FLOAT: bytecode_save_raw(save, &value.as.float_val, sizeof(double)); break;
    case IR_TYPE_BOOL: bytecode_save_varint(save, value.as.bool_val); break;
    case IR_TYPE_LIST:
        list = value.as.list_val;
        bytecode_save_varint(save, list->size);
        for (size_t i = 0; i < list->size; i++) {
            bytecode_save_value(save, list->items[i]);
        }
        break;
    case IR_TYPE_STRING:
        list = value.as.list_val;
        bytecode_save_varint(save, list->size);
        for (size_t i = 0; i < list->size; i++) {
            IrValue val = list->items[i];
            if (val.type != IR_TYPE_INT && val.type != IR_TYPE_BYTE) val.as.int_val = '?';
            bytecode_save_varint(save, *(uint64_t*)&val.as.int_val);
        }
        break;
    case IR_TYPE_FUNC:
    case IR_TYPE_LABEL:
        assert(false && "TODO");
        break;
    }
}

void bytecode_save_const_value(IrMemArena* save, IrConstValue value) {
    bytecode_save_varint(save, value.type);

    IrList* list;

    switch (value.type) {
    case IR_TYPE_NOTHING: break;
    case IR_TYPE_BYTE: bytecode_save_varint(save, value.as.byte_val); break;
    case IR_TYPE_INT: bytecode_save_varint(save, *(uint64_t*)&value.as.int_val); break;
    case IR_TYPE_FLOAT: bytecode_save_raw(save, &value.as.float_val, sizeof(double)); break;
    case IR_TYPE_BOOL: bytecode_save_varint(save, value.as.bool_val); break;
    case IR_TYPE_LIST:
        list = value.as.list_val;
        bytecode_save_varint(save, list->size);
        for (size_t i = 0; i < list->size; i++) {
            bytecode_save_value(save, list->items[i]);
        }
        break;
    case IR_TYPE_STRING:
        list = value.as.list_val;
        bytecode_save_varint(save, list->size);
        for (size_t i = 0; i < list->size; i++) {
            IrValue val = list->items[i];
            if (val.type != IR_TYPE_INT && val.type != IR_TYPE_BYTE) val.as.int_val = '?';
            bytecode_save_varint(save, *(uint64_t*)&val.as.int_val);
        }
        break;
    case IR_TYPE_FUNC:
        assert(value.as.func_val.hint != NULL);
        bytecode_save_array(save, value.as.func_val.hint, sizeof(char), strlen(value.as.func_val.hint));
        break;
    case IR_TYPE_LABEL:
        assert(value.as.label_val.name != NULL);
        bytecode_save_array(save, value.as.label_val.name, sizeof(char), strlen(value.as.label_val.name));
        bytecode_save_varint(save, value.as.label_val.pos);
        break;
    }
}

void bytecode_save(IrBytecode* bc, const char* filepath) {
    FILE* f = fopen(filepath, "wb");
    if (!f) return;

    IrMemArena* save = ir_arena_new(GiB(4), KiB(512));

    bytecode_save_array(save, IR_SAVE_IDENT, sizeof(char), sizeof(IR_SAVE_IDENT) - 1);
    bytecode_save_varint(save, IR_SAVE_MAX_VERSION);

    bytecode_save_varint(save, bc->pool->list.size);
    for (size_t i = 0; i < bc->pool->list.size; i++) {
        bytecode_save_const_value(save, bc->pool->list.items[i]);
    }

    bytecode_save_array(save, bc->code.items, sizeof(unsigned char), bc->code.size);
    bytecode_save_varint_array(save, bc->labels.items, bc->labels.size);

    fwrite(save + 1, 1, save->pos - IR_ARENA_BASE_POS, f);

    ir_arena_free(save);

    fclose(f);
}

IrFunction ir_func_by_hint(const char* hint) {
    return (IrFunction) { .hint = hint, .ptr = NULL };
}

IrFunction ir_func_by_ptr(IrRunFunction func) {
    return (IrFunction) { .hint = NULL, .ptr = func };
}

#define GET_LABEL(idx) (pool_list.items[bc->labels.items[(idx)]].as.label_val)
void bytecode_print(IrBytecode* bc) {
    size_t i         = 0,
           label_num = 0,
           op_count  = 0;
    IrFunction func;

    IrConstValueList pool_list = bc->pool->list;

    printf("; === Bytecode %s ===\n", bc->name ? bc->name : "*Unnamed*");
    while (i < bc->code.size) {
        if (label_num < bc->labels.size) {
            while (label_num < bc->labels.size && GET_LABEL(label_num).pos < i) label_num++;
            while (label_num < bc->labels.size && GET_LABEL(label_num).pos == i) {
                printf("%s:\n", GET_LABEL(label_num).name);
                label_num++;
            }
        }

        printf("    ");

        IrList* list;

        static_assert(IR_LAST == 82, "Exhaustive opcode in exec_run_bytecode");
        switch (bc->code.items[i]) {
        case IR_PUSHL:
            CHECK_IMMEDIATE;
            list = pool_list.items[CODE_IMMEDIATE].as.list_val;
            if (list) {
                printf("pushl (%p, %zu/%zu)\n", list, list->size, list->capacity);
            } else {
                printf("pushl\n");
            }
            i += 3;
            break;
        case IR_PUSHA:
            CHECK_IMMEDIATE;
            list = pool_list.items[CODE_IMMEDIATE].as.list_val;
            if (list) {
                printf("pusha (%p, %zu/%zu) \"", list, list->size, list->capacity);
                for (size_t j = 0; j < list->size; j++) {
                    if (j > 30) {
                        printf("...");
                        break;
                    }
                    IrValue c = list->items[j];
                    switch (c.type) {
                    case IR_TYPE_INT: printf("%lc", c.as.int_val); break;
                    case IR_TYPE_BYTE: printf("%c", c.as.byte_val); break;
                    default: printf("?"); break;
                    }
                }
                printf("\"\n");
            } else {
                printf("pusha\n");
            }
            i += 3;
            break;
        case IR_ILLEGAL: printf("inval\n"); break;
        case IR_PUSHN: printf("pushn\n"); break;
        case IR_POP: printf("pop\n"); break;
        case IR_DUP: printf("dup\n"); break;
        case IR_ADDI: printf("addi\n"); break;
        case IR_SUBI: printf("subi\n"); break;
        case IR_MULI: printf("muli\n"); break;
        case IR_DIVI: printf("divi\n"); break;
        case IR_MODI: printf("modi\n"); break;
        case IR_POWI: printf("powi\n"); break;
        case IR_NOTI: printf("noti\n"); break;
        case IR_ANDI: printf("andi\n"); break;
        case IR_ORI: printf("ori\n"); break;
        case IR_XORI: printf("xori\n"); break;
        case IR_ADDF: printf("addf\n"); break;
        case IR_SUBF: printf("subf\n"); break;
        case IR_MULF: printf("mulf\n"); break;
        case IR_DIVF: printf("divf\n"); break;
        case IR_MODF: printf("modf\n"); break;
        case IR_POWF: printf("powf\n"); break;
        case IR_NOT: printf("not\n"); break;
        case IR_AND: printf("and\n"); break;
        case IR_OR: printf("or\n"); break;
        case IR_XOR: printf("xor\n"); break;
        case IR_LESSI: printf("lessi\n"); break;
        case IR_MOREI: printf("morei\n"); break;
        case IR_LESSF: printf("lessf\n"); break;
        case IR_MOREF: printf("moref\n"); break;
        case IR_LESSEQI: printf("lesseqi\n"); break;
        case IR_MOREEQI: printf("moreeqi\n"); break;
        case IR_LESSEQF: printf("lesseqf\n"); break;
        case IR_MOREEQF: printf("moreeqf\n"); break;
        case IR_EQ: printf("eq\n"); break;
        case IR_NEQ: printf("neq\n"); break;
        case IR_ITOF: printf("itof\n"); break;
        case IR_ITOB: printf("itob\n"); break;
        case IR_ITOA: printf("itoa\n"); break;
        case IR_FTOI: printf("ftoi\n"); break;
        case IR_FTOB: printf("ftob\n"); break;
        case IR_FTOA: printf("ftoa\n"); break;
        case IR_BTOI: printf("btoi\n"); break;
        case IR_BTOF: printf("btof\n"); break;
        case IR_BTOA: printf("btoa\n"); break;
        case IR_ATOI: printf("atoi\n"); break;
        case IR_ATOF: printf("atof\n"); break;
        case IR_ATOB: printf("atob\n"); break;
        case IR_LTOA: printf("ltoa\n"); break;
        case IR_NTOA: printf("ntoa\n"); break;
        case IR_TOI: printf("toi\n"); break;
        case IR_TOF: printf("tof\n"); break;
        case IR_TOB: printf("tob\n"); break;
        case IR_TOA: printf("toa\n"); break;
        case IR_TOL: printf("tol\n"); break;
        case IR_TYPEOF: printf("typeof\n"); break;
        case IR_ADDL: printf("addl\n"); break;
        case IR_INDEXL: printf("indexl\n"); break;
        case IR_SETL: printf("setl\n"); break;
        case IR_INSERTL: printf("insertl\n"); break;
        case IR_DELL: printf("dell\n"); break;
        case IR_LENL: printf("lenl\n"); break;
        case IR_RET: printf("ret\n"); break;
        case IR_DYNJMP: printf("dynjmp\n"); break;
        case IR_DYNIF: printf("dynif\n"); break;
        case IR_DYNCALL: printf("dyncall\n"); break;
        case IR_DYNRUN: printf("dynrun\n"); break;
        case IR_PUSHI:
            CHECK_IMMEDIATE;
            printf("pushi %ld\n", pool_list.items[CODE_IMMEDIATE].as.int_val);
            i += 3;
            break;
        case IR_PUSHF:
            CHECK_IMMEDIATE;
            printf("pushf %g\n", pool_list.items[CODE_IMMEDIATE].as.float_val);
            i += 3;
            break;
        case IR_PUSHB:
            CHECK_IMMEDIATE;
            printf("pushb %s\n", pool_list.items[CODE_IMMEDIATE].as.bool_val ? "true" : "false");
            i += 3;
            break;
        case IR_PUSHLB:
            CHECK_IMMEDIATE;
            printf("pushlb <%s>\n", pool_list.items[CODE_IMMEDIATE].as.label_val.name);
            i += 3;
            break;
        case IR_PUSHFN:
            CHECK_IMMEDIATE;
            func = pool_list.items[CODE_IMMEDIATE].as.func_val;
            if (func.ptr) {
                if (func.hint) {
                    printf("pushfn \"%s\" (%p)\n", func.hint, func.ptr);
                } else {
                    printf("pushfn (%p)\n", func.ptr);
                }
            } else {
                if (func.hint) {
                    printf("pushfn \"%s\"\n", func.hint);
                } else {
                    printf("pushfn inval\n");
                }
            }
            i += 3;
            break;
        case IR_POPC:
            CHECK_IMMEDIATE;
            printf("popc %ld\n", pool_list.items[CODE_IMMEDIATE].as.int_val);
            i += 3;
            break;
        case IR_LOAD:
            CHECK_IMMEDIATE;
            printf("load %ld\n", pool_list.items[CODE_IMMEDIATE].as.int_val);
            i += 3;
            break;
        case IR_STORE:
            CHECK_IMMEDIATE;
            printf("store %ld\n", pool_list.items[CODE_IMMEDIATE].as.int_val);
            i += 3;
            break;
        case IR_GLOAD:
            CHECK_IMMEDIATE;
            printf("gload %ld\n", pool_list.items[CODE_IMMEDIATE].as.int_val);
            i += 3;
            break;
        case IR_GSTORE:
            CHECK_IMMEDIATE;
            printf("gstore %ld\n", pool_list.items[CODE_IMMEDIATE].as.int_val);
            i += 3;
            break;
        case IR_JMP:
            CHECK_IMMEDIATE;
            printf("jmp <%s>\n", pool_list.items[CODE_IMMEDIATE].as.label_val.name);
            i += 3;
            break;
        case IR_IF:
            CHECK_IMMEDIATE;
            printf("if <%s>\n", pool_list.items[CODE_IMMEDIATE].as.label_val.name);
            i += 3;
            break;
        case IR_IFNOT:
            CHECK_IMMEDIATE;
            printf("ifnot <%s>\n", pool_list.items[CODE_IMMEDIATE].as.label_val.name);
            i += 3;
            break;
        case IR_CALL:
            CHECK_IMMEDIATE;
            printf("call <%s>\n", pool_list.items[CODE_IMMEDIATE].as.label_val.name);
            i += 3;
            break;
        case IR_RUN:
            CHECK_IMMEDIATE;
            func = pool_list.items[CODE_IMMEDIATE].as.func_val;
            if (func.ptr) {
                if (func.hint) {
                    printf("run \"%s\" (%p)\n", func.hint, func.ptr);
                } else {
                    printf("run (%p)\n", func.ptr);
                }
            } else {
                if (func.hint) {
                    printf("run \"%s\"\n", func.hint);
                } else {
                    printf("run inval\n");
                }
            }
            i += 3;
            break;
        default: printf("unknown\n"); break;
        }
        i++;
        op_count++;
    }
    printf("; Op count: %zu, Code: %zu bytes\n", op_count, bc->code.size);
}

void exec_set_error(IrExec* exec, const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vsnprintf(exec->last_error, IR_LAST_ERROR_SIZE, fmt, va);
    va_end(va);
}

static void* exec_heap_malloc(IrHeap* heap, size_t size) {
    if (heap->mem->pos + size > heap->mem_max) return NULL;
    void* ptr = ir_arena_alloc(heap->mem, size);
    if (ptr) heap->chunks_count++;
    return ptr;
}

static bool exec_heap_copy_chunk(IrExec* exec, void** ref_data) {
    if (*ref_data == NULL) return false;

    IrHeapChunk* chunk = (*(IrHeapChunk**)ref_data) - 1;
    if ((void*)chunk < (void*)exec->heap.mem ||
        (void*)chunk >= (void*)exec->heap.mem + exec->heap.mem->pos)
    {
        return false;
    }

    if (chunk->copy_ptr) {
        *ref_data = chunk->copy_ptr;
        return false;
    }

    IrHeapChunk* new_chunk = exec_heap_malloc(&exec->second_heap, sizeof(IrHeapChunk) + chunk->size);
    if (!new_chunk) return false;

    memcpy(new_chunk, chunk, sizeof(IrHeapChunk) + chunk->size);
    new_chunk->copy_ptr = NULL;

    chunk->copy_ptr = new_chunk->data;
    *ref_data = (void*)new_chunk->data;
    return true;
}

static void exec_heap_copy_value(IrExec* exec, IrValue* value) {
    switch (value->type) {
    case IR_TYPE_STRING: ;
        if (!exec_heap_copy_chunk(exec, (void**)&value->as.list_val)) break;
        if (!exec_heap_copy_chunk(exec, (void**)&value->as.list_val->items)) break;
        break;
    case IR_TYPE_LIST: ;
        if (!exec_heap_copy_chunk(exec, (void**)&value->as.list_val)) break;

        if (!exec_heap_copy_chunk(exec, (void**)&value->as.list_val->items)) break;
        for (size_t i = 0; i < value->as.list_val->size; i++) {
            exec_heap_copy_value(exec, &value->as.list_val->items[i]);
        }
        break;
    default:
        break;
    }
}

void exec_collect(IrExec* exec) {
    ir_arena_clear(exec->second_heap.mem);
    exec->second_heap.chunks_count = 0;
    exec->second_heap.mem_max = exec->heap.mem_max;

    // Copy stack values
    for (size_t i = 0; i < exec->stack.size; i++) {
        exec_heap_copy_value(exec, &exec->stack.items[i]);
    }

    // Copy variable values
    for (size_t i = 0; i < exec->variables.size; i++) {
        IrValueList* frame = &exec->variables.items[i];
        for (size_t j = 0; j < frame->size; j++) {
            exec_heap_copy_value(exec, &frame->items[j]);
        }
    }

    for (size_t i = 0; i < exec->globals.size; i++) {
        exec_heap_copy_value(exec, &exec->globals.items[i]);
    }

    size_t memory_freed = exec->heap.mem->pos - exec->second_heap.mem->pos;
    size_t chunks_deleted = exec->heap.chunks_count - exec->second_heap.chunks_count;

    IrHeap temp_heap = exec->heap;
    exec->heap = exec->second_heap;
    exec->second_heap = temp_heap;

    printf("exec_collect: %zu bytes freed, %zu chunks deleted\n", memory_freed, chunks_deleted);
}

void* exec_malloc(IrExec* exec, size_t size) {
    if (size == 0) return NULL;

    const size_t chunk_size = sizeof(IrHeapChunk) + size;
    IrHeapChunk* chunk = exec_heap_malloc(&exec->heap, chunk_size);
    if (chunk == NULL) {
        exec_collect(exec);

        chunk = exec_heap_malloc(&exec->heap, chunk_size);
        if (chunk == NULL) {
            while (exec->heap.mem->pos + chunk_size > exec->heap.mem_max && exec->heap.mem_max < exec->heap.mem->reserve_size) {
                exec->heap.mem_max = MIN(exec->heap.mem_max * 2, exec->heap.mem->reserve_size);
            }
            printf("exec_malloc: raising memory limit to %zu bytes\n", exec->heap.mem_max);
            chunk = exec_heap_malloc(&exec->heap, chunk_size);

            if (chunk == NULL) {
                exec_set_error(exec, "Heap out of memory. Tried to allocate %zu bytes but only %zu bytes were free", chunk_size, exec->heap.mem->reserve_size - exec->heap.mem->pos);
                return NULL;
            }
        }
    }

    chunk->copy_ptr = NULL;
    chunk->size = size;
    return chunk->data;
}

void* exec_realloc(IrExec* exec, void* ptr, size_t new_size) {
    if (!ptr) return exec_malloc(exec, new_size);
    if (new_size == 0) return NULL;

    IrHeapChunk* old_chunk = (IrHeapChunk*)ptr - 1;

    const size_t new_chunk_size = sizeof(IrHeapChunk) + new_size;
    IrHeapChunk* new_chunk = exec_heap_malloc(&exec->heap, new_chunk_size);
    if (new_chunk == NULL) {
        // Avoid losing ptr value by copying chunk contents into temporary block
        IrHeapChunk* new_old_chunk = malloc(sizeof(IrHeapChunk) + old_chunk->size);
        memcpy(new_old_chunk, old_chunk, sizeof(IrHeapChunk) + old_chunk->size);
        old_chunk = new_old_chunk;

        exec_collect(exec);

        new_chunk = exec_heap_malloc(&exec->heap, new_chunk_size);
        if (new_chunk == NULL) {
            while (exec->heap.mem->pos + new_chunk_size > exec->heap.mem_max && exec->heap.mem_max < exec->heap.mem->reserve_size) {
                exec->heap.mem_max = MIN(exec->heap.mem_max * 2, exec->heap.mem->reserve_size);
            }
            printf("exec_realloc: raising memory limit to %zu bytes\n", exec->heap.mem_max);
            new_chunk = exec_heap_malloc(&exec->heap, new_chunk_size);

            if (new_chunk == NULL) {
                exec_set_error(exec, "Heap out of memory. Tried to allocate %zu bytes but only %zu bytes were free", new_chunk_size, exec->heap.mem_max - exec->heap.mem->pos);
                free(new_old_chunk);
                return NULL;
            }
        }
    }

    new_chunk->copy_ptr = NULL;
    new_chunk->size = new_size;

    memcpy(new_chunk->data, old_chunk->data, old_chunk->size);

    if ((IrHeapChunk*)ptr - 1 != old_chunk) free(old_chunk);
    return new_chunk->data;
}

static void exec_heap_free(IrHeap* heap) {
    if (heap->mem) {
        ir_arena_free(heap->mem);
        heap->mem = NULL;
    }
}

static IrHeap exec_heap_new(size_t memory_min, size_t memory_max) {
    IrHeap heap = {0};
    if (memory_max == 0 || memory_min > memory_max) return heap;
    heap.mem = ir_arena_new(memory_max, memory_min);
    heap.mem_max = memory_min;
    return heap;
}

IrExec exec_new(size_t memory_min, size_t memory_max) {
    IrExec exec = {0};

    IrHeap heap = exec_heap_new(memory_min, memory_max);
    IrHeap second_heap = exec_heap_new(memory_min, memory_max);

    exec.heap = heap;
    exec.second_heap = second_heap;
    return exec;
}

void exec_free(IrExec* exec) {
    ir_list_free(exec->chunks);
    ir_list_free(exec->stack);
    ir_list_free(exec->globals);

    for (size_t i = 0; i < exec->variables.size; i++) {
        ir_list_free(exec->variables.items[i]);
    }
    ir_list_free(exec->variables);

    printf("exec_free: %zu bytes allocated, %zu chunks created\n", exec->heap.mem->pos, exec->heap.chunks_count);
    exec_heap_free(&exec->heap);
    exec_heap_free(&exec->second_heap);
}

void exec_set_run_function_resolver(IrExec* exec, IrRunFunctionResolver resolver) {
    exec->resolve_run_function = resolver;
}

void exec_add_bytecode(IrExec* exec, IrBytecode bc) {
    ir_list_append(exec->chunks, bc);
}

IrBytecode* exec_find_bytecode(IrExec* exec, const char* bc_name) {
    for (size_t i = 0; i < exec->chunks.size; i++) {
        if (!strcmp(exec->chunks.items[i].name, bc_name)) return &exec->chunks.items[i];
    }
    return NULL;
}

IrLabel* bytecode_find_label(IrBytecode* bc, const char* label_name) {
    IrConstValue label;
    label.type = IR_TYPE_LABEL;
    label.as.label_val.name = label_name;

    ConstId id = bytecode_pool_get(bc->pool, label);
    if (id == (size_t)-1) return NULL;
    return &bc->pool->list.items[id].as.label_val;
}

bool exec_run(IrExec* exec, const char* bc_name, const char* label_name) {
    IrBytecode* bc = exec_find_bytecode(exec, bc_name);
    if (!bc) {
        exec_set_error(exec, "Bytecode with name \"%s\" is not found", bc_name);
        return false;
    }

    IrLabel* label = bytecode_find_label(bc, label_name);
    if (!label) {
        exec_set_error(exec, "Label with name \"%s\" is not found in bytecode \"%s\"", label_name, bc_name);
        return false;
    }

    return exec_run_bytecode(exec, bc, label->pos);
}

void exec_push_variable_stack(IrExec* exec) {
    ir_list_append(exec->variables, (IrValueList) {0});
}

void exec_pop_variable_stack(IrExec* exec) {
    ir_list_free(exec->variables.items[exec->variables.size - 1]);
    exec->variables.size--;
}

void exec_push_value(IrExec* exec, IrValue value) {
    ir_list_append(exec->stack, value);
}

#define _ir_make_push(_name, _type, _valname, _irtype) \
void _name(IrExec* exec, _type _valname) { \
    IrValue val; \
    val.type = _irtype; \
    val.as._valname = _valname; \
    exec_push_value(exec, val); \
}

_ir_make_push(exec_push_int, int64_t, int_val, IR_TYPE_INT)
_ir_make_push(exec_push_float, double, float_val, IR_TYPE_FLOAT)
_ir_make_push(exec_push_bool, bool, bool_val, IR_TYPE_BOOL)
_ir_make_push(exec_push_func, IrRunFunction, func_val, IR_TYPE_FUNC)
_ir_make_push(exec_push_label, size_t, label_val, IR_TYPE_LABEL)
_ir_make_push(exec_push_list, IrList*, list_val, IR_TYPE_LIST)
_ir_make_push(exec_push_list_string, IrList*, list_val, IR_TYPE_STRING)

#undef _ir_make_push

void exec_push_nothing(IrExec* exec) {
    IrValue val = {0};
    exec_push_value(exec, val);
}

IrValue exec_get_value(IrExec* exec) {
    IR_ASSERT(exec->stack.size > 0);
    return exec->stack.items[exec->stack.size - 1];;
}

#define _ir_make_get(_name, _type, _valname, _irtype) \
_type _name(IrExec* exec) { \
    IrValue value = exec_get_value(exec); \
    IR_ASSERT(value.type == _irtype); \
    return value.as._valname; \
}

_ir_make_get(exec_get_int, int64_t, int_val, IR_TYPE_INT)
_ir_make_get(exec_get_float, double, float_val, IR_TYPE_FLOAT)
_ir_make_get(exec_get_bool, bool, bool_val, IR_TYPE_BOOL)
_ir_make_get(exec_get_func, IrRunFunction, func_val, IR_TYPE_FUNC)
_ir_make_get(exec_get_label, size_t, label_val, IR_TYPE_LABEL)
_ir_make_get(exec_get_list, IrList*, list_val, IR_TYPE_LIST)
_ir_make_get(exec_get_list_string, IrList*, list_val, IR_TYPE_STRING)

#undef _ir_make_get

IrValue exec_pop_value(IrExec* exec) {
    IR_ASSERT(exec->stack.size > 0);
    return exec->stack.items[--exec->stack.size];
}

void exec_pop_multiple(IrExec* exec, size_t count) {
    IR_ASSERT(exec->stack.size >= count);
    exec->stack.size -= count;
}

void exec_dup_value(IrExec* exec) {
    exec_push_value(exec, exec_get_value(exec));
}

#define _ir_make_pop(_name, _type, _valname, _irtype) \
_type _name(IrExec* exec) { \
    IR_ASSERT(exec->stack.size > 0); \
    IrValue val = exec_pop_value(exec); \
    IR_ASSERT(val.type == _irtype); \
    return val.as._valname; \
}

_ir_make_pop(exec_pop_int, int64_t, int_val, IR_TYPE_INT)
_ir_make_pop(exec_pop_float, double, float_val, IR_TYPE_FLOAT)
_ir_make_pop(exec_pop_bool, bool, bool_val, IR_TYPE_BOOL)
_ir_make_pop(exec_pop_func, IrRunFunction, func_val, IR_TYPE_FUNC)
_ir_make_pop(exec_pop_label, size_t, label_val, IR_TYPE_LABEL)
_ir_make_pop(exec_pop_list, IrList*, list_val, IR_TYPE_LIST)
_ir_make_pop(exec_pop_list_string, IrList*, list_val, IR_TYPE_STRING)

#undef _ir_make_pop

IrList* exec_list_new(IrExec* exec) {
    IrList* list = exec_malloc(exec, sizeof(IrList));
    if (!list) return NULL;
    memset(list, 0, sizeof(IrList));
    list->owned = true;
    return list;
}

// NOTE: This only accepts ascii text
bool exec_push_string(IrExec* exec, const char* str) {
    IrList* list = exec_list_new(exec);
    if (!list) return false;
    exec_push_list_string(exec, list);

    size_t str_len = strlen(str);
    list->size = str_len;
    list->capacity = str_len;

    void* items = exec_realloc(exec, list->items, list->capacity * sizeof(*list->items));
    if (!items) return false;

    list = exec_get_list_string(exec);
    list->items = items;

    for (size_t i = 0; i < str_len; i++) {
        IrValue val;
        val.type = IR_TYPE_INT;
        val.as.int_val = str[i];
        list->items[i] = val;
    }
    return true;
}

void exec_get_string(IrList* string, char* buf, size_t buf_len) {
    IR_ASSERT(string != NULL);

    size_t i;
    for (i = 0; i < string->size && i < buf_len - 1; i++) {
        IrValue value = string->items[i];
        switch (value.type) {
        case IR_TYPE_INT:
        case IR_TYPE_BYTE:
            buf[i] = value.as.byte_val;
            break;
        default:
            buf[i] = '?';
            break;
        }
    }
    buf[i] = '\0';
}

void exec_pop_string(IrExec* exec, char* buf, size_t buf_len) {
    IrList* list = exec_pop_list_string(exec);
    exec_get_string(list, buf, buf_len);
}

int64_t ir_int_pow(int64_t base, int64_t exp) {
    if (exp == 0) return 1;

    int64_t result = 1;
    while (exp) {
        if (exp & 1) result *= base;
        exp >>= 1;
        base *= base;
    }
    return result;
}

#define IR_EXEC_FAIL do { \
    return_val = false; \
    goto exec_return; \
} while (0)
#define IR_STRING_BUF_LEN 64

bool exec_run_bytecode(IrExec* exec, IrBytecode* bc, size_t pos) {
    bool return_val = true;
    exec_push_variable_stack(exec);

    IrConstValueList pool_list = bc->pool->list;

    IrValueList* variable_frame;
    int64_t variable_frame_pos;

    char string_buf[IR_STRING_BUF_LEN];

    int64_t left_int,   right_int;
    double  left_float, right_float;
    bool    left_bool,  right_bool;
    IrValue left_value, right_value;
    IrList* list;
    size_t label_pos;
    IrFunction* func;

    for (size_t i = pos; i < bc->code.size; i++) {
        static_assert(IR_LAST == 82, "Exhaustive opcode in exec_run_bytecode");
        switch (bc->code.items[i]) {
        case IR_PUSHN: exec_push_nothing(exec); break;
        case IR_PUSHI:
            exec_push_int(exec, pool_list.items[CODE_IMMEDIATE].as.int_val);
            i += 3;
            break;
        case IR_PUSHF:
            exec_push_float(exec, pool_list.items[CODE_IMMEDIATE].as.float_val);
            i += 3;
            break;
        case IR_PUSHB:
            exec_push_bool(exec, pool_list.items[CODE_IMMEDIATE].as.bool_val);
            i += 3;
            break;
        case IR_PUSHLB:
            exec_push_label(exec, pool_list.items[CODE_IMMEDIATE].as.label_val.pos);
            i += 3;
            break;
        case IR_PUSHFN:
            exec_push_func(exec, pool_list.items[CODE_IMMEDIATE].as.func_val.ptr);
            i += 3;
            break;
        case IR_POP: exec_pop_value(exec); break;
        case IR_POPC:
            exec_pop_multiple(exec, pool_list.items[CODE_IMMEDIATE].as.int_val);
            i += 3;
            break;
        case IR_DUP: exec_dup_value(exec); break;
        case IR_LOAD:
            IR_ASSERT(exec->variables.size > 0);
            variable_frame = &exec->variables.items[exec->variables.size - 1];
            variable_frame_pos = pool_list.items[CODE_IMMEDIATE].as.int_val;
            IR_ASSERT(variable_frame_pos >= 0);
            IR_ASSERT((size_t)variable_frame_pos < variable_frame->size);
            exec_push_value(exec, variable_frame->items[variable_frame_pos]);
            i += 3;
            break;
        case IR_STORE:
            IR_ASSERT(exec->variables.size > 0);
            variable_frame = &exec->variables.items[exec->variables.size - 1];
            variable_frame_pos = pool_list.items[CODE_IMMEDIATE].as.int_val;
            IR_ASSERT(variable_frame_pos >= 0);

            left_value = exec_pop_value(exec);
            if ((size_t)variable_frame_pos >= variable_frame->size) {
                while ((size_t)variable_frame_pos > variable_frame->size) {
                    ir_list_append(*variable_frame, (IrValue) {0});
                }
                ir_list_append(*variable_frame, left_value);
            } else {
                variable_frame->items[variable_frame_pos] = left_value;
            }
            i += 3;
            break;
        case IR_GLOAD:
            variable_frame_pos = pool_list.items[CODE_IMMEDIATE].as.int_val;
            IR_ASSERT(variable_frame_pos >= 0);
            IR_ASSERT((size_t)variable_frame_pos < exec->globals.size);
            exec_push_value(exec, exec->globals.items[variable_frame_pos]);
            i += 3;
            break;
        case IR_GSTORE:
            variable_frame_pos = pool_list.items[CODE_IMMEDIATE].as.int_val;
            IR_ASSERT(variable_frame_pos >= 0);

            left_value = exec_pop_value(exec);
            if ((size_t)variable_frame_pos >= exec->globals.size) {
                while ((size_t)variable_frame_pos > exec->globals.size) {
                    ir_list_append(exec->globals, (IrValue) {0});
                }
                ir_list_append(exec->globals, left_value);
            } else {
                exec->globals.items[variable_frame_pos] = left_value;
            }
            i += 3;
            break;
        case IR_ADDI:
            right_int = exec_pop_int(exec);
            left_int  = exec_pop_int(exec);
            exec_push_int(exec, left_int + right_int);
            break;
        case IR_SUBI:
            right_int = exec_pop_int(exec);
            left_int  = exec_pop_int(exec);
            exec_push_int(exec, left_int - right_int);
            break;
        case IR_MULI:
            right_int = exec_pop_int(exec);
            left_int  = exec_pop_int(exec);
            exec_push_int(exec, left_int * right_int);
            break;
        case IR_DIVI:
            right_int = exec_pop_int(exec);
            left_int  = exec_pop_int(exec);
            exec_push_int(exec, left_int / right_int);
            break;
        case IR_MODI:
            right_int = exec_pop_int(exec);
            left_int  = exec_pop_int(exec);
            exec_push_int(exec, left_int % right_int);
            break;
        case IR_POWI:
            right_int = exec_pop_int(exec);
            left_int  = exec_pop_int(exec);
            exec_push_int(exec, ir_int_pow(left_int, right_int));
            break;
        case IR_NOTI:
            left_int  = exec_pop_int(exec);
            exec_push_int(exec, ~left_int);
            break;
        case IR_ANDI:
            right_int = exec_pop_int(exec);
            left_int  = exec_pop_int(exec);
            exec_push_int(exec, left_int & right_int);
            break;
        case IR_ORI:
            right_int = exec_pop_int(exec);
            left_int  = exec_pop_int(exec);
            exec_push_int(exec, left_int | right_int);
            break;
        case IR_XORI:
            right_int = exec_pop_int(exec);
            left_int  = exec_pop_int(exec);
            exec_push_int(exec, left_int ^ right_int);
            break;
        case IR_ADDF:
            right_float = exec_pop_float(exec);
            left_float  = exec_pop_float(exec);
            exec_push_float(exec, left_float + right_float);
            break;
        case IR_SUBF:
            right_float = exec_pop_float(exec);
            left_float  = exec_pop_float(exec);
            exec_push_float(exec, left_float - right_float);
            break;
        case IR_MULF:
            right_float = exec_pop_float(exec);
            left_float  = exec_pop_float(exec);
            exec_push_float(exec, left_float * right_float);
            break;
        case IR_DIVF:
            right_float = exec_pop_float(exec);
            left_float  = exec_pop_float(exec);
            exec_push_float(exec, left_float / right_float);
            break;
        case IR_MODF:
            right_float = exec_pop_float(exec);
            left_float  = exec_pop_float(exec);
            exec_push_float(exec, fmod(left_float, right_float));
            break;
        case IR_POWF:
            right_float = exec_pop_float(exec);
            left_float  = exec_pop_float(exec);
            exec_push_float(exec, pow(left_float, right_float));
            break;
        case IR_NOT:
            left_bool = exec_pop_bool(exec);
            exec_push_bool(exec, !left_bool);
            break;
        case IR_AND:
            right_bool = exec_pop_bool(exec);
            left_bool = exec_pop_bool(exec);
            exec_push_bool(exec, left_bool && right_bool);
            break;
        case IR_OR:
            right_bool = exec_pop_bool(exec);
            left_bool = exec_pop_bool(exec);
            exec_push_bool(exec, left_bool || right_bool);
            break;
        case IR_XOR:
            right_bool = exec_pop_bool(exec);
            left_bool = exec_pop_bool(exec);
            exec_push_bool(exec, left_bool != right_bool);
            break;
        case IR_LESSI:
            right_int = exec_pop_int(exec);
            left_int = exec_pop_int(exec);
            exec_push_bool(exec, left_int < right_int);
            break;
        case IR_MOREI:
            right_int = exec_pop_int(exec);
            left_int = exec_pop_int(exec);
            exec_push_bool(exec, left_int > right_int);
            break;
        case IR_LESSEQI:
            right_int = exec_pop_int(exec);
            left_int = exec_pop_int(exec);
            exec_push_bool(exec, left_int <= right_int);
            break;
        case IR_MOREEQI:
            right_int = exec_pop_int(exec);
            left_int = exec_pop_int(exec);
            exec_push_bool(exec, left_int >= right_int);
            break;
        case IR_LESSF:
            right_float = exec_pop_float(exec);
            left_float  = exec_pop_float(exec);
            exec_push_bool(exec, left_float < right_float);
            break;
        case IR_MOREF:
            right_float = exec_pop_float(exec);
            left_float  = exec_pop_float(exec);
            exec_push_bool(exec, left_float > right_float);
            break;
        case IR_LESSEQF:
            right_float = exec_pop_float(exec);
            left_float  = exec_pop_float(exec);
            exec_push_bool(exec, left_float <= right_float);
            break;
        case IR_MOREEQF:
            right_float = exec_pop_float(exec);
            left_float  = exec_pop_float(exec);
            exec_push_bool(exec, left_float >= right_float);
            break;
        case IR_EQ:
            right_value = exec_pop_value(exec);
            left_value  = exec_pop_value(exec);
            if (left_value.type != right_value.type) {
                exec_push_bool(exec, false);
                break;
            }

            switch (left_value.type) {
            case IR_TYPE_NOTHING: exec_push_bool(exec, true); break;
            case IR_TYPE_BYTE: exec_push_bool(exec, left_value.as.byte_val == right_value.as.byte_val); break;
            case IR_TYPE_INT: exec_push_bool(exec, left_value.as.int_val == right_value.as.int_val); break;
            case IR_TYPE_FLOAT: exec_push_bool(exec, left_value.as.float_val == right_value.as.float_val); break;
            case IR_TYPE_BOOL: exec_push_bool(exec, left_value.as.bool_val == right_value.as.bool_val); break;
            case IR_TYPE_LIST: exec_push_bool(exec, left_value.as.list_val == right_value.as.list_val); break;
            case IR_TYPE_STRING:
                if (left_value.as.list_val->size != right_value.as.list_val->size) {
                    exec_push_bool(exec, false);
                    break;
                }
                for (size_t i = 0; i < left_value.as.list_val->size; i++) {
                    if (left_value.as.list_val->items[i].as.int_val != right_value.as.list_val->items[i].as.int_val) {
                        exec_push_bool(exec, false);
                        break;
                    }
                }
                exec_push_bool(exec, true);
                break;
            case IR_TYPE_FUNC: exec_push_bool(exec, left_value.as.func_val == right_value.as.func_val); break;
            case IR_TYPE_LABEL: exec_push_bool(exec, left_value.as.label_val == right_value.as.label_val); break;
            }
            break;
        case IR_NEQ:
            right_value = exec_pop_value(exec);
            left_value  = exec_pop_value(exec);
            if (left_value.type != right_value.type) {
                exec_push_bool(exec, true);
                break;
            }

            switch (left_value.type) {
            case IR_TYPE_NOTHING: exec_push_bool(exec, false); break;
            case IR_TYPE_BYTE: exec_push_bool(exec, left_value.as.byte_val != right_value.as.byte_val); break;
            case IR_TYPE_INT: exec_push_bool(exec, left_value.as.int_val != right_value.as.int_val); break;
            case IR_TYPE_FLOAT: exec_push_bool(exec, left_value.as.float_val != right_value.as.float_val); break;
            case IR_TYPE_BOOL: exec_push_bool(exec, left_value.as.bool_val != right_value.as.bool_val); break;
            case IR_TYPE_LIST: exec_push_bool(exec, left_value.as.list_val != right_value.as.list_val); break;
            case IR_TYPE_STRING:
                if (left_value.as.list_val->size != right_value.as.list_val->size) {
                    exec_push_bool(exec, true);
                    break;
                }
                for (size_t i = 0; i < left_value.as.list_val->size; i++) {
                    if (left_value.as.list_val->items[i].as.int_val != right_value.as.list_val->items[i].as.int_val) {
                        exec_push_bool(exec, true);
                        break;
                    }
                }
                exec_push_bool(exec, false);
                break;
            case IR_TYPE_FUNC: exec_push_bool(exec, left_value.as.func_val != right_value.as.func_val); break;
            case IR_TYPE_LABEL: exec_push_bool(exec, left_value.as.label_val != right_value.as.label_val); break;
            }
            break;
        case IR_ITOF: exec_push_float(exec, exec_pop_int(exec)); break;
        case IR_ITOB: exec_push_bool(exec, exec_pop_int(exec) != 0); break;
        case IR_ITOA:
            left_int = exec_pop_int(exec);
            snprintf(string_buf, IR_STRING_BUF_LEN, "%ld", left_int);
            if (!exec_push_string(exec, string_buf)) IR_EXEC_FAIL;
            break;
        case IR_FTOI: exec_push_int(exec, exec_pop_float(exec)); break;
        case IR_FTOB: exec_push_bool(exec, exec_pop_float(exec) != 0); break;
        case IR_FTOA:
            left_float = exec_pop_float(exec);
            snprintf(string_buf, IR_STRING_BUF_LEN, "%g", left_float);
            if (!exec_push_string(exec, string_buf)) IR_EXEC_FAIL;
            break;
        case IR_BTOI: exec_push_int(exec, exec_pop_bool(exec)); break;
        case IR_BTOF: exec_push_float(exec, exec_pop_bool(exec)); break;
        case IR_BTOA:
            left_bool = exec_pop_bool(exec);
            snprintf(string_buf, IR_STRING_BUF_LEN, "%s", left_bool ? "true" : "false");
            if (!exec_push_string(exec, string_buf)) IR_EXEC_FAIL;
            break;
        case IR_NTOA:
            exec_pop_value(exec);
            exec_push_string(exec, "nothing");
            break;
        case IR_LTOA:
            list = exec_pop_list(exec);
            IR_ASSERT(list != NULL);
            if (list->size == 0) {
                snprintf(string_buf, IR_STRING_BUF_LEN, "[List: Empty]");
            } else {
                snprintf(string_buf, IR_STRING_BUF_LEN, "[List: %p, %zu/%zu]", list, list->size, list->capacity);
            }
            if (!exec_push_string(exec, string_buf)) IR_EXEC_FAIL;
            break;

        case IR_ATOI:
            exec_pop_string(exec, string_buf, IR_STRING_BUF_LEN);
            exec_push_int(exec, atol(string_buf));
            break;
        case IR_ATOF:
            exec_pop_string(exec, string_buf, IR_STRING_BUF_LEN);
            exec_push_float(exec, atof(string_buf));
            break;
        case IR_ATOB:
            exec_pop_string(exec, string_buf, IR_STRING_BUF_LEN);
            exec_push_bool(exec, string_buf[0] != '\0' ? true : false);
            break;

        case IR_TOI:
            left_value = exec_pop_value(exec);
            switch (left_value.type) {
            case IR_TYPE_INT:   exec_push_int(exec, left_value.as.int_val); break;
            case IR_TYPE_FLOAT: exec_push_int(exec, left_value.as.float_val); break;
            case IR_TYPE_BOOL:  exec_push_int(exec, left_value.as.bool_val); break;
            case IR_TYPE_BYTE:  exec_push_int(exec, left_value.as.byte_val); break;
            case IR_TYPE_STRING:
                exec_get_string(left_value.as.list_val, string_buf, IR_STRING_BUF_LEN);
                exec_push_int(exec, atol(string_buf));
                break;
            case IR_TYPE_NOTHING: exec_push_int(exec, 0); break;
            default:
                exec_set_error(exec, "Invalid type passed to toi");
                IR_EXEC_FAIL;
                break;
            }
            break;
        case IR_TOF:
            left_value = exec_pop_value(exec);
            switch (left_value.type) {
            case IR_TYPE_INT:   exec_push_float(exec, left_value.as.int_val); break;
            case IR_TYPE_FLOAT: exec_push_float(exec, left_value.as.float_val); break;
            case IR_TYPE_BOOL:  exec_push_float(exec, left_value.as.bool_val); break;
            case IR_TYPE_BYTE:  exec_push_float(exec, left_value.as.byte_val); break;
            case IR_TYPE_STRING:
                exec_get_string(left_value.as.list_val, string_buf, IR_STRING_BUF_LEN);
                exec_push_float(exec, atof(string_buf));
                break;
            case IR_TYPE_NOTHING: exec_push_float(exec, 0.0); break;
            default:
                exec_set_error(exec, "Invalid type passed to tof");
                IR_EXEC_FAIL;
                break;
            }
            break;
        case IR_TOB:
            left_value = exec_pop_value(exec);
            switch (left_value.type) {
            case IR_TYPE_INT:   exec_push_bool(exec, left_value.as.int_val != 0); break;
            case IR_TYPE_FLOAT: exec_push_bool(exec, left_value.as.float_val != 0); break;
            case IR_TYPE_BOOL:  exec_push_bool(exec, left_value.as.bool_val); break;
            case IR_TYPE_BYTE:  exec_push_bool(exec, left_value.as.byte_val != 0); break;
            case IR_TYPE_STRING:
                exec_get_string(left_value.as.list_val, string_buf, IR_STRING_BUF_LEN);
                exec_push_bool(exec, string_buf[0] != '\0' ? true : false);
                break;
            case IR_TYPE_NOTHING: exec_push_bool(exec, false); break;
            default:
                exec_set_error(exec, "Invalid type passed to tob");
                IR_EXEC_FAIL;
                break;
            }
            break;
        case IR_TOA:
            left_value = exec_pop_value(exec);
            switch (left_value.type) {
            case IR_TYPE_INT:
                snprintf(string_buf, IR_STRING_BUF_LEN, "%ld", left_value.as.int_val);
                if (!exec_push_string(exec, string_buf)) IR_EXEC_FAIL;
                break;
            case IR_TYPE_FLOAT:
                snprintf(string_buf, IR_STRING_BUF_LEN, "%g", left_value.as.float_val);
                if (!exec_push_string(exec, string_buf)) IR_EXEC_FAIL;
                break;
            case IR_TYPE_BOOL:
                snprintf(string_buf, IR_STRING_BUF_LEN, "%s", left_value.as.bool_val ? "true" : "false");
                if (!exec_push_string(exec, string_buf)) IR_EXEC_FAIL;
                break;
            case IR_TYPE_BYTE:
                snprintf(string_buf, IR_STRING_BUF_LEN, "%d", left_value.as.byte_val);
                if (!exec_push_string(exec, string_buf)) IR_EXEC_FAIL;
                break;
            case IR_TYPE_STRING:
                exec_push_value(exec, left_value);
                break;
            case IR_TYPE_LIST:
                list = left_value.as.list_val;
                IR_ASSERT(list != NULL);
                if (list->size == 0) {
                    snprintf(string_buf, IR_STRING_BUF_LEN, "[List: Empty]");
                } else {
                    snprintf(string_buf, IR_STRING_BUF_LEN, "[List: %p, %zu/%zu]", list, list->size, list->capacity);
                }
                if (!exec_push_string(exec, string_buf)) IR_EXEC_FAIL;
                break;
            case IR_TYPE_NOTHING:
                if (!exec_push_string(exec, "nothing")) IR_EXEC_FAIL;
                break;
            default:
                exec_set_error(exec, "Invalid type passed to toa");
                IR_EXEC_FAIL;
                break;
            }
            break;
        case IR_TOL:
            left_value = exec_pop_value(exec);
            switch (left_value.type) {
            case IR_TYPE_LIST: exec_push_value(exec, left_value); break;
            default:
                exec_set_error(exec, "Invalid type passed to tol");
                IR_EXEC_FAIL;
                break;
            }
            break;
        case IR_TYPEOF:
            left_value = exec_pop_value(exec);
            switch (left_value.type) {
            case IR_TYPE_NOTHING:
                if (!exec_push_string(exec, "nothing")) IR_EXEC_FAIL;
                break;
            case IR_TYPE_BYTE:
                if (!exec_push_string(exec, "byte")) IR_EXEC_FAIL;
                break;
            case IR_TYPE_INT:
                if (!exec_push_string(exec, "integer")) IR_EXEC_FAIL;
                break;
            case IR_TYPE_FLOAT:
                if (!exec_push_string(exec, "float")) IR_EXEC_FAIL;
                break;
            case IR_TYPE_BOOL:
                if (!exec_push_string(exec, "bool")) IR_EXEC_FAIL;
                break;
            case IR_TYPE_LIST:
                if (!exec_push_string(exec, "list")) IR_EXEC_FAIL;
                break;
            case IR_TYPE_STRING:
                if (!exec_push_string(exec, "str")) IR_EXEC_FAIL;
                break;
            case IR_TYPE_FUNC:
                if (!exec_push_string(exec, "func")) IR_EXEC_FAIL;
                break;
            case IR_TYPE_LABEL:
                if (!exec_push_string(exec, "label")) IR_EXEC_FAIL;
                break;
            default:
                assert(false && "Unhandled ir type in IR_TYPEOF");
                break;
            }
            break;
        case IR_PUSHL:
            list = (IrList*)pool_list.items[CODE_IMMEDIATE].as.list_val;
            if (!list) {
                list = exec_list_new(exec);
                if (!list) IR_EXEC_FAIL;
            }
            exec_push_list(exec, list);
            i += 3;
            break;
        case IR_PUSHA:
            list = (IrList*)pool_list.items[CODE_IMMEDIATE].as.list_val;
            if (!list) {
                list = exec_list_new(exec);
                if (!list) IR_EXEC_FAIL;
            }
            exec_push_list_string(exec, list);
            i += 3;
            break;
        case IR_ADDL: ;
            left_value = exec_pop_value(exec);
            right_value = exec_get_value(exec);
            IR_ASSERT(right_value.type == IR_TYPE_STRING || right_value.type == IR_TYPE_LIST);
            list = right_value.as.list_val;
            IR_ASSERT(list != NULL);

            if (!list->owned) {
                exec_set_error(exec, "Attempt to modify constant list %p", list);
                IR_EXEC_FAIL;
            }

            if (list->size >= list->capacity) {
                if (list->capacity == 0) list->capacity = 4;
                else list->capacity *= 2;
                void* items = exec_realloc(exec, list->items, list->capacity * sizeof(*list->items));
                if (!items) IR_EXEC_FAIL;
                right_value = exec_get_value(exec);
                list = right_value.as.list_val;
                list->items = items;
            }
            list->items[list->size++] = left_value;
            exec_pop_value(exec);
            break;
        case IR_INDEXL:
            left_int = exec_pop_int(exec);

            right_value = exec_pop_value(exec);
            IR_ASSERT(right_value.type == IR_TYPE_STRING || right_value.type == IR_TYPE_LIST);
            list = right_value.as.list_val;
            IR_ASSERT(list != NULL);

            if (left_int < 1 || (size_t)left_int > list->size) {
                exec_set_error(exec, "Out of bounds list access. Tried to index value %ld with list of size %zu", left_int, list->size);
                IR_EXEC_FAIL;
            } else {
                exec_push_value(exec, list->items[left_int - 1]);
            }
            break;
        case IR_SETL:
            left_value = exec_pop_value(exec);
            left_int = exec_pop_int(exec);

            right_value = exec_pop_value(exec);
            IR_ASSERT(right_value.type == IR_TYPE_STRING || right_value.type == IR_TYPE_LIST);
            list = right_value.as.list_val;
            IR_ASSERT(list != NULL);

            if (!list->owned) {
                exec_set_error(exec, "Attemt to modify constant list %p", list);
                IR_EXEC_FAIL;
            }
            if (left_int < 1 || (size_t)left_int > list->size) break;
            list->items[left_int - 1] = left_value;
            break;
        case IR_INSERTL:
            left_value = exec_pop_value(exec);
            left_int = exec_pop_int(exec);

            right_value = exec_get_value(exec);
            IR_ASSERT(right_value.type == IR_TYPE_STRING || right_value.type == IR_TYPE_LIST);
            list = right_value.as.list_val;
            IR_ASSERT(list != NULL);

            if (!list->owned) {
                exec_set_error(exec, "Attemt to modify constant list %p", list);
                IR_EXEC_FAIL;
            }

            if (left_int < 1 || (size_t)left_int > list->size + 1) break;

            if (list->size >= list->capacity) {
                if (list->capacity == 0) list->capacity = 4;
                else list->capacity *= 2;
                void* items = exec_realloc(exec, list->items, list->capacity * sizeof(*list->items));
                if (!items) IR_EXEC_FAIL;
                right_value = exec_get_value(exec);
                list = right_value.as.list_val;
                list->items = items;
            }
            memmove(list->items + left_int, list->items + left_int - 1, (list->size - left_int - 1) * sizeof(IrValue));
            list->size++;
            list->items[left_int] = left_value;
            exec_pop_value(exec);
            break;
        case IR_DELL:
            left_int = exec_pop_int(exec);

            right_value = exec_pop_value(exec);
            IR_ASSERT(right_value.type == IR_TYPE_STRING || right_value.type == IR_TYPE_LIST);
            list = right_value.as.list_val;
            IR_ASSERT(list != NULL);

            if (!list->owned) {
                exec_set_error(exec, "Attemt to modify constant list %p", list);
                IR_EXEC_FAIL;
            }
            if (left_int < 1 || (size_t)left_int > list->size) break;
            memmove(list->items + left_int - 1, list->items + left_int, (list->size - left_int - 2) * sizeof(IrValue));
            list->size--;
            break;
        case IR_LENL:
            right_value = exec_pop_value(exec);
            IR_ASSERT(right_value.type == IR_TYPE_STRING || right_value.type == IR_TYPE_LIST);
            list = right_value.as.list_val;
            IR_ASSERT(list != NULL);

            exec_push_int(exec, list->size);
            break;

        case IR_JMP:
            i = pool_list.items[CODE_IMMEDIATE].as.label_val.pos - 1;
            break;
        case IR_IF:
            left_bool = exec_pop_bool(exec);
            if (left_bool) {
                i = pool_list.items[CODE_IMMEDIATE].as.label_val.pos - 1;
            } else {
                i += 3;
            }
            break;
        case IR_IFNOT:
            left_bool = exec_pop_bool(exec);
            if (!left_bool) {
                i = pool_list.items[CODE_IMMEDIATE].as.label_val.pos - 1;
            } else {
                i += 3;
            }
            break;
        case IR_CALL:
            if (!exec_run_bytecode(exec, bc, pool_list.items[CODE_IMMEDIATE].as.label_val.pos)) IR_EXEC_FAIL;
            i += 3;
            break;
        case IR_RUN: ;
            func = &pool_list.items[CODE_IMMEDIATE].as.func_val;
            if (!func->ptr) {
                if (!exec->resolve_run_function) {
                    exec_set_error(exec, "Called run instruction, but no run function resolver has been attached");
                    IR_EXEC_FAIL;
                }
                func->ptr = exec->resolve_run_function(exec, func->hint);
                if (!func->ptr) {
                    exec_set_error(exec, "Function \"%s\" does not exist at runtime", func->hint);
                    IR_EXEC_FAIL;
                }
            }
            if (!func->ptr(exec)) IR_EXEC_FAIL;
            i += 3;
            break;
        case IR_DYNJMP:
            label_pos = exec_pop_label(exec);
            i = label_pos - 1;
            break;
        case IR_DYNIF:
            label_pos = exec_pop_label(exec);
            left_bool = exec_pop_bool(exec);
            if (left_bool) {
                i = label_pos - 1;
            }
            break;
        case IR_DYNCALL:
            label_pos = exec_pop_label(exec);
            if (!exec_run_bytecode(exec, bc, label_pos)) IR_EXEC_FAIL;
            break;
        case IR_DYNRUN:
            IrRunFunction func = exec_pop_func(exec);
            if (!func) {
                exec_set_error(exec, "Resolving funcs in dynrun instruction is not allowed");
                IR_EXEC_FAIL;
            }
            if (!func(exec)) IR_EXEC_FAIL;
            break;
        case IR_RET:
            goto exec_return;
        case IR_ILLEGAL:
        default:
            exec_set_error(exec, "Illegal op: %d", bc->code.items[i]);
            IR_EXEC_FAIL;
        }
    }

exec_return:
    exec_pop_variable_stack(exec);
    return return_val;
}

void exec_print_value(IrValue* value) {
    IrList* list;
    switch (value->type) {
    case IR_TYPE_NOTHING:
        printf("nothing");
        break;
    case IR_TYPE_BYTE:
        printf("byte '%c' %02x", value->as.byte_val, value->as.byte_val);
        break;
    case IR_TYPE_INT:
        printf("int %ld", value->as.int_val);
        break;
    case IR_TYPE_FLOAT:
        printf("float %g", value->as.float_val);
        break;
    case IR_TYPE_BOOL:
        printf("bool %s", value->as.bool_val ? "true" : "false");
        break;
    case IR_TYPE_LIST: ;
        list = value->as.list_val;
        if (!list) {
            printf("[List: Empty]");
        } else {
            printf("[List: %p (%zu/%zu)]", list->items, list->size, list->capacity);
        }
        break;
    case IR_TYPE_STRING: ;
        list = value->as.list_val;
        if (!list) {
            printf("[String: Empty]");
        } else {
            printf("[String: %p (%zu/%zu)]", list->items, list->size, list->capacity);
        }
        break;
    case IR_TYPE_FUNC:
        printf("func %p", value->as.func_val);
        break;
    case IR_TYPE_LABEL:
        printf("label %zu", value->as.label_val);
        break;
    }
}

void exec_print_stack(IrExec* exec) {
    printf("=== Stack info ===\n");
    if (exec->stack.size == 0) {
        printf("    empty :(\n");
        return;
    }

    for (size_t i = 0; i < exec->stack.size; i++) {
        printf("%zu: ", i);
        exec_print_value(&exec->stack.items[i]);
        printf("\n");
    }
}

void exec_print_variables(IrExec* exec) {
    printf("=== Variable info ===\n");
    if (exec->variables.size == 0) {
        printf("    empty :(\n");
        return;
    }

    for (size_t i = 0; i < exec->variables.size; i++) {
        IrValueList list = exec->variables.items[i];
        for (size_t j = 0; j < list.size; j++) {
            printf("%zu: ", j);
            exec_print_value(&list.items[j]);
            printf("\n");
        }
        printf("\n");
    }
}

IrMemArena* ir_arena_new(size_t reserve_size, size_t commit_size) {
    unsigned long pagesize = ir_plat_get_pagesize();

    reserve_size = IR_ALIGN_UP_POW2(reserve_size, pagesize);
    commit_size = IR_ALIGN_UP_POW2(commit_size, pagesize);

    IrMemArena* arena = ir_plat_mem_reserve(reserve_size);

    if (!ir_plat_mem_commit(arena, commit_size)) return NULL;

    arena->reserve_size = reserve_size;
    arena->commit_size = commit_size;
    arena->pos = IR_ARENA_BASE_POS;
    arena->commit_pos = commit_size;

    return arena;
}

void ir_arena_free(IrMemArena* arena) {
    ir_plat_mem_release(arena, arena->reserve_size);
}

char* ir_arena_sprintf(IrMemArena* arena, size_t max_size, const char* fmt, ...) {
    char* str = ir_arena_alloc(arena, max_size);

    va_list va;
    va_start(va, fmt);
    int size = vsnprintf(str, max_size, fmt, va);
    va_end(va);

    ir_arena_pop(arena, max_size - size - 1);
    return str;
}

void* ir_arena_alloc_packed(IrMemArena* arena, size_t size) {
    size_t old_pos = arena->pos;
    size_t new_pos = old_pos + size;

    if (new_pos > arena->reserve_size) { return NULL; }

    if (new_pos > arena->commit_pos) {
        size_t new_commit_pos = new_pos;
        new_commit_pos += arena->commit_size - 1;
        new_commit_pos -= new_commit_pos % arena->commit_size;
        new_commit_pos = MIN(new_commit_pos, arena->reserve_size);

        unsigned char* mem = (unsigned char*)arena + arena->commit_pos;
        size_t commit_size = new_commit_pos - arena->commit_pos;

        if (!ir_plat_mem_commit(mem, commit_size)) return NULL;

        arena->commit_pos = new_commit_pos;
    }

    arena->pos = new_pos;

    unsigned char* out = (unsigned char*)arena + old_pos;

    return out;
}

void* ir_arena_alloc(IrMemArena* arena, size_t size) {
    size_t pos_aligned = IR_ALIGN_UP_POW2(arena->pos, IR_ARENA_ALIGN);
    size_t new_pos = pos_aligned + size;

    if (new_pos > arena->reserve_size) { return NULL; }

    if (new_pos > arena->commit_pos) {
        size_t new_commit_pos = new_pos;
        new_commit_pos += arena->commit_size - 1;
        new_commit_pos -= new_commit_pos % arena->commit_size;
        new_commit_pos = MIN(new_commit_pos, arena->reserve_size);

        unsigned char* mem = (unsigned char*)arena + arena->commit_pos;
        size_t commit_size = new_commit_pos - arena->commit_pos;

        if (!ir_plat_mem_commit(mem, commit_size)) return NULL;

        arena->commit_pos = new_commit_pos;
    }

    arena->pos = new_pos;

    unsigned char* out = (unsigned char*)arena + pos_aligned;

    return out;
}

void* ir_arena_realloc(IrMemArena* arena, void* ptr, size_t old_size, size_t new_size) {
    if (!ptr || old_size == 0) return ir_arena_alloc(arena, new_size);

    void* ret = ir_arena_alloc(arena, new_size);
    memcpy(ret, ptr, old_size);
    return ret;
}

void ir_arena_pop(IrMemArena* arena, size_t size) {
    size = MIN(size, arena->pos - IR_ARENA_BASE_POS);
    arena->pos -= size;
}

void ir_arena_pop_to(IrMemArena* arena, size_t pos) {
    size_t size = pos < arena->pos ? arena->pos - pos : 0;
    ir_arena_pop(arena, size);
}

void ir_arena_clear(IrMemArena* arena) {
    ir_arena_pop_to(arena, IR_ARENA_BASE_POS);
}

#ifdef _WIN32

#include <windows.h>

unsigned long ir_plat_get_pagesize(void) {
    SYSTEM_INFO sysinfo = { 0 };
    GetSystemInfo(&sysinfo);

    return sysinfo.dwPageSize;
}

void* ir_plat_mem_reserve(size_t size) {
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
}

bool ir_plat_mem_commit(void* ptr, size_t size) {
    void* ret = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    return ret != NULL;
}

bool ir_plat_mem_decommit(void* ptr, size_t size) {
    return VirtualFree(ptr, size, MEM_DECOMMIT);
}

bool ir_plat_mem_release(void* ptr, size_t size) {
    return VirtualFree(ptr, size, MEM_RELEASE);
}

#else

#include <unistd.h>
#include <sys/mman.h>

unsigned long ir_plat_get_pagesize(void) {
    return sysconf(_SC_PAGESIZE);
}

void* ir_plat_mem_reserve(size_t size) {
    void* out = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (out == MAP_FAILED) return NULL;
    return out;
}

bool ir_plat_mem_commit(void* ptr, size_t size) {
    return !mprotect(ptr, size, PROT_READ | PROT_WRITE);
}

bool ir_plat_mem_decommit(void* ptr, size_t size) {
    if (mprotect(ptr, size, PROT_NONE) == -1) return false;
    return !madvise(ptr, size, MADV_DONTNEED);
}

bool ir_plat_mem_release(void* ptr, size_t size) {
    return !munmap(ptr, size);
}

#endif // _WIN32


#endif // SCRAP_IR_IMPLEMENTATION
