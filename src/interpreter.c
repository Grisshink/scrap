// Scrap is a project that allows anyone to build software using simple, block based interface.
// This file contains all code for interpreter.
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
#include "ast.h"
#include "vec.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void arg_stack_push_arg(Exec* exec, Data data);
void arg_stack_undo_args(Exec* exec, size_t count);
void variable_stack_pop_layer(Exec* exec);
void variable_stack_cleanup(Exec* exec);
void chain_stack_push(Exec* exec, ChainStackData data);
void chain_stack_pop(Exec* exec);

Data data_copy(Data arg) {
    if (arg.storage.type == DATA_STORAGE_STATIC) return arg;

    Data out;
    out.type = arg.type;
    out.storage.type = DATA_STORAGE_MANAGED;
    out.storage.storage_len = arg.storage.storage_len;
    out.data.custom_arg = malloc(arg.storage.storage_len);
    if (arg.type == DATA_LIST) {
        out.data.list_arg.len = arg.data.list_arg.len;
        for (size_t i = 0; i < arg.data.list_arg.len; i++) {
            out.data.list_arg.items[i] = data_copy(arg.data.list_arg.items[i]);
        }
    } else {
        memcpy((void*)out.data.custom_arg, arg.data.custom_arg, arg.storage.storage_len);
    }
    return out;
}

void data_free(Data arg) {
    if (arg.storage.type == DATA_STORAGE_STATIC) return;
    switch (arg.type) {
    case DATA_LIST:
        if (!arg.data.list_arg.items) break;
        for (size_t i = 0; i < arg.data.list_arg.len; i++) {
            data_free(arg.data.list_arg.items[i]);
        }
        free((Data*)arg.data.list_arg.items);
        break;
    default:
        if (!arg.data.custom_arg) break;
        free((void*)arg.data.custom_arg);
        break;
    }
}

int data_to_int(Data arg) {
    switch (arg.type) {
    case DATA_BOOL:
    case DATA_INT:
        return arg.data.int_arg;
    case DATA_DOUBLE:
        return (int)arg.data.double_arg;
    case DATA_STR:
        return atoi(arg.data.str_arg);
    default:
        return 0;
    }
}

double data_to_double(Data arg) {
    switch (arg.type) {
    case DATA_BOOL:
    case DATA_INT:
        return (double)arg.data.int_arg;
    case DATA_DOUBLE:
        return arg.data.double_arg;
    case DATA_STR:
        return atof(arg.data.str_arg);
    default:
        return 0.0;
    }
}

int data_to_bool(Data arg) {
    switch (arg.type) {
    case DATA_BOOL:
    case DATA_INT:
        return arg.data.int_arg != 0;
    case DATA_DOUBLE:
        return arg.data.double_arg != 0.0;
    case DATA_STR:
        return *arg.data.str_arg != 0;
    case DATA_LIST:
        return arg.data.list_arg.len != 0;
    default:
        return 0;
    }
}

const char* data_to_str(Data arg) {
    static char buf[32];

    switch (arg.type) {
    case DATA_STR:
        return arg.data.str_arg;
    case DATA_BOOL:
        return arg.data.int_arg ? "true" : "false";
    case DATA_DOUBLE:
        buf[0] = 0;
        snprintf(buf, 32, "%f", arg.data.double_arg);
        return buf;
    case DATA_INT:
        buf[0] = 0;
        snprintf(buf, 32, "%d", arg.data.int_arg);
        return buf;
    case DATA_LIST:
        return "# LIST #";
    default:
        return "";
    }
}

Exec exec_new(void) {
    Exec exec = (Exec) {
        .code = NULL,
        .arg_stack_len = 0,
        .control_stack_len = 0,
        .thread = (pthread_t) {0},
        .running_state = EXEC_STATE_NOT_RUNNING,
        .current_error_block = NULL,
    };
    exec.current_error[0] = 0;
    return exec;
}

void exec_free(Exec* exec) {
    (void) exec;
}

void exec_copy_code(Vm* vm, Exec* exec, BlockChain* code) {
    if (vm->is_running) return;
    exec->code = code;
}

