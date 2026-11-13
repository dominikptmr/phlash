#ifndef PHLASH_CAPSLOCK_H
#define PHLASH_CAPSLOCK_H

#include <stdbool.h>

// keeps other mappings
// lasts until capslock_restore or reboot
bool capslock_remap(void);

// restores the mapping if something reset it
// true if it had to
bool capslock_check(void);

bool capslock_restore(void);

#endif
