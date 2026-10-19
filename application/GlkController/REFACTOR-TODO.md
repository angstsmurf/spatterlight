# GlkController: cleanup / refactor / dedup TODO

Survey of `application/GlkController/` (read-only pass; no code changed).
Line numbers are approximate (taken at survey time, on master 494c24224).
Priority: **P1** suspected bugs, **P2** real duplication, **P3** structure, **P4** hygiene / dead code.

Files: GlkController.m (2259), +GlkRequests (1435), +Autorestore (943), +FullScreen (558),
+Speech (531), +InterpreterGlue (366), +MenuItems (153), +BorderColor (89), GlkController_Private.h.

---

## P1: Suspected bugs (verify first)

- [x] **FullScreen `centerWindowFrame`**: `screen.frame.origin.x + self.borderFullScreenSize.height` looks like it should be `origin.y`; it then adds `screen.frame.origin` again, so the offset is double-counted on secondary displays.
- [x] **MenuItems.m:134**: `NSLocalizedString(@"Disike", nil)`. Typo in the key; the localized "Dislike" title is never found. Check the .strings files before fixing.
- [x] **InterpreterGlue `noteDataAvailable:`**: `maxibuf` leaks on the `unibuf == NULL` error return. Also `write()` results are unchecked, and it uses `goto again`.
- [x] **GlkRequests `handleChangeTitle:` / `handleShowError:`**: `[@(buf) substringToIndex:…]` result is discarded (no-op statement); `buf[len]='\0'` is also used.
- [x] **GlkRequests `handleOpenPrompt:` / `handleSavePrompt:`**: `waitforfilename = NO` is set right after the asynchronous `beginSheetModal…`, so the flag is false while the sheet is up. Check how `queueEvent:` (`waitforfilename` → queue) relies on it.
- [x] **GlkRequests `handleSetTimer:`**: Kerkerkruit branch calls `[timer invalidate]` on an already-nil timer.
- [x] **GlkRequests `handlePrintOnWindow:`**: `i > 1` (maybe `> 0`), odd `len = (size_t)i;`, `isEqualTo:[NSNull null]`.
- [x] **Speech `saveAsRTF:`** casts every non-graphics window to `GlkTextBufferWindow` (grid windows are a different class). Works only because both expose `textview`; use `isKindOfClass:` or a shared accessor.
- [x] **Speech `largestWithMoves`** same pattern: casts non-graphics windows (including buffer windows) to `GlkTextGridWindow` to read `.textview.string`.
- [x] **GlkController `contentSizeToCharCells:`** mutates `_theme` as a side effect.
- [x] **Autorestore `autoSaveOnExit` / `handleAutosave:`** check only `error`, not the BOOL return of the archive/write call.
- [x] **Autorestore `encodeWithCoder`** mutates `self.firstResponderView` as a side effect of encoding.
- [x] **Autorestore `buildAppSupportDir`** falls back via `gFolderMapExt` only because messaging nil returns nil; make the intent explicit.

## P2: Duplication to remove

### Autorestore / autosave files
- [x] One central helper for the autosave filenames (`autosave-GUI.plist`, `-GUI-late.plist`, `autosave-bak.plist`, `autosave.glksave`, `autosave-bak.glksave`, …). Currently literals are spread across GlkController.m (`runTerp`, `runTerpWithAutorestore`), Autorestore (`deleteAutosaveFiles` lists `autosave-GUI.plist` twice) and InterpreterGlue (`noteTaskDidTerminate:`). Suggest `-appSupportFile:` + constants.
- [x] File creation-date fetch (`attributesOfItemAtPath` + `NSFileCreationDate`) appears 3+ times in `runTerpWithAutorestore` → `-creationDateOfFile:`.
- [x] Deprecated `unarchiveObjectWithFile:` inside `@try`, 3× → one `-unarchiveGUIFile:` helper using the secure API.
- [x] `autoSaveOnExit` and `handleAutosave:` share archive-and-write logic.
- [x] `terminateTask` (GlkController.m) overlaps with the termination block in `reset:` (Autorestore).
- [x] `waitForData` (Autorestore) vs `pollMoreData` (InterpreterGlue): both are `select()` wrappers; merge with a timeout parameter.
- [x] `restoreUI`: quote-box handling duplicated in its two branches.
- [x] `buildAppSupportDir` rebuilds `gFolderMap` / `gFolderMapExt` on every call → `static`/`dispatch_once`.

