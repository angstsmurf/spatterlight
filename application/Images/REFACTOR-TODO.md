# Images — remaining items

Review of 2026-10-19; the fixes went in the same day. What is left needs a
measurement or a decision, or is not worth doing on its own. Tests:
`ImageHandlerTests` and `BufferImageRescaleTests`, plus `GameImportXCTests`
testZorkZero/testArthur/testShogun/testJourney for picture message traffic.
Run them one class at a time through the isolation rig, never the whole
bundle.

## What changed, for orientation

- `ImageHandler` is not archived (see `GlkController+Autorestore.m`), so all
  `NSSecureCoding` code went, and with it `ImageFile`: a bookmark only paid
  off across launches. That settles A18, A19, A20 and most of C1/C2.
- Split into `ImageAnimation`, `ImageResource` and `ImageHandler` (D).
  `ImageHandler.h` still imports `ImageAnimation.h`.
- The last image is read-only outside the handler. `imageForNumber:`,
  `dataForImageNumber:` and `animationForImageNumber:` leave it alone, so the
  buffer window's rescale no longer saves and restores it, and the Narcolepsy
  mask no longer reaches into `resources`.
- A failed FINDIMAGE now clears the last image instead of leaving the
  previous picture for a draw request.
- A PURGEIMG replacement is read whole. Bocfel passes the size of the pixels,
  and its TIFF keeps the IFD after them, so the old read was always short and
  only the `initWithContentsOfFile:` fallback decoded it (twice the reading).
- A purge deletes only files the handler owns (PURGEIMG replacements), not
  any non-Blorb file. A purged LOADIMAGE `PIC3`, or a Hugo resource file,
  used to be deleted.
- Blorb Rect placeholders are decoded by `ImageResource` (two 32-bit values),
  so FINDIMAGE finds them and they survive cache eviction. A Blorb with Rect
  chunks now sends one LOADIMAGE fewer per placeholder; none of the facit
  games has one.
- `lastImageLabel` had no callers and went. Alt texts stay in one handler
  table and go onto every image made for that number, purge replacements
  included (they used to lose theirs after a purge without a replacement).

## Needs a measurement

- **B5 [U].** Blorb pictures are sliced out of the game data with
  `dataForResource:` at launch. If that copies, every picture is in memory
  twice. Mapping the file (`NSDataReadingMappedIfSafe`) and loading lazily
  would cut launch memory for large Blorbs (Kerkerkruip, 452 pictures).
  Measure first.

## Needs a decision

- **Purge size.** The PURGEIMG size is now ignored. Bocfel is the only
  sender; the protocol field could carry the file length or be dropped.
- **Temp files at the end of a session.** The last replacement under each
  number is never purged, so its TIFF stays in Bocfel's temp folder. Deleting
  owned files in `dealloc` would fix it; check with an autorestore first
  that nothing reads them after the handler is gone.

## Small, low value alone

- **C1 remainder.** `ImageResource` and `SoundResource` still share the
  byte-range read (open, seek, read, close, with the pre-10.15 fallback). It
  is about 30 lines; a shared helper would have to keep `SoundResource`'s
  bookmark resolution and archive keys.
- **A13.** The frame of an animation is a property of the bitmap rep, which
  every cell that shows the image shares. Correct while drawing is
  synchronous, as `imageForFrame:` says.
- **D.** `drawableLastImage` (the zero-size check in
  `handleDrawImageOnWindow:`) and an `ImageCache` wrapper around the cost
  function: no second user yet.
