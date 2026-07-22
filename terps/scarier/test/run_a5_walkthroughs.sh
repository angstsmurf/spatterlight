#!/bin/sh
# Drive every ADRIFT-5 walkthrough regression script through the ground-truth
# differential (Scarier a5run_dump  vs  FrankenDrift reference) and summarise.
#
# Each test/<Game>_walkthrough.txt is paired with its game file below.  A game
# is SKIPped when its (uncommittable) .blorb/.taf is absent.  For each present
# game we run test/a5_groundtruth.sh in TWO RNG modes and count the diff hunks
# in each, comparing against a per-mode baseline budget:
#
#   vanilla   FrankenDrift's stock System.Random (what the real ADRIFT Runner and
#             upstream FD use).  Answers "does Scarier match a faithful FD running
#             as shipped?"  RAND-selected text (combat rolls, OneOf picks, random
#             teleports) can't line up here, so such games keep an RNG residual.
#   xoshiro   FD patched (FD_RNG=xoshiro) to draw Scarier's xoshiro128** stream,
#             so RAND text aligns and the diff becomes a full every-line check.
#             A xoshiro count BELOW vanilla = that residual was pure RNG noise;
#             a count EQUAL to vanilla = a real RNG-independent logic bug.
#
# Tracking both is the point: it separates real divergences (same in both columns)
# from RNG-stream noise (xoshiro lower), and a draw-order regression shows up as
# the xoshiro number rising even when vanilla stays flat.  A golden
# test/<Game>_expected.txt is strict-diffed for the vanilla column (fast, no
# dotnet) when present.  STATUS combines the two modes:
#
#   MATCH      0 hunks in BOTH modes.
#   DIVERGE    at baseline in both modes (recorded bug; see A5_WALKTHROUGH_FINDINGS.md).
#   OKbetter   below baseline in some mode -- re-bless that column in the MAP.
#   FAIL       exceeded a budget in either mode (regression), or golden mismatch.
#
# Exit status: non-zero iff any game regressed in either mode.
#
# Usage:
#   test/run_a5_walkthroughs.sh [-v] [-j N] [substring]
#     -v          dump each diff to /tmp/a5wt/<Game>{,.xoshiro}.diff
#     -j N        run N games concurrently (default: hw.ncpu; 1 = serial)
#     substring   only run scripts whose name contains <substring>
#
# Env: FD_ROOT (default ~/frankendrift), FD_SEED (default 1234) -- see
#      a5_groundtruth.sh.  XOSHIRO=0 skips the (dotnet-heavy) xoshiro pass.
#      SAVERESTORE=0 skips the per-game save/restore self-check (a5run_dump
#      only; on by default, adds one extra replay per game).

set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
GT="$HERE/test/a5_groundtruth.sh"
GAMES="$HERE/test/adrift5-games"
A5RUN="$HERE/test/a5run_dump"

VERBOSE=0
BLESS=0
JOBS="${A5WT_JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
while :; do
    case "${1:-}" in
        -v) VERBOSE=1; shift ;;
        -b|--bless) BLESS=1; shift ;;   # (re)write each matched game's golden
        -j) JOBS="$2"; shift 2 ;;
        *) break ;;
    esac
done
[ "$JOBS" -ge 1 ] 2>/dev/null || JOBS=1
FILTER="${1:-}"
[ "$VERBOSE" = 1 ] && mkdir -p /tmp/a5wt