### Window / geometry
- [x] Fullscreen test `(self.window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen` repeated in GlkController.m, FullScreen, Autorestore → `-isInSystemFullscreen`.
- [x] `zoomIn:` / `zoomOut:` / `zoomToActualSize:` are near-identical.
- [x] `zoomContentToSize:` and `noteDefaultSizeChanged:` duplicate windowed/fullscreen frame math (clip to screen, offset origin.y, new-frame computation).
  - Windowed branch shared as `-windowFrameForContentSize:screenHeightMargin:`. Fullscreen branches left alone: their height math really differs (borderView height with vs. without borders).
- [x] `sendArrangeEventWithFrame:force:` duplicates the `initArrangeWidth:` event creation in `contentDidResize:` and `notePreferencesChanged:`.
- [x] `noteColorModeChanged:` / `notePreferencesChanged:` duplicate light/dark/game.theme resolution → one `-resolvedTheme`.
- [x] `nohacks`-or-`identifyGame` branch appears in `runTerp` and `notePreferencesChanged`.
- [x] `startGameInFullScreenAnimationWithDuration:` duplicates border_finalFrame/center-frame setup from `enterFullScreenAnimationWithDuration:`.
  - Shared setup in helpers (see P2); the second animation step is now `-startGameInFullScreenEnlargeToFrame:duration:showAlert:`.
- [x] Autoresizing-mask combinations repeated many times in FullScreen → named constants.
- [x] "Largest window by area" loop exists three times: Speech `speakLargest:`, Speech `largestWithMoves`, BorderColor `largestWindow` (plus `framePending ? pendingFrame : bounds` size logic in BorderColor ×2). One helper `-areaOfWindow:` / `-largestOf:`.
- [x] Loops `for win in gwindows.allValues if isKindOfClass:GlkTextBufferWindow` (markLastSeen, performScroll, storeScrollOffsets, restoreScrollOffsets, speakMostRecentAfterDelay, checkZMenuAndSpeak, …) → `-bufferWindows`; same for grid windows (`statusWindow`, `saveAsRTF:`, `validateMenuItem:`).
- [ ] `guessFocus` and NEXTEVENT focus code partly redundant.
  - Declined for now: the two paths run at different points in the event cycle and the ordering is subtle; only the buffer-window loop was shared (`-bufferWindows`).

### GlkRequests
- [x] `if (!self.gwindows.count && shouldRestoreUI) { windowsToRestore = …; [self restoreUI]; reqWin = … }` repeated 6× (PRINT, UNPRINT, INITLINE, INITCHAR, INITMOUSE, INITLINK) → `-windowForRequest:` helper.
- [x] OKAY filename reply (`reply.cmd=OKAY; len; write; write`) in `handleOpenPrompt:`, `feedSaveFileToPrompt`, `handleSavePrompt:`.
- [x] `handleChangeTitle:` / `handleShowError:` share string decoding.
- [x] `handleNewWindowOfType:`: 3 near-identical case bodies.
- [x] `handleStyleHint…` / `handleClearHint…` duplicated switches.
- [x] BANNERCOLS / BANNERLINES duplicate; BEEP sound-play code duplicated.
- [x] SIZWIN / DRAWIMAGE: malloc+memcpy and `free` on every early exit → stack struct copy.
- [x] `handlePrintOnWindow:`: malloc+memcpy plus three copy-pasted memmove-removal branches → one compaction loop.
- [x] `unicodeAvailableForChar:` scans all fonts on every CANPRINT → cache; NSData/UTF-32 trick can be simplified.

### InterpreterGlue
- [x] `queueEvent:` repeats `EVTKEY||EVTLINE||EVTHYPER||EVTMOUSE` test twice → `isInputEvent`; its three-way branch can be simplified.
- [x] Inline protocol logging in `noteDataAvailable:` → extract; PRINT logging malloc+memcpy's what it could read in place; `@try` around log writes used inconsistently.

