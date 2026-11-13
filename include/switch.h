#ifndef PHLASH_SWITCH_H
#define PHLASH_SWITCH_H

#include <stdbool.h>

// 1-based index as --list shows
// current display only
bool switch_to(int index);

bool switch_cycle(bool reverse);

bool switch_desktop(void);

// call regularly on the work queue
void switch_track(void);

bool switch_back(void);

#endif
