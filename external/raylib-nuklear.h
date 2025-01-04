/**********************************************************************************************
*
*   raylib-nuklear v5.5.0 - Nuklear GUI for Raylib.
*
*   NOTE:
*       - This version of library was modified by @Grisshink to fit particular needs, it may potentially
*         be broken or contain bugs if used out of scrap.c.
*
*   FEATURES:
*       - Use the Nuklear immediate-mode graphical user interface in raylib.
*
*   DEPENDENCIES:
*       - raylib 5.5+ https://www.raylib.com/
*       - Nuklear https://github.com/Immediate-Mode-UI/Nuklear
*
*   LICENSE: zlib/libpng
*
*   raylib-nuklear is licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software:
*
*   Copyright (c) 2020 Rob Loach (@RobLoach)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#ifndef RAYLIB_NUKLEAR_H
#define RAYLIB_NUKLEAR_H

#include "raylib.h"

// Nuklear defines
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_COMMAND_USERDATA
#define NK_KEYSTATE_BASED_INPUT

#ifndef NK_ASSERT
#ifdef NDEBUG
#define NK_ASSERT(condition) ((void)0)
#else
#define NK_ASSERT(condition) do { if (!(condition)) { TraceLog(LOG_ERROR, "NUKLEAR: Failed assert \"%s\" (%s:%i)", #condition, "nuklear.h", __LINE__); }} while (0)
#endif  // NDEBUG
#endif  // NK_ASSERT

#include "nuklear.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_API struct nk_context* InitNuklear(int fontSize, Shader* rect_shader, float* shader_time);                // Initialize the Nuklear GUI context using raylib's font
NK_API struct nk_context* InitNuklearEx(struct nk_user_font* userFont, Shader* rect_shader, float* shader_time); // Initialize the Nuklear GUI context, with a custom font
NK_API struct nk_user_font* LoadFontIntoNuklear(Font font, float fontSize);
NK_API Font LoadFontFromNuklear(int fontSize);                      // Loads the default Nuklear font
NK_API void UpdateNuklear(struct nk_context * ctx);                 // Update the input state and internal components for Nuklear
NK_API void UpdateNuklearEx(struct nk_context * ctx, float deltaTime); // Update the input state and internal components for Nuklear, with a custom frame time
NK_API void DrawNuklear(struct nk_context * ctx);                   // Render the Nuklear GUI on the screen
NK_API void UnloadNuklear(struct nk_context * ctx);                 // Deinitialize the Nuklear context
NK_API struct nk_color ColorToNuklear(Color color);                 // Convert a raylib Color to a Nuklear color object
NK_API struct nk_colorf ColorToNuklearF(Color color);               // Convert a raylib Color to a Nuklear floating color
NK_API struct Color ColorFromNuklear(struct nk_color color);        // Convert a Nuklear color to a raylib Color
NK_API struct Color ColorFromNuklearF(struct nk_colorf color);      // Convert a Nuklear floating color to a raylib Color
NK_API struct Rectangle RectangleFromNuklear(struct nk_context * ctx, struct nk_rect rect); // Convert a Nuklear rectangle to a raylib Rectangle
NK_API struct nk_rect RectangleToNuklear(struct nk_context * ctx, Rectangle rect); // Convert a raylib Rectangle to a Nuklear Rectangle
NK_API struct nk_image TextureToNuklear(Texture tex);               // Convert a raylib Texture to A Nuklear image
NK_API struct Texture TextureFromNuklear(struct nk_image img);      // Convert a Nuklear image to a raylib Texture
NK_API struct nk_image LoadNuklearImage(const char* path);          // Load a Nuklear image
NK_API void UnloadNuklearImage(struct nk_image img);                // Unload a Nuklear image. And free its data
NK_API void CleanupNuklearImage(struct nk_image img);               // Frees the data stored by the Nuklear image
NK_API void SetNuklearScaling(struct nk_context * ctx, float scaling); // Sets the scaling for the given Nuklear context
NK_API float GetNuklearScaling(struct nk_context * ctx);            // Retrieves the scaling of the given Nuklear context
NK_API Shader* GetNuklearShader(struct nk_context * ctx);
NK_API float* GetNuklearShaderTime(struct nk_context * ctx);

// Internal Nuklear functions
NK_API float nk_raylib_font_get_text_width(nk_handle handle, float height, const char *text, int len);
NK_API float nk_raylib_font_get_text_width_user_font(nk_handle handle, float height, const char *text, int len);
NK_API void nk_raylib_clipboard_paste(nk_handle usr, struct nk_text_edit *edit);
NK_API void nk_raylib_clipboard_copy(nk_handle usr, const char *text, int len);
NK_API void* nk_raylib_malloc(nk_handle unused, void *old, nk_size size);
NK_API void nk_raylib_mfree(nk_handle unused, void *ptr);
NK_API struct nk_context* InitNuklearContext(struct nk_user_font* userFont, Shader* rect_shader, float* shader_time);
NK_API void nk_raylib_input_keyboard(struct nk_context * ctx);
NK_API void nk_raylib_input_mouse(struct nk_context * ctx);

#ifdef __cplusplus
}
#endif

#endif  // RAYLIB_NUKLEAR_H

#ifdef RAYLIB_NUKLEAR_IMPLEMENTATION
#ifndef RAYLIB_NUKLEAR_IMPLEMENTATION_ONCE
#define RAYLIB_NUKLEAR_IMPLEMENTATION_ONCE

#include <stddef.h> // NULL
#include <math.h> // cosf, sinf, sqrtf

// Math
#ifndef NK_COS
#define NK_COS cosf
#endif  // NK_COS
#ifndef NK_SIN
#define NK_SIN sinf
#endif  // NK_SIN
#ifndef NK_INV_SQRT
#define NK_INV_SQRT(value) (1.0f / sqrtf(value))
#endif  // NK_INV_SQRT

#define NK_IMPLEMENTATION
#include "nuklear.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RAYLIB_NUKLEAR_DEFAULT_FONTSIZE
/**
 * The default font size that is used when a font size is not provided.
 */
#define RAYLIB_NUKLEAR_DEFAULT_FONTSIZE 13
#endif  // RAYLIB_NUKLEAR_DEFAULT_FONTSIZE

#ifndef RAYLIB_NUKLEAR_DEFAULT_ARC_SEGMENTS
/**
 * The amount of segments used when drawing an arc.
 *
 * @see NK_COMMAND_ARC_FILLED
 */
