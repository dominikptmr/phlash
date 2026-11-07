#include "config.h"

#include "keys.h"
#include "tap.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// One binding or setting per line, '#' starts a comment line:
//
//   hyper [+ mod ...] - key : action      Hyper + key
//   [mod + ...] hyper [+ mod ...] : action  Hyper tapped with the mods held
//   set <setting> <value>
//
//   mod     cmd | opt | ctrl | shift
//   key     what the key types in the current layout ("y", "ö"), a name from
//           keys.c ("left", "f5") or a hex keycode ("0x29")
//   action  space back | space cycle [prev] | desktop
//           | app <name> [; tab <combo> | ; tab none]
//           | tab next | tab prev | window next | window prev
//           | key <combo> | exec <shell command>
//   setting tab <combo>             next-tab shortcut for apps without "; tab"
//           tap-ms <50-1000>        how quickly a tap has to be released
//           app-window-mod <mods>   Hyper + these + an app's key cycles that
//                                   app's windows ("cmd", "none")
//
// app names match either the name --list shows or the .app bundle's name, so
// both "Code" and "Visual Studio Code" work; quotes around the name are optional.
// "app A | B | C" lists alternatives in order of preference. Pressing an app's
// key while it is already in front sends its next-tab shortcut.
//
// Behavior that has no line doesn't happen: the config defines it all.

#define DEFAULT_TAP_MS 300

const char *config_default_path(void) {
    static char path[PATH_MAX];
    const char *home = getenv("HOME");
    snprintf(path, sizeof path, "%s/.config/workspeed/config", home ? home : "");
    return path;
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

// Splits off the first word of s; returns the trimmed rest.
static char *split_word(char *s) {
    char *rest = s;
    while (*rest && !isspace((unsigned char)*rest)) rest++;
    if (*rest) *rest++ = '\0';
    return trim(rest);
}

static int parse_key(const char *s) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && s[2]) {
        char *end;
        long code = strtol(s, &end, 16);
        return *end == '\0' && code >= 0 && code < 128 ? (int)code : -1;
    }
    return keys_code(s);
}

static bool parse_mod(const char *s, CGEventFlags *mods) {
    if (strcmp(s, "cmd") == 0) *mods |= kCGEventFlagMaskCommand;
    else if (strcmp(s, "opt") == 0 || strcmp(s, "alt") == 0) *mods |= kCGEventFlagMaskAlternate;
    else if (strcmp(s, "ctrl") == 0) *mods |= kCGEventFlagMaskControl;
    else if (strcmp(s, "shift") == 0) *mods |= kCGEventFlagMaskShift;
    else return false;
    return true;
}

// Parses "hyper [+ mod ...] - key". Modifies s.
static const char *parse_key_trigger(char *s, struct binding *b) {
    // The key is the last word, so "hyper - -" binds the minus key.
    char *key = s + strlen(s);
    while (key > s && !isspace((unsigned char)key[-1])) key--;
    if (key == s) return "expected 'hyper - key'";
    int code = parse_key(key);
    if (code < 0) return "unknown key";
    b->keycode = (uint16_t)code;

    key[-1] = '\0';
    char *prefix = trim(s);
    size_t len = strlen(prefix);
    if (len == 0 || prefix[len - 1] != '-') return "expected '-' before the key";
    prefix[len - 1] = '\0';

    b->mods = 0;
    bool first = true;
    for (char *word = strtok(prefix, "+"); word; word = strtok(NULL, "+"), first = false) {
        word = trim(word);
        if (first) {
            if (strcmp(word, "hyper") != 0) return "combo must start with 'hyper'";
        } else if (!parse_mod(word, &b->mods)) {
            return "unknown modifier";
        }
    }
    if (first) return "combo must start with 'hyper'";
    return NULL;
}

// Parses a tap: "hyper", "cmd + hyper", "opt + shift + hyper", in any order.
static const char *parse_tap_trigger(char *s, struct binding *b) {
    b->tap = true;
    b->mods = 0;
    int hypers = 0;
    for (char *word = strtok(s, "+"); word; word = strtok(NULL, "+")) {
        word = trim(word);
        if (strcmp(word, "hyper") == 0) hypers++;
        else if (!parse_mod(word, &b->mods)) return "unknown modifier";
    }
    return hypers == 1 ? NULL : "a tap needs exactly one 'hyper'";
}

