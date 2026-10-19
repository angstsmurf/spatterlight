# BlorbFramework — cleanup / refactor / dedup findings

Review date: 2026-10-19. **Worked through 2026-10-19** — status notes in *italics* under each item.

Verification: app, BlorbFramework, QuickLook and Thumbnailer targets build clean. The old
(HEAD) and new parsers were compiled standalone and run over 324 real Blorbs on disk. The
only output differences were the intended ones: SNam now decodes (46 files), the cover
flag is stable on a second call, and 3 files with no usable picture now report
`fakeFrontispiece = NO` instead of `YES` with a nil cover. Ran via the isolation rig and all
passed: 4 old + 6 new Blorb/IFF tests, plus testArthur, testJourney, testShogun and testZorkZero.

Scope: `BlorbFramework/` (`Blorb.{h,m}`, `BlorbResource.{h,swift}`, `iff.{h,c}`,
`BlorbFramework.h`, `BlorbFramework.docc`), plus the call sites that duplicate or
depend on it (`SpatterlightQuickLook/PreviewViewController.m`, `GameImporter.m`,
`ImageView.m`, `ImageHandler.m`, `ThumbnailProvider.m`).

The framework is small (~700 lines) and already hardened against truncated files
(see recent commits 0b9099b2b, 65e49817e). The items below are ordered by value.
Bugs come first because they are cheaper to fix than to leave, even though the task
was cleanup.

Existing coverage to lean on: `SpatterlightTests/GameImportTests.m` has Blorb
truncation/corruption tests (`testTruncatedBlorbResourceStartAtEOFDoesNotCrash` etc.).
Run those before and after each step.

---

## A. Latent bugs found while reading (fix first, each is a few lines)

- [x] **A1. `Snam` / `SNam` case mismatch — dead UTF-16 branch.** `Blorb.m:73` lists
  `@"SNam"` in `optionalChunksIDs`, but `Blorb.m:124` tests
  `[chunkString isEqualToString:@"Snam"]`. The check never matches, so SNam is always
  decoded as ASCII (garbage for a UTF-16BE payload, or `nil`). Fix the spelling, then
  check what the spec/real files actually use before choosing the encoding.
  *Done. Optional chunks are now a `{FourCC → encoding}` table. Arthur, Journey, Shogun and Zork Zero SNam payloads are UTF-16BE and now decode to the real title (QuickLook shows it as the "Resource associated with the game" line; before, that line was empty).*
- [x] **A2. `Blorb.snam` is declared but never set** (`Blorb.h:22`). It is `readonly`, has
  no setter and no ivar assignment anywhere, and nothing in the app reads it
  (`SNam` goes into `optionalChunks` instead). Delete the property, or populate it
  from the `SNam` chunk and drop the `SNam` entry from the dictionary path.
  *Done: property deleted. SNam stays in `optionalChunks`, because QuickLook reads it from there.*
- [x] **A3. `coverImageData` is not idempotent.** It mutates `frontispiece` and resets
  `_fakeFrontispiece = NO` on every call (`Blorb.m:264-294`). After the first call
  assigns a fake frontispiece, `!frontispiece` is false on the second call, so
  `fakeFrontispiece` reports `NO` and the "boring image" skip (`frontispiece++`) no
  longer applies from the right starting point. `GameImporter.m:563` reads
  `blorb.fakeFrontispiece` after calling `coverImageData`, so anything that calls it
  twice on one object sees different answers. Fix: compute the result once in
  `init` (or lazily and cache both the data and the flag), keep the getter pure.
  *Done. Resolved lazily once; `coverImageData` and `fakeFrontispiece` share the cached result, so the order of calls doesn't matter.*
- [x] **A4. Frontispiece-skip walks by `number`, not by index.** `frontispiece++`
  followed by `for res in images` only works if the next-numbered picture appears
  *later* in `images`. If the boring image is the last or the numbers are sparse
  the loop falls off the end and returns `nil` even when other pictures exist.
  Rewrite as "iterate `images` in order, take the first non-blacklisted".
  *Done. Index-order walk. Also, if Fspc names a missing or unreadable picture, it now falls back to the fake walk instead of returning nil.*
- [x] **A5. SHA is computed before `_fakeFrontispiece` is tested** (`Blorb.m:276-280`).
  Real frontispieces pay for a hash they never use, and `dataForResource:` is
  called twice for the winning image (`:276` and `:291`). Test the flag first and
  reuse `imageData`.
  *Done. Hashes only on the fake path, and reads each picture once.*
