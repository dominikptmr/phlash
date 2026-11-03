#include "tap.h"

#include "keys.h"

#include <stdio.h>
#include <time.h>

// Releasing Hyper within this time, with nothing pressed in between, is a tap.
#define TAP_NS 300000000ULL

static CFMachPortRef port;
static CFRunLoopSourceRef source;
static tap_hotkey_fn hotkey_fn;
static tap_alone_fn alone_fn;
static tap_release_fn release_fn;
static void *hotkey_ctx;

static bool debug;
static bool hyper_down;
static bool hyper_used;         // another key or modifier was pressed while Hyper was held
// Our own clock: CGEvent timestamps are mach ticks on Apple silicon, not ns.
static uint64_t hyper_down_at;
// Keys whose key-down we swallowed, so their key-up is swallowed too even if
// Hyper was released in between. Indexed by keycode.
static bool swallowed[128];

// Handles F18 (Hyper) itself; it never reaches apps.
static void on_hyper(CGEventRef event, bool down) {
    if (down) {
        if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat)) return;
        hyper_down = true;
        hyper_used = false;
        hyper_down_at = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
        return;
    }
    hyper_down = false;
    if (!hyper_used && clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - hyper_down_at < TAP_NS) {
        alone_fn(hotkey_ctx);
    }
    release_fn(hotkey_ctx);
}

static CGEventRef callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *ctx) {
    (void)proxy;
    (void)ctx;

    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        // We may have missed key-ups while disabled.
        hyper_down = false;
        for (int i = 0; i < 128; i++) swallowed[i] = false;
        release_fn(hotkey_ctx);
        CGEventTapEnable(port, true);
        if (debug) fprintf(stderr, "tap: re-enabled after being disabled (type %d)\n", type);
        return event;
    }

    // Keys workspeed types itself (like an app's next-tab shortcut, sent while
    // Hyper is still held) must reach the app, not be swallowed as a combo.
    if (CGEventGetIntegerValueField(event, kCGEventSourceUserData) == TAP_SYNTHETIC) return event;

    uint16_t keycode = (uint16_t)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
    if (debug) {
        const char *name = keys_name(keycode);
        CGEventFlags flags = CGEventGetFlags(event);
        fprintf(stderr, "tap: %-5s key %-6s (0x%02x) flags 0x%08llx%s%s%s%s  hyper %d\n",
                type == kCGEventKeyDown ? "down" : type == kCGEventKeyUp ? "up" : "flags",
                name ? name : "?", keycode, (unsigned long long)flags,
                flags & kCGEventFlagMaskCommand ? " cmd" : "",
                flags & kCGEventFlagMaskAlternate ? " opt" : "",
                flags & kCGEventFlagMaskControl ? " ctrl" : "",
                flags & kCGEventFlagMaskShift ? " shift" : "",
                hyper_down);
    }

    if (type == kCGEventFlagsChanged) {
        if (hyper_down) hyper_used = true;
        return event;
    }
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) return event;

    bool down = type == kCGEventKeyDown;
    if (keycode == KEY_F18) {
        on_hyper(event, down);
        return NULL;
    }
    if (keycode >= 128) return event;

    if (!down) {
        if (!swallowed[keycode]) return event;
        swallowed[keycode] = false;
        return NULL;
    }

    // Every key pressed with Hyper is swallowed, bound or not: an unbound combo
    // would otherwise reach the app as the plain key.
    if (!hyper_down) return event;
    hyper_used = true;
    swallowed[keycode] = true;

    if (!CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat)) {
        hotkey_fn(keycode, CGEventGetFlags(event) & TAP_MOD_MASK, hotkey_ctx);
    }
    return NULL;
}

bool tap_start(tap_hotkey_fn fn, tap_alone_fn alone, tap_release_fn release, void *ctx) {
    hotkey_fn = fn;
    alone_fn = alone;
    release_fn = release;
    hotkey_ctx = ctx;

    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                       CGEventMaskBit(kCGEventFlagsChanged);
    port = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                            mask, callback, NULL);
    if (!port) return false;

    source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, port, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CGEventTapEnable(port, true);
    return true;
}

void tap_set_debug(bool on) {
    debug = on;
}

void tap_stop(void) {
    if (!port) return;
    CGEventTapEnable(port, false);
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CFRelease(source);
    CFRelease(port);
    source = NULL;
    port = NULL;
}
