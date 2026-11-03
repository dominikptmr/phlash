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

// Line format, one binding per line, '#' starts a comment line:
//
//   hyper [+ mod ...] - key : action
//
//   mod     cmd | opt | ctrl | shift
//   key     a name from keys.c ("u", ";", "left", "f5") or a hex keycode ("0x29")
//   action  space <1-16> | space prev | space next | desktop
//           | app <name> [; tab <combo> | ; tab none] | exec <shell command>
//
// app names match either the name --list shows or the .app bundle's name, so
// both "Code" and "Visual Studio Code" work; quotes around the name are optional.
// "app A | B | C" lists alternatives in order of preference.
//
// Pressing an app's key while it is already in front sends its next-tab
// shortcut, ctrl+tab unless "; tab" names another combo (e.g. ctrl+right) or
// none. Hyper + cmd + an app's key cycles that app's windows; this needs no
// line of its own.

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
static const char *parse_combo(char *s, struct binding *b) {
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

// Parses "ctrl+tab" or "cmd+shift+ä": modifiers, then the key.
static const char *parse_combo_keys(char *s, struct combo *c) {
    c->mods = 0;
    char *key = strrchr(s, '+');
    if (key && key[1] == '\0' && key > s && key[-1] == '+') key--;  // "cmd++"
    char *mods = s;
    if (key) {
        *key++ = '\0';
        for (char *mod = strtok(mods, "+"); mod; mod = strtok(NULL, "+")) {
            if (!parse_mod(trim(mod), &c->mods)) return "unknown modifier in tab shortcut";
        }
    } else {
        key = s;
    }
    int code = parse_key(trim(key));
    if (code < 0) return "unknown key in tab shortcut";
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
        b->tab_enabled = false;
        return NULL;
    }
    return parse_combo_keys(value, &b->tab);
}

static const char *parse_action(char *s, struct binding *b) {
    char *verb = s;
    char *arg = s;
    while (*arg && !isspace((unsigned char)*arg)) arg++;
    if (*arg) *arg++ = '\0';
    arg = trim(arg);

    if (strcmp(verb, "space") == 0) {
        if (strcmp(arg, "prev") == 0) {
            b->action = ACTION_SPACE_PREV;
        } else if (strcmp(arg, "next") == 0) {
            b->action = ACTION_SPACE_NEXT;
        } else {
            char *end;
            long n = strtol(arg, &end, 10);
            if (*arg == '\0' || *end != '\0' || n < 1 || n > 16) {
                return "space needs 1-16, prev or next";
            }
            b->action = ACTION_SPACE;
            b->space = (int)n;
        }
        return NULL;
    }
    if (strcmp(verb, "desktop") == 0) {
        if (*arg) return "desktop takes no argument";
        b->action = ACTION_DESKTOP;
        return NULL;
    }
    if (strcmp(verb, "app") == 0) {
        b->tab_enabled = true;
        b->tab = (struct combo){.keycode = 0x30, .mods = kCGEventFlagMaskControl};  // ctrl+tab
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
    if (strcmp(verb, "exec") == 0) {
        if (*arg == '\0') return "exec needs a command";
        b->action = ACTION_EXEC;
        b->arg = strdup(arg);
        return b->arg ? NULL : strerror(errno);
    }
    return "unknown action (space, desktop, app, exec)";
}

struct config *config_load(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "workspeed: %s: %s\n", path, strerror(errno));
        return NULL;
    }

    struct config *config = calloc(1, sizeof *config);
    size_t capacity = 0;
    int errors = 0;
    char *line = NULL;
    size_t line_cap = 0;

    for (int lineno = 1; getline(&line, &line_cap, file) > 0; lineno++) {
        char *s = trim(line);
        if (*s == '\0' || *s == '#') continue;

        struct binding b = {0};
        char *colon = strchr(s, ':');
        const char *err = colon ? NULL : "expected ':' between combo and action";
        if (!err) {
            *colon = '\0';
            err = parse_combo(trim(s), &b);
        }
        if (!err) err = parse_action(trim(colon + 1), &b);
        if (!err && config_find(config, b.keycode, b.mods)) err = "combo is already bound";

        if (err) {
            fprintf(stderr, "workspeed: %s:%d: %s\n", path, lineno, err);
            free(b.arg);
            errors++;
            continue;
        }

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
        if (b->keycode == keycode && b->mods == (mods & TAP_MOD_MASK)) return b;
    }
    return NULL;
}
