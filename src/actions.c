#include "actions.h"

#include "apps.h"
#include "proc.h"
#include "switch.h"

void action_run(const struct binding *b) {
    // App actions decide themselves whether to keep a tab switcher open.
    if (b->action != ACTION_APP) apps_release_held();
    switch (b->action) {
    case ACTION_SPACE:
        switch_to(b->space);
        break;
    case ACTION_SPACE_PREV:
        switch_prev();
        break;
    case ACTION_SPACE_NEXT:
        switch_next();
        break;
    case ACTION_DESKTOP:
        switch_desktop();
        break;
    case ACTION_APP:
        apps_focus(b->arg, b->tab_enabled ? &b->tab : NULL);
        break;
    case ACTION_EXEC: {
        char *const argv[] = {"sh", "-c", b->arg, NULL};
        proc_spawn("/bin/sh", argv);
        break;
    }
    }
}
