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

#include <Carbon/Carbon.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

// main thread only since tap and signals run on the main run loop
static struct config *config;
static const char *config_path;
static dispatch_queue_t work;

static void usage(FILE *out) {
    fprintf(out,
            "usage: phlash [--check] [--list] [--debug]\n"
            "\n"
            "  --check        check the config file and exit\n"
            "  --list         list spaces (* = current) and the apps on them, then exit\n"
            "  --debug        log hotkeys and actions\n"
            "\n"
            "Caps lock becomes Hyper while phlash runs. What Hyper + key and tapping\n"
            "Hyper (alone or with modifiers held) do comes from the config.\n"
            "Send SIGHUP to reload the config.\n");
}

// a running process never sees the grant
// re-exec every 2 s keeping the pid for launchd
static void wait_for_accessibility(char *argv[]) {
    bool asked = getenv("PHLASH_ASKED") != NULL;
    const void *keys[] = {kAXTrustedCheckOptionPrompt};
    const void *values[] = {asked ? kCFBooleanFalse : kCFBooleanTrue};
    CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    bool trusted = AXIsProcessTrustedWithOptions(options);
    CFRelease(options);
    if (trusted) {
        if (asked) fprintf(stderr, "phlash: Accessibility permission granted\n");
        return;
    }

    if (!asked) fprintf(stderr, "phlash: waiting for Accessibility permission (System Settings → "
                                "Privacy & Security → Accessibility)\n");
    setenv("PHLASH_ASKED", "1", 1);
    sleep(2);
    execvp(argv[0], argv);
    perror("phlash: restart failed");
    exit(1);
}

// a second instance would double every hotkey
static bool lock_single_instance(void) {
    char path[PATH_MAX];
    size_t len = confstr(_CS_DARWIN_USER_TEMP_DIR, path, sizeof path);
    if (len == 0 || len > sizeof path) snprintf(path, sizeof path, "/tmp/");
    strlcat(path, "phlash.lock", sizeof path);

    // exec children must not inherit the lock
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) return true;  // can't tell so start anyway
    return flock(fd, LOCK_EX | LOCK_NB) == 0;
}

static void print_key(uint16_t keycode) {
    const char *name = keys_name(keycode);
    if (name) printf("%s", name);
    else printf("0x%02x", keycode);
}

static bool print_mods(CGEventFlags mods, const char *separator) {
    bool any = false;
    for (size_t i = 0; i < keys_modifier_count; i++) {
        const struct modifier *m = &keys_modifiers[i];
        if (!(mods & m->flag & TAP_MOD_MASK)) continue;
        printf("%s%s", any ? separator : "", m->name);
        any = true;
    }
    return any;
}

static void print_combo(const struct combo *c) {
    if (print_mods(c->mods, "+")) printf("+");
    print_key(c->keycode);
}

static void print_binding(const struct binding *b) {
    if (b->tap_modifier) {
        printf("%s : ", keys_tap_modifier_name(b->tap_keycodes));
    } else if (b->tap) {
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
    case ACTION_APP:
        printf("app ");
        for (size_t i = 0; i < b->app_count; i++) printf("%s%s", i ? " | " : "", b->apps[i]);
        printf("\n");
        break;
    case ACTION_EXEC: printf("exec %s\n", b->arg); break;
    }
}

static void print_settings(const struct config *c) {
    printf("set tab ");
    print_combo(&c->default_tab);
    for (size_t i = 0; i < c->app_tab_count; i++) {
        printf("\nset tab ");
        if (c->app_tabs[i].none) printf("none");
        else print_combo(&c->app_tabs[i].tab);
        printf(" for %s", c->app_tabs[i].name);
    }
    printf("\nset tap-ms %d\nset app-window-mod ", c->tap_ms);
    if (!print_mods(c->app_window_mods, "+")) printf("none");
    printf("\n");
}

static void run(const struct config *c, const struct binding *b) {
    dispatch_async(work, ^{
        if (log_debug) print_binding(b);
        action_run(c, b);
    });
}

static void on_hotkey(uint16_t keycode, CGEventFlags mods, void *ctx) {
    (void)ctx;
    // a reload frees the old config after queued work
    // so c stays valid for it
    const struct config *c = config;
    const struct binding *b = config_find(c, keycode, mods);

    const struct binding *app = NULL;
    if (!b && c->app_window_mods && (mods & TAP_MOD_MASK) == c->app_window_mods) {
        app = config_find(c, keycode, 0);
        if (app && app->action != ACTION_APP) app = NULL;
    }

    debugf("hotkey: key 0x%02x mods 0x%08llx -> %s", keycode, (unsigned long long)mods,
           b ? "bound" : app ? "next window" : "no binding");
    if (b) run(c, b);
    else if (app) dispatch_async(work, ^{ apps_next_window(app); });
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

static void on_tap(CGEventFlags mods, void *ctx) {
    (void)ctx;
    const struct config *c = config;  // see on_hotkey
    const struct binding *b = config_find_tap(c, mods);
    debugf("hotkey: hyper tapped, mods 0x%08llx -> %s", (unsigned long long)mods,
           b ? "bound" : "no binding");
    if (b) run(c, b);
}

static void on_modifier_tap(uint16_t keycode, void *ctx) {
    (void)ctx;
    const struct config *c = config;
    const struct binding *b = config_find_modifier_tap(c, keycode);
    debugf("hotkey: modifier 0x%02x tapped -> %s", keycode, b ? "bound" : "no binding");
    if (b) run(c, b);
}

// outside space changes aren't announced so poll
#define TRACK_INTERVAL_MS 100

static void start_tracking_spaces(void) {
    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, work);
    dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, TRACK_INTERVAL_MS * NSEC_PER_MSEC,
                              TRACK_INTERVAL_MS / 2 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(timer, ^{ switch_track(); });
    dispatch_resume(timer);
}

