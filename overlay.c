#include "overlay.h"
#include "objc.h"

// QuartzCore's filter names are NSString constants; C only needs the symbols.
extern id const kCAFilterNearest;
extern id const kCAFilterLinear;

enum {
  NSWindowStyleMaskBorderless = 0,
  NSBackingStoreBuffered = 2,
  NSScreenSaverWindowLevel = 1000,
  NSWindowCollectionBehaviorCanJoinAllSpaces = 1 << 0,
  NSWindowCollectionBehaviorStationary = 1 << 4,
  NSWindowCollectionBehaviorIgnoresCycle = 1 << 6,
  NSWindowCollectionBehaviorFullScreenAuxiliary = 1 << 8,
};

static id window;
static id layer;
static CGRect screen; // current display, in global points (origin top-left)
static bool visible;

void overlay_init(bool linear) {
  // Start on the main display; overlay_set_display() moves it.
  screen = CGDisplayBounds(CGMainDisplayID());
  CGRect frame = {CGPointZero, screen.size};
  window = MSG(id, CGRect, unsigned long, unsigned long, BOOL)(
      MSG(id)(cls("NSWindow"), sel("alloc")),
      sel("initWithContentRect:styleMask:backing:defer:"), frame,
      NSWindowStyleMaskBorderless, NSBackingStoreBuffered, NO);
  MSG(void, BOOL)(window, sel("setReleasedWhenClosed:"), NO);
  MSG(void, long)(window, sel("setLevel:"), NSScreenSaverWindowLevel);
  MSG(void, BOOL)(window, sel("setIgnoresMouseEvents:"), YES);
  MSG(void, BOOL)(window, sel("setOpaque:"), YES);
  MSG(void, BOOL)(window, sel("setHasShadow:"), NO);
  MSG(void, id)(window, sel("setBackgroundColor:"),
                MSG(id)(cls("NSColor"), sel("blackColor")));
  MSG(void, unsigned long)(window, sel("setCollectionBehavior:"),
                           NSWindowCollectionBehaviorCanJoinAllSpaces |
                               NSWindowCollectionBehaviorStationary |
                               NSWindowCollectionBehaviorIgnoresCycle |
                               NSWindowCollectionBehaviorFullScreenAuxiliary);

  // A layer-hosting view: we own the layer, AppKit never redraws over it.
  // Setting the layer before wantsLayer is what makes it hosting.
  layer = NEW("CALayer");
  MSG(void, id)(layer, sel("setMagnificationFilter:"),
                linear ? kCAFilterLinear : kCAFilterNearest);
  MSG(void, CGColorRef)(layer, sel("setBackgroundColor:"),
                        CGColorGetConstantColor(kCGColorBlack));
  id view = MSG(id)(window, sel("contentView"));
  MSG(void, id)(view, sel("setLayer:"), layer);
  MSG(void, BOOL)(view, sel("setWantsLayer:"), YES);
}

void overlay_set_display(CGDirectDisplayID display) {
  screen = CGDisplayBounds(display);
  // Cocoa frames have their origin at the bottom-left of the main display,
  // with y going up; CG bounds at its top-left, with y going down.
  CGFloat main_h = CGDisplayBounds(CGMainDisplayID()).size.height;
  CGRect frame = {
      {screen.origin.x, main_h - screen.origin.y - screen.size.height},
      screen.size,
  };
  MSG(void, CGRect, BOOL)(window, sel("setFrame:display:"), frame, NO);
}

// Layer property changes animate by default; we want them applied instantly.
static void begin(void) {
  MSG(void)(cls("CATransaction"), sel("begin"));
  MSG(void, BOOL)(cls("CATransaction"), sel("setDisableActions:"), YES);
}

static void commit(void) { MSG(void)(cls("CATransaction"), sel("commit")); }

void overlay_set_surface(IOSurfaceRef surface) {
  // CALayer displays an IOSurface directly: no copy, no GPU code.
  begin();
  MSG(void, id)(layer, sel("setContents:"), (id)surface);
  commit();
  if (!visible) {
    MSG(void)(window, sel("orderFrontRegardless"));
    visible = true;
  }
}

void overlay_set_source(CGRect src) {
  // contentsRect is in the image's unit square, with the origin at the
  // bottom-left, while our input is global with the origin at the top-left:
  // make it relative to the display, then flip y.
  CGSize sz = screen.size;
  CGFloat x = src.origin.x - screen.origin.x;
  CGFloat y = src.origin.y - screen.origin.y;
  CGRect unit = {
      {x / sz.width, 1.0 - (y + src.size.height) / sz.height},
      {src.size.width / sz.width, src.size.height / sz.height},
  };
  begin();
  MSG(void, CGRect)(layer, sel("setContentsRect:"), unit);
  commit();
}

void overlay_hide(void) {
  MSG(void, id)(window, sel("orderOut:"), NULL);
  visible = false;
  // Drop the last frame so it can't flash up next time.
  begin();
  MSG(void, id)(layer, sel("setContents:"), NULL);
  commit();
}
