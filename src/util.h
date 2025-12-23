#ifndef SCRAP_UTIL_H
#define SCRAP_UTIL_H

#include <time.h>
#include <stdarg.h>

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

#define CONVERT_COLOR(color, type) (type) { color.r, color.g, color.b, color.a }

#define MOD(x, y) (((x) % (y) + (y)) % (y))
#define LERP(min, max, t) (((max) - (min)) * (t) + (min))
#define UNLERP(min, max, v) (((float)(v) - (float)(min)) / ((float)(max) - (float)(min)))

typedef struct {
    struct timespec start;
    const char* name;
} Timer;

Timer start_timer(const char* name);
double end_timer(Timer timer);
void scrap_log(int log_level, const char *text, va_list args);

#endif // SCRAP_UTIL_H
