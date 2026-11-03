#ifndef WORKSPEED_APPS_H
#define WORKSPEED_APPS_H

#include "config.h"
#include "windows.h"

#include <stdbool.h>

// Finds the window of app `name` to focus: one on the current space if there
// is one, else the frontmost one on any space. Minimized windows don't count.
// name matches the window owner's name ("Code") or its .app bundle's name
// ("Visual Studio Code"), ignoring case.
bool apps_find(const char *name, struct window *out);

// Switches instantly to the app's space and brings it to the front. spec is
// one name or alternatives in order of preference ("WebStorm | PhpStorm"):
// the first one with a window on a space wins. If one of them is already in
// front, types next_tab into it instead (nothing if next_tab is NULL), keeping
// its modifiers held until apps_release_held. Does
// nothing if none has a window (not running, or only minimized); it never
// launches apps. Blocks until done.
void apps_focus(const char *spec, const struct combo *next_tab);

// Releases the modifiers apps_focus keeps held for a next-tab shortcut. Call
// when Hyper is released and before any other action.
void apps_release_held(void);

// If one of the spec's apps is in front, switches to its next window, in
// Mission Control order (by space, left to right) and wrapping around.
// Otherwise the same as apps_focus without next_tab.
void apps_next_window(const char *spec);

#endif
