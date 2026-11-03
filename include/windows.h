#ifndef WORKSPEED_WINDOWS_H
#define WORKSPEED_WINDOWS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define WINDOWS_MAX 512

struct window {
    uint32_t id;
    pid_t pid;
    char app[128];      // owning app's name, as the window server reports it
    uint64_t space;     // 0 if on no space (minimized, hidden app)
};

// Fills out with the normal app windows on all spaces, front to back,
// skipping menus, panels, invisible and tiny helper windows.
// Returns the number of windows.
size_t windows_get(struct window out[WINDOWS_MAX]);

#endif
