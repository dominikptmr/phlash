#include "log.h"

#include <stdarg.h>
#include <stdio.h>

bool log_debug;

void debugf(const char *format, ...) {
    if (!log_debug) return;
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}
