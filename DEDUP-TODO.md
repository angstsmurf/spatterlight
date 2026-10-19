# Deduplication candidates (Objective-C, app + extensions + tests)

Survey only, 2026-10-20. No code was changed. Scope: Spatterlight's own `.m` files. `terps/`, `glkimp/`, `SFBAudioEngine/` and `ImageHashing/` are excluded.
Findings come from three read-only scans. Line numbers were checked at the time of the scan and will drift. Re-check before editing.
Note: `GlkTextGridWindow*` files were mid-edit in the working tree at scan time.
Any new `.h`/`.m` needs a pbxproj entry, and for extensions an entry in each target.

Risk: L = low, M = medium, H = high. "Saves" is an approximate number of lines removed.

## Status, 2026-10-20 (uncommitted)

**Done:** A1, A1b, A2 (`SPAnnounce` in SPAccessibility.h), A3, A4, A6, A8, A9 + B4 (NSAlert+Spatterlight), A10a-d, B1a/B1b/B7c (coredata/ReadOnlyStore, in both extensions), B2a-e, B3a, B5 (`+[Blorb blorbAtURL:]`, `+coverImageDataAtURL:`), B6a, B6b, B7a, B7b, C1, C2 + C3 (SpatterlightTests/GlkControllerTestCase), C4, C5, C7 (XCTestCase+TemporaryDirectory), C8 (NSBitmapImageRep+TestImages).

**Done in part:**
- A5: GlkWindow has `-themeStyle:`, `themeBackground` and `-themeAttributesForStyle:`; the grid and buffer windows override the first two. This replaces every grid/buffer `isKindOfClass:` in GlkWindow.m except the Beyond Zork up/down key check. GlkController+GlkRequests.m and the two Preferences sites were left alone: they choose by window *type* (a popup tag, a wintype), not by a window object. The GlkRequests site also falls back to the grid background for graphics windows, while `themeBackground` gives the buffer one.
- A7: only the safe parts. `-syncStyleFlagsWithTheme` (`usingStyles`/`underlineLinks`), `themeLinkStyle` and `-updateLinkTextAttributesOf:` live in GlkWindow, and the buffer's `buildStyles` uses `themeAttributesForStyle:`. The restyle loops stay in the subclasses.
- B3b: the two views have drifted apart: the kerning rules, the 12.5 pt line spacing, the constraint priorities and whether the text is selectable all differ. Only the longest-word title fit is shared, as `-[NSFont fontFittingLongestWordOf:inWidth:]` in NSFont+Categories, which both targets already compile. InfoView's `+addTopConstraintsToView:` now uses anchors, as SideInfoView's does. The paragraph-style and text-field blocks were not merged.
- A9/B4: the informational (OK-only) alerts and the pluralisation snippets were left as they are.
- C6: `+textViewOfWindow:scrollViewIndex:` replaces the 11 plain lookups. Sites that use the scroll view later, the "buffer scroll view" lookups and the `[UITests app]` one-liners were kept.

**Kept, with reasons:**
- A10e: the notification has observers in GameImportTests, so it is now the constant `GlkCommandScriptDidCompleteNotification` and was not deleted.
- A10f: only 5 of the 27 `gwindows.allValues` loops pick out grid windows, so a `gridWindows` accessor would save about 10 lines.
- B7d: saves about 6 lines; the two sides handle errors differently, and Game.m is built into several targets.
- B7e: saves about 8 lines; one side fills a dictionary and the other fills managed objects.

**Behaviour changes:**
- A6: the buffer's drag threshold went from 0.3 to 3 points, to match GlkGraphicsWindow.
- B1b/B2d: the `like[c]` lookups (path, hashTag, ifid, serialString, image originalURL) are now `==[c]`, so `*` and `?` in a path are no longer wildcards.
- A3: the grid now says "End of command history" at the end, and its strings are localized.
- Keypress forwarding logs the same text from every window. Buffer windows accept `initMouse` silently.
- The GlkControllerTestCase tearDown cancels perform requests on every controller, its windows and its quote boxes.
- B6a: the "has the value changed" check goes through `isEqual:`, so nil and NSNull compare differently from before.
- C8: GraphicsFillTests' point colours are read back with the Copy blend mode.

