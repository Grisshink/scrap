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
#include "raylib.h"
#include "blocks.h"

Timer start_timer(const char* name) {
    Timer timer;
    timer.name = name;
    clock_gettime(CLOCK_MONOTONIC, &timer.start);
    return timer;
}

double end_timer(Timer timer) {
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
 
    double time_taken = (end.tv_sec - timer.start.tv_sec) * 1e+6 + (end.tv_nsec - timer.start.tv_nsec) * 1e-3;
    return time_taken;
}

ScrVec as_scr_vec(Vector2 vec) {
    return (ScrVec) { vec.x, vec.y };
}

Vector2 as_rl_vec(ScrVec vec) {
    return (Vector2) { vec.x, vec.y };
}

Color as_rl_color(ScrColor color) {
    return (Color) { color.r, color.g, color.b, color.a };
}

int leading_ones(unsigned char byte) {
    int out = 0;
    while (byte & 0x80) {
        out++;
        byte <<= 1;
    }
    return out;
}

const char* into_data_path(const char* path) {
    return TextFormat("%s%s", GetApplicationDirectory(), path);
}

ScrBlock block_new_ms(ScrBlockdef* blockdef) {
    ScrBlock block = block_new(blockdef);
    for (size_t i = 0; i < vector_size(block.arguments); i++) {
        if (block.arguments[i].type != ARGUMENT_BLOCKDEF) continue;
        block.arguments[i].data.blockdef->func = block_exec_custom;
    }
    update_measurements(&block, PLACEMENT_HORIZONTAL);
    return block;
}
