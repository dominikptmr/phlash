#include "tap.h"

#include "keys.h"
#include "log.h"

#include <time.h>

// modifiers don't count so cmd + tap is its own gesture
static uint64_t tap_ns = 300 * 1000000ULL;

static CFMachPortRef port;
static CFRunLoopSourceRef source;
static tap_hotkey_fn hotkey_fn;
static tap_alone_fn alone_fn;
static tap_release_fn release_fn;
static tap_mods_fn mods_fn;
static tap_modifier_fn modifier_fn;
static void *hotkey_ctx;

static bool hyper_down;
static bool hyper_used;
// CGEvent timestamps are mach ticks on Apple silicon
static uint64_t hyper_down_at;
// swallow key-ups of swallowed key-downs even after Hyper release
static bool swallowed[128];

static bool modifier_armed;
static uint16_t modifier_key;
static uint64_t modifier_down_at;

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

static void on_modifier(uint16_t keycode, CGEventFlags flags) {
    CGEventFlags flag = keys_modifier_flag(keycode);
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
        // key-ups may be missed while disabled
        hyper_down = false;
        modifier_armed = false;
        for (int i = 0; i < 128; i++) swallowed[i] = false;
        release_fn(hotkey_ctx);
        CGEventTapEnable(port, true);
        debugf("tap: re-enabled after being disabled (type %d)", type);
        return event;
    }

    // let our own keys through to the app
    if (CGEventGetIntegerValueField(event, kCGEventSourceUserData) == TAP_SYNTHETIC) return event;

    if (type != kCGEventKeyDown && type != kCGEventKeyUp && type != kCGEventFlagsChanged) {
        modifier_armed = false;
        return event;
    }

    uint16_t keycode = (uint16_t)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

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

    // swallow all Hyper combos so unbound ones don't leak
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

void tap_stop(void) {
    if (!port) return;
    CGEventTapEnable(port, false);
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CFRelease(source);
    CFRelease(port);
    source = NULL;
    port = NULL;
}