**Verified:**
- The app, tests and UITests build with the scheme. QuickLook and Thumbnailer build separately, because build.sh does not build them.
- The touched unit test classes pass through the isolation rig.
- B3a/B3b: the old and new InfoView were rendered in a harness, 21 layouts (4 dictionaries and 3 Metadata cases at 3 sizes). The subview frames and the pixels are identical.
- The UI tests (C4-C6) were compiled but not run.

## A. App: windows and controller

| # | Item | Where | Fix | Saves | Risk |
|---|------|-------|-----|-------|------|
| A1 | Quote-box speakable text is built by hand in 4 places: `box.quoteboxColumns.count ? box.deinterleavedQuoteboxString : box.textview.string` (the grid copy uses `textstorage.string`). The `"QUOTE: \n\n"` prefix is built twice. | GlkController+Speech.m:228, :231, :391; GlkTextBufferWindow+Speech.m:264-267; GlkTextGridWindow+Speech.m:27 | Add `-[GlkTextGridWindow speakableString]` (and maybe `quotePrefixedString:`) in GlkTextGridWindow+QuoteBox.m. | 15 | L |
| A1b | "Append last quote box to a window list" is repeated. | RotorHandler.m:99, :220; GlkController+Speech.m:195, :493 | `-[GlkController windowsIncludingQuoteBox]` | 10 | L |
| A2 | Accessibility announcement posting has 4 copies. NotificationBezel and InputHistory are line-for-line identical, including the `if (mainWin)` guard. | GlkController+Speech.m:462 (`announce:priority:`); GlkController+InterpreterGlue.m:150 (hand-built dict); NotificationBezel.m:173-182; InputHistory.m:122-136 | Shared helper, e.g. `SPAnnounce(NSString*, NSAccessibilityPriorityLevel)` in a small SPAccessibility.h/.m. Keep the `announcementWindow` fallback for the glkctl variant. | 30 | L |
| A3 | History-travel code is duplicated between buffer and grid. **Likely bug:** the grid's forward case says "Start of command history", while the buffer says "End of command history". The grid strings are also not localized. | GlkTextBufferWindow+Input.m:403-445 vs GlkTextGridWindow+Input.m:406-445 (forward case: buffer :437, grid :441) | `-[GlkWindow announceHistoryEdgeForward:empty:]` or an `InputHistory` method. Fixes the wording bug. | 12 | L |
| A4 | keyDown preamble has 3 copies: decode `ch` with `chartokeycode`, "pass key on to the window that wantsFocus" loop, numpad remap (the grid skips it for Bureaucracy). | GlkTextBufferWindow+Input.m:30-52; GlkTextGridWindow+Input.m:131-153; GlkGraphicsWindow.m:594-610; also InputTextField.m:21-23 for the decode | `-[GlkWindow keycodeForEvent:]` and `-[GlkWindow forwardKeyDownToFocusedWindow:]` | 30 | L |
| A5 | `isKindOfClass:[GlkTextGridWindow class]` dispatch inside base `GlkWindow` chooses `gGridStyleNames` vs `gBufferStyleNames` and `gridBackground` vs `bufferBackground`. The same choice recurs in GlkController+GlkRequests.m:348, Preferences+Settings.m:118 and Preferences+Fonts.m:145. The `blockBgCol` block is duplicated verbatim in `applyZColorsAndThenReverse` and `applyReverseOnly`. | GlkWindow.m:180-185, :202-206, :233, :380-383, :454-457, :533; GlkTextGridWindow+Styles.m:26 (`themeAttributesForStyle:`); GlkTextBufferWindow+Styles.m:84-98 | Overridable `-themeStyleForStyle:` and `-themeBackground` on GlkWindow, overridden in the subclasses. | 40 | L-M (autorestore suite covers it) |
| A6 | Click-or-drag tracking loop has 3 copies. **Inconsistency:** drag threshold is 0.3 in BufferTextView and 3 in GlkGraphicsWindow. The drag filename `game.path.lastPathComponent.stringByDeletingPathExtension` also repeats. | BufferTextView.m:93-121 (name :112); GlkGraphicsWindow.m:522-576 (name :571); Library window/ImageView.m:919 | `-[NSView trackClickOrDragWithEvent:threshold:click:drag:]` in NSView+Categories.m and `-[Game dragBaseName]`. Decide which threshold the buffer should use. | 30 | M (behaviour) |
| A7 | prefsDidChange skeleton and restyle loop (BZ terminator adjust, `usingStyles`/`underlineLinks` change detection, re-apply `blockStyles` plus link/ZColor/Reverse attrs, `linkTextAttributes` update, not-alive resize). | GlkTextBufferWindow+Styles.m:100-280; GlkTextGridWindow+Styles.m:41-160 (link attrs: buffer :235-239, grid :34-39) | Only partial sharing is possible. Do the safe parts (`syncStyleFlags:`, `updateLinkTextAttributesWithUnderline:`) and leave the restyle loop. | 40 | M-H (restore and scroll code is fragile) |
| A8 | Duplicate ivars and stubs. `line_request`, `hyper_request` and `mouse_request` are re-declared in 3 places. `initHyperlink`/`cancelHyperlink`/`initMouse`/`cancelMouse` are trivial stubs in several classes. `sendKeypress:` is near-identical in all 3 windows (buffer and graphics also call `markLastSeen`, the grid does not). Encode/decode of those flags is duplicated. | GlkTextBufferWindowPrivate.h:30-31; GlkTextGridWindowPrivate.h:32-34; GlkGraphicsWindow.m:40; buffer Hyperlinks.m:22-30; grid Input.m:26-32, :55-61, :187; graphics :506-512 | Move flags and base stubs into GlkWindow; add `-queueCharEvent:`. **Archive keys must stay unchanged.** | 30 | L |
| A9 | Suppressible confirm alert. Same shape also in Preferences+Themes.m:419 and Preferences+Settings.m:500 (see B4). | CommandScriptHandler.m:47-71 (`SPConfirmUnlessSuppressed`, static); AppDelegate.m:534-555; GlkController.m:936-965; GlkController+Autorestore.m:1180-1215 | Promote `SPConfirmUnlessSuppressed` to an `NSAlert+Spatterlight` category. Keep sync and async sheet variants separate. Merge with B4. | 40 | L-M |
| A10a | `speakPrevious`/`speakNext` are the same in buffer and grid. They differ only in the string getter and a `[self setLastMove]` call. | GlkTextBufferWindow+Speech.m:293-330; GlkTextGridWindow+Speech.m:62-90 | Move to GlkWindow with an overridable `-speakableStringForMoveRange:`. Share the "At first move.\n" and "At last move.\n" strings. | 30 | L |
| A10b | The grid's `speakStatus` re-implements `cancelPendingMenuAnnouncements` inline. | buffer Speech.m:239-246; grid Speech.m:20-24 | Move `cancelPendingMenuAnnouncements` to GlkWindow. | 5 | none |
| A10c | Link-range enumeration is duplicated. | grid Speech.m:93-104; buffer Speech.m:397-412 | `-[NSAttributedString linkRangesInRange:]` | 12 | none |
| A10d | "Point minus textContainerInset" appears 4 times. | BufferTextView.m:84-85, :128-129; GlkTextBufferWindow+Hyperlinks.m:64-65; GlkTextGridWindow+Input.m:82-83 | `-[NSTextView containerPointFromViewPoint:]` | 10 | none |
| A10e | `GlkCommandScriptDidComplete` is posted with a raw string literal from two places. No observer was found in `application/*.m`. | CommandScriptHandler.m:126; GlkController+InterpreterGlue.m:167 | Define a constant. If there really is no observer (check tests and Library first), delete it. | - | L |
| A10f | 25 `for (GlkWindow *x in gwindows.allValues)` loops. Only `bufferWindows` is cached. | e.g. GlkController+Speech.m:379, GlkController.m:1749 | Add a matching `gridWindows` accessor. | ~15 | L |

