# Fullscreen exit: content briefly stands still while the border moves

Status: cause found and fixed in the working tree (2026-10-30); awaiting the
user's confirmation on their own runs. The temporary diagnostics are still in
`GlkController+FullScreen.m`, marked `TEMPORARY`, and should be removed once
the fix is confirmed. Then delete this file.

## The problem

Sometimes, during the custom exit-fullscreen animation, the window contents
look stationary for a fraction of a second while the window border moves left.
Intermittent, seen in plain Curses, possibly only with the green button, never
in earlier harness runs (which captured at 22 fps and looked only for a
mismatch between the status bar and the window's right edge).

## Cause

The system runs its own clock for the exit transition. It gives
`window:startCustomAnimationToExitFullScreenWithDuration:` a duration
(0.5 s), and when that duration is up it moves the window from the fullscreen
Space to the desktop whatever the custom animation has done. During that Space
switch, about 100 ms, the window's picture is frozen: no Core Animation commit
from the app reaches the screen, while the window's frame on the server still
moves. Frozen picture plus moving frame is exactly the reported look.

Our animation overran that deadline on every exit:

- The step timer's clock started after the style mask change, which takes
  40–70 ms (`setStyleMask:` runs an `NSPerformVisuallyAtomicChange` that
  resizes the window). So the whole animation ran that much behind the system.
- The last step therefore landed 55–95 ms after the system's deadline, and the
  `NSWorkspaceActiveSpaceDidChangeNotification` arrived at ~517 ms, before our
  final step (553–593 ms). The commit for the second-to-last step took ~90 ms
  instead of ~15 ms, spanning the Space switch.

On this M1 the lag was one or two steps (the window 6–22 pt wider than final),
so the visible effect was a 34–86 ms hold of a slightly-too-wide picture that
then snapped to the final frame. Any extra stall on the main thread during the
exit (the user's machine, a busy game, a long layout) makes the lag larger and
the freeze obvious. That explains the intermittency.

Evidence, from 8 baseline runs with the diagnostics on (log and 60 fps band
capture; details under "Harness"):

| | before the fix | after the fix (16 runs) |
|---|---|---|
| last step (eased 1.0) | 553–593 ms | 449–467 ms |
| Space change notification | 516–522 ms | 513–520 ms |
| pre-final picture hold | 34–86 ms, window 6–22 pt too wide | 0 ms in every run |

## The fix

In `window:startCustomAnimationToExitFullScreenWithDuration:`:

- The animation's progress is measured from the moment the system calls us,
  before the style mask change, through a new
  `stepFullScreenAnimationWithDuration:from:step:completion:` (the old
  three-argument method calls it with "now", so the enter animation is
  unchanged).
- The steps run for `duration - kExitFullScreenMargin` (margin 0.05 s), so the
  final frame is set and committed about 50 ms before the system takes over.

The first step now arrives 43–79 ms after the call with eased 0.02–0.07; the
window simply stands still for the style mask change, which is a start delay,
not a content/border split.

`display:YES` in the step: the log shows each step takes about 2 ms including
the draw, and the content moves in step with the frame on every tick, so there
is no reason to revert to the `display:NO` variant. Keep it.

Ruled out by the log: no foreign frame changes during the animation (the only
"outside a step" resize is our own `setStyleMask:` call), no late ticks beyond
the first, the snapshot window from the enter is on screen but with its
content view hidden and plays no part, the system's `windowDidExitFullScreen:`
arrives after our completion and is ignored as intended.

## Confirming it on the user's machine

The diagnostics log to a file when the user default `FullscreenExitLog` names
one:

```
defaults write net.ccxvii.spatterlight FullscreenExitLog /tmp/fsexit.log
```

or `-FullscreenExitLog /tmp/fsexit.log` as a scheme argument. Each exit writes
a block: start (window/server/target frames, on-screen windows, snapshot
window), the style mask change, every step (gap since the previous one, LATE
if over 25 ms, eased value, frame set, server bounds, game view frame, time
taken, mouse), the CA commit completion per step, every window/game view frame
change outside a step with a call stack, and the notifications (will/did exit
fullscreen, Space change, screen, occlusion, key). A glitching exit would show
the step with `eased 1.000` after the `NSWorkspaceActiveSpaceDidChangeNotification`
line, or a large gap in the steps around 450–520 ms. With the fix, the last
step should always precede the Space change by ~50 ms.

To remove the diagnostics: delete everything marked `// TEMPORARY` in
`GlkController+FullScreen.m` (the `#pragma mark - Temporary diagnostics`
section, the `fsExitLog*` category methods, the hooks in the exit method and
in `windowDidEnterFullScreen:`/`windowDidExitFullScreen:`, and
`#include <unistd.h>`), then `defaults delete net.ccxvii.spatterlight FullscreenExitLog`.

## Harness

In `/private/tmp/claude-502/-Users-administrator-spatterlight/872c519e-caa6-43a5-b06c-48ccc01a76c1/scratchpad/`:

- `relaunch.sh` copies the Debug build to `iso/SpatFS.app` with bundle ID
  `net.tmp.spatfs`, its own HOME and store, the log on, and opens Curses.
- `runexit.sh N tag` runs N green-button exits from a window moved to (20, 60),
  with the time in fullscreen cycling 0.4/0.8/1.5/3/6 s, the time between the
  title-bar reveal and the click cycling 0.25/0.5/0.9/1.3 s, the pointer
  sliding to the button and moving away during every other exit.
- `reccap2 DIR SECONDS Y0 HEIGHT` captures a full-width band of the display at
  60 fps with ScreenCaptureKit into memory and writes PNGs named by ms.
  `mouse move|click|path` posts CGEvents. `act.applescript` drives the window
  through System Events with the inline bundle-ID tell.
- `chk2.py DIR` tracks the content (the longest grey run: the status bar, or
  the title quote box) against the window's right edge, flags mismatches, and
  prints the "pre-final hold": how long the last non-final picture stayed on
  screen before the final one. That hold is the glitch's measurable signature.

Harness notes kept from before: test only on an isolated copy with its own
bundle ID and inline `tell (first application process whose bundle identifier
is ...)`, checking the frontmost app before input; in fullscreen the green
button is at about (62, 46) on this 1440x900 display once the title bar is
revealed; the window can't be repositioned while in fullscreen.