- [x] **A6. `IFhd` parsing is gated on "no executable resource or ZCOD"** but runs in the
  chunk walk, which may precede `RIdx`-visible resources only in odd files; the
  comment says "Glulx not handled here (yet)". Decide whether Glulx IFhd (first
  128 bytes of the game file, per Treaty of Babel) belongs here, and either
  implement it or turn the comment into a documented non-goal in `Blorb.h`.
  *Documented as a non-goal on `releaseNumber` in Blorb.h (Babel handles Glulx).*
- [x] **A7. `unpackLong` returns `UINT32_MAX` for any value with the top bit set**
  (`iff.c:19-21`) — a hidden clamp that callers do not know about. It is
  effectively a silent "reject" for lengths. Rename or document it
  (`unpackLongClamped`?), or have a separate non-clamping reader, because
  `IFFID()`-style tags (`unpackLong(data)` in `chunkIDAndLength`) with a first byte
  ≥ 0x80 (non-ASCII tag) are being clamped too. Mac Roman `©` in a chunk ID would
  hit this.
  *Done: clamp removed. Every length goes through `safeSubdataFromOffset:` or `chunkHeaderAt()`, so a huge length is skipped (test: testBlorbHugeChunkLength).*
- [x] **A8. `IFFID` shifts a signed `char`** (`iff.c:15`): `c1 << 24` with `c1 > 127`
  is sign-extending / UB. Cast each to `unsigned char` (or `uint32_t`) first.
  Likewise `unpackShort` has an `int` shift cast in the wrong place
  (`(unsigned int)(a << 8) | b`) — harmless today but misleading.
  *Done. Unsigned casts; `unpackShort` returns `uint16_t`.*
- [x] **A9. Pointer-arithmetic bound checks can be UB.** `ptr + 12 > end`,
  `ptr + 18 <= end` etc. form pointers past the buffer end when `ptr` is already
  near `end`. Switch to offset (`NSUInteger`) arithmetic against `data.length`,
  which `safeSubdataFromOffset:` already does. This also removes the
  `(ptr - base)` conversions sprinkled through `Blorb.m`.
  *Done. Blorb.m and QuickLook's Quetzal code use offsets against `length`.*
- [x] **A10. Noisy `NSLog`s in library code**: "Found Resource Description Chunk!"
  (`:147`), "Providing a fake frontispiece index" (`:271`), and the "Found no
  resource…" error (`:172`) log on every import/thumbnail. Remove, or gate behind
  a debug macro. The `%d` format for `FourCharCode`/`unsigned` in `:172` should be
  `%u` / FourCC string.
  *Done: all removed.*

## B. Dedup / consolidation

- [x] **B1. "Read a chunk header at offset, check bounds, take subdata" appears four
  times** (`dataForResource:`, `zcodeData`, `pictureData`, and the init-time chunk
  walk). Introduce one private helper, e.g.
  `- (nullable NSData *)payloadOfChunkAtOffset:(NSUInteger)start
  expectedID:(FourCharCode)id` returning payload and (optionally via out-param) the
  chunk ID. Then:
  - `zcodeData` = `payloadOfChunk(.. expecting 'ZCOD')` on the Exec resource,
  - `pictureData` = `[self dataForResource:[self findResourceOfUsage:PictureResource]]`,
  - `dataForResource:` = the helper.
  `pictureData` and `zcodeData` become ~3 lines each.
  *Done: `payloadOfChunkAtOffset:expectedID:`, plus C `chunkHeaderAt()` in iff.c.*
- [x] **B2. `resources` / `findResourceOfUsage:` / `resourcesForUsage:`** — make
  `findResourceOfUsage:` = `resourcesForUsage:.firstObject`, or build a
  `usage → [resources]` dictionary once in `init` (resources are immutable after
  init, and `coverImageData`/`GameImporter` call these repeatedly). Also the lookup
  loop in the `RDes` branch (`:164-170`) wants a `(usage,number)` dictionary.
  `-findResourceOfUsage:` is also re-declared in the class extension
  (`Blorb.m:32`) although it is already in `Blorb.h:39` — drop the duplicate.
  *Partly done. The duplicate declaration is gone, and RDes uses a `(usage.number) → resource` dictionary. I didn't add a usage cache: resource counts are tiny and `findResourceOfUsage:` stops at the first hit.*