static void reload_config(void) {
    load_layout();
    struct config *fresh = config_load(config_path);
    if (!fresh) {
        fprintf(stderr, "phlash: keeping the previous config\n");
        return;
    }
    struct config *old = config;
    config = fresh;
    tap_set_tap_ms(config->tap_ms);
    // free after queued actions
    dispatch_async(work, ^{ config_free(old); });
    printf("phlash: reloaded %s (%zu bindings)\n", config_path, config->count);
}

static struct stat config_stat;

// stat also catches editors saving by rename and symlinked configs
static bool config_changed(void) {
    struct stat st = {0};
    stat(config_path, &st);
    bool changed = st.st_ino != config_stat.st_ino || st.st_size != config_stat.st_size ||
                   st.st_mtimespec.tv_sec != config_stat.st_mtimespec.tv_sec ||
                   st.st_mtimespec.tv_nsec != config_stat.st_mtimespec.tv_nsec;
    config_stat = st;
    return changed;
}

static void start_watching(void) {
    config_changed();
    dispatch_source_t timer =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), NSEC_PER_SEC,
                              NSEC_PER_SEC / 2);
    dispatch_source_set_event_handler(timer, ^{
        if (config_changed()) reload_config();
        if (capslock_check()) printf("phlash: caps lock mapping was reset, restored it\n");
    });
    dispatch_resume(timer);
}

// key names depend on the layout
static void on_layout_changed(CFNotificationCenterRef center, void *observer, CFStringRef name,
                              const void *object, CFDictionaryRef info) {
    (void)center;
    (void)observer;
    (void)name;
    (void)object;
    (void)info;
    reload_config();
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
        {"check", no_argument, NULL, 'k'},
        {"list", no_argument, NULL, 'l'},
        {"debug", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    bool check_only = false;
    config_path = config_default_path();
    int opt;
    while ((opt = getopt_long(argc, argv, "kldh", options, NULL)) != -1) {
        switch (opt) {
        case 'k': check_only = true; break;
        case 'l': list_print(); return 0;
        case 'd': log_debug = true; break;
        case 'h': usage(stdout); return 0;
        default: usage(stderr); return 2;
        }
    }

    setvbuf(stdout, NULL, _IOLBF, 0);

    if (access(config_path, F_OK) != 0 && errno == ENOENT && config_create(config_path)) {
        printf("phlash: created empty %s\n", config_path);
    }

    load_layout();
    config = config_load(config_path);
    if (check_only) {
        if (!config) return 1;
        print_settings(config);
        for (size_t i = 0; i < config->count; i++) print_binding(&config->bindings[i]);
        printf("phlash: %s is valid (%zu bindings)\n", config_path, config->count);
        config_free(config);
        return 0;
    }

    if (!lock_single_instance()) {
        // exit 0 so launchd doesn't restart us
        fprintf(stderr, "phlash: already running\n");
        config_free(config);
        return 0;
    }
    // exiting would make launchd restart us in a loop
    if (!config) {
        fprintf(stderr, "phlash: running without bindings until the config is fixed\n");
        config = config_new();
    }
    wait_for_accessibility(argv);

    work = dispatch_queue_create("phlash.work", DISPATCH_QUEUE_SERIAL);
    tap_set_tap_ms(config->tap_ms);
    if (!tap_start(on_hotkey, on_tap, on_release, on_mods, on_modifier_tap, NULL)) {
        fprintf(stderr, "phlash: cannot create event tap\n");
        return 1;
    }
    if (!capslock_remap()) {
        tap_stop();
        return 1;
    }
    // first window server query takes ~70 ms so warm up now
    spaces_active();
    start_tracking_spaces();
    start_watching();
    CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(), NULL,
                                    on_layout_changed,
                                    kTISNotifySelectedKeyboardInputSourceChanged, NULL,
                                    CFNotificationSuspensionBehaviorDeliverImmediately);

    on_signal(SIGINT, stop);
    on_signal(SIGTERM, stop);
    on_signal(SIGHUP, reload_config);

    printf("phlash: running with %s (%zu bindings)\n", config_path, config->count);
    CFRunLoopRun();

    capslock_restore();
    tap_stop();
    // don't leave tab modifiers held
    dispatch_sync(work, ^{ apps_release_held(); });
    printf("phlash: stopped\n");
    return 0;
}
