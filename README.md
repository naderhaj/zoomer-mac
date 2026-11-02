# zoomer

A tiny screen magnifier for macOS. Hold **Cmd+Alt+Shift** and scroll to zoom
toward the cursor; move the mouse to pan. Scroll back out or press **Esc** to
close the zoom.

It zooms whichever display the cursor is on, shows the live screen (video,
typing, everything keeps updating), and stays out of the way: no Dock icon,
no menu bar, clicks go straight through.

Written in plain C (~600 lines) talking to AppKit, ScreenCaptureKit and Core
Animation through the Objective-C runtime.

## Requirements

- macOS 12.3 or later (ScreenCaptureKit), on Apple Silicon
- Xcode Command Line Tools (`xcode-select --install`)

## Build and run

```sh
make          # builds ./zoomer
make run      # builds and runs it
make install  # copies it to /usr/local/bin (PREFIX=... to change)
```

## Permissions

zoomer needs two permissions, both in **System Settings > Privacy & Security**:

- **Accessibility**, to see the shortcut and the scroll.
- **Screen & System Audio Recording**, to capture the screen it magnifies.

When you run it from a terminal, macOS grants these to the terminal app, not
to zoomer. After granting Screen Recording, restart the terminal.

## Options

```
zoomer [-r] [-l]

  -r  reverse the scroll direction
  -l  smooth (linear) filtering instead of crisp pixels
  -h  show this help
```

Quit with Ctrl-C.

## How it works

- `tap.c`: a global event tap catches Cmd+Alt+Shift+scroll, mouse moves and
  Esc, and works out which part of the display to show.
- `capture.c`: a ScreenCaptureKit stream of the display, excluding zoomer's
  own window so it doesn't magnify itself.
- `overlay.c`: a borderless, click-through window whose layer shows the
  captured frames directly (no copies), cropped with `contentsRect`.

## License

MIT
