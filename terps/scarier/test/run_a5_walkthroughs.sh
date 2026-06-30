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
#   test/run_a5_walkthroughs.sh [-v] [substring]
#     -v          dump each diff to /tmp/a5wt/<Game>{,.xoshiro}.diff
#     substring   only run scripts whose name contains <substring>
#
# Env: FD_ROOT (default ~/frankendrift), FD_SEED (default 1234) -- see
#      a5_groundtruth.sh.  XOSHIRO=0 skips the (dotnet-heavy) xoshiro pass.

set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
GT="$HERE/test/a5_groundtruth.sh"
GAMES="$HERE/test/adrift5-games"
A5RUN="$HERE/test/a5run_dump"

VERBOSE=0
[ "${1:-}" = "-v" ] && { VERBOSE=1; shift; }
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
# Two budgets per game: VANILLA (FD's stock System.Random) | XOSHIRO (FD patched
# to draw Scarier's xoshiro128** stream, FD_RNG=xoshiro -- RAND-selected text then
# aligns, so the diff is a full every-line check).  Tracking both separates
# RNG-independent logic bugs (same count in both columns) from RNG-stream noise
# (xoshiro lower).  A xoshiro count BELOW vanilla means real RAND divergence
# collapsed once the streams aligned -- the truer conformance number.
#   name | game file | vanilla budget | xoshiro budget
MAP=$(cat <<'EOF'
AchtungPanzer|AchtungPanzer.blorb|0|0
anno1700|Anno1700.blorb|0|0
AxeOfKolt|TheAxeOfKolt.blorb|0|0
SpectreOfCastleCoris|TheSpectreOfCastleCoris.blorb|0|0
StarshipQuest|StarshipQuest.blorb|0|0
MagneticMoon|MagneticMoon.blorb|0|0
RevengeOfTheSpacePirates|RevengeOfTheSpacePirates.blorb|0|0
DieFeuerfaust|DieFeuerfaust.blorb|0|0
LostChildren|TheLostChildren.blorb|0|0
RunBronwynnRun|RunBronwynnRun.blorb|0|0
RtC|RtC.blorb|21|21
TreasureHuntInTheAmazon|TreasureHuntInTheAmazon.blorb|42|42
StoneOfWisdom|StoneOfWisdom.blorb|2|0
GrandpasRanch|Grandpa_ParserComp_V1.blorb|0|0
JacarandaJim|JacarandaJim.blorb|99|0
SixSilverBullets|SixSilverBullets.blorb|20|10
EOF
)

# Set XOSHIRO=0 to skip the (dotnet-heavy) xoshiro pass and check vanilla only.
RUN_XOSHIRO="${XOSHIRO:-1}"

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

REGFILE=$(mktemp)
trap 'rm -f "$REGFILE"' EXIT

printf "%-24s %-9s %-12s %-12s\n" "GAME" "STATUS" "vanilla" "xoshiro"
printf "%-24s %-9s %-12s %-12s\n" "----" "------" "-------" "-------"

echo "$MAP" | while IFS='|' read -r name game vbudget xbudget; do
    [ -z "$name" ] && continue
    case "$name" in *"$FILTER"*) : ;; *) continue ;; esac
    script="$HERE/test/${name}_walkthrough.txt"
    gf="$GAMES/$game"
    [ -f "$script" ] || { printf "%-24s %-9s\n" "$name" "NOSCRIPT"; continue; }
    if [ ! -f "$gf" ]; then
        printf "%-24s %-9s (game file absent: %s)\n" "$name" "SKIP" "$game"
        continue
    fi

    # --- vanilla -------------------------------------------------------------
    # Fast strict golden diff when a golden exists (vanilla transcript, no dotnet).
    golden="$HERE/test/${name}_expected.txt"
    if [ -f "$golden" ] && [ -x "$A5RUN" ]; then
        got=$("$A5RUN" "$gf" "$script" 2>/dev/null | sed 's/[[:space:]]*$//' | cat -s)
        if printf '%s\n' "$got" | diff -q "$golden" - >/dev/null 2>&1; then hv=0; else hv=999; fi
    else
        vout=$("$GT" "$gf" "$script" 2>/dev/null)
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
        xout=$(FD_RNG=xoshiro "$GT" "$gf" "$script" 2>/dev/null)
        [ "$VERBOSE" = 1 ] && printf '%s\n' "$xout" > "/tmp/a5wt/${name}.xoshiro.diff"
        hx=$(printf '%s\n' "$xout" | grep -cE '^[0-9]+(,[0-9]+)?[acd][0-9]+')
        xv=$(verdict "$hx" "$xbudget"); xtag=${xv#* }
        xcell="$hx (=$xbudget)"
        [ "$xtag" = better ] && xcell="$hx (<$xbudget rebless)"
        [ "$xtag" = REGRESSION ] && xcell="$hx (>$xbudget FAIL)"
    else
        hx=$xbudget; xtag=ok; xcell="(skipped)"
    fi

    # --- combined status -----------------------------------------------------
    if [ "$vtag" = REGRESSION ] || [ "$xtag" = REGRESSION ]; then
        status=FAIL; echo "$name" >> "$REGFILE"
    elif [ "$vtag" = better ] || [ "$xtag" = better ]; then
        status=OKbetter
    elif [ "$hv" = 0 ] && [ "$hx" = 0 ]; then
        status=MATCH
    else
        status=DIVERGE
    fi
    printf "%-24s %-9s %-12s %-12s\n" "$name" "$status" "$vcell" "$xcell"
done

echo
echo "MATCH = 0 in both modes; DIVERGE = at baseline (see"
echo "test/A5_WALKTHROUGH_FINDINGS.md); OKbetter = below baseline in some mode"
echo "(re-bless the MAP); FAIL = exceeded a budget (regression).  vanilla = FD's"
echo "stock System.Random; xoshiro = FD_RNG=xoshiro aligned stream (XOSHIRO=0 skips)."

# Non-zero exit if any game regressed in either mode (the documented contract).
if [ -s "$REGFILE" ]; then
    echo
    echo "REGRESSIONS: $(tr '\n' ' ' < "$REGFILE")"
    exit 1
fi
exit 0
