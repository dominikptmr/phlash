#ifndef PHLASH_APPS_H
#define PHLASH_APPS_H

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
// its modifiers held until apps_hyper_released. Does
// nothing if none has a window (not running, or only minimized); it never
// launches apps. Blocks until done.
void apps_focus(const char *spec, const struct combo *next_tab);

// Releases the modifiers kept held for a tab shortcut (see apps_focus and
// apps_cycle_tab). Call before any other action and on exit.
void apps_release_held(void);

// Ends a tab gesture started by an app key (apps_focus): call when Hyper is
// released.
void apps_hyper_released(void);

// Ends a tab gesture started by apps_cycle_tab once its modifier is let go:
// call with the held modifiers whenever they change.
void apps_mods_changed(CGEventFlags mods);

// Next (or previous) tab of whichever app is in front, with that app's own tab
// shortcut from config ("; tab"), else "set tab". The shortcut's modifiers stay
// held until the user releases the modifiers in `until`, or until Hyper is
// released if `until` is 0.
void apps_cycle_tab(const struct config *config, bool reverse, CGEventFlags until);

// Types the shortcut into the app in front.
void apps_send_key(const struct combo *c);

// Next (or previous) window of whichever app is in front, across spaces.
void apps_cycle_window(bool reverse);

// If one of the spec's apps is in front, switches to its next window, in
// Mission Control order (by space, left to right) and wrapping around.
// Otherwise the same as apps_focus without next_tab.
void apps_next_window(const char *spec);

#endif
