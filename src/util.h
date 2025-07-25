#ifndef SCRAP_UTIL_H
#define SCRAP_UTIL_H

#include <stdarg.h>

typedef struct {
    struct timespec start;
    const char* name;
} Timer;

int leading_ones(unsigned char byte);
Timer start_timer(const char* name);
double end_timer(Timer timer);
void scrap_log(int log_level, const char *text, va_list args);

#endif // SCRAP_UTIL_H
