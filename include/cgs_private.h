#ifndef PHLASH_CGS_PRIVATE_H
#define PHLASH_CGS_PRIVATE_H

// private undocumented SkyLight calls
// check here first when a macOS update breaks spaces

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>

typedef int CGSConnectionID;

#define CGS_SPACE_MASK_ALL 7

extern CGSConnectionID CGSMainConnectionID(void);

extern CFArrayRef CGSCopyManagedDisplaySpaces(CGSConnectionID cid, CFStringRef display);

extern CFArrayRef CGSCopySpacesForWindows(CGSConnectionID cid, int mask, CFArrayRef windows);

extern uint64_t CGSGetActiveSpace(CGSConnectionID cid);

extern AXError _AXUIElementGetWindow(AXUIElementRef element, uint32_t *window_id);

#endif
