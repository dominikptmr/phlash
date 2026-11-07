#include "apps.h"

#include "cgs_private.h"
#include "log.h"
#include "spaces.h"
#include "switch.h"
#include "tap.h"

#include <ApplicationServices/ApplicationServices.h>
#include <libproc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_NAMES 16

// The window list, refreshed by every lookup. Only used from the serial work
// queue (or once from main), so one buffer is enough and keeps 80 KB off the
// queue's stack.
static struct window windows[WINDOWS_MAX];
static size_t window_count;

static void refresh_windows(void) {
    window_count = windows_get(windows);
}

// owner is the window server's name for the process ("Code"); the .app
// bundle's name ("Visual Studio Code") matches too.
static bool name_matches(pid_t pid, const char *owner, const char *name) {
    if (owner && strcasecmp(owner, name) == 0) return true;

    // ".../Visual Studio Code.app/Contents/MacOS/Electron" -> "Visual Studio Code"
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof path) <= 0) return false;
    char *bundle_end = strstr(path, ".app/");
    if (!bundle_end) return false;
    *bundle_end = '\0';
    char *slash = strrchr(path, '/');
    return strcasecmp(slash ? slash + 1 : path, name) == 0;
}

static bool pid_matches(pid_t pid, const char *name) {
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].pid == pid) return name_matches(pid, windows[i].app, name);
    }
    return name_matches(pid, NULL, name);
}

// In the current window list: the app's window on the active space if it has
// one, else its frontmost one on any space.
static const struct window *find_window(const char *name) {
    uint64_t active = spaces_active();
    const struct window *found = NULL;
    for (size_t i = 0; i < window_count; i++) {
        const struct window *w = &windows[i];
        if (w->space == 0 || !name_matches(w->pid, w->app, name)) continue;
        if (w->space == active) return w;
        if (!found) found = w;
    }
    return found;
}

bool apps_find(const char *name, struct window *out) {
    refresh_windows();
    const struct window *w = find_window(name);
    if (w) *out = *w;
    return w != NULL;
}