# script-basename | game file | baseline hunk budget (current known divergence;
# 0 = must stay clean).  Raise/lower a budget when you re-bless after a fix.
#
# Re-blessed after the P1 event-scheduler fix (WhenStart=0 events left
# NotYetStarted + LocationTrigger System tasks armed on Player move).  The five
# premature-death/teleport games (Axe, Spectre, Starship, DieFeuerfaust,
# RunBronwynn) now play their whole scripts instead of dying turn ~1, so their
# budgets ROSE as the rest of the transcript's downstream P2/P9 divergences
# became visible; Revenge (139->53) and Amazon (230->45) dropped sharply.  See
# A5_WALKTHROUGH_FINDINGS.md / TODO_a5_walkthrough_bugs.md (P1 marked done).
#
# P2a (exit listing "Exits are .../An exit leads ...") added to
# a5text_view_location: Jacaranda 439->433 (Grandpa exits now match too); RtC
# stays 141 (<ShowExits>False</>).  TODO P2a marked done.
#
# End-of-game text: the harness's "[GAME OVER]" stub was replaced by the engine
# emitting clsUserSession.CheckEndOfGame's real block (win/lose banner +
# EndGameText + "scored X out of a possible Y, in T turns." + restart prompt).
# Where the game ends at the same point as FD the banner now matches:
# MagneticMoon 34->32, Amazon 45->44.  Where Scarier still ends at the WRONG turn
# the richer block exposes that pre-existing timing bug instead of hiding it
# behind a 1-line stub: StarshipQuest 15->16 (plays one extra turn -> "53" vs FD
# "52"), RunBronwynn 47->49 (ends prematurely; FD plays on).
#
# P9 noref-task gate: a command whose object/character reference resolves to
# nothing now only "matches" a task that has a Must-Exist restriction of that
# reference type (a5restr_has_exist; mirrors clsTask.HasObjectExistRestriction /
# HasCharacterExistRestriction + the second-chance pass), instead of matching any
# task and surfacing the wrong (lower-priority) task's "...not sure which object
# you are trying to <verb>." message.  So e.g. `examine <nonexistent>` now
# surfaces ExamineObjects' "You see no such thing." like FD rather than
# ExCharsObj's message.  Broad improvement, no regressions: AxeOfKolt 72->64,
# Spectre 96->69, StarshipQuest 16->9, MagneticMoon 32->30, Revenge 53->49,
# DieFeuerfaust 40->26, LostChildren 383->361, RunBronwynn 49->47, Jacaranda
# 433->432.
#
# P3a pronoun resolution (clsUserSession.GrabIt + the EvaluateInput it/them/him/
# her substitution): the last-referenced object/character per pronoun class is
# tracked (a5_state_t.s_it/s_them/s_him/s_her) and a following "it"/"them"/...
# is textually replaced by that entity's full name (echoing FD's "(name)" line)
# before parsing.  Jacaranda 432->151 (the "get it"/"wear it" chain now resolves;
# the residue is downstream event/template/NPC-presence bugs, not pronouns).  No
# other game's script uses pronouns, so the rest are unchanged.
#
# P7 NPC presence (clsLocation.CharactersVisibleAtLocation): the room "X, Y and
# Z are here." list now includes characters seated on / inside furniture in the
# room, not only "At Location" ones.  a5state_new decodes "On Object"/"In Object"
# CharacterLocation into char_onobj/char_in, and the renderer uses the new
# a5state_character_at_location (IsVisibleAtLocation/BoundVisible: a char on an
# object is present wherever the object is).  MagneticMoon 30->27 (Captain
# Rosenloev, seated on Chair1 in the control room, is now listed).
#
# P2b/timer cantsee-no-tick: an out-of-scope object reference ("You can't see any
# X!") no longer ticks TurnBasedStuff.  In FrankenDrift an unresolvable reference
# leaves GetGeneralTask returning Nothing, so ProcessTurn takes the `sTaskKey Is
# Nothing` branch (clsUserSession.vb:3436) which prints the message but never
# calls TurnBasedStuff -- unlike a matched-but-failing task (noref/have_fail),
# which does tick.  Scarier had ticked on the cantsee path, drifting turn-based
# event countdowns early.  StarshipQuest 9->1 (hyperspace death now at FD's "52
# turns" exactly) and Revenge 49->27.  MagneticMoon 27->29: its timed "took too
# long" death moved 213->276 turns (FD 301), i.e. much closer -- the +2 is just
# realignment noise in the unwinnable flailing tail (both engines fail every
# command at the end); the residual 276-vs-301 gap is the separate, unfixed
# noref-vs-cantsee task-selection mismatch (Scarier resolves some commands via a
# no-ref task that ticks where FD takes the no-task cantsee path).
#
# P4 object-group property inheritance (a5_apply_group_properties): an Objects-type
# group's <Property> list is now folded into its member objects' static props at
# load (clsItem htblInheritedProperties).  Amazon's BuyableItems group confers
# StaticOrDynamic=Dynamic (-> the for-sale items resolve to the trading post and
# become seen) + BuyableItem (-> the buy task's Must-HaveProperty passes), so
# `buy ammo/provisions/matches/torch` now purchase instead of "(objects must be
# seen)".  TreasureHuntInTheAmazon 44->41 (residual = P2b Date/Time + "You move"
# status lines, a provisions completion-message line-join, title centring, and a
# downstream darkness divergence).  No other game regressed.
#
# P9b noref-vs-cantsee precedence (a5run_input): a no-reference (Must-Exist
# sNoRefTask) match no longer preempts a cantsee/ambiguity.  In FD a cantsee or
# "which?" ambiguity is a *first-pass* GetGeneralTask result (the reference
# name-matched >1 objects -> sAmbTask), while the exist-restriction sNoRefTask is
# found only in the *second-chance* pass, which runs solely when sAmbTask Is
# Nothing (clsUserSession.vb:5965 recursion guard).  Scarier had collapsed both
# into one scan and checked have_noref before have_amb, so e.g. "get flashlight
# from backpack" -- where GetObjectC (%objects%) swallows the whole "flashlight
# from backpack" as one unmatched (Must-Exist) reference but TakeObjectsFromOthers
# (%objects% from %object2%) name-resolves "flashlight" to several unseen objects
# (cantsee) -- showed the noref's "I'm not sure which object you are trying to
# get." and TICKED, instead of FD's no-tick "You can't see any flashlights!".
# Reordering have_amb (cantsee/ambig) ahead of have_noref fixes it: MagneticMoon
# 29->14, AxeOfKolt 64->56, DieFeuerfaust 26->19, Revenge 27->23, Spectre 69->68,
# RunBronwynn 47->46, LostChildren 361->359, StoneOfWisdom 8->5.  No regressions.
# (Residual MagneticMoon "cut glass with laser" noref-vs-cantsee is a SEPARATE
# reference-resolution bug: object2 "laser" fails to name-match in that state, so
# CutObjectW yields noref not cantsee -- no cantsee for the reorder to prefer.)
#
# End-of-game banner pSpace (emit_endgame): FrankenDrift's CheckEndOfGame Displays
# the win/lose banner through pSpace, so a paragraph break always separates it
# from the turn's preceding output.  A game that ends via a turn-based event
# (StarshipQuest's hyperspace death, MagneticMoon's "took too long" timer) emits
# its death text with a single trailing newline, so Scarier's banner abutted it;
# now emit_endgame tops the trailing newlines up to a blank line.  StarshipQuest
# 1->0 (now golden), AxeOfKolt 56->54, MagneticMoon 14->13.  RunBronwynn 46->47
# and Amazon 41->42 each rose by one line-shift artifact: both emit the banner at
# the WRONG point (RunBronwynn ends prematurely, Amazon misses the P2b Date/Time
# lines and never reaches the real win), where FD has no banner at all, so the
# extra blank just splits one hunk -- faithful change, re-blessed +1.
#
# Blank CharHereDesc suppression (char_here_desc, a5text.cpp): a character whose
# CharHereDesc property is *present but blank* is now omitted from the room's "X
# is here." present-list, mirroring FD's clsLocation.ViewLocation (HasProperty ==
# ContainsKey, so a blank value overrides the default and IsHereDesc returns "").
# Scarier had gated on the property having a <Value> node, so a value-less
# CharHereDesc fell through to the default "<Name> is here." (e.g. Revenge's
# Customs Official, whose room prose already says "A customs official stands
# here.").  Broad drop, no regressions: SpectreOfCastleCoris 68->44, Revenge
# 23->8, Amazon 42->34.
#
# P3b name_match backtracking (a5run.cpp): the noun matcher mirrors FD's
# `(article )?(prefix_i )?...(name)` regex, but consumed prefix words GREEDILY
# without backtracking.  When a prefix word also equals a name alias -- the ID
# pass (Revenge) has prefix "ID" and name "id" -- "examine id" consumed "id" as
# the prefix and left nothing for the name, so it failed ("You see no such
# thing.") where FD's regex backtracks and matches "id" as the noun.  name_match
# now backtracks (name_match_prefix tries skip-vs-consume for each prefix word,
# and both article branches).  Strictly a superset of the old matches (= the
# regex), so more conformant, no regressions: AxeOfKolt 54->50, MagneticMoon
# 13->10, Revenge 8->7, RunBronwynn 47->45, JacarandaJim 151->147.
#
# CorrectCommand bracket normalisation (a5_correct_command, a5model.cpp): a
# faithful port of clsUserSession.CorrectCommand/ProcessBlock, applied to every
# task command at load (FD runs it at game-start init).  It rewrites an optional
# `{x} y` group into `{x }y` (and `{a/b}` into `{ [a/b]}`), moving the space
# adjacent to the optional group INSIDE it so that space becomes optional.  This
# is what makes a bare "look around" match `[look] [around] {me/...}` and -- more
# importantly -- bare-direction movement match `{[go] {to {the}}} %direction%`
# (e.g. "N").  Scarier had been faking this with a single ")? " -> ")? ?"
# relaxation in a5parse.cpp convert_to_re; that hack is REMOVED now (FD's
# ConvertToRE has no such relaxation), so the matcher mirrors FD verbatim and the
# two no longer double-relax.  AxeOfKolt 50->42, SpectreOfCastleCoris stays 34,
# Revenge 7->5, LostChildren 359->354, StoneOfWisdom 5->3; StarshipQuest still a
# clean golden.  TreasureHuntInTheAmazon 34->65 and JacarandaJim 147->450 ROSE:
# without CorrectCommand those games' bare-direction moves silently failed
# ("didn't understand"), so Scarier was stuck near the start; now it traverses
# the whole game and the transcript exposes the pre-existing DARKNESS bug
# (Scarier prints full room descriptions where FD prints "It is too dark to make
# anything out clearly.") plus, for Amazon, the P2b Date/Time line placement --
# the same "now-playable, downstream bugs visible" budget rise as the P1 fix.
#
# Re-blessed after the malformed-bracket / specific-override-completion fixes
# (2026-06-29): (1) a malformed BracketSequence such as Anno's "##A#" now FAILS
# the restriction block (frankendrift's EvaluateRestrictionBlock falls off the
# end returning default False) instead of passing, so the over-specific
# "OpeningClo2" task no longer fires and the general open task runs; (2)
# MoveObject now honours the "Everything*" source variants (EverythingInGroup,
# EverythingHeldBy, ...) so the closet's clothes are moved in and listed; (3) a
# Specific *override* sub-task is now marked Completed when it runs (and skipped
# when already complete + non-repeatable), so "<task> Must BeComplete"
# restrictions (Anno's PullHookIn -> PushingBri secret passage) resolve.
# anno1700 127->98, SpectreOfCastleCoris 34->16, DieFeuerfaust 15->14,
# GrandpasRanch 45->39.  JacarandaJim 442->446 is RNG/event-timing realignment
# noise (its rain + %AlanRemarks% RAND draws shift with the corrected event
# timing; it has no malformed bracket sequences, so the bracket fix doesn't
# touch it) -- still the known RAND-divergence wall.
#
# (2026-06-29) Nested-override fix: run_general's override-children loop was
# extracted into a recursive execute_task_with_overrides() so a Specific override
# resolves its OWN nested children (FD's AttemptToExecuteTask recursion), plus a
# per-index ReferencedObjectN ref lookup (collect_refs/ref_info_for_name) and a
# bind_reference fix so %object2% no longer clobbers the bare ReferencedObject.
# Anno's "get trunk" now shows the "heavy as sin" trunk override (GetAWooden ->
# TakeObjectFromLocation -> TakeObjects); JacarandaJim 446->438.
#
# (2026-06-29) SetTasks-Execute override path NOW LIVE + cannon puzzle ported:
# the SetTasks Execute handler dispatches through execute_task_with_overrides, so
# Anno "get powder" fires PutSomeDry (the "skull" message, gunpowder -> held
# skull).  The downstream "pour powder in cannon" / fuse chain gates on
# `Player Must BeHoldingObject Gunpowder1`, which now resolves transitively:
# BeHoldingObject (a5restr char_holds_object) recurses through held
# containers/supporters like FD's clsCharacter.IsHoldingObject, so gunpowder
# inside the held skull still counts as held.  Anno stays at 67 (powder message
# fixed, cannon puzzle plays through); GrandpasRanch 39->35.
#
# (2026-06-29) Look-task actions + Text-type Specific overrides:
#  - `SetTasks Execute Look` now runs the Look task's own <Actions> after the room
#    view (FD's ExecuteTask(Look) is a full AttemptToExecuteTask), so Grandpa's
#    `vnl_NameTyped` -> `Execute Look` -> `Execute vnl_TutorialSt` tutorial lines
#    fire.  No-op for every other game (only Grandpa's Look carries actions).
#  - Text-type Specific overrides now match: ref_info_for_name resolves a `%text%`
#    reference to its bound ReferencedText, and refs_match_specifics compares a
#    Text specific case-insensitively (FD RefsMatchSpecifics .ToLower).  Grandpa's
#    `say %text%` -> `vnl_SayKennedy` ("kennedy") cherries puzzle now works.
#    GrandpasRanch 35->28; JacarandaJim 438->124 (its conversation/keyword
#    overrides are text-keyed -- big systematic win).  No regressions.
#
# (2026-06-29) SetTasks-Execute literal-key arg + character `.Parent`:
#  - eval_arg_to_key ran a bare entity-key arg (no `%`, e.g. `vnl_Dial`) through
#    a5text_process, whose display pipeline auto-capitalised the first letter
#    ("vnl_Dial" -> "Vnl_Dial") and corrupted the case-sensitive key, so
#    `Execute SetObjectTo (vnl_Dial)` failed `ReferencedObject Must Exist`.  Now a
#    no-`%` arg is passed through verbatim.  Unblocked Grandpa's whole endgame
#    (turn dial -> SetObjectTo override -> dial prompt -> box -> map -> Buster ->
#    dig -> WIN): GrandpasRanch 28->12.
#  - `%Player%.Parent` (a5expr) returned "" for a character; clsCharacter.Parent
#    is Location.Key, so it now resolves the on/in object (char_onobj) else the
#    location -- Grandpa's "(getting off the chair first)".  GrandpasRanch 12->11.
#
# (2026-06-29) StoneOfWisdom: real winning walkthrough.  The old script was a
# reconstruction of the bundled ROT13 hint sheet with NO navigation -- the avatar
# never left the first room and every command failed ("You can't see any troll!"),
# so it tested nothing.  Replaced with a genuine 137-turn linear playthrough that
# wins 50/50 (*** You have won ***) in FrankenDrift.  Budget 2->6: Scarier now
# DIVERGES because of a real (catalogued, unfixed) troll-combat bug -- after the
# combat ring knocks the troll unconscious, Scarier ALSO fires the troll's
# "you-didn't-act -> die" death event on the same turn, so the player is killed at
# turn 10 and never reaches the rest of the game (FD's knockout cancels that
# event).  Of the 6 hunks, 3 are inherent RAND troll-taunt lines (default-RNG mode)
# and the rest are that death + its loss-vs-137-turn-win cascade.  See
# TODO_a5_walkthrough_bugs.md ("Troll knockout death") / A5_WALKTHROUGH_FINDINGS.md.
#
# (2026-06-30) Bare-`all` narrowing done properly + MoveCharacter bulk source forms.
#  - resolve_plural already narrowed a bare `all`/list to the restriction-passing
#    items, but resolve_refine's final whole-set re-check then evaluated the task's
#    restrictions with ReferencedObjects bound to the raw "k1|k2|..." pipe (which
#    a5restr could not resolve -> Must Exist=0 -> the whole multi-object command
#    failed).  Two faithful fixes: (a) a5restr resolve_key now resolves a piped
#    ReferencedObjects binding to its FIRST item (FD's GetReference returns Items(0);
#    per-item narrowing already happened in RefineMatchingPossibilites); (b) a5text
#    %TheObjects[%objects%]% / bare %objects% render a piped key list as FD's "the a,
#    the b and the c" definite article list (htblObjects.List).  StoneOfWisdom 44->5
#    (`put all in backpack` -> coins, then `climb tree`); RunBronwynn stays 9.
#  - MoveCharacter only handled a single `Character` source; ported FD's bulk
#    MoveCharacterWho set (EveryoneAtLocation/InGroup/Inside/On/WithProperty) so the
#    Jacaranda wand-teleport `EveryoneAtLocation Location33 ToLocation Location34`
#    actually moves the player -> the colour-button room is now in scope, the buttons
#    push, navigation reaches the quarry, and the buzzard-nest event fires (inventory
#    aligns to within one item).
#  - JacarandaJim 109->271 (re-bless): both fixes are strictly more faithful but make
#    the heavily-RNG game play far deeper, surfacing pre-existing divergences -- 247
#    of 271 hunks are AFTER the champagne event (Task67 `MoveCharacter %Player%
#    ToLocationGroup Group7`, a *random* room pick), whose whole-game tail cascade is
#    irreducible under vanilla RNG (Scarier and FD draw different Group7 rooms; would
#    align under FD_RNG=xoshiro).  The other ~24 are the catalogued Alan-follower /
#    rain-timing RAND residual.  No rendering bug (verified: no pipe leaks, correct
#    "a, b and c" lists).  Remaining JJ blockers: MoveCharacter ToLocationGroup +
#    %DisplayLocation%/%LocationOf% functions (champagne), the Alan follower, rain RNG.
# (2026-06-30) StoneOfWisdom 5->2: the two non-RNG residuals are gone.
#  - Window-counter off-by-one (`x window`): ExamineObjects is AggregateOutput, so FD
#    defers its %object%.Description function replacement to final Display -- AFTER the
#    AfterTextAndActions child ExamineAnO increments Windowcoun.  Scarier rendered the
#    parent text eagerly, before the child's action, so the "movement"/"creatures"
#    segments lagged one turn.  Fixed in execute_task_with_overrides: an aggregate
#    parent with no actions of its own and passing After-children now runs the children
#    first, then renders the parent text (spliced ahead of the children's output).
#  - <# OneOf(...) #> always returned the first operand (a5sexpr stub): SoW's troll
#    "step forward" line is a 4-way OneOf.  Wired a5sexpr_rng_hook = a5rand_between and
#    implemented oneof/either/rand/urand per FD's clsVariable.SetToExpression semantics
#    (oneof index = Random(n-1)).  StoneOfWisdom is now a perfect MATCH (0) under
#    FD_RNG=xoshiro; the vanilla residual 2 is only the troll OneOf drawing a different
#    element from the unaligned System.Random stream (the documented RNG caveat).
# (2026-06-30) JacarandaJim 271|261 -> 101|5 (the heavily-RNG game now byte-aligns
# under xoshiro).  Five fixes made the RNG draw stream IDENTICAL to FD's (187==187
# draws): (1) ev_init drew Length-then-StartDelay where FD draws StartDelay-then-
# Length (VB evaluates `StartDelay.Value + Length.Value` left-to-right) -- swapped,
# fixing every random event timer; (2) MoveCharacter lacked `ToSameLocationAs`
# (Alan's follow task brings him on a teleport -> "Alan is with me") and
# `ToLocationGroup` (the champagne `MoveCharacter %Player% ToLocationGroup Group7`
# random-room pick, a Group7-of-21 RandomKey draw -- the single missing draw that
# desynced the whole stream); (3) `Event.Position`/`Event.Length` OO-properties were
# unresolved (a5expr_event_prop_hook + item_kind 'e' + a5text_eval_expression now
# runs the bare-key ReplaceOO pass) -- the rain "still raining" message is gated on
# `Event12.Position > 0`; (4) single-arg `RAND(8)` returned the literal instead of
# `Random(0,8)` (no draw) -- the rain message's `<# If(RAND(8)=1,...) #>` never drew;
# (5) `%LocationOf%`/`%DisplayLocation%` text functions (champagne teleport prose)
# + nested-override specifics expansion (`give tape to pirate`: Task49[tape] overrides
# Task48[any,pirate] -> effective specifics [tape,pirate]).  Vanilla 101 = the
# documented System.Random caveat (its rain/Alan-remark text can't align to xoshiro);
# the residual 5 under xoshiro are non-RNG: a pSpace gap, a darkness-group timing, a
# climb message, an "I move east" echo.  Only JJ and Amazon moved.
# (2026-06-30) TreasureHuntInTheAmazon 6 -> 54 (re-bless): the old 6 was an EARLY-DEATH
# artifact -- `MoveCharacter Jaguar ToSameLocationAs %Player%` was a no-op, so the
# jaguar never reached the player, `shoot jaguar` found no target, and the player
# DIED at turn ~120 (the 355-line transcript's missing tail counted as one diff
# block = 6 hunks).  The JJ ToSameLocationAs fix moves the jaguar -> the player shoots
# it and plays the FULL game like FD (823->after the held-recursion fix below).  Then
# `BeHeldByCharacter` was non-recursive, so the silver key INSIDE the carried jewelry
# box wasn't "held" and `unlock door` failed (FD's clsCharacter.IsHoldingObject
# recurses through held containers); fixed in a5restr (is_holding_object) -> the door
# unlocks, the cave is traversed, 115->54.  The residual 54 is 53 P2b Date/Time-line
# placement hunks (the `Beforeplay`/`Beforeplay1` BeforeTextAndActions overrides that
# `SetTasks Execute ts_tasCheckTime` after each move/look -- FD shows it 99x, Scarier
# 87x) + the title's leading spaces.  Tracked as the open P2b residual.
# (2026-07-02) CallOfTheShaman -> full MATCH (0|0, golden).  Full 265-point
# win ("*** CONGRATULATIONS! ***") derived by play from the bundled ROT1 hint
# sheet.  The 3 residual hunks (identical in both RNG modes, all in the endgame
# banner/credits) were all fixed:
#   (1) `%turns%`/`%Turns%` built-in added to eval_function (a5text.cpp), matching
#       FD's ReplaceIgnoreCase(sText,"%turns%",...) (Global.vb:1763) -- ci_eq folds
#       case so the capital alias resolves too.  Value = st->turns-1 (FD bumps
#       Adventure.Turns *after* Process() returns, so a task's own output sees the
#       pre-command count), giving "151".
#   (2)+(3) credits URLs "Https://.../Http://..." were wrongly leading-capped: the
#       a5text_display_alr Display-boundary re-cap runs on PLAIN text, where the
#       credits' `<del>` tag before the URL has been stripped, so `\n`+"https"
#       looked like a line start (FD caps the MARKED-UP text, where the tag's '>'
#       blocks it).  Fixed by (a) capping the room view on its still-marked-up
#       buffer in view_location_impl (so genuine `\n`-start NPC "is here" lines
#       still cap) and (b) suppressing the boundary re-cap's `^`/`\n` line-start
#       rules (auto_capitalise_ex allow_line_start=0), keeping only real sentence
#       punctuation there.  See A5_WALKTHROUGH_FINDINGS.md.
#
# (2026-07-09) JustAnotherFairyTale (Finn Rosenløv, IFComp 2020) wired at MATCH
# 0|0, golden-backed.  Full 131-command win (`give compass to wizard` ->
# "*** You have won ***"); no score (pure win/lose).  Route transcribed from the
# author's official hint sheet and verified move-by-move; Scarier is byte-
# identical to FrankenDrift in BOTH RNG modes with NO engine changes -- the game
# was already fully conformant.  Puzzle spine: painting-compass (wizard link),
# Polish-troll "poles" raft, magic-rope bank-roof gold heist, compass-guided
# hedge maze (NW/N), Father Time clock repair (ask his NAME topic sets DragonName
# so `call puff` isn't "cheating"), willow/wind "Puff the Magic Dragon" summon,
# sleeping-potion dungeon guard.
#
# (2026-07-10) TheMysteriousSpaceship (Kenneth Pedersen, Release 1) wired at
# MATCH 0|0 first try, golden-backed, NO engine changes.  Full 66-command win
# (`press yellow button` -> "*** You have won ***"); no score (pure win/lose).
# Blind-derived from the game XML.  Puzzle spine: the vampire Vlad Nosferatu
# was Muslim (Koran dedication), so the crucifix is a red herring -- rotate
# the ship 180deg (6 x right button) and open the shutters so Karpath's
# CRESCENT MOON shows, hammer a bookcase pole into a stake and knife-whittle
# it pointy, trap + kill a rat so the blood lures the vampire, flee forward
# into the moonlit cockpit (he paralyzes on entry), stake him, take his access
# card, swipe into the engine room, replace the blown fuse, press yellow.
# Fully deterministic: no RAND draws (3-turn mousetrap event, scripted chase).
#
# (2026-07-10) TheTartarusProject (Kenneth Pedersen, Release 1 "competition
# release") wired at MATCH 0|0 first try, golden-backed.  Full 103-command win
# (no score, pure win/lose); blind-derived from the game XML.  Needed ONE engine
# fix: the per-turn route-restriction memo (clsCharacter.dictHasRouteCache) --
# `e` through the security door passes PlayerMovement's HaveRouteInDirection
# while the door is open, the BeforeActionsOnly override s_GoToTheEas slams it
# shut, and FD still moves the player on the turn's cached first verdict.
# Puzzle spine: phone -> cafe (your DOUBLE from parallel world B) -> Fu-Tech at
# night, card out the window to smuggle him in, hide in cubicle from the guard,
# laptop `tartarus16` + USB core-code update, type B + activate = double home;
# type C + push trolley + sit in chair = YOU to world C, where you are DEAD --
# Michael Peters gets the USB proof, you wake committed in the psychiatric
# hospital: coin from Linda's room (she couch-sits when you linger in the living
# room), buy Peter's cigarette, gift it to Tom (he leaves for the garden), break
# Tom's old window (door closed) -> Michael drives you to Fu-Tech C.  Endgame
# elevator dance vs Jack: ride to the basement and WAIT ~7 turns -- the
# JackAndThe event (10 turns) has him call the elevator; empty car ferries up,
# he rides down to the counter; then call the car back, ride to 1, N = win.
# All Jack tasks' "%Player% Must BeAtLocation" clauses are COMPLETION-MESSAGE
# gates (you only see what happens where you are), not task restrictions.
#
# (2026-07-10) MurderMostFoul (David Whyld, 2023) wired at MATCH 0|0,
# golden-backed: FULL MAXIMUM-SCORE WIN "283 out of 283", all 33/33 former
# professions + 14/14 footnotes, 1364 commands.  Route = the author's own
# bundled "basic walkthrough" PDF transcribed 1:1 + 3 repairs (2nd greenhouse
# Whistle -- it RE-LOCKS; extra NW after the fake-vase give -- Pinkerton's
# cut-scene eats the move; Open panel on the lair return) + the max-score
# additions (ask Joves about the burnt letter, read Pinkerton's journal,
# paint-LAST + z for the Given3Item event, give fake vase).  The vanilla
# column pins Scarier's own golden; a direct FD-vanilla diff shows 5, all
# RAND-picked overheard-gossip lines (the System.Random caveat).  FIVE engine
# fixes fell out (whole corpus stays at baseline in both modes):
#   (1) AggregateOutput raw-template response merging (resp_add_comp): FD keys
#       AddResponse by the UNexpanded completion (OO chains resolve only at
#       Display), so the game's custom PlayerMovement1 -- MoveCharacter +
#       Execute Look + its own "%Player%.Location.Description" After message --
#       collapses into the Look response instead of printing every room TWICE.
#   (2) view_location_impl honours the ambient marking_display (FD's
#       bTestingOutput) instead of forcing retire-on-render, so the Look
#       dance's test renders no longer eat a first-visit <DisplayOnce> segment
#       (which un-pinned the view and let the movement completion re-render
#       the post-retire variant as a second copy); the real-output call sites
#       (resp_flush is_look, emit_look, game-start view) set marking=1.
#   (3) ALR-borne %variable% deferral (A5_VARDEF_MARK): FD expands ALRs only
#       inside Display, so the game's <s1>/<s2>/<s3> "Your score has increased
#       by N points to %scor%." overrides read the POST-IncVariable value;
#       Scarier's eager ALR pass now emits a sentinel resolved at the
#       Display-commit boundaries (before the LocationTrigger drain / event
#       tick / finish_turn), fixing both the off-by-one score lines and the
#       room-header SCORE/professions counters mid-turn.
#   (4) GrabIt character-descriptor matching is CASE-SENSITIVE against the
#       lowercased input (FD only .ToLower's ProperName), so "talk to talia"
#       does not retarget `her` (descriptor "Talia" never matches) and the
#       later "ask talia about her plans" echoes FD's "(Sophia)".
#   (5) run_task's Look branch renders a genuinely NON-aggregate stock Look
#       (a game that sets <Aggregate>False</Aggregate>) eagerly at Execute
#       time, matching FD's rendered-text AddResponse (inert for the corpus).
#
# Two budgets per game: VANILLA (FD's stock System.Random) | XOSHIRO (FD patched
# to draw Scarier's xoshiro128** stream, FD_RNG=xoshiro -- RAND-selected text then
# aligns, so the diff is a full every-line check).  Tracking both separates
# RNG-independent logic bugs (same count in both columns) from RNG-stream noise
# (xoshiro lower).  A xoshiro count BELOW vanilla means real RAND divergence
# collapsed once the streams aligned -- the truer conformance number.
# (2026-07-02) ThingsThatGoBumpInTheNight wired at DIVERGE 8|8, FIXED same day ->
# MATCH 0|0 (golden committed).  The winning 310-turn / max-250-point script was
# derived from the game's OWN built-in WALKTHROUGH command (type WALKTHROUGH at
# the start menu -- it prints the whole solution as a " - "-list ending "GAME
# COMPLETED"), with three built-in moves corrected for this build's scripted
# cut-scenes (see the script header comment).  Both engines now play it
# byte-identically to "*** CONGRATULATIONS! ***".  Four faithful fixes, all in
# the BeExactText / SetTasks-Execute response model (see
# TODO_a5_walkthrough_bugs.md for the full write-up):
#   (1) resolve_plural's Applicable refine now probes with an EMPTY typed text
#       (FD's fresh clsSingleItem has no sCommandReference), so `MustNot
#       BeExactText All` passes during the refine and only fails afterwards --
#       the library RemoveBeforeDrop helper then fails SILENTLY on `drop all`
#       and the scan falls through to the real DropObjects task (which was the
#       fatal every-seen-object over-expansion);
#   (2) the final resolve_plural pass iterates the REFINED item set (FD's
#       NewReferencesWorking), not the original parse;
#   (3) `SetTasks Execute <task> (%objects%)` passes a same-name reference
#       STRAIGHT THROUGH (key and typed text; FD vb:2188), and a computed arg
#       binds an empty typed text -- so the Execute'd take-chain helpers see
#       the original "all" and their BeExactText gates behave;
#   (4) an Execute'd task whose restrictions fail WITH a message now shows it
#       (FD's ExecuteTask is a full AttemptToExecuteTask) via a per-command
#       response scope implementing htblResponsesFail's text dedup and the
#       pass-cancels-fail flush rule -- fixes the dark-room `get dirt` ("It is
#       too dark to find the dirt.") without leaking cancelled fails (Grandpa's
#       flashlight "not on or inside another object!", TBN's taken grapnel).
#
# (2026-07-02) LostLabyrinthOfLazaitch wired -> initially DIVERGE 403|403.  The
# full 520-point win script is the game's OWN built-in walkthrough (Larry
# Horsfield games embed the whole solution -- type WLKTHRGH in-game; extracted
# verbatim from the cl_Walkthroug task text, annotations stripped, `o`/`b`
# start-menu handshake prepended); FrankenDrift replays the UNMODIFIED native
# commands to "*** CONGRATULATIONS! ***" (451 turns, 520 pts), so every hunk was
# a Scarier bug.  FIXED same day, 403|403 -> 8|0: (1) location seen-tracking
# (st->loc_seen, clsCharacter.HasSeenLocation) -- `Location94 MustNot
# HaveBeenSeenByCharacter Player` on the FahrenLayb teleport child always failed
# under the old always-"seen" stub, killing the "Fahren Layburn" spell and the
# whole village endgame; (2) restriction key "ReferencedObjects" now falls back
# to the singular ReferencedObject binding at lookup (FD GetReference has no
# ReferenceMatch condition for it, clsUserSession.vb:3990) -- fixes `sheath
# sword`; resolved at LOOKUP, not bound at capture, so failed match attempts
# can't leak a stale alias (that variant regressed AxeOfKolt `drop chainmail`);
# (3) FD ViewLocation name-cases CharHereDesc via the ##CHARNAME## round-trip for
# single characters too ("white stallion" -> "White Stallion"); (4) the Look task
# now runs FD's Before+AggregateOutput message dance (clsUserSession.vb:1164-1207):
# two bTestingOutput renders around its actions -- each re-drawing any <# OneOf #>
# in the room view -- response pinned to the FIRST render when they differ, else
# the aggregate raw re-renders at final Display; response slot reserved BEFORE
# actions (Grandpa's tutorial-after-view ordering).  This aligns the xoshiro draw
# stream for random room views (the riding OneOf).  Residual vanilla 8 = pure
# System.Random-vs-xoshiro OneOf picks in the riding rooms (same class as
# SixSilverBullets 18/0); xoshiro 0 = full conformance.  No vanilla golden while
# the vanilla stream diverges (RNG-bound, like JacarandaJim).
#
# (2026-07-02) TheEuripidesEnigma 11|11 -> MATCH 0|0 (golden committed).  Three
# more root causes on top of the identity-ALR hang fix (full write-up in
# TODO_a5_walkthrough_bugs.md):
#   (1) the room-view listing filters (ExplicitlyExclude / ExplicitlyList) now
#       consult the RUNTIME SetProperty override layer, not just the static
#       model -- the drone / boom box run `SetProperty <obj> ExplicitlyExclude
#       <Selected>` to drop themselves from "Also here is ..." once prose
#       conveys them (7 hunks: the whole "object in prose still auto-lists"
#       family);
#   (2) per-command pass-response TEXT dedup (htblResponsesPass keying) on the
#       direct path -- `press on` Executes cl_ToCrawler11 AND cl_ToCrawler12
#       (identical journey text), each ending in `Execute Look`, all shown once
#       (the extra cut-scene block + the doubled cliff-face description);
#   (3) a non-aggregate Before completion on the response-map path retires its
#       <DisplayOnce> segments (FD's ToString at vb:1167 runs pre-actions with
#       bTestingOutput=False) -- the crevice squeeze showed its first-time
#       paragraph on every later pass (the wording "variant" hunk).
#
# (2026-07-02) BugHuntOnMenelaus wired -- but as SCARIER-SURPASSES-FD, not an FD
# MATCH.  The winning 100/100 script is the game's own built-in WALKTHROUGH (two
# transcription fixes: Erlin enters the cottage via `In`; the final move is `n` to
# the shuttle).  Root fix: MoveCharacter ToSwitchWith / BECOME player-switching --
# previously a silent no-op in Scarier, so every marine BECOME left the viewpoint
# stuck in Captain Erlin's basement and the game was unplayable past the first
# kill.  Implemented via a dynamic current-player key (a5_state_t.player_key,
# clsAdventure.Player): ToSwitchWith retargets it when the player is involved
# (FD clsUserSession.vb), and every %Player%/player-scope resolution now reads it
# instead of the hard-coded literal "Player".  char_perspective also keys 2nd
# person on "is the current player" so a switched-in marine narrates as "you ...",
# not by name.  All 20 golden games stay byte-identical (player_key == "Player"
# until a BECOME, and no other game uses BECOME) -- zero regressions.
#   With that, Scarier plays the WHOLE game to "*** CONGRATULATIONS! *** ...the
# maximum 100 points!" (all 6 Meneltra, 69 turns).
#   (UPDATE 2026-07-02) FrankenDrift originally could NOT: Davey's 3rd-floor
# Meneltra is past a pass-gated corridor, and after taking the elevator any N/S
# move died with "Sorry, I didn't understand that command." -- this is the known
# ADRIFT Runner v5.0.35 bug that was fixed in v5.0.36 (see the game's hint thread,
# intfiction.org/t/63289).  FD reproduced it because its GetGeneralTask aborted on
# an NRE: cl_PlayerMove1's bracket sequence indexes past its loaded restriction
# list and hands PassSingleRestriction a Nothing, and restx.Copy threw.  FIXED in
# FrankenDrift (FrankenDrift.Adrift/clsUserSession.vb, PassSingleRestriction: guard
# `If restx Is Nothing Then Return False` -- a null restriction fails closed, the
# same result the old catch fell through to, without the crash that aborted task
# selection).  FD now walks the corridor and WINS 100/100 too.  The xoshiro budget
# dropped 23 -> 2 -> 1.  The `read sign` divergence (Scarier "You can't see the
# sign." vs FD "Sorry, I'm not sure which object you are trying to read.") is FIXED
# (2026-07-03): HasSeenObject is per-character in FD, so after a BECOME the new
# viewpoint must not inherit the old player's sightings -- Jones must NOT "have
# seen" the elevator-lobby sign that Davey saw.  Scarier had one global player-
# centric obj_seen/char_seen/loc_seen that leaked across a viewpoint switch; now
# these are snapshotted per character on ToSwitchWith (a5state_switch_seen), so
# `read sign` fails the ReadObjects `HaveBeenSeenByCharacter` gate like FD.  The
# remaining xoshiro 1 is the disembark blank line: FD emits a paragraph break
# between the Execute-Look room view and the cl_AqulianSpe LocationTrigger System
# task's completion message (the drain-path room-view separator), which Scarier
# space-joins -- a cosmetic mid-transcript blank line on a shared room-view path
# (see TODO_a5_walkthrough_bugs.md).  vanilla column strict-diffs Scarier's own
# winning transcript (test/BugHuntOnMenelaus_expected.txt, budget 0 = must stay a
# win).  See TODO_a5_walkthrough_wiring.md.
#
# (2026-07-02) MaroonedOnMazoomah (Larry Horsfield, 2010): the walkthrough script
# is the game's OWN built-in full solution, lifted verbatim from its FullSol task
# ("FS"/"FULL SOLUTION"/"WALKTHROUGH" dumps the command list as the task
# completion message), with a leading "c"/"b" prefix to clear the intro and
# start-options screens.  The author's shipped list does NOT win under a faithful
# engine, so THREE one-line repairs (all FD-verified) were added -- see the
# walkthrough header: (1) "put flask in bag" before the zip-wire slide (needs
# both hands free), (2) "get crystal" before "connect crystal" (must be Held By
# Player), (3) "cut pc door with laser" instead of the ambiguous "cut door".
# With those it reaches the real win ("*** CONGRATULATIONS! *** ... 135/150") and
# Scarier is byte-identical to FrankenDrift in BOTH RNG modes (0/0 MATCH), so it
# is golden-backed (MaroonedOnMazoomah_expected.txt, runs without dotnet).
#
# (2026-07-03) FinnsBigAdventure 0|1 -> 0|0: a failing event/System/walk task now
# shows its deciding restriction's <Message>, mirroring FD's AttemptToExecuteTask
# (bCalledFromEvent=True, bChildTask=False still evaluates responses -> sMessage =
# sRestrictionText -> AddResponse -> Display).  attempt_event_task_impl
# (a5run_events.cpp) had returned SILENTLY on any restriction failure, so FBA's
# cl_AtButcherS LocationTrigger -- whose "rope tied" restriction carries "As you
# walk past the butcher's stall, Paddy ... you pull on his leash to stop him ..."
# -- printed nothing where FD shows the leash message.  The message is read from
# st->restriction_text (the side effect a5restr_pass already set, == FD's
# sRestrictionText); it is NOT re-derived via a5restr_fail_message, which would
# re-evaluate the block and draw any RAND()-valued restriction a SECOND time (SSB
# TimeTrapsT `Roller Must BeEqualTo 'RAND(1,16)'`) -- that double-draw was a
# transient xoshiro 0->6 regression, fixed by reading the cached node.  The
# msg_has_output gate drops the (overwhelmingly common) messageless failing
# system task, so the corpus is otherwise unchanged.  FBA golden regenerated.
#
# (2026-07-03) Six games wired at MATCH 0|0 from OPENING-TURN SMOKE PROBES, not
# full walkthroughs: Halloween (Haven), MagorInvestigates (MI), MuseumHeist,
# October31st, TheFortressOfFear, Xanix (XXR).  [Halloween since UPGRADED to a
# blind-derived FULL WIN, MuseumHeist to a perfect-score run, and Xanix
# (2026-07-05) to a blind-derived FULL MAX-SCORE WIN 600+/600 (best ending,
# 370 commands, MATCH 0|0 both modes + the room-view ALR sealing fix) --
# see TODO_a5_walkthrough_bugs.md.]
# Their _walkthrough.txt is the
# generic 4-command probe `look / examine me / inventory / wait`; the golden
# guards intro + first-room render + basic-verb output byte-exact against FD
# (all 6 were 0-hunk in both RNG modes when added).  These are conformance
# guards for the opening turns, NOT max-score wins -- promote to a real
# walkthrough if/when one is derived.  (Tingalan, the 7th new game, is NOT wired:
# its `examine me`/array-index bugs were fixed -- see TODO_a5_walkthrough_bugs.md.)
#
# (2026-07-03) Tingalan NOW wired at 0|0 (golden-backed) after the lowercase-rand()
# engine fix resurrected its (previously dead) per-turn encounter RNG -- Roller's
# 11 rand(...) SetVariables were all evaluating to 0.  Opening is byte-exact vs FD
# in both modes (the vanilla golden diff is 0; a direct FD-diff would show 1, the
# %notallowed[RAND(1,6)]% array pick differing from FD's System.Random -- pure RNG
# noise like JacarandaJim/SSB, which the committed golden pins).  This is still the
# generic smoke probe, NOT a winning walkthrough: deeper play (movement into the
# woods) still diverges in the hunger/thirst/wound tick timing -- see the Tingalan
# derivation entry in TODO_a5_walkthrough_bugs.md.
#
# (2026-07-03) BookOfJax (Larry Horsfield) wired -- DIVERGE 55|55.  The script is
# a derived, deterministic MAXIMUM-score win ("...scoring the maximum 500 points!",
# 652 turns); FrankenDrift and Scarier BOTH play it to that win, so every hunk is a
# Scarier text divergence, not a stuck/dead run.  Wiring it surfaced two bugs:
#   (1) FIXED same day (503->55): the stock Look task's AggregateOutput
#       CompletionMessage segments were rendered WITHOUT honouring <DisplayOnce>,
#       so BoJ's first-turn "(Don't forget ... use the LUMINO spell.)" hint (a
#       DisplayOnce segment gated on "prow seen") reprinted on EVERY room view
#       where FD shows it once.  render_look_string (a5run_action.cpp) now mirrors
#       eval_desc_into / clsDescription.ToString: a shown DisplayOnce segment is
#       suppressed thereafter, and retired on real output (marking_display=1 at the
#       three real render sites; the two pre/post-action test renders stay =0).
#       Whole 34-game corpus stays byte-identical in both RNG modes -- zero
#       regressions (no other golden game's Look aggregate carries a DisplayOnce).
#   (2) FIXED (55->2): a bare object-key OO-property inside a room-description
#       <# #> expression (e.g. the potting shed's "...a cupboard which is
#       <#LCASE(cl_Door1.OpenStatus)#>.", and the farm gates/half-doors
#       cl_gate1/2, cl_door3/4/6, cl_hatch) was NOT resolved -- Scarier left the
#       literal key ("...which is cl_door1." vs FD's "...which is locked.").
#       replace_expressions (a5text.cpp) only ran expr_substitute (%ref%.Prop +
#       %func%) on each <#...#> body; the bare key never got the a5expr_replace
#       pass because protect_exprs had hidden the body from process_inner's outer
#       ReplaceOO.  It now runs a5expr_replace on the substituted body before
#       a5_eval_sexpr, mirroring a5text_eval_expression.  53 of 55 hunks were this.
#   (3) FIXED (2->1): the game's custom %Turns_Taken% counter (per-turn TurnBased
#       event cl_TurnsTaken1 -> cl_TurnsTaken2 IncVariable) over-counted by 6 (601
#       vs FD 595).  FD's post-Display NewReferences (which the event-fired task
#       iterates) is only the LAST *displayed* message's refs; its AddResponse
#       bHasOutput gate never enters an empty-output response.  On `put all in bag`
#       re-putting items already inside the bag (empty cl_PutObjInBa completions),
#       Scarier left n_ref_items=7 (the silent moves) so the event ticked +7; FD
#       left 1 (the one visible "...length of rope..." message) -> +1.  resp_flush
#       (a5run_action.cpp) now re-applies the last OUTPUT-producing entry's refs as
#       the leftover.  Amazon's `get ammo and rifle` +2 double-tick preserved (its
#       two takes share one non-empty 2-ref message).  Whole corpus unchanged.
#       The remaining 1 is the plough-message leading pSpace.
#       See A5_WALKTHROUGH_FINDINGS.md / TODO_a5_walkthrough_bugs.md.
#
# (2026-07-03) Halloween UPGRADED from the 4-command smoke probe to a
# blind-derived FULL WIN (Finn Rosenløv, DANISH; 42 commands, `dræb dracula`
# -> "*** Du har vundet ***"), 0|0 both modes, golden re-blessed.  Derived
# from the a5dump model XML alone (Tingalan/MuseumHeist template).  Surfaced
# 2 general engine fixes: character-in-closed-container visibility in the
# room view (Dracula in the coffin got an "is here" line) and rich-Text
# property values on characters/locations evaluating to "0" in OO chains
# (%character%.dk_BestemtKar).  See the Halloween entry in
# TODO_a5_walkthrough_bugs.md.
#
# (2026-07-03) October31st UPGRADED from the 4-command smoke probe to a FULL
# 100/100 WIN (153 turns, all four monsters: witch->oven, skeleton->gap,
# werewolf->poisoned meatball, mummy->holy-water vial, then Dracula staked),
# converted from the author's PDF walkthrough (user-supplied).  xoshiro = 0
# = full every-line conformance; the vanilla column (106) is inherent
# System.Random-vs-xoshiro divergence in the werewolf/mummy RANDOM WALKS
# (FD-vanilla's werewolf crosses the player elsewhere and that run dies), so
# NO golden -- same class as JacarandaJim/SixSilverBullets.  Surfaced 2
# general engine fixes (walk/event sub-display <DisplayOnce> retire; command-
# topic keywords must go through CorrectCommand) -- see the October 31st
# entry in TODO_a5_walkthrough_bugs.md.
# (2026-07-11) Oktober31Dansk wired as a second October 31st row (at the time
# it displaced the then-unwired October31stComp -- see the 2026-07-21 entry
# below, which corrects the "redundant build" call): a full DANISH translation
# of the same game, wired with its
# own 143-turn WIN (100/100), derived by re-translating the English route into
# the game's Danish vocabulary.  xoshiro 0; vanilla 32 is the identical
# werewolf random-walk RNG class as October31st (FD-vanilla's werewolf reaches
# the player a turn later and that run dies), so DIVERGE row, no golden.
# Surfaced 2 general engine fixes: (1) UTF-8-aware lowercasing of localized
# direction synonyms (a5parse.cpp) -- byte-wise tolower() left the game's
# DirectionEast "Øst/Ø/..." uppercased, so bare "øst"/"ø" matched no East exit
# (and "ø" was not even a known word); (2) a character INSIDE a closed opaque
# container (Dracula asleep in the crypt casket) no longer leaks an "is here"
# line when MoveCharacter InsideObject has synced char_loc to the container's
# room (a5state.cpp:a5state_character_visible_at_location) -- the same
# closed-container class as Halloween's coffin, but reached via char_loc here.
#
# (2026-07-03) LostChildren and MagneticMoon REWIRED to their NATIVE built-in
# WLKTHRGH walkthroughs (user report: the old CASA/Spectrum-original
# differential scripts derailed -- hundreds of soft failures that only looked
# like MATCHes because FD derailed identically).  LostChildren: full rescue-
# ending win, ZERO corrections, 0|0 golden-backed (3 engine fixes: resolved-
# location AloneWith/BeAlone, the Time "Skip N turns" action, double-render
# ListDescription selection scan).  MagneticMoon: 795/800 win (9 corrections
# vs the earlier-build text, see the script header; 6 engine fixes: per-item
# execution-time restriction re-eval on plural commands, empty-Text-property
# "" vs "0", expression-mode OO string quoting, clsVariable expr-AND-expr
# string concat, empty ObjectHashTable.List -> "nothing", bChanged-gated
# second boundary ALR round).
#
# (2026-07-04) MagneticMoon upgraded to a PERFECT 800/800 win, MATCH 0|0
# golden-backed.  The "missing 5" was NOT a build artifact: the built-in
# walkthrough simply never knocks on the temple wall (KnockOnWal +5) --
# `knock on wall` added before the stethoscope listen.  The two 2|2 budget
# hunks became engine fixes: (a) [am/are/is] conjugation now uses FD's
# PronounKeys nearest-preceding-rendered-name perspective ("The medic IS
# wearing a stethoscope"), (b) the Display-boundary first ALR round skips
# already-applied occurrences of a self-containing ALR (the tied cable's
# "some electrical cable[.]" suffix no longer doubles its ".") -- see
# TODO_a5_walkthrough_bugs.md.
#
# (2026-07-03) Illumina WIRED as a FULL WIN (new corpus game, Finn Rosenløv;
# user-supplied blorb + the author's CASA solution).  13 commands, one
# correction ("open guard room door" -> the first "open southern door").
# Scarier plays it to *** You have won ***; FrankenDrift CANNOT — its parser
# answers "Open what?" to `open southern door` (a real FD reference-
# resolution gap, BugHunt-class divergence in the correct direction).
# Golden = Scarier's winning transcript (vanilla 0); the xoshiro column
# carries the FD differential (5, RNG-independent) as the documented FD gap.
#
# (2026-07-11) IlluminaDansk WIRED as a FULL WIN: a full Danish translation of
# Illumina (Finn Rosenløv), from the Danish solution by "Denk" (based on the
# author's English solution, with permission).  13 commands, VERBATIM.  Unlike
# the English build this is a clean MATCH 0|0 in BOTH columns -- the Danish
# door answers to "sydlig dor" natively before AND after `laes skiltet`, so
# `aabn sydlig dor` opens the guard-room door and FD resolves it fine (no
# "Open what?" gap).  Wins to *** Du har vundet ***.
#
# (2026-07-04) RtC (Return to Camelot, Finn Rosenløv, IF Comp 2011) rewired
# from a stuck non-win to a FULL WIN.  The author's .doc walkthrough needed
# only trivial command fixes (courtyard<->kitchen stairs "d"/"up" -> the real
# compass exits "s"/"n"; "wear armour" -> "wear battered suit of armour"), but
# the true blocker was engine-level: RtC is Version 5.000020, which predates
# ADRIFT 5.0.22's <TaskExecution> element and ran under the v4-compatible
# HighestPriorityPassingTask mode.  Its central "unlock chain" puzzle only
# fires as a lower-priority *passing* task after the stock library "cannot be
# unlocked" fallback -- impossible under HighestPriorityTask.  Scarier now
# version-gates that default (a5model.cpp: element-less files below 5.000022 =>
# HighestPriorityPassingTask), so it reaches *** You have won ***.  FrankenDrift
# originally hardcoded HighestPriorityTask regardless of version and so could not
# win this game.  (2026-07-11) That FrankenDrift bug is now FIXED -- FileIO.vb
# applies the same version gate (element absent + dFileVersion < 5.000022 =>
# HighestPriorityPassingTask), so FD wins RtC too and the transcript is
# byte-identical to Scarier: MAP re-blessed 0|13 => 0|0, no other game affected.
#
# (2026-07-04) AoS upgraded 620/650 -> the MAXIMUM 650/650 ("scoring the
# maximum 650 points!" finale), MATCH 0|0 golden-backed.  The missing 30 =
# six 5-point content finds (read notice / x table graffiti / read sign at the
# paddock BEFORE knocking -- it teleports Muffin / rub the onyx ring / read
# labels / open case + x pipe); composition notes in AoS_walkthrough.txt,
# including the verified over-max alternates (NoSack2, rig dinghy, dodging
# the -15 tool penalties) that reach 680/650.  One engine fix fell out: the
# Display-boundary ALR pass ran over PLAIN text, so it matched an OldText
# ACROSS a stripped formatting tag that blocks FD's Display-time ReplaceALRs
# (AoS names its Known guard "The guard<Halberd>", defeating the game's own
# "comes marching from above" override).  a5text_render_plain now drops an
# A5_ALR_MARK sentinel where a tag is stripped; boundary matching is blocked
# exactly where FD's is and finish_turn strips the marks (a5text.h).  All
# other goldens byte-identical.
#
# (2026-07-04) SpectreOfCastleCoris rewired from the CASA-based non-win (town-
# only, desynced) to a FULL MAXIMUM-SCORE WIN: "...in 923 turns, scoring the
# maximum of 700 points!"  Base = the game's own built-in WLKTHRGH solution,
# repaired against this build's map + the Spectre-banish cadence re-derived by
# the adaptive solver (see the walkthrough header and
# test/spectre_prayer_solver.py).  Golden = Scarier's winning transcript
# (vanilla 0).  The xoshiro budget of 1 is a single real, RNG-independent
# divergence: the "say cook food" task chain runs "Execute Look" BEFORE
# "Execute Claude Cooks" (which moves the wooden tray into the kitchen), and
# FD's Look shows the tray anyway because FD expands the room's dynamic
# object list at DISPLAY time, after the whole turn's actions -- same
# display-time family as the AoS ALR-boundary note above.  Scarier renders the
# listing at Look-execution time, so its (arguably more correct) description
# omits the not-yet-moved tray.  Recorded in TODO_a5_walkthrough_bugs.md.
#
# (2026-07-04) AxeOfKolt rebuilt from the game's own built-in WLKTHRGH (was the
# old CASA-derived non-win that lost at 0/1200) + navigation repairs.  The
# engine fix that unblocked it: under HighestPriorityPassingTask a task that
# PASSES with no output no longer claims the turn (it continues to
# higher-priority tasks, FD AttemptToExecuteTask vb:916), so `buy beer` reaches
# the real tavern task, the Hussars clear the outlaws, and play advances past
# the forest.
# (2026-07-05) COMPLETE: full deterministic WIN (axe lowered to Kelson) under
# seed 1234; golden = the winning Scarier transcript.  Needed two more engine
# fixes (MoveCharacter InsideObject/OntoObject now syncs char_loc through the
# carrier object -- Grat's loo hut; and the furniture/container location sync
# keeps the current room when the object is a multi-room location-group static
# -- the cell floor on `lie down`) plus built-in-walkthrough repairs (corpse is
# 2S not 1S; burrow is in/out; "get tub"="get pot"; ONE `lever rails west` not
# two east; "sit on straw"->"lie down"; comma typos).  xoshiro budget 37: ~6
# pre-existing text divergences (dwark room-desc x2, Edwina greet, armourer
# echo, refusal-order swaps) + the magpie-chase RNG cascade -- FD picks a
# different Either() variant from shot 2 and misses the shot Scarier hits, so
# FD never gets the cotton and its transcript diverges from the Galhexia kill
# onward.  Next conformance target: align the magpie draw order/count
# (Task998 RAND(1,100) + Either() evaluation) so xoshiro drops toward the ~6
# text hunks.
#
# (2026-07-05, later) Magpie-chase RNG aligned: the Before-message flat path now
# interleaves render1 -> actions -> render2 (-> finalize) on a FROZEN template
# (a5text_process_frozen), exactly FD's clsUserSession.vb:1176-1205 -- see the
# run_task comments.  FD now reaches the win too, and the walkthrough's magpie
# segment was re-derived for the aligned stream (misses at tree1 + the E-W path,
# unconditional close-range kill at the abandoned cottage).  xoshiro 37 -> 16:
# the residual = the 6 pre-existing text hunks (dwark room-desc x2, Edwina
# greet, armourer echo) + FD printing refusals Scarier suppresses (armourer,
# leather sheath) + Scarier printing a failing-restriction message before a
# passing override's text (talisman "Right idea - wrong location!", block pile,
# wave axe "You are not carrying") + the tomb stone-slab state divergence
# (slab-dropped text missing from Scarier's room descs, 5 hunks).
#
# (2026-07-05, later still) xoshiro 16 -> 0: FULL MATCH, five engine fixes.
# (1) SetVariable LHS "Variable330[Hidden]" -- FD splits the key at '[' and
# ignores the index for a Length-1 variable (FileIO.vb / vb:2135); Scarier's
# lookup saw the whole string and silently no-op'd, so BlockTriggered never
# became 1 and every slab-gated room-desc segment stayed off (the 6-hunk tomb
# family).  (2) FD's NotUnderstood-on-empty-turn (vb:3421-3431): the chosen
# task runs, qTasksToRun drains, and if sOutputText is STILL empty FD falls
# to NotUnderstood() -- skipping TurnBasedStuff (no event tick).  Mirrored
# after scan_tasks, with two FD subtleties: the drain precedes the check
# (Mazoomah's `push radio` win text arrives via YouHaveWon LocationTriggers)
# and sOutputText is the raw MARKED-UP buffer, so a markup-only completion
# counts as output (st->turn_out_nonempty; BugHunt's <img> zoo map, which also
# keeps its turn ticking).  Fixes the armourer/leather-sheath refusals AND the
# silent oilman greet.  (3) Turn-global sReferencedText (vb:2567/3400/4474):
# every command-matched candidate's %text% capture overwrites the global slots
# during the scan -- even when that candidate fails -- and ReferencedText
# restrictions read the GLOBAL, defaulting to the raw input post-scan.  AoK's
# s_SayHelloTo ("say hello", no %text% of its own) passes its `ReferencedText
# Must BeContain "hello"` via that leakage (the Edwina greet + "(Edwina)"
# family).  (4) Direct-path override fails buffer in the exec scope and flush
# after the passes with FD's POSITIONAL cancel (vb:804-834): a later passing
# override with matching refs cancels the fail (talisman/block-pile/wave-axe),
# while a 2-ref fail survives a 1-ref pass (AoS `get ashes` keeps "nothing
# suitable to carry the ashes in").  (5) view_location_impl now runs ALR round
# 1 on the UNCAPPED marked-up room view before its cap (FD Display order,
# Global.vb:527-539), so AoK's Override12 blanks the lowercase "the dwark is
# here." NPC listing (the 2 pass room descs).  Golden re-blessed; whole
# 39-game corpus byte-identical in both RNG modes.
#
# (2026-07-05) RevengeOfTheSpacePirates COMPLETE: full deterministic MAXIMUM
# 550/550 WIN ("scoring the maximum 550 points!", 479 turns), MATCH 0|0 in
# both modes, golden-backed.  Continues the 410-point WIP (commit 0d7c054d):
# inserted the Wednesday hotel-bill detour (+15 -- PayBill only fires once the
# gym pays you off, PaidOff1 gates the foyer stop) and scripted the endgame
# (nurse-lure medical raid for the ZX128 hypogun, cockroach-sandwich comms raid
# for the sub-space radio call -- which MUST precede `set timer` -- bomb at the
# radar, the two 1-turn corridor-guard kills, Grande pickup, FCC stun grenade,
# and the command-chair launch with `input 4361 9622`).  See
# A5_WALKTHROUGH_FINDINGS.md for the full route notes.
#
# (2026-07-07) SixSilverBulletsTruth: a SECOND walkthrough of Six Silver Bullets
# that plays the LONG game to the OTHER ending.  The original
# SixSilverBullets_walkthrough.txt burns the clock and LEAVES THE CITY (a win,
# blissful ignorance); this one COMPLETES the assigned mission -- name the Black
# Agent "Thomas Flint" so the clocktower sniper leaves, kill the White Agent on
# her patrol, take the Skyscraper CEO's ID badge (through the Enemies of
# Freedom), infiltrate the mansion and assassinate the Governor, then kill the
# Indigo Agent once suspicion is high enough -- and so WALKS RIGHT IN at the
# docks (Missionacc==2) to READ THE DOSSIER and LEARN THE TRUTH ("*** You have
# lost ***", the game's real ending).  DIVERGE 111|0: xoshiro 0 = full
# conformance; vanilla 111 = pure System.Random-vs-xoshiro RNG text (epigraph /
# dream / OneOf picks over a much longer run), same class as the 18|0 short one.
# It also exercises scriptable %PopUpInput% (the "Thomas Flint" naming prompt):
# a5run_dump's popup_from_script feeds the next script line, and FrankenDrift's
# HeadlessRunner.TryGetScriptedInput does the same, so both stay byte-aligned.
#
# Son of Camelot (Finn Rosenloev, ADRIFT 5) -- DIVERGE 0|2.  Full winning
# walkthrough; needed two faithful engine bug fixes (Scarier AND FrankenDrift):
#   * RunImmediately System tasks now fire their event/walk Start controls
#     (a5run.cpp run_immediate_tasks + FD clsUserSession.vb:214 already did this),
#     so the repeating guided-tour Event7 (Start Completion Task60) starts and
#     Megan patrols -- without it Scarier left her parked at the courtyard;
#   * a *structurally* zero-length looping "follow the player" walk (Megan's/the
#     wolves' `Player 0` step) now steps each turn even once Finished
#     (a5run_events.cpp wk_do_steps / FD clsCharacter.vb DoAnySteps).  The check
#     is structural (all steps have turn count 0), NOT the runtime length, so a
#     normal patrol walk stopped before it ever started (Fortress of Fear's
#     Custodian is Finished with length 0) is not caught.
# The xoshiro residual is 2 cosmetic hunks: the Adventure-Upgrade prompt/title
# spacing and one "put fork on box" message (Scarier matches the generic put
# task, FD the game's specific Task133).  The vanilla column uses the golden
# (0); the raw FD-vanilla differential is huge because the enchanted forest is
# entered via MoveCharacter ToLocationGroup = a random room.
#
# (2026-07-10) LandOfTheMountainKing (LMKversion3, Kenneth Pedersen, ADRIFT5) ->
# WON 100/100 ("Ultimate King Slayer"), MATCH 0|0, golden-backed.  A turn-by-turn
# RPG: nine RNG combats (bat/squid/warrior/swamp-monster/werewolf/ogre + king)
# fought in weapon-acquisition order (fists->dagger->silver sword) plus the
# lighthouse-lamp cave-lighting puzzle.  Engine fix: a key-typed property
# (ObjectKey/CharacterKey/LocationKey) SetProperty whose value is a bare
# reference sentinel ("ReferencedObject") was stored verbatim instead of resolved
# to the bound key -- so `equip dagger`'s `SetProperty %Player% EquippedWe
# ReferencedObject` left EquippedWe = the literal word, and NO combat hit/miss
# subtask (all gated on EquippedWe == Knife/s_Sword/Nothing) ever fired, making
# every armed enemy unkillable (`kill squid` -> NotUnderstood).  a5run_action.cpp
# now resolves such sentinels at set time (matches FD's key branch).  Combat is
# all RAND text, so the raw FD-vanilla differential is huge (~342); the vanilla
# column uses the golden (0).  One free LOOK in the garden aligns the xoshiro
# combat stream so the king falls to a single mid-battle berry heal.
#
# (2026-07-10) Snowdrift (Anonymous, "Snowdrift V1", ADRIFT5) -> reaches the
# "To be continued..." ending at 50/50 (its full max), MATCH 0|0, golden-backed.
# A short unfinished demo: survive a blizzard (a 15-turn turn-based FREEZE event,
# escaped by getting underground), shovel out a buried trap door, descend, open
# the red library, dial pi (3-1-4-1) on a 1-4-only dial phone, and ride an
# elevator up.  No engine change: the two elevator doors are the author's
# real-time (seconds) events, and the tasks that open them (ElevatorDo1/2) carry
# the one-character command ".", so the ADRIFT wait verb "z" (a single char) is
# exactly what fires them -- FrankenDrift opens the doors on the identical inputs
# (0|0 both modes).  The orphaned DigScore (+15) never fires; 50 IS the max.
#
# (2026-07-10) TheHeritage (Finn Rosenløv, 2016) wired at MATCH 0|0 in both
# modes, golden-backed, first try -- no engine change.  A one-room mystery
# (the author's fixed re-release of his unwinnable 2015 comp entry): fireplace
# ashes -> loose brick -> key -> wine cabinet -> drink EXACTLY two glasses
# (Wine variable 4 -> 2; the ring pressure plate needs Wine == 2, a third
# drink is unwinnable and a fourth is a LOSE ending) -> bottle on the
# mantelpiece ring -> secret room = WIN.  No scoring, no events, no RAND text.
#
# (2026-07-10) TheWayHome (Kenneth Pedersen, part 2 of The Bash Saga, v3) wired
# at MATCH 0|0 in both modes, golden-backed, first try -- no engine change.
# MAX-SCORE WIN 115/115 (23 x 5 pts), fully deterministic (no RAND draws).
# Ice-valley escape (blanket->stone->rope, ladder sled through the wall, sand
# on boots, saw hole, diamond from the floor hatch, 7-turn dog chase into the
# ice hole, diamond in the idol's hollow eye melts the valley) then Ravine
# City (poster->Tome->keymaster, Brian's gloves->catch rat->lure Da Mon, lock
# pick -> Louniss' PSKLOM4 potion -> apple of force -> SAY READY TO GUARD ->
# eat apple as the fire worm surfaces = WIN).
#
# (2026-07-10) TheRoyalPuzzle (Kenneth Pedersen 2017/2023 v3, ADRIFT5 port of
# the Zork/Dungeon Royal Puzzle via Ethan Dicks' zdungeon.z5) wired at MATCH
# 0|0 in both modes, golden-backed, first try -- no engine change.  WON in 41
# game moves; fully deterministic (no RAND draws).  The whole game is the
# classic 6x6 sliding-sandstone grid, implemented via dynamic location-group
# membership (Sandstones/s_Eastladder/s_Westladder) mutated by
# AddLocationToGroup/RemoveLocationFromGroup, with Dummy-character probe moves
# standing in for cell queries -- good coverage of those engine paths.  Route
# was machine-solved: BFS over the exact extracted rules (push = wall cell ->
# free cell beyond, player follows; diagonals blocked when both flanking
# orthogonals are walls; pushing the (6,3) wall west is specially forbidden).
# Get the gold card from under the wall at <4,4>, walk the east-ladder wall
# from <2,5> around the south of the grid and up column 2 to <1,2>, then climb
# the ladder at <1,1> holding the card = WIN.  The steel-door slit exit is the
# LOSE ending (card confiscated).
#
# (2026-07-10) CosmicAdventure ("An Unspecified Cosmic Adventure That Doesn't
# Have Words Quest and Space in the Name", Karmo Talts 2019, v5.0000353,
# extracted from its exe-wrapped build like TheVirtualHuman -- but here the
# trailing-marker payload is a full blorb whose FORM length field undercounts
# and must be patched to cover the real RIdx/ADRI/IFmd/TEXT chunk stream)
# wired at MATCH 0|0 in both modes, golden-backed, first try -- no engine
# change.  Unscored comedy; WON in 39 commands: serve the burger (patty +
# secret sauce IN the buns while holding them; "put X between Y" is a decoy
# task), whiskey->card->cab for the drunk, wet the sink cloth and clean the
# filth, then east = abducted; show meat to beast, take head (acid cuts the
# bars), simless phone from the hallway table, sheet->tie guard->his gun +
# record his yelling, shoot guards (gun breaks, battery is inside the DROPPED
# broken gun -- take it, put in laser), eye from remains fools the retinal
# scanner, voice file fools the mike, then shoot weapon on top = WIN.
#
# (2026-07-10) NobleCrook1 (Noble Crook, episode 1, kaemi 2016) wired at MATCH
# 0|0 in both modes, golden-backed, first try -- no engine change.  Unscored
# hotel-heist comedy; WON in 49 commands.  Chain: towel FIRST (wet soap blocks
# leaving your room -- every lobby exit is also gated on not holding the
# counter key, so wet+dry the soap, then take key 107, impress it in the soft
# soap at the counter and put it straight back), lottery ticket from the yard
# ashtray to the poor man at the gate (he wins on YOUR numbers; you pass out
# and inherit the doctor's bag), buy spaghetti on hotel credit, flush it to
# summon the plumber, swap bags (drop doctor's, take plumber's = auto-sneak to
# the lobby where a drunk vomits on you and drops his key), file the blank
# into a duplicate via the soap impression, unlock room 107 (wife scene throws
# you out; do NOT speak to the man in 107 afterwards -- any topic is an
# instant LOSE), circus sign from the cupboard into the gate holder (take the
# hotel sign out first) to stop the rat exodus, then shoplift: take perfume +
# tin can, give the CAN to the clerk while holding the perfume (he resets the
# alarm as you walk out), give perfume to the woman = WIN.
#
# (2026-07-11) NobleCrook2/3/4 (Noble Crook, episodes 2-4, kaemi 2016) each
# wired at MATCH 0|0 in both modes, golden-backed, first ground-truth try -- no
# engine change.  Same author/engine as ep1; derived XML-dump-first.  All three
# unscored; movement/holding gates carry the puzzles as usual.
#   EP2 (40 cmds, WIN): expose the fake tribesmen, then win the brooch.  The
#   journalist guards the calling card in the restaurant, so first do the banana
#   gag: take the fashion magazine (lobby), take the banana peel (yard trash
#   can), PUT PEEL ON STEPLADDER in the hallway -> the workman falls and the
#   journalist leaves his post; now take the stepladder AND take the card.  On
#   the beach GIVE CARD then GIVE MAGAZINE to the woman (card-first order matters
#   -- the win branch needs her already holding the card) -> she leaves and the
#   tribesmen relocate to the front yard.  Fetch the hose (cupboard SE of lobby),
#   ATTACH HOSE TO PIPE at the yard and TURN TAP -- only fires with the tribesmen
#   present (spray washes off their blackface; the exposed Lady appears in the
#   restaurant).  Carry the stepladder down the beach, CLIMB LADDER, take the
#   coconut and THROW COCONUT AT BLOKE *from the palm top* (ThrowNut requires
#   PlayerLocation=top), down, take the dropped book, GIVE BOOK TO LADY for a
#   drink, POUR DRINK = WIN.
#   EP3 (17 cmds, WIN): the whole cocktail/horn/seismologist chain is optional
#   flavour -- the win only needs the bandaged bloke holding ice AND the stolen
#   seismology guide.  Enter the lobby (the woman hands you a Kafka novel),
#   TAKE BOOK (she lends you her seismology book but blocks every lobby exit
#   while you hold it), then DROP NOVEL -> you swap the novel decoy for the guide
#   and can leave.  Grab the towel (toilet), OPEN ICEBOX + take ice cubes
#   (kitchen), PUT CUBES IN TOWEL, then on the beach GIVE TOWEL TO BLOKE (he
#   wraps the ice to his head) and GIVE GUIDE TO BLOKE (needs him holding the
#   cubes) = WIN.
#   EP4 (21 cmds, WIN): linear escape.  KISS HANDS -> knocked out into a sinking
#   car; OPEN BOX, take knuckles, HIT WINDOW WITH KNUCKLES, HIT SHARDS WITH
#   KNUCKLES (clears the Out gate), OUT into the sea; OPEN TRUNK, take tool,
#   GET TIRE WITH TOOL, INHALE (snorkel until the beach men leave), UP; take the
#   shovel, north, DIG BUSH WITH SHOVEL, TAKE BUSH (holding it is the gate to go
#   north), north, GRAB RUNNERS -> caught at the mansion cell; ASK WOMAN ABOUT
#   HOTEL (she monologues the oil plot and summons the thugs), KNEEL (they shoot
#   each other), take the tray, PUT TRAY UNDER SHIRT = WIN.
#
# (2026-07-10) Sophia ("Sophia or Wisdom Defined", 2021, v5.0000364) wired at
# MATCH 0|0 in both modes, golden-backed, first try -- no engine change.
# Unscored allegorical mini-quest (11 rooms) with a built-in per-room WLKTR
# hint command; WON in 34 commands with the TRUE ending.  Route: say
# "shakespeare" at the quote door ("richard iii" is a secret bonus that opens
# a side room east), kiss the sleeping princess and answer NO to her proposal
# (YES = alternate early "married" Win via EndGame; NO drops the key to the
# thorned door), console Boethius with "say virtue to boethius" (hello/yes
# first for the full consolation-of-philosophy exchange; teleports you to the
# halls of thought), take communion in 5.3 (eat bread THEN drink wine -- wine
# is gated on the bread; 3 waits step the Son of Man dialogue), take the
# knife, name Rubliev's icon ("holy trinity" in 5.2, which spawns your Image
# walking to the cross room), "kill my image" on St Andrew's cross holding
# the knife (opens the south door in 5), then in the end room take the RUSTY
# sword and "attack demon" = WIN.  The golden sword is the LOSE trap (it
# shatters, EndGame Lose), and "hit"/"kill demon" while holding it hits the
# higher-priority Hit task -- "attack demon" with the rusty sword is safe.
#
# (2026-07-10) Thy Balconyman (burninatedPeasant, 2024, v5.0000366) wired at
# MATCH 0|0 in both modes, golden-backed.  Tiny meme-medieval quest (18 rooms,
# MaxScore 40, real max 38): GetSteak's +2 is unreachable (Steak is Static so
# the library take-task's StaticOrDynamic restriction fails before the
# specific fires -- FD-identical), and the topic IncVariables (intro +5, lips
# +5, haircut +15) never land because ADRIFT only lets a TASK change Score.
# Route: chat the woman up, Mat el Gato gives the lips (ask about lips; eat
# for +3), fetch cane + sit office chair in the one-way Drab Hallway, escape
# the Antechamber by grabbing the flask WITHOUT pliers (VRAP -> Balcony),
# give cane (+5), say hi + "say yes" at the barber (sideburn), jello from the
# basement cupboard into the Dank Hallway box (+7, opens N), coke pants from
# the Locker Room to Guillermo (+10, drops Meowmere), bare "hit" kills The
# Cheat (+11, opens N), pliers from the Forest, then flask WITH pliers = WIN.
# Surfaced 3 engine fixes: (1) Score changes are dropped when no task is
# executing (FD ExecuteSingleAction's `task IsNot Nothing` gate) -- topics
# and events can't score; (2) topic actions run with NO owning task (their
# Score IncVariables are no-ops even mid-Say-task); (3) clsCharacter
# Introduced: a %CharacterName% render in a conversation reply upgrades later
# indefinite descriptor renders to "the X" and pronoun-replaced renders still
# mark -- but task messages / room listings / walk announcements are built
# outside bDisplaying and never upgrade or mark.
#
# (2026-07-10) Thy Dunjohnman (burninatedPeasant, 2024, v5.0000366) wired at
# MATCH 0|0 in both modes, golden-backed.  Six-room joke dungeon by the
# Balconyman author: key from the Chamber Pot, unlock the iron door (+10),
# see the platform in the Office, pull the lever in the Machinations Room
# (platform down, coins land on it), take coins (+5), NE from Machinations
# (gated on having SEEN the coins) to the Winner Room, push RIGHTbutton = WIN
# (leftbutton = boulder death).  Final score 15/30: the game renames East to
# "chamber pot" AND hangs a one-shot silent Override specific (SetPtsNoTo,
# MaxScore=30) off PlayerMovement East, so the FIRST east attempt is consumed
# without moving (falls through to NotUnderstood) and the max can never be
# scored -- FD-identical.  Surfaced 2 engine fixes: (1) the character-subject
# HaveSeenObject restriction op was missing (fell through to pass, opening
# the gated SE/NE exits from turn one); (2) a5parse_canonical_direction never
# matched multi-word localized direction synonyms ("chamber pot"), because
# only the input side was space-stripped.
#
# (2026-07-10) Ectocomp 2011 four-pack (all v5.000021 3-hour speed-IF tafs):
#  - TheHouse (Po. Prune) MATCH 0|0: author's .doc walkthrough.  The win
#    REQUIRES answering YES to the "Adventure Upgrade" bracket-correction
#    question (Task29's verbatim "#A#A#O#" lose-task otherwise passes via the
#    trailing OR once Vickie is untied and outranks the winning Task28).  The
#    question is now a real host prompt answered through NORMAL input (like
#    the ADRIFT 4 gender prompt): a5model_upgrade_pending/question/answer +
#    FD's CorrectBracketSequence ("#A#O#..." -> "#A(#O#...)"); a5run_dump
#    consumes a literal leading yes/y/no/n script line (anything else is
#    pushed back and answers no, exactly FrankenDrift.Headless's peek), and
#    os_glk asks interactively.  A host that never asks keeps the legacy
#    prepend-question-imply-NO path, so RtC / SonOfCamelot / TheVirtualHuman
#    goldens are byte-identical.  FD.Headless fix: AskYesNoQuestion now peeks
#    past blank/#-comment lines (a commented script can lead with its answer).
#  - DeathShack (Mel S.) golden-backed, was an RtC-class FD gap: part
#    II's `open door` needs the version-gated HighestPriorityPassingTask mode
#    to fall through Task2's Location1-only "You can't do that here!" to the
#    stock OpenObjects; FD used to hardcode HighestPriorityTask and never reach
#    part IV / the note.  (2026-07-11) FIXED by the same FileIO.vb version gate
#    as RtC -- FD now falls through too; MAP re-blessed 0|6 => 0|0.
#  - IgnisFatuus (DCBSupafly) MATCH 0|0 first try, blind-derived full win
#    (the jester carves the jack-o'-lantern he is banished into).
#  - StuckPiggy (Mike Desert) MATCH 0|0: author's txt walkthrough (win =
#    `kiss billy`, reversed in the txt).  Surfaced the scan-end continuation
#    fix (a5run.cpp scan_tasks): a passing-but-silent task (the empty `open
#    clock` stub) starts FD's EvaluateInput(Priority+1) continuation, which
#    is a FRESH pass -- candidates recorded before it are cleared, and a
#    failing-with-output / amb / noref candidate the continuation itself
#    records now surfaces (OpenObjects' "You can't open the old grandfather
#    clock as it is locked!") instead of being swallowed by the cont_active
#    return.
#
# (2026-07-10) Ectocomp 2012 pair (both v5.000026 3-hour speed-IF tafs):
#  - Beythilda (A. Hazard) MATCH 0|0 golden-backed: all-verse witch vignette,
#    full WIN (EndGame Win) via the game's own `walkthrough` route.  The mob
#    event's "5 to 12" length is an RNG draw, so the vanilla golden diff is 0
#    but a direct FD-vanilla diff shows 2 (the ***CCRAACCKK*** break-in lands
#    one `wait` later on FD's System.Random -- pure RNG-stream noise; the
#    script's wait padding survives every roll).  Byte-exact under xoshiro.
#  - ECOD3D (Mel S.) MATCH 0|0 first try, blind-derived: The Evil Chicken of
#    Doom 3D, full route to the single EndGame Neutral (shotgun the two-nosed
#    chicken).  Surfaced the no-Player-character fallback: the game defines
#    only the NonPlayer "Steve", and clsAdventure.Player's getter promotes
#    the first character in the table to viewpoint (a5state_new player_key).
#
# (2026-07-10) Ranaway (TrexandDrago Development, Single Choice Jam 2023,
#    v5.0000366) MATCH 0|0 first try: keypress-menu escape vignette played to
#    its narrative end (no EndGame action in the file).  EXE-EXTRACTED --
#    Ranaway.exe stores the payload offset as a '\0'+hex trailer and the
#    carved blorb's FORM length undercounts (patched to filesize-8), the
#    CosmicAdventure recipe; carved copy = adrift5-games/Ranaway.blorb.
#
# (2026-07-11) Space Detective, episodes 1-7 (a 7-part ADRIFT 5 serial) all
#    wired at MATCH 0|0, golden-backed, blind-derived from each game's model XML.
#    Each is a short inventory puzzle: ep1 dimensional-gateway blackmail/battery
#    chain to reconnect the shuttle door; ep2 seedy-bar murder investigation
#    (hologram-skin disguise); ep3 sticky-stick perfume fetch + dynamite the dam;
#    ep4 apartment-building map-piece heist; ep5 knock out two guards + power a
#    shuttle generator with a thunderball; ep6 elevator-shaft infiltration of an
#    electronics firm; ep7 shoot the villain's spaceship out the shuttle window.
#    ONE engine fix fell out of ep4: a5parse_canonical_direction resolved a typed
#    direction word by scanning kDirs in COMPASS order, so ep4's data quirk
#    (<DirectionNorthEast>West/W</> redefines NorthEast's synonyms to "west"/"w"
#    while West keeps its default) mis-bound "west" to the non-existent NorthEast
#    exit, stranding the player (unwinnable).  Now scans in DirectionsEnum order
#    (kDirEnum: West=3 before NorthEast=8), mirroring FrankenDrift's
#    `For eDirection = North To NorthWest`, so West wins the synonym tie.  Default
#    games have disjoint synonyms so the reorder is inert -- whole corpus stays
#    98 MATCH / 11 DIVERGE-at-baseline, zero regressions.
#
# ReturnToCastleCoris (Larry Horsfield, 2020; Alaric Blackmoon episode, Version
# 5.0000366) -- built-in WLKTHRGH extracted and repaired to a MAXIMUM-score win
# ("...in 437 turns, scoring the maximum 400 points!"); see the header of
# test/ReturnToCastleCoris_walkthrough.txt for the four route repairs (start-menu
# o/b, slime-eater sack tunnel level, gold-ring-vs-alehouse death, Flambeau's
# green door).  Budget 0|1: vanilla strict-diffs the committed golden
# (ReturnToCastleCoris_expected.txt, must stay 0); xoshiro is the FD-conformance
# column.  The xoshiro residual (1) is the first `look in gap`:
# the task's second completion alternate ("You can also see ... under there.",
# gated on `AnyObject Must BeAtLocation Location66`) lists Location66's
# Under-Outcrop static scenery (tiny stream, fresh water, walls, ...) in Scarier
# but FrankenDrift emits only the base sentence.  Vanilla adds 5 pure-RNG hunks:
# the salt-flats' random vulture/eagle/lizard atmosphere event draws different
# text under .NET System.Random vs. xoshiro (aligned away under FD_RNG=xoshiro).
#
# (2026-07-21) AxeOfKoltComp WIRED at MATCH 0|0, golden-backed -- "AoK Comp.blorb",
# the last game file in test/adrift5-games with no MAP row.  It is Larry
# Horsfield's 2012 "V5 Intro Comp Entry": a four-location taster that compresses
# the full Axe of Kolt's Chapter 1 (15 locations, 4 of them menu/prologue pages;
# no built-in WLKTHRGH; never playtested by the author).  The route's spine is
# lifted VERBATIM from AxeOfKolt_walkthrough.txt -- read signpost / help
# innkeeper / w / x tapestry / read writing / e, then the outlaws-pass-dwark-
# trapper-tapestry-magor-forest-ferry-kolt-xixon-kelson-father topic run -- with
# the barmaid renamed Bella -> Lizzie and `ask about X` -> `ask lizzie about X`;
# the remaining 18 ask topics and `kiss lizzie` are comp-only and were derived
# from the module XML.  MAXIMUM score: "you scored 72 out of a possible 1000"
# (the 1000 is the full game's leftover MaxScore; 72 is every Score action in
# the module).  Two traps the route has to respect: exactly ONE turn in the Inn
# Yard before Event1's Task75 drags you inside without the offer of help, and a
# hard 50-move Event2 window from `help innkeeper` to the traveller's arrival
# (Task198 -> Task65, EndGame Neutral) inside Group2.
#   ONE engine fix fell out of it: the "does the Introduction end in a <cls>?"
#   test at game start compared the rendered intro's LAST BYTE against
#   A5_CLS_MARK, but this intro ends "<waitkey><cls></b>" and the stripped </b>
#   leaves an A5_ALR_MARK sentinel *after* the wipe marker.  The raw test read
#   that as "text after the wipe" and appended a paragraph break, which survived
#   sb_resolve_cls as a stray blank line above the first location -- the one
#   hunk in both RNG modes.  a5run.cpp now scans back over the non-output
#   presentation sentinels (msg_ends_with_cls), the same set msg_has_output
#   already ignores.  Inert for every other game (no other corpus intro puts
#   markup after its trailing <cls>); whole corpus re-run, zero regressions.
#
# (2026-07-21) October31stComp WIRED at 106|0 -- and the older note above
# calling it "the redundant second English build" was WRONG.  It is the
# ORIGINAL COMPETITION RELEASE (module LastUpdated 2022-07-16); the row wired as
# October31st is the later "October 31st. (post comp. version)" (2022-08-01).
# The two module XMLs differ in 24 hunks and the changes are real content, not
# churn: the comp build's intro documents an extra "Z" = orphanage hints system
# that the post-comp build DROPPED; the post-comp build lengthens the
# cage-escape text and adds a witch-doctor/curse paragraph to the old man's
# backstory; and one event's <Length> is 29 in the comp build vs 20 in the
# post-comp one, with its subevents at +15 vs +20.  The ROUTE is unaffected --
# October31st_walkthrough.txt replays verbatim to the same MAXIMUM ending
# ("*** You have won ***", 100/100, 153 turns, one fewer than the post-comp
# build) -- so October31stComp_walkthrough.txt is that script with its own
# header; keep the two in step unless the event-timing difference forces them
# apart.  Budget 106|0 is the SAME profile as October31st: xoshiro 0 = full
# every-line conformance, and the vanilla 106 is the identical werewolf
# random-walk RNG class (System.Random vs xoshiro sends FD-vanilla's werewolf
# elsewhere and that run dies), so DIVERGE row, no golden.
#
#   name | game file | vanilla budget | xoshiro budget
MAP=$(cat <<'EOF'
AchtungPanzer|AchtungPanzer.blorb|0|0
anno1700|Anno1700.blorb|0|0
AxeOfKolt|TheAxeOfKolt.blorb|0|0
AxeOfKoltComp|AoK Comp.blorb|0|0
SpectreOfCastleCoris|TheSpectreOfCastleCoris.blorb|0|0
StarshipQuest|StarshipQuest.blorb|0|0
MagneticMoon|MagneticMoon.blorb|0|0
Illumina|Illumina.blorb|0|0
IlluminaDansk|IlluminaDansk.blorb|0|0
RevengeOfTheSpacePirates|RevengeOfTheSpacePirates.blorb|0|0
DieFeuerfaust|DieFeuerfaust.blorb|0|0
LostChildren|TheLostChildren.blorb|0|0
RunBronwynnRun|RunBronwynnRun.blorb|0|0
RtC|RtC.blorb|0|0
TreasureHuntInTheAmazon|TreasureHuntInTheAmazon.blorb|0|0
StoneOfWisdom|StoneOfWisdom.blorb|2|0
GrandpasRanch|Grandpa_ParserComp_V1.blorb|0|0
JacarandaJim|JacarandaJim.blorb|99|0
SixSilverBullets|SixSilverBullets.blorb|18|0
SixSilverBulletsTruth|SixSilverBullets.blorb|111|0
PathwayToDestruction|PathwayToDestruction.blorb|0|0
CallOfTheShaman|TheCallOfTheShaman.blorb|0|0
ThingsThatGoBumpInTheNight|TBN v.2.blorb|0|0
LostLabyrinthOfLazaitch|TheLostLabyrinthOfLazaitch.blorb|8|0
MaroonedOnMazoomah|MaroonedOnMazoomah.blorb|0|0
TheEuripidesEnigma|TheEuripidesEnigma.blorb|0|0
DwarfOfDirewoodForest|DwarfOfDirewoodForest.blorb|0|0
DwarfOfDirewoodForestDDF|DDF.blorb|0|0
BugHuntOnMenelaus|Bug Hunt On Menelaus.blorb|0|0
Tribute|Tribute.blorb|0|0
AoS|AoS v.4.blorb|0|0
FinnsBigAdventure|FBA v.3c.blorb|0|0
Halloween|Halloween.blorb|0|0
MagorInvestigates|MI_v.1.blorb|0|0
MuseumHeist|MuseumHeist.blorb|0|0
October31st|October31st.blorb|106|0
October31stComp|October31stComp.blorb|106|0
Oktober31Dansk|Oktober31Dansk.blorb|32|0
TheFortressOfFear|TheFortressOfFear.blorb|0|0
Xanix|XXR v.4.blorb|0|0
Tingalan|Tingalan.blorb|0|0
TingalanTrue|Tingalan.blorb|0|0
BookOfJax|BoJ v.2.blorb|0|0
GrandmasFlyingSaucer|GFS_Frankendrift.blorb|0|0
TheGardenParty|TheGardenParty.blorb|0|0
TheDeadOfWinter|TheDeadOfWinter.blorb|0|0
LostCoastlines|Lost_Coastlines.taf|0|0
Skybreak|Skybreak.taf|0|0
SkybreakWin|Skybreak.taf|0|0
ISummonThee|ISummonThee.taf|5|0
BeThere|BeThere.taf|0|0
SonOfCamelot|SoC.blorb|0|0
AlienDiver|AlienDiver.blorb|0|0
AlgernonsConundrum|AlgernonsConundrum.blorb|0|0
AllThroughTheNight|AllThroughTheNight.blorb|0|0
AnAdventurersBackyard|AnAdventurersBackyard.blorb|0|0
GalensQuest|GalensQuest.blorb|0|0
JustAnotherFairyTale|JustAnotherFairyTale.blorb|0|0
LMKversion3|LMKversion3.blorb|0|0
Snowdrift|SnowdriftV1.taf|0|0
TheMysteriousSpaceship|TMSr2.blorb|0|0
TheTartarusProject|TTP.blorb|0|0
MurderMostFoul|MurderMostFoul.taf|0|0
TheHeritage|TheHeritage.blorb|0|0
TheWayHome|TheWayHome.blorb|0|0
TheRoyalPuzzle|TheRoyalPuzzleV3.blorb|0|0
TheDragonDiamond|DragonDiamond_V2.blorb|0|0
TheVirtualHuman|TheVirtualHuman.taf|0|0
CosmicAdventure|CosmicAdventure.blorb|0|0
NobleCrook1|NobleCrook1.blorb|0|0
NobleCrook2|NobleCrook2.blorb|0|0
NobleCrook3|NobleCrook3.blorb|0|0
NobleCrook4|NobleCrook4.blorb|0|0
RaceAgainstTime|RaceAgainstTime.blorb|0|0
Sophia|Sophia.blorb|0|0
ThyBalconyman|ThyBalconyman.blorb|0|0
ThyDunjohnman|ThyDunjohnman.blorb|0|0
TheSalvage|TheSalvage.blorb|0|0
AmbassadorToDupal|AmbassadorToDupal.taf|0|0
Bariscebik|bariscebik.taf|0|0
CanYouStandUp|canyoustandup.taf|0|0
ColorOfMilkCoffee|coloromc.taf|0|0
DontGo|dontgo.taf|0|0
WhatTheMurdererHadLeft|murderer.taf|0|0
AReadingInMay|reading.taf|0|0
TheHouse|The House.taf|0|0
DeathShack|DeathShack.taf|0|0
IgnisFatuus|Ignis Fatuus.taf|0|0
StuckPiggy|Stuck Piggy2_0.taf|0|0
Beythilda|Beythilda.taf|0|0
ECOD3D|ECOD3D.taf|0|0
Ranaway|Ranaway.blorb|0|0
Temperamentum|Temperamentum v5.blorb|0|0
Trapped|Trapped.taf|0|0
BlankWallIntro|blankwall.taf|0|0
TheDarkHour|darkhour.blorb|0|0
Beagle2|Beagle2.blorb|0|0
JustAnotherChristmasDay|JACD.blorb|0|0
HeadCase|Intro_compVB1.taf|0|0
MargaritaBlender|ML256_Blender.taf|0|0
MonsterAge|Monster_Age_I.taf|0|0
Organic|Organic vA0.0.5b.taf|0|0
ShatteredMemory|Shattered Memory - Concept Demo.taf|0|0
TheDayProgram|The Day Program V1.taf|0|0
TheLastExpedition|TheLastExpedition_Final.taf|0|0
WW2ElevatorEscape|WW2 Elevator Escape R3.blorb|0|0
ADifficultPuzzle|A_Difficult_Puzzle_v2(LimitedParserEdition).blorb|0|0
SpaceDetective1|SpaceDetective1.blorb|0|0
SpaceDetective2|SpaceDetective2.blorb|0|0
SpaceDetective3|SpaceDetective3.blorb|0|0
SpaceDetective4|SpaceDetective4.blorb|0|0
SpaceDetective5|SpaceDetective5.blorb|0|0
SpaceDetective6|SpaceDetective6.blorb|0|0
SpaceDetective7|SpaceDetective7.blorb|0|0
ReturnToCastleCoris|ReturnToCastleCoris.blorb|0|0
EdithsCats|edithscats.taf|0|0
BirthOfThePhoenix|PhoenixV3.blorb|0|0
EOF
)

