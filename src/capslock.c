#include "capslock.h"

#include "proc.h"

#include <stddef.h>

// HID page 0x07 caps lock 0x39 -> F18 0x6D
static const char remap_json[] =
    "{\"UserKeyMapping\":[{\"HIDKeyboardModifierMappingSrc\":0x700000039,"
    "\"HIDKeyboardModifierMappingDst\":0x70000006D}]}";
static const char restore_json[] = "{\"UserKeyMapping\":[]}";

static bool hidutil_set(const char *json) {
    char *const argv[] = {"hidutil", "property", "--set", (char *)json, NULL};
    return proc_run("/usr/bin/hidutil", argv, true);
}

bool capslock_remap(void) {
    return hidutil_set(remap_json);
}

bool capslock_restore(void) {
    return hidutil_set(restore_json);
}
