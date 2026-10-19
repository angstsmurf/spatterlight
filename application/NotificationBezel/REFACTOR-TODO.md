# NotificationBezel — remaining items

Review of 2026-10-19; the fixes went in the same day (A1–A5, A8–A10, B2,
C1, C2, C4, C5, E). What is left needs a behaviour decision, touches other
directories, or is not worth doing yet.

## Kept on purpose

- **Pre-10.15 / 10.14 `@available` branches (old B1).** Not dead: the app
  targets set `MACOSX_DEPLOYMENT_TARGET[sdk=macosx13*/14*/15*/26*] = 10.13`,
  so these still run on 10.13/10.14. That also keeps the `_image` icon branch
  in `BezelContentView` live (the "?" → `Question` fallback).

## Needs a decision

- **A7 (rest).** The announcement now goes out even with `ShowBezels` off
  (decided 2026-10-19), but it is still dropped silently when there is no
  `NSApp.mainWindow`.
- **Window level / collection behaviour (from A8).** No `level` or
  `collectionBehavior` is set; over a full-screen game window or on another
  Space the bezel may not appear. Needs a check in full-screen.

## Needs a visual check

- `BezelWindow roundedRectMaskWithSize:` and the tint in `BezelContentView`
  now use `+[NSImage imageWithSize:flipped:drawingHandler:]` instead of
  `lockFocus`. Offscreen renders of the content view matched (text bezels
  byte-identical; game-over looks the same; large icons are now fully
  tinted). The rounded vibrant window mask has not been checked on screen.

## Crosses directories

- **C3. Accessibility announcement helper.** `speakString:` is one of seven
  copies of the `NSAccessibilityAnnouncementRequestedNotification` dance
  (`BureaucracyForm.m`, `ZMenu.m`, `JourneyMenuHandler.m`,
  `GlkController+InterpreterGlue.m`, `InputHistory.m`,
  `GlkController+Speech.m`). Extract one helper and use it everywhere.
- **`ShowBezels` literal** is still repeated in `Preferences.m` (766, 2215);
  a shared constant would go with C3-style cleanup.

## Only if more bezel types appear (D)

- Class methods (`+showStandardWithText:onScreen:`) instead of the eight
  throwaway `alloc initWithScreen:` call sites; would also be the natural
  home for A6.
- Subclasses per bezel type instead of the `type` switch in `drawRect`,
  `wantsUpdateLayer`, `allowsVibrancy`.
- `kBezelType` / `kStandard` are generic global names, but match repo style.
