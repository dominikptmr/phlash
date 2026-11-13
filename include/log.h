#ifndef PHLASH_LOG_H
#define PHLASH_LOG_H

#include <stdbool.h>

extern bool log_debug;

void debugf(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