#define RAYLIB_NUKLEAR_DEFAULT_ARC_SEGMENTS 20
#endif  // RAYLIB_NUKLEAR_DEFAULT_ARC_SEGMENTS

#ifdef RAYLIB_NUKLEAR_INCLUDE_DEFAULT_FONT
    #include "raylib-nuklear-font.h"
#endif

/**
 * The user data that's leverages internally through Nuklear.
 */
typedef struct NuklearUserData {
    float scaling; // The scaling of the Nuklear user interface.
    Shader* rect_shader;
    float* shader_time;
} NuklearUserData;

/**
 * Nuklear callback; Get the width of the given text.
 *
 * @internal
 */
NK_API float
nk_raylib_font_get_text_width(nk_handle handle, float height, const char *text, int len)
{
    NK_UNUSED(handle);

    if (len > 0) {
        // Grab the text with the cropped length so that it only measures the desired string length.
        const char* subtext = TextSubtext(text, 0, len);

        // Raylib only counts the spacing between characters, but Nuklear expects one spacing to be
        // counter for every character in the string:
        return (float)MeasureText(subtext, (int)height) + height / 10.0f;
    }

    return 0;
}

/**
 * Nuklear callback; Get the width of the given text (userFont version)
 *
 * @internal
 */
NK_API float
nk_raylib_font_get_text_width_user_font(nk_handle handle, float height, const char *text, int len)
{
    if (len > 0) {
        // Grab the text with the cropped length so that it only measures the desired string length.
        const char* subtext = TextSubtext(text, 0, len);

        // Spacing is determined by the font size divided by 10.
        // Raylib only counts the spacing between characters, but Nuklear expects one spacing to be
        // counter for every character in the string:
        return MeasureTextEx(*(Font*)handle.ptr, subtext, height, 0.0f).x;
    }

    return 0;
}

/**
 * Nuklear callback; Paste the current clipboard.
 *
 * @internal
 */
NK_API void
nk_raylib_clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    const char *text = GetClipboardText();
    NK_UNUSED(usr);
    if (text != NULL) {
        nk_textedit_paste(edit, text, (int)TextLength(text));
    }
}

/**
 * Nuklear callback; Copy the given text.
 *
 * @internal
 */
NK_API void
nk_raylib_clipboard_copy(nk_handle usr, const char *text, int len)
{
    NK_UNUSED(usr);
    char* trimmedText = (char*)MemAlloc((unsigned int)(sizeof(char) * (size_t)(len + 1)));
    if(!trimmedText)
        return;
    nk_memcopy(trimmedText, text, (nk_size)len);
    trimmedText[len] = 0;
    SetClipboardText(trimmedText);
    MemFree(trimmedText);
}

/**
 * Nuklear callback; Allocate memory for Nuklear.
 *
 * @internal
 */
NK_API void*
nk_raylib_malloc(nk_handle unused, void *old, nk_size size)
{
    NK_UNUSED(unused);
    NK_UNUSED(old);
    return MemAlloc((unsigned int)size);
}

/**
 * Nuklear callback; Free memory for Nuklear.
 */
NK_API void
nk_raylib_mfree(nk_handle unused, void *ptr)
{
    NK_UNUSED(unused);
    MemFree(ptr);
}

/**
 * Initialize the Nuklear context for use with Raylib, with the given Nuklear user font.
 *
 * @param userFont The Nuklear user font to initialize the Nuklear context with.
 *
 * @internal
 */
NK_API struct nk_context*
InitNuklearContext(struct nk_user_font* userFont, Shader* rect_shader, float* shader_time)
{
    struct nk_context* ctx = (struct nk_context*)MemAlloc(sizeof(struct nk_context));
    if (ctx == NULL) {
        TraceLog(LOG_ERROR, "NUKLEAR: Failed to initialize nuklear memory");
        return NULL;
    }

    struct NuklearUserData* userData = (struct NuklearUserData*)MemAlloc(sizeof(struct NuklearUserData));
    if (userData == NULL) {
        TraceLog(LOG_ERROR, "NUKLEAR: Failed to initialize nuklear user data");
        MemFree(ctx);
        return NULL;
    }

    // Allocator
    struct nk_allocator alloc;
    alloc.userdata = nk_handle_ptr(0);
    alloc.alloc = nk_raylib_malloc;
    alloc.free = nk_raylib_mfree;

    // Initialize the context.
    if (!nk_init(ctx, &alloc, userFont)) {
        TraceLog(LOG_ERROR, "NUKLEAR: Failed to initialize nuklear");
        MemFree(ctx);
        MemFree(userData);
        return NULL;
    }

    // Clipboard
    ctx->clip.copy = nk_raylib_clipboard_copy;
    ctx->clip.paste = nk_raylib_clipboard_paste;
    ctx->clip.userdata = nk_handle_ptr(0);

    // Set the internal user data.
    userData->scaling = 1.0f;
    userData->rect_shader = rect_shader;
    userData->shader_time = shader_time;
    nk_handle userDataHandle;
    userDataHandle.id = 1;
    userDataHandle.ptr = (void*)userData;
    nk_set_user_data(ctx, userDataHandle);

    TraceLog(LOG_INFO, "NUKLEAR: Initialized GUI");

    return ctx;
}

/**
 * Initialize the Nuklear context for use with Raylib.
 *
 * @param fontSize The size of the font to use for GUI text. Use 0 to use the default font size of 10.
 *
 * @return The nuklear context, or NULL on error.
 */
NK_API struct nk_context*
InitNuklear(int fontSize, Shader* rect_shader, float* shader_time)
{
    // User font.
    struct nk_user_font* userFont = (struct nk_user_font*)MemAlloc(sizeof(struct nk_user_font));

    // Use the default font size if desired.
    if (fontSize <= 0) {
        fontSize = RAYLIB_NUKLEAR_DEFAULT_FONTSIZE;
    }

    userFont->height = (float)fontSize;
    userFont->width = nk_raylib_font_get_text_width;
    userFont->userdata = nk_handle_ptr(0);

    // Nuklear context.
    return InitNuklearContext(userFont, rect_shader, shader_time);
}

/**
 * Initialize the Nuklear context for use with Raylib, with a supplied custom font.
 *
 * @param font The custom raylib font to use with Nuklear.
 * @param fontSize The desired size of the font. Use 0 to set the default size of 10.
 *
 * @return The nuklear context, or NULL on error.
 */
