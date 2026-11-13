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

struct app_tab {
    char *name;
    bool none;
    struct combo tab;
};

struct binding {
    bool tap;
    bool tap_modifier;
    uint16_t tap_keycodes[2];
    uint16_t keycode;
    CGEventFlags mods;
    enum action_type action;
    bool reverse;
    char *arg;
    char **apps;
    size_t app_count;
    struct combo key;
};

struct config {
    struct binding *bindings;
    size_t count;
    struct combo default_tab;
    struct app_tab *app_tabs;
    size_t app_tab_count;
    int tap_ms;
    CGEventFlags app_window_mods;
};

const char *config_default_path(void);

bool config_create(const char *path);

// empty config with default settings
struct config *config_new(void);

struct config *config_load(const char *path);
void config_free(struct config *config);

const struct binding *config_find(const struct config *config, uint16_t keycode,
                                  CGEventFlags mods);

const struct binding *config_find_tap(const struct config *config, CGEventFlags mods);

const struct binding *config_find_modifier_tap(const struct config *config, uint16_t keycode);

#endif
