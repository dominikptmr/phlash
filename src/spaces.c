#include "spaces.h"

#include "cgs_private.h"

static CGSConnectionID connection(void) {
    static CGSConnectionID cid;
    if (!cid) cid = CGSMainConnectionID();
    return cid;
}

static uint64_t dict_u64(CFDictionaryRef dict, CFStringRef key) {
    CFNumberRef number = CFDictionaryGetValue(dict, key);
    int64_t value = 0;
    if (number && CFGetTypeID(number) == CFNumberGetTypeID()) {
        CFNumberGetValue(number, kCFNumberSInt64Type, &value);
    }
    return (uint64_t)value;
}

size_t spaces_get(struct space out[SPACES_MAX]) {
    CFArrayRef displays = CGSCopyManagedDisplaySpaces(connection(), NULL);
    if (!displays) return 0;

    size_t count = 0;
    CFIndex display_count = CFArrayGetCount(displays);
    for (CFIndex d = 0; d < display_count; d++) {
        CFDictionaryRef display = CFArrayGetValueAtIndex(displays, d);
        CFArrayRef spaces = CFDictionaryGetValue(display, CFSTR("Spaces"));
        if (!spaces) continue;

        CFIndex space_count = CFArrayGetCount(spaces);
        for (CFIndex s = 0; s < space_count && count < SPACES_MAX; s++) {
            CFDictionaryRef space = CFArrayGetValueAtIndex(spaces, s);
            uint64_t id = dict_u64(space, CFSTR("id64"));
            if (!id) id = dict_u64(space, CFSTR("ManagedSpaceID"));
            out[count++] = (struct space){
                .id = id,
                .display = (int)d,
                .fullscreen = dict_u64(space, CFSTR("type")) == 4,
            };
        }
    }
    CFRelease(displays);
    return count;
}

uint64_t spaces_active(void) {
    return CGSGetActiveSpace(connection());
}

int spaces_index(const struct space *spaces, size_t count, uint64_t id) {
    for (size_t i = 0; i < count; i++) {
        if (spaces[i].id == id) return (int)i + 1;
    }
    return 0;
}

uint64_t spaces_of_window(uint32_t window_id) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &window_id);
    CFArrayRef windows = CFArrayCreate(kCFAllocatorDefault, (const void **)&number, 1,
                                       &kCFTypeArrayCallBacks);
    CFRelease(number);

    CFArrayRef spaces = CGSCopySpacesForWindows(connection(), CGS_SPACE_MASK_ALL, windows);
    CFRelease(windows);
    if (!spaces) return 0;

    // "All Desktops" windows have several and the first is enough
    int64_t id = 0;
    if (CFArrayGetCount(spaces) > 0) {
        CFNumberGetValue(CFArrayGetValueAtIndex(spaces, 0), kCFNumberSInt64Type, &id);
    }
    CFRelease(spaces);
    return (uint64_t)id;
}