- [x] **B3. IFhd (game identifier) parsing is duplicated** in
  `Blorb.m:130-144` and `PreviewViewController.m` (`titleFromQuetzalSave:` ~1265,
  `hashFromQuetzalSave:` ~1344), and in `GameImporter.m:589-605`
  (`shortAtPointer(ptr + 2)` etc.). Add to the framework one small value type or
  C struct, e.g. `IFhdInfo { release; serial[6]; checksum; }` plus
  `BOOL parseIFhd(const void *chunkPayload, size_t len, IFhdInfo *out)` in
  `iff.h` (Quetzal and Blorb IFhd have the same layout), and have all three users call it.
  `ifidFromIFhd` (`Blorb.m:296`) could then take the struct and be reused by the
  Quetzal path (the QuickLook code builds the same `ZCODE-…` IFID elsewhere).
  *Done for Blorb and Quetzal: `IFhdInfo` and `parseIFhd()` in iff.h, and `+[Blorb zcodeIFIDWithRelease:serial:checksum:]` replaces QuickLook's copy of the IFID rule. Not done for GameImporter's raw Z-header read (offsets 2/18/28): that is the story-file header, not an IFhd chunk. It got a `length < 30` guard instead.*
- [x] **B4. QuickLook's Quetzal parsing reads without bounds checks.**
  `PreviewViewController.m:1265-1281` does `isForm(ptr…)` (needs 12 bytes) then reads
  `ptr + 8…ptr + 17`, then walks to the next chunk and reads `ptr + 20` with
  `length - 12` (unsigned underflow if `length < 12`), all without checking
  `data.length`. This is the same class of bug the Blorb hardening commits fixed.
  Moving the parsing into the framework (B3) with a length-taking API fixes both.
  (`hashFromQuetzalSave:` does check `>= 84` but uses a `malloc`/`getBytes`/`free`
  for what `[data subdataWithRange:]` plus a hex helper would do.)
  *Done. Every read is bounded, and the IntD path checks `length >= 12`. Not covered by a unit test, because PreviewViewController isn't in the test target. The primitives it uses are covered (testIFFHelpers).*
- [x] **B5. Hex-string formatting loop** in `hashFromQuetzalSave:` duplicates what
  `FileSignature` already does for SHA digests; share a single `hexStringFromBytes:`
  helper (put it in `FileSignature` or a `NSData` category the framework owns).
  *Done: `+[FileSignature hexStringFromData:]`. `sha256StringFromData:` uses it, and QuickLook lower-cases the result to keep its old output.*
- [x] **B6. `isBlorbURL:` extension list** (`:42-46`) — turn into a static
  `NSSet`/array and `containsObject:`; check whether the same list exists
  elsewhere in the app (e.g. library import, `UTType` handling, Info.plist
  document types) and share a single source.
  *Done: static NSSet. Not merged with the app's other lists: `GameImporter.m:312` deliberately routes only blorb/blb/zblorb through the Blorb import path, and it spells the last one "zbl", so `.zlb` never takes that path. Possibly a typo for "zlb"; left for you to decide. AppDelegate's list is the full game-type list.*
- [ ] **B7. The `ThumbnailProvider`, `PreviewViewController`, `ImageView`,
  `GameImporter` all do "if isBlorbURL → `initWithData:` → `coverImageData`".**
  Add a convenience `+[Blorb coverImageDataForURL:]` (or
  `-initWithURL:` + `-coverImageData`), so those four call sites lose their
  duplicated nil-checks. Keep the data-based API for already-loaded data.
  *Declined. Only ThumbnailProvider would get shorter. The others keep the Blorb object for metadata, IFhd or resources too.*

## C. Dead / unused code

- [x] **C1. The whole IFF *writer* in `iff.c` is unused by the app:**
  `IFFCreateBuffer`, `IFFSetBuffer`, `IFFCloseBuffer`, `IFFBeginForm`, `IFFEndForm`,
  `IFFNextForm`, `IFFBeginChunk`, `IFFEndChunk`, `IFFGetChunk`, `IFFNextChunk`,
  `IFFWriteByte`, `IFFWrite`, `packLong`, `IFFHandle` — no references outside
  `iff.c`/`iff.h` (grep over `application`, QuickLook, Thumbnailer, iFictionQuickLook,
  tests, glkimp). They are Yazmin leftovers. Delete them (the writer also has bugs:
  `expandBufferIfNeeded` only doubles once so a large `IFFWrite` can still
  overflow, ignores `malloc` failure silently and then writes anyway, and
  `IFFEndChunk` pads before recording the length in a way that is easy to misread).
  Only used pieces: `IFFID`, `unpackLong`, `unpackShort`, `isForm`, `isID`,
  `chunkIDAndLength`, `paddedLength`.
  *Done: writer, `IFFHandle` and `packLong` deleted.*