NK_API struct nk_context*
InitNuklearEx(struct nk_user_font* userFont, Shader* rect_shader, float* shader_time)
{
    // Nuklear context.
    return InitNuklearContext(userFont, rect_shader, shader_time);
}

NK_API struct nk_user_font*
LoadFontIntoNuklear(Font font, float fontSize) 
{
    struct Font* newFont = (struct Font*)MemAlloc(sizeof(struct Font));

    // Use the default font size if desired.
    if (fontSize <= 0.0f) {
        fontSize = (float)RAYLIB_NUKLEAR_DEFAULT_FONTSIZE;
    }
    newFont->baseSize = font.baseSize;
    newFont->glyphCount = font.glyphCount;
    newFont->glyphPadding = font.glyphPadding;
    newFont->glyphs = font.glyphs;
    newFont->recs = font.recs;
    newFont->texture = font.texture;

    // Create the nuklear user font.
    struct nk_user_font* userFont = (struct nk_user_font*)MemAlloc(sizeof(struct nk_user_font));
    userFont->userdata = nk_handle_ptr(newFont);
    userFont->height = fontSize;
    userFont->width = nk_raylib_font_get_text_width_user_font;

    return userFont;
}

/**
 * Load the default Nuklear font. Requires `RAYLIB_NUKLEAR_INCLUDE_DEFAULT_FONT` to be defined.
 *
 * @param size The size of the font to load (optional). Provide 0 if you'd like to use the default size from Nuklear.
 *
 * @return The loaded font, or an empty font on error.
 *
 * @code
 * #define RAYLIB_NUKLEAR_INCLUDE_DEFAULT_FONT
 * #include "raylib-nuklear.h"
 * Font font = LoadFontFromNuklear(0);
 * @endcode
 */
NK_API Font LoadFontFromNuklear(int size) {
#ifndef RAYLIB_NUKLEAR_INCLUDE_DEFAULT_FONT
    (void)size;
    TraceLog(LOG_ERROR, "NUKLEAR: RAYLIB_NUKLEAR_INCLUDE_DEFAULT_FONT must be defined to use LoadFontFromNuklear()");
    return CLITERAL(Font) {0};
#else
    if (size <= 0) {
        size = RAYLIB_NUKLEAR_DEFAULT_FONTSIZE;
    }

    #ifndef RAYLIB_NUKLEAR_DEFAULT_FONT_GLYPHS
    /**
     * The amount of glyphs to load for the default font.
     */
    #define RAYLIB_NUKLEAR_DEFAULT_FONT_GLYPHS 95
    #endif

    return LoadFontFromMemory(".ttf", RAYLIB_NUKLEAR_DEFAULT_FONT_NAME, RAYLIB_NUKLEAR_DEFAULT_FONT_SIZE, size, NULL, RAYLIB_NUKLEAR_DEFAULT_FONT_GLYPHS);
#endif
}

/**
 * Convert the given Nuklear color to a raylib color.
 */
NK_API Color
ColorFromNuklear(struct nk_color color)
{
    Color rc;
    rc.a = color.a;
    rc.r = color.r;
    rc.g = color.g;
    rc.b = color.b;
    return rc;
}

/**
 * Convert the given raylib color to a Nuklear color.
 */
NK_API struct nk_color
ColorToNuklear(Color color)
{
    struct nk_color rc;
    rc.a = color.a;
    rc.r = color.r;
    rc.g = color.g;
    rc.b = color.b;
    return rc;
}

/**
 * Convert the given Nuklear float color to a raylib color.
 */
NK_API Color
ColorFromNuklearF(struct nk_colorf color)
{
    return ColorFromNuklear(nk_rgba_cf(color));
}

/**
 * Convert the given raylib color to a raylib float color.
 */
NK_API struct nk_colorf
ColorToNuklearF(Color color)
{
    return nk_color_cf(ColorToNuklear(color));
}

/**
 * Draw the given Nuklear context in raylib.
 *
 * @param ctx The nuklear context.
 */