### Speech / MenuItems / BorderColor
- [x] `like:` / `dislike:` are the same toggle with 1 vs 2 → `-toggleLike:(int)`; same duplication in `validateMenuItem:` (title/state pairs).
- [x] `speakString:` removes `\0` twice (charset trim + `stringWithCharacters:&nc`); only the trim/replace is needed.
- [x] `dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(x * NSEC_PER_SEC)), main_queue, ^{…})` repeated in GlkController.m, Speech (3×), others → `-afterDelay:perform:`.
- [x] Notification-name literals (`"PreferencesChanged"`, `"DefaultSizeChanged"`, `"BorderChanged"`, `"ColorModeChanged"`) used in GlkController.m, MenuItems, etc. → `extern NSString *const`.
- [x] `#ifndef DEBUG #define NSLog(...) #endif` copy-pasted into every .m → shared header (e.g. GlkController_Private.h).
  - Done app-wide rather than only here: application/DebugLog.h replaces the copy in all 31 .m files (the app, SpatterlightQuickLook/InfoView.m, Treaty of Babel IFStory.m / IFDBDownloader.m). The header imports Foundation itself, so it can't define the macro before Foundation's `NSLog` declaration. AppDelegate, GlkGraphicsWindow and GlkTextGridWindow keep a local `#ifdef DEBUG` override that logs to stderr without the timestamp.

## P3: Structure

- [x] `handleRequest:` is a ~725-line switch → per-command handler methods (NEXTEVENT, SIZWIN, INITCHAR, INITLINE, BEEP, REFRESH, …) with a dispatch table; remove stray `#pragma mark` inside the switch.
  - The long cases are now methods (`#pragma mark Per-request handlers`): `-handleNextEvent:reply:` (returns whether to stop reading), `-handleDeleteWindow:name:`, `-handleBeep:…`, `-handleSizeWindow:rect:`, `-handleSetBackground:…`, `-handleDrawImageOnWindow:rect:`, `-handleInitLine:buffer:`, `-handleCancelLine:reply:buffer:`, `-handleInitChar:`, `-handleRefreshWindow:…`, `-handleMenuItem:buffer:`, `-dismissExpiredQuoteBox`. `-windowForRequest:` does the `a1` bounds check once.
  - Kept the switch instead of a dispatch table: the handlers take different arguments (some the reply, some the buffer, some a struct copied out of it), and a table of selectors would have meant boxing them all into one signature. Every case is now a few lines, so the switch is the table.
  - Stray `#pragma mark` lines inside the switch are plain comments; the switch sets `BOOL stop` and records `lastRequest` once at the end.
