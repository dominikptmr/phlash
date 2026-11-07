#include "actions.h"

#include "apps.h"
#include "proc.h"
#include "switch.h"
#include "tap.h"

void action_run(const struct config *config, const struct binding *b) {
    // Tab and app actions decide themselves whether to keep a tab switcher open.
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
        apps_focus(b->arg, config_app_tab(config, b));
        break;
    case ACTION_TAB: {
        // A tap with option held keeps the switcher open until option is let
        // go; with Hyper + key, until Hyper is. Shift only picks the direction.
        CGEventFlags until = b->tap ? b->mods & TAP_MOD_MASK & ~kCGEventFlagMaskShift : 0;
        apps_cycle_tab(config, b->reverse, until);
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
