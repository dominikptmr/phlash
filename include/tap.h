#ifndef WORKSPEED_TAP_H
#define WORKSPEED_TAP_H

#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>
#include <stdint.h>

// Marks events workspeed posts itself (in kCGEventSourceUserData); the tap
// lets them through untouched.
#define TAP_SYNTHETIC 0x77737064  // "wspd"

// Modifier flags that can be combined with Hyper.
#define TAP_MOD_MASK (kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate | \
                      kCGEventFlagMaskControl | kCGEventFlagMaskShift)

// Called on the tap's thread for each key pressed while Hyper is held (not for
// auto-repeats). mods holds any modifiers held on top of Hyper. Must return
// quickly, since the whole system's keyboard input waits on it; queue real work
// elsewhere. The key itself never reaches apps, bound or not.
typedef void (*tap_hotkey_fn)(uint16_t keycode, CGEventFlags mods, void *ctx);

// Called on the tap's thread when Hyper is tapped (pressed and released within
// 300 ms, no other key pressed). mods holds the modifiers held at release, so
// cmd + tap and a bare tap can mean different things. Same rules as
// tap_hotkey_fn.
typedef void (*tap_alone_fn)(CGEventFlags mods, void *ctx);

// Called on the tap's thread whenever Hyper is released (after tap_alone_fn
// for a tap), and when the tap had to be re-enabled and may have missed it.
typedef void (*tap_release_fn)(void *ctx);

// Called on the tap's thread whenever the user presses or releases a modifier;
// mods holds the modifiers now held. Same rules as tap_hotkey_fn.
typedef void (*tap_mods_fn)(CGEventFlags mods, void *ctx);

// Hyper is F18 held, which caps lock becomes after capslock_remap().
// Installs the event tap on the current run loop. Needs Accessibility
// permission.
bool tap_start(tap_hotkey_fn fn, tap_alone_fn alone, tap_release_fn release, tap_mods_fn mods,
               void *ctx);
void tap_stop(void);

// How quickly Hyper has to be released to count as a tap (default 300 ms).
void tap_set_tap_ms(int ms);

// Logs every key and modifier event the tap sees to stderr.
void tap_set_debug(bool on);

#endif
