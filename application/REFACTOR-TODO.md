# application/ (top-level files) — refactor / cleanup notes

Scope: only the files directly in `application/` (main.m, main.h,
DebugLog.h, Constants.h, AppDelegate, GlkEvent, GlkWindow,
CommandScriptHandler, HelpPanelController, CoverImageHandler,
CoverImageView, RotorHandler, FolderAccess, FileSignature, ZColor and the
NS* category files). Subdirectories have their own notes.

Review 2026-10-19; first fix pass 2026-10-19 (see Done). Item numbers are
from the original review.

---

## Open

### Correctness

- **A8 GlkEvent: `Theme` is not encoded.** It is a managed object and can't
  go into the archive as-is. An event restored from an autosave gets its
  theme from the controller; check that every decode path sets it before
  `writeEvent:` uses `theme`.
- **A20 RotorHandler: casts every non-graphics window to `GlkTextGridWindow`**
  to read `.textview`. It works because grid and buffer windows both have
  `textview`. A `textview` property on GlkWindow (nil for graphics) would
  remove the casts.

### Dead code

- **B2** About 20 `GlkWindow` methods only `NSLog` "not implemented" for
  subclasses to override. One `-subclassResponsibility:` helper, or
  removal plus `respondsToSelector:` at the call sites.
- **B3** Possibly unused imports in `NSImage+Categories.m` and
  `CoverImageView.m`. Not checked.
- **B6** `NSColor+integer colorFromData:` is only used by the legacy
  defaults migration in `Preferences/BuiltInThemes.m`. Keep it; a comment
  saying so would help.
- **B8** `FolderAccess listActiveSecurityBookmarks` is called from
  `applicationWillTerminate:` and only logs. With DebugLog.h imported, the
  loop runs with no output in release builds; it could be `#ifdef DEBUG`.
- **B10** `main.h` is an umbrella of imports used by main.m and
  AppDelegate.m. Folding it away is optional.

### Duplication

- **C4** `NSString oldSignatureFromFile` repeats FileSignature's Blorb chunk
  walk. **Do not merge them**: the old algorithm has to give the old
  answer, because GameImporter.m and GlkController+Autorestore.m match
  existing library entries against it.
- **C13** RotorHandler: the "filter, then build a result with
  `customLabel`/`targetRange`" tail is still written out per rotor.
- **C14** GlkWindow: repeated `isKindOfClass:` checks for grid, buffer or
  graphics windows; `-isGrid`/`-isBuffer` or a `type` property.

### Structure

- **D1** AppDelegate.m is large. Split into categories (menus, defaults,
  restoration), as was done for GlkController.
- **D2** Globals declared in Constants.h are defined elsewhere
  (`globalBookmarks` in FolderAccess.m, `gGameFileTypes` and others in
  AppDelegate.m). Move each extern into its owner's header, or add a
  Constants.m. If `globalBookmarks` stays behind FolderAccess's class
  methods, it can become `static`.
- **D3** FolderAccess keeps its state in a file-global dictionary. A
  singleton store would be easier to test.
- **D4** CoverImageHandler mixes view creation, window animation and the
  full-screen transition; the three cover styles could be separate.
- **D5** RotorHandler holds five unrelated rotors in one file. The step
  helper is now shared, so a per-rotor category split is mechanical.
- **D6** NSString+Categories mixes signatures, Blorb parsing, filename
  cleaning and display formatting.

### Minor

- **E3** NSDate+relative creates an `NSDateFormatter` per call. It sets
  different styles on each path, so a cache needs one formatter per style.
  Left alone: the callers aren't hot.
- **E5** CoverImageHandler: `performSelector:…afterDelay:0.0` on
  `deferredRestart:` and the "FIXME: Just fork the interpreter NSTask
  here". This falls under the "Do not touch" timing below.
- **E6** CoverImageHandler uses `CGWindowListCreateImage`, deprecated since
  macOS 14. Its replacement, ScreenCaptureKit, is asynchronous, so moving to
  it means reworking the snapshot hand-off.
- **E7** FolderAccess `needsPermissionForURL:` reads the file on macOS 15
  and later to test permission. This is deliberate: the plain readability
  check gives the wrong answer there. Reading only the first byte, or using
  `fileHandleForReadingFromURL:`, would be cheaper.
- **E8** Animation timings in CoverImageHandler (0.2, 0.3, 0.5, 1.5, 3 and 4
  s) and HelpPanelController's +40/+81 are unnamed. The latter already has
  a comment.
- **E9** FolderAccess NSLogs pass `NSInteger` to `%ld` without a `(long)`
  cast.

## Done (2026-10-19)

- **A1** AppDelegate window restoration: tests `hasPrefix:@"infoWin"` and
  `@"gameWin"` instead of calling `substringToIndex:7` on any identifier.
- **A2** `continueUserActivity`: returns NO when the URI or the object ID is
  nil, instead of calling `objectWithID:nil`.
- **A3** `openDocument:` gives the panel no starting directory when the
  GameDirectory default is unset, instead of `fileURLWithPath:nil`.
- **A4** `helpLicenseWindow` is strong.
- **A5** Removed the trailing `;` from the local DEBUG `NSLog` macro. The
  macro stays local: it writes to stderr on purpose.
- **A6/A7** GlkEvent: `struct message reply = {0}`, the settings buffer is
  zeroed, and a `_Static_assert` checks that `settings_struct` fits the
  4096-byte buffer. Writes go through `writeAll()`, which retries on EINTR
  and short writes and skips the payload if the header write failed. The
  wire format is unchanged.
