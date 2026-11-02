#include "capture.h"
#include "objc.h"
#include "overlay.h"

#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <dispatch/dispatch.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

extern id const SCStreamFrameInfoStatus; // NSString key, from ScreenCaptureKit

enum { SCStreamOutputTypeScreen = 0, SCFrameStatusComplete = 0 };

// ScreenCaptureKit is fully asynchronous. Rather than chaining callbacks, we
// keep what we want (`want`) and what we have (`stream`, `running`), and
// step() moves one step toward the wanted state whenever either changes.
// Everything here runs on the main queue, so no locking is needed.
static bool want;    // should frames be flowing?
static CGDirectDisplayID display;        // ...from which display
static CGDirectDisplayID stream_display; // the display `stream` captures
static bool busy;    // an async operation is in flight
static bool running; // the stream has been started and not stopped
static id stream;    // SCStream, built on first use and reused
static id output;    // our SCStreamOutput / SCStreamDelegate object

static void step(void);

static void print_error(const char *what, id error) {
  id desc = error ? MSG(id)(error, sel("localizedDescription")) : NULL;
  fprintf(stderr, "zoomer: %s: %s\n", what, desc ? cstr(desc) : "unknown error");
}

// Finish an async step: back to the main queue, then take the next step.
static void done(void) {
  dispatch_async(dispatch_get_main_queue(), ^{
    busy = false;
    step();
  });
}

// -[ZoomerOutput stream:didOutputSampleBuffer:ofType:], called on main queue.
static void on_sample(id self, SEL _cmd, id s, CMSampleBufferRef sb, long type) {
  (void)self, (void)_cmd;
  if (type != SCStreamOutputTypeScreen || !want || s != stream)
    return;

  // Idle or blank frames carry a status other than "complete" and no image.
  CFArrayRef infos = CMSampleBufferGetSampleAttachmentsArray(sb, false);
  if (!infos || CFArrayGetCount(infos) < 1)
    return;
  CFDictionaryRef info = CFArrayGetValueAtIndex(infos, 0);
  CFNumberRef num = CFDictionaryGetValue(info, SCStreamFrameInfoStatus);
  int status = -1;
  if (!num || !CFNumberGetValue(num, kCFNumberIntType, &status) ||
      status != SCFrameStatusComplete)
    return;

  CVImageBufferRef image = CMSampleBufferGetImageBuffer(sb);
  IOSurfaceRef surface = image ? CVPixelBufferGetIOSurface(image) : NULL;
  if (surface)
    overlay_set_surface(surface);
}

// -[ZoomerOutput stream:didStopWithError:], called on an arbitrary queue when
// the system stops the stream (display unplugged, permission revoked...).
static void on_stop(id self, SEL _cmd, id s, id error) {
  (void)self, (void)_cmd;
  print_error("capture stopped", error);
  dispatch_async(dispatch_get_main_queue(), ^{
    if (s == stream) { // throw it away; the next start builds a fresh one
      MSG(void)(stream, sel("release"));
      stream = NULL;
      running = false;
    }
    overlay_hide();
    want = false;
  });
}

// Build the class of our output object at runtime: an NSObject subclass
// with the two methods ScreenCaptureKit calls.
static id make_output(void) {
  Class c = objc_allocateClassPair(objc_getClass("NSObject"), "ZoomerOutput", 0);
  class_addMethod(c, sel("stream:didOutputSampleBuffer:ofType:"),
                  (IMP)on_sample, "v@:@^{opaqueCMSampleBuffer=}q");
  class_addMethod(c, sel("stream:didStopWithError:"), (IMP)on_stop, "v@:@@");
  Protocol *p;
  if ((p = objc_getProtocol("SCStreamOutput")))
    class_addProtocol(c, p);
  if ((p = objc_getProtocol("SCStreamDelegate")))
    class_addProtocol(c, p);
  objc_registerClassPair(c);
  return MSG(id)(MSG(id)((id)c, sel("alloc")), sel("init"));
}

static id find_display(id content, CGDirectDisplayID want_id) {
  id displays = MSG(id)(content, sel("displays"));
  unsigned long n = MSG(unsigned long)(displays, sel("count"));
  for (unsigned long i = 0; i < n; i++) {
    id d = MSG(id, unsigned long)(displays, sel("objectAtIndex:"), i);
    if (MSG(CGDirectDisplayID)(d, sel("displayID")) == want_id)
      return d;
  }
  return NULL;
}

static id find_self(id content) {
  id apps = MSG(id)(content, sel("applications"));
  unsigned long n = MSG(unsigned long)(apps, sel("count"));
  for (unsigned long i = 0; i < n; i++) {
    id a = MSG(id, unsigned long)(apps, sel("objectAtIndex:"), i);
    if (MSG(pid_t)(a, sel("processID")) == getpid())
      return a;
  }
  return NULL;
}