NK_API void
DrawNuklear(struct nk_context * ctx)
{
    // Protect against drawing when there's nothing to draw.
    if (ctx == NULL || nk__begin(ctx) == 0) {
        return;
    }

    const struct nk_command *cmd;
    const float scale = GetNuklearScaling(ctx);
    const Shader *shader = GetNuklearShader(ctx);
    const float *shader_time = GetNuklearShaderTime(ctx);

    nk_foreach(cmd, ctx) {
        switch (cmd->type) {
            case NK_COMMAND_NOP: {
                break;
            }

            case NK_COMMAND_SCISSOR: {
                // TODO(RobLoach): Verify if NK_COMMAND_SCISSOR works.
                const struct nk_command_scissor *s =(const struct nk_command_scissor*)cmd;
                BeginScissorMode((int)(s->x * scale), (int)(s->y * scale), (int)(s->w * scale), (int)(s->h * scale));
            } break;

            case NK_COMMAND_LINE: {
                const struct nk_command_line *l = (const struct nk_command_line *)cmd;
                Color color = ColorFromNuklear(l->color);
                Vector2 startPos = CLITERAL(Vector2) {(float)l->begin.x * scale, (float)l->begin.y * scale};
                Vector2 endPos = CLITERAL(Vector2) {(float)l->end.x * scale, (float)l->end.y * scale};
                DrawLineEx(startPos, endPos, l->line_thickness * scale, color);
            } break;

            case NK_COMMAND_CURVE: {
                const struct nk_command_curve *q = (const struct nk_command_curve *)cmd;
                Color color = ColorFromNuklear(q->color);
                Vector2 begin = CLITERAL(Vector2) {(float)q->begin.x * scale, (float)q->begin.y * scale};
                Vector2 controlPoint1 = CLITERAL(Vector2) {(float)q->ctrl[0].x * scale, (float)q->ctrl[0].y * scale};
                Vector2 controlPoint2 = CLITERAL(Vector2) {(float)q->ctrl[1].x * scale, (float)q->ctrl[1].y * scale};
                Vector2 end = CLITERAL(Vector2) {(float)q->end.x * scale, (float)q->end.y * scale};
                DrawSplineSegmentBezierCubic(begin, controlPoint1, controlPoint2, end, (float)q->line_thickness * scale, color);
            } break;

            case NK_COMMAND_RECT: {
                const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;
                Color color = ColorFromNuklear(r->color);
                Rectangle rect = CLITERAL(Rectangle) {(float)r->x * scale, (float)r->y * scale, (float)r->w * scale, (float)r->h * scale};
                float roundness = (rect.width > rect.height) ?
                        ((2 * r->rounding * scale)/rect.height) : ((2 * r->rounding * scale)/rect.width);
                roundness = NK_CLAMP(0.0f, roundness, 1.0f);
                if (roundness > 0.0f) {
                    // DrawRectangleRoundedLines doesn't work in the same way as DrawRectangleLinesEx and it draws
                    // the outline outside the region defined by the rectangle. To compensate for that, shrink
                    // the rectangle by the thickness plus 1 (due to inconsistencies from DrawRectangleRoundedLines):
                    rect.x += ((float) r->line_thickness) * scale + 1.0f;
                    rect.y += ((float) r->line_thickness) * scale + 1.0f;
                    rect.width = NK_MAX(rect.width - (2 * ((float) r->line_thickness) * scale + 1.0f), 0.0f);
                    rect.height = NK_MAX(rect.height - (2 * ((float) r->line_thickness) * scale + 1.0f), 0.0f);
#if RAYLIB_VERSION_MAJOR >= 5 && RAYLIB_VERSION_MINOR == 0
                    DrawRectangleRoundedLines(rect, roundness, RAYLIB_NUKLEAR_DEFAULT_ARC_SEGMENTS, (float)r->line_thickness * scale, color);
#else
                    DrawRectangleRoundedLinesEx(rect, roundness, RAYLIB_NUKLEAR_DEFAULT_ARC_SEGMENTS, (float)r->line_thickness * scale, color);
#endif
                }
                else if (*shader_time < 1.0) {
                    BeginShaderMode(*shader);
                    DrawRectangleLinesEx(rect, r->line_thickness * scale, color);
                    EndShaderMode();
                }
            } break;

            case NK_COMMAND_RECT_FILLED: {
                const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;
                Color color = ColorFromNuklear(r->color);
                Rectangle rect = CLITERAL(Rectangle) {(float)r->x * scale, (float)r->y * scale, (float)r->w * scale, (float)r->h * scale};
                float roundness = (rect.width > rect.height) ?
                        ((2 * r->rounding * scale)/rect.height) : ((2 * r->rounding * scale)/rect.width);
                roundness = NK_CLAMP(0.0f, roundness, 1.0f);
                if (roundness > 0.0f) {
                    DrawRectangleRounded(rect, roundness, RAYLIB_NUKLEAR_DEFAULT_ARC_SEGMENTS, color);
                }
                else {
                    DrawRectangleRec(rect, color);
                }
            } break;

            case NK_COMMAND_RECT_MULTI_COLOR: {
                const struct nk_command_rect_multi_color* rectangle = (const struct nk_command_rect_multi_color *)cmd;
                Rectangle position = {(float)rectangle->x * scale, (float)rectangle->y * scale, (float)rectangle->w * scale, (float)rectangle->h * scale};
                Color left = ColorFromNuklear(rectangle->left);
                Color top = ColorFromNuklear(rectangle->top);
                Color bottom = ColorFromNuklear(rectangle->bottom);
                Color right = ColorFromNuklear(rectangle->right);
                DrawRectangleGradientEx(position, left, bottom, right, top);
            } break;

            case NK_COMMAND_CIRCLE: {
                const struct nk_command_circle *c = (const struct nk_command_circle *)cmd;
                Color color = ColorFromNuklear(c->color);
                for (unsigned short i = 0; i < c->line_thickness; i++) {
                    DrawEllipseLines((int)(c->x * scale + c->w * scale / 2.0f), (int)(c->y * scale + c->h * scale / 2.0f), c->w * scale / 2.0f - (float)i / 2.0f, c->h * scale / 2.0f - (float)i / 2.0f, color);
                }
            } break;

            case NK_COMMAND_CIRCLE_FILLED: {
                const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;
                Color color = ColorFromNuklear(c->color);
                DrawEllipse((int)(c->x * scale + c->w * scale / 2.0f), (int)(c->y * scale + c->h * scale / 2.0f), (int)(c->w * scale / 2), (int)(c->h * scale / 2), color);
            } break;

            case NK_COMMAND_ARC: {
                const struct nk_command_arc *a = (const struct nk_command_arc*)cmd;
                Color color = ColorFromNuklear(a->color);
                Vector2 center = CLITERAL(Vector2) {(float)a->cx, (float)a->cy};
                DrawRingLines(center, 0, a->r * scale, a->a[0] * RAD2DEG, a->a[1] * RAD2DEG, RAYLIB_NUKLEAR_DEFAULT_ARC_SEGMENTS, color);
            } break;

            case NK_COMMAND_ARC_FILLED: {
                const struct nk_command_arc_filled *a = (const struct nk_command_arc_filled*)cmd;
                Color color = ColorFromNuklear(a->color);
                Vector2 center = CLITERAL(Vector2) {(float)a->cx * scale, (float)a->cy * scale};
                DrawRing(center, 0, a->r * scale, a->a[0] * RAD2DEG, a->a[1] * RAD2DEG, RAYLIB_NUKLEAR_DEFAULT_ARC_SEGMENTS, color);
            } break;

            case NK_COMMAND_TRIANGLE: {
                const struct nk_command_triangle *t = (const struct nk_command_triangle*)cmd;
                Color color = ColorFromNuklear(t->color);
                Vector2 point1 = CLITERAL(Vector2) {(float)t->b.x * scale, (float)t->b.y * scale};
                Vector2 point2 = CLITERAL(Vector2) {(float)t->a.x * scale, (float)t->a.y * scale};
                Vector2 point3 = CLITERAL(Vector2) {(float)t->c.x * scale, (float)t->c.y * scale};

                // DrawLineEx(point1, point2, t->line_thickness * scale, color);
                // DrawLineEx(point2, point3, t->line_thickness * scale, color);
                // DrawLineEx(point3, point1, t->line_thickness * scale, color);
                // TODO: Add line thickness to DrawTriangleLines(), maybe via a DrawTriangleLinesEx()?
                DrawTriangleLines(point1, point2, point3, color);
            } break;

            case NK_COMMAND_TRIANGLE_FILLED: {
                const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled*)cmd;
                Color color = ColorFromNuklear(t->color);
                Vector2 point1 = CLITERAL(Vector2) {(float)t->b.x * scale, (float)t->b.y * scale};
                Vector2 point2 = CLITERAL(Vector2) {(float)t->a.x * scale, (float)t->a.y * scale};
                Vector2 point3 = CLITERAL(Vector2) {(float)t->c.x * scale, (float)t->c.y * scale};
                DrawTriangle(point1, point2, point3, color);
            } break;

            case NK_COMMAND_POLYGON: {
                const struct nk_command_polygon *p = (const struct nk_command_polygon*)cmd;
                Color color = ColorFromNuklear(p->color);
                struct Vector2* points = (struct Vector2*)MemAlloc((unsigned int)((size_t)(p->point_count + 1) * sizeof(Vector2)));
                unsigned short i;
                for (i = 0; i < p->point_count; i++) {
                    points[i].x = p->points[i].x * scale;
                    points[i].y = p->points[i].y * scale;
                }
                points[p->point_count] = points[0];
                DrawLineStrip(points, p->point_count + 1, color);
                MemFree(points);
            } break;

            case NK_COMMAND_POLYGON_FILLED: {
                // TODO: Implement NK_COMMAND_POLYGON_FILLED
                const struct nk_command_polygon_filled *p = (const struct nk_command_polygon_filled*)cmd;
                Color color = ColorFromNuklear(p->color);
                struct Vector2* points = (struct Vector2*)MemAlloc((unsigned int)((size_t)(p->point_count + 1) * sizeof(Vector2)));
                for (unsigned short i = 0; i < p->point_count; i++) {
                    points[i].x = p->points[i].x * scale;
                    points[i].y = p->points[i].y * scale;
                }
                points[p->point_count] = points[0];
                DrawLineStrip(points, p->point_count + 1, color);
                MemFree(points);
            } break;

            case NK_COMMAND_POLYLINE: {
                const struct nk_command_polyline *p = (const struct nk_command_polyline *)cmd;
                Color color = ColorFromNuklear(p->color);
                for (unsigned short i = 0; i < p->point_count - 1; i++) {
                    Vector2 start = {(float)p->points[i].x * scale, (float)p->points[i].y * scale};
                    Vector2 end = {(float)p->points[i + 1].x * scale, (float)p->points[i + 1].y * scale};
                    DrawLineEx(start, end, p->line_thickness * scale, color);
                }
            } break;

            case NK_COMMAND_TEXT: {
                const struct nk_command_text *text = (const struct nk_command_text*)cmd;
                Color color = ColorFromNuklear(text->foreground);
                float fontSize = text->font->height * scale;
                Font* font = (Font*)text->font->userdata.ptr;
                if (font != NULL) {
                    Vector2 position = {(float)text->x * scale, (float)text->y * scale};
                    DrawTextEx(*font, (const char*)text->string, position, fontSize, 0.0f, color);
                }
                else {
                    DrawText((const char*)text->string, (int)(text->x * scale), (int)(text->y * scale), (int)fontSize, color);
                }
            } break;

            case NK_COMMAND_IMAGE: {
                const struct nk_command_image *i = (const struct nk_command_image *)cmd;
                Texture texture = *(Texture*)i->img.handle.ptr;
                Rectangle source = CLITERAL(Rectangle) {i->img.region[0], i->img.region[1], (float)i->img.region[2], (float)i->img.region[3]};
                Rectangle dest = CLITERAL(Rectangle) {(float)i->x * scale, (float)i->y * scale, (float)i->w * scale, (float)i->h * scale};
                Vector2 origin = CLITERAL(Vector2) {0, 0};
                Color tint = ColorFromNuklear(i->col);
                DrawTexturePro(texture, source, dest, origin, 0, tint);
            } break;

            case NK_COMMAND_CUSTOM: {
                TraceLog(LOG_WARNING, "NUKLEAR: Unverified custom callback implementation NK_COMMAND_CUSTOM");
                const struct nk_command_custom *custom = (const struct nk_command_custom *)cmd;
                custom->callback(NULL, (short)(custom->x * scale), (short)(custom->y * scale), (unsigned short)(custom->w * scale), (unsigned short)(custom->h * scale), custom->callback_data);
            } break;

            default: {
                TraceLog(LOG_WARNING, "NUKLEAR: Missing implementation %i", cmd->type);
            } break;
        }
    }

    nk_clear(ctx);
}

