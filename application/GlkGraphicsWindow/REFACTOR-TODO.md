# application/GlkGraphicsWindow — refactor / cleanup notes

Scope: `GlkGraphicsWindow.{h,m}` and `SubImage.{h,m}`. First review
2026-10-19; first pass done the same day (see "Done" at the bottom).
Tests: `SpatterlightTests/GraphicsFillTests` (12 cases). Run them in the
isolation rig, and that class only, never the whole bundle.

## Open

1. **Shared drag-out-as-PNG helper.** `mouseDown:`'s drag setup,
   `pasteboard:item:provideDataForType:`, the file-promise delegate methods
   and `pngData` are a near copy of `Library window/ImageView.m:931-1051`
   (the buffer window's `MyAttachmentCell`/`MarginImage` have more of the
   same). Extract e.g. `ImageDragSource`, taking a block that returns the
   image and a base file name. **Keep the pre-10.14 pasteboard path:** the
   release configs set `MACOSX_DEPLOYMENT_TARGET[sdk=macosx13*…26*] = 10.13`.
   Do this together with the Library window TODO.
2. **Hoist input helpers into `GlkWindow`.** The "pass the key to a window
   that wants focus" loop in `keyDown:` and the `sendCharEvent:` /
   `sendMouseEventAtPoint:` pair are also in `GlkTextGridWindow` (see its
   TODO, C10). Do it once both windows are done.
3. **The window's `accessibilityLabel` goes stale.** It is set to the first
   described image's label (or "Image" for the placeholder) and never reset
   when `clear` or a prune removes that image. Changes what VoiceOver says,
   so check it with VoiceOver running.
4. **Autosave archives the whole canvas as an `NSImage`** on every
   autosave (`encodeWithCoder:`). Profile with a Zork Zero-sized window
   before doing anything about it; the format has to stay readable.
5. **`drawImage:`'s `maxwidth:` and `style:` parameters are unused here**,
   and `val1:`/`val2:` mean x/y. The signature is shared with
   `GlkWindow` and the buffer window, so change it in all of them or not
   at all.
6. **Optional: a `GlkCanvas` class** (context, size, scale, `fillPixels`,
   `drawInCanvas`, snapshot cache, rebuild) so the pixel code can be tested
   without a view. The rest of the window (dirty rects, sub-images, input)
   would stay. Needs new files in the pbxproj.
7. The local stderr `NSLog` macro is also in `GlkTextGridWindow.m` and
   `AppDelegate.m`. Moving it into `DebugLog.h` would change debug logging
   for every file, so it stays local unless the user wants that.

## Checked, no action

- Rounding: `fillPixels:` rounds with `lround`, the canvas with `ceil`;
  full-window fills pass `ceil(points)`, which always reaches the last pixel
  column/row after clipping. No sliver.
- `-saveAsRTF:` override is needed: `GlkController saveAsRTF:` can pick a
  graphics window, and the `GlkWindow` version assumes a text view.
- `image` stays publicly settable: `GraphicsFillTests` restores through it.

## Done (2026-10-19)

- `setFrame:` rebuilt as `rebuildCanvasWithSize:`: a **transparent window's
  canvas is now resized too** (before, it kept its old size, and the old
  image was drawn over itself, adding alpha to soft edges on each resize);
  the old contents are blitted directly instead of going through
  `drawImage:`, so a resize no longer **prunes every sub-image** or sets
  `showingImage` on a blank window. Sub-images move with the contents and
  are dropped only when wholly outside.
- `viewDidChangeBackingProperties` rebuilds the canvas at the new backing
  scale (also covers a canvas made before the view joined a window).
- Restore: `subImages` decoded with `SubImage` as an allowed class, nil
  falls back to an empty array, each gets the window as its accessibility
  parent (nothing set it before). A lone legacy "Image" placeholder is
  dropped at decode time.
- `SubImage`: `+imageWithLabel:parent:frame:`; role is set in `init` and so
  survives decoding; uuid decoded as `NSString`, fresh one if missing;
  `setAccessibilityFocused:` calls super, uses `NSView`, no longer imports
  the window header.
- The "Image" placeholder is a separate ivar, not in `subImages`: `images`
  and `accessibilityChildren` no longer add to or remove from the model,
  and the placeholder's uuid stays stable for the rotor.
  `accessibilityChildren` builds one array instead of O(n²) copies.
- `rect:coversRect:` guards a zero-area old rect (was a 0/0 NaN).
- `markDirtyRect:` used by `fillRects:`, `drawImage:`, `markWholeWindowDirty`;
  `clear` uses `removeAllObjects` + `markWholeWindowDirty`.
- Imagerule resolution is the static `SPResolveImageSize()`.
- Input: `sendMouseEventAtPoint:` / `sendCharEvent:` (mouse click and
  VoiceOver click now both call `markLastSeen`); key forwarding grabs focus
  before passing the key, as the grid window does; `mouseDown:` returns
  early without a window.
- Removed: dead `applyZColorsAndThenReverse:` override (never called on a
  graphics window), `workQueue` property + setter (lazy ivar in the
  file-promise delegate), duplicate `<NSSecureCoding>` in the extension,
  zero-default ivar assignments, trailing `;` in the `NSLog` macro, the
  unreachable `NSLog` in `saveAsRTF:`. File-name derivation is
  `imageBaseFileName`.
- Drag threshold 0.3 → 3 points, so a click with a pixel of wobble is no
  longer turned into an image drag; `Library window/ImageView.m` likewise.
- New tests: resize keeps contents top-left (opaque and transparent),
  sub-images move with a resize and are pruned by a later draw,
  WidthRatio + AspectRatio scaling, whole-window archive round-trip with
  sub-image uuid/role/parent.
