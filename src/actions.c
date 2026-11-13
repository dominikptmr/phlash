#include "actions.h"

#include "apps.h"
#include "proc.h"
#include "switch.h"
#include "tap.h"

void action_run(const struct config *config, const struct binding *b) {
    // tab and app actions manage the tab switcher themselves
    if (b->action != ACTION_APP && b->action != ACTION_TAB) apps_release_held();

    switch (b->action) {
    case ACTION_SPACE_BACK:
        switch_back();
        break;
    case ACTION_SPACE_CYCLE:
        switch_cycle(b->reverse);
        break;
    case ACTION_DESKTOP:
        switch_desktop();
        break;
    case ACTION_APP:
        apps_focus(config, b->arg);
        break;
    case ACTION_TAB: {
        // shift only sets direction
        CGEventFlags until = b->tap ? b->mods & TAP_MOD_MASK & ~kCGEventFlagMaskShift : 0;
        apps_cycle_tab(config, b->reverse, until);
        if (b->tap_modifier) apps_release_held();
        break;
    }
    case ACTION_WINDOW:
        apps_cycle_window(b->reverse);
        break;
    case ACTION_KEY:
        apps_send_key(&b->key);
        break;
    case ACTION_EXEC: {
        char *const argv[] = {"sh", "-c", b->arg, NULL};
        proc_spawn("/bin/sh", argv);
        break;
    }
    }
}