struct nk_raylib_input_keyboard_check {
    int key;
    int input_key;
    bool modifier;
};

/**
 * Update the Nuklear context for the keyboard input from raylib.
 *
 * @param ctx The nuklear context.
 *
 * @internal
 */
NK_API void
nk_raylib_input_keyboard(struct nk_context * ctx)
{
    bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool command = IsKeyDown(KEY_LEFT_SUPER);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    #define NK_RAYLIB_INPUT_KEYBOARD_CHECK_NUM 16
    struct nk_raylib_input_keyboard_check checks[NK_RAYLIB_INPUT_KEYBOARD_CHECK_NUM] = {
        (struct nk_raylib_input_keyboard_check) {KEY_DELETE, NK_KEY_DEL, true},
        (struct nk_raylib_input_keyboard_check) {KEY_ENTER, NK_KEY_ENTER, true},
        (struct nk_raylib_input_keyboard_check) {KEY_BACKSPACE, NK_KEY_BACKSPACE, true},
        (struct nk_raylib_input_keyboard_check) {KEY_C, NK_KEY_COPY, (control || command)},
        (struct nk_raylib_input_keyboard_check) {KEY_V, NK_KEY_PASTE, (control || command)},
        (struct nk_raylib_input_keyboard_check) {KEY_B, NK_KEY_TEXT_LINE_START, (control || command)},
        (struct nk_raylib_input_keyboard_check) {KEY_E, NK_KEY_TEXT_LINE_END, (control || command)},
        (struct nk_raylib_input_keyboard_check) {KEY_Z, NK_KEY_TEXT_UNDO, (control || command)},
        (struct nk_raylib_input_keyboard_check) {KEY_R, NK_KEY_TEXT_REDO, (control || command)},
        (struct nk_raylib_input_keyboard_check) {KEY_A, NK_KEY_TEXT_SELECT_ALL, (control || command)},
        (struct nk_raylib_input_keyboard_check) {KEY_LEFT, NK_KEY_TEXT_WORD_LEFT, (control || command)},
        (struct nk_raylib_input_keyboard_check) {KEY_RIGHT, NK_KEY_TEXT_WORD_RIGHT, (control || command)},
        (struct nk_raylib_input_keyboard_check) {KEY_RIGHT, NK_KEY_RIGHT, true},
        (struct nk_raylib_input_keyboard_check) {KEY_LEFT, NK_KEY_LEFT, true},
        (struct nk_raylib_input_keyboard_check) {KEY_UP, NK_KEY_UP, true},
        (struct nk_raylib_input_keyboard_check) {KEY_DOWN, NK_KEY_DOWN, true}
    };
    bool checked = false;
    for (int i = 0; i < NK_RAYLIB_INPUT_KEYBOARD_CHECK_NUM; i++) {
        struct nk_raylib_input_keyboard_check check = checks[i];
        if (IsKeyDown(check.key) && check.modifier) {
            nk_input_key(ctx, check.input_key, true);
            checked = true;
        } else {
            nk_input_key(ctx, check.input_key, false);
        }
    }
    #undef NK_RAYLIB_INPUT_KEYBOARD_CHECK_NUM

    nk_input_key(ctx, NK_KEY_SHIFT, shift);

    if (checked) {
        return;
    }

    nk_input_key(ctx, NK_KEY_LEFT, IsKeyDown(KEY_LEFT));
    nk_input_key(ctx, NK_KEY_RIGHT, IsKeyDown(KEY_RIGHT));
    nk_input_key(ctx, NK_KEY_UP, IsKeyDown(KEY_UP));
    nk_input_key(ctx, NK_KEY_DOWN, IsKeyDown(KEY_DOWN));
    nk_input_key(ctx, NK_KEY_TEXT_START, IsKeyDown(KEY_HOME));
    nk_input_key(ctx, NK_KEY_TEXT_END, IsKeyDown(KEY_END));
    nk_input_key(ctx, NK_KEY_SCROLL_START, IsKeyDown(KEY_HOME) && control);
    nk_input_key(ctx, NK_KEY_SCROLL_END, IsKeyDown(KEY_END) && control);
    nk_input_key(ctx, NK_KEY_SCROLL_DOWN, IsKeyDown(KEY_PAGE_DOWN));
    nk_input_key(ctx, NK_KEY_SCROLL_UP, IsKeyDown(KEY_PAGE_UP));

    // Keys
    if (IsKeyPressed(KEY_APOSTROPHE)) nk_input_unicode(ctx, shift ? 34 : (nk_rune)KEY_APOSTROPHE);
    if (IsKeyPressed(KEY_COMMA)) nk_input_unicode(ctx, shift ? 60 : (nk_rune)KEY_COMMA);
    if (IsKeyPressed(KEY_MINUS)) nk_input_unicode(ctx, shift ? 95 : (nk_rune)KEY_MINUS);
    if (IsKeyPressed(KEY_PERIOD)) nk_input_unicode(ctx, shift ? 62 : (nk_rune)KEY_PERIOD);
    if (IsKeyPressed(KEY_SLASH)) nk_input_unicode(ctx, shift ? 63 : (nk_rune)KEY_SLASH);
    if (IsKeyPressed(KEY_ZERO)) nk_input_unicode(ctx, shift ? 41 : (nk_rune)KEY_ZERO);
    if (IsKeyPressed(KEY_ONE)) nk_input_unicode(ctx, shift ? 33 : (nk_rune)KEY_ONE);
    if (IsKeyPressed(KEY_TWO)) nk_input_unicode(ctx, shift ? 64 : (nk_rune)KEY_TWO);
    if (IsKeyPressed(KEY_THREE)) nk_input_unicode(ctx, shift ? 35 : (nk_rune)KEY_THREE);
    if (IsKeyPressed(KEY_FOUR)) nk_input_unicode(ctx, shift ? 36 : (nk_rune)KEY_FOUR);
    if (IsKeyPressed(KEY_FIVE)) nk_input_unicode(ctx, shift ? 37 : (nk_rune)KEY_FIVE);
    if (IsKeyPressed(KEY_SIX)) nk_input_unicode(ctx, shift ? 94 : (nk_rune)KEY_SIX);
    if (IsKeyPressed(KEY_SEVEN)) nk_input_unicode(ctx, shift ? 38 : (nk_rune)KEY_SEVEN);
    if (IsKeyPressed(KEY_EIGHT)) nk_input_unicode(ctx, shift ? 42 : (nk_rune)KEY_EIGHT);
    if (IsKeyPressed(KEY_NINE)) nk_input_unicode(ctx, shift ? 40 : (nk_rune)KEY_NINE);
    if (IsKeyPressed(KEY_SEMICOLON)) nk_input_unicode(ctx, shift ? 41 : (nk_rune)KEY_SEMICOLON);
    if (IsKeyPressed(KEY_EQUAL)) nk_input_unicode(ctx, shift ? 43 : (nk_rune)KEY_EQUAL);
    if (IsKeyPressed(KEY_A)) nk_input_unicode(ctx, shift ? KEY_A : KEY_A + 32);
    if (IsKeyPressed(KEY_B)) nk_input_unicode(ctx, shift ? KEY_B : KEY_B + 32);
    if (IsKeyPressed(KEY_C)) nk_input_unicode(ctx, shift ? KEY_C : KEY_C + 32);
    if (IsKeyPressed(KEY_D)) nk_input_unicode(ctx, shift ? KEY_D : KEY_D + 32);
    if (IsKeyPressed(KEY_E)) nk_input_unicode(ctx, shift ? KEY_E : KEY_E + 32);
    if (IsKeyPressed(KEY_F)) nk_input_unicode(ctx, shift ? KEY_F : KEY_F + 32);
    if (IsKeyPressed(KEY_G)) nk_input_unicode(ctx, shift ? KEY_G : KEY_G + 32);
    if (IsKeyPressed(KEY_H)) nk_input_unicode(ctx, shift ? KEY_H : KEY_H + 32);
    if (IsKeyPressed(KEY_I)) nk_input_unicode(ctx, shift ? KEY_I : KEY_I + 32);
    if (IsKeyPressed(KEY_J)) nk_input_unicode(ctx, shift ? KEY_J : KEY_J + 32);
    if (IsKeyPressed(KEY_K)) nk_input_unicode(ctx, shift ? KEY_K : KEY_K + 32);
    if (IsKeyPressed(KEY_L)) nk_input_unicode(ctx, shift ? KEY_L : KEY_L + 32);
    if (IsKeyPressed(KEY_M)) nk_input_unicode(ctx, shift ? KEY_M : KEY_M + 32);
    if (IsKeyPressed(KEY_N)) nk_input_unicode(ctx, shift ? KEY_N : KEY_N + 32);
    if (IsKeyPressed(KEY_O)) nk_input_unicode(ctx, shift ? KEY_O : KEY_O + 32);
    if (IsKeyPressed(KEY_P)) nk_input_unicode(ctx, shift ? KEY_P : KEY_P + 32);
    if (IsKeyPressed(KEY_Q)) nk_input_unicode(ctx, shift ? KEY_Q : KEY_Q + 32);
    if (IsKeyPressed(KEY_R)) nk_input_unicode(ctx, shift ? KEY_R : KEY_R + 32);
    if (IsKeyPressed(KEY_S)) nk_input_unicode(ctx, shift ? KEY_S : KEY_S + 32);
    if (IsKeyPressed(KEY_T)) nk_input_unicode(ctx, shift ? KEY_T : KEY_T + 32);
    if (IsKeyPressed(KEY_U)) nk_input_unicode(ctx, shift ? KEY_U : KEY_U + 32);
    if (IsKeyPressed(KEY_V)) nk_input_unicode(ctx, shift ? KEY_V : KEY_V + 32);
    if (IsKeyPressed(KEY_W)) nk_input_unicode(ctx, shift ? KEY_W : KEY_W + 32);
    if (IsKeyPressed(KEY_X)) nk_input_unicode(ctx, shift ? KEY_X : KEY_X + 32);
    if (IsKeyPressed(KEY_Y)) nk_input_unicode(ctx, shift ? KEY_Y : KEY_Y + 32);
    if (IsKeyPressed(KEY_Z)) nk_input_unicode(ctx, shift ? KEY_Z : KEY_Z + 32);
    if (IsKeyPressed(KEY_LEFT_BRACKET)) nk_input_unicode(ctx, shift ? 123 : (nk_rune)KEY_LEFT_BRACKET);
    if (IsKeyPressed(KEY_BACKSLASH)) nk_input_unicode(ctx, shift ? 124 : (nk_rune)KEY_BACKSLASH);
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) nk_input_unicode(ctx, shift ? 125 : (nk_rune)KEY_RIGHT_BRACKET);
    if (IsKeyPressed(KEY_GRAVE)) nk_input_unicode(ctx, shift ? 126 : (nk_rune)KEY_GRAVE);

    // Functions
    if (IsKeyPressed(KEY_SPACE)) nk_input_unicode(ctx, KEY_SPACE);
    if (IsKeyPressed(KEY_TAB)) nk_input_unicode(ctx, 9);

    // Keypad
    if (IsKeyPressed(KEY_KP_0)) nk_input_unicode(ctx, KEY_ZERO);
    if (IsKeyPressed(KEY_KP_1)) nk_input_unicode(ctx, KEY_ONE);
    if (IsKeyPressed(KEY_KP_2)) nk_input_unicode(ctx, KEY_TWO);
    if (IsKeyPressed(KEY_KP_3)) nk_input_unicode(ctx, KEY_THREE);
    if (IsKeyPressed(KEY_KP_4)) nk_input_unicode(ctx, KEY_FOUR);
    if (IsKeyPressed(KEY_KP_5)) nk_input_unicode(ctx, KEY_FIVE);
    if (IsKeyPressed(KEY_KP_6)) nk_input_unicode(ctx, KEY_SIX);
    if (IsKeyPressed(KEY_KP_7)) nk_input_unicode(ctx, KEY_SEVEN);
    if (IsKeyPressed(KEY_KP_8)) nk_input_unicode(ctx, KEY_EIGHT);
    if (IsKeyPressed(KEY_KP_9)) nk_input_unicode(ctx, KEY_NINE);
    if (IsKeyPressed(KEY_KP_DECIMAL)) nk_input_unicode(ctx, KEY_PERIOD);
    if (IsKeyPressed(KEY_KP_DIVIDE)) nk_input_unicode(ctx, KEY_SLASH);
    if (IsKeyPressed(KEY_KP_MULTIPLY)) nk_input_unicode(ctx, 48);
    if (IsKeyPressed(KEY_KP_SUBTRACT)) nk_input_unicode(ctx, 45);
    if (IsKeyPressed(KEY_KP_ADD)) nk_input_unicode(ctx, 43);
}

