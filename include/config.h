#ifndef WORKSPEED_CONFIG_H
#define WORKSPEED_CONFIG_H

#include <CoreGraphics/CoreGraphics.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum action_type {
    ACTION_SPACE_BACK,  // the last used space
    ACTION_SPACE_CYCLE, // next space, wrapping around; previous if `reverse`
    ACTION_DESKTOP,     // first space without a fullscreen app
    ACTION_APP,         // focus app `arg` on its space; next tab if already in front
    ACTION_TAB,         // next tab of the app in front; previous if `reverse`
    ACTION_WINDOW,      // next window of the app in front; previous if `reverse`
    ACTION_KEY,         // type `key` into the app in front
    ACTION_EXEC,        // run `arg` with /bin/sh -c
};

// A key with modifiers, e.g. ctrl+tab.
struct combo {
    uint16_t keycode;
    CGEventFlags mods;
};

// An app binding's next-tab shortcut: the config's "set tab", its own
// "; tab <combo>", or "; tab none".
enum tab_mode { TAB_DEFAULT, TAB_CUSTOM, TAB_NONE };

struct binding {
    bool tap;           // Hyper tapped with `mods` held, instead of Hyper + keycode
    uint16_t keycode;
    CGEventFlags mods;  // modifiers with Hyper (TAP_MOD_MASK bits)
    enum action_type action;
    bool reverse;
    char *arg;
    enum tab_mode tab_mode;  // ACTION_APP
    struct combo tab;        // TAB_CUSTOM
    struct combo key;        // ACTION_KEY
};

struct config {
    struct binding *bindings;
    size_t count;
    struct combo default_tab;      // "set tab", ctrl+tab if not set
    int tap_ms;                    // "set tap-ms", 300 if not set
    CGEventFlags app_window_mods;  // "set app-window-mod"; 0 = off (the default)
};

// Default location: ~/.config/workspeed/config. Returned string is static.
const char *config_default_path(void);

// Parses the file at path. Prints every error with its line number to stderr
// and returns NULL if there were any.
struct config *config_load(const char *path);
void config_free(struct config *config);

// The key binding for Hyper + keycode with mods, or NULL.
const struct binding *config_find(const struct config *config, uint16_t keycode,
                                  CGEventFlags mods);

// The binding for tapping Hyper with mods held, or NULL.
const struct binding *config_find_tap(const struct config *config, CGEventFlags mods);

// The next-tab shortcut of an app binding, or NULL for "; tab none".
const struct combo *config_app_tab(const struct config *config, const struct binding *b);

#endif
