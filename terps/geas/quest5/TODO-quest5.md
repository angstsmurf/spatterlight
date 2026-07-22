# TODO: Quest 5 support in Geas

## Status (2026-07-22, a pane fold refreshes the autosave -- BOTH frontends)

- Unfolding an object's verb list in the side pane passes no turn, so no
  autosave followed it and the newest snapshot predated the fold: the state
  IS carried (`g_pane_expanded` in the Quest 5 blob, `objwin_expanded` in
  Quest 4's `geas_stash_frontend_state`), it was just never written while it
  was true.  A relaunch therefore always came back folded.
- Both toggle sites now refresh the snapshot: `aslxglk.cc`'s `gobjwin`
  hyperlink branch and `geasglk.cc`'s `objwin` one.  The live line request has
  to be **cancelled across the write** -- an archived PENDING request would
  collide with the one a restore makes -- and re-armed with anything already
  typed preloaded (`ce.val1`), the idiom the timer-tick and prefill paths in
  those same files already use.
- Verified with `test/glkdrive.py` on both engines: Quest 4 (Bear Campsite)
  unfolds Crate -> quit -> relaunch shows Crate's verbs still unfolded, and
  a typed command still runs in the same session (proving the re-armed
  request is live).  Quest 5 (Behind the Door) the same with Wall.
- Cosmetic only -- the parser prompt was always usable either way; this is
  not the input-breaking class of the verb-menu bug below.
- `make check` green, sweep 76/76, Quest 4 walkthroughs 22/22.

## Status (2026-07-22, the verb menu autorestores INTO itself)

- **Fixed: the object verb menu broke across a close/reopen** (reported: the
  "Door / 1: Look at / 2: Knock" choice stops working if you close the window
  while it is up).  The menu runs NESTED inside the parser `read_line`, where
  `g_autosave_interp` is still armed, so the snapshot was taken with the menu
  on screen — but nothing recorded that a menu WAS the prompt, so the restore
  brought back a window showing the numbered options with the engine sitting
  at the parser prompt, where the numbers mean nothing.
- **The menu is now part of the snapshot** rather than suppressed.  It is
  legitimately restorable: the click starts no script, so the menu sits
  BETWEEN turns and the world under it is clean.  `g_pending_menu_alias` +
  `g_pending_menu_verbs` record the alias and the exact options ON SCREEN
  (not a re-resolved list, which could disagree with what the player can
  see); the blob carries them; autorestore sets `g_autorestore_menu_resume`
  and the next `read_line` re-enters the menu instead of prompting.
- `run_menu_ui` gained a `resumed` flag: it reprints neither caption, options
  nor prompt, and passes `on_screen` to the first `read_line` -- the same
  trick `g_autorestore_reentry` plays for the parser prompt, which only has
  one line to worry about.  The resume returns through the ordinary
  `InEnd::Command` path, so the chosen command is dispatched by exactly the
  code a click or typed line goes through.
- `AutosaveSuspend` remains, but ONLY for `menu_provider` / `ask_provider` --
  a **pre-existing** hazard of a different kind.  Those are the EXPRESSION
  forms of ShowMenu/ask: the engine blocks inside `eval`, so a half-run
  script is on the stack with nothing recording it.  That is what makes them
  unrestorable and the verb menu restorable.
- **Also fixed, latent since the autosave landed:** `write_autosave_pair`
  created the autosave directory only `if (autosavedir == NULL)`, but
  `getautosavedir` resolves the NAME without creating it and the boot-time
  autorestore probe already calls it -- so the guard skipped the one call
  that makes the directory and every write failed with mktemp errno 2.  The
  real app pre-creates it, which is why this never showed; driving the terp
  directly (or a user whose autosave folder was removed) hits it at once.
- **New `test/glkdrive.py`** (extended from the Scarier copy) -- the first
  harness that reaches any of this.  Fake app over the glkimp protocol, now
  with `link:N` script items that send `EVTHYPER`, so hyperlinks ARE
  clickable headlessly; ending the script mid-prompt sends the
  autosave-preserving `EVTQUIT`.  The smoke harness and `aslx_replay` are
  both blind here: CheapGlk reports no hyperlink support, and the autosave
  code is `#ifdef SPATTERLIGHT`.
- Verified with it, all five paths: quit under the menu -> relaunch resumes
  INTO it with nothing reprinted and `2` runs Knock (a parser prompt would
  have said "I don't understand"); cancel on a resumed menu falls back to a
  fresh prompt; a DOUBLE close/reopen still resumes (the unchanged snapshot
  stays valid -- `geas_autosave_wanted` correctly writes nothing when only
  EVTARRANGE has happened); answering clears the pending menu so the next
  launch is an ordinary prompt; and a no-menu autorestore is unchanged.
- Sweep 76/76, `make check` green, `xcodebuild -target geas` clean.

## Status (2026-07-22, bundled-JS callbacks fire without a JS engine)

- **Fixed: Moquette stalled on the first train in Act 0** (reported: "get on"
  is the only link and clicking it does nothing).  `{command:1:get on}` was
  fine; the command's script is `JS.act0Clear ()`, and moquette.js defines
  that as a jQuery animation whose `setTimeout` ends in
  `ASLEvent("FinishAct0Clear","")` — and THAT is what moves the player on.
  With no JS engine the completion never fired and Act 0 never ended.
- The existing `js_event_bridge` only covered the spondre shape, where the
  callback name rides in the JS call's LAST ARGUMENT
  (`JS.FadeInTitle(..., "TitleDone")`).  `JS.act0Clear()` takes no argument
  at all — the name is buried in the bundled script.
- New `index_bundled_js()` reads the `<javascript src>` files out of the
  .quest package (via the existing `resource_bytes`) once per session and
  records, per JS function, the `ASLEvent` completions its body would fire.
  **Transitively**: the function the engine calls often just delegates
  (`heatherText` → `doHeatherText`, `heatherTube` → `animateHeatherTube`,
  `act4Clear` → `animateAct4Clear`), so a call-graph DFS over bundled
  functions collects them.  A brace scan skipping strings and comments bounds
  each body; a computed (non-literal) callback name is simply not indexed.
- Verified the index resolves all seven functions the engine actually calls
  to exactly the nine `event:` injections `golden/Moquette.cmd` needs:
  `act0Clear→FinishAct0Clear`, `endAct0→StartAct1`,
  `doScreenClear→JSFinish_Clear`, `heatherText→JSFinish_HeatherText`,
  `blackout→JSFinish_Blackout`, `heatherTube→JSFinish_HeatherTube`,
  `act4Clear→JSFinish_Act4Clear`.  `ASLXGLK_JS_TRACE=1` dumps the index and
  every fired completion.
- Spondre is untouched: its callbacks all arrive with a non-empty argument
  and return before the new path, and it indexes no completions at all
  (empty `g_js_callbacks`), so nothing double-fires — re-checked, its
  `TitleText1Done → TitleText2Done → TitleDone → FinishLeadin` chain is
  unchanged.
- Frontend-only, so the goldens cannot move: `aslx_replay` has its own driver
  and fires these through its `event:` directive.  Sweep 76/76 byte-identical,
  `make check` green (now including the bundled-JS scanner tests).

## Status (2026-07-21, inline object links pop their verb menu)

- **Fixed: clicking an object name in the text answered "I can't see that."**
  (reported against *Behind the Door*, where clicking the door was the only
  way forward).  Core's `{object:x}` emits
  `class="cmdlink elementmenu" data-elementid="x"` — the ELEMENT name — and
  the frontend turned that straight into `look at <element name>`.  The
  parser resolves *aliases*, not element names, so every object whose name
  differs from its alias was unclickable: Behind the Door's `doorobj` is
  aliased "door".
- `LinkAction` now carries the element name and resolves it at CLICK time,
  where the live world is available: new `Interp::verb_menu_for()` returns
  the object's display alias plus the verb menu the reference player pops
  (`GetDisplayAlias` + `GetDisplayVerbs`, inventoryverbs when carried).
  Unlike `verb_menu_objects()` it resolves BY NAME, so it also covers
  scenery and anything else the side pane never lists — `doorobj` is
  scenery, so a scope-based lookup would still have missed it.
- Sending only "look at" was the second half of the bug: with the pane
  hidden (`game.showpanes` off, or a `request (Hide, "Panes")` — Behind the
  Door does the latter for its whole opening) inline links are the ONLY
  handle on an object, so its other verbs were unreachable.  One verb runs
  straight; two or more open the numbered menu (`run_menu_ui`, nested inside
  `read_line` the same way a timer-opened menu already nests), cancellable.
  `doorobj` → `[Look at][Knock]`, `ball` → `[Look at][Take]` on "marble
  ball", so the game is now playable by clicking, as the reference is.
- Autosave blob bumped to `ASLXGLK-AUTOSAVE 3` (the new link field).  The
  live link at the click site is now copied by value: a verb menu prints,
  which renders new links into `g_links` and could reallocate it under the
  old reference.
- `LinkAction::live()` — the renderer's clickability gate — had to learn
  about the new field too.  Missing it once cost a second regression: the
  object link parsed correctly but `live()` said no, so the anchor rendered
  as plain text and "door" stopped being clickable at all.  **Anything a
  click can DO belongs in `live()`.**
- **New `test/aslxglk_link_tests.cc`, in `make check`.**  This whole area was
  untested and it shows: `aslx_replay` strips HTML before it sees a tag, and
  `aslxglk_smoke` runs on CheapGlk, whose gestalt reports NO hyperlink
  support — so `g_hyperlinks` is false there and no link is ever registered
  or clicked.  The test unity-includes `aslxglk.cc` (`link_action` is
  file-local) and calls it on real Core output for each link flavour,
  asserting both the recovered action and `live()`.  Verified to fail on
  both of today's regressions.
- Native `aslx_replay` sweep 76/76 byte-identical vs `quest5-oracle/golden/`,
  `make check` green, `xcodebuild -target geas` clean.

## Status (2026-07-21, grid map: the canvas is the game's background)

- **Fixed: exit lines missing from the map** (reported against *I Contain
  Multitudes*, which draws them in QuestViva).  The map pane hard-coded a
  white canvas, but the reference player's `setBackground()` paints
  `#gridPanel` with the *game's* background colour
  (`playercore.js`, alongside `#gamePanel`), and the grid colours are
  authored against that.  I Contain Multitudes runs on
  `defaultbackground=Black` with `mapexitcolour=White`, so its exit lines
  were white-on-white.  Rooms still showed (Core's `grid_fill` default is
  White with a Black border), which is exactly the reported symptom.
- New `GridDraw::Op::Canvas` carries the colour; `g_gm_canvas` (default
  White, the Core default, so the ~everything-else case is unchanged) is
  the surface fill AND the Glk window background.  Fed from both channels
  `SetBackgroundColour` uses: `JS.setBackground` on modern Cores, and
  `request (Background, colour)` for games embedding a Quest 5.0-era Core
  (Dream Pieces 2 — the same pre-JS pairing already handled for
  `SetStatus`/`UpdateLocation`/`SetPanelContents`).
- Zero transcript risk: both new branches sit behind `grid_draw`, which is
  null headless, so the argument is not even evaluated there.  Native
  `aslx_replay` sweep 75/75 byte-identical vs `quest5-oracle/golden/`,
  `make check` green.  Live-verified with screenshots in the app: I Contain
  Multitudes now draws white exit lines on black, Dream Pieces 2 its yellow
  room on black.
- New `ASLX_GRID_TRACE=1` on `test/aslx_replay` dumps the paint commands, so
  map behaviour can be checked without launching the app.

## Status (2026-07-19, Spatterlight autosave/autorestore — BOTH engines)

- **Spatterlight autosave/autorestore lands for the whole geas terp** —
  the classic Quest 4 frontend (GeasRunner) and the native Quest 5 engine
  both, modeled on the Bocfel implementation.  New
  `geasglk-autosave.{h,mm}` (Spatterlight app target only; every call site
  is `#ifdef SPATTERLIGHT`): at each top-level parser prompt the engine's
  own full-state serialization (`save_state(false)` / `Interp::save_game`)
  goes to `Quest Files/Autosaves/<hash>/autosave.glksave`, the Glk library
  state (TempLibrary + an engine-specific archive hook) to
  `autosave.plist`, temp-write + rename for atomicity, then
  `win_autosave(tag)` has the app snapshot the GUI.  Autosave fires AFTER
  the prompt is printed and BEFORE line input is requested, so the saved
  windows carry no pending request and a restore re-enters by just
  re-requesting input (the Bocfel re-execute-the-read pattern); it also
  refreshes after a state-changing timer tick (guarded by the
  autosave-on-timer pref) and skips engine prompts (`show menu`/`ask`/
  `wait`/`get input`) whose callbacks the snapshot cannot capture.
- **Restore**: Quest 4 boots the game silently (`g_autorestore_booting`
  swallows output, auto-answers `get_string`/`make_choice`/`wait_keypress`)
  then `load_state(data, false)` + TempLibrary `updateFromLibrary` +
  re-pointing the frontend window globals via serialization tags.  Quest 5
  rides the existing saved-game boot in `run_session`: `restore_game`
  overlays the freshly loaded world, then the library swap, then a
  frontend blob (window tags + the transcript's live `g_links` hyperlink
  actions + pane fold/status/location strings + frame-picture name)
  decoded by `aslx_recover_frontend`; InitInterface re-runs and repaints
  map/panes/panel from engine state (StartGame/begin_timers skipped, no
  "Loaded saved game" line).  A bad autosave is deleted, `win_reset()`,
  fresh process.
- **App side**: geas added to the restorable-terp list
  (`GameLauncher.m`).  **Footgun fixed**: `geasglkterm.c` only set
  `garglk_set_program_name` under `#ifdef GARGLK`, which Spatterlight
  doesn't define — glkimp's workdir map never saw "geas" and
  `create_autosavedir` silently failed on a "(null) Files" path (the app
  looks in "Quest Files").  Now set under SPATTERLIGHT too.
- **Verified live** (app driven via AppleScript/AX): Pyramid Of Terror
  (Quest 4) — quit + relaunch restores transcript AND state (inventory
  keeps the taken lichen), single prompt, no re-intro; The Deer Trail
  (Quest 5) — both the app-quit and the close-window/reopen paths restore
  transcript, state ([NOCK]/[REST] progression), status banner and pane.
  Harnesses at baseline: `make check` green, Quest 4 corpus walkthroughs
  22/22, native replay vs frozen goldens 56/57 (the one miss is the
  documented ICM oracle artifact).
- **A looping sound resumes after autorestore — via the APP, not the terp**
  (first shipped as a terp-side replay; petter then found the loop kept
  playing after the game window closed, which exposed the real design).
  The app already archives its sound channels (playing sound, loop count,
  pause/volume/fade state) in its GUI snapshot and replays them itself
  (`SoundHandler restartAll`, GlkController+Autorestore.m) — and during its
  restore it REPLACES its sound handler wholesale, so anything the terp
  starts before that point becomes an orphaned player no
  `stopAllAndCleanUp` can reach: an immortal loop.  So the terp must NOT
  replay sounds on restore.  Its only job is keeping the staged sound
  FILES alive: the app re-reads the sound from a file bookmark on restart,
  and Quest 5's deflated zip sounds were staged in unlinked mkstemp files.
  `stage_temp_file` now stages sounds (not images) in glkimp's per-session
  temp directory — swept on a normal `glk_exit`, deliberately preserved on
  an autosave-quit, exactly like the temp files backing open file streams
  — and never unlinks them.  Quest 4 sounds are real files next to the
  story, so the app's restart already worked there.  Verified with The
  Deer Trail (looping mp3 from the zip): boot plays, quit silences,
  relaunch resumes the restored session's loop, window close silences —
  the coreaudiod power assertion tracked through every step.
- **RNG position survives an autorestore** (petter pointed at Bocfel's
  seed + call-count replay).  Quest 4: the erkyrath detstate API
  (`randomness.c` `erkyrath_random_get/set_detstate` -- the full xoshiro
  words + which generator is active, no replay loop needed) rides the
  plist; recover puts the stream back AFTER the silent boot's own draws.
  Quest 5: new `Interp::capture_rng_streams`/`restore_rng_streams`
  (aslx-runtime) -- the fallback stream plus every compiled expression's
  lazily-seeded stream, keyed by expression source (exact: `expr_cache_`
  dedups by source); the blob carries them and they are applied just
  before play resumes, AFTER InitInterface's own draws, so the restored
  prompt's randomness is exactly the saved one.  Engine-level round-trip
  proven (capture -> draw -> restore -> identical draws, including into a
  fresh Interp); headless untouched (`make check` green, native replay
  still 56/57).
- **Quest 4 UNDO history survives an autorestore** (petter pointed at
  Bocfel, which writes its whole save stacks -- Undo/MSav chunks -- into
  its autosave).  The undo ring is flat snapshots, so it serializes
  cleanly: `serialize_undo_history`/`deserialize_undo_history`
  (geas-state.cc, plain GEASUNDO1 stream reusing the save-file record
  writers and its hostile-count guards), `LimitStack::contents()`
  (oldest-first ring walk), `GeasRunner::save/load_undo_history`, and
  autosave.glksave became a length-prefixed GEASAUTO1 container (engine
  state + undo history; the QUEST300 body reads to EOF so bare
  concatenation was not an option; a magicless file still loads as a bare
  legacy state).  load runs after load_state: the snapshots' props_len
  values index the restored append-only props log.  Verified live: take
  lichen -> quit -> relaunch -> UNDO undoes the pre-quit turn ("Undone.",
  item back in the room, inventory empty).
- **Quest 5 undo history deliberately does NOT survive** -- and cannot,
  faithfully: the UndoLogger's actions hold live runtime references
  (shared list/dict backings, kept-alive destroyed-element storage,
  firsttime flag pointers) with no process-independent identity, and the
  reference behaves the same way (QuestViva saves carry no undo history;
  after any load, `undo` reports nothing to undo).  Matching Bocfel here
  would mean inventing semantics the reference does not have, with
  mis-targeted collection edits as the failure mode.
- Accepted scope: an autosave is only taken at the parser prompt, so at
  most the fraction of a turn since the last prompt is lost.
- Still open in presentation: `<backgroundimage>`, babel zip metadata +
  cover art.

## Status (2026-07-17, native `.quest-save` compatibility)

- **The native Quest/QuestViva `.quest-save` format is now bidirectional**
  (§5's last open save item). A native save is a complete, self-contained ASLX
  document (`<asl version=".." original="game.quest"> ... </asl>`) — Quest's
  loader reads ONLY the save (the original `.quest` supplies resources, not
  elements), so the save re-emits the WHOLE world. `aslx-savenative.inc` ports
  QuestViva's `GameSaver` (SaveMode.SavedGame) + `FieldSaver` + `ObjectSaver`:
  - **Writer** (`Interp::save_game_native`): implied/template/dynamictemplate/
    delegate then the nested object graph (containment via the runtime `parent`
    field, ordered by sort_index) then types/functions then flat timers, in
    QuestViva's ElementType enum order. Strings are written type-less and
    everything else with an explicit type (QuestViva emits NO `<implied>` in a
    save, so a type-less attribute reloads as string — this is what keeps an
    already-converted command `<pattern>` as raw regex instead of being
    re-run through the simplepattern loader). v540+ nested collections use the
    attribute name as the tag; a generic `dictionary`/`list` (any non-string/
    non-collection entry, incl. nested dicts — Quest's grid coordinates)
    serialises typed, recursively (`write_value`).
  - **firsttime baking**: QuestViva's `FirstTimeScript.Save` re-serialises an
    already-run firsttime as just its `otherwise` body (or nothing);
    `Interp::bake_firsttime_source` reproduces this on the raw source (a
    statement walk aligned to `collect_firsttime`'s preorder), so exported
    saves don't re-fire intros/one-time text. Only scripts with a run firsttime
    are transformed.
  - **Reader** (`restore_game_native`): overlays the save onto the freshly
    reloaded original via a new loader `override_existing` mode (a same-named
    element DISPLACES its twin — QuestViva `Elements.Add` override), then drops
    mutable-family elements the save omitted. `restore_game` auto-detects
    native vs. the v1 snapshot; `is_native_save_data` sniffs `<asl ...
    original=..>`. In override mode the loader skips implied-type resolution
    and template re-registration (matching QuestViva's implied-less save
    reload; avoids re-converting patterns and duplicating templates).
  - **Loader nested typed collections**: the frame machinery now builds
    `Value`s recursively, so a dictionary whose values are dictionaries
    (Quest's grid-coordinate save state) round-trips. String-typed containers
    (stringlist / string+object dictionaries) keep raw-text/string entries
    (an empty `<value/>` is the empty string); the generic `dictionary`/`list`
    keeps typed, nested values. Fixed a pre-existing loader bug: an empty
    dict-item `<value/>` grabbed the item's surrounding whitespace.
- **App**: the RESTORE metaverb (aslxglk.cc) now accepts a native save too, so
  a game saved in the desktop Quest player / QuestViva imports into
  Spatterlight. The `save` command still writes the v1 snapshot (perfect
  Spatterlight round-trip); the native writer is the interop/export path.
- **Ground truth**: `test/quest5-oracle/save_compat.sh` drives a golden
  walkthrough's first half in one engine, snapshots a native save, and
  continues the second half in the OTHER engine, comparing both directions to a
  pure-QuestViva save/restore baseline (qvh gained a `save:` step + a
  `QVH_LOADSAVE` boot). **~18 diverse games verify byte-identical BOTH
  directions** (incl. Dracula 290 steps, Mouse 308, Bony King 286, and the
  grid-map games Night House / Poppet / Jacqueline). Documented benign
  transcript artifacts (not state): pre-v540 games' output-log replay, the
  "Loaded saved game" fallback (our save omits the HTML log), and a sort_index
  object-order nuance (Basilica). Reader (importing a Quest-written save) works
  for every tested game including all grid-map games.
- Verified: `make check` + ASan/UBSan green (native round-trip + nested-dict
  survival in `test_save_restore_native`); **native goldens still 28/29
  byte-identical** (ICM = the documented oracle artifact) after the loader
  refactor; oracle `check_golden` 29/29; the Glk frontend + smoke harness
  compile.

## Status (2026-07-17, blocking `request (Wait)` / `request (Pause, ms)`)

- **The pre-v540/v550 blocking timing requests are wired to real host hooks**
  (§3's last open input-model item). `request (Wait)` (Core's WaitForKeyPress)
  and `request (Pause, ms)` (Core's Pause) genuinely block mid-script in
  QuestViva — `DoWaitAsync`/`DoPauseAsync` are *awaited*, unlike the
  fire-and-forget `wait` command — so they land in two new blocking host hooks,
  `Interp::do_wait` / `Interp::do_pause(int ms)`, modelled on the synchronous
  `play_sound` hook (the C++ call stack genuinely blocks, so no async unwind /
  TurnSuspended parking is needed). The request dispatch (aslx-runtime.cc):
  version-gates each (`Wait` throws for v540+, `Pause` for v550+, QuestViva's
  exact messages); `Wait` claims the wait slot (`resume_parked_tail()` +
  cancel a pending `wait` callback, exactly like `BeginPrompt(ref _waitTcs)`,
  so a parked synchronous `play sound` resumes inline); `Pause` `int.TryParse`s
  its data (a non-numeric string = no pause) and uses the *separate* pause slot,
  leaving a parked sound untouched. Hook-unset (headless/oracle-parity) is a
  **silent no-op**: the tail continues inline, which is byte-identical to the
  oracle's immediate AutoAdvance (FinishWait/FinishPause) because neither
  request prints and the tail always runs before the turn's FinishTurn. Glk
  side (aslxglk.cc): `do_wait_ui` blocks on a keypress / hyperlink click (the
  reference player's "press any key to continue"), `do_pause_ui` on a one-shot
  Glk timer with a keypress skip; both self-suppress in deterministic mode
  (`gli_sa_delays` off, incl. the smoke harness — never blocks), and
  `do_pause_ui` tears its one-shot timer down and resets the file-scope
  `g_timer_armed` heartbeat flag it overrode so the prompt loop re-arms cleanly.
- Verified: `make check` + ASan/UBSan green; **28/29 native goldens
  byte-identical** (A Hobbit Trek — v540, 6 real `Pause(...)` calls on its
  winning path — stays byte-identical, incl. the newly-evaluated `Pause` data
  expression; ICM remains the documented sound artifact, not this change); A
  Hobbit Trek byte-identical under ASan with no runtime findings; the Glk
  frontend compiles and boots (Dream Pieces).
- §3's input/timing model is now **fully implemented** (get input / show menu /
  ask / wait / timers / Wait / Pause). Still open in presentation:
  `<backgroundimage>`, Spatterlight autosave/autorestore.

## Status (2026-07-17, babel metadata + cover art)

- **`babel/quest5.c` now extracts full iFiction metadata and cover art from
  Quest 5 packages** (milestone 6's last presentation-facing gap). It inflates
  `game.aslx` with zlib (a compact central-directory zip reader +
  `inflateInit2(-MAX_WBITS)`), parses the `<game>` element's bibliographic
  fields (name→title, `<gameid>`→IFID, author/subtitle→headline/category→genre/
  firstpublished/description) and synthesizes an iFiction record; a shared
  `clean_html` decodes entities THEN strips tags so double-encoded HTML titles
  (`&lt;b&gt;DRACULA&lt;/b&gt;` → `DRACULA`) come out clean. Packaged games now
  return their real `<gameid>` IFID (inflate-then-search), falling back to the
  whole-file MD5 byte-for-byte like `babel_handler`. Cover art pulls the
  `<cover>` image out of the zip (stored or deflated), sniffs PNG/JPEG, parses
  dimensions, and returns the bytes byte-identically. `NO_METADATA`/`NO_COVER`
  dropped; the standalone `babel` makefile link line gains `-lz`. Verified: all
  55 corpus packages + raw `.aslx` pass `babel -verify` AND `-lint`, 36 covers
  extract byte-exact, 0 real ASan/UBSan findings across every game ×
  {meta,cover,ifid,format}, clean under `-Wall -Wextra -Werror`. Fixed along
  the way: the returned metadata buffer is now NUL-terminated with the extent
  reserving room for it — babel's own consumers (`get_biblio`,
  `deep_babel_ifiction`) treat it as a C string via `strstr`/`fputs`, so
  without the terminator the CLI `-meta` path overflowed (a latent bug the
  synthesizing modules share). See §5.

## Status (2026-07-17, The Tree + synchronous play-sound parked continuations)

- **The Tree (Father Thyme, 2014/v2.0 2023, ASL 580) joins the corpus as the
  28th driven golden** — the second game (after The Shack) with no published
  walkthrough anywhere: the 94-step winning override was derived entirely from
  game.aslx (`overrides/The Tree.cmd` documents the solution chain). Oracle:
  state=Finished errors=0; check_golden 28/28; native aslx_replay
  **byte-identical** after one engine change (below). 27/28 native goldens
  replay exact (ICM stays the documented oracle artifact).
- **Synchronous `play sound` does NOT park the turn forever — the parked
  continuation resumes when the wait slot is next claimed.** The Tree's
  `insert_spike` is the first corpus case with load-bearing statements after a
  sync sound (`play sound ("click.mp3", true, false)` then LockExit + the
  Crusher Bayliss reveal). QuestViva semantics (PlaySoundScript.ExecuteAsync +
  WorldModel.BeginPrompt): the sound BeginPrompts `_waitTcs`, signals the turn
  suspended (the driver moves on), and awaits the TCS; the NEXT claim on the
  slot — a `wait{}` or another sync sound (BeginPrompt `TrySetCanceled`, whose
  continuation runs INLINE, OperationCanceledException swallowed) or a host
  FinishWait (`TrySetResult`) — resumes everything after the sound statement.
  In the game, the `x tube` holcast's first `wait{}` is what finally reveals
  Crusher. **This parking is only correct when something eventually claims the
  slot.** When the sync sound is the last statement before the ending — HMS
  Victory's "Eight bells.wav", Nearco II's "cantoninfa.mp3" — nothing ever does
  and the win is simply lost. Real Quest resumes on the audio's ended-event, so
  a *headless* host must report playback finished immediately: `aslx_replay`
  installs a no-op `play_sound` hook (the engine already treats "hook returned"
  as "sound over"), and the oracle's `HeadlessPlayer.PlaySoundAsync` flags
  `IsWaiting` to the same effect. Both mirror QuestViva's own WebPlayer under a
  WalkthroughRunner. Games whose tail *is* claimed later are unaffected: the
  transcripts are identical when no output sits between the two resume points. Native port: `TurnSuspended` now carries unwind-captured **frames**
  (each exec_block records its un-run statements; each script boundary —
  run_script / run_callback_boundary — claims the frames below it with a
  context snapshot); turn boundaries `park_suspension()` instead of dropping,
  and `resume_parked_tail()` re-runs each boundary group inside an equivalent
  script boundary (error aborts that group only; a new sync sound re-parks the
  remainder; owed C++ tails — EndPendingCallback finallys, pane refresh — are
  replayed after the frames). Resume points: `wait` registration, sync
  play-sound, finish_wait. Known gaps (commented): remaining loop ITERATIONS
  and a tick batch's remaining timer scripts are C++ state, not captured; a
  suspension inside an expression-position function call cannot resume the
  expression. make check + 27/28 native replays clean.

## Status (2026-07-16, panes — UpdateLists port + side pane / status banner)

- **The panes are live** (milestone 5's third slice): a port of QuestViva's
  `UpdateListsAsync` runs at every turn boundary — send_command /
  set_menu_response / set_question_response / finish_wait / tick engine-side
  (each mirroring the reference's `State != Finished` gates and
  LogException-only error topology, incl. tick's skip-on-TurnSuspended), plus
  the Begin/boot call the hosts make (run_session, aslx_replay). The
  object/exit lists (`GetPlacesObjectsList` + appended exits → "placesobjects",
  `ScopeInventory` → "inventory", `ScopeExits`/v530+ `GetExitsList` → "exits",
  each row a QuestViva `ListData`: GetListDisplayAlias text, display verbs
  incl. the pre-v520 inventoryverbs/displayverbs field fallback, element name,
  GetDisplayAlias) are computed ONLY when the new `Interp::update_list` hook is
  subscribed — QuestViva skips them when its UpdateList event is null, and the
  oracle never subscribes, so headless parity costs nothing.
  `UpdateStatusAttributes` runs REGARDLESS (the oracle ran it every turn;
  goldens stayed byte-identical) and lands in `Interp::update_status` via BOTH
  status channels: modern Core's `JS.updateStatus` and the pre-JS
  `request (SetStatus, text)` older embedded Cores use (Caught! overrides
  UpdateStatusAttributes with the old implementation — newlines normalised to
  `<br/>` so the hook sees one contract). `request (UpdateLocation, ...)` and
  `request (SetPanelContents, ...)` route to their JS-twin hooks the same way;
  request data is only evaluated when a hook consumes it.
- **Glk side** (aslxglk.cc): a right-hand side pane in the classic runner's
  objwin style (20% split + 2px divider, opened only when something is listed,
  probed at startup so the engine hook is only subscribed when the host can
  split — CheapGlk harnesses keep the null-subscriber path). Sections
  localized via the `[InventoryLabel]`/`[PlacesObjectsLabel]`/`[CompassLabel]`
  templates; compass-direction exits (game.compassdirections, matching the
  reference UI's filter) leave "Places and Objects" and form the Compass
  section. Every item is a hyperlink: objects run their first display verb
  ("Look at hat"), compass exits send the direction — clicks land as typed
  commands through the existing link machinery (pane link ids live above
  0x40000000). Status attributes show right-aligned in the banner
  ("Score: 0 | Health: 100%"); the room name now prefers Core's own
  `JS.updateLocation` string (CapFirst(GetDisplayName(...))) with a
  `player`-object fallback for v500-era games that embed pre-JS libraries and
  never set game.pov (Myothian's blank banner). **ASLEvent hyperlinks now run
  the faithful `Interp::send_event`** (SendEventCore: missing-handler print,
  v540..579 FinishTurn, pane refresh) instead of a bare call_function; the
  replayer's `event:` step uses it too (qvh grammar parity — no golden uses
  it yet).
- Verified: goldens 16/17 byte-identical (ICM = the documented artifact),
  `make check` + ASan/UBSan green (`test_update_lists` covers lists/status/
  send_event), corpus still 50-load-0-fail / 49-boot-0-error WITH the pane
  hooks subscribed, Myothian + Dream Pieces + Caught! verified in the app
  (pane fills/refreshes, compass click walks west, banner shows room +
  score/health, per-room art still draws inline).
- Still open in presentation: `<backgroundimage>`, babel zip metadata +
  cover art, Spatterlight autosave/autorestore.

## Status (2026-07-16, sounds + the command-echo fix)

- **Sounds play in the app** (milestone 5's second slice): `play sound` /
  `stop sound` land in new `Interp::play_sound`/`stop_sound` host hooks
  (PlaySoundScript evaluates filename/synchronous/loop in that order — the
  engine now does too, on BOTH paths; a synchronous play still claims the
  wait slot). Hook-less (harnesses/oracle parity) keeps the headless
  semantics: warn-once, sync play throws TurnSuspended. The Glk side
  (aslxglk.cc) registers sound resources exactly like images
  (win_loadsound + a win_findsound round-trip standing in for
  glk_image_get_info; stored zip entries point at the .quest, deflated ones
  stage through a temp file; negative-cached) and plays them on ONE
  schannel, like the reference player's single audio element. A
  **synchronous play blocks in the hook** on evtype_SoundNotify — QuestViva's
  awaited wait slot, resolved by a host that can finish a sound — with a
  keypress as skip/hang-safety; looping-sync and deterministic mode
  (gli_sa_delays off) never block; unplayable sounds "finish" instantly so
  the app never loses turn text; restart/restore stops the old session's
  sound. Verified in the app with a synthetic package (2s wav: boot blocks
  ~2s between the before/after prints — the notify only fires after real
  playback — loop + stop work) — and headless keeps abandoning the turn,
  byte-identically (goldens 16/17, `make check` + ASan/UBSan green).
  ICM's sync `beatingheart.mp3` is the real-game case: with a sound host
  the native engine shows the ending real Quest shows.
- **Command echo de-duplicated** (pre-existing bug, exposed while testing):
  every typed command showed TWICE in the app — the frontend printed its own
  "\n> " prompt AND manually echoed, but the GAME side echoes too (Core's
  `game.echocommand` default: `msg("")` + `OutputTextRaw("&gt; " + cmd)` in
  CoreParser's HandleSingleCommand; the engine does it for pre-v520). The
  reference player's transcript record IS that game-side echo. Fix: parser-
  bound lines get NO printed prompt and NO host echo (library echo forced
  off; the typed ghost line is atomically replaced by Core's "> cmd");
  host-OWNED prompts keep prompt+manual echo (`get input` via
  `command_override()`, menus, yes/no, post-game menu), and the RESTORE/
  TRANSCRIPT metaverbs echo themselves Core-style (echo_metaverb). Verified:
  Myothian Falcon plays to THE END through the smoke harness (menus incl.),
  Serpent's Eye in-app (get-input intro + per-room art + single echo),
  goldens untouched (the replayer has its own qvh step grammar).
- Still open in presentation: compass/inventory/status panes
  (JS.updateList), `<backgroundimage>`, babel zip metadata + cover art,
  Spatterlight autosave/autorestore.

## Status (2026-07-16, pictures — zip resources + inline images via new Glk 0.7.6 IMAGE2)

- **Pictures render in the app** (milestone 5's first slice): Core's `picture`
  (v540+ `<img>` print AND the pre-540 direct-UI path), the `{img:}` text
  processor and hand-written `<img src="' + GetFileUrl(..) + '">` all draw
  inline in the buffer window, verified visually (synthetic v550 + v520
  packages; Dracula/Mouse carry the real thing mid-game).
- **Zip resource API** (`aslx.hh`): `zip_list_entries` / `zip_extract_entry` /
  `zip_find_entry` (exact-then-case-insensitive, QuestViva's OrdinalIgnoreCase
  resource table) generalize the old game.aslx-only reader;
  `extract_game_aslx` is a wrapper now (still exact-name, like
  `zip.GetEntry`). `test_zip_package` + committed `fixtures/aslx/package.quest`
  (stored + deflated + directory entries).
- **Frontend resources** (aslxglk.cc): the package entry table is listed once
  at startup; `resource_bytes(name)` pulls a payload byte-range out of the
  .quest on demand (raw-.aslx games read game-adjacent files instead;
  `..` rejected like GetExternalUrlAsync). Images register with the
  Spatterlight backend under private resource numbers (win_loadimage — the
  classic runner's show_image pattern): stored entries point the app straight
  at the .quest at their byte offset; deflated ones (the packager's default)
  inflate through a mkstemp file that is unlinked as soon as
  glk_image_get_info round-trips. Failed names are negative-cached.
- **Glk 0.7.6 GLK_MODULE_IMAGE2 implemented app-wide** (user-requested): the
  spec's `glk_image_draw_scaled_ext` + `imagerule_*` + `gestalt_DrawImageScale`
  land in glkimp (glk.h/image.c/gestalt.c/gi_dispa.c with upstream's 0x00EC
  dispatch entry) and the DRAWIMAGE protocol carries imagerule+maxwidth
  (`struct drawrect`; 0 = legacy). App side: `MyAttachmentCell` stores the
  rule and resolves its display size against the line-fragment width in
  `cellFrameForTextContainer:` EVERY layout pass — so `imagerule_WidthRatio`
  and `maxwidth` track buffer resizes dynamically, exactly per spec, with no
  stored scaled copies (draws scale at render time, high interpolation).
  Legacy cells get 0.7.6's behaviour change (images wider than the window
  reduce proportionally) through the same layout-time clamp, while their
  existing theme-rescale machinery (REFRESH/xscale — bocfel Z6) is untouched;
  rule cells are skipped by it. Graphics windows resolve rules once at draw
  and ignore maxwidth, per spec. Direct win_drawimage callers (bocfel z6,
  hugo) updated. The Quest 5 frontend draws
  `WidthOrig|AspectRatio, maxwidth=$10000` — original size, never wider than
  the window, re-fitting on resize (verified by shrinking the window).
  ADRIFT/Hamper title graphic (legacy path) verified unchanged.
- `make check` + ASan/UBSan green, goldens 16/17 byte-identical (ICM is the
  documented oracle artifact), full app + all terps build. `picture` now
  ALWAYS evaluates its filename first (PictureScript does; the pre-540
  headless path previously skipped evaluation) — no golden moved.
- **Frame pictures redirect inline** (same day, follow-up): Quest's picture
  frame — the persistent panel the reference player keeps above the
  transcript, driven by `SetFramePicture`/`ClearFramePicture` and set from
  the room's `picture` attribute on every room change (OnEnterRoom) and at
  boot/restore (InitInterface) — all funnels through `JS.setPanelContents`.
  A new `Interp::set_panel_contents` host hook routes it (arg unevaluated
  when unset, as before; unit-tested); the frontend extracts the `<img src>`
  and draws it inline when the filename CHANGES (consecutive re-sets stay
  quiet; "" clears the tracker; reset per session), the way Scarier folds
  ADRIFT graphics into the buffer. Verified in Quest for the Serpent's Eye
  (126 images, per-room `<picture>`): title, then per-room art above each
  room description. NOTE Dracula's remaining art is `<backgroundimage>`
  (JS page background) + `<cover>` — different features, still open.
- Still open in presentation: sounds (`resource_bytes` is the extraction
  half; the synchronous `play sound` TurnSuspended semantics need a host that
  can finish a sound), compass/inventory/status panes (JS.updateList),
  `<backgroundimage>`, babel zip metadata + cover art, Spatterlight
  autosave/autorestore.

## Status (2026-07-16, later still — undo + save/restore land; milestone 4 complete)

- **Undo (UndoLogger port)**: `undo` and `start transaction` are real script
  commands now — see the §2 UndoLogger entry for the full mechanism (lazy
  per-command transactions, all nine action kinds, the UndoTurn/NothingToUndo
  templates, and QuestViva's PreviousTransaction-chain quirks, ported
  verbatim). Verified through Core's real parser (Dream Pieces: take/i/undo
  echoes "Undo: i" and reverts); goldens unaffected with the logger running
  hot on every replayed command. `test_undo` in the runtime tests.
- **Save/restore (v1 snapshot)**: `aslx-state.inc` serializes the DYNAMIC
  state only — every live mutable-family element (object/exit/command/verb/
  game/turnscript/timer) with elem_type/inherits/anonymous/sort_index and its
  own fields as full recursive Values (nested lists/dicts round-trip, scripts
  as source, object refs by name), which load-time elements were destroyed,
  runtime-created elements, and `firsttime` flags (keyed by script source
  text, preorder). A restore RELOADS the game file fresh and applies the
  snapshot, then follows QuestViva's saved-game boot exactly: begin_timers
  and StartGame are SKIPPED, InitInterface re-runs, "Loaded saved game" is
  printed (the reference's no-transcript fallback). Undo history and RNG
  streams reset, as in QuestViva. Glk wiring (aslxglk.cc): Core's own `save`
  command reaches the host through the new `request (RequestSave)` /
  `requestsave` dispatch (`Interp::request_save` hook) and prompts for a
  file; RESTORE/LOAD GAME is a frontend metaverb (Quest has no restore
  command — loading is a UI action) that validates the save against a
  scratch reload before tearing down the session; the post-game menu gained
  RESTORE. `test_save_restore` covers state survival, byte-identical
  re-save round-trip, StartGame-not-re-run, firsttime persistence, and
  wrong-game/truncated rejection; the CheapGlk smoke harness plays
  command.aslx, saves mid-game, and restores in a second process with
  location/inventory intact. `make check` green, ASan/UBSan clean, goldens
  16/17 (the documented ICM artifact), geas Xcode target builds.
- Still open around save: Spatterlight autosave/autorestore integration
  (hooking the glkimp autosave path like other terps), saving while an
  engine prompt (`show menu`/`ask`/`get input`/`wait`) is pending (the
  snapshot does not capture pending callbacks; Core's `save` command can
  only run from a normal parsed command anyway), and native `.quest-save`
  compatibility (GameSaver.cs re-serializes the whole world to standalone
  ASLX — deliberately NOT what v1 does; the snapshot applies onto a reload
  instead, which sidesteps every loader round-trip hazard).

## Status (2026-07-16, late night — Glk frontend + app integration: Quest 5 playable in Spatterlight)

- **Quest 5 games now run in the real app**: double-clicking a `.quest`/`.aslx`
  imports it (babel claims it as format `quest5`), launches the geas terp, and
  the game plays through the native engine in a Glk window. Verified end-to-end:
  the app-bundle terp boots Dream Pieces and sits in the prompt loop
  (`read_line` → `glk_select`, confirmed by `sample`), keystrokes round-trip,
  and via the CheapGlk smoke harness Dream Pieces and The Myothian Falcon play
  to their WINS through the exact frontend code (`test/aslxglk_smoke`,
  `make aslxglk_smoke`, needs the local corpus).
- **`aslxglk.cc` — the Glk frontend** (unity-includes the loader+runtime, the
  only aslx TU in the geas binary; `geasglk.cc:glk_main` sniffs the story file
  and dispatches, .asl/.cas untouched):
  - **Prompt loop** (§3 Glk wiring done): asks the engine what it awaits —
    `pending_menu()` → numbered menu (number / key / display-text answers,
    empty line cancels when allowed), `pending_question()` → rendered caption
    + yes/no, `pending_wait()` → keypress, else line input → `send_command`.
    `menu_provider` runs the same menu UI as a nested input loop for the
    expression form of ShowMenu. Uni line input (Glühwein-safe), manual echo
    (engine echoes for pre-520), transcript metaverb, post-game
    RESTART/QUIT menu (undo/restore need later milestones), restart =
    fresh World+Interp. Engine `warnings` are flushed to the player
    bracketed+emphasized after each step.
  - **HTML→Glk renderer** (§4 first two items): `<br/>` = newline;
    bold/italic/underline from `<b>/<i>/<u>/<strong>/<em>` AND span-style CSS
    (`font-weight:bold` etc.), nesting-counted, mapped like the classic
    runner (Alert/Emphasized/Subheader/User2); entity decode incl. numeric;
    UTF-8 → `glk_put_char_uni`; everything else stripped. **cmdlinks become
    Glk hyperlinks**: `data-command` (command/exit links) sends the command
    as if typed, `onclick="ASLEvent('F','p')"` (Core ShowMenu options) calls
    the function, `elementmenu` object links fall back to "look at <id>";
    without hyperlink support links render underlined.
  - **Timers**: 1s heartbeat armed while any timer is enabled (gated on
    `gli_sa_delays` like the classic loop); a tick that opens a prompt or
    ends the game cancels pending input and re-dispatches.
  - **Core dir**: `$ASLX_CORE`, else exe-relative `../Resources/aslx-core`
    (app bundle; verified in a fake-bundle layout) or `./aslx-core` (dev).
- **App integration** (§5 mostly done): `babel/quest5.c` (claims PK zips
  containing game.aslx + raw `<asl` XML; raw-XML `<gameid>` GUID returned as
  IFID, zip metadata needs zlib — still NO_METADATA/NO_COVER, see §5),
  registered in modules.h + babel makefile; `quest5 → geas` in AppDelegate,
  Autorestore ("Quest 5"), glkimp fileref; `quest`/`aslx` in gGameFileTypes +
  Info.plist (document type + `public.quest5` UTI); Xcode: aslxglk.cc in the
  geas target (links libz+libexpat), `aslx-core/` copied into app Resources.
- Still open for presentation/integration: images (`picture`'s `<img>` needs
  zip resource extraction), sounds (incl. resolving the synchronous
  `play sound` TurnSuspended semantics against a host that can actually
  finish a sound), compass/inventory/status panes (JS.updateList requests),
  `request (Wait/Pause)`,
  babel zip metadata + cover art, game-local libraries inside the zip,
  locked inherited collections. (Save/restore + undo landed later the same
  day — see the top status entry.)

## Status (2026-07-16, night — error-cascade topology: 16/17 golden-exact)

- **16 of 17 golden walkthroughs replay BYTE-IDENTICAL**, including the two
  known-glitchy games. The 17th (I Contain Multitudes, 27 diff lines) is the
  DOCUMENTED QuestViva artifact (pending synchronous-sound TCS leak — the
  oracle never shows the ending; the native engine correctly does, like real
  Quest. Do not chase). `make check` green, ASan/UBSan clean, corpus still
  50-load-0-fail / 49-boot-0-error (spondre = QuestViva's own abort point),
  oracle check_golden 17/17.
- **Landed in this batch** (each mechanism verified by instrumenting the
  oracle: qvh gained `QVH_TRACE_ERRORS=1` — stack dump per LogError — and the
  engines share `ASLX_TRACE_CALLS=1` / `QVH_TRACE_CALLS=2` streaming
  per-function-call depth traces, diffed frame-for-frame):
  - **NCalc's double-evaluation quirk** (Whitefield's 8×/8×/1× error
    cascade): for the operators QuestViva's EvaluateBinaryAsync intercepts
    (`+ - * / % = <>`), the handler evaluates both operands; when both are
    "standard" NCalc types (null/number/bool/string) it sets no result and
    NCalc's native path evaluates the operands AGAIN — BinaryEventArgs caches
    with `??=`, so only a NULL operand actually re-runs. An erroring ASLX
    function (which prints at its own boundary and yields null) therefore
    runs once more per enclosing binary op — 2^3 = 8 error prints for
    `Grid_Get(x) + a - b + c` — feeding the 20-error breaker at exactly the
    oracle's rate. Ported in eval Binary (aslx-runtime.cc).
  - **Native NCalc null semantics** (same both-standard path): arithmetic
    with a null operand yields null SILENTLY (MathHelper); `null + "s"`
    concats (null → ""); a null on ONE side of a relational comparison is a
    "type conflict" — false for everything except `<>`. And
    **HandleBinaryResult's int/int division intercept**: `10 / 4` = 2 (FLEE
    IL semantics), div/mod by int 0 throws .NET's "Attempted to divide by
    zero."
  - **LogException-only wrappers** (`Interp::log_exception`): PrintAsync →
    OutputText, HandleCommandAsyncInternal, and TryFinishTurnAsync catch
    LogException-ONLY — logged, not printed, NOT fed to the breaker. Only
    throws that BYPASS RunScriptAsync's catch reach them — above all the
    depth-cap throw, which fires BEFORE the callee's try. Consequence: a
    frame at the 200 cap that catches an error still counts it, but its
    report-print itself dies silently inside PrintAsync (the catch runs
    before the depth decrement) — which is why Bony King's death spiral
    prints ONE error, logs 31, counts 16, and never wedges.
  - **`on ready` callbacks are real stack frames**: AddOnReady and the
    on-ready flush loop run callbacks through RunScriptAsync, so
    run_callback_boundary now does full depth accounting (cap-throw BEFORE
    the guarded region, propagating to the ENCLOSING boundary) and
    add_on_ready releases the pending count on a throw (AddOnReady's
    finally). This makes the depth at which sub-calls die during Bony King's
    beforeenter spiral match the oracle frame-for-frame (which decides
    whether firsttime text, display-name links, or whole descriptions
    survive each unwinding iteration).
  - **Wedged-vs-Finished mirrored in the replayer**:
    `Interp::script_errors_fatal()` exposes the breaker, aslx_replay reports
    `[state=Wedged]` exactly like qvh's reflection probe.

## Status (2026-07-16, evening — golden-parity batch)

- **14 of 17 golden walkthroughs replay BYTE-IDENTICAL through the native
  engine** (up from 2 this morning; test/aslx_replay.cc vs
  quest5-oracle/golden/): Dream Pieces, One Night Stand, A Christmas Game (+
  Sequel), A Hobbit Trek, Basilica de Sangre, EFMB, all three Guttersnipes,
  Jacqueline, Mouse/Christmas, Myothian Falcon, Dream Pieces 2. The three
  remaining are the two known-glitchy games — Whitefield (598 diff lines, the
  oracle's error-breaker Wedge) and The Bony King of Nowhere (345, the
  pov.parent=null death-spiral scene) — whose tails need QuestViva's exact
  error-cascade topology (which LogException-only wrapper catches which throw:
  PrintAsync/UpdateLists/FinishTurn swallow depth-errors without printing or
  feeding the breaker), plus I Contain Multitudes (25, a DOCUMENTED QuestViva
  artifact: its pending synchronous-sound TCS is cancelled by the next wait's
  BeginPrompt, whose synchronous .NET continuation re-registers and is then
  clobbered by `tcs = new`, leaking _pendingCallbackCount — QuestViva never
  shows the ending; the native engine does, like real Quest. Do not chase).
  `make check` green throughout; corpus still 49/50-boot-0-error (spondre
  matches QuestViva's own abort point); oracle check_golden 17/17.
- **Landed in this batch** (each found by chasing a golden first-divergence,
  each verified against QuestViva source):
  - **Timers** (§3): TimerRunner port — `begin_timers()`/`tick(secs)`/
    `next_timer_seconds()`/`has_enabled_timeout()` on Interp;
    enabled/interval/trigger fields against game.timeelapsed; due scripts run
    with `this` = the timer; disabled timers get triggers pushed forward.
    aslx_replay mirrors the oracle's DrainTimers exactly (drain only
    self-destructing "timeout*" timers, tick exactly the reported delta).
    EnableTimer/DisableTimer/SetTimeout/SetTimeoutID are Core ASLX and run as
    library code; there is NO enable/disable script command in QuestViva.
  - **Synchronous `play sound` suspends the turn** (`TurnSuspended` in
    aslx-runtime.hh): QuestViva parks the whole async chain on the wait slot
    and headless nothing resolves it, so everything after the statement —
    including FinishTurn — is silently abandoned. Thrown by the statement,
    passed through script boundaries un-reported, caught at send_command /
    prompt callbacks / tick / host boot.
  - **`type` + `elementtype` fields** on every element (QuestViva sets them at
    Element construction; verbs are ObjectType.Command). Core's
    AddToResolvedNames gates game.lastobjects on `obj.type = "object"` — this
    is what makes "it"/pronoun resolution work. Runtime-created elements also
    now get a `name` field (SetTimeout's `destroy (this.name)` was a no-op).
  - **UTF-8 identifiers** ("Glühwein"): bytes >= 0x80 are word characters in
    the lexer, encode_identifier_spaces, and keyword-boundary checks.
  - **Collections: eager backing + clone-on-set.** Every collection creation
    site allocates its backing eagerly (Value::ensure_backing; a lazily-null
    store broke reference semantics — mutations through a function parameter
    allocated on the copy). And Fields.Set semantics: assigning a list/dict
    CLONES it whenever the assignment changes the OWN attribute (QuestList.
    RequiresCloning is unconditional; the changed-check consults own attrs
    only) — `newPOV.pov_alt = newPOV.pov_alt` localises an inherited list
    (Basilica's possession). v530+ null assignment REMOVES the attribute
    (HasAttribute goes false; ICM's `player.grid_coordinates = null`).
  - **Destroyed elements excluded from enumerations** (AllObjects/AllCommands/
    AllExits/AllTurnScripts/GetDirectChildren + live_timers): destroy only
    unregisters the name, and a reused name previously enumerated twice —
    Jacqueline's SetTurnTimeout chain fired at half the authored delay.
  - **Every `parent` write re-appends the child** (even same-value writes:
    SetParentFromFields runs unconditionally in Fields.Set) — WearGarment's
    redundant `object.parent = game.pov` really reorders the inventory (Mouse).
  - **QuestViva error semantics**: expression failures wrap as
    "Error evaluating expression '<src>': <msg>" at the per-compiled-expression
    root (EvalError keeps the innermost message across Eval() nesting); no
    "(in fn)" suffix on player-facing messages (kept in the world.errors log);
    object-taking builtins throw .NET's "Value cannot be null. (Parameter
    'obj')" / GetParameter's "expected object parameter" texts;
    *DictionaryItem throws "The given key 'k' was not present in the
    dictionary."; report printing goes through PrintAsync (SafeXML + Core
    OutputText). Statements whose expression fails to PARSE compile to a stub
    that errors only if reached (NCalc parses lazily — EFMB's
    `MoveObject (coin, )` behind a false guard stays silent).
  - **RNG is per-compiled-expression** (the big one): QuestViva's
    NcalcExpressionEvaluator constructs its own ExpressionOwner — and thus its
    own fresh seed-1234 ErkyrathRandom — PER COMPILED EXPRESSION. Each root
    Expr now owns a lazily-created Rng (Interp::active_rng()); both engines
    gained stream tracing (ASLX_TRACE_RAND / QVH_TRACE_RAND, same format) to
    diff streams. Caveat: we cache compiled expressions by SOURCE TEXT, so two
    scripts with byte-identical RNG expressions share one stream where
    QuestViva would have two — not yet observed in a golden.
  - **Sorts use .NET culture word-sort** (dotnet_string_less: case-insensitive
    first, lowercase-before-uppercase tiebreak) in ObjectListSort/
    StringListSort — EFMB sorts turnscripts by name ("archimedes" <
    "Attendant").
  - **`create exit`** (3/4/5-arg CreateExitScript incl. generated "exitN" ids
    + anonymous flag), **`Clone`/`ShallowClone`** (GetUniqueElementName
    trailing-digit numbering, type-stack + own-fields copy with fresh
    backings, deep child clone — Dream Pieces 2), **expression-form
    `ShowMenu(caption, options, allowCancel)`** returning the key via a new
    synchronous `Interp::menu_provider` host hook (ExpressionOwner.ShowMenu
    awaits mid-expression; the replayer feeds the next script line, the Glk
    layer will use a nested prompt loop — Myothian Falcon), **`picture`
    prints an `<img>` via OutputText on v540+** (its `<br/>` is the blank
    line in oracle transcripts), and pre-540 empty prints emit nothing
    (`<output></output>` strips to nothing).
  - The replayer's HTML strip is now qvh-exact (case-SENSITIVE `<br/>` only —
    uppercase `<BR>` is dropped without a newline, St Hesper's cage sign).

## Status (2026-07-16)

- **Ground-truth oracle: built and frozen** (§7, milestone 6's diff half).
  `terps/geas/test/quest5-oracle/` drives real `.quest`/`.aslx` games headless
  through QuestViva (.NET port of the Quest 5 engine), RNG routed through
  `erkyrath_random()` (seed 1234), and captures normalised golden transcripts.
  17 walkthroughs driven — **15 Finished, 1 Running (ICM), 1 Wedged
  (Whitefield)**; the last two are genuine QuestViva-vs-Quest limitations, not
  drift. `check_golden.sh` replays all 17 `.cmd` scripts and diffs vs frozen
  `.out` (17/17), doubling as a determinism check. This is the reference the
  native engine will diff against, with no .NET dependency at replay time.
- **Native engine: milestones 1–2 landed; milestone 3 mostly done; milestone
  4's input model landed** — Core boots, real player commands
  (`look`/`take`/`examine`/`open`/`go`) run end-to-end through Core's parser
  with zero script errors, and the prompt layer (`get input`/`show menu`/
  `ask`/`wait` + disambiguation menus) works through the new `send_command`
  host API (see the M3/M4 entries below).
  - **M1 loader** (`aslx.hh`/`aslx.cc`): `.quest` (in-memory zip over zlib) or
    raw `.aslx` (libexpat SAX) → typed element tree (containment, `<inherit>`
    chains, `<implied>`/`<template>` registration, full attribute-type set,
    simplepattern→regex). Loads all **76** corpus games, 0 failures.
  - **M2 script/expression core** (`aslx-runtime.hh`/`aslx-runtime.cc` +
    `-parse.inc`/`-builtins.inc`): a hand-written lexer + recursive-descent
    expression parser over the typed `Value`, and a script interpreter for
    `msg`, `if`/`else if`/`else`, `while`, `for`, `foreach`, `=` assignment
    (var and `obj.attr`), function calls, `return`, comments. Field access
    resolves inheritance (own → inherited types, most-recent-first). ~40
    built-ins (attribute/type, list, string, number) with `GetRandomInt`/
    `GetRandomDouble` routed through a deterministic erkyrath_random() port.
    Tested by `test/aslx_runtime_test.cc` (`make check`, clean under ASan/UBSan).
  - **M3 loader half landed** (Core bundling + `<include>` resolution).
    The non-editor Core libraries are bundled under `terps/geas/quest5/aslx-core/`
    (root `Core*.aslx`/`GamebookCore.aslx` + `Languages/*.aslx`), and the
    loader resolves `<include ref="..">` inline in source order — game-adjacent
    dir, then Core dir, then Core/Languages — matching QuestViva's
    `GetLibraryStream()`. `Editor*`/`CoreEditor*` refs are skipped (not
    bundled). Recursion is deduped/guarded. Three Core-driven attribute types
    now load too: `listextend` (flagged `Value::list_extend`; merge-on-read
    landed 2026-07-16 -- see the typed-lists entry), `list` (heterogeneous `<value>` list), and delegate-typed
    attributes (an attr whose `type=` names a `<delegate>` loads its body as a
    delegate-implementation Script). **All 76 corpus games load with 0 errors**;
    a fresh game boots the full Core tree (~330 functions / ~56 types). Wired as
    `test/aslx_loader_test.cc:test_coreboot()` (fixture `coreboot.aslx`),
    `make check` green, clean under ASan/UBSan.
  - **M3 script layer widened** (2026-07-15): the big batch of Core-critical
    script commands and built-ins landed, all unit-tested and clean under
    ASan/UBSan; the 76-game 0-error load invariant is preserved.
    - `[Something]` template substitution now resolves at **load time** (like
      QuestViva's `GameLoader.GetTemplate`): `[name]` in script bodies and
      string attributes is replaced with the registered template's text
      (later-registered wins, unmatched refs left verbatim so regex classes
      like `[^\w]` survive). `Template()`/`DynamicTemplate()` built-ins cover
      the runtime calls. `aslx.cc:Loader::replace_templates`.
    - New script commands: `switch`/`case`/`default`, `firsttime`/`otherwise`
      (per-instance run flag), `do (obj,"script"[,params])`, `invoke`,
      `create`/`create`-with-type, `destroy`, `set(obj,attr,val)`,
      `list add`/`list remove`, `dictionary add`/`dictionary remove`,
      `on ready` (deferred FIFO queue + `drain_on_ready()`), `error`, `finish`
      (sets `World::finished`), `undo`/`start transaction` (accepted, no-op
      until the UndoLogger lands), and the `JS.*` bridge (`JS.addText` → output
      sink, other `JS.*` ignored). List/dictionary mutators resolve their
      target as an lvalue (local var or obj.attr, copy-on-write for inherited).
    - New built-ins: `Template`/`DynamicTemplate`, dictionary
      constructors/`DictionaryItem`/`DictionaryCount`/`DictionaryContains` and
      the typed `*DictionaryItem`, `StringListItem`/`ObjectListItem`,
      `AllObjects`/`AllCommands`, `GetDirectChildren`/`GetAllChildObjects`,
      `Contains`, `GetAttributeNames`, 2-arg `TypeOf(obj,attr)`, `CapFirst`,
      `SafeXML`. (`JSSafe` is Core ASLX, not an engine primitive — it comes
      free once these run.)
    - Tests: `test/aslx_runtime_test.cc:test_script_commands` +
      `:test_new_builtins` (fixture `runtime.aslx` gained a template, a
      dynamictemplate, and an object script).
  - **M3 parser primitives: regex landed** (2026-07-15): `IsRegexMatch`,
    `GetMatchStrength`, `Populate` (each in the 2-arg and 3-arg/cacheID forms)
    port `Utility.IsRegexMatch/GetMatchStrength/Populate` + `RegexCache.cs`. The
    core wrinkle: Quest patterns are .NET regexes with named groups
    (`^look at (?<object1>.*)$`) and `std::regex` (ECMAScript) has no named-group
    syntax, so `rewrite_dotnet_regex` (aslx-runtime.cc) strips `(?<name>`/
    `(?'name'` to plain `(` and records the group name by capture index, leaving
    non-capturing/lookaround groups untouched. Repeated names across an
    alternation (`take (?<object>.*)|get (?<object>.*)`) are coalesced to one
    logical group with .NET's last-successful-capture value (`named_groups`).
    Matching is `regex_search` (search, not full-match, mirroring .NET `Match`)
    with `icase`; the `RegexCache` keys on cacheID alone (pattern ignored on a
    hit), so the no-cacheID forms compile fresh. Wired as
    `test/aslx_runtime_test.cc:test_regex_primitives`; `make check` green, clean
    under ASan/UBSan; the 76-game 0-error load invariant is preserved. This
    proves `std::regex` ECMAScript covers what CoreParser's simplepattern regexes
    need (§2 "Command parsing comes free" caveat resolved for the common case).
  - **M3 boot pipeline: the game boots and renders through Core** (2026-07-15):
    a fresh game plus the full Core library now runs `InitInterface` +
    `StartGame` + the `on ready` queue with **zero script errors**, and the
    opening title + room description come out through Core's own `{...}` text
    processor. Regression: `test/aslx_runtime_test.cc:test_coreboot_runs`
    (`make check` green, clean under ASan/UBSan; 76-game 0-error load preserved).
    Landed to get there, all traced empirically by running Core and fixing what
    it hit:
    - **`msg` → `OutputText`** (v540+): `msg` now calls Core's `OutputText`
      (which runs `ProcessText` then `JS.addText`) when it exists, so the entire
      `{...}` text processor -- itself ASLX in `CoreOutput`/`CoreTypes` -- runs.
      Unlocked by three primitives: `Eval(expr[,scope])`, `foreach` over a
      `scriptdictionary` (the `textprocessorcommands` dispatch table), and the
      pre-existing `ScriptDictionaryItem`/`invoke`-with-params.
    - **`not` precedence bug fixed** (critical): `not` bound tighter than `=`, so
      `not obj = null` -- which is *everywhere* in Core -- parsed as
      `(not obj) = null`. Moved `not`/`!` to their own level between `and/or` and
      equality (`parse_not`), matching QuestViva's notOperator; `-`/`~` stay
      tight. This alone was silently breaking most Core conditionals.
    - **Backslash string escapes**: `obscure_strings`, `remove_comments`, and the
      expression lexer now honour `\"` `\'` `\\` (Parlot semantics). Before, a
      body like `Replace(s,"\"","")` swallowed the rest of the function.
    - **Implicit default types**: every element gets `defaultobject`/
      `defaultgame`/`defaultexit`/`defaultcommand`/`defaultturnscript` prepended
      (QuestViva `ObjectFactory` + `DefaultTypeNames`); this is what gives the
      game object its `showtitle`/`textprocessorcommands` defaults.
    - **`parent` as a field**: containment is now exposed as a `parent`
      ObjectRef field (not just the load-time pointer), and
      `GetDirectChildren`/`GetAllChildObjects`/`Contains` derive from it, so a
      runtime `MoveObject` (which sets `obj.parent`) stays correct.
    - **CoreEditor.aslx bundled**: it is an "editor" library but QuestViva loads
      it at runtime (defines `editor_player` + runtime types every editor-made
      game inherits); its 18 UI sub-includes stay unbundled/skipped.
    - New builtins: `HasScript`/`HasDouble`/`HasDelegateImplementation`,
      `AllExits`/`AllTurnScripts`, `GetTimer`, `GetUIOption` (empty headless),
      `ToInt`/`ToDouble`/`ToString`, `Eval`. `request` accepted as a no-op (its
      first arg is a bare enum identifier, so it must not evaluate its args).
  - **M3 command driving: real commands run end-to-end** (2026-07-16): a booted
    game now takes player input through Core's `HandleCommand` → `ScopeCommands`
    → `IsRegexMatch`/`GetMatchStrength` → `ResolveName`/`GetScope` → verb script
    chain, with **zero script errors**. `look`, `take`/`inventory`, `examine`,
    `open`, and `go`-through-an-exit (into a second room, with the object list
    and the `{exit:}` link rendered) all work. Regression:
    `test/aslx_runtime_test.cc:test_command_driving` (fixture `command.aslx`);
    `make check` green, clean under ASan/UBSan; 50-package corpus still loads
    0-error. Landed, each traced by running a command and fixing what Core hit:
    - **Command patterns resolve three ways** (`aslx.cc`, matching QuestViva's
      CommandLoader): a `pattern=` attribute is template-substituted then stored
      RAW as its regex (`[look]` → `^look$|^l$`; `^restart$` verbatim) with no
      simplepattern conversion; a `template=` attribute names a verbtemplate
      whose `;`-joined text goes through `ConvertVerbSimplePattern` (lazy
      `(?<object>.*?)`, plus `displayverb`); a nested `<pattern>` keeps the
      existing greedy simplepattern conversion. `template_text()` unifies the
      verb/plain template lookup (verbtemplates `"; "`-joined, `AddVerbTemplate`).
    - **`name` exposed as a field** on every element (QuestViva
      `FieldDefinitions.Name`). It was missing, so `cmd.name` -- the RegexCache
      cacheID in `HandleSingleCommand` -- was empty and every command collapsed
      onto one cache entry (the first pattern won for all input). This also fixed
      the empty `{object:}`/room description, which key off `.name`/`.alias`.
    - **Lists and dictionaries are now REFERENCE types** (`aslx.hh`): their
      backing lives behind a `shared_ptr`, so copying a Value (function-arg
      binding, `y = x`, reading a field) shares it and `list add`/`dictionary
      add` through any alias is visible everywhere -- exactly Quest's
      QuestList/QuestDictionary. This is what makes `CompareNames`,
      `ResolveNameFromList`, and every Core helper that builds a caller-supplied
      list work. Builtins that derive a *new* collection (`ListExclude`,
      `ListCombine`, `ObjectListSort`) call `detach()` first.
    - **Dictionary values are typed per-entry** (`Value`, not a flat string):
      Quest dicts hold boxed values and a single dict can mix objects, strings
      and booleans (CoreParser's `currentcommandresolvedelements` holds the
      resolved objects alongside a boolean `multiple`). `do`/`invoke`/`Eval`
      param binding now hands each resolved object to the verb script as an
      object ref, not the string of its name -- the fix that let `open` pass its
      object to `TryOpenClose`.
    - **QuestList operators** `+`/`-`/`*` (`QuestList<T>` overloads): list+list
      merge, list+elem append, elem+list prepend, list-elem remove-first,
      list*list union -- handled before scalar/string arithmetic. `GetScope`'s
      `ListCombine(...) - game.pov` was silently coercing the list to numeric 0.
    - **`GetObject` resolves exits** (they are `ElementType.Object` in QuestViva,
      just `ObjectType.Exit`), so `ProcessTextCommand_Exit` renders `{exit:}`.
    - New builtins: `IsGameRunning`, `ObjectListSort`/`ObjectListSortDescending`,
      `StringListSort`/`StringListSortDescending`.
  - **M3 real-game blockers cleared: 49/50 corpus games boot with 0 errors**
    (2026-07-16, second batch — the whole corpus through `InitInterface` +
    `StartGame` + `on ready`, up from 2-of-5 sampled the day before). Landed,
    each verified against QuestViva source and unit-tested
    (`test/aslx_runtime_test.cc:test_realgame_constructs`):
    - **Multi-word (space-encoded) identifiers** (`Utility.EncodeIdentifierSpaces`
      port): outside string literals, adjacent word-char words join into one
      identifier unless either is an expression keyword (`and or xor not if in`,
      case-sensitive). We join with `'\x01'` and the lexer folds it back to a
      space inside a single Ident token — no decode pass. Covers Dracula's
      `GetBoolean(OUTSIDE INN, "MORNING")` and Magi's `game.Next text` (member
      names with spaces work too).
    - **`in` / `not in`** at the relational precedence level (QuestNCalc
      grammar): list membership, dictionary-key lookup, substring.
    - **`x => { script }` script-literal assignment** (SetScriptScript; `=>`
      checked before `=`, like SetScriptConstructor). This was Everyman's
      `unexpected token '>'`.
    - **Trailing script-block argument**: `Proc (args) { script }` (and
      `Proc { script }`) passes the block as one extra script-literal argument
      bound to the function's last parameter (FunctionCallScript.paramFunction).
      This is how ShowMenu callbacks and the classic inlined-CoreTimers
      `SetTimerScript (timer) { ... }` pattern work.
    - **`this` binding**: `do (obj, "action" [, params])` and `rundelegate` run
      their script with `this` = the object (WorldModel.RunScriptAsync's
      thisElement) — fixes every `changedparent`-style script Core invokes via
      `do`. `invoke` deliberately does NOT bind this (matches InvokeScript).
    - **`rundelegate` + `RunDelegateFunction`**: shared implementation; the
      delegate's `paramnames` come from the `<delegate>` element named by the
      implementation Script's declared_type; returns the script's return value.
    - **`create timer` / `create turnscript`** (create_object grew an elem_type
      param; turnscripts get defaultturnscript, timers have no default type) and
      **`GetUniqueElementName`** (trailing-digits-stripped numbering).
      **`GetObject` resolves turnscripts and the game element** too (they are
      ElementType.Object in QuestViva) — dynamic-turnscript code relies on it.
    - **`True`/`FALSE`/`Null` are case-insensitive** (NCalc `Terms.Text(...,
      true)`; null via the evaluator's parameter check).
    - **NCalc `if(cond, a, b)`** (lazy branches) **and `cast(x, int)`** (the
      type arg is a bare identifier, special-cased before evaluation like
      `_evaluatingCastType`).
    - **`GetFileURL`** returns the filename headless (real URL mapping is
      presentation-milestone work).
    - **Media commands no-op with a one-time `world.warnings` entry** (new
      vector, separate from `errors` so the 0-error invariants stay strict):
      `picture`, `play sound`, `stop sound`, `insert`. `wait { ... }` parses and
      (until the §3 input model) runs its callback immediately.
    - **QuestViva error semantics**: runtime errors (unknown variable/function,
      foreach over non-list, bad index, the `error` command...) now THROW;
      `run_script` is the per-script-body boundary (RunScriptAsync's catch) that
      logs to `world.errors` AND prints "Error running script: ..." once to the
      player. After 20 errors the session is declared wedged and finished
      (MaxScriptErrors/scriptErrorsFatal); a 200-deep script recursion cap
      matches MaxScriptExecutionDepth. Error messages carry the innermost
      executing function ("... (in UpdatePlayerUI)") via a call-frame stack.
    - Corpus smoke driver: scratch `boot_smoke.cc` (loads + boots every game in
      `~/Downloads/Quest 5 games`, reports load/boot errors) — worth wiring into
      test/ as a permanent gate once a corpus path convention is settled.
    - The one remaining boot failure, **spondre**, is two things: (a) QuestViva
      itself errors on its boot (`game.pov.longtermtopics` with pov unset —
      verified against the oracle, same abort point), and (b) **lists holding
      only strings**: spondre builds lists of *dictionaries* and indexes them
      (`match["score"]`), which needs QuestList-style typed lists (see below).
  - **M3 typed lists landed** (2026-07-16): list entries are now full `Value`s
    behind the shared backing (QuestList<object>), completing the dict-values
    change. Loader materialises typed entries (`<value type="int">` etc. in a
    `type="list"` attr; String entries for stringlists, ObjectRef for
    objectlists); `list add` stores the boxed value verbatim (a list can hold
    dictionaries — spondre's `match["score"]`); indexing/`ListItem`/foreach
    hand back the typed entry; `in`/`ListContains`/`list remove`/the QuestList
    operators compare via `values_equal`, with collections comparing by
    REFERENCE identity (shared-backing pointer), matching .NET. Verified
    against QuestViva source while porting: `list remove` takes the FIRST
    occurrence only (QuestList.Remove → List<T>.Remove; ours removed all),
    `ListExclude` filters ALL occurrences and also accepts a list to exclude
    (both fixed). Container TypeOf stays "stringlist"/"objectlist" — QuestViva
    has a distinct "list" name for QuestList<object>, still TODO if a game
    branches on it. Tests: `test/aslx_runtime_test.cc:test_typed_lists` +
    typed-`<value>` loader checks (fixture `hello.aslx`); `make check` green,
    clean under ASan/UBSan. **spondre now aborts at QuestViva's own abort
    point** (foreach over `game.pov.longtermtopics` with pov unset — the
    genuine game bug the oracle hits), with the rest of the corpus still
    49/50-boot-0-error and 0 load failures.
  - **M3 listextend merge-on-read landed** (2026-07-16, with typed lists):
    `resolve_field` now collects the whole field chain -- the base value is the
    first NON-extend field, and every `listextend` field anywhere in the chain
    is an extension. Reads merge base-entries-first then extensions
    least-derived to most-derived (QuestViva Fields.GetMergedResult /
    QuestList.MergeLists accumulate parent-first), rebuilt on every read into a
    stable per-(element,attr) slot. Before this, a listextend field silently
    SHADOWED the inherited base. Unit-tested in `test_new_builtins` (fixture
    `runtime.aslx`: container base + openable extension + own extension);
    corpus boot output byte-identical (displayverbs feed UI verb menus).
  - **M4 input model landed (engine side)** (2026-07-16): the four prompt
    script commands (`get input`, `show menu`, `ask`, real pending `wait`) plus
    the host-facing entry points, ported from QuestViva's fire-and-forget
    pending-callback model rather than the nested-glk_select plan (§3 has the
    details). Highlights:
    - `Interp::send_command()` ports `HandleCommandAsyncInternal`: a pending
      `get input` consumes the line (command override, `result` param bound),
      otherwise pre-v520 echo + Core `HandleCommand` + pre-v580 `FinishTurn`.
    - Prompts are fire-and-forget (QuestViva ExecuteAsync): the callback is
      registered, the enclosing script keeps running, the host resolves later
      via `set_menu_response(key|null)` / `set_question_response(bool)` /
      `finish_wait()`. `pending_menu()` (MenuData: caption + ordered
      key→display options + allow_cancel), `pending_question()` (caption; the
      engine does NOT print it — ShowQuestion is host UI), `pending_wait()`,
      `command_override()` tell the host what the prompt is waiting for.
      A new same-kind prompt cancels the old one (BeginPrompt TrySetCanceled).
    - **`on ready` semantics corrected to WorldModel.AddOnReady**: it runs
      IMMEDIATELY (inline) unless a prompt callback is outstanding
      (`_pendingCallbackCount`), in which case it queues; resolving the prompt
      flushes the queue FIFO (EndPendingCallbackAsync). Our old queue-always +
      manual-drain model ran callbacks too late / menus'd have drained early.
    - `msg` caption/echo printing factored into `print_via_core`
      (WorldModel.PrintAsync: OutputText on v540+, failures reported and
      swallowed); menu responses echo " - <display text>".
    - **Core disambiguation needed NO engine prompt**: Core's `ShowMenu` is
      pure ASLX (numbered options via `msg`, callback in `game.menucallback`,
      answer routed by `HandleCommand` → `HandleMenuTextResponse`), so two
      same-alias objects + `take hat` + `1` works end-to-end through
      send_command. Only missing primitive was `IsInt` (+ `IsDouble`), added.
    - Tests: `test/aslx_runtime_test.cc:test_input_model` (fixture
      `command.aslx` gained two same-alias hats + type/pick/query/nap
      commands); `make check` green, ASan/UBSan clean; corpus still
      49/50-boot-0-error, boot output byte-identical except The Acreage, whose
      `wait`-gated intro sections now emerge in the correct authored order.
  - **Verb elements + change scripts + child sort order** (2026-07-16, the
    replay-driven batch — each found by chasing a golden diff):
    - **`<verb>` elements are commands** (QuestViva loads them as
      ElementType.Command): `AllCommands()` now returns them (ScopeCommands
      needs that), they inherit the `defaultverb` type (generic
      property-dispatch script, separator, multi-object defaults; inserted
      above the implicit defaultcommand), and the `<implied element="command">`
      declarations apply to them — which routes a verb's nested `<pattern>` to
      the simplepattern loader, whose isverb branch does
      `ConvertVerbSimplePattern(pattern, separator)` (deferred to finish_load
      so the type-provided separator "with; using" resolves; also sets
      displayverb = first verb, `#object#` stripped). A verb's `pattern=`
      ATTRIBUTE stays raw like a command's (CommandLoader), but `template=`
      defers like the nested form (LoadPattern is virtual). This fixed every
      "wear X"/"break X" → "I don't understand".
    - **`changed<attr>` + SortIndex** — see §2 Change notification.
  - Still open for M3/M4: `request (Wait)`/`request (Pause)`
    for pre-v540 games (truly blocking mid-script in QuestViva, needs a
    blocking host hook), game-local
    libraries bundled *inside* a `.quest` zip, and locked
    inherited collections (mutating a type-provided list without assigning it
    first throws "Cannot modify the contents of this list as it is defined by
    an inherited type..." in QuestViva; we copy-on-write silently). Timers
    landed (evening status); the error-cascade topology landed (night
    status, 16/17 golden-exact). Milestone 5 remains.

## 0. Scope and reality check

Quest 5 shares nothing with Quest 4 except lineage. The game format is XML
(`.aslx`, or `.quest` = a zip containing `game.aslx` plus resources), the world
model is a typed attribute/element system, scripts are a different language,
and expressions are .NET-flavoured (originally evaluated by FLEE). Almost all
game *logic* (parser, verbs, scopes, turn handling) is not in the engine at
all — it lives in `Core.aslx` and friends, libraries written in ASLX itself
and shipped with the player.

Consequence: Quest 5 support cannot be grafted onto the existing ASL text
interpreter (`read_geas_file` → `preprocess`/`decompile` → `GeasFile`). It is
a **second engine sharing the Glk frontend**, exactly the shape of Scarier
(SCARE v4 + new a5 engine behind the `os_glk` seam). Keep the existing engine
for `.asl`/`.cas`, dispatch `.aslx`/`.quest` to the new one.

Reference source (MIT licensed, so Core libraries can be bundled):
<https://github.com/textadventures/quest>

- branch `v5` — classic Quest 5: `WorldModel/` (engine), `WorldModel/WorldModel/Core/`
  (the ASLX core libraries), `Legacy/` (Quest 1–4 converter), `IASL/` +
  `Player/` (UI seam).
- branch `main` — "Quest Viva", a cross-platform .NET port. `src/Engine/` is
  the same WorldModel; FLEE has been replaced by an NCalc-based evaluator
  (`src/Engine/Expressions/NcalcExpressionEvaluator.cs`, ~22 KB + a 25 KB
  parser). Two takeaways: the FLEE expression semantics are re-implementable
  on a small hand-written parser, and Viva runs headless on macOS — perfect
  for a ground-truth diff harness.

## 1. Loading: zip + XML + includes

- [x] Zip reader for `.quest` packages — a small self-contained in-memory
      reader over system zlib (`aslx.cc:extract_game_aslx`), pulls `game.aslx`
      out of the package. (Didn't reuse the scott MiniZip helper — it's coupled
      to scott internals.) Other package resources are left for the presentation
      milestone.
- [x] XML parser for `.aslx` via `libexpat` SAX (`aslx.cc:load_aslx_buffer`).
      ASLX quirks handled (BOM, CDATA, script bodies kept verbatim, nested
      containment, attribute type system). `<include>` refs are now resolved
      inline (see the Core-bundling item below). ASLX quirks to
      preserve:
      - root `<asl version="NNN">` (500–580 seen in the wild) — gates
        compatibility behaviour, like `asl-version` does in `set_game`
        (`geas-runner.cc:1462`);
      - attribute type system: `<attr name="x" type="int">3</attr>`,
        implicit string/boolean forms, `<value>` and CDATA;
      - script text as element bodies where newlines/indentation matter —
        do not whitespace-normalise;
      - nested `<object>` elements (containment by nesting);
      - [x] `<include ref="English.aslx"/>` resolution: game dir first, then
        bundled Core dir, then Core/Languages (QuestViva's `GetLibraryStream()`
        order — note game-adjacent wins, unlike the v5 desktop player). Resolved
        inline during the SAX parse so template precedence matches Quest's
        source-order processing; dedup + depth guard against cycles
        (`aslx.cc:Loader::resolve_include`).
- [x] Bundle the non-editor core libraries as terp resources under
      `terps/geas/quest5/aslx-core/` (`Core.aslx`, `CoreCommands`, `CoreParser`,
      `CoreOutput`, `CoreTypes`, `CoreScopes`, `CoreDescriptions`,
      `CoreFunctions`, `CoreTimers`, `CoreTurnScripts`, `CoreStatusAttributes`,
      `CoreGrid`, `CoreWearable`, `CoreCombat`, `CoreEffects`, `GamebookCore`,
      `Languages/*.aslx`). All `CoreEditor*.aslx`/`Editor*.aslx` skipped (they
      are referenced but simply not bundled, so the loader treats a missing
      `*editor*` ref as a no-op). `.template` files are an editor artifact — not
      bundled. Copied verbatim from the QuestViva checkout (MIT); refresh recipe
      in `aslx-core/README.txt`. One version is enough — Core branches on the
      game's declared `<asl version=>`. The core dir is passed to `load_file` or
      taken from `$ASLX_CORE`; app wiring will point it at the resource bundle.
- [x] Templates: `<template>`/`<dynamictemplate>`/`<verbtemplate>` elements are
      parsed and registered (`World::templates` etc.). `[Something]`
      substitution now runs at load time on script bodies and string attributes
      (`aslx.cc:Loader::replace_templates`, mirroring
      `GameLoader.GetTemplate`): later-registered templates win, unmatched
      references are left verbatim (so regex classes survive). Not applied to
      `simplepattern` attributes — a command template like `pattern="[look]"`
      resolves to a raw regex and must skip simplepattern conversion; that path
      belongs to the parser milestone. `Template()`/`DynamicTemplate()`
      built-ins handle the runtime calls. See `Templates.cs`.

## 2. WorldModel port (the new engine core)

Port of `v5:WorldModel/WorldModel/` (or `main:src/Engine/`, which is cleaner):

- [x] **Element/Fields**: elements of type object, command, function,
      turnscript, timer, walkthrough, template, delegate, type, javascript…
      each with a typed field bag (`aslx::Value` variant: string, int, double,
      bool, object ref, script, string/object list, string/object/script dict,
      null). The element id is exposed as a `name` field (QuestViva's
      `FieldDefinitions.Name` — Core keys `cmd.name`/`obj.name` off it, notably
      the RegexCache cacheID). **Lists and dictionaries are reference types**:
      their storage sits behind a `shared_ptr` so copies (function args, `y = x`,
      field reads) alias one backing and `list add`/`dictionary add` propagate,
      matching QuestList/QuestDictionary; derived-list builtins `detach()` first.
      **List entries and dictionary values are full Values** (per-entry typing),
      so one collection can mix objects, strings, numbers and dictionaries
      (CoreParser's resolvedelements; spondre's list-of-dicts). Collections
      compare by reference identity on the shared backing (.NET semantics).
      Delegate values not yet a distinct runtime type.
- [x] **Inheritance**: `<inherit name="type"/>` chains resolved own → inherited
      types most-recent-first, recursively (`Interp::resolve_field`). Each
      element also gets an implicit default type (`defaultobject`/`defaultgame`/
      …) prepended at load, and containment is exposed as a `parent` ObjectRef
      field (children derived from it, so runtime `MoveObject` is honoured).
- [x] **Change notification** (2026-07-16): `changed<attr>` scripts fire on
      script field writes (`obj.attr = v` and `set()`), resolved through
      inheritance, with `oldvalue` bound and `this` = the element —
      Element.SetFieldAsync semantics (fires on every script assignment, no
      same-value check). This is what makes `changedparent` → `OnEnterRoom`
      print room descriptions on movement (and got the POV gender right:
      "You pick it up", not "picks it up"). A runtime reparent also bumps the
      element to the end of its new parent's children
      (`Element::sort_index`, WorldModel.UpdateElementSortOrder) and
      GetDirectChildren/GetAllChildObjects enumerate in SortIndex order —
      inventory listings match the oracle's after take/drop churn.
- [x] **UndoLogger** (2026-07-16): a faithful port of QuestViva's
      `UndoLogger.cs` transaction log. Transactions are LAZY: Core's parser
      runs `start transaction (game.pov.currentcommand)` per successfully
      parsed non-`<isundo/>` command, which COMMITS the previous command's
      transaction (dropped if it logged nothing) and opens this one; nothing
      commits at end-of-turn, and boot/StartGame changes are never logged
      (actions record only while a transaction is open). Logged actions:
      own-field set/remove (with `added` deciding remove-vs-restore, old
      values kept with their original backing pointers, SetFromUndo
      semantics: no changed-scripts, no cloning), the sort_index bump of
      parent writes, element create/destroy (destroy keeps the storage alive
      and undo just re-registers the name, CreateDestroyLogEntry), list/dict
      mutations (actions hold the live shared backing, like UndoListAdd's
      IQuestList reference), timer trigger/timeelapsed writes, and
      `firsttime` flags (UndoFirstTime). `undo` = RollbackTransaction:
      commit, roll back ONE transaction (actions applied in reverse via an
      in-place reverse, quirks included), print the `UndoTurn` dynamic
      template ("Undo: take lamp") or `NothingToUndo`, and step the
      PreviousTransaction chain back — including QuestViva's genuine
      re-commit-after-undo quirk (undo, command, undo, undo rolls the same
      transaction back twice, the second time applying forward). Stacks are
      unbounded, exactly like the reference; the redo stack exists but
      nothing in play mode pops it. Tests:
      `test/aslx_runtime_test.cc:test_undo`; verified end-to-end through
      Core's parser in Dream Pieces (take/i/undo echoes "Undo: i" and
      reverts). Goldens unaffected (16/17, ASan/UBSan clean) even though
      every replayed command now runs the logger hot.
- [~] **Script commands** (~45; enumerate `v5:.../Scripts/`): DONE —
      `msg`, `if`/`else if`/`else`, `while`, `for`, `foreach`, `=` assignment
      (var and `obj.attr`), function invocation (statement position), `return`,
      comments (`ScriptFactory.CreateScript` statement splitter ported),
      `switch`/`case`/`default`, `firsttime`/`otherwise`, `do`, `invoke`,
      `create` (+ type), `destroy`, `set` (3-arg field form), `list add`/
      `list remove`, `dictionary add`/`dictionary remove`, `on ready`, `error`,
      `finish`, `undo`/`start transaction` (real, via the UndoLogger port —
      2026-07-16), `request`
      (no-op — headless UI request; its enum arg is left unevaluated), and the
      `JS.*` bridge. `msg` routes through Core's `OutputText` (v540+) so the
      `{...}` text processor runs. Also done (2026-07-16): `rundelegate`,
      `create timer`/`create turnscript`, `x => {script}` assignment, trailing
      `{script}` call argument, `this` binding in `do`/`rundelegate`,
      `picture`/`play sound`/`stop sound`/`insert` (host hooks for picture +
      sounds, 2026-07-16; headless no-op + warning without one),
      `wait` (runs callback immediately pending §3), `error` (throws, QuestViva
      semantics). Also done (2026-07-16): `get input`, `show menu`, `ask`,
      real pending `wait` (the §3 input model — fire-and-forget callbacks
      resolved by the host), and `create exit` (evening batch). There is NO
      `enable`/`disable` timer script command in QuestViva —
      EnableTimer/DisableTimer are Core ASLX and run as library code.
- [~] **Expression evaluator**: hand-written lexer + recursive-descent parser
      over `aslx::Value` (`aslx-runtime.cc`), compiled-expression cache per
      source string. DONE: `=`/`==` equality, `<>`/`!=`, `and or xor not`,
      `< > <= >=`, `+ - * / %` with int→double promotion, `+` string concat,
      the `QuestList<T>` operator overloads (`list+list` merge, `list±elem`
      append/remove-first, `elem+list` prepend, `list*list` union — Core's
      `GetScope` does `list - game.pov`), ternary `?:`, dot member access
      (`box.capacity`), `[]` list/dict indexers,
      `null`/`true`/`false` literals, dispatch to ASLX `<function>` elements and
      ~65 built-ins: type/attribute (`GetBoolean`/`GetInt`/`GetString`/
      `HasAttribute`/`TypeOf` 1- & 2-arg/`DoesInherit`), world model
      (`AllObjects`/`AllCommands`/`GetDirectChildren`/`GetAllChildObjects`/
      `Contains`/`GetAttributeNames`), lists (`NewList`/`ListCount`/`ListItem`/
      `StringListItem`/`ObjectListItem`/`ListContains`/`ListExclude`/
      `ListCombine`), dictionaries (`NewDictionary`/`NewStringDictionary`/
      `NewObjectDictionary`/`NewScriptDictionary`/`DictionaryCount`/
      `DictionaryContains`/`DictionaryItem`/`*DictionaryItem`), strings (`Split`/
      `Join`/`Left`/`Right`/`Mid`/`Instr`/`Replace`/`LCase`/`UCase`/`CapFirst`/
      `SafeXML`), templates (`Template`/`DynamicTemplate`), the regex parser
      primitives (`IsRegexMatch`/`GetMatchStrength`/`Populate`, 2- & 3-arg),
      `Eval(expr[,scope])`, the has-family (`HasScript`/`HasString`/`HasBoolean`/
      `HasInt`/`HasDouble`/`HasObject`/`HasDelegateImplementation`), world-model
      helpers (`AllExits`/`AllTurnScripts`/`GetTimer`/`GetUIOption`), the `To*`
      converters (`ToInt`/`ToDouble`/`ToString`), and numbers
      (`CInt`/`CDbl`/`Abs`/`Max`/`Min`/`GetRandomInt`/`GetRandomDouble`).
      **Operator precedence fixed**: `not` binds looser than `=`/comparison
      (own `parse_not` level), so `not obj = null` is `not (obj = null)` — Core
      depends on this everywhere. Also done (2026-07-16): multi-word
      (space-encoded) identifiers, `in`/`not in`, case-insensitive
      `True`/`False`/`Null`, NCalc `if()`/`cast()`, `GetFileURL`/
      `GetUniqueElementName`/`RunDelegateFunction`; typed (Value-holding)
      lists with reference-identity equality (2026-07-16). TODO: the remaining
      built-ins (DateTime, `FormatList` and the scope helpers) and list/dict
      literals.
- [x] **Command parsing comes free**: implemented in `CoreParser.aslx`, and it
      now runs — `HandleCommand` → `ScopeCommands` → `IsRegexMatch`/
      `GetMatchStrength` → `ResolveName`/`GetScope`/`ResolveNameFromList`/
      `CompareNames` → verb script all execute as library code. `look`/`take`/
      `examine`/`open`/`go` work end-to-end (0 errors). The four engine gaps that
      blocked it: command patterns from `pattern="[tmpl]"`/`template="verb"`
      (not just nested `<pattern>`); the `name` field (RegexCache cacheID);
      reference-type lists/dicts (caller-built match lists); per-entry dict value
      typing (object params to verb scripts). Disambiguation menus work too
      (2026-07-16, `test_input_model`): Core's ShowMenu stores
      `game.menucallback` and the numbered answer is routed by `HandleCommand`
      — all ASLX, no engine prompt involved.
- [x] **Determinism**: `GetRandomInt`/`GetRandomDouble` route through a
      xoshiro128**+SplitMix32 port of `erkyrath_random()` (`aslx::Rng`, seed
      1234, byte-identical to `terps/common_utils/randomness.c` and the oracle's
      `ErkyrathRandom.cs`). App wiring will point it at the real
      `erkyrath_random()` under `SPATTERLIGHT`; see `geas-runner.cc:1421`.

## 3. Input and timing model

Classic Quest 5's engine thread blocked mid-script on `get input` /
`show menu` / `ask` / `wait`; QuestViva (the oracle) instead made them
**fire-and-forget**: the script command registers a callback, the rest of the
enclosing script keeps running, and the next player input resolves it
(`_commandOverride` / `_menuTcs` / `_questionTcs` / `_waitTcs`), with
`_pendingCallbackCount` deferring `on ready` until the prompt resolves. We
ported that model exactly (2026-07-16) — it needs **no nested glk_select and
no second thread**: the Glk prompt loop just asks the engine what it is
waiting for and routes the next line/keypress.

- [x] `get input` → command override: the next `send_command` line binds
      `result` and runs the callback instead of the parser.
- [x] `show menu` / `ask` → `pending_menu()` (MenuData) / `pending_question()`
      exposed to the host; resolved by `set_menu_response(key|null)` /
      `set_question_response(bool)`. Core's own ShowMenu/disambiguation is
      pure ASLX via `game.menucallback` and needs none of this — numbered
      answers arrive as ordinary commands.
- [x] `wait` → `pending_wait()` + `finish_wait()` (host keypress; headless
      harnesses auto-advance like the oracle's AutoAdvance).
- [x] Glk wiring in aslxglk.cc (2026-07-16): pending menus/questions rendered
      as numbered/yes-no prompts, `wait` as a keypress event, expression
      ShowMenu via a nested `menu_provider` input loop.
- [x] Timer elements + `SetTimeout`/`SetTimerScript` → an engine Tick(secs)
      entry (TimerRunner port: `begin_timers`/`tick`/`next_timer_seconds`/
      `has_enabled_timeout`, 2026-07-16). `glk_request_timer_events` 1s
      heartbeat at prompt level in aslxglk.cc, gated on `gli_sa_delays`
      (aslx_replay mirrors the oracle's deterministic DrainTimers).
- [x] `request (Wait)` / `request (Pause, ms)` (pre-v540/v550 games only):
      genuinely blocking mid-script even in QuestViva (`DoWaitAsync`/
      `DoPauseAsync` are awaited). Ported 2026-07-17 as two blocking host hooks
      (`Interp::do_wait` / `do_pause(ms)`) mirroring the `play_sound` pattern:
      the request dispatch (aslx-runtime.cc) gates the version (Wait throws for
      v540+, Pause for v550+, QuestViva's exact messages), Wait claims the wait
      slot (resume any parked sync sound + cancel a pending `wait` callback,
      like `BeginPrompt(_waitTcs)`), Pause `int.TryParse`s its data (separate
      `_pauseTcs` slot, so it leaves a parked sound untouched), then calls the
      hook. A synchronous host BLOCKS in the hook and the tail runs inline;
      unset, both are a silent no-op — byte-identical to the oracle's immediate
      AutoAdvance since neither request prints and the tail runs before the
      turn's FinishTurn either way (so the 28/29 native goldens, incl. A Hobbit
      Trek's 6 `Pause` calls, stay byte-identical; ICM = the documented sound
      artifact). Glk side (aslxglk.cc): `do_wait_ui` blocks on a keypress /
      hyperlink click (like the reference "press any key"), `do_pause_ui` on a
      one-shot Glk timer with a keypress skip; both self-suppress in
      deterministic mode (gli_sa_delays off) so the smoke harness never blocks,
      and `do_pause_ui` resets the `g_timer_armed` heartbeat flag it overrode.
      `make check` + ASan/UBSan green; A Hobbit Trek byte-identical under ASan.

## 4. Output mapping (HTML/JS → Glk)

Quest 5 emits HTML through the IASL `PrintText` interface and drives a JS UI.

- [x] HTML-subset → Glk styles converter (aslxglk.cc render_html, 2026-07-16):
      `<b><i><u>` + span-style CSS bold/italic/underline, `<br/>`, entity
      decode, UTF-8 → uni output; fonts/colours/alignment dropped gracefully.
      (Core's output is a constrained subset; don't write a browser.)
- [x] Object/command links (`<a class="cmdlink" ...>`) → Glk hyperlinks
      (2026-07-16): `data-command` sends the command, ASLEvent onclicks call
      the function, elementmenu links fall back to "look at".
- [x] `picture` → inline buffer images (2026-07-16): render_html's `<img>`
      + the pre-540 `Interp::show_picture` hook draw zip/adjacent resources
      via win_loadimage + the new Glk 0.7.6 `glk_image_draw_scaled_ext`
      (`WidthOrig|AspectRatio`, maxwidth=$10000 — window-fitted, dynamic on
      resize). See the top status entry.
- [x] `play sound` / `stop sound` → one Glk sound channel (2026-07-16):
      `Interp::play_sound`/`stop_sound` hooks; resources registered via
      win_loadsound (win_findsound round-trip validates + frees temp files);
      synchronous play blocks on evtype_SoundNotify in the hook (keypress
      skips), which resolves the TurnSuspended semantics against a real
      host. See the top status entry.
- [x] Panes: the picture frame (SetFramePicture → JS.setPanelContents) is
      redirected INLINE (2026-07-16, the Scarier approach — drawn on change,
      per-room art above the room description). Compass/inventory/"places and
      objects"/status attributes landed 2026-07-16 (the UpdateLists port —
      see the top status entry): a quest4-objwin-style side pane with
      hyperlinked, localized sections, status right-aligned in the banner,
      room name from JS.updateLocation. Both the JS.* and the pre-JS
      `request (SetStatus/UpdateLocation/SetPanelContents)` channels route to
      the same hooks.
- [ ] `JS.*` calls: implement the handful Core itself uses (`JS.eval` from
      user games gets a one-time warning and is ignored). Games built around
      custom JavaScript UIs are explicitly out of scope — detect and warn.
      Done so far: the restart channel (2026-07-16) — Core's `restart`
      command is `JS.eval("window.location.reload();")` (older embedded
      Cores probe the desktop player's `RestartGame()` in the same eval);
      both land in the `request_restart` host hook, and aslxglk ends the
      session with `SessionEnd::Restart` (same teardown/reboot as the
      post-game menu). Hook unset (headless), JS.eval stays ignored with
      its argument unevaluated.

## 5. App/frontend integration

- [x] Dispatch in `glk_main` (2026-07-16): `aslx_is_quest5_file` sniffs —
      `PK\x03\x04` zip containing `game.aslx`, or XML whose root is `<asl` →
      `aslx_glk_main`; else the existing `read_geas_file` path. (The aslx
      runner has its own host seam, not `GeasInterface` — the interaction
      models are too different.)
- [x] New sources into the geas target of `Spatterlight.xcodeproj` (only
      aslxglk.cc compiles — it unity-includes the rest; links libz+libexpat)
      *and* the standalone `test/Makefile` (`aslxglk_smoke` vs CheapGlk).
- [x] Babel: `babel/quest5.c` claims zips containing `game.aslx` and raw
      `<asl` XML. **Metadata + cover landed (2026-07-17)**: game.aslx is
      inflated with zlib (a small central-directory zip reader +
      `inflateInit2(-MAX_WBITS)`), the `<game>...</game>` element is parsed for
      the bibliographic fields (name= → title, `<gameid>` → IFID, `<author>`,
      `<subtitle>` → headline, `<category>` → genre, `<firstpublished>`,
      `<description>`), and a synthesized iFiction record is emitted. Quest
      stores these as HTML, so a shared `clean_html` decodes entities THEN
      strips tags (double-encoded titles like `&lt;b&gt;DRACULA&lt;/b&gt;`
      resolve to `DRACULA`); the output is XML-escaped, `<br/>` preserved in
      the description. IFID from packaged games is now the real `<gameid>`
      (inflate-then-search), falling back to the whole-file MD5 exactly like
      `babel_handler`'s auto-fallback (byte-identical, verified). Cover art:
      the `<cover>` filename is extracted from the zip (stored or deflated),
      format sniffed (PNG/JPEG magic), dimensions parsed, and the bytes handed
      back verbatim (byte-identical to the packaged image). `NO_METADATA`/
      `NO_COVER` dropped; the standalone `babel` makefile link line gains
      `-lz` (the app target already links libz). Verified: all 55 corpus
      packages + raw `.aslx` produce iFiction that passes `babel -verify` AND
      `-lint`, 36 covers extract byte-exact, 0 real AddressSanitizer/UBSan
      findings across every game × {meta,cover,ifid,format}, clean under
      `-Wall -Wextra -Werror`. (The one babel self-test failure, `alan`, is a
      pre-existing lowercase-IFID mismatch, unrelated.)
- [x] `Info.plist` (2026-07-16): `quest`/`aslx` document type +
      `public.quest5` UTI; also gGameFileTypes in AppDelegate.m, the
      quest5→geas terp table, Autorestore + fileref format names.
- [x] Save/undo (2026-07-16): undo via the §2 UndoLogger port; save/restore
      via the v1 snapshot format (`aslx-state.inc`,
      `Interp::save_game`/`restore_game`) wired into aslxglk.cc — Core's
      `save` command lands in the `request (RequestSave)` host hook, RESTORE
      is a frontend metaverb with scratch-reload validation, and the
      post-game menu offers RESTORE. **Native `.quest-save` compatibility
      landed 2026-07-17** (`aslx-savenative.inc`, GameSaver port + firsttime
      baking + a loader override mode + recursive nested typed collections):
      `save_game_native`/`restore_game_native`, `restore_game` auto-detects the
      format, and the RESTORE metaverb imports a Quest/QuestViva-written save.
      Bidirectional, verified against the QuestViva oracle by
      `test/quest5-oracle/save_compat.sh` (~18 games byte-identical both ways).
      See the top status entry. Still open: Spatterlight autosave/autorestore
      integration.

## 6. Legacy dispatch note

Quest 5's own player runs Quest 1–4 games by *converting* them (`Legacy/`).
We do the opposite: `.asl`/`.cas` keep going to the existing native engine,
which is more faithful than the converter. `QCGF001` (Quest 1 compiled)
stays unsupported, as in `quest4.c`.

## 7. Testing

- [x] Corpus: 49 games + 102 walkthroughs pulled into `~/Downloads`
      (textadventures.co.uk + if-archive). See the `quest5-corpus` memo.
- [x] Ground truth: Quest Viva's `src/Engine` builds headless on macOS
      (.NET 10, `terps/geas/test/quest5-oracle/build.sh` clones + builds
      `~/questviva-oracle`). `qvh` driver emits normalised transcripts;
      RNG wired to `erkyrath_random()`. Same recipe as `test/a5_groundtruth.sh`.
- [x] Walkthrough regression scripts: `.quest` games' shipped walkthroughs
      (+ Welbourn corpus) extracted by `extract_walkthrough.py`, driven by
      `run_corpus.sh`; 17 wired (15 win). Frozen as `golden/<Game>.cmd`+`.out`.
- [~] Extend `terps/geas/test/`: aslx fixtures with `.cmd`/`.expected`
      goldens under `run_fixtures.sh`, walkthrough runner rows for real
      games, `make check` stays the gate. (Oracle goldens exist; the native
      engine still needs its own fixture harness — starts with milestone 1.)
      **Native replay driver landed** (2026-07-16): `test/aslx_replay.cc`
      (`make aslx_replay`, needs the local corpus so not in `check`) replays
      an oracle `.cmd` through the native engine with qvh's exact step
      grammar/normalisation and diffs against `quest5-oracle/golden/*.out`.
      First-light baseline was ~30–70% matching lines, 0 games finishing;
      by the end of 2026-07-16 **16 of 17 goldens replay byte-identical**
      (night status; only the documented ICM oracle artifact remains).
      DrainTimers and Wedged-vs-Finished are both mirrored.

### 7.1 More-games queue (corpus gaps)

Wiring recipe per game: copy from the staging dir `~/Downloads/More Quest 5
games` into `~/Downloads/Quest 5 games` (what `check_golden.sh` reads) + its
MANIFEST row, then override → golden pair → native `aslx_replay` byte-diff →
README row → one commit. See the `quest5-remaining-games` memo.

Staged but unwired (`~/Downloads/More Quest 5 games`):

- [ ] **Stranger** — the largest game in the corpus (~350 rooms). Blocked: a
      real-time reflex trap makes it not oracle-winnable headless. A 287-command
      opening plus the first full walkthrough are recorded in the
      `quest5-stranger-unregarded` memo.
- [ ] **Signos** (M4u, 2012, ASL 520, Quest 5.2.4515.34846; 2,311,079 bytes).
      Fetched 2026-07-22 from
      `ifarchive.org/if-archive/games/competition2012/quest/signos/Signos.quest`.
      Found via ScummVM's `engines/glk/quest/detection_tables.h`, which is
      otherwise fully covered by our goldens — it was that table's only entry we
      lacked. Verified as ScummVM's exact file: its `DT_ENTRY0` md5
      `636793562d75ee82a4ea10d3bd3c62d6` is a **5000-byte-prefix** hash and
      matches; full-file md5 is `23568d251e33f0a502f8c68cf75d35b1`. English
      despite the Spanish title. Ships **no** `<walkthrough>` element, so the
      script must be derived from source. Media-heavy (14 images, several
      mp3/wav) — expect the synchronous `play sound` ending-eater; see the
      `quest5-remaining-games` memo.
- [ ] `game.quest`, `nnnnnbhn12345ABC.quest` — placeholder/junk names; triage
      whether either is a real game before spending a slot on it.

(ScummVM's "sleepingassassin" entry shares micky's md5; it is the same file as
*El asesino durmiente*, already wired.)

## 8. Milestones

1. **Loader** ✅ (2026-07-15): unzip + XML → element tree; `dump()` debug mode.
   `aslx.hh`/`aslx.cc` + `test/aslx_loader_test.cc`; loads all 76 corpus games.
2. **Script/expression core** ✅ (2026-07-15): runs a hand-written `.aslx`
   (msg/if/while/for/foreach/`=`/functions/return) with no Core library.
   `aslx-runtime.*` + `test/aslx_runtime_test.cc`. Expression evaluator, script
   interpreter, inheritance-aware field resolution, ~40 built-ins, deterministic
   RNG. Remaining script commands / built-ins tracked in §2.
3. **Core.aslx boots + drives commands** (loader ✅ 2026-07-15; boot ✅
   2026-07-15; command driving ✅ 2026-07-16): every corpus game plus a fresh
   default game loads its full Core tree with 0 errors, `InitInterface`+
   `StartGame`+`on ready` execute with 0 errors, and real player commands
   (`look`/`take`/`inventory`/`examine`/`open`/`go`) now run end-to-end through
   Core's `HandleCommand` → scope → resolve → verb-script chain with 0 errors.
   `test/aslx_loader_test.cc:test_coreboot` (load) +
   `test/aslx_runtime_test.cc:test_coreboot_runs` (boot) +
   `:test_command_driving` (commands) + `:test_input_model` (disambiguation).
   Change scripts landed 2026-07-16; two real games replay byte-identical to
   their oracle goldens (test/aslx_replay.cc).
4. **Interaction** (input model ✅ 2026-07-16; Glk prompt loop ✅ 2026-07-16):
   get input, menus, ask, wait land engine-side with host entry points
   (`send_command` + `set_menu_response`/`set_question_response`/
   `finish_wait`, `test_input_model`) and aslxglk.cc drives them in the app.
   Undo landed 2026-07-16 (UndoLogger port, §2); save/restore landed
   2026-07-16 (v1 snapshot, §5). Milestone complete.
5. **Presentation** (HTML→styles + hyperlinks ✅ 2026-07-16; pictures + zip
   resource extraction ✅ 2026-07-16, via the new Glk 0.7.6 IMAGE2 support;
   sounds ✅ 2026-07-16, incl. synchronous-play blocking + the command-echo
   de-dup; panes ✅ 2026-07-16, the UpdateLists port + side pane / status
   banner): remaining — `<backgroundimage>`.
6. **Integration** (babel claim + Info.plist + Xcode wiring ✅ 2026-07-16;
   app-verified with Dream Pieces; babel zip metadata + cover art ✅
   2026-07-17): remaining — a corpus-wide replay gate wired into `make check`.

Size honesty: milestones 2–3 alone are on the order of the whole existing
runner (`geas-runner.cc` is 5.5 k lines) — the engine primitives are small
individually but numerous, and fidelity lives in the details of the
expression evaluator and field-resolution order. The payoff is that once
those are right, the entire game-logic layer (Core.aslx) is not our code.

Rejected alternatives, for the record: transpiling ASLX to ASL 4 (semantic
gap is unbridgeable — typed attributes, delegates, closures over script
objects); embedding Mono/.NET in Spatterlight (unshippable); reusing QuestJS
(it *converts* games and is not behaviour-faithful).
