#ifndef WORKSPEED_CONFIG_H
#define WORKSPEED_CONFIG_H

#include <CoreGraphics/CoreGraphics.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum action_type {
    ACTION_SPACE,       // switch to space `space`
    ACTION_SPACE_PREV,
    ACTION_SPACE_NEXT,
    ACTION_DESKTOP,     // first space without a fullscreen app
    ACTION_APP,         // focus app `arg` on its space; next tab if already in front
    ACTION_EXEC,        // run `arg` with /bin/sh -c
};

// A key with modifiers, e.g. ctrl+tab.
struct combo {
    uint16_t keycode;
    CGEventFlags mods;
};

struct binding {
    uint16_t keycode;
    CGEventFlags mods;  // modifiers on top of Hyper (TAP_MOD_MASK bits)
    enum action_type action;
    int space;
    char *arg;
    // ACTION_APP: shortcut sent when the app is already in front ("; tab ...").
    bool tab_enabled;
    struct combo tab;
};

struct config {
    struct binding *bindings;
    size_t count;
};

// Default location: ~/.config/workspeed/config. Returned string is static.
const char *config_default_path(void);

// Parses the file at path. Prints every error with its line number to stderr
// and returns NULL if there were any.
struct config *config_load(const char *path);
void config_free(struct config *config);

const struct binding *config_find(const struct config *config, uint16_t keycode,
                                  CGEventFlags mods);

#endif