bool exec_block(Exec* exec, Block block, Data* block_return, bool from_end, bool omit_args, Data control_arg) {
    BlockFunc execute_block = block.blockdef->func;
    if (!execute_block) return false;

    int stack_begin = exec->arg_stack_len;

    if (block.blockdef->arg_id != -1) {
        arg_stack_push_arg(exec, (Data) {
            .type = DATA_INT,
            .storage = (DataStorage) {
                .type = DATA_STORAGE_STATIC,
                .storage_len = 0,
            },
            .data = (DataContents) {
                .int_arg = block.blockdef->arg_id,
            },
        });
    }

    if (block.blockdef->chain) {
        arg_stack_push_arg(exec, (Data) {
            .type = DATA_CHAIN,
            .storage = (DataStorage) {
                .type = DATA_STORAGE_STATIC,
                .storage_len = 0,
            },
            .data = (DataContents) {
                .chain_arg = block.blockdef->chain,
            },
        });
    }

    if (block.blockdef->type == BLOCKTYPE_CONTROL || block.blockdef->type == BLOCKTYPE_CONTROLEND) {
        arg_stack_push_arg(exec, (Data) {
            .type = DATA_CONTROL,
            .storage = (DataStorage) {
                .type = DATA_STORAGE_STATIC,
                .storage_len = 0,
            },
            .data = (DataContents) {
                .control_arg = from_end ? CONTROL_ARG_END : CONTROL_ARG_BEGIN,
            },
        });
        if (!from_end && block.blockdef->type == BLOCKTYPE_CONTROLEND) {
            arg_stack_push_arg(exec, control_arg);
        }
    }
    if (!omit_args) {
        for (vec_size_t i = 0; i < vector_size(block.arguments); i++) {
            Argument block_arg = block.arguments[i];
            switch (block_arg.type) {
            case ARGUMENT_TEXT:
            case ARGUMENT_CONST_STRING:
                arg_stack_push_arg(exec, (Data) {
                    .type = DATA_STR,
                    .storage = (DataStorage) {
                        .type = DATA_STORAGE_STATIC,
                        .storage_len = 0,
                    },
                    .data = (DataContents) {
                        .str_arg = block_arg.data.text,
                    },
                });
                break;
            case ARGUMENT_BLOCK: ; // This fixes gcc-9 error
                Data arg_return;
                if (!exec_block(exec, block_arg.data.block, &arg_return, false, false, (Data) {0})) return false;
                arg_stack_push_arg(exec, arg_return);
                break;
            case ARGUMENT_BLOCKDEF:
                break;
            default:
                return false;
            }
        }
    }

    *block_return = execute_block(exec, exec->arg_stack_len - stack_begin, exec->arg_stack + stack_begin);
    arg_stack_undo_args(exec, exec->arg_stack_len - stack_begin);

    return true;
}

