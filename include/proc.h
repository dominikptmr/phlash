#ifndef PHLASH_PROC_H
#define PHLASH_PROC_H

#include <stdbool.h>

bool proc_run(const char *path, char *const argv[], bool quiet);

bool proc_spawn(const char *path, char *const argv[]);

#endif
