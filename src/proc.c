#include "proc.h"

#include <dispatch/dispatch.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

extern char **environ;

bool proc_spawn(const char *path, char *const argv[]) {
    pid_t pid;
    int err = posix_spawn(&pid, path, NULL, NULL, argv, environ);
    if (err != 0) {
        fprintf(stderr, "phlash: cannot run %s: %s\n", path, strerror(err));
        return false;
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        int status;
        waitpid(pid, &status, 0);
    });
    return true;
}