/**
 * Update the Nuklear context for the mouse input from raylib.
 *
 * @param ctx The nuklear context.
 *
 * @internal
 */
NK_API void
nk_raylib_input_mouse(struct nk_context * ctx)
{
    const float scale = GetNuklearScaling(ctx);
    const int mouseX = (int)((float)GetMouseX() / scale);
    const int mouseY = (int)((float)GetMouseY() / scale);

    nk_input_motion(ctx, mouseX, mouseY);
    nk_input_button(ctx, NK_BUTTON_LEFT, mouseX, mouseY, IsMouseButtonDown(MOUSE_LEFT_BUTTON));
    nk_input_button(ctx, NK_BUTTON_RIGHT, mouseX, mouseY, IsMouseButtonDown(MOUSE_RIGHT_BUTTON));
    nk_input_button(ctx, NK_BUTTON_MIDDLE, mouseX, mouseY, IsMouseButtonDown(MOUSE_MIDDLE_BUTTON));

    // Mouse Wheel
    float mouseWheel = GetMouseWheelMove();
    if (mouseWheel != 0.0f) {
        struct nk_vec2 mouseWheelMove;
        mouseWheelMove.x = 0.0f;
        mouseWheelMove.y = mouseWheel;
        nk_input_scroll(ctx, mouseWheelMove);
    }
}

