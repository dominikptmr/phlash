#include "windows.h"

#include "spaces.h"

#include <CoreGraphics/CoreGraphics.h>

#define MIN_WINDOW_SIZE 100

static double dict_double(CFDictionaryRef dict, CFStringRef key) {
    CFNumberRef number = CFDictionaryGetValue(dict, key);
    double value = 0;
    if (number) CFNumberGetValue(number, kCFNumberDoubleType, &value);
    return value;
}

static int64_t dict_i64(CFDictionaryRef dict, CFStringRef key) {
    CFNumberRef number = CFDictionaryGetValue(dict, key);
    int64_t value = 0;
    if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &value);
    return value;
}

size_t windows_get(struct window out[WINDOWS_MAX]) {
    // owner and IDs need no Screen Recording permission
    CFArrayRef list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionAll | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    if (!list) return 0;

    size_t count = 0;
    CFIndex n = CFArrayGetCount(list);
    for (CFIndex i = 0; i < n && count < WINDOWS_MAX; i++) {
        CFDictionaryRef info = CFArrayGetValueAtIndex(list, i);
        if (dict_i64(info, kCGWindowLayer) != 0) continue;
        if (dict_double(info, kCGWindowAlpha) <= 0) continue;

        CGRect bounds;
        CFDictionaryRef bounds_dict = CFDictionaryGetValue(info, kCGWindowBounds);
        if (!bounds_dict || !CGRectMakeWithDictionaryRepresentation(bounds_dict, &bounds)) continue;
        if (bounds.size.width < MIN_WINDOW_SIZE || bounds.size.height < MIN_WINDOW_SIZE) continue;

        struct window *w = &out[count];
        w->id = (uint32_t)dict_i64(info, kCGWindowNumber);
        w->pid = (pid_t)dict_i64(info, kCGWindowOwnerPID);
        w->app[0] = '\0';
        CFStringRef owner = CFDictionaryGetValue(info, kCGWindowOwnerName);
        if (owner) CFStringGetCString(owner, w->app, sizeof w->app, kCFStringEncodingUTF8);
        w->space = spaces_of_window(w->id);
        count++;
    }
    CFRelease(list);
    return count;
}
