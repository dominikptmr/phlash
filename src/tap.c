#include "tap.h"

#include "keys.h"

#include <stdio.h>
#include <time.h>

// Releasing Hyper within this time, with no other key pressed in between, is a
// tap. Modifiers don't count as other keys: cmd/option/ctrl + tap are gestures.
static uint64_t tap_ns = 300 * 1000000ULL;

static CFMachPortRef port;
static CFRunLoopSourceRef source;
static tap_hotkey_fn hotkey_fn;
static tap_alone_fn alone_fn;
static tap_release_fn release_fn;
static tap_mods_fn mods_fn;
static tap_modifier_fn modifier_fn;
static void *hotkey_ctx;

static bool debug;
static bool hyper_down;
static bool hyper_used;         // another (non-modifier) key was pressed while Hyper was held
// Our own clock: CGEvent timestamps are mach ticks on Apple silicon, not ns.
static uint64_t hyper_down_at;
// Keys whose key-down we swallowed, so their key-up is swallowed too even if
// Hyper was released in between. Indexed by keycode.
static bool swallowed[128];

static bool modifier_armed;
static uint16_t modifier_key;
static uint64_t modifier_down_at;

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
    if (!hyper_used && clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - hyper_down_at < tap_ns) {
        alone_fn(CGEventGetFlags(event) & TAP_MOD_MASK, hotkey_ctx);
    }
    release_fn(hotkey_ctx);
}

static CGEventFlags modifier_flag(uint16_t keycode) {
    switch (keycode) {
    case 0x36: case 0x37: return kCGEventFlagMaskCommand;
    case 0x3A: case 0x3D: return kCGEventFlagMaskAlternate;
    case 0x3B: case 0x3E: return kCGEventFlagMaskControl;
    case 0x38: case 0x3C: return kCGEventFlagMaskShift;
    case 0x3F: return kCGEventFlagMaskSecondaryFn;
    default: return 0;
    }
}

static void on_modifier(uint16_t keycode, CGEventFlags flags) {
    CGEventFlags flag = modifier_flag(keycode);
    flags &= TAP_MOD_MASK | kCGEventFlagMaskSecondaryFn;
    uint64_t now = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    if (flag && (flags & flag)) {
        modifier_armed = flags == flag && !hyper_down;
        modifier_key = keycode;
        modifier_down_at = now;
        return;
    }
    if (modifier_armed && modifier_key == keycode && flags == 0 &&
        now - modifier_down_at < tap_ns) {
        modifier_fn(keycode, hotkey_ctx);
    }
    modifier_armed = false;
}

static CGEventRef callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *ctx) {
    (void)proxy;
    (void)ctx;

    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        // We may have missed key-ups while disabled.
        hyper_down = false;
        modifier_armed = false;
        for (int i = 0; i < 128; i++) swallowed[i] = false;
        release_fn(hotkey_ctx);
        CGEventTapEnable(port, true);
        if (debug) fprintf(stderr, "tap: re-enabled after being disabled (type %d)\n", type);
        return event;
    }

    // Keys phlash types itself (like an app's next-tab shortcut, sent while
    // Hyper is still held) must reach the app, not be swallowed as a combo.
    if (CGEventGetIntegerValueField(event, kCGEventSourceUserData) == TAP_SYNTHETIC) return event;

    if (type != kCGEventKeyDown && type != kCGEventKeyUp && type != kCGEventFlagsChanged) {
        modifier_armed = false;
        return event;
    }

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
        on_modifier(keycode, CGEventGetFlags(event));
        mods_fn(CGEventGetFlags(event) & TAP_MOD_MASK, hotkey_ctx);
        return event;
    }
    if (type == kCGEventKeyDown) modifier_armed = false;

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

bool tap_start(tap_hotkey_fn fn, tap_alone_fn alone, tap_release_fn release, tap_mods_fn mods,
               tap_modifier_fn modifier, void *ctx) {
    hotkey_fn = fn;
    alone_fn = alone;
    release_fn = release;
    mods_fn = mods;
    modifier_fn = modifier;
    hotkey_ctx = ctx;

    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                       CGEventMaskBit(kCGEventFlagsChanged) |
                       CGEventMaskBit(kCGEventLeftMouseDown) |
                       CGEventMaskBit(kCGEventRightMouseDown) |
                       CGEventMaskBit(kCGEventOtherMouseDown) |
                       CGEventMaskBit(kCGEventScrollWheel);
    port = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                            mask, callback, NULL);
    if (!port) return false;

    source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, port, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CGEventTapEnable(port, true);
    return true;
}

void tap_set_tap_ms(int ms) {
    tap_ns = (uint64_t)ms * 1000000ULL;
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
