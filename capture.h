#ifndef ZOOMER_CAPTURE_H
#define ZOOMER_CAPTURE_H

#include <CoreGraphics/CoreGraphics.h>

// Live capture of a display with ScreenCaptureKit, excluding our own
// overlay. Frames go to overlay_set_surface(). Both calls are asynchronous
// and safe to call in any order, any number of times, from the main thread.
// Starting on another display replaces the stream.

void capture_start(CGDirectDisplayID display);
void capture_stop(void);

#endif