#define BLOCKDEF chain->blocks[i].blockdef
bool exec_run_chain(Exec* exec, BlockChain* chain, int argc, Data* argv, Data* return_val) {
    int skip_layer = -1;
    size_t base_len = exec->control_stack_len;
    chain_stack_push(exec, (ChainStackData) {
        .skip_block = false,
        .layer = 0,
        .running_ind = 0,
        .custom_argc = argc,
        .custom_argv = argv,
        .is_returning = false,
        .return_arg = (Data) {0},
    });
    exec->running_chain = chain;
    Data block_return;
    for (size_t i = 0; i < vector_size(chain->blocks); i++) {
        pthread_testcancel();
        size_t block_ind = i;
        ChainStackData* chain_data = &exec->chain_stack[exec->chain_stack_len - 1];
        chain_data->running_ind = i;
        bool from_end = false;
        bool omit_args = false;
        bool return_used = false;
        if (chain_data->is_returning) {
            break;
        }

        if (BLOCKDEF->type == BLOCKTYPE_END || BLOCKDEF->type == BLOCKTYPE_CONTROLEND) {
            if (BLOCKDEF->type == BLOCKTYPE_CONTROLEND && chain_data->layer == 0) continue;
            variable_stack_pop_layer(exec);
            chain_data->layer--;
            control_stack_pop_data(block_ind, size_t)
            control_stack_pop_data(block_return, Data)
            if (block_return.type == DATA_OMIT_ARGS) omit_args = true;
            if (block_return.storage.type == DATA_STORAGE_MANAGED) data_free(block_return);
            from_end = true;
            if (chain_data->skip_block && skip_layer == chain_data->layer) {
                chain_data->skip_block = false;
                skip_layer = -1;
            }
        }
        if (!chain_data->skip_block) {
            if (!exec_block(exec, chain->blocks[block_ind], &block_return, from_end, omit_args, (Data){0})) {
                chain_stack_pop(exec);
                return false;
            }
            exec->running_chain = chain;
            if (chain_data->running_ind != i) i = chain_data->running_ind;
        }
        if (BLOCKDEF->type == BLOCKTYPE_CONTROLEND && block_ind != i) {
            from_end = false;
            if (!exec_block(exec, chain->blocks[i], &block_return, from_end, false, block_return)) {
                chain_stack_pop(exec);
                return false;
            }
            if (chain_data->running_ind != i) i = chain_data->running_ind;
            return_used = true;
        }
        if (BLOCKDEF->type == BLOCKTYPE_CONTROL || BLOCKDEF->type == BLOCKTYPE_CONTROLEND) {
            control_stack_push_data(block_return, Data)
            control_stack_push_data(i, size_t)
            if (chain_data->skip_block && skip_layer == -1) skip_layer = chain_data->layer;
            return_used = true;
            chain_data->layer++;
        }
        if (!return_used && block_return.storage.type == DATA_STORAGE_MANAGED) {
            data_free(block_return);
        }
    }
    *return_val = exec->chain_stack[exec->chain_stack_len - 1].return_arg;
    while (exec->chain_stack[exec->chain_stack_len - 1].layer >= 0) {
        variable_stack_pop_layer(exec);
        exec->chain_stack[exec->chain_stack_len - 1].layer--;
    }
    exec->control_stack_len = base_len;
    chain_stack_pop(exec);
    return true;
}
#undef BLOCKDEF

void exec_thread_exit(void* thread_exec) {
    Exec* exec = thread_exec;
    variable_stack_cleanup(exec);
    arg_stack_undo_args(exec, exec->arg_stack_len);
    exec->running_state = EXEC_STATE_DONE;
}

void* exec_thread_entry(void* thread_exec) {
    Exec* exec = thread_exec;
    pthread_cleanup_push(exec_thread_exit, thread_exec);

    exec->running_state = EXEC_STATE_RUNNING;
    exec->arg_stack_len = 0;
    exec->control_stack_len = 0;
    exec->chain_stack_len = 0;
    exec->running_chain = NULL;

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        Block* block = &exec->code[i].blocks[0];
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;
        for (size_t j = 0; j < vector_size(block->arguments); j++) {
            if (block->arguments[j].type != ARGUMENT_BLOCKDEF) continue;
            block->arguments[j].data.blockdef->chain = &exec->code[i];
        }
    }

    for (size_t i = 0; i < vector_size(exec->code); i++) {
        Block* block = &exec->code[i].blocks[0];
        if (block->blockdef->type != BLOCKTYPE_HAT) continue;
        bool cont = false;
        for (size_t j = 0; j < vector_size(block->arguments); j++) {
            if (block->arguments[j].type == ARGUMENT_BLOCKDEF) {
                cont = true;
                break;
            }
        }
        if (cont) continue;
        Data bin;
        if (!exec_run_chain(exec, &exec->code[i], -1, NULL, &bin)) {
            exec->running_chain = NULL;
            pthread_exit((void*)0);
        }
        exec->running_chain = NULL;
    }

    pthread_cleanup_pop(1);
    pthread_exit((void*)1);
}

bool exec_start(Vm* vm, Exec* exec) {
    if (vm->is_running) return false;
    if (exec->running_state != EXEC_STATE_NOT_RUNNING) return false;
    vm->is_running = true;

    exec->running_state = EXEC_STATE_STARTING;
    if (pthread_create(&exec->thread, NULL, exec_thread_entry, exec)) {
        exec->running_state = EXEC_STATE_NOT_RUNNING;
        return false;
    }

    return true;
}

bool exec_stop(Vm* vm, Exec* exec) {
    if (!vm->is_running) return false;
    if (exec->running_state != EXEC_STATE_RUNNING) return false;
    if (pthread_cancel(exec->thread)) return false;
    return true;
}