// A key binding has "- key"; a tap has no key at all.
static const char *parse_trigger(char *s, struct binding *b) {
    return strchr(s, '-') ? parse_key_trigger(s, b) : parse_tap_trigger(s, b);
}

// Parses "ctrl+tab" or "cmd+shift+ä": modifiers, then the key.
static const char *parse_combo_keys(char *s, struct combo *c) {
    c->mods = 0;
    char *key = strrchr(s, '+');
    if (key && key[1] == '\0' && key > s && key[-1] == '+') key--;  // "cmd++"
    char *mods = s;
    if (key) {
        *key++ = '\0';
        for (char *mod = strtok(mods, "+"); mod; mod = strtok(NULL, "+")) {
            if (!parse_mod(trim(mod), &c->mods)) return "unknown modifier in shortcut";
        }
    } else {
        key = s;
    }
    int code = parse_key(trim(key));
    if (code < 0) return "unknown key in shortcut";
    c->keycode = (uint16_t)code;
    return NULL;
}

// Parses an app binding's options after ';', currently only "tab ...".
static const char *parse_app_options(char *s, struct binding *b) {
    s = trim(s);
    if (strncmp(s, "tab", 3) != 0 || !isspace((unsigned char)s[3])) {
        return "unknown app option (tab)";
    }
    char *value = trim(s + 3);
    if (strcmp(value, "none") == 0) {
        b->tab_mode = TAB_NONE;
        return NULL;
    }
    b->tab_mode = TAB_CUSTOM;
    return parse_combo_keys(value, &b->tab);
}

// "next" or "prev" -> b->reverse.
static const char *parse_direction(const char *arg, struct binding *b, const char *usage) {
    if (strcmp(arg, "next") == 0) b->reverse = false;
    else if (strcmp(arg, "prev") == 0) b->reverse = true;
    else return usage;
    return NULL;
}

static const char *parse_action(char *s, struct binding *b) {
    char *verb = s;
    char *arg = split_word(s);

    if (strcmp(verb, "space") == 0) {
        if (strcmp(arg, "back") == 0) {
            b->action = ACTION_SPACE_BACK;
        } else if (strncmp(arg, "cycle", 5) == 0 && (arg[5] == '\0' || isspace((unsigned char)arg[5]))) {
            b->action = ACTION_SPACE_CYCLE;
            char *dir = trim(arg + 5);
            if (strcmp(dir, "prev") == 0) b->reverse = true;
            else if (*dir) return "space cycle takes prev or nothing";
        } else {
            return "space needs back or cycle";
        }
        return NULL;
    }
    if (strcmp(verb, "desktop") == 0) {
        if (*arg) return "desktop takes no argument";
        b->action = ACTION_DESKTOP;
        return NULL;
    }
    if (strcmp(verb, "tab") == 0) {
        b->action = ACTION_TAB;
        return parse_direction(arg, b, "tab needs next or prev");
    }
    if (strcmp(verb, "window") == 0) {
        b->action = ACTION_WINDOW;
        return parse_direction(arg, b, "window needs next or prev");
    }
    if (strcmp(verb, "app") == 0) {
        b->tab_mode = TAB_DEFAULT;
        char *options = strchr(arg, ';');
        if (options) {
            *options++ = '\0';
            const char *err = parse_app_options(options, b);
            if (err) return err;
            arg = trim(arg);
        }
        size_t len = strlen(arg);
        if (len >= 2 && arg[0] == '"' && arg[len - 1] == '"') {
            arg[len - 1] = '\0';
            arg++;
        }
        if (*arg == '\0') return "app needs a name";
        b->action = ACTION_APP;
        b->arg = strdup(arg);
        return b->arg ? NULL : strerror(errno);
    }
    if (strcmp(verb, "key") == 0) {
        if (*arg == '\0') return "key needs a shortcut, like cmd+t";
        b->action = ACTION_KEY;
        return parse_combo_keys(arg, &b->key);
    }
    if (strcmp(verb, "exec") == 0) {
        if (*arg == '\0') return "exec needs a command";
        b->action = ACTION_EXEC;
        b->arg = strdup(arg);
        return b->arg ? NULL : strerror(errno);
    }
    return "unknown action (space, desktop, app, tab, window, key, exec)";
}

