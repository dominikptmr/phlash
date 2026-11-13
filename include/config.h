#ifndef PHLASH_CONFIG_H
#define PHLASH_CONFIG_H

#include <CoreGraphics/CoreGraphics.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum action_type {
    ACTION_SPACE_BACK,
    ACTION_SPACE_CYCLE,
    ACTION_DESKTOP,
    ACTION_APP,
    ACTION_TAB,
    ACTION_WINDOW,
    ACTION_KEY,
    ACTION_EXEC,
};

struct combo {
    uint16_t keycode;
    CGEventFlags mods;
};

enum tab_mode { TAB_DEFAULT, TAB_CUSTOM, TAB_NONE };

struct binding {
    bool tap;
    bool tap_modifier;
    uint16_t tap_keycodes[2];
    uint16_t keycode;
    CGEventFlags mods;
    enum action_type action;
    bool reverse;
    char *arg;
    enum tab_mode tab_mode;
    struct combo tab;
    struct combo key;
};

struct config {
    struct binding *bindings;
    size_t count;
    struct combo default_tab;
    int tap_ms;
    CGEventFlags app_window_mods;
};

const char *config_default_path(void);

bool config_create_default(const char *path);

struct config *config_load(const char *path);
void config_free(struct config *config);

const struct binding *config_find(const struct config *config, uint16_t keycode,
                                  CGEventFlags mods);

const struct binding *config_find_tap(const struct config *config, CGEventFlags mods);

const struct binding *config_find_modifier_tap(const struct config *config, uint16_t keycode);

const struct combo *config_app_tab(const struct config *config, const struct binding *b);

#endif
