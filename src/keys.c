#include "keys.h"

#include <Carbon/Carbon.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

// Bindings are stored as virtual keycodes, which name physical key positions.
// Keys that type a character get their name from the active keyboard layout
// (keys_load_layout), so "y", "ö" or "#" mean the key printed with that
// character; these US names are only the fallback. Other keys have fixed names.
static const char *const fallback_names[128] = {
    [0x00] = "a", [0x01] = "s", [0x02] = "d", [0x03] = "f", [0x04] = "h",
    [0x05] = "g", [0x06] = "z", [0x07] = "x", [0x08] = "c", [0x09] = "v",
    [0x0B] = "b", [0x0C] = "q", [0x0D] = "w", [0x0E] = "e", [0x0F] = "r",
    [0x10] = "y", [0x11] = "t", [0x12] = "1", [0x13] = "2", [0x14] = "3",
    [0x15] = "4", [0x16] = "6", [0x17] = "5", [0x18] = "=", [0x19] = "9",
    [0x1A] = "7", [0x1B] = "-", [0x1C] = "8", [0x1D] = "0", [0x1E] = "]",
    [0x1F] = "o", [0x20] = "u", [0x21] = "[", [0x22] = "i", [0x23] = "p",
    [0x24] = "return", [0x25] = "l", [0x26] = "j", [0x27] = "'", [0x28] = "k",
    [0x29] = ";", [0x2A] = "\\", [0x2B] = ",", [0x2C] = "/", [0x2D] = "n",
    [0x2E] = "m", [0x2F] = ".", [0x30] = "tab", [0x31] = "space", [0x32] = "`",
    [0x33] = "delete", [0x35] = "escape",
    [0x40] = "f17", [0x4F] = "f18", [0x50] = "f19", [0x5A] = "f20",
    [0x60] = "f5", [0x61] = "f6", [0x62] = "f7", [0x63] = "f3", [0x64] = "f8",
    [0x65] = "f9", [0x67] = "f11", [0x69] = "f13", [0x6A] = "f16",
    [0x6B] = "f14", [0x6D] = "f10", [0x6F] = "f12", [0x71] = "f15",
    [0x73] = "home", [0x74] = "pageup", [0x75] = "forwarddelete", [0x76] = "f4",
    [0x77] = "end", [0x78] = "f2", [0x79] = "pagedown", [0x7A] = "f1",
    [0x7B] = "left", [0x7C] = "right", [0x7D] = "down", [0x7E] = "up",
};

// The main block's character keys, including the ISO key left of Z/Y (0x0A),
// which US keyboards lack.
static const uint16_t character_keys[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x32,
};

// Names from the active layout (UTF-8); empty where the key types nothing.
// Once loaded, they replace the fallback names of all character keys.
static char layout_names[128][8];
static bool layout_loaded;

static bool is_character_key(uint16_t keycode) {
    for (size_t i = 0; i < sizeof character_keys / sizeof character_keys[0]; i++) {
        if (character_keys[i] == keycode) return true;
    }
    return false;
}

const char *keys_name(uint16_t keycode) {
    if (keycode >= 128) return NULL;
    if (layout_loaded && is_character_key(keycode)) {
        return layout_names[keycode][0] ? layout_names[keycode] : NULL;
    }
    return fallback_names[keycode];
}

int keys_code(const char *name) {
    for (int i = 0; i < 128; i++) {
        const char *n = keys_name((uint16_t)i);
        if (n && strcasecmp(n, name) == 0) return i;
    }
    return -1;
}

// The character keycode types without modifiers, as lowercase UTF-8.
static bool translate(const UCKeyboardLayout *layout, uint16_t keycode, char out[8]) {
    UInt32 dead_keys = 0;
    UniChar chars[4];
    UniCharCount length = 0;
    OSStatus err = UCKeyTranslate(layout, keycode, kUCKeyActionDisplay, 0, LMGetKbdType(),
                                  kUCKeyTranslateNoDeadKeysBit, &dead_keys, 4, &length, chars);
    if (err != noErr || length != 1 || chars[0] <= ' ' || chars[0] == ':') return false;

    CFMutableStringRef s = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppendCharacters(s, chars, (CFIndex)length);
    CFStringLowercase(s, NULL);
    bool ok = CFStringGetCString(s, out, 8, kCFStringEncodingUTF8);
    CFRelease(s);
    return ok;
}

bool keys_load_layout(void) {
    memset(layout_names, 0, sizeof layout_names);
    layout_loaded = false;

    // The layout behind the current input source, also for input methods.
    TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
    if (!source) return false;
    CFDataRef data = TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData);
    if (!data) {
        CFRelease(source);
        return false;
    }
    const UCKeyboardLayout *layout = (const UCKeyboardLayout *)CFDataGetBytePtr(data);

    layout_loaded = true;
    for (size_t i = 0; i < sizeof character_keys / sizeof character_keys[0]; i++) {
        uint16_t keycode = character_keys[i];
        char name[8];
        // A character two keys share (rare) stays with the first one.
        if (translate(layout, keycode, name) && keys_code(name) < 0) {
            memcpy(layout_names[keycode], name, sizeof name);
        }
    }
    CFRelease(source);
    return true;
}

static const struct {
    const char *name;
    uint16_t keycodes[2];
} tap_modifiers[] = {
    {"cmd", {0x37, 0x36}},   {"lcmd", {0x37, 0x37}},   {"rcmd", {0x36, 0x36}},
    {"opt", {0x3A, 0x3D}},   {"lopt", {0x3A, 0x3A}},   {"ropt", {0x3D, 0x3D}},
    {"alt", {0x3A, 0x3D}},   {"lalt", {0x3A, 0x3A}},   {"ralt", {0x3D, 0x3D}},
    {"ctrl", {0x3B, 0x3E}},  {"lctrl", {0x3B, 0x3B}},  {"rctrl", {0x3E, 0x3E}},
    {"shift", {0x38, 0x3C}}, {"lshift", {0x38, 0x38}}, {"rshift", {0x3C, 0x3C}},
    {"fn", {0x3F, 0x3F}},
};
#define TAP_MODIFIER_COUNT (sizeof tap_modifiers / sizeof tap_modifiers[0])

bool keys_tap_modifier(const char *name, uint16_t keycodes[2]) {
    for (size_t i = 0; i < TAP_MODIFIER_COUNT; i++) {
        if (strcasecmp(tap_modifiers[i].name, name) != 0) continue;
        keycodes[0] = tap_modifiers[i].keycodes[0];
        keycodes[1] = tap_modifiers[i].keycodes[1];
        return true;
    }
    return false;
}

const char *keys_tap_modifier_name(const uint16_t keycodes[2]) {
    for (size_t i = 0; i < TAP_MODIFIER_COUNT; i++) {
        if (tap_modifiers[i].keycodes[0] == keycodes[0] &&
            tap_modifiers[i].keycodes[1] == keycodes[1]) {
            return tap_modifiers[i].name;
        }
    }
    return NULL;
}