# Set XOSHIRO=0 to skip the (dotnet-heavy) xoshiro pass and check vanilla only.
RUN_XOSHIRO="${XOSHIRO:-1}"

# Save/restore self-check (a5run_dump only, no dotnet): SAVERESTORE=0 skips it.
# Replays each game a second time with A5_SAVE_AT set to the script midpoint,
# which makes a5run_dump serialize the full state, free the run, rebuild it from
# the blob, and continue -- the transcript must stay byte-identical.  A mismatch
# is a FAIL: it means the save format dropped some state (how the task_scored
# score-inflation bug in TheFortressOfFear was caught).
RUN_SAVERESTORE="${SAVERESTORE:-1}"

# Hunk count for one (game, script) under a given RNG mode.  $3="" = vanilla,
# $3="xoshiro" = aligned stream.  Diff hunk headers look like 12c12 / 5a6 / 10,14d9.
count_hunks() {  # $1=gamefile $2=script $3=rngmode
    FD_RNG="$3" "$GT" "$1" "$2" 2>/dev/null \
        | grep -cE '^[0-9]+(,[0-9]+)?[acd][0-9]+'
}

# Per-mode verdict vs budget, echoed as "<count> <tag>" where tag is one of
# ok/better/REGRESSION (or a bare count when that mode is skipped).
verdict() {  # $1=count $2=budget
    if   [ "$1" -gt "$2" ]; then echo "$1 REGRESSION"
    elif [ "$1" -lt "$2" ]; then echo "$1 better"
    else                         echo "$1 ok"
    fi
}

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