- **A9** `getCurrentAttributesForStyle:` falls back to `style_Normal` when
  the style is out of range.
- **A11** CommandScriptHandler encodes `commandIndex - 1` only when the
  index is above 0, so it no longer wraps around. The decode is unchanged:
  the step back is intended, so that the command in progress is resent
  after an autorestore.
- **A12** FolderAccess bookmarks: secure unarchive with an explicit class
  list into a mutable copy. Saves use
  `archivedDataWithRootObject:requiringSecureCoding:YES` with an atomic
  write, and errors are logged. FolderAccess now declares `<NSSecureCoding>`
  in its class extension. It had `supportsSecureCoding` but not the
  protocol, and the secure decoder refuses classes that don't conform.
  Checked with a standalone harness: a dictionary written with the old
  `archiveRootObject:toFile:` loads with the new code, and so does one
  written by the new save.
- **A13** `existingAncestor:` matches on whole path components.
- **A14** `suitableDirectoryForURL:` clamps the home path to
  `MIN(3, count)` components.
- **A16** `getVolumeNameForURL:` sets `value` to nil before use.
- **A17** `startAccessing` sets `_active` only after
  `startAccessingSecurityScopedResource` succeeds.
- **A21** RotorHandler: `bestMatchRange` is initialised.
- **A22** The links rotor and the command-history rotor check the string
  length before `characterAtIndex:`.
- **A23** A Colder Light's window order is used only if all four windows
  exist.
- **A24/B1** Removed the `dateBySubtractingDays:` declaration, which had no
  implementation and no callers.
- **A25** `isJPEG` tests for `FF D8 FF`, so Exif files are recognised. This
  matters to the iFiction export in MetadataHandler, which names the cover
  type. `isPNG` compares the 4-byte magic without a malloc.
- **A26** HelpPanelController uses `CGFLOAT_MAX`.
- **B4/B5** CoverImageHandler: fixed the file header, removed the empty class
  extension and moved the unused `__block imageView` into the block.
- **B7** Not dead: `themesMenuItem`, `journeyPartyMenuItem` and
  `journeyIndividualCommandsMenuItem` are all connected in MainMenu.xib and
  used.
- **B9** Removed the redundant `return;` at the end of
  `forceAccessDialogToURL:`.
- **C1** `-showLibraryOrOpenDocument` is shared by
  `applicationOpenUntitledFile:` and `applicationDidBecomeActive:`.
- **C2** CommandScriptHandler: `SPConfirmUnlessSuppressed()` builds both
  "don't show again" alerts. `-windowWithLineRequest` is shared by
  `findGlkWindowWithInput` and `restoreFromSaveFile:`. Alert texts are
  localised.
- **C3** NSBitmapImageRep+retro: `SPRepFromZImage()` builds the CGImage for
  both NEO and MG1, with a `__bridge` cast. This answers E10: the file is
  ARC.
- **C5** `SPTruncatedTitle()` is shared by both titles in
  `stringWithSummaryOfGames:` and no longer cuts a composed character in
  half.
- **C6** `scrubInvalidCharacters` is table-driven. The two literal “ and ”
  entries were the same characters as `“` and `”`, so they were
  dropped.
- **C7** `+restoreURL:force:` is shared by `restoreURL:` and
  `forceRestoreURL:`.
- **C8** `askForAccessToURL:` uses `forceAccessDialogToURL:` instead of
  its own copy of the open-panel setup.
- **C9/C10** CoverImageHandler: `SPBorderlessWindow()` makes all four
  borderless windows, and `SPOpacityAnimation()` builds the three fades.
  The timings are unchanged.
- **C11/C12** RotorHandler: `SPRotorStep()` replaces the four pasted
  next/previous blocks, and `SPRotorLabelTrimSet()` is a static set.
- **D7** Covered by the static assert in A7. `struct message` itself comes
  from `protocol.h`, so it can't drift.
- **E1** DebugLog.h is imported by CommandScriptHandler, CoverImageHandler,
  FileSignature, FolderAccess, GlkEvent, NSManagedObjectContext+safeSave,
  NSString+Categories and RotorHandler. The seven `linksRotor` NSLogs, which
  ran on every VoiceOver query, are gone.
- **E2** Fixed the stale file headers (CoverImageHandler,
  NSManagedObjectContext+safeSave). The NSData+Categories.h signature
  comment is accurate.
- **E4** RotorHandler: "Image with link I.D. %@" and "image" are localised.

Checked and not a problem:

- **A10** `saveAsRTF:` is overridden in GlkTextBufferWindow; the base class
  never casts.
- **A15** In `askForAccessToURL:` the block runs when access is granted or
  not needed, and is skipped only when the user cancels the panel.
- **A18** The strong `self` in the delayed blocks lasts at most 4 s.
- **A19** Observers registered with `addObserver:selector:` are dropped
  automatically on dealloc (macOS 10.11+).
- **C15** HelpPanelController: nothing to do.

## Do not touch

- `GlkEvent` wire format and `struct message` layout: shared with the
  interpreter processes (glkimp). Change them only together with
  `protocol.h`.
- `CoverImageHandler` full-screen transition timing and `deferredRestart:`
  ordering: fragile, and only testable by hand.
- The `RotorHandler` comment about always offering the command-history rotor
  (VoiceOver caches the rotor set at focus time): don't gate it on
  `largestWithMoves`.
- `NSString oldSignatureFromFile`: see C4.
