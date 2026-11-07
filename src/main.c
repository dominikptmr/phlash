#include "actions.h"
#include "apps.h"
#include "capslock.h"
#include "config.h"
#include "keys.h"
#include "list.h"
#include "log.h"
#include "spaces.h"
#include "switch.h"
#include "tap.h"

#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

// Only touched on the main thread: the tap callback and signal handlers both
// run on the main run loop.
static struct config *config;
static const char *config_path;
static dispatch_queue_t work;
static bool debug;

static void usage(FILE *out) {
    fprintf(out,
            "usage: phlash [--config PATH] [--check] [--list] [--debug]\n"
            "\n"
            "  --config PATH  config file (default ~/.config/phlash/config)\n"
            "  --check        check the config file and exit\n"
            "  --list         list spaces (* = current) and the apps on them, then exit\n"
            "  --debug        log every key event and hotkey to stderr/stdout\n"
            "\n"
            "Caps lock becomes Hyper while phlash runs. What Hyper + key and tapping\n"
            "Hyper (alone or with modifiers held) do comes from the config.\n"
            "Send SIGHUP to reload the config.\n");
}

// Asks once (which also adds phlash to the Accessibility list), then waits
// for the grant instead of exiting, so launchd doesn't restart us into a loop
// of prompts.
static void wait_for_accessibility(void) {
    const void *keys[] = {kAXTrustedCheckOptionPrompt};
    const void *values[] = {kCFBooleanTrue};
    CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    bool trusted = AXIsProcessTrustedWithOptions(options);
    CFRelease(options);
    if (trusted) return;

    fprintf(stderr, "phlash: waiting for Accessibility permission (System Settings → "
                    "Privacy & Security → Accessibility)\n");
    while (!AXIsProcessTrusted()) sleep(2);
    fprintf(stderr, "phlash: Accessibility permission granted\n");
}

// Two instances (say the LaunchAgent and one started from CLion) would both
// remap caps lock and run every hotkey twice. The lock is released on exit.
static bool lock_single_instance(void) {
    char path[PATH_MAX];
    size_t len = confstr(_CS_DARWIN_USER_TEMP_DIR, path, sizeof path);
    if (len == 0 || len > sizeof path) snprintf(path, sizeof path, "/tmp/");
    strlcat(path, "phlash.lock", sizeof path);

    // O_CLOEXEC: apps started by exec bindings must not inherit the lock.
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) return true;  // can't tell; don't refuse to start over it
    return flock(fd, LOCK_EX | LOCK_NB) == 0;  // fd stays open while we run
}

static void print_key(uint16_t keycode) {
    const char *name = keys_name(keycode);
    if (name) printf("%s", name);
    else printf("0x%02x", keycode);
}

// Prints the modifiers joined by separator; false if there were none.
static bool print_mods(CGEventFlags mods, const char *separator) {
    static const struct { CGEventFlags flag; const char *name; } names[] = {
        {kCGEventFlagMaskCommand, "cmd"}, {kCGEventFlagMaskAlternate, "opt"},
        {kCGEventFlagMaskControl, "ctrl"}, {kCGEventFlagMaskShift, "shift"},
    };
    bool any = false;
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (!(mods & names[i].flag)) continue;
        printf("%s%s", any ? separator : "", names[i].name);
        any = true;
    }
    return any;
}

static void print_combo(const struct combo *c) {
    if (print_mods(c->mods, "+")) printf("+");
    print_key(c->keycode);
}

static void print_binding(const struct binding *b) {
    if (b->tap) {
        if (print_mods(b->mods, " + ")) printf(" + ");
        printf("hyper : ");
    } else {
        printf("hyper");
        if (b->mods) {
            printf(" + ");
            print_mods(b->mods, " + ");
        }
        printf(" - ");
        print_key(b->keycode);
        printf(" : ");
    }

    const char *dir = b->reverse ? "prev" : "next";
    switch (b->action) {
    case ACTION_SPACE_BACK: printf("space back\n"); break;
    case ACTION_SPACE_CYCLE: printf("space cycle%s\n", b->reverse ? " prev" : ""); break;
    case ACTION_DESKTOP: printf("desktop\n"); break;
    case ACTION_TAB: printf("tab %s\n", dir); break;
    case ACTION_WINDOW: printf("window %s\n", dir); break;
    case ACTION_KEY:
        printf("key ");
        print_combo(&b->key);
        printf("\n");
        break;
    case ACTION_APP: {
        printf("app %s ; tab ", b->arg);
        const struct combo *tab = config_app_tab(config, b);
        if (tab) print_combo(tab);
        else printf("none");
        printf("\n");
        break;
    }
    case ACTION_EXEC: printf("exec %s\n", b->arg); break;
    }
}

static void print_settings(const struct config *c) {
    printf("set tab ");
    print_combo(&c->default_tab);
    printf("\nset tap-ms %d\nset app-window-mod ", c->tap_ms);
    if (!print_mods(c->app_window_mods, "+")) printf("none");
    printf("\n");
}