# One game, start to finish; writes its table row to $WORKDIR/<idx>.row and,
# on a regression, its name to $WORKDIR/<idx>.reg.  Runs in the background
# (up to $JOBS at once) -- rows are printed in MAP order once all are done.
run_one() {  # $1=idx $2=name $3=game $4=vbudget $5=xbudget
    local idx name game vbudget xbudget script gf golden stxt row
    local got want vout xout hv hx vv xv vtag xtag vcell xcell status
    local srfail srcell ncmd mid srtxt
    idx=$1 name=$2 game=$3 vbudget=$4 xbudget=$5
    row="$WORKDIR/$idx.row"
    script="$HERE/test/${name}_walkthrough.txt"
    gf="$GAMES/$game"
    [ -f "$script" ] || { printf "%-24s %-9s\n" "$name" "NOSCRIPT" > "$row"; return; }
    if [ ! -f "$gf" ]; then
        printf "%-24s %-9s (game file absent: %s)\n" "$name" "SKIP" "$game" > "$row"
        return
    fi

    # ONE Scarier replay per game, shared by the golden compare/bless AND both
    # RNG-mode ground-truth diffs (via SCARIER_TXT): Scarier's transcript is
    # RNG-mode-independent -- only FrankenDrift's generator changes between the
    # vanilla and xoshiro passes, so replaying Scarier per mode was pure waste.
    stxt="$WORKDIR/$idx.scarier"
    "$A5RUN" "$gf" "$script" 2>/dev/null > "$stxt" || true

    # --- vanilla -------------------------------------------------------------
    # Fast strict golden diff when a golden exists (vanilla transcript, no dotnet).
    golden="$HERE/test/${name}_expected.txt"
    if [ "$BLESS" = 1 ]; then
        # (Re)write the golden through the SAME normalisation the comparison uses,
        # so blessing is canonical and never trips the trailing-newline artifact.
        sed 's/[[:space:]]*$//' "$stxt" | cat -s > "$golden"
        printf "%-24s %-9s %s\n" "$name" "BLESSED" "$golden" > "$row"
        return
    fi

    # --- save/restore self-check (a5run_dump only; SAVERESTORE=0 skips) -------
    # Save+restore the full state at the script midpoint; the transcript must
    # stay byte-identical to the plain replay ($stxt), or the save format lost
    # state.  A mismatch is folded into STATUS=FAIL below.
    srfail=0; srcell="(skipped)"
    if [ "$RUN_SAVERESTORE" = 1 ]; then
        ncmd=$(grep -c . "$script")
        mid=$(( ncmd / 2 )); [ "$mid" -lt 1 ] && mid=1
        srtxt="$WORKDIR/$idx.sr"
        A5_SAVE_AT="$mid" "$A5RUN" "$gf" "$script" 2>/dev/null > "$srtxt" || true
        if diff -q "$stxt" "$srtxt" >/dev/null 2>&1; then srcell="OK"; else srcell="DIFF"; srfail=1; fi
        rm -f "$srtxt"
    fi

    if [ -f "$golden" ]; then
        got=$(sed 's/[[:space:]]*$//' "$stxt" | cat -s)
        # Compare content only: both sides are captured via $(...), which strips
        # trailing newlines identically, so a golden written by a plain `> file`
        # redirect (one extra newline) still matches -- no diff -q newline artifact.
        want=$(cat "$golden")
        if [ "$got" = "$want" ]; then hv=0; else hv=999; fi
    else
        vout=$(SCARIER_TXT="$stxt" "$GT" "$gf" "$script" 2>/dev/null)
        [ "$VERBOSE" = 1 ] && printf '%s\n' "$vout" > "/tmp/a5wt/${name}.diff"
        hv=$(printf '%s\n' "$vout" | grep -cE '^[0-9]+(,[0-9]+)?[acd][0-9]+')
    fi
    vv=$(verdict "$hv" "$vbudget"); vtag=${vv#* }
    vcell="$hv (=$vbudget)"
    [ "$vtag" = better ] && vcell="$hv (<$vbudget rebless)"
    [ "$vtag" = REGRESSION ] && vcell="$hv (>$vbudget FAIL)"
    [ "$hv" = 999 ] && vcell="golden MISMATCH"

    # --- xoshiro -------------------------------------------------------------
    if [ "$RUN_XOSHIRO" = 1 ]; then
        xout=$(SCARIER_TXT="$stxt" FD_RNG=xoshiro "$GT" "$gf" "$script" 2>/dev/null)
        [ "$VERBOSE" = 1 ] && printf '%s\n' "$xout" > "/tmp/a5wt/${name}.xoshiro.diff"
        hx=$(printf '%s\n' "$xout" | grep -cE '^[0-9]+(,[0-9]+)?[acd][0-9]+')
        xv=$(verdict "$hx" "$xbudget"); xtag=${xv#* }
        xcell="$hx (=$xbudget)"
        [ "$xtag" = better ] && xcell="$hx (<$xbudget rebless)"
        [ "$xtag" = REGRESSION ] && xcell="$hx (>$xbudget FAIL)"
    else
        hx=$xbudget; xtag=ok; xcell="(skipped)"
    fi
    rm -f "$stxt"

    # --- combined status -----------------------------------------------------
    if [ "$vtag" = REGRESSION ] || [ "$xtag" = REGRESSION ] || [ "$srfail" = 1 ]; then
        status=FAIL; echo "$name" > "$WORKDIR/$idx.reg"
    elif [ "$vtag" = better ] || [ "$xtag" = better ]; then
        status=OKbetter
    elif [ "$hv" = 0 ] && [ "$hx" = 0 ]; then
        status=MATCH
    else
        status=DIVERGE
    fi
    printf "%-24s %-9s %-12s %-12s %-11s\n" "$name" "$status" "$vcell" "$xcell" "$srcell" > "$row"
}

if [ ! -x "$A5RUN" ]; then
    echo "build the Scarier harness first: make -f Makefile.headless a5run" >&2
    exit 1
fi

printf "%-24s %-9s %-12s %-12s %-11s\n" "GAME" "STATUS" "vanilla" "xoshiro" "saverestore"
printf "%-24s %-9s %-12s %-12s %-11s\n" "----" "------" "-------" "-------" "-----------"

# Fan the games out $JOBS at a time, then print the collected rows in MAP order.
# The pipeline's subshell owns the jobs, so the trailing `wait` must stay inside
# the braces.
#
# Throttle by keeping $JOBS RUNNING jobs, not by launching $JOBS and waiting for
# all of them: game runtimes are wildly skewed (Fortress of Fear is 32s of the
# 138s serial total, Axe of Kolt another 17s, while most games are under 1s), so
# a batch barrier costs the slowest member of every batch.  Measured over the
# real MAP order at -j8: barrier 78s vs 38s here, against a 32s floor set by the
# single longest game.  `jobs -pr` (running only) is the portable-enough probe --
# /bin/sh is bash 3.2 on macOS, which has no `wait -n`.
echo "$MAP" | {
    n=0
    while IFS='|' read -r name game vbudget xbudget; do
        [ -z "$name" ] && continue
        case "$name" in *"$FILTER"*) : ;; *) continue ;; esac
        n=$((n+1))
        while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do sleep 0.1; done
        # </dev/null: the children inherit THIS while-loop's stdin (the MAP
        # pipe); any grandchild that reads stdin (dotnet on an FD_CACHE miss)
        # would silently EAT map rows -- 51 games vanished from one run before
        # this was pinned down.  Row-count assert below backstops it.
        run_one "$(printf '%03d' "$n")" "$name" "$game" "$vbudget" "$xbudget" </dev/null &
    done
    wait
    echo "$n" > "$WORKDIR/expected_rows"
}

