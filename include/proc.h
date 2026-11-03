#ifndef WORKSPEED_PROC_H
#define WORKSPEED_PROC_H

#include <stdbool.h>

// Runs path with argv and waits for it. Returns true if it exited with status 0.
// quiet sends the child's stdout to /dev/null.
bool proc_run(const char *path, char *const argv[], bool quiet);

// Starts path with argv without waiting; the child is reaped in the background.
bool proc_spawn(const char *path, char *const argv[]);

#endif
