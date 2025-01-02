#include "scrap.h"
#include "raylib.h"
#include "blocks.h"

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
