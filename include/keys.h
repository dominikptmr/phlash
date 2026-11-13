#ifndef PHLASH_KEYS_H
#define PHLASH_KEYS_H

#include <stdbool.h>
#include <stdint.h>

enum {
    KEY_F18 = 0x4F,
};

// call on main thread before parsing config
bool keys_load_layout(void);

const char *keys_name(uint16_t keycode);

int keys_code(const char *name);

bool keys_tap_modifier(const char *name, uint16_t keycodes[2]);
const char *keys_tap_modifier_name(const uint16_t keycodes[2]);

#endif
