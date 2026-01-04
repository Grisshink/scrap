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

#define EDITOR_DEFAULT_PROJECT_NAME "project.scrp"

#define DROP_TEX_WIDTH ((float)(conf.ui_size - BLOCK_OUTLINE_SIZE * 4) / (float)drop_tex.height * (float)drop_tex.width)
#define FONT_PATH_MAX_SIZE 256
#define FONT_SYMBOLS_MAX_SIZE 1024
#define ACTION_BAR_MAX_SIZE 128

#define SHADOW_DISTANCE floorf(1.66 * (float)conf.ui_size / 32.0)
#define BLOCK_OUTLINE_SIZE (2.0 * (float)conf.ui_size / 32.0)
#define BLOCK_TEXT_SIZE floorf((float)conf.ui_size * 0.6)
#define BLOCK_IMAGE_SIZE (conf.ui_size - BLOCK_OUTLINE_SIZE * 4)
#define BLOCK_PADDING (5.0 * (float)conf.ui_size / 32.0)
#define BLOCK_STRING_PADDING (10.0 * (float)conf.ui_size / 32.0)
#define BLOCK_CONTROL_INDENT (16.0 * (float)conf.ui_size / 32.0)
#define SIDE_BAR_PADDING (10.0 * (float)conf.ui_size / 32.0)
#define BLOCK_GHOST_OPACITY 0x99
#define BLOCK_ARG_OPACITY 0xdd

#define PANEL_BACKGROUND_COLOR { 0x10, 0x10, 0x10, 0xff }

#define TEXT_SELECTION_COLOR { 0x00, 0x60, 0xff, 0x80 }

#define WINDOW_ELEMENT_PADDING (10.0 * (float)conf.ui_size / 32.0)

#define DATA_PATH "data/"
#define LOCALE_PATH "locale/"
#define CONFIG_PATH "config.txt"
#define CONFIG_FOLDER_NAME "scrap"

#define LICENSE_URL "https://github.com/Grisshink/scrap/blob/main/LICENSE"

#define CODEPOINT_REGION_COUNT 3

#define DEBUG_BUFFER_LINES 32
#define DEBUG_BUFFER_LINE_SIZE 256

#define CATEGORY_CONTROL_COLOR { 0xff, 0x99, 0x00, 0xff }
#define CATEGORY_TERMINAL_COLOR { 0x00, 0xaa, 0x44, 0xff }
#define CATEGORY_MATH_COLOR { 0x00, 0xcc, 0x77, 0xff }
#define CATEGORY_LOGIC_COLOR { 0x77, 0xcc, 0x44, 0xff }
#define CATEGORY_STRING_COLOR { 0xff, 0x00, 0x99, 0xff }
#define CATEGORY_MISC_COLOR { 0x00, 0x99, 0xff, 0xff }
#define CATEGORY_DATA_COLOR { 0xff, 0x77, 0x00, 0xff }

#define UNIMPLEMENTED_BLOCK_COLOR { 0x66, 0x66, 0x66, 0xff }

#define MAX_ERROR_LEN 512

#define MIN_MEMORY_LIMIT 4194304 // 4 MB
#define MAX_MEMORY_LIMIT 4294967296 // 4 GB
