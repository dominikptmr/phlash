#include "apps.h"

#include "cgs_private.h"
#include "keys.h"
#include "log.h"
#include "spaces.h"
#include "switch.h"
#include "tap.h"
#include "windows.h"

#include <ApplicationServices/ApplicationServices.h>
#include <libproc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// work queue only so static keeps 80 KB off its stack
static struct window windows[WINDOWS_MAX];
static size_t window_count;

#define BUNDLES_MAX 64

struct bundle {
    pid_t pid;
    char name[128];
    char id[256];
};

// filled on demand and cleared with the window list
static struct bundle bundles[BUNDLES_MAX];
static size_t bundle_count;

static void refresh_windows(void) {
    window_count = windows_get(windows);
    bundle_count = 0;
}

static void read_bundle_id(const char *app_path, char *out, size_t size) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, (const UInt8 *)app_path, (CFIndex)strlen(app_path), true);
    CFBundleRef bundle = url ? CFBundleCreate(kCFAllocatorDefault, url) : NULL;
    CFStringRef id = bundle ? CFBundleGetIdentifier(bundle) : NULL;
    if (!id || !CFStringGetCString(id, out, (CFIndex)size, kCFStringEncodingUTF8)) out[0] = '\0';
    if (bundle) CFRelease(bundle);
    if (url) CFRelease(url);
}

static const struct bundle *bundle_of(pid_t pid) {
    for (size_t i = 0; i < bundle_count; i++) {
        if (bundles[i].pid == pid) return &bundles[i];
    }
    struct bundle *b = &bundles[bundle_count < BUNDLES_MAX ? bundle_count++ : BUNDLES_MAX - 1];
    b->pid = pid;
    b->name[0] = b->id[0] = '\0';

    // ".../Visual Studio Code.app/Contents/MacOS/Electron" -> "Visual Studio Code"
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof path) <= 0) return b;
    char *end = strstr(path, ".app/");
    if (!end) return b;
    end[4] = '\0';
    read_bundle_id(path, b->id, sizeof b->id);
    *end = '\0';
    char *slash = strrchr(path, '/');
    strlcpy(b->name, slash ? slash + 1 : path, sizeof b->name);
    return b;
}

static bool name_matches(pid_t pid, const char *owner, const char *name) {
    if (owner && strcasecmp(owner, name) == 0) return true;
    const struct bundle *b = bundle_of(pid);
    return strcasecmp(b->name, name) == 0 || strcasecmp(b->id, name) == 0;
}

static bool pid_matches(pid_t pid, const char *name) {
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].pid == pid) return name_matches(pid, windows[i].app, name);
    }
    return name_matches(pid, NULL, name);
}

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
    if (pid) return pid;

    // fallback when AX reports no front app
    uint64_t active = spaces_active();
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].space != active) continue;
        debugf("app: no front app from AX, using %s (pid %d)", windows[i].app, windows[i].pid);
        return windows[i].pid;
    }
    return 0;
}

static const char *front_name(char *const names[], size_t count, pid_t *pid_out) {
    *pid_out = frontmost_pid();
    if (!*pid_out) return NULL;
    for (size_t i = 0; i < count; i++) {
        if (pid_matches(*pid_out, names[i])) return names[i];
    }
    return NULL;
}

static const struct combo *tab_for(const struct config *config, pid_t pid) {
    for (size_t i = 0; i < config->app_tab_count; i++) {
        const struct app_tab *t = &config->app_tabs[i];
        if (pid_matches(pid, t->name)) return t->none ? NULL : &t->tab;
    }
    return &config->default_tab;
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
    // unknown index still gets there via activate with animation
    if (index) switch_to(index);
}

// tab shortcut mods except shift stay held until the gesture ends
// keeps tab switchers like Zen and Firefox open across presses
static CGEventFlags held_mods;
static CGEventFlags hold_until;

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
    for (size_t i = keys_modifier_count; i-- > 0;) {
        const struct modifier *m = &keys_modifiers[i];
        if (!(held_mods & m->flag)) continue;
        held_mods &= ~m->flag;
        post_key(m->left, false, held_mods, true);
    }
    hold_until = 0;
}

void apps_hyper_released(void) {
    if (held_mods && !hold_until) apps_release_held();
}

void apps_mods_changed(CGEventFlags mods) {
    if (held_mods && hold_until && !(mods & hold_until)) apps_release_held();
}