Checked and not worth merging: the `createBeyondZorkStyle` implementations (different algorithms), the autorestore key encoding in GlkWindow (already shared), the column/margin math `(inset.width + lineFragmentPadding) * 2`, and FolderAccess.m / NSString+Categories.m (no internal duplication).

## B. Library, preferences, data, extensions

| # | Item | Where | Fix | Saves | Risk |
|---|------|-------|-----|-------|------|
| B1a | QuickLook and Thumbnailer share the persistent container setup (about 25 lines each, identical except the log text). | ThumbnailProvider.m:46-75; PreviewViewController.m:107-134 | Read-only store helper compiled into both extension targets (e.g. `ReadOnlyStore +persistentContainer`). | 25 | L |
| B1b | Game lookup "path like[c], then hashTag like[c] via signatureFromFile". The single-Game fetch shape also repeats in `titleFromIfid`, `titleFromHash` and `titleFromQuetzalSave`. | ThumbnailProvider.m:119-135; PreviewViewController.m:358-373, :1118-1135, :1166-1180, :1280-1290 | `+fetchGameForURL:inContext:` in the same helper. **Semantics:** these use `like[c]`, where `*` and `?` are wildcards. coredata/Fetches.m `fetchGameForHash:` uses `==[c]`. Keep it or switch deliberately. | 25 | L |
| B2a | Core Data fetch boilerplate (request, predicate, execute, log nil, log >1, return first). | coredata/Fetches.m:15-35, :37-58, :134-153 | Private `fetchFirstOfEntity:predicate:` | 40 | L |
| B2b | Callers bypass `Fetches findTheme` and fetch themes by name inline. | Preferences.m:66-71, :651-665; BuiltInThemes.m:786-793 | Route through `Fetches` (check `like[c]` vs `==`). | 20 | L |
| B2c | "Fetch all Games, `includesPropertyValues=NO`" and the `editable == YES` themes fetch are repeated. | Preferences+Themes.m:405-408, ~535; Preferences.m:~710-713, ~736 | Small `Fetches` helpers | 20 | L |
| B2d | Image-by-URL fetch twice. | Library window/ImageView.m:828-845 (`=`); coredata/CoverImageStore.m:96-112 (`like[c]`) | Route ImageView through CoverImageStore. Check the `=` vs `like[c]` difference first. | 18 | L |
| B2e | "Fetch, take first, clear the others" for hardDark and hardLight. | Preferences+Themes.m:75-130 | Local helper | 20 | L |
| B3a | QuickLook InfoView repeats itself. `updateWithMetadata:` and `updateWithDictionary:` each have about 35 lines of topSpacer constraints, about 40 lines of divider, a rating-star block, the longest-word title fit and the IFID line. `formattedStringFromDate:` has identical today/yesterday branches. | InfoView.m:249-492 vs :494-786 (title fit :322-331 vs :587-596); :104-130 | Extract `addTopSpacer…`, `addDivider…` and `addIFIDLine…`. The layout is pixel-sensitive, so compare before and after screenshots. | ~150 | M |
| B3b | InfoView vs the main app's SideInfoView: paragraph-style logic in `addSubViewWithtext:…`, the NSTextField styling block, `+addTopConstraintsToView:`, and the title longest-word fit. | InfoView.m:132-171, :947-966, :915-945, :322-331 vs SideInfoView.m:78-112, :117-134, :453-466, :335-346 | Shared helper category compiled into both targets. Do B3a first. SideInfoView is the cleaner model (anchors). | 80 | M |
| B4 | Alert boilerplate: 22 `NSAlert alloc` sites, at least 8 of them plain confirms. The two Preferences suppression alerts (keys `OverwriteStylesAlertSuppression` and `UseForAllAlertSuppression`) are identical in shape. Merge with A9. Pluralisation snippets (is/are, has/have, it/them) repeat next to `stringWithSummaryOfGames:`. | MissingFilesFinder.m:243; ImageView.m:393; TableViewController+LibraryManagement.m:134, :230; TableViewController+GameActions.m:81; MetadataHandler.m:460; Preferences+LibraryOrganiser.m:117; Preferences+Themes.m:340, :415-440; Preferences+Settings.m:492-522. Pluralisation: Preferences+Themes.m:417, MissingFilesFinder.m:246, TableViewController+GameActions.m:83, AppDelegate.m:522 | `NSAlert` category with `+confirmWithMessage:info:okTitle:` and `-runSuppressibleForWindow:key:completion:`. Model on `ReportImportFailure` in GameImporter.m ~245. | ~100 | L |
| B5 | Blorb cover reader sequence (`isBlorbURL`, `dataWithContentsOfURL`, `Blorb initWithData`, `coverImageData`) is repeated. **Safety inconsistency:** the extensions use `@try`, the main-app copies do not. | ThumbnailProvider.m:91-101; PreviewViewController.m:426-438, :1073-1091; ImageView.m:491, :737-740; GameImporter.m:430-431, :762; GameLauncher.m:210-212; Images/ImageHandler.m:108 | `+[Blorb coverImageDataAtURL:]` and `+[Blorb blorbAtURL:]` in Blorb.m: nil-safe and exception-catching. | ~40 | L |
| B6a | Preferences+Settings setters bypass the existing `changeBooleanAttribute:fromButton:` / `changeMenuAttribute:fromPopUp:` (lines 22-34) and hand-roll "compare, clone theme if not editable, set". | changeVOMenuMenu (210), changeVOImageMenu (217), changeBeepHighMenu (259), changeBeepLowMenu (267), changeZVersion (283), changeBZVerticalAdjustment (295) | Generic `-changeThemeValue:forKey:` | ~25 | L |
| B6b | Five global-defaults bool setters (ShowBezels, ShowLibrary, AddToLibrary, SaveInGameDirectory, RecheckForMissing). | Preferences+Settings.m:428-470 | `setDefaultsBoolFromSender:key:`. Preserve KVO and bindings behaviour. | ~15 | L |
| B7a | The same 0.3 s `dispatch_after` weak/strong-self pattern appears 6 times. | PreviewViewController.m ~616-637, 679-686, 717-727, 730-735, 747-754 | `-afterDelay:perform:` | ~20 | L |
| B7b | Icon-fallback block and the attrDict (system font 20 bold, controlTextColor) are rebuilt in two places. | PreviewViewController.m:451-460, :884-892 | Local helper | ~15 | L |
| B7c | `+initialize` "required category methods" loop is identical. | PreviewViewController.m:56-72; ThumbnailProvider.m:25-40 | One shared function (with B1) | ~15 | L |
| B7d | Bookmark resolve/stale/refresh uses the same options (WithoutUI, SuitableForBookmarkFile). LibraryOrganizer.m:55-130 and FolderAccess.m:37, :343 use SecurityScope and so differ. Small benefit. | Game.m:45-99; Sounds/SoundResource.m:45-92 | `NSURL` bookmark category | ~20 | L |
| B7e | Four bibliographic fields copied into a dict. | PreviewViewController.m:1096-1103 vs coredata/Metadata+IFiction.m `addInfoFromBibliographic` | Minor | ~8 | L |

