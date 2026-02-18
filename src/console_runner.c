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

#define SCRAP_IR_IMPLEMENTATION
#include "scrap_ir.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

bool vm_debug(IrExec* exec) {
    exec_print_stack(exec);
    exec_print_variables(exec);
    return true;
}

bool vm_collect(IrExec* exec) {
    exec_collect(exec);
    return true;
}

bool square(IrExec* exec) {
    int64_t val = exec_pop_int(exec);
    exec_push_int(exec, val * val);
    return true;
}

void print_val(IrValue* val) {
    switch (val->type) {
    case IR_TYPE_NOTHING:
        printf("nothing");
        break;
    case IR_TYPE_BYTE:
        printf("0x%02x", val->as.byte_val);
        break;
    case IR_TYPE_INT:
        printf("%ld", val->as.int_val);
        break;
    case IR_TYPE_FLOAT:
        printf("%gf", val->as.float_val);
        break;
    case IR_TYPE_BOOL:
        printf("%s", val->as.bool_val ? "true" : "false");
        break;
    case IR_TYPE_LIST:
        printf("list = [");
        for (size_t i = 0; i < val->as.list_val->size; i++) {
            print_val(&val->as.list_val->items[i]);
            if (i + 1 < val->as.list_val->size) printf(", ");
        }
        printf("]");
        break;
    case IR_TYPE_FUNC:
        if (val->as.func_val.ptr) {
            if (val->as.func_val.hint) {
                printf("func(\"%s\" %p)", val->as.func_val.hint, val->as.func_val.ptr);
            } else {
                printf("func(%p)", val->as.func_val.ptr);
            }
        } else {
            printf("func(\"%s\")", val->as.func_val.hint);
        }
        break;
    case IR_TYPE_LABEL:
        printf("label(\"%s\")", val->as.label_val.name);
        break;
    }
}

bool print_value(IrExec* exec) {
    IrValue val = exec_pop_value(exec);
    print_val(&val);
    printf("\n");
    return true;
}

bool print_str(IrExec* exec) {
    IrList* list = exec_pop_list(exec);
    if (!list) return false;
    printf("\"");
    for (size_t i = 0; i < list->size; i++) {
        IrValue c = list->items[i];
        switch (c.type) {
        case IR_TYPE_INT: printf("%lc", (wint_t)c.as.int_val); break;
        case IR_TYPE_BYTE: printf("%c", c.as.byte_val); break;
        default: printf("?"); break;
        }
    }
    printf("\"\n");
    return true;
}

IrRunFunction resolve_function(IrExec* exec, const char* hint) {
    (void) exec;
    // printf("Resolve: %s\n", hint);
    if (!strcmp(hint, "square")) {
        return square;
    } else if (!strcmp(hint, "print_value")) {
        return print_value;
    } else if (!strcmp(hint, "print_str")) {
        return print_str;
    } else if (!strcmp(hint, "debug")) {
        return vm_debug;
    } else if (!strcmp(hint, "collect")) {
        return vm_collect;
    }
    return NULL;
}

int main(void) {
    IrBytecode bc = bytecode_new("main");

    bytecode_push_label(&bc, "entry");

    bytecode_push_op(&bc, IR_PUSHL);
    IrLabelID loop = bytecode_push_label(&bc, "loop");

    bytecode_push_op(&bc, IR_DUP);
    bytecode_push_op(&bc, IR_DUP);
    bytecode_push_op(&bc, IR_LENL);
    bytecode_push_op(&bc, IR_ADDL);

    bytecode_push_op(&bc, IR_DUP);
    bytecode_push_op(&bc, IR_LENL);
    bytecode_push_op_int(&bc, IR_PUSHI, 10);
    bytecode_push_op(&bc, IR_LESSI);
    bytecode_push_op_label(&bc, IR_IF, loop);

    bytecode_push_op(&bc, IR_DUP);
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("print_value"));
    bytecode_push_op_func(&bc, IR_RUN, ir_func_by_hint("debug"));

    bytecode_push_op(&bc, IR_RET);

    bytecode_print(&bc);

    IrExec exec = exec_new(1024 * 1024); // 1 MB
    if (exec.last_error[0] != 0) {
        printf("Exec create error: %s\n", exec.last_error);
        exec_free(&exec);
        return 1;
    }
    exec_set_run_function_resolver(&exec, resolve_function);
    exec_add_bytecode(&exec, bc);

    if (!exec_run(&exec, "main", "entry")) 
        printf("Runtime error: %s\n", exec.last_error);

    exec_free(&exec);
    return 0;
}
