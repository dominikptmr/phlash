#ifndef WORKSPEED_SPACES_H
#define WORKSPEED_SPACES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPACES_MAX 64

struct space {
    uint64_t id;
    int display;        // 0-based position in Mission Control's display list
    bool fullscreen;    // native fullscreen or Split View
};

// Fills out with every space in Mission Control order, displays one after
// another, so out[i] is space index i + 1.
// Returns the number of spaces.
size_t spaces_get(struct space out[SPACES_MAX]);

// ID of the space that has focus.
uint64_t spaces_active(void);

// 1-based index of the space with this ID, or 0 if it isn't in the list.
int spaces_index(const struct space *spaces, size_t count, uint64_t id);

// The space a window is on, or 0 if none (e.g. minimized or hidden).
uint64_t spaces_of_window(uint32_t window_id);

#endif
