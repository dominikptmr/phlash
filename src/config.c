#include "config.h"

#include "keys.h"
#include "tap.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

// syntax in docs/configuration.md

#define DEFAULT_TAP_MS 300

static const char new_config[] =
    "# phlash config, syntax: "
    "https://github.com/dominikptmr/phlash/blob/main/docs/configuration.md\n";

const char *config_default_path(void) {
    static char path[PATH_MAX];
    const char *home = getenv("HOME");
    snprintf(path, sizeof path, "%s/.config/phlash/config", home ? home : "");
    return path;
}

bool config_create(const char *path) {
    char dir[PATH_MAX];
    strlcpy(dir, path, sizeof dir);
    char *slash = strrchr(dir, '/');
    if (!slash) return false;
    *slash = '\0';
    for (char *p = dir + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        mkdir(dir, 0755);
        *p = '/';
    }
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "phlash: cannot create %s: %s\n", dir, strerror(errno));
        return false;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        fprintf(stderr, "phlash: cannot create %s: %s\n", path, strerror(errno));
        return false;
    }
    bool ok = write(fd, new_config, sizeof new_config - 1) == (ssize_t)(sizeof new_config - 1);
    close(fd);
    if (!ok) fprintf(stderr, "phlash: cannot write %s\n", path);
    return ok;
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

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
    else if (strcmp(s, "opt") == 0) *mods |= kCGEventFlagMaskAlternate;
    else if (strcmp(s, "ctrl") == 0) *mods |= kCGEventFlagMaskControl;
    else if (strcmp(s, "shift") == 0) *mods |= kCGEventFlagMaskShift;
    else return false;
    return true;
}

static const char *parse_key_trigger(char *s, struct binding *b) {
    // key is the last word so "hyper - -" binds minus
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

static const char *parse_tap_trigger(char *s, struct binding *b) {
    b->tap = true;
    b->mods = 0;
    if (!strchr(s, '+') && strcmp(s, "hyper") != 0) {
        if (!keys_tap_modifier(s, b->tap_keycodes)) return "unknown key to tap";
        b->tap_modifier = true;
        return NULL;
    }
    int hypers = 0;
    for (char *word = strtok(s, "+"); word; word = strtok(NULL, "+")) {
        word = trim(word);
        if (strcmp(word, "hyper") == 0) hypers++;
        else if (!parse_mod(word, &b->mods)) return "unknown modifier";
    }
    return hypers == 1 ? NULL : "a tap needs exactly one 'hyper'";
}

static const char *parse_trigger(char *s, struct binding *b) {
    return strchr(s, '-') ? parse_key_trigger(s, b) : parse_tap_trigger(s, b);
}

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

static const char *add_app_tab(struct config *config, const char *name, const struct app_tab *t) {
    for (size_t i = 0; i < config->app_tab_count; i++) {
        if (strcasecmp(config->app_tabs[i].name, name) == 0) return "app already has a tab shortcut";
    }
    config->app_tabs = realloc(config->app_tabs, (config->app_tab_count + 1) * sizeof *config->app_tabs);
    struct app_tab *slot = &config->app_tabs[config->app_tab_count];
    *slot = *t;
    slot->name = strdup(name);
    if (!slot->name) return strerror(errno);
    config->app_tab_count++;
    return NULL;
}

static const char *parse_tab(char *value, struct config *config) {
    char *names = strstr(value, " for ");
    if (!names) return parse_combo_keys(value, &config->default_tab);
    *names = '\0';
    names = trim(names + 5);

    struct app_tab t = {0};
    char *shortcut = trim(value);
    if (strcmp(shortcut, "none") == 0) {
        t.none = true;
    } else {
        const char *err = parse_combo_keys(shortcut, &t.tab);
        if (err) return err;
    }
    for (char *name = strsep(&names, "|"); name; name = strsep(&names, "|")) {
        name = trim(name);
        if (*name == '\0') return "tab ... for needs app names";
        const char *err = add_app_tab(config, name, &t);
        if (err) return err;
    }
    return NULL;
}

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

static const char *parse_setting(char *s, struct config *config) {
    char *name = s;
    char *value = split_word(s);
    if (*value == '\0') return "set needs a value";

    if (strcmp(name, "tab") == 0) return parse_tab(value, config);
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
    if (b->tap_modifier) {
        for (size_t i = 0; i < config->count; i++) {
            const struct binding *other = &config->bindings[i];
            if (other->tap_modifier && other->tap_keycodes[0] == b->tap_keycodes[0] &&
                other->tap_keycodes[1] == b->tap_keycodes[1]) {
                return true;
            }
        }
        return false;
    }
    return b->tap ? config_find_tap(config, b->mods) != NULL
                  : config_find(config, b->keycode, b->mods) != NULL;
}

struct config *config_load(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "phlash: %s: %s\n", path, strerror(errno));
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
            fprintf(stderr, "phlash: %s:%d: %s\n", path, lineno, err);
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
    for (size_t i = 0; i < config->app_tab_count; i++) free(config->app_tabs[i].name);
    free(config->app_tabs);
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
        if (b->tap && !b->tap_modifier && b->mods == (mods & TAP_MOD_MASK)) return b;
    }
    return NULL;
}

const struct binding *config_find_modifier_tap(const struct config *config, uint16_t keycode) {
    const struct binding *either_side = NULL;
    for (size_t i = 0; i < config->count; i++) {
        const struct binding *b = &config->bindings[i];
        if (!b->tap_modifier) continue;
        bool first = b->tap_keycodes[0] == keycode, second = b->tap_keycodes[1] == keycode;
        if (first && second) return b;
        if ((first || second) && !either_side) either_side = b;
    }
    return either_side;
}