bool exec_join(Vm* vm, Exec* exec, size_t* return_code) {
    if (!vm->is_running) return false;
    if (exec->running_state == EXEC_STATE_NOT_RUNNING) return false;

    void* return_val;
    if (pthread_join(exec->thread, &return_val)) return false;
    vm->is_running = false;
    exec->running_state = EXEC_STATE_NOT_RUNNING;
    *return_code = (size_t)return_val;
    return true;
}

void exec_set_skip_block(Exec* exec) {
    exec->chain_stack[exec->chain_stack_len - 1].skip_block = true;
}

bool exec_try_join(Vm* vm, Exec* exec, size_t* return_code) {
    if (!vm->is_running) return false;
    if (exec->running_state != EXEC_STATE_DONE) return false;

    void* return_val;
    if (pthread_join(exec->thread, &return_val)) return false;
    vm->is_running = false;
    exec->running_state = EXEC_STATE_NOT_RUNNING;
    *return_code = (size_t)return_val;
    return true;
}

bool variable_stack_push_var(Exec* exec, const char* name, Data arg) {
    if (exec->variable_stack_len >= VM_VARIABLE_STACK_SIZE) return false;
    if (*name == 0) return false;
    Variable var;
    var.name = name;
    var.value = arg;
    var.chain_layer = exec->chain_stack_len - 1;
    var.layer = exec->chain_stack[var.chain_layer].layer;
    exec->variable_stack[exec->variable_stack_len++] = var;
    return true;
}

void variable_stack_pop_layer(Exec* exec) {
    size_t count = 0;
    for (int i = exec->variable_stack_len - 1; i >= 0 &&
                                               exec->variable_stack[i].layer == exec->chain_stack[exec->chain_stack_len - 1].layer &&
                                               exec->variable_stack[i].chain_layer == exec->chain_stack_len - 1; i--) {
        Data arg = exec->variable_stack[i].value;
        if (arg.storage.type == DATA_STORAGE_UNMANAGED || arg.storage.type == DATA_STORAGE_MANAGED) {
            data_free(arg);
        }
        count++;
    }
    exec->variable_stack_len -= count;
}

void variable_stack_cleanup(Exec* exec) {
    for (size_t i = 0; i < exec->variable_stack_len; i++) {
        Data arg = exec->variable_stack[i].value;
        if (arg.storage.type == DATA_STORAGE_UNMANAGED || arg.storage.type == DATA_STORAGE_MANAGED) {
            data_free(arg);
        }
    }
    exec->variable_stack_len = 0;
}

Variable* variable_stack_get_variable(Exec* exec, const char* name) {
    for (int i = exec->variable_stack_len - 1; i >= 0; i--) {
        if (!strcmp(exec->variable_stack[i].name, name)) return &exec->variable_stack[i];
    }
    return NULL;
}

void chain_stack_push(Exec* exec, ChainStackData data) {
    if (exec->chain_stack_len >= VM_CHAIN_STACK_SIZE) {
        TraceLog(LOG_ERROR, "[VM] Chain stack overflow");
        PTHREAD_FAIL(exec);
    }
    exec->chain_stack[exec->chain_stack_len++] = data;
}

void chain_stack_pop(Exec* exec) {
    if (exec->chain_stack_len == 0) {
        TraceLog(LOG_ERROR, "[VM] Chain stack underflow");
        PTHREAD_FAIL(exec);
    }
    exec->chain_stack_len--;
}

void arg_stack_push_arg(Exec* exec, Data arg) {
    if (exec->arg_stack_len >= VM_ARG_STACK_SIZE) {
        TraceLog(LOG_ERROR, "[VM] Arg stack overflow");
        PTHREAD_FAIL(exec);
    }
    exec->arg_stack[exec->arg_stack_len++] = arg;
}

void arg_stack_undo_args(Exec* exec, size_t count) {
    if (count > exec->arg_stack_len) {
        TraceLog(LOG_ERROR, "[VM] Arg stack underflow");
        PTHREAD_FAIL(exec);
    }
    for (size_t i = 0; i < count; i++) {
        Data arg = exec->arg_stack[exec->arg_stack_len - 1 - i];
        if (arg.storage.type != DATA_STORAGE_MANAGED) continue;
        data_free(arg);
    }
    exec->arg_stack_len -= count;
}
