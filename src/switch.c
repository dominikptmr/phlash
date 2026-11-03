#include "switch.h"

#include "log.h"
#include "spaces.h"

#include <ISS.h>

#include <stdio.h>
#include <time.h>

// How long to wait for a switch to land before giving up on confirming it.
#define SETTLE_TIMEOUT_MS 250
#define SETTLE_POLL_MS 2

// ISS posts the swipe and returns; it lands a few ms later. Anything done in
// between (like activating an app, or computing the next switch from the
// current space) would see the old space, so wait for the new one.
static void wait_for_space(uint64_t id) {
    struct timespec poll = {0, SETTLE_POLL_MS * 1000000L};
    for (int waited = 0; waited < SETTLE_TIMEOUT_MS; waited += SETTLE_POLL_MS) {
        if (spaces_active() == id) return;
        nanosleep(&poll, NULL);
    }
}

// The active space and the one before it, for switch_back. Only touched on
// the work queue, like everything else here, so a switch in progress (and the
// spaces a multi-space swipe passes through) is never recorded halfway.
static uint64_t current_id;
static uint64_t previous_id;

void switch_track(void) {
    uint64_t active = spaces_active();
    if (!active || active == current_id) return;
    if (current_id) previous_id = current_id;
    current_id = active;
    debugf("space: now %llu, last %llu", (unsigned long long)current_id,
           (unsigned long long)previous_id);
}

bool switch_to(int index) {
    struct space spaces[SPACES_MAX];
    size_t count = spaces_get(spaces);
    if (count == 0) {
        fprintf(stderr, "workspeed: cannot read the space list\n");
        return false;
    }
    if (index < 1 || (size_t)index > count) {
        printf("workspeed: space %d doesn't exist (%zu spaces)\n", index, count);
        return false;
    }

    switch_track();
    int current = spaces_index(spaces, count, spaces_active());
    if (current == index) return true;
    if (current == 0) {
        fprintf(stderr, "workspeed: cannot find the current space\n");
        return false;
    }
    const struct space *target = &spaces[index - 1];
    if (target->display != spaces[current - 1].display) {
        printf("workspeed: space %d is on another display\n", index);
        return false;
    }

    // ISS counts 0-based within the display under the pointer.
    unsigned int local = 0;
    for (int i = 0; i < index - 1; i++) {
        if (spaces[i].display == target->display) local++;
    }
    if (!iss_switch_to_index(local)) {
        fprintf(stderr, "workspeed: switching to space %d failed\n", index);
        return false;
    }
    wait_for_space(target->id);
    switch_track();
    return true;
}

// Moves one space within the current display; does nothing at either end.
static bool switch_by(int delta) {
    struct space spaces[SPACES_MAX];
    size_t count = spaces_get(spaces);
    int current = spaces_index(spaces, count, spaces_active());
    if (current == 0) return false;

    int target = current + delta;
    if (target < 1 || (size_t)target > count) return false;
    if (spaces[target - 1].display != spaces[current - 1].display) return false;
    return switch_to(target);
}

bool switch_prev(void) {
    return switch_by(-1);
}

bool switch_next(void) {
    return switch_by(1);
}

bool switch_desktop(void) {
    struct space spaces[SPACES_MAX];
    size_t count = spaces_get(spaces);
    int current = spaces_index(spaces, count, spaces_active());
    if (current == 0) return false;

    for (size_t i = 0; i < count; i++) {
        if (spaces[i].display == spaces[current - 1].display && !spaces[i].fullscreen) {
            return switch_to((int)i + 1);
        }
    }
    printf("workspeed: no desktop space on this display\n");
    return false;
}

bool switch_back(void) {
    switch_track();
    if (!previous_id) return false;

    struct space spaces[SPACES_MAX];
    size_t count = spaces_get(spaces);
    int index = spaces_index(spaces, count, previous_id);
    if (!index) {
        // Its fullscreen app was closed, or it was removed in Mission Control.
        printf("workspeed: the last used space is gone\n");
        previous_id = 0;
        return false;
    }
    return switch_to(index);
}
