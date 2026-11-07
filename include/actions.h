#ifndef WORKSPEED_ACTIONS_H
#define WORKSPEED_ACTIONS_H

#include "config.h"

// Performs a binding's action. Runs on the work queue, never on the tap thread.
void action_run(const struct config *config, const struct binding *b);

#endif
