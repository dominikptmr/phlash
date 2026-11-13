#ifndef PHLASH_TAP_H
#define PHLASH_TAP_H

#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>
#include <stdint.h>

#define TAP_SYNTHETIC 0x70686c73  // "phls"

#define TAP_MOD_MASK (kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate | \
                      kCGEventFlagMaskControl | kCGEventFlagMaskShift)

// callbacks run on the tap thread and must return fast
// all keyboard input waits on them
typedef void (*tap_hotkey_fn)(uint16_t keycode, CGEventFlags mods);
typedef void (*tap_alone_fn)(CGEventFlags mods);
typedef void (*tap_release_fn)(void);
typedef void (*tap_mods_fn)(CGEventFlags mods);
typedef void (*tap_modifier_fn)(uint16_t keycode);

// needs Accessibility
bool tap_start(tap_hotkey_fn fn, tap_alone_fn alone, tap_release_fn release, tap_mods_fn mods,
               tap_modifier_fn modifier);
void tap_stop(void);

void tap_set_tap_ms(int ms);

#endif