/**
 * Update the Nuklear context for raylib's state.
 *
 * @param ctx The nuklear context to act upon.
 */
NK_API inline void
UpdateNuklear(struct nk_context * ctx)
{
    UpdateNuklearEx(ctx, GetFrameTime());
}

/**
 * Update the Nuklear context for raylib's state.
 *
 * @param ctx The nuklear context to act upon.
 * @param deltaTime Time in seconds since last frame.
 */
NK_API void
UpdateNuklearEx(struct nk_context * ctx, float deltaTime)
{
    // Update the time that has changed since last frame.
    ctx->delta_time_seconds = deltaTime;

    // Update the input state.
    nk_input_begin(ctx);
    {
        nk_raylib_input_mouse(ctx);
        nk_raylib_input_keyboard(ctx);
    }
    nk_input_end(ctx);
}

/**
 * Unload the given Nuklear context, along with all internal raylib textures.
 *
 * @param ctx The nuklear context.
 */
NK_API void
UnloadNuklear(struct nk_context * ctx)
{
    struct nk_user_font* userFont;

    // Skip unloading if it's not set.
    if (ctx == NULL) {
        return;
    }

    // Unload the font.
    userFont = (struct nk_user_font*)ctx->style.font;
    if (userFont != NULL) {
        // Clear the raylib Font object.
        void* fontPtr = userFont->userdata.ptr;
        if (fontPtr != NULL) {
            MemFree(fontPtr);
        }

        // Clear the user font.
        MemFree(userFont);
        ctx->style.font = NULL;
    }

    // Unload the custom user data.
    if (ctx->userdata.ptr != NULL) {
        MemFree(ctx->userdata.ptr);
    }

    // Unload the nuklear context.
    nk_free(ctx);
    MemFree(ctx);
    TraceLog(LOG_INFO, "NUKLEAR: Unloaded GUI");
}

