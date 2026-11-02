#include "tap.h"
#include "capture.h"
#include "overlay.h"

#include <CoreGraphics/CoreGraphics.h>
#include <math.h>

#define MODS                                                                   \
  (kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate | kCGEventFlagMaskShift)
#define MAX_ZOOM 32.0
#define SPEED 0.01 // zoom factor per point of scroll, as exp(SPEED * delta)
#define KEY_ESCAPE 53

static CFMachPortRef tap;
static double direction = 1.0;
static double zoom = 1.0;
static bool active;
static CGRect bounds; // the zoomed display, in global points

// Magnify around the cursor: the screen point under the cursor stays under
// it. With c relative to the display, screen point p is drawn at
// c + (p - c) * zoom, so the visible source spans
// [c - c/zoom, c + (size - c)/zoom], which never leaves the display.
static void update(CGPoint c) {
  CGRect b = bounds;
  c.x = fmin(fmax(c.x - b.origin.x, 0), b.size.width);
  c.y = fmin(fmax(c.y - b.origin.y, 0), b.size.height);
  CGRect src = {{b.origin.x + c.x - c.x / zoom, b.origin.y + c.y - c.y / zoom},
                {b.size.width / zoom, b.size.height / zoom}};
  overlay_set_source(src);
}

// The display under the cursor, or the main one if somehow there is none.
static CGDirectDisplayID display_at(CGPoint p) {
  CGDirectDisplayID d;
  uint32_t n = 0;
  if (CGGetDisplaysWithPoint(p, 1, &d, &n) != kCGErrorSuccess || n == 0)
    return CGMainDisplayID();
  return d;
}

static void close_zoom(void) {
  active = false;
  zoom = 1.0;
  capture_stop();
  overlay_hide();
}

static void scroll(CGEventRef e) {
  // Trackpads scroll on axis 1. A mouse wheel with Shift held arrives as
  // horizontal scrolling (axis 2), so fall back to that.
  double dy = CGEventGetIntegerValueField(e, kCGScrollWheelEventPointDeltaAxis1);
  if (dy == 0)
    dy = CGEventGetIntegerValueField(e, kCGScrollWheelEventPointDeltaAxis2);
  dy *= direction;

  if (!active) {
    if (dy <= 0) // zooming out of nothing
      return;
    // Zoom the display the cursor is on; it stays the zoomed one until close.
    CGDirectDisplayID d = display_at(CGEventGetLocation(e));
    bounds = CGDisplayBounds(d);
    active = true;
    overlay_set_display(d);
    capture_start(d);
  }

  zoom = fmin(zoom * exp(dy * SPEED), MAX_ZOOM);
  if (zoom <= 1.0)
    close_zoom();
  else
    update(CGEventGetLocation(e));
}

// Returning NULL swallows the event; returning `e` passes it on.
static CGEventRef on_event(CGEventTapProxy proxy, CGEventType type,
                           CGEventRef e, void *user) {
  (void)proxy, (void)user;
  switch (type) {
  case kCGEventTapDisabledByTimeout:
  case kCGEventTapDisabledByUserInput:
    // macOS turns taps off if they're slow or the user did something like
    // enter secure input; turn it back on.
    CGEventTapEnable(tap, true);
    return e;

  case kCGEventScrollWheel:
    if ((CGEventGetFlags(e) & MODS) != MODS)
      return e;
    scroll(e);
    return NULL;

  case kCGEventMouseMoved:
  case kCGEventLeftMouseDragged:
  case kCGEventRightMouseDragged:
  case kCGEventOtherMouseDragged:
    if (active)
      update(CGEventGetLocation(e));
    return e;

  case kCGEventKeyDown:
    if (active &&
        CGEventGetIntegerValueField(e, kCGKeyboardEventKeycode) == KEY_ESCAPE) {
      close_zoom();
      return NULL;
    }
    return e;

  default:
    return e;
  }
}

bool tap_init(bool reverse) {
  direction = reverse ? -1.0 : 1.0;
  CGEventMask mask = CGEventMaskBit(kCGEventScrollWheel) |
                     CGEventMaskBit(kCGEventMouseMoved) |
                     CGEventMaskBit(kCGEventLeftMouseDragged) |
                     CGEventMaskBit(kCGEventRightMouseDragged) |
                     CGEventMaskBit(kCGEventOtherMouseDragged) |
                     CGEventMaskBit(kCGEventKeyDown);
  // kCGEventTapOptionDefault (not ListenOnly) so we can swallow events.
  tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                         kCGEventTapOptionDefault, mask, on_event, NULL);
  if (!tap)
    return false;
  CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(NULL, tap, 0);
  CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopCommonModes);
  CFRelease(src);
  return true;
}