// "WebStorm | PhpStorm | Visual Studio Code" -> one name per entry, in buf.
static size_t split_names(const char *spec, char *buf, size_t size, char *names[MAX_NAMES]) {
    strlcpy(buf, spec, size);
    size_t count = 0;
    for (char *rest = buf, *name; count < MAX_NAMES && (name = strsep(&rest, "|"));) {
        while (*name == ' ' || *name == '\t') name++;
        char *end = name + strlen(name);
        while (end > name && (end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';
        if (*name) names[count++] = name;
    }
    return count;
}

static pid_t frontmost_pid(void) {
    AXUIElementRef system = AXUIElementCreateSystemWide();
    CFTypeRef app = NULL;
    pid_t pid = 0;
    if (AXUIElementCopyAttributeValue(system, kAXFocusedApplicationAttribute, &app) ==
            kAXErrorSuccess && app) {
        AXUIElementGetPid((AXUIElementRef)app, &pid);
        CFRelease(app);
    }
    CFRelease(system);
    return pid;
}

// The entry of names[] that is the app with this pid, or NULL.
static const char *front_name_for(pid_t pid, char *names[], size_t count, pid_t *pid_out) {
    for (size_t i = 0; i < count; i++) {
        if (pid_matches(pid, names[i])) {
            *pid_out = pid;
            return names[i];
        }
    }
    return NULL;
}

// The entry of names[] that is the app in front, or NULL.
static const char *front_name(char *names[], size_t count, pid_t *pid_out) {
    pid_t pid = frontmost_pid();
    return pid ? front_name_for(pid, names, count, pid_out) : NULL;
}

static bool activate(pid_t pid) {
    AXUIElementRef app = AXUIElementCreateApplication(pid);
    if (!app) return false;
    AXError err = AXUIElementSetAttributeValue(app, kAXFrontmostAttribute, kCFBooleanTrue);
    CFRelease(app);
    return err == kAXErrorSuccess;
}

static void switch_to_space_of(uint64_t space) {
    if (space == spaces_active()) return;
    struct space spaces[SPACES_MAX];
    size_t count = spaces_get(spaces);
    int index = spaces_index(spaces, count, space);
    // Unknown index: activating still gets there, just with the animation.
    if (index) switch_to(index);
}

// --- next tab ---------------------------------------------------------------

// The tab shortcut's modifiers (all but shift) stay pressed for the app until
// the gesture ends, like holding option during option+tab: apps that show a tab
// switcher while ctrl is down (Zen, Firefox) keep it open across presses and
// pick the tab when it's released. The gesture ends when Hyper is released
// (hold_until 0), or when the user lets go of the modifiers in hold_until
// (option for option + caps lock taps).
static CGEventFlags held_mods;
static CGEventFlags hold_until;

static const struct {
    CGEventFlags flag;
    uint16_t keycode;   // left-hand modifier key
} modifier_keys[] = {
    {kCGEventFlagMaskControl, 0x3B},
    {kCGEventFlagMaskAlternate, 0x3A},
    {kCGEventFlagMaskShift, 0x38},
    {kCGEventFlagMaskCommand, 0x37},
};
#define MODIFIER_KEY_COUNT (sizeof modifier_keys / sizeof modifier_keys[0])

// Posts a key event marked so our own tap lets it through.
static void post_key(uint16_t keycode, bool down, CGEventFlags flags, bool modifier) {
    CGEventRef e = CGEventCreateKeyboardEvent(NULL, keycode, down);
    if (!e) return;
    if (modifier) CGEventSetType(e, kCGEventFlagsChanged);
    CGEventSetFlags(e, flags);
    CGEventSetIntegerValueField(e, kCGEventSourceUserData, TAP_SYNTHETIC);
    CGEventPost(kCGSessionEventTap, e);
    CFRelease(e);
}

void apps_send_key(const struct combo *c) {
    post_key(c->keycode, true, c->mods, false);
    post_key(c->keycode, false, c->mods, false);
}

void apps_release_held(void) {
    for (size_t i = MODIFIER_KEY_COUNT; i-- > 0;) {
        if (!(held_mods & modifier_keys[i].flag)) continue;
        held_mods &= ~modifier_keys[i].flag;
        post_key(modifier_keys[i].keycode, false, held_mods, true);
    }
    hold_until = 0;
}

void apps_hyper_released(void) {
    if (held_mods && !hold_until) apps_release_held();
}

void apps_mods_changed(CGEventFlags mods) {
    if (held_mods && hold_until && !(mods & hold_until)) apps_release_held();
}

// Types the shortcut into the app in front, leaving its modifiers held until
// the gesture ends (see hold_until). Shift only goes on the key itself, so
// going backwards and forwards keeps the switcher open.
static void send_tab(const struct combo *c, CGEventFlags until) {
    CGEventFlags hold = c->mods & ~kCGEventFlagMaskShift;
    if (held_mods != hold) apps_release_held();
    for (size_t i = 0; i < MODIFIER_KEY_COUNT; i++) {
        if (!(hold & modifier_keys[i].flag) || (held_mods & modifier_keys[i].flag)) continue;
        held_mods |= modifier_keys[i].flag;
        post_key(modifier_keys[i].keycode, true, held_mods, true);
    }
    hold_until = until;
    CGEventFlags flags = held_mods | (c->mods & kCGEventFlagMaskShift);
    post_key(c->keycode, true, flags, false);
    post_key(c->keycode, false, flags, false);
}

// The previous-tab version of a next-tab shortcut: ctrl+right -> ctrl+left,
// ctrl+tab -> ctrl+shift+tab.
static struct combo reversed(const struct combo *c) {
    enum { LEFT = 0x7B, RIGHT = 0x7C, DOWN = 0x7D, UP = 0x7E };
    struct combo r = *c;
    switch (c->keycode) {
    case LEFT: r.keycode = RIGHT; break;
    case RIGHT: r.keycode = LEFT; break;
    case DOWN: r.keycode = UP; break;
    case UP: r.keycode = DOWN; break;
    default: r.mods ^= kCGEventFlagMaskShift; break;
    }
    return r;
}

void apps_cycle_tab(const struct config *config, bool reverse, CGEventFlags until) {
    refresh_windows();
    pid_t pid = frontmost_pid();
    if (!pid) return;

    // The app's own binding knows its tab shortcut; others get "set tab".
    const struct combo *tab = &config->default_tab;
    for (size_t i = 0; i < config->count; i++) {
        const struct binding *b = &config->bindings[i];
        if (b->action != ACTION_APP) continue;
        char buf[512];
        char *names[MAX_NAMES];
        size_t count = split_names(b->arg, buf, sizeof buf, names);
        pid_t matched;
        if (!front_name_for(pid, names, count, &matched)) continue;
        tab = config_app_tab(config, b);
        if (!tab) {
            debugf("app: tab cycling is off for %s", b->arg);
            return;
        }
        break;
    }
    struct combo combo = reverse ? reversed(tab) : *tab;
    debugf("app: %s tab in pid %d", reverse ? "previous" : "next", pid);
    send_tab(&combo, until);
}

void apps_focus(const char *spec, const struct combo *next_tab) {
    char buf[512];
    char *names[MAX_NAMES];
    size_t count = split_names(spec, buf, sizeof buf, names);
    refresh_windows();

    pid_t pid;
    const char *front = front_name(names, count, &pid);
    if (front) {
        if (next_tab) {
            debugf("app: %s is in front, next tab", front);
            send_tab(next_tab, 0);  // held while Hyper is
        }
        return;
    }
    apps_release_held();  // leaving this app: let its tab switcher go first

    for (size_t i = 0; i < count; i++) {
        const struct window *found = find_window(names[i]);
        if (!found) continue;
        struct window w = *found;
        debugf("app: %s has a window on space %llu (pid %d), focusing", names[i],
               (unsigned long long)w.space, w.pid);
        switch_to_space_of(w.space);
        if (!activate(w.pid)) fprintf(stderr, "workspeed: cannot activate %s\n", names[i]);
        return;
    }
    // Apps that aren't open, or only minimized, are left alone for now.
    debugf("app: no window of %s on any space, doing nothing", spec);
}

// --- window cycling ---------------------------------------------------------

struct ordered {
    const struct window *w;
    int space_index;
};

// Mission Control order: by space, then by window ID within a space.
static int compare_ordered(const void *a, const void *b) {
    const struct ordered *x = a, *y = b;
    if (x->space_index != y->space_index) return x->space_index < y->space_index ? -1 : 1;
    return x->w->id < y->w->id ? -1 : x->w->id > y->w->id;
}

// The window server ID of the app's focused window, or 0.
static uint32_t focused_window_id(pid_t pid) {
    AXUIElementRef app = AXUIElementCreateApplication(pid);
    CFTypeRef window = NULL;
    uint32_t id = 0;
    if (AXUIElementCopyAttributeValue(app, kAXFocusedWindowAttribute, &window) ==
            kAXErrorSuccess && window) {
        if (_AXUIElementGetWindow((AXUIElementRef)window, &id) != kAXErrorSuccess) id = 0;
        CFRelease(window);
    }
    CFRelease(app);
    return id;
}

// Brings one specific window of the app to the front. The app's Accessibility
// window list only covers the current space, so switch there first.
static void raise_window(pid_t pid, uint32_t id) {
    AXUIElementRef app = AXUIElementCreateApplication(pid);
    CFTypeRef list = NULL;
    if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, &list) == kAXErrorSuccess && list) {
        if (CFGetTypeID(list) == CFArrayGetTypeID()) {
            for (CFIndex i = 0; i < CFArrayGetCount(list); i++) {
                AXUIElementRef window = (AXUIElementRef)CFArrayGetValueAtIndex(list, i);
                uint32_t window_id = 0;
                if (_AXUIElementGetWindow(window, &window_id) != kAXErrorSuccess ||
                    window_id != id) {
                    continue;
                }
                AXUIElementSetAttributeValue(window, kAXMainAttribute, kCFBooleanTrue);
                AXUIElementPerformAction(window, kAXRaiseAction);
                break;
            }
        }
        CFRelease(list);
    }
    CFRelease(app);
    activate(pid);
}

