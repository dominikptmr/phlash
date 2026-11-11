#ifndef PHLASH_KEYS_H
#define PHLASH_KEYS_H

#include <stdbool.h>
#include <stdint.h>

// macOS virtual keycodes we refer to by number.
enum {
    KEY_F18 = 0x4F,
};

// Names character keys after what they type in the current keyboard layout
// (German: "y" is the key left of "x", "ö" exists, ";" doesn't). Call on the
// main thread before parsing the config. Without it, or if it fails, names
// follow the US layout.
bool keys_load_layout(void);

// Config-style name for a virtual keycode ("u", "ö", "left", "f5"), or NULL.
const char *keys_name(uint16_t keycode);

// Virtual keycode for a config-style name (case-insensitive), or -1 if unknown.
int keys_code(const char *name);

bool keys_tap_modifier(const char *name, uint16_t keycodes[2]);
const char *keys_tap_modifier_name(const uint16_t keycodes[2]);

#endif
