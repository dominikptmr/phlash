#include "capslock.h"

#include <IOKit/hid/IOHIDProperties.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>
#include <stdio.h>

// HID page 0x07 caps lock 0x39 -> F18 0x6D
#define CAPS_LOCK 0x700000039LL
#define F18 0x70000006DLL

static IOHIDEventSystemClientRef client;
static CFDictionaryRef ours;
// user's own mappings put back on exit
static CFArrayRef saved;

static long long mapping_key(CFDictionaryRef m, CFStringRef key) {
    CFNumberRef n = CFDictionaryGetValue(m, key);
    long long value = 0;
    if (n && CFGetTypeID(n) == CFNumberGetTypeID()) CFNumberGetValue(n, kCFNumberLongLongType, &value);
    return value;
}

static bool is_ours(CFDictionaryRef m) {
    return mapping_key(m, CFSTR(kIOHIDKeyboardModifierMappingSrcKey)) == CAPS_LOCK &&
           mapping_key(m, CFSTR(kIOHIDKeyboardModifierMappingDstKey)) == F18;
}

static CFMutableArrayRef copy_mappings(bool drop_caps_lock) {
    CFMutableArrayRef out = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFTypeRef current = IOHIDEventSystemClientCopyProperty(client, CFSTR(kIOHIDUserKeyUsageMapKey));
    if (current && CFGetTypeID(current) == CFArrayGetTypeID()) {
        for (CFIndex i = 0; i < CFArrayGetCount(current); i++) {
            CFDictionaryRef m = CFArrayGetValueAtIndex(current, i);
            if (CFGetTypeID(m) != CFDictionaryGetTypeID() || is_ours(m)) continue;
            if (drop_caps_lock &&
                mapping_key(m, CFSTR(kIOHIDKeyboardModifierMappingSrcKey)) == CAPS_LOCK) {
                continue;
            }
            CFArrayAppendValue(out, m);
        }
    }
    if (current) CFRelease(current);
    return out;
}

static bool apply(void) {
    if (saved) CFRelease(saved);
    saved = copy_mappings(false);
    CFMutableArrayRef mappings = copy_mappings(true);
    CFArrayAppendValue(mappings, ours);
    bool ok = IOHIDEventSystemClientSetProperty(client, CFSTR(kIOHIDUserKeyUsageMapKey), mappings);
    CFRelease(mappings);
    if (!ok) fprintf(stderr, "phlash: cannot remap caps lock\n");
    return ok;
}

bool capslock_remap(void) {
    client = IOHIDEventSystemClientCreateSimpleClient(kCFAllocatorDefault);
    if (!client) {
        fprintf(stderr, "phlash: cannot reach the HID system\n");
        return false;
    }
    long long src = CAPS_LOCK, dst = F18;
    CFNumberRef src_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &src);
    CFNumberRef dst_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &dst);
    const void *keys[] = {CFSTR(kIOHIDKeyboardModifierMappingSrcKey),
                          CFSTR(kIOHIDKeyboardModifierMappingDstKey)};
    const void *values[] = {src_number, dst_number};
    ours = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
    CFRelease(src_number);
    CFRelease(dst_number);
    return apply();
}

bool capslock_check(void) {
    CFTypeRef current = IOHIDEventSystemClientCopyProperty(client, CFSTR(kIOHIDUserKeyUsageMapKey));
    bool present = false;
    if (current && CFGetTypeID(current) == CFArrayGetTypeID()) {
        for (CFIndex i = 0; i < CFArrayGetCount(current) && !present; i++) {
            CFDictionaryRef m = CFArrayGetValueAtIndex(current, i);
            present = CFGetTypeID(m) == CFDictionaryGetTypeID() && is_ours(m);
        }
    }
    if (current) CFRelease(current);
    if (present) return false;
    apply();
    return true;
}

bool capslock_restore(void) {
    return IOHIDEventSystemClientSetProperty(client, CFSTR(kIOHIDUserKeyUsageMapKey), saved);
}
