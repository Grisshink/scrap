
#define DROP_TEX_WIDTH ((float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)drop_tex.height * (float)drop_tex.width)
#define FONT_PATH_MAX_SIZE 256
#define FONT_SYMBOLS_MAX_SIZE 1024
#define ACTION_BAR_MAX_SIZE 128

#define BLOCK_OUTLINE_SIZE (2.0 * (float)conf.font_size / 32.0)
#define BLOCK_TEXT_SIZE (conf.font_size * 0.6)
#define BLOCK_IMAGE_SIZE (conf.font_size - BLOCK_OUTLINE_SIZE * 4)
#define BLOCK_PADDING (5.0 * (float)conf.font_size / 32.0)
#define BLOCK_STRING_PADDING (10.0 * (float)conf.font_size / 32.0)
#define BLOCK_CONTROL_INDENT (16.0 * (float)conf.font_size / 32.0)
#define SIDE_BAR_PADDING (10.0 * (float)conf.font_size / 32.0)

#define DATA_PATH "data/"
#define CONFIG_PATH "config.txt"

#define LICENSE_URL "https://www.gnu.org/licenses/gpl-3.0.html"
