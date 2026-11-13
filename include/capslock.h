#ifndef PHLASH_CAPSLOCK_H
#define PHLASH_CAPSLOCK_H

#include <stdbool.h>

// lasts until capslock_restore or reboot
bool capslock_remap(void);
bool capslock_restore(void);

#endif