/**
 * Convert the given Nuklear rectangle to a raylib Rectangle.
 */
NK_API struct
Rectangle RectangleFromNuklear(struct nk_context* ctx, struct nk_rect rect)
{
    float scaling = GetNuklearScaling(ctx);
    Rectangle output;
    output.x = rect.x * scaling;
    output.y = rect.y * scaling;
    output.width = rect.w * scaling;
    output.height = rect.h * scaling;
    return output;
}

/**
 * Convert the given raylib Rectangle to a Nuklear rectangle.
 */
NK_API struct
nk_rect RectangleToNuklear(struct nk_context* ctx, Rectangle rect)
{
    float scaling = GetNuklearScaling(ctx);
    return nk_rect(rect.x / scaling, rect.y / scaling, rect.width / scaling, rect.height / scaling);
}

/**
 * Convert the given raylib texture to a Nuklear image
 */
NK_API struct nk_image
TextureToNuklear(Texture tex)
{
	// Declare the img to store data and allocate memory
	// For the texture
	struct nk_image img;
	struct Texture* stored_tex = (struct Texture*)MemAlloc(sizeof(Texture));

	// Copy the data from the texture given into the new texture
	stored_tex->id = tex.id;
	stored_tex->width = tex.width;
	stored_tex->height = tex.height;
	stored_tex->mipmaps = tex.mipmaps;
	stored_tex->format = tex.format;

	// Initialize the nk_image struct
	img.handle.ptr = stored_tex;
	img.w = (nk_ushort)stored_tex->width;
	img.h = (nk_ushort)stored_tex->height;

    // Set the region so we can sub-select the image later.
    img.region[0] = (nk_ushort)0;
    img.region[1] = (nk_ushort)0;
    img.region[2] = img.w;
    img.region[3] = img.h;

	return img;
}

/**
 * Convert the given Nuklear image to a raylib Texture
 */
NK_API struct Texture
TextureFromNuklear(struct nk_image img)
{
	// Declare texture for storage
	// And get back the stored texture
	Texture tex;
	Texture* stored_tex = (Texture*)img.handle.ptr;

	// Copy the data from the stored texture to the texture
	tex.id = stored_tex->id;
	tex.width = stored_tex->width;
	tex.height = stored_tex->height;
	tex.mipmaps = stored_tex->mipmaps;
	tex.format = stored_tex->format;

	return tex;
}

/**
 * Load a Nuklear image directly
 *
 * @param path The path to the image
 */
NK_API struct nk_image
LoadNuklearImage(const char* path)
{
	return TextureToNuklear(LoadTexture(path));
}

/**
 * Unload a loaded Nuklear image
 *
 * @param img The Nuklear image to unload
 */
NK_API void
UnloadNuklearImage(struct nk_image img)
{
	Texture tex = TextureFromNuklear(img);
	UnloadTexture(tex);
	CleanupNuklearImage(img);
}

/**
 * Cleans up memory used by a Nuklear image
 * Does not unload the image.
 *
 * @param img The Nuklear image to cleanup
*/
NK_API void
CleanupNuklearImage(struct nk_image img)
{
    MemFree(img.handle.ptr);
}

/**
 * Sets the scaling of the given Nuklear context.
 *
 * @param ctx The nuklear context.
 * @param scaling How much scale to apply to the graphical user interface.
 */
NK_API void
SetNuklearScaling(struct nk_context * ctx, float scaling)
{
    if (ctx == NULL) {
        return;
    }

    if (scaling <= 0.0f) {
        TraceLog(LOG_WARNING, "NUKLEAR: Cannot set scaling to be less than 0");
        return;
    }

    struct NuklearUserData* userData = (struct NuklearUserData*)ctx->userdata.ptr;
    if (userData != NULL) {
        userData->scaling = scaling;
    }
}

/**
 * Retrieves the scale value of the given Nuklear context.
 *
 * @return The scale value that had been set for the Nuklear context. 1.0f is the default scale value.
 */
NK_API float
GetNuklearScaling(struct nk_context * ctx)
{
    if (ctx == NULL) {
        return 1.0f;
    }

    struct NuklearUserData* userData = (struct NuklearUserData*)ctx->userdata.ptr;
    if (userData != NULL) {
        return userData->scaling;
    }

    return 1.0f;
}

NK_API Shader*
GetNuklearShader(struct nk_context * ctx)
{
    if (ctx == NULL) {
        return NULL;
    }

    struct NuklearUserData* userData = (struct NuklearUserData*)ctx->userdata.ptr;
    if (userData != NULL) {
        return userData->rect_shader;
    }

    return NULL;
}

NK_API float*
GetNuklearShaderTime(struct nk_context * ctx)
{
    if (ctx == NULL) {
        return NULL;
    }

    struct NuklearUserData* userData = (struct NuklearUserData*)ctx->userdata.ptr;
    if (userData != NULL) {
        return userData->shader_time;
    }

    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif  // RAYLIB_NUKLEAR_IMPLEMENTATION_ONCE
#endif  // RAYLIB_NUKLEAR_IMPLEMENTATION