// shift only on the key so reversing keeps the switcher open
static void send_tab(const struct combo *c, CGEventFlags until) {
    CGEventFlags hold = c->mods & ~kCGEventFlagMaskShift;
    if (held_mods != hold) apps_release_held();
    for (size_t i = 0; i < keys_modifier_count; i++) {
        const struct modifier *m = &keys_modifiers[i];
        if (!(hold & m->flag) || (held_mods & m->flag)) continue;
        held_mods |= m->flag;
        post_key(m->left, true, held_mods, true);
    }
    hold_until = until;
    CGEventFlags flags = held_mods | (c->mods & kCGEventFlagMaskShift);
    post_key(c->keycode, true, flags, false);
    post_key(c->keycode, false, flags, false);
}

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
    const struct combo *tab = tab_for(config, pid);
    if (!tab) {
        debugf("app: tab cycling is off for pid %d", pid);
        return;
    }
    struct combo combo = reverse ? reversed(tab) : *tab;
    debugf("app: %s tab in pid %d", reverse ? "previous" : "next", pid);
    send_tab(&combo, until);
}

static void focus_first(char *const names[], size_t count) {
    apps_release_held();
    for (size_t i = 0; i < count; i++) {
        const struct window *found = find_window(names[i]);
        if (!found) continue;
        struct window w = *found;
        debugf("app: %s has a window on space %llu (pid %d), focusing", names[i],
               (unsigned long long)w.space, w.pid);
        switch_to_space_of(w.space);
        if (!activate(w.pid)) fprintf(stderr, "phlash: cannot activate %s\n", names[i]);
        return;
    }
    debugf("app: no window of %s or its fallbacks, doing nothing", names[0]);
}

void apps_focus(const struct config *config, const struct binding *b) {
    refresh_windows();
    pid_t pid;
    const char *front = front_name(b->apps, b->app_count, &pid);
    if (!front) {
        focus_first(b->apps, b->app_count);
        return;
    }
    const struct combo *tab = tab_for(config, pid);
    if (tab) {
        debugf("app: %s is in front, next tab", front);
        send_tab(tab, 0);
    }
}

struct ordered {
    const struct window *w;
    int group;
    int space_index;
};

static int compare_ordered(const void *a, const void *b) {
    const struct ordered *x = a, *y = b;
    if (x->group != y->group) return x->group < y->group ? -1 : 1;
    if (x->space_index != y->space_index) return x->space_index < y->space_index ? -1 : 1;
    return x->w->id < y->w->id ? -1 : x->w->id > y->w->id;
}

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

// AX window list only covers the current space so switch first
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

static size_t add_windows(struct ordered *list, size_t n, pid_t pid, const char *name, int group,
                          const struct space *spaces, size_t space_count) {
    for (size_t i = 0; i < window_count && n < WINDOWS_MAX; i++) {
        const struct window *w = &windows[i];
        if (w->space == 0) continue;
        if (name ? !name_matches(w->pid, w->app, name) : w->pid != pid) continue;
        list[n++] = (struct ordered){w, group, spaces_index(spaces, space_count, w->space)};
    }
    return n;
}

static void cycle_windows(struct ordered *list, size_t n, pid_t front_pid, const char *label,
                          bool reverse) {
    if (n < 2) {
        debugf("app: %s has no other window", label);
        return;
    }
    qsort(list, n, sizeof list[0], compare_ordered);

    // focused window lags after a jump
    // so it only picks among windows on the active space
    uint64_t active = spaces_active();
    uint32_t focused = focused_window_id(front_pid);
    size_t at = n;
    for (size_t i = 0; i < n; i++) {
        const struct window *w = list[i].w;
        if (w->pid != front_pid || w->space != active) continue;
        if (at == n || w->id == focused) at = i;
    }
    uint32_t current = at < n ? list[at].w->id : 0;
    if (at == n) at = reverse ? 0 : n - 1;
    struct window next = *list[(reverse ? at + n - 1 : at + 1) % n].w;

    debugf("app: %s window %u -> %u on space %llu", label, current, next.id,
           (unsigned long long)next.space);
    switch_to_space_of(next.space);
    raise_window(next.pid, next.id);
}

void apps_next_window(const struct binding *b) {
    refresh_windows();
    pid_t pid;
    const char *front = front_name(b->apps, b->app_count, &pid);
    if (!front) {
        focus_first(b->apps, b->app_count);
        return;
    }
    apps_release_held();

    struct space spaces[SPACES_MAX];
    size_t space_count = spaces_get(spaces);
    struct ordered list[WINDOWS_MAX];
    size_t n = 0;
    for (size_t i = 0; i < b->app_count; i++) {
        n = add_windows(list, n, 0, b->apps[i], (int)i, spaces, space_count);
    }
    cycle_windows(list, n, pid, b->apps[0], false);
}

void apps_cycle_window(bool reverse) {
    apps_release_held();
    refresh_windows();
    pid_t pid = frontmost_pid();
    if (!pid) return;
    struct space spaces[SPACES_MAX];
    size_t space_count = spaces_get(spaces);
    struct ordered list[WINDOWS_MAX];
    size_t n = add_windows(list, 0, pid, NULL, 0, spaces, space_count);
    cycle_windows(list, n, pid, "app in front", reverse);
}