- [x] `runTerpWithAutorestore` (~310 lines) → move to the Autorestore category and split (decide-source / load-GUI / compare-tags / restore). `TempLibrary` shim class at the top of GlkController.m goes with it.
  - Now in +Autorestore (`#pragma mark Autorestore at launch`), declared in GlkController+Autorestore.h, together with the `TempLibrary` shim (decoded by class name, so its file doesn't matter). Steps: `-loadRestoredControllers`, `-checkTerpAutosave` (NO → start normally), `-usePreviousTerpAutosave` (swap in the -bak pair, game state first), `-adoptRestoredTheme`, `-adoptRestoredWindowFrame`.
- [x] `runTerp:` (~300 lines) → helpers: theme fallback, `setupStyleHints`, `registerNotifications`, border/mask setup, autorestore decision.
  - `-adoptFallbackThemeIfNeeded`, `-resetStyleHints`, `-registerForNotifications`, `-setUpBorderAndGameView`, `-startInterpreterOrAutorestore`, in that order, right after `runTerp:`.
- [x] `identifyGame:` (~110-line if/else of IFID `isEqualToString:`) → static dictionary IFID→`kGameIdentity` plus prefix rule (LEVEL9-001/002/019).
- [x] `runTerpNormal` window-cascade loop (L835-860) → `-cascadeWindowFrame:`.
- [x] `forkInterpreterTask`: extract `openInterpreterLog` (pair with `closeLogFile`), and bocfel/tads argument building.
  - `-openLogFile` sits next to `-closeLogFile` in InterpreterGlue; arguments built by `-interpreterArgumentsForTheme:`.
- [x] `enterFullScreenAnimationWithDuration:` 5-level nested blocks → step helpers.
  - Each completion handler calls the next step through `weakSelf`: `-enterFullScreenEnlargeToFrame:duration:` → `-enterFullScreenPositionGameViewWithDuration:` → `-enterFullScreenFadeOutSnapshotWithDuration:` → `-enterFullScreenExtendGameView`.
- [x] `restoreUI` long → split; `dealWithOldFormatAutosaveDir` deeply nested migration shim reading whole files only to rewrite them: use `moveItem`, or schedule removal.
  - `restoreUI` keeps its order of operations and calls `-chooseRestoredControllerForCommandScript`, `-adoptRestoredSoundHandler`, `-adoptRestoredWindows` (→ `-adoptRestoredWindow:` per key), `-adoptRestoredJourneyMenus`, `-finishRestoredWindows`.
  - `dealWithOldFormatAutosaveDir` flattened (early return, per-file `-migrateOldFormatAutosaveFile:toDirectory:`). It still copies via NSData instead of `moveItem`: an atomic write gives the file a fresh creation date, and autorestore compares creation dates, so `moveItem` would change behaviour for a path nobody can easily test. Removing the shim is the user's call.
- [x] `findGridWindowIn:` weak block-recursion → plain recursive method.
- [x] `windowWillClose:` mixes many responsibilities (try/catch around `removeObserver`).
  - The `@try` is gone: a new `observingVoiceOver` ivar records whether the VoiceOver KVO observer was added (also stops it being added twice). Handing the current game to another window is `-passCurrentGameToAnotherSession`.
- [x] Move `showingInfocomV6Menu`, `gameTitle` (Speech) and the `journeyMenuHandler` / `infocomV6MenuHandler` lazy getters (GlkRequests) next to their ivars/owner.
  - Lazy getters moved to GlkController.m (`#pragma mark Menu handlers`). `showingInfocomV6Menu` / `gameTitle` stay in Speech: they are VoiceOver accessors used only by speech code.
- [x] `GlkHelperView` out of GlkController.m into its own file.
  - Now GlkHelperView.h/.m. GlkController.h imports GlkHelperView.h, so existing users (tests, Preferences) need no change.
  - No project.pbxproj edit was needed: application/GlkController/ is a synchronized folder group, so new files in it are picked up automatically.

## P4: Headers, dead code

- [x] GlkController.h exposes internal properties (`windowsToBeAdded/Removed`, `stashedTheme`, `restoredStylesStale`, `lastLayoutChurnTimestamp`, …) → move to Private.h; `performScroll` declared in both headers.
  - The five listed properties moved to Private.h (every user, including the tests, already imports it); `performScroll` is now declared once.
- [ ] Many `@public` ivars used across categories → properties or Private.h only.
  - Not done: touches nearly every category method; worth its own pass.
- [x] `MAXWIN` `#define` → `static const`; `kGameIdentity` typedef sits mid-@interface.
  - Now `kMaxGlkWindows` in Private.h (must match glkimp/connect.c); typedef moved above the @interface.
- [x] Dead / noise: commented-out `wintypenames` / `stylehintnames` arrays (top of GlkController.m); empty `/* * */` blocks (GlkController.m, Autorestore); redundant `setCommandScriptHandler:` override; doubled `[Preferences instance]` lookups; `if (Preferences.instance)` after use (also MenuItems `applyTheme:` nil check); commented logging in `deleteFiles:`.
- [x] Deprecated constants: `NSOnState`/`NSOffState` (MenuItems) → `NSControlStateValueOn/Off`.
- [x] FullScreen: untyped `window` parameter in `window:startCustomAnimationToExitFullScreenWithDuration:`; FIXME Narcolepsy resize hack in `windowDidExitFullScreen:`.
  - Parameter typed. Narcolepsy hack left as is (game-specific workaround, no repro to verify a change).
- [x] `charCellsToContentSize:` NaN guard: document why or fix upstream.
  - Documented as defensive; no known upstream source.

## Suggested order
1. P1 items (small, verifiable).
2. Autosave filename helper + `-isInSystemFullscreen` + `-bufferWindows` (cheap, wide effect).
3. `windowForRequest:` helper in GlkRequests, then split `handleRequest:`.
4. Move/split `runTerpWithAutorestore`, `runTerp`, `identifyGame:`.
5. Header hygiene.

Verify each step with the existing Spatterlight build + the autosave tests (see project test notes); don't run the whole XCTest bundle.
