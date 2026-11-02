#ifndef ZOOMER_OVERLAY_H
#define ZOOMER_OVERLAY_H

#include <CoreGraphics/CoreGraphics.h>
#include <IOSurface/IOSurfaceRef.h>
#include <stdbool.h>

// Fullscreen, click-through window over one display that shows a part of
// the captured screen stretched to fill it.

void overlay_init(bool linear);
// Move the window over this display. Call before showing frames from it.
void overlay_set_display(CGDirectDisplayID display);
// Show a new captured frame, and the window if it was hidden.
void overlay_set_surface(IOSurfaceRef surface);
// Which part of the screen to magnify, in global display points
// (origin top-left, like CGEvent locations).
void overlay_set_source(CGRect src);
void overlay_hide(void);

#endif