// Switches to the app's next (or previous) window in Mission Control order:
// by space, left to right, then by window ID, wrapping around.
static void cycle_windows(pid_t pid, const char *label, bool reverse) {
    struct space spaces[SPACES_MAX];
    size_t space_count = spaces_get(spaces);
    struct ordered list[WINDOWS_MAX];
    size_t n = 0;
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].pid != pid || windows[i].space == 0) continue;
        list[n].w = &windows[i];
        list[n].space_index = spaces_index(spaces, space_count, windows[i].space);
        n++;
    }
    if (n < 2) {
        debugf("app: %s has no other window", label);
        return;
    }
    qsort(list, n, sizeof list[0], compare_ordered);

    // Step from the focused window. Without a known focused window, the first
    // one on the active space counts as current.
    uint32_t current = focused_window_id(pid);
    uint64_t active = spaces_active();
    size_t at = n;
    for (size_t i = 0; i < n && at == n; i++) {
        if (current ? list[i].w->id == current : list[i].w->space == active) at = i;
    }
    if (at == n) at = 0;
    struct window next = *list[(reverse ? at + n - 1 : at + 1) % n].w;

    debugf("app: %s window %u -> %u on space %llu", label, current, next.id,
           (unsigned long long)next.space);
    switch_to_space_of(next.space);
    raise_window(next.pid, next.id);
}

void apps_next_window(const char *spec) {
    apps_release_held();
    char buf[512];
    char *names[MAX_NAMES];
    size_t count = split_names(spec, buf, sizeof buf, names);
    refresh_windows();

    pid_t pid;
    const char *front = front_name(names, count, &pid);
    if (!front) {
        apps_focus(spec, NULL);  // not in front yet: just go there
        return;
    }
    cycle_windows(pid, front, false);
}

void apps_cycle_window(bool reverse) {
    apps_release_held();
    refresh_windows();
    pid_t pid = frontmost_pid();
    if (pid) cycle_windows(pid, "app in front", reverse);
}
