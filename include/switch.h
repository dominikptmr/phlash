#ifndef PHLASH_SWITCH_H
#define PHLASH_SWITCH_H

#include <stdbool.h>

// Instant (unanimated) space switching. Indexes are 1-based, in Mission
// Control's left-to-right order, fullscreen spaces included, as --list shows.
// Only within the current display. Blocks until the switch has landed.
bool switch_to(int index);

// Next (or previous) space on the current display, wrapping around at the ends.
bool switch_cycle(bool reverse);

// Switches to the first space on the current display without a fullscreen app.
bool switch_desktop(void);

// Notes the active space, so switch_back knows where we came from. Call it
// regularly (space changes from Mission Control or the trackpad aren't
// announced to us), on the same queue as the switches.
void switch_track(void);

// Switches to the space that was active before the current one.
bool switch_back(void);

#endif