- [x] **C2. Commented-out `zChecksumFromData:`** (`Blorb.m:313-337`, references a
  non-existent `word()` helper). Delete, or move into a proper function if the
  Glulx/Z checksum is wanted (see A6).
  *Done: deleted.*
- [x] **C3. `@synthesize metaData;`** at `Blorb.m:262` sits between methods, and
  `metaData` is *also* declared as an ivar in the class extension (`:28`) and as a
  readonly property in the header. Pick one: drop the ivar and `@synthesize`
  and use `_metaData`. Same for the `data` ivar (name collides conceptually with
  every method parameter named `data` — rename to `_data`) and `frontispiece`
  (`NSInteger` ivar in the extension next to `_fakeFrontispiece`; fold into one
  private-state block).
  *Done. Ivars are `_data`, `_frontispiece`/`_hasFrontispiece` and `_coverImageResolved`. `metaData` uses its synthesized `_metaData`.*
- [x] **C4. Deprecated `NS_NONATOMIC_IOSONLY` on two properties** (`Blorb.h:41,43`)
  — meaningless on macOS; use plain `@property (readonly, nullable) NSData *coverImageData`.
  (They are also computed properties implemented as methods; fine, but `copy` on a
  readonly computed property is noise.)
  *Done.*
- [x] **C5. `releaseNumber`, `checkSum`, `serialNumber` are public *read-write***
  (`Blorb.h:24-26`) and `optionalChunks` is a public `NSMutableDictionary`,
  `fakeFrontispiece` is read-write. Nothing outside assigns them. Make them
  `readonly` (and `optionalChunks` an immutable `NSDictionary<NSString *, NSString *> *`
  copy) to match the framework's actual contract.
  *Done. Read-only, and `optionalChunks` is an immutable `NSDictionary<NSString *, NSString *>`. The IFhd fields are documented as Z-code only.*