Already well factored: Sounds (MIDIPlayer, MIDISoundBackend, SoundHandler), iFiction/, BlorbFramework, iFictionQuickLook.
Not examined in depth: HelpPanelController.m, CoverImageHandler.m (beyond its function list), MyCoreDataCoreSpotlightDelegate, GlkStyle.m / Theme.m, IFDBDownloader.m, the VoiceOver menu detection code, and MetadataHandler / GameLauncher beyond the Blorb and alert sites above.

## C. Tests

Only one shared helper exists today: `CoreDataManager initInMemoryWithModelName:` (coredata/CoreDataManager.h:29). There is no shared base class or support file.

| # | Item | Where | Fix | Saves | Risk |
|---|------|-------|-----|-------|------|
| C1 | **Hand-rolled in-memory Core Data stack, 11 copies** (momd lookup, model, coordinator, in-memory store, main-queue context). Three files already use `initInMemoryWithModelName:` (GraphicsFillTests.m:44, GameImportTests.m:139, IFDBDownloaderTests.m:106). The comment "Always an in-memory store: createDefaultThemeInContext with forceRebuild…" is copied verbatim. | AppSupportDirTests.m:33-49; BufferImageRescaleTests.m:70-84; GridRestyleTests.m:80-94; GridWindowTests.m:54-68; QuoteBoxTests.m:86-100; StyleMeasureTests.m:60-74; BuiltInThemesTests.m:36-50; IfidMigrationTests.m:52-65; LibraryOrganizerTests.m:42-55; IFictionParserTests.m:37-52; GameImportTests.m:2483-2495 | Use `initInMemoryWithModelName:` and `.mainManagedObjectContext`. **Not checked:** how that method builds its context, and whether AppSupportDirTests uses the coordinator or model beyond setUp. Verify both first. | ~130 | L |
| C2 | **GlkController-without-interpreter factory, 5 copies** (`makeController`: nullarray of `stylehint_NUMHINTS`, style-hint arrays, empty `gwindows`/`windowsToBeAdded`/`windowsToBeRemoved`/`queue`, null-device `sendfh`, 800x600 NSWindow plus screen assert, GlkHelperView, `controllers addObject:`). The `send:to:a1:a2:a3:text:` request sender has 3 to 4 copies. The tearDown cancel-perform loop has 4. | GridRestyleTests.m:134-167, :170-180, :117-120; GridWindowTests.m:98-130, :132, :80-85; QuoteBoxTests.m:137-170, :173-183, :118-123; BufferImageRescaleTests.m:119-151, ~195-205, :104-107; StyleMeasureTests.m:90-108 (reduced, window-less) | `GlkControllerTestCase` base class (owns `context`, `theme`, `game`, `controllers`) with `-makeControllerWithTheme:`, `-send:…`, and an overridable tearDown hook. QuoteBoxTests and GridWindowTests cancel extra objects. StyleMeasureTests needs a "bare" option or stays out. | 200-250 | L-M |
| C3 | Default theme + Game fixture sequence (`createDefaultThemeInContext:forceRebuild:YES`, insert Game, set ifid and theme) plus the nullify teardown. | GridWindowTests.m:69-78; QuoteBoxTests.m:101-116; BufferImageRescaleTests.m:85-93; GridRestyleTests.m:95-115; StyleMeasureTests.m:75-77; GraphicsFillTests.m:48 | `-makeGameWithIfid:theme:` in the C2 base class | 40-50 | L |
| C4 | UITests wait boilerplate: 29 hand-built `XCTNSPredicateExpectation` + `XCTWaiter` sequences, 24 of them on `value CONTAINS/ENDSWITH/BEGINSWITH`. `waitForElement:toExistWithTimeout:` and `…toBeHittableWithTimeout:` exist as identical class and instance pairs. | UITests.m ~833, 901-905, 957-960, 1116-1117, 1514-1517, 1556-1558; :56-81 | `+waitForElement:valueMatching:timeout:`. Make the instance versions forward to the class versions. | 60-90 | L (UI tests can't be run casually) |
| C5 | UITests per-game transcript-compare tail (read transcript, log error, `isEqualToString:facit`) repeats 13 times; `NSLog(@"Error: %@", error)` appears 24 times. | UITests.m, e.g. testGlulxe, testTADS3, testAlan2, testJacl, testScott. Existing `+transcriptFromFile:` ~138-147. | `+transcriptAtURL:` and `-assertTranscript:inDir:named:equalsFacitFor:`. Share only the tail and the wait wrapper. Window titles and finished-window renames vary per test. | ~150 | M |
| C6 | `scrollView`/`textView` lookup (23 times) and 35 `XCUIApplication *app = [UITests app]` lines. | UITests.m | `+textViewOfWindow:` | small | L |
| C7 | Per-test unique temp dir create and remove (4 copies) plus single-file temp writers. IfidMigrationTests applies `URLByResolvingSymlinksInPath`, and the helper must keep that as an option. | BufferImageRescaleTests.m:95-101, :109; ImageHandlerTests.m:24-31; IfidMigrationTests.m:73-80; LibraryOrganizerTests.m:62-69; SoundHandlerTests.m:292-298; AppSupportDirTests.m:52-61 | `-makeTempDirectoryNamed:` using `addTeardownBlock:` (SoundHandlerTests already does this) | ~40 | L |
| C8 | Pixel readback (CGBitmapContext, draw, read) is duplicated within GraphicsFillTests.m:78-100 and :104-125. Two solid-colour image builders exist: BufferImageRescaleTests.m:155-165 and GraphicsFillTests.m:128-134. The long `NSBitmapImageRep` initialiser appears at ImageHandlerTests.m:97 and :291. | as listed | `-bitmapOfImage:` and a small `TestImages` helper | ~25 | L |

Not worth unifying: the `waitForExpectationsWithTimeout:` blocks in GameImportTests.m, and the run-loop spinning in SoundHandlerTests, IFDBDownloaderTests and QuoteBoxTests.

## Suggested order

1. **Bug / inconsistency fixes found on the way:** A3 (grid history wording), B5 (missing `@try`), A6 (drag thresholds), A10e (dead notification), the `like[c]` vs `==[c]` question in B1b and B2d.
2. **Cheap and low risk:** C1, B2a, A1, A2, A10b-d, B1.
3. **Medium work, clear win:** C2 and C3 together, A4, A8, A9 and B4 together, B3a.
4. **Careful:** A5, A7, B3b, C5.
