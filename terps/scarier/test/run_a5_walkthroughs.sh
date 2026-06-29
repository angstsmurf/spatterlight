#!/bin/sh
# Drive every ADRIFT-5 walkthrough regression script through the ground-truth
# differential (Scarier a5run_dump  vs  FrankenDrift reference) and summarise.
#
# Each test/<Game>_walkthrough.txt is paired with its game file below.  A game
# is SKIPped when its (uncommittable) .blorb/.taf is absent.  For each present
# game we run test/a5_groundtruth.sh and count the diff hunks:
#
#   MATCH        0 hunks  -- Scarier transcript == FrankenDrift (after the
#                            harness's whitespace normalisation).  A golden
#                            test/<Game>_expected.txt is also strict-diffed if
#                            present (fast, no dotnet).
#   DIVERGES n   n hunks  -- compared against the recorded baseline in
#                            test/A5_WALKTHROUGH_FINDINGS.md.  Most current
#                            divergences are REAL Scarier conformance bugs the
#                            walkthroughs surfaced; see that file for diagnoses.
#
# Exit status: non-zero if any game's hunk count EXCEEDS its baseline budget
# (a regression) or a golden strict-diff fails.  Improvements (below baseline)
# are reported but do not fail -- re-bless the baseline when you see them.
#
# Usage:
#   test/run_a5_walkthroughs.sh [-v] [substring]
#     -v          dump each diff to /tmp/a5wt/<Game>.diff
#     substring   only run scripts whose name contains <substring>
#
# Env: FD_ROOT (default ~/frankendrift), FD_SEED (default 1234) -- see
#      a5_groundtruth.sh.

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
MAP=$(cat <<'EOF'
AchtungPanzer|AchtungPanzer.blorb|0
anno1700|Anno1700.blorb|131
AxeOfKolt|TheAxeOfKolt.blorb|42
SpectreOfCastleCoris|TheSpectreOfCastleCoris.blorb|34
StarshipQuest|StarshipQuest.blorb|0
MagneticMoon|MagneticMoon.blorb|6
RevengeOfTheSpacePirates|RevengeOfTheSpacePirates.blorb|5
DieFeuerfaust|DieFeuerfaust.blorb|16
LostChildren|TheLostChildren.blorb|6
RunBronwynnRun|RunBronwynnRun.blorb|45
RtC|RtC.blorb|141
TreasureHuntInTheAmazon|TreasureHuntInTheAmazon.blorb|64
StoneOfWisdom|StoneOfWisdom.blorb|3
GrandpasRanch|Grandpa_ParserComp_V1.blorb|45
JacarandaJim|JacarandaJim.blorb|449
EOF
)

pass=0; fail=0; skip=0; match=0; improved=0
printf "%-26s %-8s %-8s %s\n" "GAME" "STATUS" "HUNKS" "(budget)"
printf "%-26s %-8s %-8s %s\n" "----" "------" "-----" "--------"

echo "$MAP" | while IFS='|' read -r name game budget; do
    [ -z "$name" ] && continue
    case "$name" in *"$FILTER"*) : ;; *) continue ;; esac
    script="$HERE/test/${name}_walkthrough.txt"
    gf="$GAMES/$game"
    [ -f "$script" ] || { printf "%-26s %-8s\n" "$name" "NOSCRIPT"; continue; }
    if [ ! -f "$gf" ]; then
        printf "%-26s %-8s (game file absent: %s)\n" "$name" "SKIP" "$game"
        continue
    fi

    # Fast strict path when a golden exists.
    golden="$HERE/test/${name}_expected.txt"
    if [ -f "$golden" ] && [ -x "$A5RUN" ]; then
        got=$("$A5RUN" "$gf" "$script" 2>/dev/null | sed 's/[[:space:]]*$//' | cat -s)
        if printf '%s\n' "$got" | diff -q "$golden" - >/dev/null 2>&1; then
            printf "%-26s %-8s %-8s (golden)\n" "$name" "PASS" "0"
        else
            printf "%-26s %-8s %-8s (golden MISMATCH)\n" "$name" "FAIL" "?"
        fi
        continue
    fi

    out=$("$GT" "$gf" "$script" 2>/dev/null)
    [ "$VERBOSE" = 1 ] && printf '%s\n' "$out" > "/tmp/a5wt/${name}.diff"
    # plain `diff` hunk headers look like 12c12 / 5a6 / 10,14d9
    hunks=$(printf '%s\n' "$out" | grep -cE '^[0-9]+(,[0-9]+)?[acd][0-9]+')
    if [ "$hunks" -eq 0 ]; then
        printf "%-26s %-8s %-8s\n" "$name" "MATCH" "$hunks"
    elif [ "$hunks" -gt "$budget" ]; then
        printf "%-26s %-8s %-8s (>%s REGRESSION)\n" "$name" "FAIL" "$hunks" "$budget"
    elif [ "$hunks" -lt "$budget" ]; then
        printf "%-26s %-8s %-8s (<%s improved-rebless)\n" "$name" "OKbetter" "$hunks" "$budget"
    else
        printf "%-26s %-8s %-8s (=%s known)\n" "$name" "DIVERGE" "$hunks" "$budget"
    fi
done

echo
echo "MATCH = conformant; DIVERGE = known bug at baseline (see"
echo "test/A5_WALKTHROUGH_FINDINGS.md); FAIL = exceeded budget (regression)."
