# TODO: Quest 5 support in Geas

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
    The non-editor Core libraries are bundled under `terps/geas/aslx-core/`
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
      `terps/geas/aslx-core/` (`Core.aslx`, `CoreCommands`, `CoreParser`,
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
- [ ] `request (Wait)` / `request (Pause, ms)` (pre-v540/v550 games only):
      genuinely blocking mid-script even in QuestViva (`DoWaitAsync` is
      awaited) — needs a blocking host hook; currently a no-op.

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

## 5. App/frontend integration

- [x] Dispatch in `glk_main` (2026-07-16): `aslx_is_quest5_file` sniffs —
      `PK\x03\x04` zip containing `game.aslx`, or XML whose root is `<asl` →
      `aslx_glk_main`; else the existing `read_geas_file` path. (The aslx
      runner has its own host seam, not `GeasInterface` — the interaction
      models are too different.)
- [x] New sources into the geas target of `Spatterlight.xcodeproj` (only
      aslxglk.cc compiles — it unity-includes the rest; links libz+libexpat)
      *and* the standalone `test/Makefile` (`aslxglk_smoke` vs CheapGlk).
- [~] Babel: `babel/quest5.c` (2026-07-16) claims zips containing `game.aslx`
      and raw `<asl` XML; raw-XML `<gameid>` GUID is returned as the IFID.
      TODO: zip metadata needs a zlib inflate of game.aslx — IFID from
      packaged games, `gamename`/`author` ifiction, cover art; then drop
      `NO_METADATA`/`NO_COVER`.
- [x] `Info.plist` (2026-07-16): `quest`/`aslx` document type +
      `public.quest5` UTI; also gGameFileTypes in AppDelegate.m, the
      quest5→geas terp table, Autorestore + fileref format names.
- [x] Save/undo (2026-07-16): undo via the §2 UndoLogger port; save/restore
      via the v1 snapshot format (`aslx-state.inc`,
      `Interp::save_game`/`restore_game`) wired into aslxglk.cc — Core's
      `save` command lands in the `request (RequestSave)` host hook, RESTORE
      is a frontend metaverb with scratch-reload validation, and the
      post-game menu offers RESTORE. See the top status entry. Still open:
      Spatterlight autosave/autorestore integration, and native
      `.quest-save` compatibility (Quest serialises the whole world back to
      ASLX, `GameSaver.cs`) as a later, optional milestone — the SCARE
      `.tas` work proved two-way save compat is doable.

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
   app-verified with Dream Pieces): remaining — babel zip metadata/cover,
   a corpus-wide replay gate wired into `make check`.

Size honesty: milestones 2–3 alone are on the order of the whole existing
runner (`geas-runner.cc` is 5.5 k lines) — the engine primitives are small
individually but numerous, and fidelity lives in the details of the
expression evaluator and field-resolution order. The payoff is that once
those are right, the entire game-logic layer (Core.aslx) is not our code.

Rejected alternatives, for the record: transpiling ASLX to ASL 4 (semantic
gap is unbridgeable — typed attributes, delegates, closures over script
objects); embedding Mono/.NET in Spatterlight (unshippable); reusing QuestJS
(it *converts* games and is not behaviour-faithful).