for row in "$WORKDIR"/*.row; do
    [ -f "$row" ] && cat "$row"
done

# Backstop: every MAP row must have produced a .row file.  A shortfall means
# rows were lost (e.g. a child ate the MAP pipe) -- fail loudly, because a
# truncated table otherwise reads as "everything ran and passed".
expected=$(cat "$WORKDIR/expected_rows" 2>/dev/null || echo 0)
actual=$(ls "$WORKDIR"/*.row 2>/dev/null | wc -l | tr -d ' ')
if [ "$actual" -ne "$expected" ]; then
    echo
    echo "ERROR: only $actual of $expected MAP rows ran -- suite output is INCOMPLETE" >&2
    exit 1
fi

# Tally + legend.  Every line below is prefixed "#" so that grepping the suite
# output for a status word only ever hits real game rows: the legend names the
# same tokens the rows use, and an unprefixed legend made every "how did the run
# go" grep report phantom MATCH/FAIL hits.  The TOTAL line is the machine-readable
# summary -- prefer it over counting rows.
echo
echo "# TOTAL: $(cat "$WORKDIR"/*.row 2>/dev/null | awk '{print $2}' | sort | uniq -c |
                 awk '{printf "%s%s=%s", (NR>1 ? ", " : ""), $2, $1}')"
echo "#"
echo "# STATUS   MATCH     0 hunks in both modes"
echo "#          DIVERGE   at baseline (see test/A5_WALKTHROUGH_FINDINGS.md)"
echo "#          OKbetter  below baseline in some mode -- re-bless the MAP"
echo "#          FAIL      exceeded a budget, OR the save/restore self-check"
echo "#                    diverged (i.e. a regression)"
echo "# COLUMNS  vanilla     FD stock System.Random"
echo "#          xoshiro     FD_RNG=xoshiro aligned stream (XOSHIRO=0 skips)"
echo "#          saverestore mid-script A5_SAVE_AT round-trip must be"
echo "#                      byte-identical (SAVERESTORE=0 skips)"

# Non-zero exit if any game regressed in either mode (the documented contract).
if ls "$WORKDIR"/*.reg >/dev/null 2>&1; then
    echo
    echo "# REGRESSIONS: $(cat "$WORKDIR"/*.reg | tr '\n' ' ')"
    exit 1
fi
exit 0