// Parses "<setting> <value>" after "set".
static const char *parse_setting(char *s, struct config *config) {
    char *name = s;
    char *value = split_word(s);
    if (*value == '\0') return "set needs a value";

    if (strcmp(name, "tab") == 0) return parse_combo_keys(value, &config->default_tab);
    if (strcmp(name, "tap-ms") == 0) {
        char *end;
        long ms = strtol(value, &end, 10);
        if (*end != '\0' || ms < 50 || ms > 1000) return "tap-ms needs 50-1000";
        config->tap_ms = (int)ms;
        return NULL;
    }
    if (strcmp(name, "app-window-mod") == 0) {
        config->app_window_mods = 0;
        if (strcmp(value, "none") == 0) return NULL;
        for (char *mod = strtok(value, "+"); mod; mod = strtok(NULL, "+")) {
            if (!parse_mod(trim(mod), &config->app_window_mods)) {
                return "app-window-mod needs modifiers (cmd, opt+shift, ...) or none";
            }
        }
        return NULL;
    }
    return "unknown setting (tab, tap-ms, app-window-mod)";
}

static bool already_bound(const struct config *config, const struct binding *b) {
    return b->tap ? config_find_tap(config, b->mods) != NULL
                  : config_find(config, b->keycode, b->mods) != NULL;
}

struct config *config_load(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "workspeed: %s: %s\n", path, strerror(errno));
        return NULL;
    }

    struct config *config = calloc(1, sizeof *config);
    config->default_tab = (struct combo){.keycode = 0x30, .mods = kCGEventFlagMaskControl};
    config->tap_ms = DEFAULT_TAP_MS;
    size_t capacity = 0;
    int errors = 0;
    char *line = NULL;
    size_t line_cap = 0;

    for (int lineno = 1; getline(&line, &line_cap, file) > 0; lineno++) {
        char *s = trim(line);
        if (*s == '\0' || *s == '#') continue;

        const char *err;
        struct binding b = {0};
        bool is_binding = false;
        if (strncmp(s, "set", 3) == 0 && isspace((unsigned char)s[3])) {
            err = parse_setting(trim(s + 3), config);
        } else {
            is_binding = true;
            char *colon = strchr(s, ':');
            err = colon ? NULL : "expected ':' between trigger and action";
            if (!err) {
                *colon = '\0';
                err = parse_trigger(trim(s), &b);
            }
            if (!err) err = parse_action(trim(colon + 1), &b);
            if (!err && already_bound(config, &b)) err = "already bound";
        }

        if (err) {
            fprintf(stderr, "workspeed: %s:%d: %s\n", path, lineno, err);
            free(b.arg);
            errors++;
            continue;
        }
        if (!is_binding) continue;

        if (config->count == capacity) {
            capacity = capacity ? capacity * 2 : 32;
            config->bindings = realloc(config->bindings, capacity * sizeof *config->bindings);
        }
        config->bindings[config->count++] = b;
    }

    free(line);
    fclose(file);
    if (errors) {
        config_free(config);
        return NULL;
    }
    return config;
}

void config_free(struct config *config) {
    if (!config) return;
    for (size_t i = 0; i < config->count; i++) free(config->bindings[i].arg);
    free(config->bindings);
    free(config);
}

const struct binding *config_find(const struct config *config, uint16_t keycode,
                                  CGEventFlags mods) {
    for (size_t i = 0; i < config->count; i++) {
        const struct binding *b = &config->bindings[i];
        if (!b->tap && b->keycode == keycode && b->mods == (mods & TAP_MOD_MASK)) return b;
    }
    return NULL;
}

const struct binding *config_find_tap(const struct config *config, CGEventFlags mods) {
    for (size_t i = 0; i < config->count; i++) {
        const struct binding *b = &config->bindings[i];
        if (b->tap && b->mods == (mods & TAP_MOD_MASK)) return b;
    }
    return NULL;
}

const struct combo *config_app_tab(const struct config *config, const struct binding *b) {
    switch (b->tab_mode) {
    case TAB_DEFAULT: return &config->default_tab;
    case TAB_CUSTOM: return &b->tab;
    case TAB_NONE: return NULL;
    }
    return NULL;
}
