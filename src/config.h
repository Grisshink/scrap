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

#define DROP_TEX_WIDTH ((float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)drop_tex.height * (float)drop_tex.width)
#define FONT_PATH_MAX_SIZE 256
#define FONT_SYMBOLS_MAX_SIZE 1024
#define ACTION_BAR_MAX_SIZE 128

#define SHADOW_DISTANCE floorf(1.66 * (float)conf.font_size / 32.0)
#define BLOCK_OUTLINE_SIZE (2.0 * (float)conf.font_size / 32.0)
#define BLOCK_TEXT_SIZE floorf((float)conf.font_size * 0.6)
#define BLOCK_IMAGE_SIZE (conf.font_size - BLOCK_OUTLINE_SIZE * 4)
#define BLOCK_PADDING (5.0 * (float)conf.font_size / 32.0)
#define BLOCK_STRING_PADDING (10.0 * (float)conf.font_size / 32.0)
#define BLOCK_CONTROL_INDENT (16.0 * (float)conf.font_size / 32.0)
#define SIDE_BAR_PADDING (10.0 * (float)conf.font_size / 32.0)

#define WINDOW_ELEMENT_PADDING (10.0 * (float)conf.font_size / 32.0)

#define DATA_PATH "data/"
#define CONFIG_PATH "config.txt"
#define CONFIG_FOLDER_NAME "scrap"

#define LICENSE_URL "https://www.gnu.org/licenses/gpl-3.0.html"

#define CODEPOINT_REGION_COUNT 2

#define DEBUG_BUFFER_LINES 32
#define DEBUG_BUFFER_LINE_SIZE 256

#define CATEGORY_CONTROL_COLOR { 0xff, 0x99, 0x00, 0xff }
#define CATEGORY_TERMINAL_COLOR { 0x00, 0xaa, 0x44, 0xff }
#define CATEGORY_MATH_COLOR { 0x00, 0xcc, 0x77, 0xff }
#define CATEGORY_LOGIC_COLOR { 0x77, 0xcc, 0x44, 0xff }
#define CATEGORY_STRING_COLOR { 0xff, 0x00, 0x99, 0xff }
#define CATEGORY_MISC_COLOR { 0x00, 0x99, 0xff, 0xff }
#define CATEGORY_DATA_COLOR { 0xff, 0x77, 0x00, 0xff }