static void on_hotkey(uint16_t keycode, CGEventFlags mods, void *ctx) {
    (void)ctx;
    // Actions run later on the work queue; a reload frees the old config only
    // after work already queued, so this pointer stays valid for them.
    const struct config *c = config;
    const struct binding *b = config_find(c, keycode, mods);

    // Hyper + "set app-window-mod" + an app's key cycles that app's windows,
    // unless the combo is bound to something itself.
    const struct binding *app = NULL;
    if (!b && c->app_window_mods && (mods & TAP_MOD_MASK) == c->app_window_mods) {
        app = config_find(c, keycode, 0);
        if (app && app->action != ACTION_APP) app = NULL;
    }

    if (debug) {
        fprintf(stderr, "hotkey: key 0x%02x mods 0x%08llx -> %s\n", keycode,
                (unsigned long long)mods, b ? "bound" : app ? "next window" : "no binding");
    }
    if (b) {
        dispatch_async(work, ^{
            if (debug) print_binding(b);
            action_run(c, b);
        });
    } else if (app) {
        dispatch_async(work, ^{ apps_next_window(app->arg); });
    }
}

static void load_layout(void) {
    if (!keys_load_layout()) {
        fprintf(stderr, "phlash: cannot read the keyboard layout, using US key names\n");
    }
}

static void on_release(void *ctx) {
    (void)ctx;
    dispatch_async(work, ^{ apps_hyper_released(); });
}

static void on_mods(CGEventFlags mods, void *ctx) {
    (void)ctx;
    dispatch_async(work, ^{ apps_mods_changed(mods); });
}

// Hyper tapped: runs the tap line for the modifiers held ("cmd + hyper : ...").
static void on_tap(CGEventFlags mods, void *ctx) {
    (void)ctx;
    const struct config *c = config;  // see on_hotkey
    const struct binding *b = config_find_tap(c, mods);
    debugf("hotkey: hyper tapped, mods 0x%08llx -> %s", (unsigned long long)mods,
           b ? "bound" : "no binding");
    if (!b) return;
    dispatch_async(work, ^{
        if (debug) print_binding(b);
        action_run(c, b);
    });
}

// Space changes made outside phlash (trackpad, Mission Control, clicking an
// app) aren't announced to us, so look at the active space a few times a second.
// One cheap window server query; the leeway lets macOS batch the wakeups.
#define TRACK_INTERVAL_MS 100

static void start_tracking_spaces(void) {
    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, work);
    dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, TRACK_INTERVAL_MS * NSEC_PER_MSEC,
                              TRACK_INTERVAL_MS / 2 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(timer, ^{ switch_track(); });
    dispatch_resume(timer);
}

static void reload_config(void) {
    // The layout may have changed since the last load.
    load_layout();
    struct config *fresh = config_load(config_path);
    if (!fresh) {
        fprintf(stderr, "phlash: keeping the previous config\n");
        return;
    }
    struct config *old = config;
    config = fresh;
    tap_set_tap_ms(config->tap_ms);
    // Actions already queued still point into the old config; free it after them.
    dispatch_async(work, ^{ config_free(old); });
    printf("phlash: reloaded %s (%zu bindings)\n", config_path, config->count);
}

static void on_signal(int sig, void (*handler)(void)) {
    signal(sig, SIG_IGN);
    dispatch_source_t src = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, (uintptr_t)sig, 0,
                                                   dispatch_get_main_queue());
    dispatch_source_set_event_handler(src, ^{ handler(); });
    dispatch_resume(src);
}

static void stop(void) {
    CFRunLoopStop(CFRunLoopGetMain());
}

int main(int argc, char *argv[]) {
    static const struct option options[] = {
        {"config", required_argument, NULL, 'c'},
        {"check", no_argument, NULL, 'k'},
        {"list", no_argument, NULL, 'l'},
        {"debug", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    bool check_only = false;
    config_path = config_default_path();
    int opt;
    while ((opt = getopt_long(argc, argv, "c:kldh", options, NULL)) != -1) {
        switch (opt) {
        case 'c': config_path = optarg; break;
        case 'k': check_only = true; break;
        case 'l': list_print(); return 0;
        case 'd': debug = log_debug = true; break;
        case 'h': usage(stdout); return 0;
        default: usage(stderr); return 2;
        }
    }

    setvbuf(stdout, NULL, _IOLBF, 0);

    load_layout();
    config = config_load(config_path);
    if (!config) return 1;
    if (check_only) {
        print_settings(config);
        for (size_t i = 0; i < config->count; i++) print_binding(&config->bindings[i]);
        printf("phlash: %s is valid (%zu bindings)\n", config_path, config->count);
        config_free(config);
        return 0;
    }

    if (!lock_single_instance()) {
        // Exit 0 so the LaunchAgent doesn't keep restarting us while the other runs.
        fprintf(stderr, "phlash: already running\n");
        config_free(config);
        return 0;
    }
    wait_for_accessibility();

    work = dispatch_queue_create("phlash.work", DISPATCH_QUEUE_SERIAL);
    tap_set_debug(debug);
    tap_set_tap_ms(config->tap_ms);
    if (!tap_start(on_hotkey, on_tap, on_release, on_mods, NULL)) {
        fprintf(stderr, "phlash: cannot create event tap\n");
        return 1;
    }
    if (!capslock_remap()) {
        tap_stop();
        return 1;
    }
    // The first window server query takes ~70 ms to connect; pay it now, not on
    // the first hotkey.
    spaces_active();
    start_tracking_spaces();

    on_signal(SIGINT, stop);
    on_signal(SIGTERM, stop);
    on_signal(SIGHUP, reload_config);

    printf("phlash: running with %s (%zu bindings)\n", config_path, config->count);
    CFRunLoopRun();

    capslock_restore();
    tap_stop();
    // Don't leave a tab shortcut's modifier logically held after we're gone.
    dispatch_sync(work, ^{ apps_release_held(); });
    printf("phlash: stopped\n");
    return 0;
}
