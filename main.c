#include "objc.h"
#include "overlay.h"
#include "tap.h"

#include <ApplicationServices/ApplicationServices.h>
#include <dispatch/dispatch.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

enum { NSApplicationActivationPolicyAccessory = 1 };

static void usage(FILE *out) {
  fprintf(out, "usage: zoomer [-r] [-l]\n"
               "\n"
               "Hold Cmd+Alt+Shift and scroll to zoom toward the cursor.\n"
               "Scroll back out or press Esc to close the zoom.\n"
               "\n"
               "  -r  reverse the scroll direction\n"
               "  -l  smooth (linear) filtering instead of crisp pixels\n"
               "  -h  show this help\n");
}

// Handle a signal on the main queue instead of in a signal handler, so we
// can do anything there, not just async-signal-safe things.
static void on_signal(int sig) {
  signal(sig, SIG_IGN); // stop the default action (terminate)
  dispatch_source_t s = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, sig,
                                               0, dispatch_get_main_queue());
  dispatch_source_set_event_handler(s, ^{
    printf("%szoomer: bye\n", sig == SIGINT ? "\n" : "");
    exit(0);
  });
  dispatch_resume(s);
}

int main(int argc, char *argv[]) {
  bool reverse = false, linear = false;

  int opt;
  while ((opt = getopt(argc, argv, "rlh")) != -1) {
    switch (opt) {
    case 'r':
      reverse = true;
      break;
    case 'l':
      linear = true;
      break;
    case 'h':
      usage(stdout);
      return 0;
    default:
      usage(stderr);
      return 2;
    }
  }
  if (optind < argc) {
    fprintf(stderr, "zoomer: unexpected argument: %s\n", argv[optind]);
    usage(stderr);
    return 2;
  }

  // When run from a terminal, macOS asks for both permissions on behalf of
  // the terminal app, not zoomer itself.
  const void *keys[] = {kAXTrustedCheckOptionPrompt};
  const void *vals[] = {kCFBooleanTrue};
  CFDictionaryRef opts =
      CFDictionaryCreate(NULL, keys, vals, 1, &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);
  bool trusted = AXIsProcessTrustedWithOptions(opts);
  CFRelease(opts);
  if (!trusted) {
    fprintf(stderr, "zoomer: needs Accessibility permission to see the "
                    "shortcut.\n  Grant it in System Settings > Privacy & "
                    "Security > Accessibility,\n  to your terminal app, then "
                    "run zoomer again.\n");
    return 1;
  }
  if (!CGPreflightScreenCaptureAccess()) {
    CGRequestScreenCaptureAccess();
    fprintf(stderr, "zoomer: needs Screen Recording permission to zoom.\n"
                    "  Grant it in System Settings > Privacy & Security > "
                    "Screen & System Audio Recording,\n  to your terminal "
                    "app, then restart the terminal and run zoomer again.\n");
    return 1;
  }

  // A background app: no Dock icon, no menu bar, never takes focus.
  id app = MSG(id)(cls("NSApplication"), sel("sharedApplication"));
  MSG(void, long)(app, sel("setActivationPolicy:"),
                  NSApplicationActivationPolicyAccessory);

  overlay_init(linear);
  if (!tap_init(reverse)) {
    fprintf(stderr, "zoomer: could not create the event tap\n");
    return 1;
  }

  on_signal(SIGINT);
  on_signal(SIGTERM);

  printf("zoomer: hold Cmd+Alt+Shift and scroll to zoom, Ctrl-C to quit\n");
  MSG(void)(app, sel("run")); // never returns
  return 0;
}
