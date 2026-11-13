#ifndef PHLASH_WINDOWS_H
#define PHLASH_WINDOWS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define WINDOWS_MAX 512

struct window {
    uint32_t id;
    pid_t pid;
    char app[128];
    uint64_t space;
};

size_t windows_get(struct window out[WINDOWS_MAX]);

#endif