// Runs on a ScreenCaptureKit queue. SCK objects are fine to build here; the
// result is handed to the main queue.
static void build_stream(id content, CGDirectDisplayID display_id) {
  id display = find_display(content, display_id);
  if (!display) {
    fprintf(stderr, "zoomer: display %u not found for capture\n", display_id);
    return;
  }

  // Excluding our own app keeps the overlay out of the capture, otherwise it
  // would magnify itself. (SCK ignores NSWindowSharingNone since macOS 15.)
  id me = find_self(content);
  if (!me)
    fprintf(stderr, "zoomer: warning: could not exclude own window from capture\n");
  id exclude = me ? MSG(id, id)(cls("NSArray"), sel("arrayWithObject:"), me)
                  : MSG(id)(cls("NSArray"), sel("array"));
  id filter = MSG(id, id, id, id)(
      MSG(id)(cls("SCContentFilter"), sel("alloc")),
      sel("initWithDisplay:excludingApplications:exceptingWindows:"), display,
      exclude, MSG(id)(cls("NSArray"), sel("array")));

  // Capture at full pixel resolution (2x on Retina).
  CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);
  size_t width = CGDisplayModeGetPixelWidth(mode);
  size_t height = CGDisplayModeGetPixelHeight(mode);
  CGDisplayModeRelease(mode);

  id config = NEW("SCStreamConfiguration");
  MSG(void, size_t)(config, sel("setWidth:"), width);
  MSG(void, size_t)(config, sel("setHeight:"), height);
  MSG(void, OSType)(config, sel("setPixelFormat:"), kCVPixelFormatType_32BGRA);
  MSG(void, BOOL)(config, sel("setShowsCursor:"), NO); // the real one is on top
  MSG(void, CMTime)(config, sel("setMinimumFrameInterval:"), CMTimeMake(1, 60));
  MSG(void, long)(config, sel("setQueueDepth:"), 3);

  id s = MSG(id, id, id, id)(MSG(id)(cls("SCStream"), sel("alloc")),
                             sel("initWithFilter:configuration:delegate:"),
                             filter, config, output);
  MSG(void)(filter, sel("release"));
  MSG(void)(config, sel("release"));

  id error = NULL;
  if (!MSG(BOOL, id, long, id, id *)(
          s, sel("addStreamOutput:type:sampleHandlerQueue:error:"), output,
          SCStreamOutputTypeScreen, (id)dispatch_get_main_queue(), &error)) {
    print_error("could not add stream output", error);
    MSG(void)(s, sel("release"));
    return;
  }

  dispatch_async(dispatch_get_main_queue(), ^{
    stream = s;
    stream_display = display_id;
  });
}

static void step(void) {
  if (busy)
    return;

  // A stopped stream of another display is no use any more.
  if (stream && !running && stream_display != display) {
    MSG(void)(stream, sel("release"));
    stream = NULL;
  }

  if (want && !stream) {
    busy = true;
    if (!output)
      output = make_output();
    CGDirectDisplayID d = display;
    // Include off-screen windows: our overlay is hidden right now, and we
    // need our app to show up so we can exclude it.
    MSG(void, BOOL, BOOL, void (^)(id, id))(
        cls("SCShareableContent"),
        sel("getShareableContentExcludingDesktopWindows:onScreenWindowsOnly:"
            "completionHandler:"),
        NO, NO, ^(id content, id error) {
          if (content)
            build_stream(content, d);
          else
            print_error("could not list screen content (Screen Recording "
                        "permission?)",
                        error);
          dispatch_async(dispatch_get_main_queue(), ^{
            busy = false;
            if (!stream)
              want = false; // failed: don't retry in a loop
            step();
          });
        });
  } else if (want && !running) {
    busy = true;
    running = true;
    MSG(void, void (^)(id))(stream, sel("startCaptureWithCompletionHandler:"),
                            ^(id error) {
                              if (error) {
                                print_error("could not start capture", error);
                                dispatch_async(dispatch_get_main_queue(), ^{
                                  running = false;
                                  want = false;
                                });
                              }
                              done();
                            });
  } else if (running && (!want || stream_display != display)) {
    busy = true;
    running = false;
    MSG(void, void (^)(id))(stream, sel("stopCaptureWithCompletionHandler:"),
                            ^(id error) {
                              if (error)
                                print_error("could not stop capture", error);
                              done();
                            });
  }
}

void capture_start(CGDirectDisplayID d) {
  want = true;
  display = d;
  step();
}

void capture_stop(void) {
  want = false;
  step();
}
