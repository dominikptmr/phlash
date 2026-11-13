#ifndef PHLASH_SPACES_H
#define PHLASH_SPACES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPACES_MAX 64

struct space {
    uint64_t id;
    int display;
    bool fullscreen;
};

// out[i] is space i + 1
size_t spaces_get(struct space out[SPACES_MAX]);

uint64_t spaces_active(void);

// 1-based or 0 if missing
int spaces_index(const struct space *spaces, size_t count, uint64_t id);

uint64_t spaces_of_window(uint32_t window_id);

#endif
