# VoiceOver menu detection — remaining refactor items

Scope: `application/VoiceOver menu detection/` (ZMenu, InfocomV6MenuHandler, JourneyMenuHandler with
JourneyMenuItem and JourneyTextFormatter, BureaucracyForm).

The first pass (2026-10-19) fixed the crash, bounds and decoding bugs and did the cheap dedup. It also moved
internal state into class extensions and split JourneyMenuItem and JourneyTextFormatter into their own files.
Tests are in `SpatterlightTests/VoiceOverMenuTests.m`: Latin1BytesTests, InfocomV6MenuHandlerTests,
JourneyTextFormatterTests, JourneyCodingTests and BureaucracyFormTests. Run them one class at a time in the
isolation rig, never the whole bundle.

Notes that still hold:
- `NSMenuItem` does not adopt NSSecureCoding, so `JourneyMenuHandler` is plain `<NSCoding>`. Autorestore
  decodes with `requiresSecureCoding = NO`. `JourneyMenuItem` is `<NSSecureCoding>`.
- Interpreter menu text is Latin-1. Decode it with `+[NSString stringWithLatin1Bytes:length:]`.
- Announcements go through `-[GlkController announce:priority:]`, which posts to `announcementWindow`
  (the main window while Journey is the current game).

---

## Behaviour questions

1. **Out-of-bounds Journey clicks.** `sendCorrespondingMouseEventForMenuItem:` logs "Mouse click out of
   bounds" but still sends the event, as it always has. Find out whether any real menu position reaches this
   (`kMenuColumns`/`kMenuRows` geometry against a small grid window) before deciding to drop it.
2. **`InfocomV6MenuHandler` sets `_delegate.zmenu.haveSpokenMenu`.** The flag is silently lost when `zmenu`
   is nil, and `menuLineStringWithTitle:…` (a string builder) sets it as a side effect. Resolve this as part
   of item 4.
3. **`-detectForm` still calls `addMove`.** It now returns NO when no fields are found, but it still appends
   (bumps) the form's move range on every call. Move that out to the caller, or into an explicit `-refresh`.

## Structure

4. **`@protocol VoiceOverMenu`.** ZMenu and InfocomV6MenuHandler depend on each other both ways: the
   handler sets `zmenu.haveSpokenMenu`, and ZMenu branches on `showingInfocomV6Menu` and forwards to the
   handler. Make both implement one protocol (`selectedLine`, `menuLineString…`, `instructionString`,
   `haveSpoken`), and give GlkController one `id<VoiceOverMenu> currentMenu`. This also merges the two
   copies of the "Menu item N of M" / instructions assembly.
5. **ZMenu detector split.** ZMenu (≈860 lines) still mixes cluster detection, selected-line detection,
   command extraction and speech. Split out a `ZMenuDetector` of pure functions over the attributed string.
   Move the per-game hacks (Beyond Zork, Anchorhead, Thaumistry, Vespers, The Unforgotten, Junior
   Arithmancer, TADS 3, Hugo) out of `-detectMenu` into one table (`pattern`, `initialChar`, `maxLines`,
   `extraCheck`). Write the fixtures in the Tests section first.
6. **JourneyMenuHandler split, part 2.** Still to do:
   - `JourneyDialogPresenter`: the `display…` methods, accessory views and timing.
   - A mouse-event helper for the click geometry.
   - `JourneyMenuHandler+Coding`.
   - One method per menu type instead of the long `handleMenuItemOfType:` switch.

   New app sources need pbxproj entries; test files don't.
7. **`recreateDialog`** still does the guard checks, builds the last-move prefix and schedules presentation
   in one method. Split it into `-shouldRecreateDialog`, `-dialogTextByPrependingLastMove` and
   `-presentStoredDialog`.
8. **Journey dialog state machine.** `shouldShowDialog`, `reallyShowingDialog`, `restoredShowingDialog`,
   `skipNextDialog`, the three `shouldStartNew…` flags and the two `…DialogAddedMove` flags encode an
   implicit state machine. Replace them with an enum (`Idle`, `Pending`, `Showing`, `Restored`). Check
   autorestore compatibility, since some of these flags are archived.
9. **Shared lifecycle.** The four helpers are all created by GlkController, nilled at game end, and
   cancelled with per-class `cancelPreviousPerformRequestsWithTarget:` calls in `GlkController+Speech.m`.
   A small base class or protocol (weak controller reference, `-cancelPendingSpeech`) would gather those.

## Naming

10. ZMenu and BureaucracyForm call the controller `glkctl`; the two handlers call it `delegate`, though it is
   not a delegate. Every JourneyMenuHandler member has a redundant `journey` prefix, and `kJourneyDialogType`
   should be `JourneyDialogType`. Renaming the Journey members touches Autorestore and GlkController, and
   possibly archive keys. Keep the archive keys unchanged.

## Tests

11. **ZMenu has no tests yet.** Add detection fixtures on synthetic text storages, one per game rule (Beyond
    Zork, Anchorhead, Thaumistry, Vespers, Unforgotten, TADS 3 with two items, Hugo), plus all-blank and
    empty-line cases. `-detectMenu` reads `glkctl.gwindows`, so it needs either a stub controller or the
    detector split (item 5) first.
12. Journey: a test of an old save without `tagCounter`. It should fall back to the highest verb tag + 1.
