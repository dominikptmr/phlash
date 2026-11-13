#include "list.h"

#include "spaces.h"
#include "windows.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void print_apps(const struct window *windows, size_t count, uint64_t space_id) {
    bool first = true;
    for (size_t i = 0; i < count; i++) {
        if (windows[i].space != space_id) continue;

        bool seen = false;
        for (size_t j = 0; j < i && !seen; j++) {
            seen = windows[j].space == space_id && strcmp(windows[j].app, windows[i].app) == 0;
        }
        if (seen) continue;

        printf("%s%s", first ? "" : ", ", windows[i].app);
        first = false;
    }
    if (first) printf("-");
    printf("\n");
}

void list_print(void) {
    struct space spaces[SPACES_MAX];
    size_t space_count = spaces_get(spaces);
    if (space_count == 0) {
        fprintf(stderr, "phlash: cannot read the space list\n");
        return;
    }
    static struct window windows[WINDOWS_MAX];
    size_t window_count = windows_get(windows);
    uint64_t active = spaces_active();

    int display = -1;
    for (size_t i = 0; i < space_count; i++) {
        const struct space *s = &spaces[i];
        if (s->display != display) {
            display = s->display;
            printf("display %d\n", display + 1);
        }
        printf("  %2zu %c %-10s  ", i + 1, s->id == active ? '*' : ' ',
               s->fullscreen ? "fullscreen" : "desktop");
        print_apps(windows, window_count, s->id);
    }

    printf("  no space      ");
    print_apps(windows, window_count, 0);
}
