#ifndef PHLASH_APPS_H
#define PHLASH_APPS_H

#include "config.h"

#include <stdbool.h>

void apps_focus(const char *spec, const struct combo *next_tab);

// call before other actions and on exit
void apps_release_held(void);

void apps_hyper_released(void);

void apps_mods_changed(CGEventFlags mods);

// until 0 = hold until Hyper is released
void apps_cycle_tab(const struct config *config, bool reverse, CGEventFlags until);

void apps_send_key(const struct combo *c);

void apps_cycle_window(bool reverse);

void apps_next_window(const char *spec);

#endif
