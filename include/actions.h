#ifndef PHLASH_ACTIONS_H
#define PHLASH_ACTIONS_H

#include "config.h"

// runs on the work queue never the tap thread
void action_run(const struct config *config, const struct binding *b);

#endif
