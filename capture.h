#ifndef ZOOMER_CAPTURE_H
#define ZOOMER_CAPTURE_H

#include <CoreGraphics/CoreGraphics.h>

// Live capture of a display with ScreenCaptureKit, excluding our own
// overlay. Frames go to overlay_set_surface(). Both calls are asynchronous
// and safe to call in any order, any number of times, from the main thread.
// Starting on another display replaces the stream.
//
// If the capture cannot start, or the system stops it later (display
// unplugged, permission revoked...), `on_lost` is called on the main thread.

void capture_start(CGDirectDisplayID display, void (*on_lost)(void));
void capture_stop(void);

#endif
