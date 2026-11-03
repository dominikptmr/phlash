#ifndef WORKSPEED_CAPSLOCK_H
#define WORKSPEED_CAPSLOCK_H

#include <stdbool.h>

// Remaps caps lock to F18 for all keyboards via hidutil, so it can act as Hyper.
// Other tools that remap caps lock (Hyperkey, Karabiner) conflict with this.
// The mapping lasts until capslock_restore() or the next reboot.
bool capslock_remap(void);
bool capslock_restore(void);

#endif
