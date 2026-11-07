#ifndef PHLASH_CGS_PRIVATE_H
#define PHLASH_CGS_PRIVATE_H

// Private SkyLight (CGS) calls, re-exported by CoreGraphics. Read-only queries
// that work with SIP enabled; the same ones yabai uses for its queries.
// Undocumented: if a macOS update breaks space lookup, look here first.

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>

typedef int CGSConnectionID;

// Mask for CGSCopySpacesForWindows: current, other and user spaces.
#define CGS_SPACE_MASK_ALL 7

extern CGSConnectionID CGSMainConnectionID(void);

// One dictionary per display: "Display Identifier", "Current Space" and
// "Spaces" (in Mission Control order; each has "id64"/"ManagedSpaceID" and
// "type": 0 = desktop, 4 = fullscreen or Split View). display NULL = all.
extern CFArrayRef CGSCopyManagedDisplaySpaces(CGSConnectionID cid, CFStringRef display);

// Space IDs (CFNumbers) the given window IDs (CFNumbers) are on.
extern CFArrayRef CGSCopySpacesForWindows(CGSConnectionID cid, int mask, CFArrayRef windows);

// The space that currently has focus.
extern uint64_t CGSGetActiveSpace(CGSConnectionID cid);

// The window server ID (as in CGWindowList) of an Accessibility window element.
// Private HIServices call, used by yabai, AltTab and Hammerspoon alike.
extern AXError _AXUIElementGetWindow(AXUIElementRef element, uint32_t *window_id);

#endif
