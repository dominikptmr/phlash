#ifndef WORKSPEED_LOG_H
#define WORKSPEED_LOG_H

#include <stdbool.h>

// Set by --debug.
extern bool log_debug;

// Prints to stderr when --debug is on; a newline is added.
void debugf(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
