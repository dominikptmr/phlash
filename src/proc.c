#include "proc.h"

#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static bool start(const char *path, char *const argv[], bool quiet, pid_t *pid) {
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    if (quiet) posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);

    int err = posix_spawn(pid, path, &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (err != 0) {
        fprintf(stderr, "phlash: cannot run %s: %s\n", path, strerror(err));
        return false;
    }
    return true;
}

bool proc_run(const char *path, char *const argv[], bool quiet) {
    pid_t pid;
    if (!start(path, argv, quiet, &pid)) return false;

    int status;
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "phlash: %s failed\n", path);
        return false;
    }
    return true;
}

bool proc_spawn(const char *path, char *const argv[]) {
    pid_t pid;
    if (!start(path, argv, false, &pid)) return false;

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        int status;
        waitpid(pid, &status, 0);
    });
    return true;
}