- [ ] **C6. `BlorbFramework.docc` is still the Xcode template** ("Summary / Text /
  Group / Symbol" placeholder tokens). Either write it or remove the catalogue
  and its pbxproj reference.
  *Declined: needs a pbxproj edit, and the project file has an unrelated pending change.*
- [x] **C7. `BlorbFramework.h` mixes `#include <BlorbFramework/iff.h>` with
  `#import`**; use `#import` consistently. `Blorb.h` imports all of `Cocoa`
  but only needs `Foundation` (it uses `NSData`, `NSArray`, `FourCharCode`) — the
  framework is also linked into the QuickLook/Thumbnail extensions where AppKit
  is unnecessary weight. Check whether anything depends on the transitive import
  before switching.
  *Done. `#import` throughout; Blorb.h and BlorbResource.swift use Foundation. All four targets still build.*

## D. Naming / layout tidy-ups

- [ ] **D1. `BlorbResource.h` contains only an anonymous `NS_ENUM`**
  (`ExecutableResource`, `PictureResource`, `SoundResource`) while the
  `BlorbResource` *class* lives in `BlorbResource.swift` (which has the
  stale file header `Resource.swift`, and is declared as
  `Blorb.Resource` with `@objc(BlorbResource)`). Rename the header to e.g.
  `BlorbUsage.h`, give the enum a name and prefix
  (`BlorbUsage…`), and update `BlorbFramework.h` and the pbxproj entry.
  Note the enum constant `SoundResource` collides by name with
  `@interface SoundResource` in `application/Sounds/SoundHandler.{h,m}`;
  it compiles today only because enum constants and class names do not
  share a namespace in ObjC — a prefix removes the trap.
  *Declined: header rename needs pbxproj.*
- [ ] **D2. Public Swift class is a plain data holder with a lone
  initializer** (`usage`, `number`, `start`, `descriptiontext`, `chunkType`).
  `descriptiontext` is lower-cased inconsistently (`descriptionText`; it is also
  used by `ImageHandler.m:377`, so rename needs a coordinated edit, or keep the
  ObjC name via `@objc(descriptiontext)`). `chunkType` is a `String?` holding a
  FourCC; consider storing the `FourCharCode` and only formatting on demand
  (it is compared as a string in `Blorb.m:137`). Also: the one Swift file forces the
  `BlorbFramework-Swift.h` import and the Swift runtime into a framework otherwise
  written in C/ObjC — worth considering porting `BlorbResource` to ObjC so the
  framework is pure ObjC, which also avoids the circular-looking
  `#import <BlorbFramework/BlorbFramework-Swift.h>` inside `Blorb.m`.
  *Declined: an ObjC port would mean new files in pbxproj. The stale file header is fixed (BlorbResource.swift).*
- [x] **D3. Header attributions** — files are mostly Yazmin-derived; `Blorb.h` still
  says "Yazmin" while `Blorb.m` says "Spatterlight / based on Yazmin". Make the
  headers consistent (licence note matters since this is third-party code).
  *Done for Blorb.h, iff.h and iff.c ("Spatterlight / based on Yazmin", same as Blorb.m).*
- [x] **D4. Four-character-code helpers.** `IFFID('R','I','d','x')` repeated
  character-wise is hard to read and was the source of A1. Add a
  `#define FOURCC("RIdx")`-style macro (or `static const FourCharCode kRIdx`
  constants in one place) and use the constants for `RIdx`, `IFmd`, `Fspc`,
  `IFhd`, `RDes`, `ZCOD`, and the optional-chunk set. Make the optional-chunk
  table `{ID, encoding}` pairs, which also resolves A1 structurally.
  *Done. A `FourCharCode` enum (`kRIdx`… `kZCOD`) plus the optional-chunk table. Note: boxing needs `@((FourCharCode)'AUTH')`, because `@('AUTH')` boxes a truncated `char`.*
- [x] **D5. `initWithData:` is ~120 lines with three nested loops.** Split into
  private methods: `parseResourceIndexAt:`, `parseChunkAt:` (one method per chunk
  type: metadata, frontispiece, optional text chunk, IFhd, RDes). Reuse `count`
  variable names carefully (`count` declared in the RIdx scope is *reused* in the
  RDes branch at `:151`, easy to break).
  *Done: `parseResourceIndexAt:intoArray:`, `parseChunksFromOffset:`, `parseIFhd:` and `parseResourceDescriptionsAt:`.*
- [x] **D6. `initWithData:` returns `nil` for `length < 13` but
  `isBlorbData:` accepts `>= 12`;** the two disagree by one byte. Pick a single
  minimum-size constant. `initWithData:` is also declared `NS_DESIGNATED_INITIALIZER`
  with a `nullable` argument but never validates the `FORM`/`IFRS` header itself;
  decide whether `init` should reject non-Blorb data (callers currently rely on
  checking the extension first).
  *Done. A single `kFormHeaderLength = 12`. `init` still doesn't check for IFRS (documented); callers check the extension or call `isBlorbData:`.*
- [x] **D7. Mixed C `int` returns** for predicate helpers (`isID`, `isForm`) while
  the header already `#include`s `<stdbool.h>`; return `bool`. Also in `iff.h`,
  document that `isForm` needs ≥ 12 readable bytes (see B4), or add a length
  parameter.
  *Done. Returns `bool`, and `isForm` takes a length.*

## E. Tests to add alongside the refactor

- [x] Unit tests for the new chunk helper (B1) and IFhd parser (B3): truncated,
  zero-length, length = `UINT32_MAX`, odd-length (padding) chunks.
  *testIFFHelpers, testBlorbHugeChunkLength, testBlorbOptionalTextChunks (odd length), testBlorbIFhd (short IFhd).*
- [x] Regression test for A3: call `coverImageData` twice on a blorb with no `Fspc`
  and assert identical data and a stable `fakeFrontispiece`.
  *testBlorbCoverImageWithoutFspcIsStable, testBlorbCoverImageUsesFspc.*
- [x] Regression test for A1/A2: blorb with an `SNam` chunk round-trips its title.
- [ ] Quetzal-truncation tests for the QuickLook path once it is moved onto the
  framework parser (B4).
  *Not done: PreviewViewController isn't in the test target.*

## Suggested order

1. A1–A5, A10 (small, behaviour-visible, covered by tests added in E).
2. C1–C4 (pure deletions / visibility; compile-checked).
3. B1–B2, D4–D5 (restructure `Blorb.m` behind existing tests).
4. B3–B5 + B4 (framework-owned IFhd parser, then switch QuickLook / GameImporter).
5. D1–D3, B6–B7, C5–C7 (renames and API tightening; touch pbxproj and call sites last).

Remember `xcode-new-header-needs-pbxproj`: any new `.h` in this framework must be added
to the pbxproj Headers phase (Public) by hand.
