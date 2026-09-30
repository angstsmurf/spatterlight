/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Command dispatch: the priority pass, the standard command tables,
 * run_all_commands() and run_player_input().
 *
 * Split out of scrunner.cpp; see scrunner.h for what the five files share.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"
#include "scrunner.h"



/* Movement commands for the four point compass. */
static scr_commands_t MOVE_COMMANDS_4[] = {
  {"{go {to {the}}} [north/n]", lib_cmd_go_north},
  {"{go {to {the}}} [east/e]", lib_cmd_go_east},
  {"{go {to {the}}} [south/s]", lib_cmd_go_south},
  {"{go {to {the}}} [west/w]", lib_cmd_go_west},
  {"{go {to {the}}} [up/u]", lib_cmd_go_up},
  {"{go {to {the}}} [down/d]", lib_cmd_go_down},
  {"{go {to {the}}} [in/inside/enter]", lib_cmd_go_in},
  {"{go {to {the}}} [out/o/outside/exit]", lib_cmd_go_out},
  {NULL, NULL}
};

/* Movement commands for the eight point compass. */
static scr_commands_t MOVE_COMMANDS_8[] = {
  {"{go {to {the}}} [north/n]", lib_cmd_go_north},
  {"{go {to {the}}} [east/e]", lib_cmd_go_east},
  {"{go {to {the}}} [south/s]", lib_cmd_go_south},
  {"{go {to {the}}} [west/w]", lib_cmd_go_west},
  {"{go {to {the}}} [up/u]", lib_cmd_go_up},
  {"{go {to {the}}} [down/d]", lib_cmd_go_down},
  {"{go {to {the}}} [in/inside/enter]", lib_cmd_go_in},
  {"{go {to {the}}} [out/o/outside/exit]", lib_cmd_go_out},
  {"{go {to {the}}} [northeast/north-east/ne]", lib_cmd_go_northeast},
  {"{go {to {the}}} [southeast/south-east/se]", lib_cmd_go_southeast},
  {"{go {to {the}}} [northwest/north-west/nw]", lib_cmd_go_northwest},
  {"{go {to {the}}} [southwest/south-west/sw]", lib_cmd_go_southwest},
  {NULL, NULL}
};

/* "Priority" library commands, may take precedence over the game. */
static scr_commands_t PRIORITY_COMMANDS[] = {

  /* Acquisition of and disposal of inventory.
   *
   * Bare `pick %object%` is a genuine take synonym in both real Runners:
   * run400 answers `pick pretty flowers` (Professor Von Witt) with "You take
   * the pretty flowers from the window box.", and run390 answers `pick boat`
   * (Marooned v1) with "You can't take the wrecked boat."  It reaches the
   * whole *object* take family -- `pick all`, `pick X from Y`, `pick all
   * from Y` -- but NOT the NPC handlers: run400 gives `pick burton` "Take
   * what?" where `take burton` is answered "...would appreciate being
   * handled.", and `pick all from burton` "I don't understand where you want
   * to get things from." where `take all from burton` again gets the
   * "handled" reply.  `pick up X from Y` is not a take-from either ("Take
   * what?").  All measured live 2026-08-18; Professor Von Witt's own bundled
   * walkthrough depends on the bare-`pick` form.  `pick` is also the only
   * one of these that survives the check described above `close %object%`
   * below: it appears as a literal in all four Runner listings, where
   * `grab` -- once listed here -- appears in none. */
  {"[[get/take/remove/extract/pick] [all/everything] from/empty] %object%",
   lib_cmd_take_all_from},
  {"[[get/take/remove/extract/pick] [all/everything] from/empty] %object%"
   " [[except/but] {for}/apart from] %text%",
   lib_cmd_take_from_except_multiple},
  {"[get/take/remove/extract/pick] [all/everything]"
   " [[except/but] {for}/apart from] %text% from %object%",
   lib_cmd_take_from_except_multiple},
  {"[get/take/remove/extract/pick] %text% from %object%",
   lib_cmd_take_from_multiple},
  {"[get/take] [all/everything] from %character%", lib_cmd_take_all_from_npc},
  {"[get/take] [all/everything] from %character%"
   " [[except/but] {for}/apart from] %text%",
   lib_cmd_take_from_npc_except_multiple},
  {"[get/take] [all/everything]"
   " [[except/but] {for}/apart from] %text% from %character%",
   lib_cmd_take_from_npc_except_multiple},
  {"[get/take] %text% from %character%", lib_cmd_take_from_npc_multiple},
  {"[[get/take/pick up/pick] [all/everything]/pick [all/everything] up]",
   lib_cmd_take_all},
  {"[get/take/pick up/pick] [all/everything]"
   " [[except/but] {for}/apart from] %text%",
   lib_cmd_take_except_multiple},
  /* `pick %text% up` before the bare-`pick` catch-all: a failed object parse
   * in lib_cmd_take_multiple falls through, but keep `pick flowers up` from
   * ever being read as `pick "flowers up"` in the first place. */
  {"pick %text% up", lib_cmd_take_multiple},
  {"[get/take/pick up/pick] %text%", lib_cmd_take_multiple},
  /*
   * "drop X in Y" and "drop X on Y" are Adrift's put handlers wearing a
   * different verb: `drop wallet in bin` answers "You put your wallet inside
   * the rubbish bin.", and `drop wallet on bin` gives the put-on refusal
   * "You can't put anything onto the rubbish bin!".  "put down" and "all"
   * behave the same way.  Verified live against run400.exe with Ticket to No
   * Where, whose walkthrough disposes of five bits of litter with
   * "drop <litter> in bin".  These have to precede the plain drop patterns
   * below, whose %text% would otherwise swallow the "in <container>" tail and
   * leave the player with "Drop what?".
   */
  /*
   * 4.0 names the CONTAINER of a put/drop line before its object, and
   * three of name_object's exits speak for a container that is not there
   * or not a container -- "I don't understand what you want to put things
   * inside.", "Where do you want to put the desk?", "You can't put anything
   * onto the box!" -- before any of the rows below could name the object;
   * see lib_cmd_put_container_400().  The refusal defers from here like
   * the rows below and prints from the STANDARD_COMMANDS twin.
   */
  {"put *", lib_cmd_put_container_400},
  {"[drop/put down] *", lib_cmd_put_container_400},
  {"[drop/put down] [all/everything] [in/into/inside {of}] %object%",
   lib_cmd_put_all_in},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%"
   " [in/into/inside {of}] %object%", lib_cmd_put_in_except_multiple},
  {"[drop/put down] %text% [in/into/inside {of}] %object%",
   lib_cmd_put_in_multiple},
  {"[drop/put down] [all/everything] [on/onto/on top of] %object%",
   lib_cmd_put_all_on},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%"
   " [on/onto/on top of] %object%", lib_cmd_put_on_except_multiple},
  {"[drop/put down] %text% [on/onto/on top of] %object%",
   lib_cmd_put_on_multiple},
  /*
   * The plain "put" spellings of the same handlers.  Priority placement is
   * live-verified (run400, 2026-08-02, probe FM7 / TheADRIFTProject): a
   * completable put beats a matched-but-failing task, so these must run
   * before the loud-fail task pass; the refusal cases defer from here (see
   * run_priority_commands) and print from the STANDARD_COMMANDS duplicates.
   */
  {"put [all/everything] [in/into/inside {of}] %object%", lib_cmd_put_all_in},
  {"put [all/everything] [[except/but] {for}/apart from] %text%"
   " [in/into/inside {of}] %object%", lib_cmd_put_in_except_multiple},
  {"put %text% [in/into/inside {of}] %object%", lib_cmd_put_in_multiple},
  {"put [all/everything] [on/onto/on top of] %object%", lib_cmd_put_all_on},
  {"put [all/everything] [[except/but] {for}/apart from] %text%"
   " [on/onto/on top of] %object%", lib_cmd_put_on_except_multiple},
  {"put %text% [on/onto/on top of] %object%", lib_cmd_put_on_multiple},
  /* 3.9's "can't put anything inside/onto that!"; see lib_put_that_390(). */
  {"put %text% [in/into/inside {of}] *", lib_cmd_put_in_that_390},
  {"put %text% [on/onto/on top of] *", lib_cmd_put_on_that_390},
  {"[[drop/put down] [all/everything]/put [all/everything] down]",
   lib_cmd_drop_all},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%",
   lib_cmd_drop_except_multiple},
  {"[drop/put down] %text%", lib_cmd_drop_multiple},
  {"put %text% down", lib_cmd_drop_multiple},
  /* Below 4.0 `leave` is a third spelling of `drop`; see
   * lib_cmd_leave_all_pre400().  The all-row has to come first, as "leave
   * all" matches the named row with %text% = "all" as well. */
  {"leave [all/everything]", lib_cmd_leave_all_pre400},
  {"leave [all/everything] [[except/but] {for}/apart from] %text%",
   lib_cmd_leave_except_multiple_pre400},
  {"leave %text%", lib_cmd_leave_multiple_pre400},

  /*
   * Inventory display.  Treated as a priority system command so that it is
   * not pre-empted by a game task whose command matches "i"/"inventory" but
   * whose restrictions fail (those tasks print their fail message in a later
   * pass).  This matches the Adrift runner -- e.g. "i" in Lair of the
   * CyberCow lists the items you hold rather than showing the failing task's
   * "...lost it all in the well" message.  A game task with *passing*
   * restrictions still overrides this, as those run before priority commands.
   */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[inventory/inv]", lib_cmd_inventory},
#else
  {"[inventory/inv/i]", lib_cmd_inventory},
#endif
  {NULL, NULL}
};

/* Standard library commands, other than movement and priority above. */
static scr_commands_t STANDARD_COMMANDS[] = {

  /* Inventory, and general investigation of surroundings. */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[inventory/inv]", lib_cmd_inventory},
  {"[ex/exam/examine/look {at}] {{the} [room/location]}",
   lib_cmd_look_typed},
  {"[ex/exam/examine/look {at/in}] %object%",
   lib_cmd_examine_object},
  {"[ex/exam/examine/look {at}] %character%",
   lib_cmd_examine_npc},
  {"[ex/exam/examine/look {at}] [me/self/myself]",
   lib_cmd_examine_self},
  {"[ex/exam/examine/look {at}] all", lib_cmd_examine_all},
#else
  {"[inventory/inv/i]", lib_cmd_inventory},
  {"[x/ex/exam/examine/l/look {at}] {{the} [room/location]}",
   lib_cmd_look_typed},
  {"[x/ex/exam/examine/look {at/in}] %object%",
   lib_cmd_examine_object},
  {"[x/ex/exam/examine/look {at}] %character%",
   lib_cmd_examine_npc},
  {"[x/ex/exam/examine/look {at}] [me/self/myself]",
   lib_cmd_examine_self},
  {"[x/ex/exam/examine/look {at}] all", lib_cmd_examine_all},
#endif

  /* Attempted acquisition of and disposal of NPCs. */
  {"[get/take/pick up] %character%", lib_cmd_take_npc},
  {"pick %character% up", lib_cmd_take_npc},

  /*
   * Manipulating selected objects.  The put-family rows repeat their
   * PRIORITY_COMMANDS twins (drop spellings included): the priority pass
   * defers the refusal cases so a matched task's fail message can claim the
   * input first, and this second appearance prints the refusal when no task
   * did.
   */
  {"put *", lib_cmd_put_container_400},
  {"[drop/put down] *", lib_cmd_put_container_400},
  {"put [all/everything] [in/into/inside {of}] %object%", lib_cmd_put_all_in},
  {"put [all/everything] [[except/but] {for}/apart from] %text%"
   " [in/into/inside {of}] %object%", lib_cmd_put_in_except_multiple},
  {"put %text% [in/into/inside {of}] %object%", lib_cmd_put_in_multiple},
  {"put [all/everything] [on/onto/on top of] %object%", lib_cmd_put_all_on},
  {"put [all/everything] [[except/but] {for}/apart from] %text%"
   " [on/onto/on top of] %object%", lib_cmd_put_on_except_multiple},
  {"put %text% [on/onto/on top of] %object%", lib_cmd_put_on_multiple},
  {"[drop/put down] [all/everything] [in/into/inside {of}] %object%",
   lib_cmd_put_all_in},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%"
   " [in/into/inside {of}] %object%", lib_cmd_put_in_except_multiple},
  {"[drop/put down] %text% [in/into/inside {of}] %object%",
   lib_cmd_put_in_multiple},
  {"[drop/put down] [all/everything] [on/onto/on top of] %object%",
   lib_cmd_put_all_on},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%"
   " [on/onto/on top of] %object%", lib_cmd_put_on_except_multiple},
  {"[drop/put down] %text% [on/onto/on top of] %object%",
   lib_cmd_put_on_multiple},
  {"open %object%", lib_cmd_open_object},
  /*
   * DO NOT ADD VERB SYNONYMS BY DIFFING RESPONSES AGAINST A LIVE RUNNER.
   * Twelve were added that way on 2026-08-18 -- grab, inspect, check, shut,
   * hand, consume, slay, ignite, shatter, crack, swallow, yank -- each
   * "confirmed" because it drew the canonical verb's unmatched-object
   * response rather than the catch-all.  Every one was a false positive:
   * the probe game, easter.taf, ships its own 100-entry SYNONYM table
   * (`SCR_DUMP_TASKS=1` prints it) that rewrites all of them before the
   * parser ever sees them.  None of the twelve occurs as a literal --
   * anywhere, in any casing -- in run370.bas, run380.bas, run390's
   * Form1.frm or run400.bas, while every verb that survives here does.
   *
   * The listings are the authority: grep the four of them in
   * ~/Adrift_decompile, or use `index/verbs.py -w <word>`, which lists the
   * matcher literals per Runner.  A live probe can only confirm what the
   * listing already shows, and only on a game with no SYNONYM table of its
   * own (The Town of Azra has none, easter.taf and The Cellar do).
   */
  {"close %object%", lib_cmd_close_object},
  {"unlock %object% with %text%", lib_cmd_unlock_object_with},
  {"lock %object% with %text%", lib_cmd_lock_object_with},
  {"unlock %object%", lib_cmd_unlock_object},
  {"lock %object%", lib_cmd_lock_object},
  {"read %object%", lib_cmd_read_object},
  {"read *", lib_cmd_read_other},
  {"give %object% to %character%", lib_cmd_give_object_npc},
  /*
   * The Runner's give is word-order free: the character handler tests
   * c("give"), a present NPC, and then every object whose name appears
   * anywhere in the line (run400 48022F-480384, run390 45A0BA, run380
   * 440E8C, run370 438F79), so `give cathy diary` and `give diary cathy`
   * both offer the diary.  House (4.00) runner_probes/house.run400.b.txt
   * 2026-09-06: `give cathy diary` gets the same reply as `give diary to
   * cathy`.  Scarier used to fall to "Give the diary to who?".
   */
  {"give %character% %object%", lib_cmd_give_object_npc},
  {"give %object% %character%", lib_cmd_give_object_npc},
  {"sit {down/up} [on/in] %object%", lib_cmd_sit_on_object},
  {"stand {up/down} [on/in] %object%", lib_cmd_stand_on_object},
  {"[lie/lay] on %object%", lib_cmd_lie_on_object},
  /*
   * `get up` is a `stand` synonym in every Runner; `get off` and `get down`
   * are `stand`-off-a-thing synonyms that arrived in 3.9.  The sitstand
   * proc tests `c("get off") Or c("get down")` (run400 loc_46B6E4, run390
   * loc_44432x); neither string appears anywhere in run370/run380.
   * Measured live on the one file microwaveman.taf -- a 3.80 game -- run
   * under three Runners, which is the way to see a *library* change rather
   * than a file-format one:
   *
   *              run380              run390                  run400
   *   get up    "already standing!"  "already standing!"   "already standing!"
   *   get down  "Take what?"         "not standing on ..."  "not standing on ..."
   *   get off   "Take what?"         "not standing on ..."  "not standing on ..."
   *
   * Deliberate deviation: Scarier takes all three at every version, with
   * the 3.9 behaviour, rather than the pre-3.9 "Take what?" (see
   * lib_cmd_get_off()).
   *
   * `get on %object%` arrived at the same time; it is a `stand on` synonym
   * that refuses differently.  See lib_cmd_get_on_object().
   */
  {"get on %object%", lib_cmd_get_on_object},
  {"get {down/up} off %object%", lib_cmd_get_off_object},
  {"get off", lib_cmd_get_off},
  {"get down", lib_cmd_get_down},
  {"get up", lib_cmd_stand_on_floor},
  {"sit {down/up} {[on/in] {the} [ground/floor]}", lib_cmd_sit_on_floor},
  {"stand {up/down} {[on/in] {the} [ground/floor]}", lib_cmd_stand_on_floor},
  {"[lie/lay] {down/up} {[on/in] {the} [ground/floor]}", lib_cmd_lie_on_floor},
  /* 3.7's scope-free sitstand loops; see lib_cmd_sit_scan_370(). */
  {"sit {down/up} [on/in] *", lib_cmd_sit_scan_370},
  {"stand {up/down} [on/in] *", lib_cmd_stand_scan_370},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_lie_scan_370},
  {"eat %object%", lib_cmd_eat_object},

  /* Dressing up, and dressing down. */
  {"[[wear/put on/don] [all/everything]/put [all/everything] on]",
   lib_cmd_wear_all},
  {"[wear/put on/don] [all/everything] [[except/but] {for}/apart from] %text%",
   lib_cmd_wear_except_multiple},
  {"[wear/put on/don] %text%", lib_cmd_wear_multiple},
  {"put %text% on", lib_cmd_wear_multiple},
  {"[[remove/take off/doff] [all/everything]/take [all/everything] off/strip]",
   lib_cmd_remove_all},
  {"[remove/take off/doff] [all/everything]"
   " [[except/but] {for}/apart from] %text%",
   lib_cmd_remove_except_multiple},
  {"[remove/take off/doff] %text%", lib_cmd_remove_multiple},
  {"take %text% off", lib_cmd_remove_multiple},

  /* Selected NPC interactions and conversation. */
  {"ask %character% about %text%", lib_cmd_ask_npc_about},

  /*
   * `talk to %character% about %text%` is the same conversation branch as
   * `ask`: every Runner guards it with `c("ask") Or c("talk to")` (run370
   * loc_4387F4, run380 loc_440683, run390 loc_4597F2, run400 loc_47F8F7).
   * `speak to` is not in that list at any version, so it only ever reaches
   * the ask-format hint below, even where a topic would have matched.  A
   * `talk to` with no matching topic lands on the hint too -- see
   * lib_ask_npc_about().
   *
   * The hint itself is the per-character pass's `talk`/`speak` branch, and
   * its guard is where the versions part: 3.7 and 3.8 match the bare words
   * (run370 loc_438748, run380 loc_4405D7), 3.9 and 4.0 require the "to"
   * (run390 loc_45973D, run400 loc_47F84A).  Hence the second row's gate.
   * Both rows sit ahead of `talk *` further down, which is the generaltasks
   * rabblings line -- the fall-through 3.9/4.0 leave for a bare `talk bob`.
   * See lib_cmd_talk_to_npc().
   */
  {"talk to %character% about %text%", lib_cmd_talk_to_npc_about},
  {"[talk/speak] to %character% *", lib_cmd_talk_to_npc},
  {"[talk/speak] %character% *", lib_cmd_talk_to_npc_pre_390},
  {"[attack/kick] %character% with %object%", lib_cmd_attack_npc_with},
  {"chop %character% with %object%", lib_cmd_chop_npc_with},
  {"cut %character% with %object%", lib_cmd_cut_npc_with},
  {"hit %character% with %object%", lib_cmd_hit_npc_with},
  {"shoot %character% with %object%", lib_cmd_shoot_npc_with},
  {"stab %character% with %object%", lib_cmd_stab_npc_with},
  {"throw %object% at %character%", lib_cmd_throw_npc_with},
  {"kill %character% with %object%", lib_cmd_kill_npc_with},
  {"fight %character% with %object%", lib_cmd_fight_npc_with},
  /*
   * `slap` is a pre-parse rewrite to `hit`, not a grammar verb, and it is
   * space-bounded in 4.0 so a command-initial `slap` never fires there.
   * `smack` is in no Runner at all.  See lib_cmd_slap_*() for the sites.
   */
  {"[attack/kick] %character%", lib_cmd_attack_npc},
  {"slap %character% with %object%", lib_cmd_slap_npc_with},
  {"slap %character%", lib_cmd_slap_npc},
  {"chop %character%", lib_cmd_chop_npc},
  {"cut %character%", lib_cmd_cut_npc},
  {"shoot %character%", lib_cmd_shoot_npc},
  {"stab %character%", lib_cmd_stab_npc},
  {"kill %character%", lib_cmd_kill_npc},
  {"fight %character%", lib_cmd_fight_npc},
  /*
   * One line naming several NPCs: dobattle strikes every one of them, and
   * a battle verb naming no NPC at all asks "Who do you want to attack?";
   * see lib_battle_attack_many().  `throw` is one of var_90's verbs too
   * (47E9EF-47EADB): les_feux T115 `throw grappin on rocher` asks Who.
   */
  {"[attack/kick/fight/kill/chop/cut/hit/shoot/stab/throw] %text% with %object%",
   lib_cmd_attack_npcs_with},
  {"[attack/kick/fight/kill/chop/cut/hit/shoot/stab/throw] %text%",
   lib_cmd_attack_npcs},
  /* A bare battle verb asks too, ahead of `kick`'s and `hit`'s "what?". */
  {"[attack/kick/fight/kill/chop/cut/hit/shoot/stab/throw]",
   lib_cmd_attack_npcs},

  /* More movement, waiting, and miscellaneous administrative commands. */
  /*
   * `go` and `enter` are the two verbs generaltasks answers with a nudge back
   * to the compass -- run370 loc_43DD8B / loc_43DDB4, run380 loc_44481C /
   * loc_444845, run390 loc_45DF66 / loc_45DF83, run400 loc_48936C /
   * loc_489383.  The movement table above has already taken every bare
   * direction word, `enter` and `exit` among them, so what is left here is a
   * bare `go` and an `enter` with something attached.  See
   * lib_cmd_just_a_direction().
   *
   * `goto`/`go to` keep their own rows below: they are the Runner's
   * gotoplace(), which exits at once on a bare `go`, `goto` or `go to`
   * (run390 loc_43C7B0, run400 loc_464998) and so leaves those to the nudge
   * as well.
   */
  {"go", lib_cmd_just_a_direction},
  {"go to", lib_cmd_just_a_direction},
  {"enter *", lib_cmd_just_a_direction},

  /*
   * gotoplace() itself, which tests the whole line: "goto" anywhere, or a
   * line starting "go " (3.9+, and a deviation below; see
   * lib_goto_line_enters()) or "go to" (3.7, 3.8).  See lib_cmd_go_place().
   * What it leaves is a bare `goto`, which the Runner answers with
   * DontUnderstand (run380x runner_probes/goto.run380.b.rtf, run390x
   * runner_probes/goto.run390.b.txt, run400x
   * runner_probes/goto.run400.txt).  Deliberate deviation: Scarier lists
   * the exits, as SCARE did.
   */
  {"*", lib_cmd_go_place},
  {"goto", lib_cmd_print_room_exits},
  {"[exits/directions/where]", lib_cmd_print_room_exits},
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[wait] %number%", lib_cmd_wait_number},
  {"[wait]", lib_cmd_wait},
#else
  /*
   * `z` only entered the Runner vocabulary at 3.90 (index/verbs.py; cave.taf
   * run380 live 2026-08-31 answers it "Say again?").  Deliberate deviation:
   * Scarier waits on it at every version.  A game's own `z` task still wins,
   * tasks being matched first, and no 3.7/3.8 corpus game has one.
   */
  {"[wait/z] %number%", lib_cmd_wait_number},
  {"[wait/z]", lib_cmd_wait},
#endif
  {"save", lib_cmd_save},
  {"[restore/load]", lib_cmd_restore},
  {"restart", lib_cmd_restart},
  /*
   * The Runner takes six words for "do that again", tested as a set on the
   * whole input line before anything else looks at it:
   *
   *   00089FE2  If s = "!!" Or s = "again" Or s = "last" Or s = "previous"
   *                Or s = "!" Or s = "g" Then
   *
   * (mdlSpreadTheLoad.Sub_20_62 in run400).  All six measured live in run400
   * on easter.taf -- "previous" repeats an examine, "!!" repeats it again --
   * with one wrinkle worth knowing: the history scan that follows skips only
   * entries *identical* to the word just typed, so "last" straight after
   * "previous" repeats the literal word "previous" and is refused.  "!" and
   * "!!" are Scarier's own history shorthand as well, and keep the richer
   * SCARE forms ("!5", "!take") the Runner has no equivalent of.
   */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[again/last/previous]", lib_cmd_again},
#else
  {"[again/g/last/previous]", lib_cmd_again},
#endif
  /*
   * `redo` is ours; `!` is the Runner's, so only the word goes away under
   * SCARIER_NO_ABBREVIATIONS.  The `!5` / `!take` arguments stay either way --
   * the Runner has no equivalent, but it has no bare `!`-plus-argument form to
   * clash with either, so they cost nothing.
   */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"!%number%", lib_cmd_redo_number},
  {"!%text%", lib_cmd_redo_text},
  {"!", lib_cmd_redo_last},
#else
  {"[redo /!]%number%", lib_cmd_redo_number},
  {"[redo /!]%text%", lib_cmd_redo_text},
  {"[redo/!]", lib_cmd_redo_last},
#endif
  /*
   * `bye` and `end` are exact synonyms of `quit` in every Runner -- one
   * three-way whole-line test that unloads the form and answers "I'm so glad
   * you said no..." if the confirmation is declined (run400 loc_48AA85-48AAC9,
   * run370 loc_43C06B).  `q` is ours, not theirs; see the audit note above.
   */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[quit/bye/end]", lib_cmd_quit},
#else
  {"[quit/q/bye/end]", lib_cmd_quit},
#endif
  {"endgame", lib_cmd_endgame},
  /*
   * Audited 2026-09-07 against the four Runner constant pools and the 426-game
   * v4 corpus (notes/WINE-TRANSCRIPTS-TODO.md, "Audited 2026-09-07"): besides
   * the `stats` row below, `hints`, `q`, `brief`, `verbose`, `notify`,
   * `notification`, `redo`, `hist`, `gpl`, `license` and `statusline` are all
   * SCARE inventions no Runner accepts (the Runner words are `hint`, `quit`,
   * `history`).  None of them diverges today: a game task is matched before
   * run_standard_commands(), so any task carrying COMPLETE text wins and the
   * invention never runs -- only a *silent* task leaves the gap that `stats`
   * fell into.  `hints` is the widest exposure (224 tasks in 11 games).
   *
   * All eleven are compiled out by SCARIER_NO_ABBREVIATIONS, alongside the
   * `g`/`i`/`z` shorthands, so that build offers a game exactly the meta
   * vocabulary the Runner does and can never steal a line from a task.  The
   * default build keeps them: they are useful, and the corpus says they are
   * harmless.
   *
   * `turns`/`undo` are 3.80+ and `version` 3.90+ in the Runner; left ungated
   * here because no 3.70/3.80 corpus game names them.
   */
  {"turns", lib_cmd_turns},
  {"score", lib_cmd_score},
  {"undo", lib_cmd_undo},
  /* `past` is the Runner's own synonym of `history` -- the two are one
   * whole-line test in all four (run370 loc_43BA2A, run380 loc_44228D,
   * run400 loc_48A51A).  The `%number%` form is a SCARE extension on top of
   * both, and `hist` is ours alone; see the audit note below. */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[history/past] %number%", lib_cmd_history_number},
  {"[history/past]", lib_cmd_history},
  {"hint", lib_cmd_hints},
#else
  {"[hist/history/past] %number%", lib_cmd_history_number},
  {"[hist/history/past]", lib_cmd_history},
  {"[hint/hints]", lib_cmd_hints},
  {"verbose", lib_cmd_verbose},
  {"brief", lib_cmd_brief},
  {"[notify/notification] %text%", lib_cmd_notify_on_off},
  {"[notify/notification]", lib_cmd_notify},
#endif
  {"time", lib_cmd_time},
  {"date", lib_cmd_date},
  {"[help/commands]", lib_cmd_help},
#ifndef SCARIER_NO_ABBREVIATIONS
  {"[gpl/license]", lib_cmd_license},
#endif
  {"[about/info/information/author]", lib_cmd_information},
  {"[clear/cls/clr]", lib_cmd_clear},
#ifndef SCARIER_NO_ABBREVIATIONS
  {"statusline", lib_cmd_statusline},
#endif
  {"[control panel/control-panel/control/panel]", lib_cmd_control_panel},
  /*
   * `stats` was a SCARE invention: the string does not occur in ANY of the
   * four Runner binaries, so it stole `suburbanprodigy3` T31 from the game's
   * own task.  Dropped 2026-09-07; see test/adrift4/notes/
   * WINE-TRANSCRIPTS-TODO.md "Ported 2026-09-07: `stats` is not a Runner
   * command".  `status` itself is 3.90/4.00 only and lives solely inside the
   * Battle System handler (run400 47DCA1, run390 44C510), which is why both
   * handlers below are gated on battle_is_enabled().
   */
  {"status %character%", lib_cmd_status_npc},
  {"[status]", lib_cmd_status_player},
  {"wield %object%", lib_cmd_wield},
  {"wield %text%", lib_cmd_wield_other},
  {"wield", lib_cmd_wield_other},
  {"version", lib_cmd_version},

  {"[locate/where {is/are}/find] %object%", lib_cmd_locate_object},
  {"[locate/where {is}/find] %character%", lib_cmd_locate_npc},

  {"[count/num]", lib_cmd_count},

  {NULL, NULL}
};

/*
 * Standard response commands; no real action, just output.  A separate
 * table because run_standard_commands() only reaches it after the table
 * above has had both its positional and its containment pass: the Runner's
 * per-verb co() finds an object named anywhere in the line before any
 * "You see no such thing." can fire, so `x silver key` must examine "a key"
 * (man_overboard, 4.00, run400 transcript line 170) rather than fall to
 * lib_cmd_examine_other.
 */
/*
 * Two catch-alls that are not part of the bucket below, even though they look
 * like catch-alls of the same kind.  Both belong to named handlers that
 * run390's generaltasks() calls ABOVE the out-of-room task refusal at
 * loc_45FFE8, so both outrank "You can't do that here!" where the therest()
 * catch-alls lose to it.
 *
 * Take is the first.  In run390 the take code is a dedicated handler with its
 * own "Take what?" and its own "You can't get anything from that.", not
 * therest()'s.  ALEXIS.TAF under
 * run390 measures the difference: `take pot`, `take jacket`, `take coins`,
 * `take ornate key` and four more all answer "Take what?" on turns where a
 * task matching the line exists in another room and `give stones to larnt` on
 * the same row answers "You can't do that here!" instead of therest()'s "Give
 * what?" (runner_probes/alexis_worn_cube.run390.txt).  Hence a table of its
 * own, run at the end of run_standard_verb_commands().
 */
/*
 * run390's insides() -- the whole "take X from Y" handler -- answers before
 * the undress verb and before the generic catch-alls, so its two "nothing
 * answers to the noun after from" replies need a table above STANDARD_
 * COMMANDS rather than a row inside it: `remove coin from zzzz` is run390's
 * "Get the coin from what?" and run400's "I don't understand where you want
 * to get things from.", never scarier's "You are not wearing the coin!"
 * (p39DARK/p4TFROM, runner_probes/tfrom.run400.feed2.txt,
 * runner_probes/dark.run390.feed5.txt, 2026-09-10).  Both rows end in a
 * wildcard because the container slot resolved nothing; the %object% rows for
 * the same shapes are up in PRIORITY_COMMANDS and have already declined by
 * here.  See lib_cmd_take_from_nowhere().
 */
static scr_commands_t STANDARD_TAKE_FROM_COMMANDS[] = {
  {"[[get/take/remove/extract/pick] [all/everything] from/empty] *",
   lib_cmd_take_from_nowhere_all},
  {"[get/take/remove/extract/pick] %text% from *", lib_cmd_take_from_nowhere},
  {NULL, NULL}
};

/*
 * The pre-4.0 put's answers for a container slot that named nothing.  Unlike
 * the take-from pair above, these belong BELOW STANDARD_COMMANDS: a line
 * whose container is a real object is the ordinary put rows' to answer, and
 * only what they decline -- "put lamp in box" with the box a room away, "put
 * coin in me" -- reaches here.  See lib_cmd_put_in_nowhere() and its
 * surface twin lib_cmd_put_on_nowhere().
 */
static scr_commands_t STANDARD_PUT_COMMANDS[] = {
  /* The clauseless spellings, which none of the rows below can match; see
   * lib_cmd_put_no_clause_pre400(). */
  {"put [in/into/inside {of}] *", lib_cmd_put_no_clause_pre400},
  {"put [on/onto/on top of] *", lib_cmd_put_no_clause_pre400},
  {"put %text% [in/into/inside {of}] *", lib_cmd_put_in_nowhere},
  {"[drop/put down] %text% [in/into/inside {of}] *", lib_cmd_put_in_nowhere},
  {"put %text% [on/onto/on top of] *", lib_cmd_put_on_nowhere},
  {"[drop/put down] %text% [on/onto/on top of] *", lib_cmd_put_on_nowhere},
  {NULL, NULL}
};

static scr_commands_t STANDARD_ABOVE_REFUSAL_COMMANDS[] = {
  /*
   * The 4.0 named take for a noun that names only objects the player has
   * seen elsewhere; it declines to anything else, leaving "Take what?" to
   * the catch-all below it.  See lib_cmd_take_absent().
   */
  {"[get/take/pick up/pick] %object%", lib_cmd_take_absent},
  {"[get/take/pick up/pick] *", lib_cmd_get_what},
  /*
   * run380's drops() (438FF0) writes "You don't have <Prefix> <Short>!" for a
   * named object it cannot drop before it calls tasks(), and the room refusal
   * only fills an empty message -- so below 3.9 it outranks "You can't do
   * that here.".  See lib_cmd_drop_absent_pre390().
   */
  {"[drop/put down] *", lib_cmd_drop_absent_pre390},
  {"leave *", lib_cmd_leave_absent_pre400},
  {NULL, NULL}
};

static scr_commands_t STANDARD_FALLBACK_COMMANDS[] = {
  /*
   * The two 4.0-only absent-object rows sit directly above the catch-alls
   * they pre-empt, because the Runner's clause fires only when nothing
   * else in the turn has spoken; see lib_absent_seen_object().
   */
  {"open %object%", lib_cmd_open_absent},
  {"open *", lib_cmd_open_other},
  {"close %object%", lib_cmd_close_absent},
  {"close *", lib_cmd_close_other},
  {"give *", lib_cmd_verb_absent_400},
  {"give %object% *", lib_cmd_give_object},
  {"give *", lib_cmd_give_what},
  {"lock %object% *", lib_cmd_lock_object_pre_400},
  {"lock %text%", lib_cmd_lock_other},
  {"lock", lib_cmd_lock_what},
  {"unlock %object% *", lib_cmd_unlock_object_pre_400},
  {"unlock %text%", lib_cmd_unlock_other},
  {"unlock", lib_cmd_unlock_what},
  {"sit {down/up} [on/in] *", lib_cmd_verb_absent_400},
  {"sit {down/up} [on/in] *", lib_cmd_sit_other},
  {"stand {up/down} [on/in] *", lib_cmd_verb_absent_400},
  {"stand {up/down} [on/in] *", lib_cmd_stand_other},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_verb_absent_400},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_lie_other},
  {"[remove/take off/doff] *", lib_cmd_remove_what},
  {"[drop/put down] *", lib_cmd_drop_what},
  {"leave *", lib_cmd_leave_what_pre400},
  {"[wear/put on/don] *", lib_cmd_wear_what},
  /* 4.0 only, below the wear row so `put on X` keeps its own refusal; see
   * lib_cmd_put_unclear(). */
  {"put *", lib_cmd_put_unclear},
  /*
   * The swearing list is version-split at both ends.  `piss` has been in it
   * since 3.7 and was simply missed here; `bugger` arrived in 3.9 and is not
   * in the 3.7/3.8 Runners; `bloody` went the other way and was dropped in
   * 4.0.  Read out of generaltasks in all four decompiled Runners with
   * `index/verbs.py -w <word>` (~/Adrift_decompile), which lists every
   * literal handed to the parser's whole-word matchers.
   */
  {"[shit/fuck/bastard/cunt/crap/hell/shag/bollocks/bollox/piss] *",
   lib_cmd_profanity},
  {"bugger *", lib_cmd_profanity_390},
  {"bloody *", lib_cmd_profanity_pre_400},
  {"[x/ex/exam/examine/look {at}] %object%", lib_cmd_examine_absent},
  {"[x/ex/exam/examine/look {at}] *", lib_cmd_examine_other},
  {"[locate/where {is/are}/find] *", lib_cmd_locate_other},
  {"[cp/mv/ln/ls] *", lib_cmd_unix_like},
  {"dir *", lib_cmd_dos_like},
  {"ask %character% *", lib_cmd_ask_npc},
  {"ask %object% *", lib_cmd_ask_object},
  {"ask * about *", lib_cmd_ask_about_nothing},
  {"ask *", lib_cmd_ask_other},
  {"block %object% *", lib_cmd_block_object},
  {"block %text%", lib_cmd_block_other},
  {"block", lib_cmd_block_what},
  {"[break/destroy/smash] %object% *", lib_cmd_break_object},
  {"[break/destroy/smash] %object% *", lib_cmd_break_absent},
  {"[break/destroy/smash] %text%", lib_cmd_break_other},
  {"break", lib_cmd_break_what},
  {"destroy", lib_cmd_destroy_what},
  {"smash", lib_cmd_smash_what},
  {"buy %object% *", lib_cmd_buy_object},
  {"buy %object% *", lib_cmd_buy_absent},
  {"buy %text%", lib_cmd_buy_other},
  {"buy", lib_cmd_buy_what},
  {"clean %object% *", lib_cmd_clean_object},
  /* clean (488F02), wash (488FFC), cut (489041), shake (48950E), climb
   * (4898F5) and sell (489A3D) are therest arms below the 4887A0 clause too. */
  {"clean %text%", lib_cmd_verb_absent_400},
  {"clean %text%", lib_cmd_clean_other},
  {"clean", lib_cmd_clean_what},
  {"climb %object% *", lib_cmd_climb_object},
  {"climb %text%", lib_cmd_verb_absent_400},
  {"climb %text%", lib_cmd_climb_other},
  {"climb", lib_cmd_climb_what},
  {"cry *", lib_cmd_cry},
  {"cut %object% *", lib_cmd_cut_object},
  {"cut %text%", lib_cmd_verb_absent_400},
  {"cut %text%", lib_cmd_cut_other},
  {"cut", lib_cmd_cut_what},
  {"dance *", lib_cmd_dance},
  {"drink %object% *", lib_cmd_drink_object},
  {"drink %text%", lib_cmd_drink_other},
  {"drink", lib_cmd_drink_what},
  /* therest()'s seen-but-absent clause comes before the eat arm, so a
   * task that hides the noun first (Main Course `eat human`) is "can't see
   * the dead human." and not eat_other's line; see lib_cmd_verb_absent_400(). */
  {"eat *", lib_cmd_verb_absent_400},
  {"eat *", lib_cmd_eat_other},
  {"feed *", lib_cmd_feed},
  {"feel *", lib_cmd_feel},
  {"fight *", lib_cmd_fight},
  {"clear %object% *", lib_cmd_clear_object},
  {"clear %text%", lib_cmd_clear_other},
  {"fix %object% *", lib_cmd_fix_object},
  /* Every therest() verb meets the seen-but-absent clause (4887A0) before
   * its own arm: crashland T8 `fix ship`, the ship seen and left behind, is
   * "You can't see the ship." (run400 fix arm 489BE7 is below the clause). */
  {"fix %text%", lib_cmd_verb_absent_400},
  {"fix %text%", lib_cmd_fix_other},
  {"fix", lib_cmd_fix_what},
  {"fly *", lib_cmd_fly},
  {"hint *", lib_cmd_hint},
  {"hit %character%", lib_cmd_hit_npc},
  {"hit %object% *", lib_cmd_hit_object},
  {"hit %text%", lib_cmd_verb_absent_400},
  {"hit %text%", lib_cmd_hit_other},
  {"hit", lib_cmd_hit_what},
  {"slap %object% *", lib_cmd_slap_object},
  {"slap %text%", lib_cmd_verb_absent_400},
  {"slap %text%", lib_cmd_slap_other},
  {"slap", lib_cmd_slap_what},
  {"hum *", lib_cmd_hum},
  {"jump *", lib_cmd_jump},
  {"kick %character%", lib_cmd_attack_npc},
  {"kick %object% *", lib_cmd_kick_object},
  {"kick %text%", lib_cmd_verb_absent_400},
  {"kick %text%", lib_cmd_kick_other},
  {"kick", lib_cmd_kick_what},
  {"kiss %character% *", lib_cmd_kiss_npc},
  {"kiss %object% *", lib_cmd_kiss_object},
  {"kiss *", lib_cmd_kiss_other},
  {"kill *", lib_cmd_kill_other},
  {"lift %object% *", lib_cmd_lift_object},
  {"lift %text%", lib_cmd_verb_absent_400},
  {"lift %text%", lib_cmd_lift_other},
  {"lift", lib_cmd_lift_what},
  {"light %object% *", lib_cmd_light_object},
  {"light %text%", lib_cmd_verb_absent_400},
  {"light %text%", lib_cmd_light_other},
  {"light", lib_cmd_light_what},
  {"listen *", lib_cmd_listen},
  {"mend %object% *", lib_cmd_mend_object},
  {"mend %text%", lib_cmd_verb_absent_400},
  {"mend %text%", lib_cmd_mend_other},
  {"mend", lib_cmd_mend_what},
  {"move %object% *", lib_cmd_move_object},
  {"move %text%", lib_cmd_verb_absent_400},
  {"move %text%", lib_cmd_move_other},
  {"move", lib_cmd_move_what},
  {"please *", lib_cmd_please},
  {"press %object% *", lib_cmd_press_object},
  {"press %text%", lib_cmd_verb_absent_400},
  {"press %text%", lib_cmd_press_other},
  {"press", lib_cmd_press_what},
  {"pull %object% *", lib_cmd_pull_object},
  {"pull %text%", lib_cmd_verb_absent_400},
  {"pull %text%", lib_cmd_pull_other},
  {"pull", lib_cmd_pull_what},
  {"punch *", lib_cmd_punch},
  {"push %object% *", lib_cmd_push_object},
  {"push %text%", lib_cmd_verb_absent_400},
  {"push %text%", lib_cmd_push_other},
  {"push", lib_cmd_push_what},
  {"repair %object% *", lib_cmd_repair_object},
  {"repair %text%", lib_cmd_verb_absent_400},
  {"repair %text%", lib_cmd_repair_other},
  {"repair", lib_cmd_repair_what},
  {"rub %object% *", lib_cmd_rub_object},
  {"rub %text%", lib_cmd_verb_absent_400},
  {"rub %text%", lib_cmd_rub_other},
  {"rub", lib_cmd_rub_what},
  {"run *", lib_cmd_run},
  {"say *", lib_cmd_say},
  {"sell %object% *", lib_cmd_sell_object},
  {"sell %text%", lib_cmd_verb_absent_400},
  {"sell %text%", lib_cmd_sell_other},
  {"sell", lib_cmd_sell_what},
  {"shake %object% *", lib_cmd_shake_object},
  {"shake %text%", lib_cmd_verb_absent_400},
  {"shake %text%", lib_cmd_shake_other},
  {"shake", lib_cmd_shake_what},
  {"shout *", lib_cmd_shout},
  {"sing *", lib_cmd_sing},
  {"sleep *", lib_cmd_sleep},
  {"smell %object% *", lib_cmd_smell_object},
  {"smell *", lib_cmd_smell_other},
  {"stop %object% *", lib_cmd_stop_object},
  /* stop (488F56) is a therest arm below the 4887A0 clause too: British
   * Fox T376 `stop engine`, the Britmobile's engine seen elsewhere, is
   * "You can't see Britmobile engine." */
  {"stop %text%", lib_cmd_verb_absent_400},
  {"stop %text%", lib_cmd_stop_other},
  {"stop", lib_cmd_stop_what},
  {"suck %object% *", lib_cmd_suck_object},
  {"suck %text%", lib_cmd_suck_other},
  {"suck", lib_cmd_suck_what},
  {"talk to * about *", lib_cmd_ask_about_nothing},
  {"talk *", lib_cmd_talk},
  {"speak *", lib_cmd_speak_pre390},
  {"thank *", lib_cmd_thank},
  {"turn %object% *", lib_cmd_turn_object},
  {"turn %object% *", lib_cmd_turn_absent},
  /* The clause reads every object the line names, not the one a pattern
   * bound: British Fox T306 `eugene turn on computer`, Sharon's computer
   * seen elsewhere, is "You can't see Sharon's computer.". */
  {"turn %text%", lib_cmd_verb_absent_400},
  {"turn %text%", lib_cmd_turn_other},
  {"turn", lib_cmd_turn_what},
  {"touch %object% *", lib_cmd_touch_object},
  {"touch %text%", lib_cmd_touch_other},
  {"touch", lib_cmd_touch_what},
  {"unblock %object% *", lib_cmd_unblock_object},
  {"unblock %text%", lib_cmd_unblock_other},
  {"unblock", lib_cmd_unblock_what},
  {"wash %object% *", lib_cmd_wash_object},
  {"wash %text%", lib_cmd_verb_absent_400},
  {"wash %text%", lib_cmd_wash_other},
  {"wash", lib_cmd_wash_what},
  {"whistle *", lib_cmd_whistle},
  {"[why/when/what/can/how] *", lib_cmd_interrogation},
  {"xyzzy *", lib_cmd_xyzzy},
  {"campbell", lib_cmd_egotistic},
  {"[yes/no] *", lib_cmd_yes_or_no},
  {"*", lib_cmd_look_anywhere_pre_400},
  {"* %object% *", lib_cmd_verb_object},
  {"put *", lib_cmd_put_where_400},
  {"* %character% *", lib_cmd_verb_npc},

  /* Scarier debugger hook command, placed last just in case... */
  {"{#}debug{ger}", debug_cmd_debugger},

  {NULL, NULL}
};


/*
 * run_priority_commands()
 * run_standard_commands()
 *
 * Compare a user input string against commands recognized by the library,
 * and action any command.  Returns TRUE if the string matched a command
 * that then ran successfully, FALSE otherwise.
 *
 * "Priority" commands are ones that Adrift seems to action no matter what
 * the game tries to override.  For example, a simple game with one "ball"
 * object and a task "* ball *" should, if the task is restricted, override
 * "take ball" such that the ball can never be acquired.  Adrift lets the
 * "take" succeed, though (and more curiously, may respond "I don't
 * understand..." to "drop ball").  This could be an Adrift bug.  Shrug.
 *
 * For now, I can't find any better way to try to handle it than to make
 * object acquisition take precedence over game commands.
 *
 * The put-in/put-on family runs here TENTATIVELY: probed live against
 * run400 (2026-08-02, probe FM7 + a TheADRIFTProject .tas transplant), the
 * real Runner lets a put that can actually complete beat a matched task
 * whose restrictions fail ("put pill in cup" answers the library's "You put
 * the small pill inside the coffee cup." while the task's fail message is
 * suppressed), but when the put would be REFUSED (target not a container/
 * surface) the failing task's message wins ("drop pill in slime" prints the
 * fail message, not the refusal).  So in this pass the validity check
 * defers instead of refusing -- run_priority_defer() -- and the scan stops
 * so the plain-drop %text% rows cannot swallow the input; the loud-fail
 * task pass then gets its chance, and the duplicate rows in
 * STANDARD_COMMANDS print the refusal when no task claims it.
 */
#ifdef SCARIER_DUMP_TOOLS
/* SCR_TRACE_ADMIN: the last command dispatched, for the ADMIN trace line. */
std::string run_trace_last_input;
#endif

static scr_bool run_priority_pass_active = FALSE;

scr_bool run_priority_deferred = FALSE;

static scr_bool run_priority_refused = FALSE;

static scr_bool run_priority_unnamed_put = FALSE;

scr_bool
run_in_priority_pass (void)
{
  return run_priority_pass_active;
}

/*
 * run_in_put_clause_loop()
 *
 * TRUE while run_game_commands_common() is running the clauses of a 4.0 put
 * list one at a time; see lib_put_clauses_400() and lib_put_implicit_take().
 */
static scr_bool run_put_clause_loop_active = FALSE;

scr_bool
run_in_put_clause_loop (void)
{
  return run_put_clause_loop_active;
}

static void
run_priority_defer (void)
{
  assert (run_priority_pass_active);
  run_priority_deferred = TRUE;
}

/*
 * run_priority_defer_if_active()
 *
 * run_priority_defer(), but only inside the tentative priority pass: a
 * handler about to print a refusal leaves the line for a matched task's fail
 * message to claim first (see lib_put_in_is_valid()).  Returns TRUE if it
 * deferred, in which case the caller returns FALSE without printing.
 */
scr_bool
run_priority_defer_if_active (void)
{
  if (!run_priority_pass_active)
    return FALSE;

  run_priority_defer ();
  return TRUE;
}

/*
 * run_priority_refuse()
 *
 * A priority command has printed a refusal that does NOT claim the command:
 * the 4.0 put whose every object was turned away on size or capacity.  The
 * handler returns FALSE after calling this, the table walk stops, and
 * run_all_commands() lets the task passes answer the same line, joined onto
 * the refusal.  See the 4.0 put notes in run_all_commands().
 */
void
run_priority_refuse (void)
{
  assert (run_priority_pass_active);
  run_priority_refused = TRUE;
}

/*
 * run_priority_unnamed_put_object()
 *
 * 4.0's "put X in Y" where X names nothing leaves the command line
 * CLOBBERED, and the task passes never see what was typed.  run400 hands
 * every line holding the whole word "put" or "drop" to put_drop_list
 * (Proc_19_40_459DB4) ahead of the task dispatch; that normalises the line
 * ("drop " -> "put ", "inside"/"into" -> "in", "onto" -> "on"), splits it
 * at " in ", and calls name_object (Proc_19_41_46E5D8) to name the direct
 * object.  name_object sets the global command line MemVar_494174 to the
 * fragment Left(line, split) -- "put ice cream " -- and resolves that
 * (Proc_21_58_463640 mode 2).  Every other outcome leaves by 46E5CD, which
 * puts the line back; the one that does not is 46E142, reached when the
 * fragment names nothing at all.  There the Runner prints "It is not clear
 * which object you are referring to." ("Drop what?" for a "drop" line), but
 * only when no put/drop-class task pre-matches the typed line
 * (Proc_19_35_453C50 mode 2), and then returns at 46E23B with the fragment
 * still installed.  generaltasks dispatches the tasks against THAT, so a
 * task that would have claimed the typed line never matches, and the
 * catch-all -- whose noun was resolved up front from the original line --
 * answers instead: "I don't understand what you want to do with the cone."
 *
 * Measured on IceCream.taf in run400 (runner_probes/icecream.run400.put.txt,
 * 2026-09-07), whose three `put/place/set [the] ice cream in/on [the] cone`
 * tasks share one pattern: `put ice cream in cone` and `put the ice cream in
 * the cone` both come out as the catch-all, while `place ice cream in cone`
 * runs the task (place never enters put_drop_list) and `put ice cream on
 * cone` runs it too -- the "on" branch has an escape of its own at 459C39
 * that zeroes the split when the fragment resolves to nothing, so the line
 * reaches the tasks intact.  The clobber is the "in" form only.
 *
 * The put-in handler signals the fragment with this, and run_all_commands()
 * runs the task passes against the fragment in its place.
 */
void
run_priority_unnamed_put_object (void)
{
  assert (run_priority_pass_active);
  run_priority_unnamed_put = TRUE;
}

/* TRUE when this line's put-in went silent at 46E15A; see
 * lib_cmd_put_unclear(). */
scr_bool
run_priority_put_was_unnamed (void)
{
  return run_priority_unnamed_put;
}

/*
 * scr_ref_number_guard
 *
 * Save and put back the game's referenced-number state across a library
 * command table walk.
 *
 * Three library rows carry a %number% wildcard -- "wait 5", "redo 3",
 * "hist 3" (the wait row is written twice, abbreviation-gated) -- and
 * uip_match_number() sets the referenced number as a side effect of matching
 * one, exactly as it does for a game task's own %number%.  But those commands are Scarier's, not the Runner's: run400 has no
 * "wait N" at all, and its referenced number (MemVar_49420C) has just two
 * writers, numintext/numintext2, reachable only from the wildcard expansion
 * helper mdlSpreadTheLoad.Proc_19_36_45F268 and only when the pattern being
 * expanded really contains %number%.  So in the Runner a game with no number
 * wildcard anywhere can never see a referenced number other than the initial
 * zero, and a "$number = N" restriction (Type 4, Var1 0 -- run400
 * loc_4817BF..loc_4817F8) can never pass.
 *
 * Leaving the leak in makes those meta commands part of the game: Sandy.taf's
 * win task tests the referenced number against 2 where the author meant the
 * variable "mom", so `wait 2` followed by `look in toilet` won a game that is
 * unwinnable in the Runner.  Restoring around the whole walk -- not inside the
 * handlers -- also covers a row that matches and then declines, such as
 * "redo 99" falling through to the %text% row.
 *
 * Only the number is guarded.  The referenced object, character and text are
 * bound by library rows the Runner does have, and the text they later expand
 * into is meant to see them.
 */
class scr_ref_number_guard
{
public:
  explicit scr_ref_number_guard (scr_gameref_t game)
    : vars_ (gs_get_vars (game)),
      number_ (var_get_ref_number (vars_)),
      is_referenced_ (var_is_number_referenced (vars_))
  {
  }

  ~scr_ref_number_guard ()
  {
    var_restore_ref_number (vars_, number_, is_referenced_);
  }

  scr_ref_number_guard (const scr_ref_number_guard &) = delete;
  scr_ref_number_guard &operator= (const scr_ref_number_guard &) = delete;

private:
  const scr_var_setref_t vars_;
  const scr_int number_;
  const scr_bool is_referenced_;
};


scr_bool
run_priority_commands (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  scr_commandsref_t command;

  run_priority_pass_active = TRUE;
  run_priority_deferred = FALSE;
  run_priority_refused = FALSE;
  run_priority_unnamed_put = FALSE;
  for (command = PRIORITY_COMMANDS; command->command; command++)
    {
      if (uip_match (command->command, string, game))
        {
          if (command->handler (game))
            {
              run_priority_pass_active = FALSE;
              return TRUE;
            }
          if (run_priority_deferred || run_priority_refused)
            break;
        }
    }
  run_priority_pass_active = FALSE;

  /* Nothing matched the string.  Or if it did, its handler failed. */
  return FALSE;
}

/*
 * run_is_put_command()
 *
 * TRUE if the string is one of the priority table's NAMED put/drop rows --
 * "put X in Y", "drop X on Y", "drop X", "put X down".  These are the rows
 * run400's put_drop_list answers itself, above the task dispatcher; the
 * all/everything rows are deliberately NOT among them.  The probe binds
 * object references as a side effect, so they are saved and put back, as
 * run_task_reachable_by_library_callback() does for its own speculative
 * matches.
 *
 * Which side of the line a row falls on was measured on p4REPEAT2.taf and
 * p4REPEAT3.taf (run400 runner_probes/repeat2.run400.txt,
 * runner_probes/repeat3.run400.txt, 2026-09-08), where every one of the
 * probe's cells is a task whose command is the typed line, none of them
 * done, all with a RepeatText:
 *
 *   `drop coin`, `drop hat`   library, and the task NEVER ran (no "C2"/"C3")
 *   `put coin on desk`, `put hat on desk`               likewise (no C3/C4)
 *   `drop all`, `put all on desk`     the TASK ran -- "C14"/"C15", and the
 *                                     second time round its RepeatText
 *
 * The named rows are the whole of run400's drop routine Proc_19_7_46FB8C:
 * its var_88 = 0 arm resolves the noun itself and composes "<player> drop
 * <object>." at loc_46F732-46F76C without ever asking the dispatcher.  The
 * `all` arm does the opposite -- loc_46F1D8 tests the word "all" and, still
 * before anything is printed, hands the line to a task
 * (`Proc_19_35_453C50("drop all")` at 46F1EA) and returns from the routine
 * outright when one claims it (`Result: End Sub`, 46F1F8).  So an "all"
 * line reaches the tasks first and a named one does not, and the empty-hands
 * "You're not carrying anything." (loc_46F457, and 46FB33 for the put half)
 * is only ever reached once the tasks have declined.
 */
static scr_bool
run_is_put_command (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_ref_entity_guard ref_entity (game);
  std::vector<scr_bool> references (game->object_references);
  scr_commandsref_t command;
  scr_bool is_put = FALSE;

  /*
   * Take the table in order and stop at the first row that matches, exactly
   * as run_priority_commands() does: "drop all" matches the named row
   * "[drop/put down] %text%" too, with %text% = "all", and it is only
   * because the all/everything row sits above it that the Runner's `all`
   * arm is the one that runs.  Scanning for the named handlers alone made
   * every "all" line look named.
   */
  for (command = PRIORITY_COMMANDS; command->command; command++)
    {
      if (!uip_match (command->command, string, game))
        continue;
      /* The container-first rows answer only their three exits and pass
       * everything else down to these; look through them. */
      if (command->handler == lib_cmd_put_container_400)
        continue;

      is_put = command->handler == lib_cmd_put_in_multiple
               || command->handler == lib_cmd_put_on_multiple
               || command->handler == lib_cmd_drop_multiple;
      break;
    }

  game->object_references = references;
  return is_put;
}

/*
 * run_is_put_command_400()
 *
 * run_is_put_command(), and at 4.0 a second look by whole-word containment
 * when the container phrase names a fitting container or surface to run400's
 * scorer but the put rows' positional %object% does not -- `put my homework
 * on my desk`, the desk's Prefix being "your school"; see
 * lib_put_container_fits_400().  *CONTAINED says the put rows need that
 * containment again when they run.
 */
static scr_bool
run_is_put_command_400 (scr_gameref_t game, const scr_char *string,
                        scr_bool *contained)
{
  scr_bool is_put;

  *contained = FALSE;
  if (run_is_put_command (game, string))
    return TRUE;
  if (!lib_put_container_fits_400 (game, string))
    return FALSE;

  uip_set_containment (TRUE);
  is_put = run_is_put_command (game, string);
  uip_set_containment (FALSE);
  *contained = is_put;
  return is_put;
}

/*
 * run_is_inventory_command()
 *
 * TRUE for the lines run400's inventory handler answers.  That handler,
 * Proc_19_70_45C304, is called at loc_48A457 -- above the task dispatcher at
 * 48A481 -- so its listing is in the message buffer before any task runs,
 * and a task that then matches the same line has its CompleteText APPENDED
 * to the listing rather than replacing it.  Measured on p4REPEAT2/3
 * (runner_probes/repeat2.run400.txt, runner_probes/repeat3.run400.txt,
 * 2026-09-08): the first `i` answers "You are carrying a coin and a hat.  C1
 * i.", and `inv` / `inventory` the same way.  (The listing is also why a
 * spent task's RepeatText never survives here -- the 44CC7D store is gated
 * on the buffer still being empty; that is the survivor table's first row.)
 */
static scr_bool
run_is_inventory_command (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_ref_entity_guard ref_entity (game);
  scr_commandsref_t command;
  scr_bool is_inventory = FALSE;

  for (command = PRIORITY_COMMANDS; command->command && !is_inventory;
       command++)
    {
      if (command->handler != lib_cmd_inventory)
        continue;

      is_inventory = uip_match (command->command, string, game);
    }

  return is_inventory;
}

/*
 * run_repeat_survivor_400()
 *
 * TRUE if a 4.0 line is one that a spent task's RepeatText does NOT take
 * away from the standard library.  Measured 2026-09-08 with p4REPEAT.taf,
 * p4REPEAT2.taf and p4REPEAT3.taf (make_400_repeatprobe.py and
 * make_400_repeatprobe2.py, 30-odd one-cell tasks all done, non-repeatable
 * and carrying a RepeatText, each typed twice) under run400, transcripts
 * runner_probes/repeat.run400.txt, runner_probes/repeat2.run400.txt and
 * runner_probes/repeat3.run400.txt.
 *
 * run400 prints a done non-repeatable task's RepeatText inside the task
 * dispatcher at 48A481 (Proc_19_24_44CCE0) and then jumps to loc_48B4E3,
 * past every general library verb.  Only two groups of handlers escape it:
 *
 *   - the ones the input routine runs BEFORE the dispatcher: inventory
 *     (48A457) and the put/drop list (48A462).  They print first, and the
 *     RepeatText is emitted only while the output is still empty, so
 *     whatever they say silences it -- `i` lists the inventory, `drop coin`
 *     and `put coin on desk` keep "You are not holding the coin.".  The
 *     all/everything forms are in that same list but say nothing with empty
 *     hands, and `drop all` / `put all on desk` DO draw the RepeatText, so
 *     they are matched here first and excluded.
 *   - the per-character pass at 48B56E (Proc_19_0_480674), which sits BELOW
 *     48B4E3 and so still runs, its answer replacing the message: NPC
 *     examine (47FE4F) and the ask-format hint (47F84A) that `talk to X`,
 *     `speak to X` and `ask X <anything but "about">` land on.
 *
 * Everything else loses the line to the RepeatText, movement and take
 * included: `north` and `east` print it and the player does not move,
 * `take coin` never takes the coin, `look`, `wait`, `score`, `turns`,
 * `open`, `read`, `wear`, `x <object>`, `x me`, `take all` and `drop all`
 * all print it, and a wildcard task's RepeatText wins as readily as a
 * literal one's (`* dance *`) -- so neither of the pre-4.0 pass's literal
 * and movement exemptions carries over.  `kiss X`, `give X to Y` and `ask X
 * about Y` are general verbs rather than parts of the character pass, and
 * lose the line too; `talk X` without the "to" is the 3.7/3.8 spelling that
 * 4.0's hint does not answer, so it loses it as well.
 */
typedef struct
{
  const scr_char *command;
  scr_bool survives;
} scr_repeat_survivor_t;

static const scr_repeat_survivor_t REPEAT_SURVIVOR_COMMANDS[] = {
  /* Pre-dispatcher, 48A457. */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[inventory/inv]", TRUE},
#else
  {"[inventory/inv/i]", TRUE},
#endif

  /* Pre-dispatcher, 48A462 -- but only where it has something to say. */
  {"put [all/everything] *", FALSE},
  {"[drop/put down] [all/everything] *", FALSE},
  {"put %text% [in/into/inside {of}] %object%", TRUE},
  {"put %text% [on/onto/on top of] %object%", TRUE},
  {"[drop/put down] %text% [in/into/inside {of}] %object%", TRUE},
  {"[drop/put down] %text% [on/onto/on top of] %object%", TRUE},
  {"[drop/put down] %text%", TRUE},
  {"put %text% down", TRUE},

  /* The character pass at 48B56E, and the two branches that are not it. */
  {"ask %character% about %text%", FALSE},
  {"[talk/speak] to %character% about %text%", FALSE},
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[ex/exam/examine/look {at}] %character%", TRUE},
#else
  {"[x/ex/exam/examine/look {at}] %character%", TRUE},
#endif
  {"[talk/speak] to %character% *", TRUE},
  {"ask %character% *", TRUE},

  {NULL, FALSE}
};

static scr_bool
run_repeat_survivor_400 (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_ref_entity_guard ref_entity (game);
  std::vector<scr_bool> objects (game->object_references);
  std::vector<scr_bool> npcs (game->npc_references);
  const scr_repeat_survivor_t *row;
  scr_bool survives = FALSE;

  for (row = REPEAT_SURVIVOR_COMMANDS; row->command; row++)
    {
      if (uip_match (row->command, string, game))
        {
          survives = row->survives;
          break;
        }
    }

  game->object_references = objects;
  game->npc_references = npcs;
  return survives;
}

/*
 * run_normalise_put_line()
 * run_unnamed_put_fragment()
 *
 * Rebuild the command line run400's put_drop_list leaves behind when the
 * direct object of a "put X in Y" names nothing -- see the 46E142 note on
 * run_priority_unnamed_put_object() above.  put_drop_list normalises the
 * line with four plain VB Replace() calls (459B3D-459BAC, substring not
 * word, and "inside" before "into" so that "inside" cannot be reached by
 * the shorter pattern), splits it at the first " in " (459BCD), and
 * name_object installs Left(line, split) -- everything up to and including
 * the space before "in" -- as the command line at 46DE99.
 *
 * The list branches leave before the clobber, so none of them is rebuilt
 * here: a whole-word "all" or "and" in the fragment is answered by the
 * loops at 46E04E and 46E0B2 (unported), and an " and " at or beyond the
 * split sends put_drop_list round its own loop at 459C75, whose clauses are
 * carved by lib_put_clauses_400().  "put a in b and put c in d" is not an
 * example of either: the word after its " and " is "put", not an object, so
 * the top-level splitter cuts it into two commands (run_find_split_400()).
 */
std::string
run_normalise_put_line (const scr_char *string)
{
  std::string line (string);

  run_replace_all (line, "drop ", "put ");
  run_replace_all (line, "inside", "in");
  run_replace_all (line, "into", "in");
  run_replace_all (line, "onto", "on");
  return line;
}

scr_bool
run_unnamed_put_fragment (const scr_char *string, std::string &fragment)
{
  std::string line = run_normalise_put_line (string);
  std::string::size_type split;

  split = line.find (" in ");
  if (split == std::string::npos)
    return FALSE;

  fragment = line.substr (0, split + 1);
  if ((" " + fragment).find (" all ") != std::string::npos
      || (" " + fragment).find (" and ") != std::string::npos
      || line.find (" and ", split) != std::string::npos)
    return FALSE;

  return TRUE;
}

/*
 * run_move_commands()
 *
 * Return the movement command table matching the game's compass setting.
 */
static scr_commandsref_t
run_move_commands (const scr_prop_setref_t bundle)
{
  return prop_get_global_boolean (bundle, "EightPointCompass")
         ? MOVE_COMMANDS_8 : MOVE_COMMANDS_4;
}

/*
 * run_try_command_table()
 *
 * Search a command table for a match to the string, returning TRUE on the
 * first matching command whose handler succeeds.
 */
static scr_bool run_npc_library_blocked (scr_gameref_t game);
static scr_bool run_npc_row_blocked (const scr_commands_t *command);

/*
 * Set while the pre-4.0 verb pass runs, so that the give-to-character rows
 * wait for run_standard_give_npc_commands() below the room refusal.
 */
static scr_bool run_defer_give_npc = FALSE;

/*
 * Set while the 3.9 verb pass runs on a line run390's therest with-arm will
 * answer (lib_with_arm_390()): the characters() attack rows, which run390
 * calls below therest, wait for it -- and lose to it.  With the Battle
 * System on, those rows are dobattle's, which run390 calls above therest
 * (45F4AF): secret_of_lost_world's `hit ghost with excalibur` is a blow.
 */
static scr_bool run_defer_with_390 = FALSE;

static scr_bool
run_is_with_deferred_handler (scr_bool (*handler) (scr_gameref_t))
{
  static scr_bool (*const DEFERRED[]) (scr_gameref_t) = {
    lib_cmd_attack_npc, lib_cmd_attack_npc_with, lib_cmd_attack_npcs,
    lib_cmd_attack_npcs_with, lib_cmd_chop_npc, lib_cmd_chop_npc_with,
    lib_cmd_cut_npc, lib_cmd_cut_npc_with, lib_cmd_fight_npc,
    lib_cmd_fight_npc_with, lib_cmd_hit_npc_with, lib_cmd_kill_npc,
    lib_cmd_kill_npc_with, lib_cmd_shoot_npc, lib_cmd_shoot_npc_with,
    lib_cmd_slap_npc, lib_cmd_slap_npc_with, lib_cmd_stab_npc,
    lib_cmd_stab_npc_with, lib_cmd_throw_npc_with, NULL
  };

  for (scr_int index_ = 0; DEFERRED[index_]; index_++)
    {
      if (DEFERRED[index_] == handler)
        return TRUE;
    }
  return FALSE;
}

scr_bool
run_try_command_table (scr_commandsref_t command,
                       scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_bool npc_blocked = run_npc_library_blocked (game);

  for (; command->command; command++)
    {
      if (run_defer_give_npc && command->handler == lib_cmd_give_object_npc)
        continue;
      if (run_defer_with_390 && run_is_with_deferred_handler (command->handler))
        continue;

      /*
       * Once a game task has run for this line, most of the Runner's
       * character handler answers nothing: the who (47F32C), hit/kill/kick/
       * punch/attack (47F452), get/take/pick up (47F734), talk to/speak to
       * (47F863), ask-without-about hint (47FB93), where/find/locate
       * (47FCB1), examine/look (47FE4F) and take-from (4803DD) branches of
       * run400's Proc_19_0_480674 all test MemVar_4941F8 = 0, the flag
       * execute_task sets at 45A176 and the input routine clears at 489FF6.
       * run390 guards the same branches with MemVar_468198 (45939D,
       * 459658); run370 has no such flag (4386BC), and run380's rendering
       * of the test (44054B) is too ambiguous to lean on, so pre-3.9 keeps
       * every row.  Measured live on House (4.00), 2026-09-06: the silent
       * task "# attention on cathy grave vision" (`*cathy*`, once only) runs
       * on the first `get cathy` and the library then says "Take what?"
       * (runner_probes/house.run400.t93.txt,
       * runner_probes/house.run400.girl.txt); `x cathy` on that first
       * mention gets "You see no such thing."
       * (runner_probes/house.run400.t98.txt); with the task spent, both fall
       * to the NPC handlers -- "I don't think girl would appreciate being
       * handled." and her description.
       *
       * The rows that survive a task are the ones the Runner reaches by
       * another route: give is handled in the input routine at 48A98A with
       * no flag test; `ask X about Y` is re-opened at 47F922-47F935 by the
       * "<player> can't talk to that." buffer that generaltasks_verbs seeds
       * at 488C65 whenever no object took the ask (run390 tests no flag at
       * all at 4597FE) -- Humbug's silent scoring task `ask * hacker about
       * * humbug` still gets the hacker's reply
       * (runner_probes/humbug.run400.b.txt); kiss (47F7E7) is gated on the
       * buffer, not the flag; and the end-of-handler "I don't understand
       * what you want to do with" fallback at 4805DA asks for an empty
       * buffer rather than the flag.  (It does have one other guard, added
       * here 2026-09-07: `loc_4805CD: If MemVar_4941AD > 0 Then Exit Sub`
       * above it takes it off a line that a task has just ended the game on
       * -- see lib_cmd_verb_npc().)
       */
      if (npc_blocked && run_npc_row_blocked (command))
        continue;

      if (uip_match (command->command, string, game))
        {
          if (command->handler (game))
            return TRUE;
        }
    }
  return FALSE;
}

/*
 * run_movement_succeeds()
 *
 * Return TRUE if the input is a movement command that would really move the
 * player out of the room.  Prints nothing and changes nothing -- the movement
 * handlers run under lib_set_movement_probe().  Used only by the version 3.8
 * ordering in run_all_commands().
 */
scr_bool
run_movement_succeeds (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool is_movement;

  lib_set_movement_probe (TRUE);
  is_movement = run_try_command_table (run_move_commands (bundle),
                                       game, string);
  lib_set_movement_probe (FALSE);

  return is_movement;
}


/*
 * The dedicated verb handlers: everything run390's generaltasks() calls by
 * name, above the out-of-room task refusal at loc_45FFE8.  Split from the
 * fallback bucket below because the refusal goes between the two; see the
 * ordering note on run_task_refusal().
 */
static scr_bool run_standard_verb_commands_inner (scr_gameref_t game,
                                                  const scr_char *string);

scr_bool
run_standard_verb_commands (scr_gameref_t game, const scr_char *string)
{
  /*
   * Pre-4.0 the give to a present character is characters()'s (run390
   * 45A0BA) or therest()'s (run380 440E8C), both below the room refusal,
   * and run390's give writes only into an empty message (or one holding
   * " might need " / "I don't understand", 45A11D-45A167).  the_hangover
   * (3.90) T42 `give the doctor some french fries`, with Where=0 task 10
   * matching it: "You can't do that here!" (runner_transcripts/
   * the_hangover.txt), not "Doctor doesn't seem interested in the french
   * fries.".  4.0 gives in the input routine (48A98A), above the refusal.
   */
  run_defer_give_npc =
      run_get_version (gs_get_bundle (game)) < TAF_VERSION_400;
  run_defer_with_390 = !battle_is_enabled (game)
                       && lib_with_arm_390_applies (game);
  const scr_bool status = run_standard_verb_commands_inner (game, string);
  run_defer_give_npc = FALSE;
  run_defer_with_390 = FALSE;
  return status;
}

/*
 * The give-to-character rows of STANDARD_COMMANDS, run pre-4.0 between the
 * room refusal and the fallback bucket; see run_standard_verb_commands().
 */
scr_bool
run_standard_give_npc_commands (scr_gameref_t game, const scr_char *string)
{
  static scr_commands_t GIVE_NPC_COMMANDS[] = {
    {"give %object% to %character%", lib_cmd_give_object_npc},
    {"give %character% %object%", lib_cmd_give_object_npc},
    {"give %object% %character%", lib_cmd_give_object_npc},
    {NULL, NULL}
  };

  if (run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    return FALSE;
  if (run_try_command_table (GIVE_NPC_COMMANDS, game, string))
    return TRUE;
  uip_set_containment (TRUE);
  const scr_bool contained =
      run_try_command_table (GIVE_NPC_COMMANDS, game, string);
  uip_set_containment (FALSE);
  return contained;
}

static scr_bool
run_standard_verb_commands_inner (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * Search movement commands first, returning TRUE if any matching command
   * handler succeeded.  Then repeat for standard library commands.
   */
  if (run_try_command_table (run_move_commands (bundle), game, string))
    return TRUE;

  /* The take-from catch-alls; see STANDARD_TAKE_FROM_COMMANDS. */
  if (run_try_command_table (STANDARD_TAKE_FROM_COMMANDS, game, string))
    return TRUE;

  if (run_try_command_table (STANDARD_COMMANDS, game, string))
    return TRUE;

  /*
   * The same commands again with whole-word containment for a trailing
   * %object% (uip_match_entity()): the Runner's co() finds an object named
   * anywhere in the line, so `x silver key` examines "a key" (man_overboard,
   * 4.00).  A separate pass, so that every pattern's positional match has
   * had its turn first, and before the catch-alls below so that they never
   * pre-empt an object the line does name.
   */
  uip_set_containment (TRUE);
  const scr_bool contained =
      run_try_command_table (STANDARD_COMMANDS, game, string);
  uip_set_containment (FALSE);
  if (contained)
    return TRUE;

  /* The pre-4.0 put catch-alls; see STANDARD_PUT_COMMANDS. */
  if (run_try_command_table (STANDARD_PUT_COMMANDS, game, string))
    return TRUE;

  /* run390's take handler, still above the refusal; see STANDARD_ABOVE_REFUSAL_COMMANDS. */
  uip_set_containment (TRUE);
  const scr_bool taken =
      run_try_command_table (STANDARD_ABOVE_REFUSAL_COMMANDS, game, string);
  uip_set_containment (FALSE);
  return taken;
}


/*
 * STANDARD_ENDED_FALLBACK_COMMANDS
 *
 * What is left of STANDARD_FALLBACK_COMMANDS at 4.0 once a task has ended the
 * game on the line.  run400 calls therest (Proc_19_85_489F4C) at 48AFE4, BELOW
 * generaltasks' gameover jump at 48AC62 (`If MemVar_4941AD <> 0 Then GoTo
 * loc_48B4E3`), so every therest arm -- give, lock, ask, the "You can't <verb>
 * <X>." refusals, the question words -- is lost with the unhandled-verb tail.
 * iachini (run400x runner_transcripts/iachini.txt T185): `turn on tv` runs
 * silent TASK 30, whose only action is End Game, and run400 answers the game's
 * DontUnderstand, not "You can't turn the 32-inch television on.".  Rows whose
 * Runner handler sits above 48AC62 (open/close 48A515, wear 48A48C, remove
 * 48A491) or has not been placed keep their answer; so does the character
 * tail (48B56E, with its own 4805CD gate in lib_cmd_verb_npc()).
 */
static scr_commands_t STANDARD_ENDED_FALLBACK_COMMANDS[] = {
  {"open %object%", lib_cmd_open_absent},
  {"open *", lib_cmd_open_ended_400},
  {"close %object%", lib_cmd_close_absent},
  {"close *", lib_cmd_close_ended_400},
  {"sit {down/up} [on/in] *", lib_cmd_verb_absent_400},
  {"sit {down/up} [on/in] *", lib_cmd_sit_other},
  {"stand {up/down} [on/in] *", lib_cmd_verb_absent_400},
  {"stand {up/down} [on/in] *", lib_cmd_stand_other},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_verb_absent_400},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_lie_other},
  {"[remove/take off/doff] *", lib_cmd_remove_what},
  {"[drop/put down] *", lib_cmd_drop_what},
  {"[wear/put on/don] *", lib_cmd_wear_what},
  {"put *", lib_cmd_put_unclear},
  {"[shit/fuck/bastard/cunt/crap/hell/shag/bollocks/bollox/piss] *",
   lib_cmd_profanity},
  {"bugger *", lib_cmd_profanity_390},
  {"[x/ex/exam/examine/look {at}] %object%", lib_cmd_examine_absent},
  {"[x/ex/exam/examine/look {at}] *", lib_cmd_examine_other},
  {"[locate/where {is/are}/find] *", lib_cmd_locate_other},
  {"hint *", lib_cmd_hint},
  {"kiss *", lib_cmd_kiss_ended_400},
  {"* %character% *", lib_cmd_verb_npc},
  {NULL, NULL}
};


/* run390's therest(): the generic catch-alls, below the refusal. */
scr_bool
run_standard_fallback_commands (scr_gameref_t game, const scr_char *string)
{
  const scr_bool is_ended = game->pending_endgame != 0
      && run_get_version (gs_get_bundle (game)) == TAF_VERSION_400;

  /*
   * The fallback verbs resolve their noun the Runner's way too: generaltasks
   * (Proc_19_85_489F4C) runs co() once, up front, and every generic verb
   * after it sees the object it found -- `pull de la palanca` (Vardock
   * Bates, 4.00, a `tirar`->`pull` synonym leaving the "de" in place) is
   * "You pull la palanca, but nothing happens." in run400 (transcript
   * 2026-08-29), not the object-less "You pull, but nothing happens.".
   * Containment stays a per-row fallback (positional first), so a row's
   * "%object% *" form still wins over the "%text%" catch-all beneath it.
   */
  uip_set_containment (TRUE);
  const scr_bool fallback =
      run_try_command_table (is_ended ? STANDARD_ENDED_FALLBACK_COMMANDS
                                      : STANDARD_FALLBACK_COMMANDS,
                             game, string);
  uip_set_containment (FALSE);
  return fallback;
}


/*
 * run_c_word_pre400()
 *
 * The pre-4.0 Runners' c(word) test (run370 423C80, run380 429048, run390
 * 4334B0): the FIRST case-insensitive InStr hit that starts the line or
 * follows a space decides, and it is true only if the word ends there -- at
 * the end of the line, a space or a comma, and at 3.9 a period too.  A hit
 * inside a word is skipped, so `unblock` never holds `block`; a word-start
 * hit that runs on stops the search, so `lookout look` holds no `look`.
 * Returns the hit's offset, or -1.
 */
scr_int
run_c_word_pre400 (scr_int version, const scr_char *line, const scr_char *word)
{
  const scr_int length = strlen (word);
  const scr_char *hit;
  scr_char end;

  for (hit = line; *hit != NUL; hit++)
    {
      if (scr_strncasecmp (hit, word, length) != 0)
        continue;
      if (hit == line || hit[-1] == ' ')
        break;
    }
  if (*hit == NUL)
    return -1;

  end = hit[length];
  if (end == NUL || end == ' ' || end == ','
      || (end == '.' && version == TAF_VERSION_390))
    return hit - line;
  return -1;
}

/*
 * run_c_word()
 *
 * c(word) at any version: 4.0's whole-word test (lib_input_contains_word())
 * or the pre-4.0 first-hit rule above.
 */
scr_bool
run_c_word (scr_int version, const scr_char *line, const scr_char *word)
{
  return version >= TAF_VERSION_400
         ? lib_input_contains_word (line, word)
         : run_c_word_pre400 (version, line, word) >= 0;
}


/* In therest() order; '?' marks an arm that needs an empty message. */
static const scr_char *const THEREST_ARMS_380[] = {
    "open", "close", "eat", "drink", "give", "ask", "talk to", "?talk",
    "say", "look", "clean", "run", "stop", "read", "wash", "cut", "hit",
    "kill", "move", "lift", "light", "suck", "feel", "turn", "go", "enter",
    "smell", "push", "pull", "press", "shake", "clear", "clean", "kick",
    "punch", "fight", "jump", "feed", "unblock", "?block", "unlock",
    "?lock", "climb", "listen", "shout", "sing", "hum", "dance", "whistle",
    "cry", "wait", "buy", "sell", "break", "destroy", "smash", "kiss", "fly",
    "feed", "feel", "please", "fix", "repair", "mend", "date", "time",
    "sleep", "sit on", "sit in", "stand on", "stand in", "lie on", "lie in",
    NULL
  };

static const scr_char *const THEREST_ARMS_390[] = {
    "open", "close", "eat", "drink", "give", "ask", "talk to", "?talk",
    "say", "look", "clean", "run", "stop", "read", "wash", "cut", "kill",
    "move", "lift", "light", "suck", "feel", "touch", "turn", "go", "enter",
    "smell", "push", "pull", "press", "shake", "kick", "hit", "clear",
    "punch", "fight", "jump", "feed", "unblock", "block", "unlock", "lock",
    "climb", "listen", "shout", "sing", "hum", "dance", "whistle", "cry",
    "wait", "examine", "ex", "x", "buy", "sell", "break", "destroy", "smash",
    "kiss", "fly", "feed", "feel", "please", "fix", "repair", "mend", "date",
    "time", "sleep", "sit on", "sit in", "stand on", "stand in", "lie on",
    "lie in", "xyzzy", NULL
  };

/*
 * run_therest_arm_at()
 *
 * TRUE when one of therest()'s own verb arms begins at WORD.  Used to tell
 * a line that reaches therest from one an earlier handler owns; the arms
 * that need an empty message count too, being arms all the same.
 */
scr_bool
run_therest_arm_at (scr_int version, const scr_char *word)
{
  const scr_char *const *arm;

  for (arm = (version == TAF_VERSION_390) ? THEREST_ARMS_390 : THEREST_ARMS_380;
       *arm; arm++)
    {
      const scr_char *const entry = *arm + ((*arm)[0] == '?' ? 1 : 0);
      const scr_int length = strlen (entry);

      if (version < TAF_VERSION_380 && strcmp (entry, "shake") == 0)
        continue;
      if (scr_strncasecmp (word, entry, length) == 0
          && (word[length] == NUL || word[length] == ' '))
        return TRUE;
    }
  return FALSE;
}


/*
 * run_therest_winner_pre400()
 *
 * Pre-4.0 therest() is not a verb-first table.  It is one long run of
 * keyword arms, each `If c("<verb>") Then msg = ...`, over the whole line,
 * and each arm overwrites the message the one before it wrote, so the LAST
 * arm whose keyword the line holds answers (run380 443CBB-445521; run390
 * 45D465-45EB86, where most arms go through checkverb 42A504 with the same
 * c() test).  Measured on p38ASK/p39ASK with harness/make_38_askprobe.py and
 * make_39_askprobe.py (run380x runner_probes/ask.run380.kw.rtf, run390x
 * runner_probes/ask.run390.kw.txt): `zzz push stone`, `stone jump` and `zzz
 * cut stone` answer the verb wherever it stands; `push and pull stone` and
 * `push stone pull` pull, `push stone and kick` kicks, `buy stone kiss`
 * kisses, `drink push stone` pushes and `climb stone and sit on stone` sits.
 * talk, block and lock only write an empty message, so they lose to any
 * earlier arm.  The arms that live above therest() (open, read, ...) have
 * answered before any of this runs.
 *
 * Returns the winning arm's keyword and sets *offset to where c() found it,
 * or returns NULL.
 */
static const scr_char *
run_therest_winner_pre400 (scr_int version, const scr_char *line,
                           scr_int *offset)
{
  const scr_char *const *arm;
  const scr_char *winner = NULL;

  for (arm = (version == TAF_VERSION_390) ? THEREST_ARMS_390 : THEREST_ARMS_380;
       *arm; arm++)
    {
      const scr_bool needs_empty = (*arm)[0] == '?';
      const scr_char *const word = *arm + (needs_empty ? 1 : 0);
      scr_int found;

      /* run370 has no shake arm. */
      if (version < TAF_VERSION_380 && strcmp (word, "shake") == 0)
        continue;
      if (needs_empty && winner)
        continue;

      found = run_c_word_pre400 (version, line, word);
      if (found >= 0)
        {
          winner = word;
          *offset = found;
        }
    }

  return winner;
}


/*
 * run_therest_pre400()
 *
 * When the winning arm's keyword does not open the line, answer the line as
 * if it did: the keyword is moved to the front and the library runs again,
 * so its "<verb> %object% *" row answers with the object therest() found
 * (the fallback rows resolve by containment).  The look arm, and 3.9's
 * examine arms, answer the flat "Nothing special." whatever object the line
 * names.  Returns TRUE if the line was answered.
 */
scr_bool
run_therest_pre400 (scr_gameref_t game, const scr_char *string)
{
  static scr_bool is_active = FALSE;
  const scr_int version = run_get_version (gs_get_bundle (game));
  const scr_char *winner;
  scr_int offset = -1;
  std::string moved;
  scr_bool answered;

  if (is_active || version >= TAF_VERSION_400
      || game->pending_endgame != 0)
    return FALSE;

  /*
   * A winner opening the line has had its own row.  That includes a 3.7/3.8
   * `look X`, which the Runner leaves to therest's look arm ("Nothing
   * special." for anything at all, run370 434E2A, run380 44439D/43C69D;
   * p37EXAM/p38EXAM, run370x plookobj37, run380x plookobj38).  Deliberate
   * deviation: Scarier examines it, as from 3.9.
   */
  winner = run_therest_winner_pre400 (version, string, &offset);
  if (!winner || offset == 0)
    return FALSE;

  /*
   * generaltasks calls takes, drops, wears, removes and examines ahead of
   * therest, so a line naming any of them never reaches these arms at all;
   * see lib_earlier_handler_claims_pre400().  The 3.7 refusal above this
   * one has always known it (run_therest_absent_370's EARLIER list).
   */
  if (lib_earlier_handler_claims_pre400 (game, string))
    return FALSE;

  if (strcmp (winner, "look") == 0 || strcmp (winner, "examine") == 0
      || strcmp (winner, "ex") == 0 || strcmp (winner, "x") == 0)
    {
      pf_buffer_string (gs_get_filter (game), "Nothing special.\n");
      return TRUE;
    }

  moved = winner;
  moved += ' ';
  moved.append (string, offset);
  moved += ' ';
  moved.append (string + offset + strlen (winner));

  /* Tidy the spaces the cut left behind. */
  {
    std::string tidy;
    for (const scr_char c : moved)
      if (c != ' ' || (!tidy.empty () && tidy.back () != ' '))
        tidy += c;
    while (!tidy.empty () && tidy.back () == ' ')
      tidy.pop_back ();
    moved = tidy;
  }

  is_active = TRUE;
  answered = run_standard_verb_commands (game, moved.c_str ())
             || run_standard_give_npc_commands (game, moved.c_str ())
             || run_standard_fallback_commands (game, moved.c_str ());
  is_active = FALSE;
  return answered;
}


/*
 * run_therest_absent_370()
 *
 * 3.7's therest() refuses a line naming an absent object before any verb
 * arm -- see lib_therest_absent_370().  Only a line that reaches therest()
 * gets it: one holding a therest arm keyword that no earlier generaltasks
 * handler takes (run370 43B942-43C644).  Those handlers enter on c() words
 * anywhere in the line -- takes, drops, wears and removes (435E28, 430475,
 * 42C533, 4295FF), openclose (4264E7), examines (434E2A), give (43BED8),
 * wait (43C1B3), whereis (42F9FF), gotoplace, thank and the question words
 * (43C499) -- and keep their own wording, so a line holding any of them is
 * left to them.  ask, talk and say are left alone too: not measured.
 */
static scr_bool
run_therest_absent_370 (scr_gameref_t game, const scr_char *string)
{
  static const scr_char *const EARLIER[] = {
    "get", "take", "pick", "from", "drop", "put", "leave", "wear", "remove",
    "strip", "open", "close", "x", "examine", "look at", "ex", "exam", "read",
    "give", "wait", "where", "find", "locate", "goto", "go to", "thank",
    "when", "who", "what", "how", "can", "why", "score", "ask", "talk", "say",
    NULL
  };
  const scr_int version = run_get_version (gs_get_bundle (game));
  const scr_char *const *word;
  scr_int offset;

  if (version >= TAF_VERSION_380 || game->pending_endgame != 0)
    return FALSE;
  if (!run_therest_winner_pre400 (version, string, &offset))
    return FALSE;
  /* The " with " split is made above this test; see lib_with_clause_claims(). */
  if (lib_with_clause_claims (game))
    return FALSE;
  for (word = EARLIER; *word; word++)
    if (run_c_word_pre400 (version, string, *word) >= 0)
      return FALSE;
  if (lib_sitstand_claims_370 (game))
    return FALSE;

  return lib_therest_absent_370 (game);
}


/*
 * The player's current command element (pronoun-substituted), stashed by
 * run_all_commands() so that library-initiated match attempts can consult
 * the verb the player actually typed alongside the library's canonical
 * constructed command ("get <object>", and so on).  A pass that runs a
 * line of its own making points it there for the while with a
 * run_dispatch_input_guard (scrunner.h).
 */
const scr_char *run_dispatch_input = NULL;

/*
 * run_line_for_anywhere()
 *
 * The standard rows run on a line of a pass's own making: the sit words cut
 * out for lib_sitstand_anywhere()'s pre-run of wears, removes and the
 * 3.7/4.0 put, the open/close word brought to the head for
 * lib_openclose_anywhere(), the where-is half of the line for
 * lib_whereis_anywhere().
 */
scr_bool
run_line_for_anywhere (scr_gameref_t game, const scr_char *line)
{
  const run_dispatch_input_guard input (line);

  return run_standard_verb_commands (game, line);
}


/*
 * run_score_anywhere()
 *
 * Every Runner's score arm is `If c("score") Then` -- the whole word
 * anywhere in the line -- and it assigns the message, overwriting whatever
 * sitstand, inventory, the help hint or examines had written, and marks the
 * line administrative (run370 43BB98, run380 4423EC, run390 45F6B5 with
 * MemVar_468219 = 1, run400 48A6AE with MemVar_494281).  It sits above the
 * wait gate, so `score wait` is the score and no time passes; below takes,
 * drops, wears and removes, which claim or act first, and above the
 * swearing arm, which overwrites it.  Measured 2026-09-20 on p37SITN..p4SITN
 * (harness/make_3738_sitnpcprobe.py: `score sit`, `score wait`; the sit case
 * is lib_sitstand_anywhere()'s).  A line holding one of those earlier or
 * later handlers' words is left to its rows: not measured.
 */
static scr_bool
run_score_anywhere (scr_gameref_t game, const scr_char *string)
{
  static const scr_char *const OTHERS[] = {
    "get", "take", "pick", "from", "drop", "put", "leave", "wear", "remove",
    "strip", "sit", "stand", "lie", "x", "examine", "look at", "ex", "exam",
    "read", "shit", "fuck", "bastard", "cunt", "crap", "hell", "shag",
    "bollocks", "bollox", "piss", "bugger", "bloody", NULL
  };
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = run_get_version (bundle);
  const scr_char *const *word;
  const auto has_word = [&] (const scr_char *what) -> scr_bool
    {
      return run_c_word (version, string, what);
    };

  if (game->pending_endgame != 0 || !has_word ("score")
      || scr_strcasecmp (string, "score") == 0)
    return FALSE;
  for (word = OTHERS; *word; word++)
    if (has_word (*word))
      return FALSE;
  if (version >= TAF_VERSION_400 && has_word ("lay"))
    return FALSE;
  if ((version >= TAF_VERSION_380 && has_word ("look in"))
      || (version >= TAF_VERSION_390 && (has_word ("look") || has_word ("l"))))
    return FALSE;

  return lib_cmd_score (game);
}


/*
 * run_wait_anywhere()
 *
 * Every Runner answers a line holding the whole word `wait` ANYWHERE with
 * "Time passes..." and the wait turns, if no handler above has written a
 * message by then: `If c("wait") [Or line = "z"] And msg = ""` (run370
 * 43C1B3, run380 442A07, run390 45FCA2, run400 48ABB8 -- 4.0's c() too).
 * That gate sits after the tasks and the handlers that enter on words
 * anywhere in the line (takes, drops, wears, removes, sitstand, openclose,
 * examines, score, profanity), and before whereis, gotoplace and therest.
 * So `wait stone` with the stone elsewhere, `please wait`, `wait here`,
 * `push stone wait` and `turn wait` all pass time.  A line holding an
 * earlier handler's word is left to the rows that already answer it.
 * Give, say, inventory and a direction are NOT such words: inventory()
 * writes a message sitstand-fashion but nothing below wears() claims
 * without moving something, and give, say and the directions are all
 * `If msg = ""` arms below the gate -- so `give coin to bob wait`, `say
 * hello wait`, `i wait`, `inventory wait` and `n wait` are "Time passes..."
 * with nothing given, listed or walked (p37SITN..p4SITN,
 * harness/make_3738_sitnpcprobe.py: run370x runner_probes/sitn.run370.rtf,
 * run380x runner_probes/sitn.run380.rtf, run390x
 * runner_probes/sitn.run390.txt, run400x runner_probes/sitn.run400.txt,
 * 2026-09-20).  ask/talk stay with their rows: characters() overwrites the
 * wait text with the reply (`ask bob about hat wait` is "BOB HAT.").  `score
 * wait` is the score's (run_score_anywhere() runs first).  up/down/in/out with
 * wait: not measured.  run370's openclose writes nothing unless the line names
 * an openable object (4264E7), so there `open stone wait` passes time; 3.8 on
 * refuse the open.  `look wait` is examines' from 3.9 (bare `look`), time
 * passing at 3.7/3.8.
 *
 * gotoplace still runs after it and adds its "Unknown place." -- `goto hall
 * wait` answers both.  A goto that walks prints "Moving to..." straight to
 * the screen and jumps past the message print and the turn's tick (run370
 * 42BDEE "&&&"), so the "Time passes..." is lost and only the wait loop's
 * WaitTurns - 1 ticks run: the walk is administrative and the wait counter
 * stays set.  run370x reaches that with `wait goto hall`, the game's goto
 * word and "goto" each cut from the front.
 *
 * Measured 2026-09-19 on p37GOTO, p38GOTO, p39GOTO and p4EXAM
 * (harness/make_37_gotoprobe.py, make_38_gotoprobe.py,
 * make_39_gotoprobe.py, make_400_examprobe.py): run370x
 * runner_probes/goto.run370.wait.rtf, run380x
 * runner_probes/goto.run380.wait.rtf, run390x
 * runner_probes/goto.run390.wait.txt, run400x
 * runner_probes/exam.run400.wait.txt.
 */
static scr_bool
run_wait_anywhere (scr_gameref_t game, const scr_char *string)
{
  static const scr_char *const EARLIER[] = {
    "get", "take", "pick", "from", "drop", "put", "leave", "wear", "remove",
    "strip", "sit", "stand", "lie", "x", "examine", "look at", "ex", "exam",
    "read", "score", "ask", "talk", "up", "down", "in", "out",
    "shit", "fuck", "bastard", "cunt", "crap", "hell",
    "shag", "bollocks", "bollox", "piss", NULL
  };
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = run_get_version (bundle);
  const scr_char *const *word;
  const auto has_word = [&] (const scr_char *what) -> scr_bool
    {
      return run_c_word (version, string, what);
    };

  if (game->pending_endgame != 0 || !has_word ("wait"))
    return FALSE;

  /* The plain forms keep their own rows. */
  if (scr_strncasecmp (string, "wait", 4) == 0
      && strspn (string + 4, " 0123456789") == strlen (string + 4))
    return FALSE;

  for (word = EARLIER; *word; word++)
    if (has_word (*word))
      return FALSE;
  if ((version >= TAF_VERSION_390 && has_word ("bugger"))
      || (version < TAF_VERSION_400 && has_word ("bloody"))
      || (version >= TAF_VERSION_380 && has_word ("look in"))
      || (version >= TAF_VERSION_390 && (has_word ("look") || has_word ("l"))))
    return FALSE;

  if (has_word ("open") || has_word ("close"))
    {
      scr_int object;

      if (version >= TAF_VERSION_380)
        return FALSE;
      for (object = 0; object < gs_object_count (game); object++)
        {
          const scr_char *name =
              prop_get_indexed_string (bundle, "Objects", object, "Short");
          const scr_int openness = gs_object_openness (game, object);

          if ((openness == OBJ_OPEN || openness == OBJ_CLOSED)
              && name && name[0] != NUL && has_word (name))
            return FALSE;
        }
    }

  const size_t mark = pf_buffer_length (filter);
  const scr_bool was_admin = game->is_admin;

  lib_cmd_wait (game);
  const size_t after = pf_buffer_length (filter);
  game->is_admin = FALSE;
  if (lib_cmd_go_place (game) && game->is_admin)
    {
      const std::string moving = pf_cut_tail (filter, after);

      pf_truncate (filter, mark);
      pf_buffer_string (filter, moving.c_str ());
      return TRUE;
    }

  game->is_admin = was_admin;
  return TRUE;
}


/*
 * The steps of a `go <place>` walk still to be typed, and what to say on
 * arrival; see lib_cmd_go_place().  The Runner's route finder types each
 * direction into the input box and presses Return (SendKeys), so each step
 * is a line read at the prompt like any other: echoed, lower-cased, a turn
 * of its own.  run_goto_arrival_due marks the last step's turn, after which
 * run_main_loop() prints the arrival.
 *
 * run_goto_rest holds what was left of the typed line when the walk began
 * (`go to kitchen, look`).  Pre-4.0 generaltasks keeps its split queue in a
 * local (run390 var_E4, run380 var_E4), so each step, typed into the box
 * and run as a nested generaltasks, never sees it: the walk and its arrival
 * come first, then the rest of the line (run390x
 * runner_probes/goto.run390.gs.txt, run380x
 * runner_probes/goto.run380.gs.rtf).  run400 keeps the queue in the global
 * MemVar_4942E4 and empties it at the top of every generaltasks (48A01F),
 * so the first step throws the rest of the line away (run400x
 * runner_probes/goto.run400.gs.txt); see run_player_input().
 */
static std::vector<std::string> run_goto_steps;

static size_t run_goto_next = 0;

std::string run_goto_arrival;

scr_bool run_goto_arrival_due = FALSE;

static std::string run_goto_rest;

void
run_queue_goto_step (const scr_char *step)
{
  run_goto_steps.push_back (step);
}

void
run_set_goto_arrival (const scr_char *text)
{
  run_goto_arrival = text;
}

void
run_finish_goto_walk (void)
{
  run_goto_steps.clear ();
  run_goto_next = 0;
  run_goto_arrival.clear ();
  run_goto_arrival_due = FALSE;
}

void
run_cancel_goto_walk (void)
{
  run_finish_goto_walk ();
  run_goto_rest.clear ();
}

/*
 * scr_take_scripted_line()
 *
 * For the ports' line readers: TRUE and the next walk step in buffer if a
 * walk is under way, in place of reading from the player.  The port prints
 * its prompt first and echoes the step as if typed.
 */
scr_bool
scr_take_scripted_line (scr_char *buffer, scr_int length)
{
  if (run_goto_next >= run_goto_steps.size ())
    return FALSE;

  strncpy (buffer, run_goto_steps[run_goto_next++].c_str (), length - 1);
  buffer[length - 1] = NUL;
  if (run_goto_next >= run_goto_steps.size ())
    {
      run_goto_steps.clear ();
      run_goto_next = 0;
      run_goto_arrival_due = TRUE;
    }
  return TRUE;
}

/*
 * run_get_dispatch_input()
 *
 * Expose the stashed command element to other modules.  sclibrar's
 * SCR_TRACE_CO diagnostic needs the player's own words to reproduce the
 * Runner's whole-command containment test.
 */
const scr_char *
run_get_dispatch_input (void)
{
  return run_dispatch_input;
}

/*
 * Tasks that have already been run by the current command element, reset by
 * run_all_commands() alongside run_dispatch_input.
 *
 * A command is dispatched to the tasks twice, once with restrictions ignored
 * and once with them honoured, and the spent-task refusal pass below has to
 * know the difference between a task that was already done when the player
 * typed, and one this very command has just completed.  Only the first is a
 * refusal.  "Shadow of the Past" is the case: `examine good book` runs a
 * silent task (it drops a key and scores, and has no completion text), which
 * turns its own "book not yet crumbled" restriction false; the library
 * examine then prints the book's -- now crumbled -- description.  run400
 * answers the first `examine good book` with that description and only a
 * second one with the restriction's "The book is nothing but dust now."
 * (measured live 2026-08-23).
 */
std::vector<scr_bool> run_tasks_ran_this_command;

/* TRUE while a 4.0 line has a spent task's RepeatText to answer with; set
   by run_all_commands() from its REFUSAL_PASS_PROBE. */
scr_bool run_repeat_found_400 = FALSE;

/*
 * The last typed command as the dispatcher saw it, and whether a game task
 * ran for it: the pre-4.0 end-of-turn ambiguity prompt (see
 * lib_co_ambiguity_prompt()) fires only on a line no task claimed, and the
 * Runner's flag for that (MemVar_44F12C) is set at the same place any task
 * executes, library-callback tasks included.
 */
std::string run_co_pending_input;

scr_bool run_co_task_claimed = FALSE;

/*
 * The element as it reached run_all_commands(), before the give and
 * ask/talk reference rewrites: run390's checkverb compares the typed line
 * against its verb (42A4F4), so a bare `give` is still "give" there even
 * though the line it stores is the rewritten one.
 */
const scr_char *
run_get_line_input (void)
{
  return run_co_pending_input.empty () ? NULL : run_co_pending_input.c_str ();
}

/*
 * The line element `again` repeats.  run_player_input() owns it; it sits out
 * here so that run_session_state() can keep it across an autosave.
 */
scr_char run_prior_element[LINE_BUFFER_SIZE];

/*
 * The last two typed lines, lower-cased, as the Runner's command history
 * holds them: run390 shifts every new line into MemVar_468100 at 436268-
 * 43629F, run380 into MemVar_44F098, so (1) is the line being run and (2)
 * the one typed before it.  Blank lines go in too.  See run_with_history().
 */
std::string run_typed_line, run_previous_typed_line;

/*
 * The output that left the temporary game, and the undo game, in their
 * states; undo replays it (see lib_cmd_undo()).  Older undo states carry
 * theirs in the memo ring.
 */
std::string run_temporary_text, run_undo_text;

const std::string &
run_get_undo_text (void)
{
  return run_undo_text;
}


static scr_bool
run_npc_library_blocked (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;
  return run_any_task_ran_this_command ();
}

/*
 * run_npc_row_blocked()
 *
 * Whether a library row is one of the character-handler branches that the
 * Runner skips once a task has run for this line.  See the comment at the
 * call site in run_try_command_table() for the run400 addresses.
 */
static scr_bool
run_npc_row_blocked (const scr_commands_t *command)
{
  if (command->handler == lib_cmd_attack_npcs
      || command->handler == lib_cmd_attack_npcs_with)
    return TRUE;
  if (strstr (command->command, "%character%") == NULL)
    return FALSE;
  return command->handler != lib_cmd_give_object_npc
         && command->handler != lib_cmd_ask_npc_about
         && command->handler != lib_cmd_talk_to_npc_about
         && command->handler != lib_cmd_kiss_npc
         && command->handler != lib_cmd_status_npc
         && command->handler != lib_cmd_verb_npc;
}


/*
 * run_put_class_guard
 *
 * Confines the put rows to their class test for one run_all_commands() line
 * (run_put_class_only), and lets them go again when the line is done.
 */
struct run_put_class_guard
{
  explicit run_put_class_guard (scr_bool on) { run_put_class_only = on; }
  ~run_put_class_guard () { run_put_class_only = FALSE; }
  run_put_class_guard (const run_put_class_guard &) = delete;
  run_put_class_guard &operator= (const run_put_class_guard &) = delete;
};

/*
 * run_line_t
 *
 * One typed line on its way through run_all_commands(): the passes the
 * Runner's generaltasks makes over it, in its order, and the flags those
 * passes hand each other.  run() is the sequence; every pass is a method of
 * its own, documented where it is defined below.
 */
class run_line_t
{
public:
  run_line_t (scr_gameref_t game_, const scr_char *string_)
    : game (game_), filter (gs_get_filter (game_)), string (string_) {}
  run_line_t (const run_line_t &) = delete;
  run_line_t &operator= (const run_line_t &) = delete;

  scr_bool run ();

private:
  void reset ();
  scr_bool lenient ();
  scr_bool spent_claim_390 ();
  void put_prepass ();
  void put_pass ();
  void get_outer_400 ();
  void task_peek ();
  void priority_pass ();
  void task_passes ();
  void battle_pass ();
  void library ();
  void settle ();
  scr_bool finish (scr_bool result);

  const scr_gameref_t game;
  const scr_filterref_t filter;
  const scr_char *const string;

  /* status is TRUE once a handler has answered the line; refused, once a
     put has printed a refusal and left the line to the task passes. */
  scr_bool status = FALSE, refused = FALSE;
  /* The register uip_rewrite_references() reads, as the line found it, and
     whether the "(<npc>)" echo came out; see spent_claim_390(). */
  scr_int prior_npc = -1;
  scr_bool ask_echo = FALSE;
  /* A spent 4.0 task's RepeatText answers the line (repeat_found) and, with
     no survivor handler, keeps the priority rows off it (repeat_pending). */
  scr_bool repeat_found = FALSE, repeat_pending = FALSE;
  /* put_drop_list's view of the line: the hoisted line it reads, its
     clauses, whether a named put row has it first, and the battle verbs
     beside it; see put_prepass(). */
  std::string put_hoisted;
  const scr_char *put_line = string;
  std::vector<std::string> put_clauses;
  scr_bool put_first = FALSE, put_contained = FALSE;
  scr_int battle_kinds = 0;
  /* The inventory listing printed ahead of the tasks; see put_pass(). */
  scr_bool inv_listed = FALSE;
  /* The line the task passes dispatch against. */
  const scr_char *task_string = string;
  /* get_outer's view of a 4.0 take line, and what its get_piece did; see
     get_outer_400(). */
  std::string outer_battle;
  const scr_char *outer_line = string;
  scr_bool outer_twice = FALSE, outer_silent = FALSE;
  size_t outer_mark = 0;
  /* Where the buffer stood when the task dispatcher got the line, and what
     the passes before and after it settled; see task_peek() and
     task_passes(). */
  size_t task_mark = 0;
  scr_bool claimed_before_tasks = FALSE, silent_before_priority = FALSE;
  scr_bool task_claimed = FALSE, silent_task_390 = FALSE;
  scr_bool silent_names_npc_390 = FALSE;
  /* The line the take and drop rows read, and its goto class; see
     priority_pass(). */
  std::string priority_hoisted, priority_goto_rest;
  const scr_char *priority_line = string;
  scr_int priority_goto = RUN_GOTO_NONE;
  /* Whether the library cascade got the line at all; see battle_pass(). */
  scr_bool reached_library = FALSE;
};

/*
 * run_line_t::reset()
 *
 * The per-line stores generaltasks makes before it looks at the line.
 */
void
run_line_t::reset ()
{
  run_dispatch_input = string;
#ifdef SCARIER_DUMP_TOOLS
  run_trace_last_input = string;
#endif
  run_co_pending_input = string;
  run_co_task_claimed = FALSE;
  run_tasks_ran_this_command.assign (gs_task_count (game), FALSE);
  lib_verb_object_note_line_top (game);
  lib_co_note_line_top (game);
  obj_mark_npc_parts_seen (game);
  lib_prepass_seen_3738 (game, string);
}

/*
 * run_line_t::lenient()
 *
 * Whether the line matches a task only leniently, for the lenient-task
 * guard run() holds over the whole line.
 */
scr_bool
run_line_t::lenient ()
{
  /* Deliberate deviation: see run_line_matches_task_strictly().  The peek
     runs strictly, so the flag is cleared for it first. */
  const scr_bool outer_lenient = run_lenient_tasks;
  run_lenient_tasks = FALSE;
  uip_set_lenient_tasks (FALSE);
  const scr_bool line_lenient = !run_line_matches_task_strictly (game, string);
  run_lenient_tasks = outer_lenient;
  return line_lenient;
}

/*
 * run_line_t::spent_claim_390()
 *
 * The ask/talk echo and the character noting that come out ahead of
 * everything, then the pre-4.0 spent task's claim on the line.  Returns
 * TRUE when that claim ends the line.
 */
scr_bool
run_line_t::spent_claim_390 ()
{
  /*
   * The "(<npc>)" an "ask about" / "talk about" echoes comes out ahead of
   * everything, task matching included; see uip_print_ask_echo().  The
   * characters this line names are noted here too: run400 notes them inside
   * characters(), which every line reaches (48B56E is below the task
   * dispatch's exits), so a task-answered line names its characters just
   * the same.  Both read the register before the noting, so remember what
   * it held for the rewrite in library().
   */
  prior_npc = game->last_npc;
  ask_echo = uip_print_ask_echo (game, string);
  uip_note_named_npcs (game, string);

  /*
   * Pre-4.0 the task dispatcher's claim comes first: a spent task whose
   * command matches the line prints its RepeatText slot and nothing below
   * tasks(0) runs -- see run_spent_task_390().  The handlers run390 runs
   * above it, and the character pass that overwrites its message, still
   * answer; they are a turn either way.  Matched on the line as typed: the
   * give and ask/talk rewrites are further down generaltasks.
   */
  if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
      && !run_repeat_assist)
    {
      const scr_char *message;
      const scr_int spent = run_spent_task_390 (game, string, &message);

      if (spent >= 0)
        {
          if (!run_spent_survivor_390 (game, string))
            run_spent_claim_390 (game, message);
          return TRUE;
        }
    }
  return FALSE;
}

/*
 * run_line_t::put_prepass()
 *
 * What the passes below need to know before any of them speaks: whether
 * a spent task answers the line, and put_drop_list's view of it.
 */
void
run_line_t::put_prepass ()
{
  /*
   * 4.0 puts are the exception to the peek: the library's put-in / put-on
   * gets the line BEFORE any task does, and a task only ever answers a put
   * the library could not complete.  Measured with the PUT4-PUT7 arena probes
   * (runner_probes/put[4-7].run400*.txt, 2026-09-05), each pairing a passing
   * put task with reporter tasks restricted on "object is inside container":
   * run400 puts the pill in the cup and the task never runs, whatever its
   * spelling -- wildcard, prefixed or literal -- while run390 on the 3.9 twin
   * (put39.taf, runner_probes/put.run390.b.txt) hands the line to the task
   * and moves nothing.  Read off run400's insides handler
   * (Proc_19_43_46639C): a completed move returns 1 and the dispatcher stops
   * there.  The peek pass would have handed the line to a non-silent matching
   * task first, which is what cost "Sommeril" its `put fish in fountain`
   * (task 18's text where the Runner puts the fish,
   * runner_probes/sommeril.run400.probe.txt) and left `take wet page`
   * answered on the next turn where run400 says "Take what?".  Pre-4.0 keeps
   * the peek order: Scarier matches run390 on the put39 probe on all twelve
   * lines (the last of them, the prefixed `put a bean in a jar` row, closed
   * 2026-09-20 with the pre-4.0 canonical-retry suppression in sclibrar.cpp).
   *
   * A put the 4.0 library REFUSES on size or capacity is different again:
   * every message path of that handler exits without setting its return
   * byte, so the refusal is printed but the line is not claimed, and the
   * task passes then answer it as well, run straight on from the refusal
   * with the two-space separator -- "The rock is too big to fit inside the
   * slot.  PUTBIG." (PUT7, runner_probes/put7.run400.b.txt; Zack
   * Smackfoot's `put knife in slot`,
   * runner_transcripts/zacksmackfoot.txt).  The handler signals that with
   * run_priority_refuse(); the join is left pending so that a refusal no
   * task follows stands alone.  Either way the line is handled once the
   * refusal is printed, so the standard table's duplicate put rows never
   * print it a second time.
   */
  /*
   * A 4.0 line that a spent task answers with its RepeatText is not the
   * priority commands' either -- run400 dispatches tasks at 48A481, above
   * everything but inventory (48A457) and the put/drop list (48A462), so
   * `take coin` on a done `take coin` task prints the RepeatText and takes
   * nothing, and `put all on desk` on a done one prints it rather than the
   * put's own empty-handed answer.  The message itself waits until the task
   * passes below have all declined, because a task that can still RUN
   * outranks one that is merely spent; all that is needed here is whether
   * there IS one, so that the priority commands stand aside.
   * run_repeat_survivor_400() holds the handlers that keep the line anyway;
   * they are still a turn, though, because the dispatcher sets its handled
   * byte before the character pass overwrites the message (`x bob` on a
   * spent task ticks the probe's event, where a plain NPC examine does not
   * -- runner_probes/repeat2.run400.txt, and see the survivor's turn in
   * settle()).
   */
  repeat_found = run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
                 && run_task_refusal (game, string, REFUSAL_PASS_PROBE);
  repeat_pending = repeat_found && !run_repeat_survivor_400 (game, string);
  run_repeat_found_400 = repeat_found;

  /*
   * put_drop_list is one of the handlers 4.0 enters on its verb ANYWHERE in
   * the line, so this pre-pass reads the same hoisted line the library does
   * further down -- `blorp drop coin` is "You drop the coin.", not the
   * leftover-word answer the line as typed scores.  See run_hoist_verb_line().
   */
  /* A battle verb beside another handler's word; see run_battle_line().
     4.0's put_drop_list, like get_outer_400(), takes its word anywhere on
     such a line: `hit bob drop coin` with the coin loose is "You are not
     holding the coin.", no blow. */
  battle_kinds = repeat_pending ? 0 : run_battle_line_class (game, string);
  if ((battle_kinds & RUN_BATTLE_DROP)
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    {
      /* The words after the drop word: "drop bob coin" finds no coin. */
      const scr_char *const word = run_battle_kind_word (TAF_VERSION_400,
                                       string, RUN_BATTLE_DROP);
      const scr_int at = run_c_word_pre400 (TAF_VERSION_400, string, word);

      put_hoisted = at == 0
          ? run_battle_respell (game, string, RUN_BATTLE_DROP, word)
          : std::string (string + at);
      put_line = put_hoisted.c_str ();
    }
  else if (run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
           && run_hoist_verb_line (game, string, put_hoisted))
    put_line = put_hoisted.c_str ();
  /*
   * put_drop_list's own clause loop, carved before anything else looks at
   * the line: run400 enters the routine on the whole-word "put"/"drop"
   * alone, so whether the line has a NAMED put row is asked of the first
   * CLAUSE, which is what the Runner actually resolves.  See
   * lib_put_clauses_400() and put_pass().
   */
  if (run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
      && !repeat_pending)
    lib_put_clauses_400 (game, put_line, put_clauses);
  if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
      || repeat_pending)
    put_first = FALSE;
  else if (put_clauses.empty ())
    put_first = run_is_put_command_400 (game, put_line, &put_contained);
  else
    put_first = run_is_put_command (game, put_clauses[0].c_str ());
}

/*
 * run_line_t::put_pass()
 *
 * The handlers run400 runs above the task dispatcher: put_drop_list's
 * take-from, the inventory listing and the named put rows, clause by
 * clause.
 */
void
run_line_t::put_pass ()
{
  if (!repeat_pending && run_put_take_400 (game, string))
    {
      status = TRUE;
      put_first = FALSE;
    }
  /*
   * The inventory listing at 48A457 comes out ahead of the dispatcher too,
   * but unlike the put/drop rows it does not take the line away from the
   * task: 48A481 runs immediately afterwards and appends the CompleteText to
   * the listing.  So print it here and leave `status` alone, letting the
   * task passes below run on the same line; the listing claims whatever they
   * decline.  See run_is_inventory_command().
   *
   * run380 and run370 do the same: generaltasks stores inventory()'s result
   * in a scratch variable (run380 4421C2, run370 43B961) without the GoTo
   * that takes() and drops() get, and tasks(0) follows (4421F6 / 43B97F).
   * inventory() prints its listing directly (run380 42E218).  Measured on
   * wrecked T24 (run380x): "I am wearing my clothes, ..." and then task 43's
   * "Boff says ...".  Only run390 lets the listing claim the line (45F45B).
   */
  inv_listed = run_get_version (gs_get_bundle (game)) != TAF_VERSION_390
               && run_is_inventory_command (game, string)
               && run_priority_commands (game, string);
  if (put_first)
    {
      /*
       * `put coin in box and hat in desk` is put_drop_list's own multi-clause
       * loop (459C75), not the top-level splitter's: the " and " sits at or
       * beyond the preposition split, so the line arrived here whole, and the
       * Runner runs the put rows once per clause inside the ONE turn.  Their
       * answers come out back to back with no separator of any kind, which is
       * exactly what falls out of running the priority pass again on the same
       * line.  lib_put_clauses_400() carves the clauses; a trailing clause
       * with no preposition of its own is dropped there, unrun.
       */
      if (!put_clauses.empty ())
        {
          std::vector<std::string>::const_iterator clause;

          run_put_clause_loop_active = TRUE;
          for (clause = put_clauses.begin ();
               clause != put_clauses.end (); ++clause)
            {
              const scr_bool is_last = (clause + 1 == put_clauses.end ());
              const run_dispatch_input_guard input (clause->c_str ());

              if (run_priority_commands (game, clause->c_str ()))
                status = TRUE;
              /*
               * A refusing clause has left its message pending, and the
               * next clause's pass would reset the flag out from under it,
               * so settle it here rather than once after the loop.  A
               * refusal-only put ends without a terminator of its own (see
               * lib_put_in_refused), and between clauses run400 shows that
               * plainly: "The coin is too big to fit inside the box.You
               * can't put anything inside the desk!" is what the Runner's
               * own scrollback holds, with no separator at all where the
               * next clause begins (p4AND, runner_probes/and.run400.c.txt
               * + DUMP_SCROLLBACK, 2026-09-08).  Only the last clause
               * keeps the pending join, which is what a task answering the
               * same line wants.
               */
              refused = run_priority_refused;
              if (refused)
                {
                  pf_note_trailing_auto_break (filter);
                  if (is_last)
                    pf_buffer_join_pending (filter);
                  else
                    pf_undo_auto_break (filter);
                }
              /*
               * name_object answers every clause where it stands, so a
               * clause the tentative priority pass deferred (a put whose
               * target is not a container, which run400 lets a loud task
               * outrank -- see run_priority_commands) has to be finished
               * here, out of the duplicate STANDARD_COMMANDS rows.  Only
               * the LAST clause is left to the whole-line passes below,
               * which parse the line's final preposition and so answer for
               * that clause anyway.
               */
              if (!is_last && !status && !refused && run_priority_deferred
                  && run_standard_commands (game, clause->c_str ()))
                status = TRUE;
            }
          run_put_clause_loop_active = FALSE;
          refused = refused && !status;
        }
      else
        {
          {
            const run_dispatch_input_guard input (put_line);

            uip_set_containment (put_contained);
            status = run_priority_commands (game, put_line);
            uip_set_containment (FALSE);
          }
          refused = !status && run_priority_refused;
          if (refused)
            {
              pf_note_trailing_auto_break (filter);
              pf_buffer_join_pending (filter);
            }
        }
    }
}

/*
 * run_get_motion_task_line()
 *
 * Deliberate deviation (2026-09-30).  run400's get_outer takes the object a
 * get line names before the task dispatcher runs, whatever words stand
 * between the verb and the name, so The X-Files' `get in the van` answers
 * "You take VW Van." and its task 26 `*Van*`, which gets the player in,
 * never runs (runner_probes/xfiles.run400.t76.txt).  TRUE when LINE is
 * "get" followed by a word that makes it a movement -- get in, into, on,
 * out, under, behind, through, up, down, ... -- and a task matches the typed
 * line.  get_outer then leaves the line to the tasks.  A plain `get van`,
 * and a movement line no task takes, keep the Runner's take.
 */
static scr_bool
run_get_motion_task_line (scr_gameref_t game, const scr_char *line,
                          const std::string &task_line)
{
  static const scr_char *const MOTION[] = {
    "in", "into", "inside", "on", "onto", "out", "under", "underneath",
    "beneath", "behind", "through", "over", "across", "aboard", "up", "down",
    "back", "away", "near", "beside", "onboard", NULL
  };
  const scr_char *next;
  size_t length;
  scr_int index_;

  if (strncmp (line, "get ", 4) != 0)
    return FALSE;
  next = line + 4;
  while (*next == ' ')
    next++;
  length = strcspn (next, " ");
  for (index_ = 0; MOTION[index_]; index_++)
    {
      if (strlen (MOTION[index_]) == length
          && strncmp (next, MOTION[index_], length) == 0)
        return run_line_matches_task_strictly (game, task_line.c_str ());
    }
  return FALSE;
}

/*
 * run_line_t::get_outer_400()
 *
 * run400's get_outer, between put_drop_list and the task dispatcher: a
 * take line's own pre-match against the take-family tasks, and the take
 * it makes when they decline.
 */
void
run_line_t::get_outer_400 ()
{
  /*
   * A 4.0 put-in whose direct object named nothing has already rewritten the
   * command line the tasks are dispatched against, and the Runner never puts
   * it back -- run_priority_unnamed_put_object() has the whole of it -- so
   * IceCream's own `put ice cream in cone` and advent350b's `drop bear` never
   * reach the tasks written for them.  Deliberate deviation (2026-09-27): the
   * task passes get the line as typed.
   */
  task_string = string;

  /*
   * run400's get_outer (4582D8) sits between put_drop_list and the
   * dispatcher, and its first act is `If c("empty") Then line = Replace(line,
   * "empty ", "get all from ")`: a whole-word "empty" ANYWHERE turns the line
   * into a take-from, so onnafa's `give empty beer mug to perry` first says
   * "You can't take anything from the beer mug." and `x empty beer mug`
   * with the mug gone is "I don't understand where you want to get things
   * from." (6095ff3b0).  Deliberate deviation: Scarier leaves a line that
   * only names an "empty ..." object alone; a line starting with "empty" is
   * the take-from rows' either way.
   */

  /*
   * get_outer (4582D8) runs at 48A46D, BEFORE the task dispatcher at 48A481.
   * On a get/take line with no whole-word "all" or "and", its get_piece
   * (473A34) first pre-matches the line against take-family tasks (453C50
   * class 1, 472D9C); a hit dispatches the task.  A miss goes on to the
   * library take, so a line whose task only matches in the dispatcher proper
   * is answered as a take.  Measured on The X-Files T76: "get in the van"
   * gives "You take VW Van." (runner_probes/xfiles.run400.t76.txt), where
   * T55's same line, with task 13 pre-matching, gets in.
   */
  /*
   * On a battle line get_outer is entered on its word anywhere, and claims
   * the way it always does, refusal included: `hit bob take coin` takes the
   * coin (or refuses) and strikes nothing, `take hit bob` names nothing to
   * take and is a blow.  See run_battle_line().
   */
  if ((battle_kinds & RUN_BATTLE_TAKE)
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    {
      outer_battle = run_battle_respell (game, string, RUN_BATTLE_TAKE,
          run_battle_kind_word (TAF_VERSION_400, string, RUN_BATTLE_TAKE));
      outer_line = outer_battle.c_str ();
    }
  if (!status && !refused && !put_first && !repeat_pending && !inv_listed
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
      && (strncmp (outer_line, "get ", 4) == 0
          || strncmp (outer_line, "take ", 5) == 0
          || strncmp (outer_line, "pick ", 5) == 0)
      && !lib_input_contains_word (outer_line, "all")
      && !lib_input_contains_word (outer_line, "and"))
    {
      /* The piece names its object by whole-word score, so "get in the
         van" is a take of the van; see lib_take_scored_400(). */
      auto outer_take = [&] () -> scr_bool
        {
          const run_dispatch_input_guard input (outer_line);
          scr_bool taken;

          taken = run_priority_commands (game, outer_line);
          if (!taken)
            {
              const scr_ref_number_guard ref_number (game);
              taken = lib_take_scored_400 (game);
            }
          return taken;
        };
      const scr_int kind = lib_task_prematch_kind_input (game, 1);

      if (kind == 2)
        {
          /*
           * get_piece dispatches a pre-matched task at 472DE7 but claims the
           * line only when the pre-matcher answered 1, a task with text of
           * its own.  A textless task (2) runs there and get_piece carries
           * on, resolving its noun only now.  make_400_takedoubleprobe.py
           * measures both halves (run400x runner_probes/tdbl.run400.txt,
           * runner_probes/tdbo.run400.txt).
           */
          outer_mark = pf_buffer_length (filter);
          run_game_commands_in_parser_context (game, task_string, TRUE, FALSE);
          if (lib_take_names_dynamic_400 (game, outer_line))
            {
              /*
               * An object in reach is taken, or refused, as usual, and the
               * task ran once only.  The take line comes first and the
               * task's text follows it (47359A, lib_take_backend_common());
               * a refusal is appended.  p4TDBO `get ball` "Player take the
               * ball.  BOBTEXT.", `get gem` held "BOBTEXT.Player is already
               * carrying the gem."; an object the task brought within reach
               * too, sommeril T49 `take silver key` "You are already
               * carrying the SILVER KEY." (runner_transcripts/sommeril.txt).
               */
              std::string task_text = pf_buffer_tail (filter, outer_mark);
              scr_int held_before = 0, held_after = 0, object;

              pf_truncate (filter, outer_mark);
              for (object = 0; object < gs_object_count (game); object++)
                held_before += gs_object_position (game, object)
                               == OBJ_HELD_PLAYER;
              status = outer_take ();
              for (object = 0; object < gs_object_count (game); object++)
                held_after += gs_object_position (game, object)
                              == OBJ_HELD_PLAYER;
              if (!task_text.empty ())
                {
                  if (held_after > held_before)
                    pf_buffer_join (filter, task_text.c_str ());
                  else
                    {
                      std::string answer = pf_buffer_tail (filter, outer_mark);

                      pf_truncate (filter, outer_mark);
                      while (!task_text.empty () && task_text.back () == '\n')
                        task_text.pop_back ();
                      pf_buffer_string (filter, task_text.c_str ());
                      pf_buffer_string (filter, answer.c_str ());
                    }
                  status = TRUE;
                }
            }
          else if (lib_task_prematch_kind_input (game, 1) != 0)
            {
              /*
               * With no object its 473241 pre-match hits again and exits
               * silently, and the dispatcher at 48A481 runs the typed line
               * AGAIN.  British Fox T318 `get grace` prints task 123's text
               * twice (runner_transcripts/britishfox.txt); p4TDBL `get bob`
               * "BOBTEXT.  BOBTEXT." with its counter up by two, `get eel`,
               * nothing printed by either run, "I don't understand.".
               */
              outer_twice = TRUE;
            }
          else
            {
              /*
               * A task the run spent misses 473241, and get_piece goes on to
               * 47332B: "Take what?" into an empty buffer, and the line is
               * get_outer's.  FunHouse T2 `pick up money`, task 11 once-only
               * and silent (score only): "Take what?" and the turn ticks
               * (runner_transcripts/funhouse.txt); p4TDBL's first `get dog`
               * prints the text its execute action set off, and nothing more.
               */
              if (pf_buffer_length (filter) == outer_mark)
                pf_buffer_string (filter, "Take what?\n");
              status = TRUE;
            }
        }
      else if (run_get_motion_task_line (game, outer_line, task_string))
        {
          /* Not a take: the dispatcher has the line.  See
             run_get_motion_task_line(). */
        }
      else if (lib_take_names_dynamic_400 (game, outer_line))
        {
          if (kind != 0)
            {
              /*
               * The pre-match hit on a task with text, or on a failing
               * restriction's message, is get_piece's claim, loud: the
               * FailMessage prints through 45404C (44CCA5) before any
               * library take could.  bigspy2 T2 `get puzzle`, the boy in the
               * room: "Hey, leave my sliding puzzle alone..." (runner_
               * transcripts/bigspy2.txt); advent350b T386 `get chain`,
               * "It's locked to the friendly bear." (runner_transcripts/
               * advent350b.txt:1753).
               */
              status = run_game_commands_in_parser_context (game, task_string,
                                                            TRUE, FALSE);
            }
          else
            status = outer_take ();
        }
    }
}

/*
 * run_line_t::task_peek()
 *
 * The task dispatcher's first look at the line, ahead of the take and
 * drop rows: the all-or-nothing peek of the design note in
 * run_all_commands().
 */
void
run_line_t::task_peek ()
{
  if (!status && !refused
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    run_restriction_cache_task_pick (game, task_string);

  /*
   * Below 3.90 a drop line's one and only look at the task matcher is the
   * one inside drops(), which silences a matched task when nothing named is
   * in hand -- deliberately not ported; see sclibrar_drop.inc.
   */

  /*
   * A put line with no in/on split that a task pre-matches reaches
   * name_object's mode-2 scorer, whose flat tie answer prints ahead of the
   * task and does not claim the line; see lib_put_task_tie_400().
   */
  if (!status && !refused && !put_first && !repeat_pending && !inv_listed
      && lib_put_task_tie_400 (game, put_line))
    {
      pf_note_trailing_auto_break (filter);
      pf_buffer_join_pending (filter);
    }

  task_mark = pf_buffer_length (filter);
  claimed_before_tasks = status;
  if (!status && !refused)
    {
      /* get_piece's run does not count as the dispatcher's one task, and
         the take it stood in for has had its turn: no silent-literal peek
         to hand the line back to it. */
      run_matcher_second_pass = outer_twice;
      status = run_game_commands_in_parser_context (
                   game, task_string, FALSE,
                   !outer_twice
                   && run_get_version (gs_get_bundle (game))
                      >= TAF_VERSION_400);
      run_matcher_second_pass = FALSE;
      if (run_any_task_ran_this_command ())
        run_takes_second_pass_370 (game, string, task_string, task_mark);
      /* A silent task on a take or drop line: the handler's own "Take
         what?" / "Drop what?" claims it (see the helper). */
      if (!status && run_any_task_ran_this_command ()
          && pf_buffer_length (filter) == task_mark
          && lib_move_what_after_silent_task_pre400 (game))
        status = TRUE;
    }
  /*
   * Below 4.0 a task the dispatcher ran that printed nothing, and that no
   * "Take what?" / "Drop what?" claimed, keeps the take and drop handlers
   * out as well: the line is the silent claim's below.  p3xBEYOND
   * (make_beyondprobe.py, 2026-09-26): run370 `take orb` with the orb on the
   * floor runs silent task "take orb" twice and answers "I don't
   * understand.", the orb left there (runner_probes/beyond.run370.c.rtf);
   * run390 `drop cape to the floor` with the cape on the floor scores the
   * silent task and says "I don't understand." rather than "You don't have
   * the red cape!"  (runner_probes/beyond.run390.c.txt).
   */
  silent_before_priority = !claimed_before_tasks && !status
      && run_any_task_ran_this_command ()
      && pf_buffer_length (filter) == task_mark
      && run_get_version (gs_get_bundle (game)) < TAF_VERSION_400;
}

/*
 * run_line_t::priority_pass()
 *
 * The priority rows -- the takes and drops that move objects to and from
 * inventory -- on the line they read, with gotoplace's turn after them.
 */
void
run_line_t::priority_pass ()
{
  /*
   * The take and drop rows live in the priority table, not in the library
   * cascade below, and pre-4.0 takes() and drops() are entered on their verb
   * ANYWHERE in the line exactly as the cascade's handlers are -- run380's
   * takes 43D788 / drops 438659, run370's 435E28 / 430475, run390's 454428 /
   * 445500, every one of them a disjunction of c() tests.  So this pass reads
   * the same hoisted line the cascade does; `blorp take coin` is a take of
   * the coin.  See run_hoist_verb_line(), and lib_move_named_whole_line_pre400()
   * for the noun half that then has to find "coin" past the nonsense word.
   */
  if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400)
    {
      /* A leading goto word is no verb to takes() and drops(); see
         run_goto_line_class(). */
      priority_goto = run_goto_line_class (game, string);
      if (priority_goto != RUN_GOTO_NONE
          && run_goto_strip_head (game, string, priority_goto_rest))
        priority_line = priority_goto_rest.c_str ();
      if (run_hoist_verb_line (game, priority_line, priority_hoisted))
        priority_line = priority_hoisted.c_str ();
    }
  else if (battle_kinds & RUN_BATTLE_DROP)
    priority_line = put_line;
  /* A 4.0 list line with a second verb is put_drop_list's or get_outer's
     own list; see run_two_verb_line_400(). */
  else if ((run_c_word_pre400 (TAF_VERSION_400, string, "all") >= 0
            || run_c_word_pre400 (TAF_VERSION_400, string, "and") >= 0)
           && run_two_verb_line_400 (game, string, priority_hoisted))
    priority_line = priority_hoisted.c_str ();

  if (!status && !put_first && !inv_listed && !repeat_pending
      && !silent_before_priority)
    {
      const size_t goto_mark = pf_buffer_length (filter);
      std::vector<scr_int> goto_places;
      scr_int object;

      if (priority_goto == RUN_GOTO_TAKE || priority_goto == RUN_GOTO_DROP)
        for (object = 0; object < gs_object_count (game); object++)
          {
            goto_places.push_back (gs_object_position (game, object));
            goto_places.push_back (gs_object_parent (game, object));
          }
      const std::vector<scr_int> battle_places
          = battle_kinds ? run_battle_places (game) : std::vector<scr_int> ();
      {
        const run_dispatch_input_guard input (priority_line);

        status = run_priority_commands (game, priority_line);
      }
      /* Below 4.0 a take or drop that only refused is wiped by dobattle,
         which then has the line; 4.0's put_drop_list and get_outer claim
         on a refusal.  See run_battle_line(). */
      if (status && battle_kinds
          && run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
          && run_battle_places (game) == battle_places)
        {
          pf_truncate (filter, goto_mark);
          status = FALSE;
        }
      /* A take that refused leaves gotoplace its turn: `take box goto
         cave` with the box in hand is "You've already got a box!Unknown
         place." below 4.0.  See run_goto_after(). */
      if (status && !goto_places.empty ())
        status = run_goto_after (game, string, priority_goto, goto_mark,
                                 goto_places, status);
      /*
       * The all/everything put rows are not put_first (see
       * run_is_put_command), so their tentative pass runs here, and a
       * refusal-only put among them -- the 4.0 closed-container refusal
       * from lib_put_in_closed_400(), a size or capacity refusal -- has
       * printed its refusal and left the line for the task passes exactly
       * as a named row's does in put_pass().  Settle it the same way, or the
       * STANDARD_COMMANDS twin below prints the refusal a second time:
       * probe PCLOSED `put all in chest` with the chest shut and a stone in
       * hand is one "The chest is closed!"
       * (runner_probes/closed.run400.feed2.txt:7).
       */
      if (!status && run_priority_refused)
        {
          refused = TRUE;
          pf_note_trailing_auto_break (filter);
          pf_buffer_join_pending (filter);
        }
    }
}

/*
 * run_line_t::task_passes()
 *
 * The task dispatcher proper: once ignoring failed restrictions, once
 * with them, and what a task that ran and printed nothing means below
 * 4.0.
 */
void
run_line_t::task_passes ()
{
  if (!status)
    status = run_game_commands_in_parser_context (game, task_string,
                                                  FALSE, FALSE);
  if (!status && !inv_listed
      && !run_defer_loud_tasks_to_movement (game, task_string))
    status = run_game_commands_in_parser_context (game, task_string,
                                                  TRUE, FALSE);
  task_claimed = !claimed_before_tasks && status
                 && run_any_task_ran_this_command ();
  /*
   * 3.9: a task that ran and printed nothing claims the line in run390 --
   * tasks() returns it, generaltasks skips everything below the dispatcher,
   * and an empty buffer is printed as the game's DontUnderstand text -- and
   * the line is not a turn.  Both halves are ours now: the clock first
   * (2026-09-19; ALEXIS.TAF T99 `open chest`, task 14 `open * chest` with no
   * CompleteText, draws nothing in run390x per alexis_tr_trace.txt, and
   * ticking there put every later battle roll a turn out of phase), then the
   * claim (2026-09-20), which is the `!silent_task_390` guard on library():
   * status stays FALSE, so run_process_input_line() prints
   * DontUnderstand, and nothing between here and there speaks.
   *
   * It is what run390 answers `read diary` in everything, `piss` in life,
   * `turn off tv` in lifesimulation and `open chest` in alexis with, and in
   * the_hangover it also keeps the filing cabinet shut: the task matched and
   * did nothing, so the library never opens it and the approval form stays
   * inside.  See the note "Measured 2026-08-23 (make_39_doneprobe.py" above.
   *
   * run370 and run380 claim the same way (p3xBEYOND, 2026-09-26: silent task
   * "wave cape slowly" +1000 is "I don't understand." where the object
   * catch-all would say "... with the red cape.",
   * runner_probes/beyond.run370.c.rtf, runner_probes/beyond.run380.c.rtf),
   * but there the line stays a turn: neither has the not-a-turn byte below
   * 3.90.
   */
  silent_task_390 = !claimed_before_tasks && !status
      && run_any_task_ran_this_command ()
      && pf_buffer_length (filter) == task_mark
      && run_get_version (gs_get_bundle (game)) < TAF_VERSION_400;
}

/*
 * run_line_t::battle_pass()
 *
 * dobattle's stamina recovery and the battle line itself, and the claims
 * a refusal, an inventory listing or get_piece's double run make on a
 * line no task answered.
 */
void
run_line_t::battle_pass ()
{
  /*
   * dobattle (run400 Proc_11_4_47F084), called from generaltasks at 48A4A2
   * when the Battle System is on, opens with the stamina recovery loop for
   * every character, 47E682-47E764 -- see battle_recover_line().  It runs
   * here, once per line element, ahead of every library verb, and it is
   * gated the way the Runner's control flow gates it:
   *
   *  - Four handlers above it end the line by GoTo loc_48B4E3 when they
   *    claim it: the inventory listing 48A457, put_drop_list 48A462
   *    (result set only when name_object 46E5D8 answered, 459CB0/459DAD),
   *    get_outer 48A46D (result set only when the per-piece get 473A34
   *    answered, 458200/4582B9), and the task dispatcher 48A481, whose tail
   *    at 44CCC0 forces its result to 0 when the message buffer is empty.
   *    That is `status` here: a silent task leaves it FALSE and recovers,
   *    a put refusal leaves it FALSE too (`refused` is turned into a claim
   *    only below), and the inventory listing by itself does not claim.
   *  - A spent task's RepeatText is printed by the dispatcher and claims
   *    the line (`repeat_found`; its survivors run from 48B4E3, still past
   *    the call).
   *  - The not-a-turn byte MemVar_494281: reset at 48A010 for each element,
   *    and set inside dobattle itself at 47DCB9 by the status path, `If
   *    c("status") And MemVar_4941B0 = ""` -- the whole word anywhere in the
   *    line, with nothing yet in the message buffer -- before the loop at
   *    47E682 tests it.  No other store reaches the test on this path.
   *
   * So a line that is not a turn (`score`, a line nobody understands) still
   * recovers, `wait` recovers once per typed line and not per waited turn,
   * a Who-continuation recovers twice, and a claimed line not at all.  The
   * counters start at Recovery (battle_start()), so the first point lands
   * Recovery lines in.  Recovery is a 4.0 property; 3.9 has none of this.
   */
  if (!status && !repeat_found
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
      && battle_is_enabled (game)
      && !(pf_buffer_length (filter) == 0
           && lib_input_contains_word (string, "status")))
    battle_recover_line (game);

  if (refused)
    {
      pf_clear_join_pending (filter);
      status = TRUE;
    }
  if (inv_listed)
    status = TRUE;
  /*
   * After get_piece's double run the library has nothing left for the line:
   * get_outer was its take.  Whatever either run printed answers it;
   * nothing at all is DontUnderstand (p4TDBL `get eel`,
   * runner_probes/tdbl.run400.txt).
   */
  outer_silent = outer_twice && !status;
  if (outer_silent && pf_buffer_length (filter) > outer_mark)
    status = TRUE;
  if (!status && !silent_task_390 && !outer_silent && battle_kinds)
    status = run_battle_line (game, string, battle_kinds);
  reached_library = !status && !silent_task_390 && !outer_silent;
}

/*
 * run_line_t::library()
 *
 * The standard library cascade, on the line rewritten around the last
 * character named and hoisted the way 4.0's handlers read it, with the
 * already-done refusals slotted where the Runner has them.
 */
void
run_line_t::library ()
{
  if (status || silent_task_390 || outer_silent)
    return;

  /*
   * Only now, with every task pass declined, does the Runner rewrite a
   * "give X" with no "to", or an "ask about" / "talk about", around the
   * last character a library command named (uip_rewrite_references()).
   * The order matters: run400's input routine dispatches typed-command
   * tasks at 48A481 (Proc_19_24_44CCE0), well before the give rewrite at
   * loc_48A98A and the Proc_19_0_480674 call at 48B56E that holds the
   * ask/talk rewrite, and a matched task jumps past both (GoTo 48B4E3).
   * So "Sommeril"'s literal task "ask about glass framed page" keeps
   * answering even with the Gargoyle as the last-named character;
   * rewriting first turned it into "ask gargoyle about ..." and lost the
   * task to the library's generic reply.  Its "(GARGOYLE)" is printed
   * all the same, up at the top of this routine -- only the rewritten
   * STRING is the library's; see uip_print_ask_echo().
   *
   * The Runner records the characters a line names inside Proc_19_0
   * (loc_47F3A2) as well, after both rewrites -- so a rewrite always
   * sees the register as the previous command left it.  That noting is
   * done up at the top of this routine now, because characters() runs
   * on every line; see uip_note_named_npcs().
   */
  scr_owned_string rewritten (uip_rewrite_references (game, string,
                                                     prior_npc, ask_echo));
  const scr_char *library_string =
      rewritten ? rewritten.get () : string;
  std::string sitstand_rest, openclose_rest;
  run_dispatch_input = library_string;
  /*
   * 4.0 enters its library handlers on the whole verb ANYWHERE in the
   * line, so a line whose verb is not at the head is answered with the
   * verb hoisted to the front; see run_hoist_verb_line().
   */
  /*
   * A goto line with one other verb: the verb's handler answers with
   * gotoplace switched off, and gotoplace has its turn after the block;
   * see run_goto_line_class().
   */
  const scr_int goto_class = run_goto_line_class (game, string);
  const size_t goto_mark = pf_buffer_length (gs_get_filter (game));
  std::vector<scr_int> goto_places;
  std::string goto_rest;
  if (goto_class != RUN_GOTO_NONE)
    {
      scr_int object;

      lib_go_place_off = TRUE;
      for (object = 0; object < gs_object_count (game); object++)
        {
          goto_places.push_back (gs_object_position (game, object));
          goto_places.push_back (gs_object_parent (game, object));
        }
      /* A leading goto word is no verb to the handlers above
         gotoplace: `goto cave take box` is the take's. */
      if (run_goto_strip_head (game, library_string, goto_rest))
        {
          library_string = goto_rest.c_str ();
          run_dispatch_input = library_string;
        }
    }
  std::string hoisted;
  if (run_hoist_verb_line (game, library_string, hoisted))
    {
      library_string = hoisted.c_str ();
      run_dispatch_input = library_string;
    }
  /*
   * Pre-4.0 the already-done refusal outranks the standard library; see
   * the note on run_task_refusal().  The room half still runs after it.
   */
  if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400)
    {
      /* With the repeat assist on, the already-done answer waits for
         the post-library pass, after movement and the library. */
      if (!run_repeat_assist)
        status = run_task_refusal (game, library_string,
                                   REFUSAL_PASS_PRE);
    }
  else if (repeat_pending)
    {
      /*
       * Matched on the line as typed, the way put_prepass()'s probe did and
       * the way run400's dispatcher does: the give and ask/talk rewrites
       * below loc_48A98A are further down the routine than 48A481.
       */
      status = run_task_refusal (game, string, REFUSAL_PASS_PRE);
    }
  /* sitstand enters on its words anywhere; see lib_sitstand_anywhere(). */
  if (!status)
    {
      status = lib_sitstand_anywhere (game, run_line_for_anywhere,
                                      &sitstand_rest);
      if (!status && !sitstand_rest.empty ())
        {
          /* The move is made; the rest of the line goes on without
             the sit words, the shape the ask/talk rows know. */
          library_string = sitstand_rest.c_str ();
          run_dispatch_input = library_string;
        }
    }
  /*
   * openclose is the next Call generaltasks makes, and it makes it on
   * every line; see lib_openclose_anywhere().  A handler below it that
   * will speak for the line gets it with the open/close word cut out.
   */
  if (!status)
    {
      status = lib_openclose_anywhere (game, string, run_line_for_anywhere,
                                       &openclose_rest);
      if (!status && !openclose_rest.empty ())
        {
          library_string = openclose_rest.c_str ();
          run_dispatch_input = library_string;
        }
    }
  /* whereis is below examines and above therest; see
     lib_whereis_anywhere(). */
  if (!status)
    status = lib_whereis_anywhere (game, run_line_for_anywhere);
  if (!status)
    status = run_score_anywhere (game, library_string);
  if (!status)
    status = run_therest_absent_370 (game, library_string);
  if (!status)
    status = run_wait_anywhere (game, library_string);
  if (!status)
    status = run_standard_verb_commands (game, library_string);
  /*
   * The out-of-room refusal sits INSIDE the library, one line above
   * run390's Call therest() -- so it outranks the generic catch-alls and
   * loses to every dedicated handler above it.
   */
  if (!status)
    status = run_task_refusal (game, library_string, REFUSAL_PASS_MID);
  if (!status)
    status = run_standard_give_npc_commands (game, library_string);
  /* run390's therest opens with its "with" arm. */
  if (!status)
    status = lib_with_arm_390 (game);
  if (!status)
    status = run_therest_pre400 (game, library_string);
  if (!status)
    status = run_standard_fallback_commands (game, library_string);
  if (!status)
    status = run_task_refusal (game, library_string, REFUSAL_PASS_POST);
  if (goto_class != RUN_GOTO_NONE)
    status = run_goto_after (game, string, goto_class, goto_mark,
                             goto_places, status);
}

/*
 * run_line_t::settle()
 *
 * The stores generaltasks makes after the library: the survivor's turn,
 * the 3.9 silent task's lost turn and topic reply, and the 4.0 namesake
 * and with-half questions.
 */
void
run_line_t::settle ()
{
  /*
   * A survivor answered a line the dispatcher had already claimed: the
   * message is the library's but the turn is the refusal's, so an answer
   * that is normally administrative counts here.  run400 prints the
   * RepeatText at 48A481 and jumps to loc_48B4E3, which is below the stores
   * that would mark the line administrative and above the character pass at
   * 48B56E -- so the NPC examine's own text comes out and the walk and event
   * tick at 48B599 still runs.
   */
  if (status && repeat_found && !repeat_pending)
    game->is_admin = FALSE;
  /*
   * The silent task's DontUnderstand and its lost turn are one store in
   * run390: generaltasks 46063E-46065A, `If msg = "" And var_350 = 0 Then
   * msg = DontUnderstand: GoTo 46067F`, and the jump lands past the calls to
   * characters() and events().  var_350 is the scan of every character's
   * Name and first Alias over the line (4605EB-460638,
   * lib_line_names_npc_390()), present or not.  So a silent task's line that
   * names a character keeps its empty buffer AND its turn, and characters()
   * runs over it -- whose ask block (4597FE) writes the topic reply, or the
   * no-response answer over the empty buffer (459B46).  Measured on A Day In
   * Toronto (toronto.taf, 3.90; run390x runner_transcripts/toronto.txt T9):
   * task 3 `ask waiter about burger` has no CompleteText and only moves the
   * burger into the room, and the Runner answers the line with the Waiter's
   * topic "ok", where the substitution alone gives the game's "huh?".
   *
   * What the Runner shows when the buffer is STILL empty after characters()
   * -- a silent task on a non-ask line naming a character, or the character
   * out of the room (459941) -- is not measured: it prints that empty buffer
   * (4606D0), and Scarier keeps the DontUnderstand text there, the turn
   * being ticked all the same.
   */
  silent_names_npc_390 = silent_task_390
                         && lib_line_names_npc_390 (game, string);
  if (silent_task_390 && !silent_names_npc_390
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    game->is_admin = TRUE;

  /*
   * 3.9: a character's topic reply replaces what a task printed for an ask or
   * talk-to-about line; see lib_ask_npc_topic_after_task_390().  4.0 leaves
   * the task's text alone.
   */
  if ((task_claimed || silent_names_npc_390)
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_390
      && run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
      && (uip_match ("ask %character% about %text%", string, game)
          || uip_match ("talk to %character% about %text%", string, game)))
    {
      if (lib_ask_npc_topic_after_task_390 (game, task_mark,
                                            silent_names_npc_390)
          && silent_names_npc_390)
        status = TRUE;
    }

  /*
   * 4.0: a line a task answered that also names a term two present
   * characters share is not a turn.  run400 generaltasks ticks only when
   * `MemVar_4941EC = &HFF` (48B5B5: turns, walks and events), and the
   * namesake scan sets that index before the tick; the block at 48B60C then
   * sees "a task ran for this line" (MemVar_4941F8 = 1), prints the task's
   * text instead of asking "Which ...", and resets the index at 48BB92 -- so
   * no question and no tick.  Measured on Sun Empire (run400x
   * sun_empire_site.txt, VBRNG_SEED=10, commands 58 and 63, site-tagged
   * draws): `get sample from orgaan soldier` with the two soldiers Skyrv and
   * Skynd present runs task 63/64, and neither the Code Red siren nor the
   * battle draws a thing that turn.  characters()' give-branch co() scan
   * over the objects does the same for an object name that two present
   * objects share (lib_co_400_line_leaves_which_pending(); Cyberclones II
   * `give electric uniform to lightning`).
   */
  if (status && !game->is_admin && run_any_task_ran_this_command ()
      && (lib_npc_400_line_names_namesakes (game, string)
          || lib_co_400_line_leaves_which_pending (game, string)))
    game->is_admin = TRUE;

  /*
   * 4.0: a handler's own "Which" prompt on a line holding "with" is undone
   * by openclose, which runs below the handlers and rewrites the pending
   * index from the text after "with" (475C63-475D6C).  Where it leaves the
   * index at -1 the prompt stays printed but the question is never
   * registered and the line is a turn: `take stone with knife` ticks,
   * `take stone` does not.  See lib_openclose_with_half_400().
   */
  if (status && lib_co_400_named_question_raised ()
      && !run_any_task_ran_this_command ()
      && lib_openclose_with_half_400 (game, string))
    {
      lib_co_400_drop_question ();
      game->is_admin = FALSE;
    }

  /*
   * 4.0: and where openclose's loop leaves the index at an object, with no
   * handler question and no task, generaltasks asks it instead of whatever
   * the line printed: `x rope with stone`, `wear flint with stone`.  A line
   * claimed above openclose never gets there.  See
   * lib_openclose_with_half_raise_400().
   */
  /*
   * openclose's loop runs on a line nobody answers, too: therest's
   * DontUnderstand comes after it (`cut rope with gems` leaves "emerald").
   */
  if (reached_library && game->is_running
      && !run_any_task_ran_this_command ())
    lib_openclose_with_antecedent_400 (game, string);
  if (status && reached_library && game->is_running
      && !run_any_task_ran_this_command ()
      && lib_openclose_with_half_raise_400 (game, string))
    game->is_admin = TRUE;

  /*
   * 4.0: and when no task ran, the same scan ASKS.  The question belongs to
   * generaltasks (48B6AE-48BB92), not to any verb: it comes after everything
   * the line printed and replaces it, whatever answered.  Measured
   * 2026-09-20 on p4BATT (Ann and Bob both "a guard", Dave and a third guard
   * next door; run400x runner_probes/batt.run400.txt and
   * runner_probes/batt.run400.b.txt) -- `x guard`, `attack guard`, `attack
   * guard dave`, `status guard`, `talk to guard`, `where is guard`, `give
   * stone to guard`, `give club to guard` and the bare noun `guard` all
   * answer "Which guard.  A guard or a guard?", and the last two lose a real
   * answer ("You don't have the stone!") to it.  `probe`, a task, keeps its
   * own text (the 48B60C gate above).
   *
   * Both halves of the question read ONE untyped index, MemVar_4941EC, and
   * the object half (48B6B1) gets first refusal: it runs when that index is
   * also a valid object index AND the line names that object, printing the
   * OBJECT's name over the character list.  In p4BATT the flagged character
   * is Bob, index 2, and object 2 is the stone, so `attack guard with stone`
   * and `give stone to guard` are "Which stone.  A guard or a guard?" while
   * `x guard sword` (object 0) and `attack guard with club` (object 1) are
   * "Which guard." -- a plain index collision, not a term choice.  Left
   * unmodelled; see the lead in notes/WINE-TRANSCRIPTS-TODO.md.
   */
  if (run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
      && game->is_running
      && !run_any_task_ran_this_command ()
      && lib_npc_400_line_names_namesakes (game, string))
    {
      pf_empty (filter);
      lib_npc_400_raise_for_line_string (game, string);
      status = TRUE;
    }
}

/*
 * run_line_t::finish()
 *
 * Clears the per-line stores and returns RESULT.
 */
scr_bool
run_line_t::finish (scr_bool result)
{
  run_dispatch_input = NULL;
  run_tasks_ran_this_command.clear ();
  return result;
}

/*
 * run_line_t::run()
 *
 * The passes in generaltasks' order.  The two guards live here so that
 * they hold for the whole line, and the design note in
 * run_all_commands() says why the passes stand where they do.
 */
scr_bool
run_line_t::run ()
{
  reset ();
  const scr_lenient_tasks_guard lenient_tasks (lenient ());
  if (spent_claim_390 ())
    return finish (TRUE);

  put_prepass ();
  const run_put_class_guard put_class
      (!put_first && !repeat_pending
       && lib_put_held_unsplit_400 (game, put_line));
  put_pass ();
  get_outer_400 ();
  task_peek ();
  priority_pass ();
  task_passes ();
  battle_pass ();
  library ();
  settle ();
  return finish (status);
}

/*
 * run_all_commands()
 * run_game_task_commands()
 *
 * Alternative facets of run_game_commands_common().  The first is used by the
 * main user input handling loop; the latter by the library when looking for
 * game commands that override standard actions.
 */
static scr_bool
run_all_commands (scr_gameref_t game, const scr_char *string)
{
  /*
   * Adrift command matching is just weird, perhaps broken.  In theory, a
   * game can override system commands with a properly constructed task and
   * set of command matchers.  However, the Runner isn't terribly consistent
   * in when this will work and when not, and some games rely on that in-
   * consistency.  In particular, a game with a "* object" task that has
   * failing restrictions will not be able to override the system's "take
   * object", whereas a game's "take object", under the same circumstances,
   * will.  Yet if the restrictions pass, a game's "* object" overrides the
   * system's "take object" with no apparent difficulty.
   *
   * For example, "The Woods Are Dark" has a "* ball *" task with the
   * restriction "must be holding ball".  Without special casing it, there's
   * no way to get the ball in the first place.
   *
   * Trying to find the right way to do things here, then, has been tricky.
   * Here's the current process:  First, try "priority" system commands; ones
   * that move objects to inventory.  These system commands will call back
   * into trying game commands for objects taken or dropped, and in those
   * tries, allow overrides only if the game task is explicit about what it's
   * doing -- it doesn't start with "*", or it does but explicitly names a
   * verb (see run_match_task_commands() for the run400 probe results behind
   * that rule) -- and handle restrictions in those tries.  Next, run game
   * commands directly, ignoring any cases where restrictions fail to let the
   * task run.  After that, retry all game commands again with restrictions
   * enabled.  And finally, try all other standard library commands.
   *
   * Priority commands go BEFORE the direct game-command pass (not after, as
   * an earlier version of this had it): probe DONE, task 72 in "Space Boy's
   * First Adventure" is the literal, unrestricted, textless command "drop
   * cape to the floor" (+250 score, no message).  run400 answers the typed
   * command with the library's ordinary "Player drop the cape." and the
   * score UNCHANGED (runner_probes/et2.run400.txt,
   * runner_probes/et4.run400.txt, 2026-08-23) -- the task never runs at all,
   * even though its own literal pattern matches the raw input exactly.  A
   * same-shaped task using a bare object with no trailing words ("* drop *
   * rock *", i.e. equivalent to the library's own canonical "drop <object>"
   * callback string) DOES run silently alongside the library's message
   * (runner_probes/et.run400.txt).  So it is specifically the priority
   * command's own callback -- matching only its constructed "verb OBJECT"
   * short form, not the raw typed line -- that gets first refusal on a
   * recognised system verb; a task whose pattern extends past that short form
   * (trailing words the library only swallows as free %text%) is never reached
   * once the library has already claimed and finished the command.  Swapping
   * the two passes reproduces that: on a recognised system verb,
   * run_priority_commands() (and its short-form task callback) gets the first
   * and often the only look, and the direct, ignore-restrictions task pass
   * only sees what the priority commands didn't recognise as a system verb at
   * all (so "etcontrol"-style non-system task commands are unaffected).
   *
   * But that swap alone regresses the same game's task 27, "{take/get}
   * {them/boots}" (CompleteText "Taken."): the callback's constructed short
   * form is built from the object's display Short name ("get pair of Flight
   * Boots"), never from an alias like "boots", so the callback attempt fails
   * for a reason unrelated to task 72's "trailing words" problem, and
   * priority's own generic take now wins a task the golden walkthrough (and,
   * definitionally, the pre-swap Runner-matching behaviour this port has
   * always had) says must win instead.  (The bare swap is not a near miss:
   * it fails 75 of the 262 corpus walkthroughs, because scarier's callback
   * string is weaker than the Runner's -- Short name only, no aliases -- so
   * far more tasks are unreachable here than there.)  So a peek at the direct
   * task pass runs BEFORE priority after all, and task 72's shape is excluded
   * from it: silent (no CompleteText, ShowRoomDesc, AdditionalMessage or
   * status-setting action) AND unreachable from any library short form for
   * any object ("drop cape" / "drop the cape" can never satisfy a pattern
   * that insists on "to the floor").  Both halves are needed -- silence alone
   * loses "Sommeril" task 35 and 3 more walkthroughs, unreachability alone
   * loses 27 of them.
   *
   * Excluding a task means abandoning the ENTIRE peek, not reading past it to
   * the next task.  ADRIFT resolves a command to the lowest-indexed matching
   * task, so stepping over a match to let a later task win invents a Runner
   * behaviour that does not exist: "The Forum" task 1 (silent literal "x ...
   * hand", which falls through to the library's own examine) was being
   * skipped in favour of task 2, "[examine/read/x/l/look]{at}[%object%]",
   * whose CompleteText then answered every examine in the game.  That cost
   * 8 corpus walkthroughs (forum, forum2, cursed, iqsfot, sommeril, funhouse
   * and both to_hell_and_beyond replays) until the peek was made all-or-
   * nothing.  On abandonment priority gets its look next, and if it does not
   * claim the command the immediately following unexcluded pass re-matches
   * from task 0, unchanged from the original order.
   */
  /*
   * The carrying-capacity accounting toggle is exposed as a Glk port command
   * ("glk capacity"), handled in the front end before input ever reaches the
   * interpreter, so there is no administrative meta-command to match here.
   */
  run_line_t line (game, string);

  return line.run ();
}

scr_bool
run_game_task_commands (scr_gameref_t game, const scr_char *string)
{
  /*
   * Try game commands, and note that this is a library call so that the parse
   * matcher can exclude game commands that begin with a '*' wildcard.
   *
   * Restrictions are honoured -- meaning a task whose restrictions fail with a
   * message gets run for the message, beating the library action -- from 4.0
   * on only.  Probe DONE, task `* get * gem *` restricted to "holding the
   * stone", measured 2026-08-23: run400 answers `get gem` without the stone
   * with "BLOCK-GEM.", while run390 on the 3.9 twin probe quietly takes the
   * gem instead.  Pre-4.0 the library wins, so the loud pass is switched off
   * here rather than being allowed to claim the command.  Tasks whose
   * restrictions PASS still run in either version -- the first loop below does
   * not consult this flag.
   */
  const scr_bool include_restrictions =
      run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400;

#ifdef SCARIER_DUMP_TOOLS
  if (getenv ("SCR_TRACE_MATCH"))
    fprintf (stderr, "DISPATCH input=[%s]\n", string);
#endif

  /*
   * This is the dispatcher 44CCE0 as the library handlers call it, and its
   * task_pick refreshes the restriction cache first: 3monkeys' take piece
   * dispatches "get the coconut husk" after its pre-match hit on task 616's
   * stale results, and that dispatch's own fallback, reading the fresh
   * ones, finds nothing to say (run400x site trace, T41).
   */
  if (include_restrictions)
    run_restriction_cache_task_pick (game, string);

  return run_game_commands_common (game, string, include_restrictions, TRUE,
                                   FALSE);
}

/*
 * run_passing_task_commands()
 *
 * run_game_task_commands() without the restriction-failure pass: only a task
 * whose restrictions pass can claim the line, and a FailMessage never does.
 * For lib_try_typed_put_line_400(), whose look-up the Runner never makes --
 * there a failing task must not take the line away from the library put the
 * game may be waiting on (deadman's `put hand on plate`, "You'll need an
 * additional thumbprint.").
 */
scr_bool
run_passing_task_commands (scr_gameref_t game, const scr_char *string)
{
  return run_game_commands_common (game, string, FALSE, TRUE, FALSE);
}

/*
 * run_typed_line_task_commands()
 *
 * Offer a line to the tasks the way run390 does from inside a library
 * handler on the player's own words: a bare `*` matches, as it would from
 * the dispatcher.  Quiet (`loud` FALSE) is the handler's own tasks(1), which
 * is checktask(line, 0) (42BD5E): passrest() puts the entry buffer back when
 * running is 0 (452BE6), so a matched task whose restrictions fail says
 * nothing.  Loud (`loud` TRUE) is generaltasks' tasks(0) (45F48B), reached
 * when the handler's result stayed 0: checktask(line, 1) lets the failing
 * restriction's FailMessage overwrite the buffer (452BBD) and the put text
 * with it.  Still one task per line.  See lib_put_task_sweep_390().
 */
scr_bool
run_typed_line_task_commands (scr_gameref_t game, const scr_char *string,
                              scr_bool loud)
{
  return run_game_commands_common (game, string, loud, FALSE, FALSE);
}


/*
 * run_comma_splits_pre390()
 *
 * Deliberate deviation: 3.7/3.8 split a line at a comma, as SCARE did and
 * 3.9 does, where run370/run380 never do (run_find_split_pre400()).  The
 * Runner rule stands wherever a task command matches the whole line, so a
 * game that wrote a comma into a command keeps it: arlo's `kill, kill, kill`
 * and `hello, customer`, tra's `mirror mirror on the wall, who's the fairest
 * of them all`, wrecked's `out (Redstown, no ticket)`, and any `say *` that
 * takes a comma in what is said.  The test is the plain matcher on the
 * unfiltered line, ahead of the synonym and pronoun rewrites.
 */
static scr_bool
run_comma_splits_pre390 (scr_gameref_t game, const scr_char *line)
{
  const scr_task_commands_guard task_commands;
  const scr_int task_count = gs_task_count (game);
  scr_int task;

  if (!strchr (line, ','))
    return FALSE;

  for (task = 0; task < task_count; task++)
    {
      for (const scr_bool forwards : {scr_bool (TRUE), scr_bool (FALSE)})
        {
          for (const scr_char *pattern :
               run_task_command_patterns (game, task, forwards))
            {
              if (pattern[strspn (pattern, WHITESPACE)] == SPECIAL_PATTERN)
                continue;
              if (scr_strcasecmp (pattern, line) == 0
                  || uip_match (pattern, line, game))
                return FALSE;
            }
        }
    }
  return TRUE;
}


/*
 * The typed line still to run, and the element of it being run now.
 * run_player_input() owns both; run_input_reset() clears them.
 */
static scr_char run_line_buffer[LINE_BUFFER_SIZE];
static scr_char run_line_element[LINE_BUFFER_SIZE];

/*
 * run_is_repeat_word()
 *
 * The words that repeat the last element; see run_player_input().
 */
static scr_bool
run_is_repeat_word (scr_gameref_t game, const scr_char *element)
{
  std::string word (element);
  const size_t first = word.find_first_not_of (' ');

  if (first == std::string::npos)
    return FALSE;
  word = word.substr (first, word.find_last_not_of (' ') - first + 1);

  if (word == "again" || word == "last" || word == "previous")
    return TRUE;
#ifndef SCARIER_NO_ABBREVIATIONS
  if (word == "g")
    return run_get_version (gs_get_bundle (game)) >= TAF_VERSION_390;
#endif
  return FALSE;
}

/*
 * run_input_reset()
 *
 * Resets all noted line input to initial conditions, for a game that is
 * not running.
 */
static void
run_input_reset (void)
{
  memset (run_line_buffer, NUL, sizeof (run_line_buffer));
  memset (run_prior_element, NUL, sizeof (run_prior_element));
  memset (run_line_element, NUL, sizeof (run_line_element));
  run_typed_line.clear ();
  run_previous_typed_line.clear ();
  lib_co_400_reset ();
  lib_battle_who_reset ();
  lib_with_prefix_390_reset ();
  lib_put_reset ();
  run_cancel_goto_walk ();
}

/*
 * run_repeat_element()
 *
 * `again`: makes the last element the current one.  Returns FALSE, with
 * the complaint printed, when there is nothing to repeat.
 */
static scr_bool
run_repeat_element (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  game->do_again = FALSE;

  /* Check there is a last element to repeat. */
  if (run_prior_element[0] == NUL)
    {
      pf_buffer_string (gs_get_filter (game),
                        "You can hardly repeat that.\n");
      return FALSE;
    }

  /*
   * 4.0 with "References in brackets" ticked (the setting Scarier
   * models) echoes the command it is about to repeat, in round brackets
   * on its own line: run400 generaltasks loc_48A058 walks the history
   * past the "again"s, then at loc_48A095 tests MemVar_4942BA and prints
   * "(" & command & ")" & vbCrLf through Proc_21_19_47B568.  run390
   * echoes it the same way: p39WITH `probe`, `again` -> "(probe)" then
   * "PROBE OK." (runner_probes/with.run390.txt, 2026-09-14).  3.8 and
   * earlier are not measured.
   */
  if (prop_get_taf_version (bundle) >= TAF_VERSION_390)
    pf_buffer_reference (gs_get_filter (game), run_prior_element,
                         gs_get_vars (game), bundle);

  /* Make the last element the current input element. */
  strncpy (run_line_element, run_prior_element, LINE_BUFFER_SIZE);
  return TRUE;
}

/*
 * run_read_line()
 *
 * Reads a new line of player input if none is buffered -- or the rest of
 * the line a pre-4.0 walk held back -- and otherwise separates output so
 * far with a newline.  Returns TRUE when a line was read.
 */
static scr_bool
run_read_line (scr_gameref_t game)
{
  scr_bool is_new_line = FALSE;

  /*
   * A pre-4.0 walk is over: the rest of the line it held back runs now,
   * in the same turn as the arrival.  See run_goto_rest.
   */
  if (run_line_buffer[0] == NUL && !run_goto_rest.empty ()
      && run_goto_steps.empty () && !run_goto_arrival_due)
    {
      strncpy (run_line_buffer, run_goto_rest.c_str (), LINE_BUFFER_SIZE - 1);
      run_line_buffer[LINE_BUFFER_SIZE - 1] = NUL;
      run_goto_rest.clear ();
    }

  if (run_line_buffer[0] == NUL)
    {
      if_read_line (run_line_buffer, sizeof (run_line_buffer));
      is_new_line = TRUE;

      /* run400 48A2EA: a new typed line, no event ticked yet. */
      evt_clear_ticked_events (game);

      /*
       * Every Runner lower-cases the whole typed line before it parses
       * anything: run400 Form1 loc_45C5D1..45C5E5 echoes `"> " & cmd`
       * first and only then assigns `cmd = LCase(cmd)`, pushing the
       * lower-cased copy into the command history array as well
       * (run390 loc_436235..436249, run380 loc_426FA9, run370
       * loc_422091 -- unconditional in all four, no version gate).
       * The echo is unaffected because it happens above the LCase, and
       * Glk echoes the input line for us here.
       *
       * This matters because the game's own SYNONYM rewrites run AFTER
       * it, so an author's replacement text is the only thing that can
       * put an upper-case letter back into a command -- which is what
       * makes a character unreferenceable in uip_case_folds_name().
       */
      for (scr_char *cursor = run_line_buffer; *cursor != NUL; cursor++)
        *cursor = scr_tolower (*cursor);
      run_previous_typed_line = run_typed_line;
      run_typed_line = run_line_buffer;
    }
  else
    if_print_character ('\n');
  return is_new_line;
}

/*
 * run_cut_element()
 *
 * Cuts the next element off the front of the line buffer, and makes it
 * the current input element.
 */
static void
run_cut_element (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int length, extent;

  /*
   * Find the length of the next input line element.  At 4.0, unless the
   * line buffer is empty, we always take the first character, even if
   * it's a separator.  This catches odd input like "." and turns it into
   * a parser complaint, rather than treating it as two empty commands
   * with a separator between them.
   */
  scr_int sep_length = 1;

  if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
    {
      /*
       * 4.0 cuts the line at the first separator, in pass order, whose
       * tail does not begin with an object name; see
       * run_find_split_400().  The separator goes, and any whitespace
       * after it: that prevents "i. ." looking like "i" and ""; it
       * instead looks like "i" and ".", and results in a parser
       * complaint.
       */
      const scr_int split = (run_line_buffer[0] == NUL)
                            ? -1
                            : run_find_split_400 (game, run_line_buffer,
                                                  &sep_length);

      length = (split < 0) ? (scr_int) strlen (run_line_buffer) : split;
      extent = length;
      extent += (run_line_buffer[length] == NUL) ? 0 : sep_length;
      extent += strspn (run_line_buffer + extent, WHITESPACE);
    }
  else
    {
      /*
       * Pre-4.0 keeps its tail as the Runner does, less one leading
       * space, and the head may be empty (`then look`); see
       * run_find_split_pre400().
       */
      const scr_int version = prop_get_taf_version (bundle);

      length = run_find_split_pre400 (version, run_line_buffer, &extent,
                                      version < TAF_VERSION_390
                                      && run_comma_splits_pre390
                                           (game, run_line_buffer));
      if (length < 0)
        length = extent = (scr_int) strlen (run_line_buffer);
      else if (length == 0 && version == TAF_VERSION_390
               && strncmp (run_line_buffer, "then", 4) == 0)
        {
          const std::string queue = run_empty_then_head_390 (run_line_buffer);

          strncpy (run_line_buffer, queue.c_str (), LINE_BUFFER_SIZE - 1);
          run_line_buffer[LINE_BUFFER_SIZE - 1] = NUL;
          length = extent = (scr_int) strlen (run_line_buffer);
        }
    }

  /*
   * Make this the current input element, and remove it and the
   * separator from the front of the line buffer.
   */
  memcpy (run_line_element, run_line_buffer, length);
  run_line_element[length] = NUL;
  memmove (run_line_buffer,
           run_line_buffer + extent, strlen (run_line_buffer) - extent + 1);

  /*
   * The Runner strips one trailing space from the head it keeps (the
   * splitter's loop at 4596E1), so `x coin , x hat` leaves "x coin",
   * not "x coin ".
   */
  if (length > 0 && run_line_element[length - 1] == ' ')
    run_line_element[length - 1] = NUL;
}

/*
 * run_count_element()
 *
 * 3.9 counts line elements, not turns: generaltasks adds one to its turn
 * counter MemVar_4681A4 at its very top (45EC5B), as run380 does to
 * MemVar_44F138 (441A21), before the not-a-turn flag is even cleared, so
 * `turns`, a DontUnderstand line and a blank line all count.  Every jump
 * back for the next queued element (4609F9) lands above the increment
 * too.  `again` does not: run390 answers the `turns` that follows 18
 * elements with 19 twice over (runner_probes/admin.run390.txt).  The
 * end-of-turn tail in run_main_loop() leaves the counter alone at 3.9.
 * The increment sits after the line is read, so an autosave taken at the
 * prompt and restored there never counts a line twice.
 */
static void
run_count_element (scr_gameref_t game)
{
  if (run_counts_line_elements (game))
    {
      game->turns++;

      /*
       * `both` is the one in-line re-run that really counts twice: 45FEB6
       * swaps the line for the menu stash MemVar_46813C.global_0 and jumps
       * back to 45EC4B, above the increment.  With no menu pending the stash
       * is empty and the re-run is DontUnderstand, which Scarier already
       * prints for the word; p39WITH `turns`, `both`, `turns` answers 24
       * then 27 (runner_probes/with.run390.txt).  The question prefix does
       * not: the jump at 4601A5 never fires for a "With what?" or "Wear
       * what?" answer (`knife`, `coin` each count once in the same drive).
       */
      if (scr_strcasecmp (run_line_element, "both") == 0
          && prop_get_taf_version (gs_get_bundle (game)) == TAF_VERSION_390)
        game->turns++;
    }
}

/*
 * run_element_filtered()
 *
 * The element with the game's synonyms and the built-in rewrites applied,
 * or NULL if nothing changed.  Deliberate deviations (2026-09-30), each for
 * the lines where the Runner's own rewrite leaves the author's task out of
 * reach:
 *
 *  - The game's synonyms are applied the Runner's way (pf_apply_synonym())
 *    and Scarier's older way (pf_apply_synonyms_by_word()).  The Runner's
 *    loops replace substrings inside words, skip a synonym whose first hit
 *    sits inside another word or whose Original has a capital, and let a
 *    later synonym rewrite an earlier one's text, so Vardock Bates' `hablar
 *    con jason` became "talk con jason jason dhirco" and no task took it.
 *  - run380 rewrites a typed "take" to "get" before any task sees the line,
 *    so a 3.80 task whose commands say only `take X` can never fire:
 *    great.taf's `take picasso` just picks the painting up.
 *
 * The Runner's spelling runs whenever it reaches a task, so Dolg's `войти в
 * дом` and a `take X` that reaches a `get X` task are unchanged.  Otherwise
 * the first other spelling that reaches a task runs, and a line no spelling
 * takes gets the whole-word synonyms and the Runner's take->get.
 */
static scr_char *
run_element_filtered (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = prop_get_taf_version (bundle);
  scr_owned_string runner (pf_filter_input (run_line_element, bundle));
  std::vector<scr_owned_string> others;
  size_t index_;

  if (version < TAF_VERSION_380)
    return runner.release ();

  others.emplace_back (pf_filter_input (run_line_element, bundle,
                                        TRUE, FALSE));
  if (version == TAF_VERSION_380)
    {
      others.emplace_back (pf_filter_input (run_line_element, bundle,
                                            FALSE, TRUE));
      others.emplace_back (pf_filter_input (run_line_element, bundle,
                                            FALSE, FALSE));
    }

  /* The spelling the matcher sees: trimmed, whitespace runs collapsed. */
  auto spelling = [] (const scr_owned_string &text) -> std::string
    {
      std::vector<scr_char> copy;
      const scr_char *source = text ? text.get () : run_line_element;

      copy.assign (source, source + strlen (source) + 1);
      return scr_normalize_string (copy.data ());
    };
  const std::string runner_spelling = spelling (runner);

  for (index_ = 0; index_ < others.size (); index_++)
    {
      if (spelling (others[index_]) != runner_spelling)
        break;
    }
  if (index_ == others.size ())
    return runner.release ();

  if (run_line_matches_task_strictly (game, runner_spelling.c_str ()))
    return runner.release ();
  for (index_ = 0; index_ < others.size (); index_++)
    {
      const std::string other = spelling (others[index_]);

      if (other != runner_spelling
          && run_line_matches_task_strictly (game, other.c_str ()))
        return others[index_].release ();
    }

  /* No spelling reaches a task: the whole-word synonyms, with take->get. */
  return others[0].release ();
}

/*
 * run_element_command()
 *
 * The command the element runs as: the element filtered for synonyms and
 * pronouns, and glued onto the previous line when it begins "with ".
 */
static void
run_element_command (scr_gameref_t game, std::string &command)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * Filter the input element for synonyms, then for pronouns.  Both are
   * scr_malloc'd, so own them with RAII; .get() feeds the raw char* to the
   * pointer-aliasing logic that decides which buffer "wins", and COMMAND
   * takes a copy of the winner.
   */
  scr_owned_string filtered (run_element_filtered (game));
  scr_owned_string replaced (uip_replace_pronouns (game,
      filtered ? filtered.get () : run_line_element));

  /*
   * If filtering didn't replace synonyms, and no pronouns were replaced, use
   * the original line element.  The "(to Nobody)" / "(GARGOYLE)" reference
   * rewrites are NOT applied here: the Runner applies them only once its
   * typed-command task dispatcher has declined the line -- see
   * run_all_commands().
   */
  command = replaced ? scr_normalize_string (replaced.get ())
            : (filtered ? scr_normalize_string (filtered.get ())
                        : run_line_element);

  /*
   * 3.8 on, an element beginning "with " is glued onto the line typed before
   * it: `If Left(line, 5) = "with " Then line = history(2) & " " & line`,
   * after the synonyms, pronouns and the everything/slap/take/except
   * rewrites (run380 441C9D, run390 45F2AF, run400 48A399; run370 has none).
   * So after `look`, `with stone` runs as "look with stone" -- the stone's
   * description at 3.9 (run390x runner_probes/npcamb.run390.with.txt) and
   * "Nothing special." at 3.8 (run380x
   * runner_probes/npcamb.run380.with.rtf).
   */
  if (prop_get_taf_version (bundle) >= TAF_VERSION_380
      && strncmp (command.c_str (), "with ", 5) == 0)
    command = run_previous_typed_line + " " + command;
}

/*
 * run_element_begin()
 *
 * The per-element stores made before the command is run: the open
 * questions are noted, and the referenced object and character forgotten.
 */
static void
run_element_begin (scr_gameref_t game, const scr_char *command,
                   scr_bool is_new_line)
{
  /*
   * Upstream SCARE echoed the rewritten command in italic square brackets,
   * for synonyms and for pronouns alike.  No Runner does that: run370 and
   * run380 have no "[" string literal at all, run390's only one is the
   * "[More]" pager, and 4.0's synonym substitution is silent too.  The echo
   * also exposed authoring the player is not meant to see -- "The Warlord,
   * The Princess & The Bulldog" routes "i"/"inv"/"inventory" through a
   * synonym to the keyword its inventory task listens for, so every "i"
   * answered with a bare "[iii]".
   *
   * What 4.0 does print, with "References in brackets" ticked, is the
   * pronoun's antecedent in round brackets on its own line -- "(a trophy)";
   * uip_replace_pronouns() buffers that as it substitutes.
   */

  /*
   * Note whether a 4.0 ambiguity prompt left a question open.  The question
   * a typed LINE began with is spent by its first element; one raised by an
   * earlier element of the same line is not, and the element that meets it
   * answers "That wasn't one of the options!".  See lib_co_400_raise() in
   * sclibrar.cpp.
   */
  lib_co_400_begin_line (is_new_line);
  lib_antecedent_begin_line_400 (game, command);
  lib_battle_who_begin_element (is_new_line);
  lib_with_prefix_390_begin_element ();

  /*
   * Every Runner forgets the referenced object and character at the top of
   * every command: run400 generaltasks stores &HFF in MemVar_494208 and
   * MemVar_49420A at 48A004/48A009, run390 in MemVar_4681A8/4681AA at
   * 45EC66/45EC6B.  Only an %object% / %character% bind or a library handler
   * sets them again, so a state restriction on "the referenced object" in a
   * task whose command names no %object% fails outright (run400 480F9E,
   * run390 44ABF0) -- it never sees whatever the previous command referenced.
   * Professor's task 7 (`take mailbox`, square) is the case: run400 never
   * runs it (runner_probes/professor.run400.profmail.txt,
   * runner_probes/professor.run400.mail2.txt,
   * runner_probes/professor.run400.profmail3.txt), Scarier used to pass it on
   * the mailbox left over from `examine mailbox`.
   *
   * 3.7 and 3.8 forget too, which p*OBJREF's task 2 shows from the printing
   * side: `zork`, whose command binds nothing, prints its text's %object%
   * and %character% raw on the turn right after `nurb rock` bound the red
   * rock, in all four Runners (runner_probes/objref.run370.rtf,
   * runner_probes/objref.run380.rtf, runner_probes/objref.run390.txt,
   * runner_probes/objref.run400.txt, 2026-09-20).  Below 3.90 the reference
   * looks to be local to checktask, which is the same thing seen from the
   * other end.  We kept the older Runners' references across the line and
   * printed "ZORKED a red rock."
   */
  var_set_ref_object (gs_get_vars (game), -1);
  var_set_ref_character (gs_get_vars (game), -1);
}

/*
 * run_element_questions()
 *
 * An open "Who do you want to attack?" continues a line nothing answered:
 * the battle prefix goes in front and the line runs again, its own output
 * discarded.  See lib_battle_who_continuation() in sclibrar.cpp.  The
 * "With what?" question rule and 3.9's own with-prefix continuation
 * follow.  Returns the element's final status.
 */
static scr_bool
run_element_questions (scr_gameref_t game, const scr_char *command,
                       scr_bool status)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const std::string rerun (lib_battle_who_continuation (command, status));

  if (!rerun.empty ())
    {
      const scr_bool is_400 = prop_get_taf_version (bundle)
                              >= TAF_VERSION_400;
      std::string collapsed (rerun);
      size_t pair;

      /*
       * 4.0 runs the joined line with its spaces collapsed and past the
       * task matcher; 3.9 runs it as joined, and its task matcher sees
       * every space: p39WITHQ's `saw rope` / `knife` fires the task wired
       * `saw rope with  knife` and not its one-space twin (run390x
       * runner_probes/withq.run390.txt, 2026-09-25).
       */
      if (is_400)
        while ((pair = collapsed.find ("  ")) != std::string::npos)
          collapsed.erase (pair, 1);

      pf_empty (gs_get_filter (game));
      game->is_admin = FALSE;
      run_rerun_skips_tasks = is_400 && collapsed != rerun;
      run_rerun_exact_spaces = !is_400
          && rerun.find ("  ") != std::string::npos;
      status = run_all_commands (game, collapsed.c_str ());
      run_rerun_skips_tasks = FALSE;
      run_rerun_exact_spaces = FALSE;

      /*
       * 3.9's prefix rerun (4601A5-4601C4) is a GoTo 45EC4B, above the
       * element counter at 45EC5B, so the joined line counts as one more
       * element: on p39WITHQ every continued `knife` and `sword` moves
       * `turns` by two, the `knife` after "Whittle it with what?" -- no
       * prefix, the object catch-all -- by one.  See run_count_element().
       */
      if (prop_get_taf_version (bundle) == TAF_VERSION_390)
        game->turns++;
    }

  /*
   * A turn that ends asking "With what?" or "...with?" leaves the line
   * plus " with " as the question prefix (3.9 and 4.0), and at 4.0 is not
   * a turn -- task text included.  See lib_question_with_rule() in
   * sclibrar.cpp.
   */
  if (status
      && lib_question_with_rule (game, rerun.empty () ? command
                                                      : rerun.c_str ()))
    game->is_admin = TRUE;

  /*
   * 3.9: an open "With what?" continues a line nothing understood, as
   * `<prefix><line>` and through therest only -- the task matcher never
   * sees the joined line.  See lib_with_prefix_390_continuation() in
   * sclibrar.cpp.
   */
  if (rerun.empty ())
    {
      const std::string joined (lib_with_prefix_390_continuation (command,
                                                                  status));

      if (!joined.empty ())
        {
          pf_empty (gs_get_filter (game));
          run_rerun_skips_tasks = TRUE;
          status = run_all_commands (game, joined.c_str ());
          run_rerun_skips_tasks = FALSE;
        }
    }
  lib_with_prefix_390_end_element ();
  return status;
}

/*
 * run_element_co_answer_400()
 *
 * 4.0: with an ambiguity question open, a line that did nothing is an
 * answer to it rather than a line the game misunderstood.  "Did nothing"
 * is either of the two refusals that end a turn empty-handed -- the
 * DontUnderstand path below, and the unhandled-verb catch-all in
 * lib_cmd_verb_object() -- and the turn's own output goes with it, the way
 * the 3.8 prompt replaces a turn wholesale (pf_empty() in
 * lib_co_ambiguity_prompt()).  A line that DID something runs normally and
 * simply spends the question: run400 answers `x tree rock` / `x rock` with
 * "A plain thing." and not with the refusal
 * (runner_probes/co.run400.t930.txt).
 *
 * Answering does not score the answer against the prompt's candidates at
 * all: generaltasks splices the typed words into the stored command in
 * front of its term and re-runs the whole line (48B097-48B15B for an
 * object, 48B15E-48B197 when the command does not hold the term), which
 * is how `chop tree` / `chop keys` ends in a SECOND full prompt naming
 * the keys as well as the trees -- the rebuilt `chop chop keys tree`
 * names them.  "That is still ambiguous!" is then the re-run's own raise
 * meeting the list the last prompt left behind, not an answer this slot
 * gives.  Measured on p4CO.taf, runner_probes/co.run400.t927.txt,
 * runner_probes/co.run400.t929.txt and runner_probes/co.run400.co11.txt
 * -- see lib_co_400_object_answer_line().
 *
 * 48B15B jumps to 489FEB, the top of the ELEMENT loop, which clears the
 * element's reply and the question but NOT what the last prompt offered
 * (48BB53's 4941F4, cleared only at the end of an element that flagged
 * nothing).  lib_co_400_take_question() is that top: it drops the
 * question the re-run must not answer a second time, and leaves that
 * list standing so that the re-run's own raise can meet it.
 *
 * Returns TRUE when the line was the answer, with STATUS the re-run's.
 */
static scr_bool
run_element_co_answer_400 (scr_gameref_t game, const scr_char *command,
                           scr_bool &status)
{
  if (!lib_co_400_question_pending () || scr_strempty (command)
      || (status && !lib_co_400_line_refused ()))
    return FALSE;

  const std::string rerun (lib_co_400_pending_is_npc ()
                           ? lib_co_400_npc_answer_line (command)
                           : lib_co_400_object_answer_line (command));

  lib_co_400_take_question ();
  pf_empty (gs_get_filter (game));

  /*
   * 489FEB is above the stores that mark a line administrative, so the
   * re-run starts a turn of its own and the answer is counted by what
   * the rebuilt line does -- not by the prompt that asked for it.
   * p4WTIE, run400 runner_probes/wtie.run400.w8.txt turn 10: `cut rope
   * with stone` is the question and no turn, and `red stone` runs `cut
   * rope with red stone`, answers "You don't have the red stone." and
   * ticks.  A re-run that asks again marks itself
   * (lib_co_400_raise_common()), and so does the still-ambiguous arm
   * below, so only a line that did something reaches the clock.
   */
  game->is_admin = FALSE;

  status = run_all_commands (game, rerun.c_str ());
  if (!status)
    {
      pf_empty (gs_get_filter (game));
      lib_co_400_print_still_ambiguous (game);
      status = TRUE;
    }
  return TRUE;
}

/*
 * run_element_not_understood()
 *
 * Nothing answered the element: the game's DontUnderstand text, or
 * nothing at all after a 4.0 ending.
 */
static void
run_element_not_understood (scr_gameref_t game, const scr_char *command)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *message;

  /*
   * An EMPTY line element complains too.  Upstream SCARE guarded this
   * whole block with `if (!scr_strempty (command))`, so a bare Return
   * printed nothing.  Both Runners answer one with DontUnderstand:
   * the stardust and xfiles feeds were the only CRLF feeds in the Wine
   * Runner harness, so every command in those two runs went in followed
   * by an extra empty Return, and run390 answered all 115 of them with
   * S_Tar_Dus's ALR for the message ("I are confused.  DURHH!",
   * runner_probes/stardust.run390.txt) and run400 all 22 of xfiles'
   * ("Nope!", runner_probes/xfiles.run400.b.txt).  No walk or event
   * line follows one, so the turn does not tick either -- which the
   * FALSE return below already gives us, run_main_loop() ticking only
   * on TRUE.  Only a genuinely empty input line gets here: the splitter
   * above takes the first character even when it is a separator, so "."
   * and "i. ." were complaints before this and still are.
   */

  /*
   * 4.0: a line that names a character and was ended by a task says
   * nothing at all.  run400's tail tests two conditions together --
   * `48B573: If MemVar_4941B0 = "" And var_29C = 0 Then
   * MemVar_4941B0 = MemVar_4941A8` -- where var_29C is set by the walk
   * over the characters at 48B53C-48B569 (uip_line_names_npc() here) and
   * MemVar_4941A8 is the game's DontUnderstand text.  Outside an ending
   * the second condition never shows: a line naming a present character
   * that nothing else answered gets the catch-all from characters()
   * instead, so the buffer is not empty.  Once a task has ended the game
   * that catch-all is suppressed (4805CD, see lib_cmd_verb_npc()), and
   * the empty buffer meets var_29C = 1 and prints nothing.
   *
   * easter.taf's winning `show basket to shopkeeper` is the measured
   * case: run400 runner_probes/easter.run400.txt:304-308 goes straight
   * from the task's text to the WinText.  Gated on the ending so the
   * general shape of the test cannot disturb an ordinary line.
   */
  if (game->pending_endgame != 0
      && run_get_version (bundle) == TAF_VERSION_400
      && uip_line_names_npc (game, run_line_element))
    {
      run_line_buffer[0] = NUL;
      return;
    }

  /*
   * Command line element not understood.  Own the escaped copy with
   * RAII (as the sibling code above does): var_set_ref_text() can throw
   * (scr_fatal_error), and the old manual scr_free() after it leaked on
   * the throw.
   */
  scr_owned_string escaped
      (pf_escape (scr_normalize_string (run_line_element)));
  var_set_ref_text (gs_get_vars (game), escaped.get ());
  message = prop_get_global_string (bundle, "DontUnderstand");
  pf_buffer_string (filter, message);
  pf_buffer_character (filter, '\n');

  /*
   * A line element that's not understood leaves the rest of the line
   * alone.  Upstream SCARE threw the remaining elements out here; no
   * Runner does.  run400 re-reads its queue at the very END of
   * generaltasks (48BCF2, `If MemVar_4942E4 <> "" Then MemVar_494174 =
   * MemVar_4942E4 : GoTo 489FEB`), below every exit the DontUnderstand
   * text can take: `wave zzz and yyy` on the p4AND probe answers NO IDEA
   * twice (runner_probes/and.run400.txt).  run390 answers `zzz, look`
   * with NO IDEA and the room (runner_probes/ask.run390.split2.txt),
   * run380 `zzz then look` with "I don't understand." and the room
   * (runner_probes/ask.run380.split2.rtf).
   */

  /*
   * 4.0: and what the setter was handed on the way down still stands --
   * openclose's loop runs above therest's DontUnderstand, so `cut rope
   * with gems` answers NO IDEA and leaves "it" at the emerald (p4WTIE,
   * runner_probes/wtie3.run400.it2.txt).  See uip_note_antecedent_400()
   * in scparser.cpp.
   */
  if (run_get_version (bundle) >= TAF_VERSION_400)
    {
      uip_commit_antecedent_400 (game);
      uip_set_pronoun_flags (FALSE, FALSE);
    }
  /* 3.9: generaltasks' co() pre-pass ran before anything answered. */
  else if (run_get_version (bundle) == TAF_VERSION_390)
    uip_assign_pronouns (game, command);
}

/*
 * run_element_note_undo()
 *
 * The element was answered: unless administrative, back up any valid
 * undo, copy the temporary game into the undo buffer, and assign the
 * pronouns the command used ready for the next element.
 */
static void
run_element_note_undo (scr_gameref_t game, const scr_char *command,
                       scr_bool was_undo_available)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * Unless administrative, back up any valid undo, copy the temporary
   * game into the undo buffer, flag the undo buffer as available, and
   * assign any pronouns used in the command ready for the next iteration.
   */
  /*
   * An undo, restore or restart is never itself backed up: at 3.9 undo
   * and restore are real turns (see lib_is_version_390() in sclibrar.cpp),
   * and backing up an undo would re-arm the buffer it just spent.
   */
  if (!game->is_admin && !game->do_restart && !game->do_restore
      && !(was_undo_available && !game->undo_available))
    {
      if (game->undo_available)
        memo_save_game (gs_get_memento (game), game->undo,
                        run_undo_text.c_str ());

      gs_copy (game->undo, game->temporary);
      run_undo_text = run_temporary_text;
      game->undo_available = TRUE;

      uip_assign_pronouns (game, command);
    }
  else if (run_get_version (bundle) >= TAF_VERSION_400
           && !game->do_restart && !game->do_restore)
    {
      /*
       * A 4.0 line that was not a turn -- a question, "see no such
       * thing" -- still leaves what the setter was handed; see
       * uip_note_antecedent_400() in scparser.cpp.
       */
      uip_commit_antecedent_400 (game);
      uip_set_pronoun_flags (FALSE, FALSE);
    }
  else if (run_get_version (bundle) == TAF_VERSION_390
           && !game->do_restart && !game->do_restore)
    uip_assign_pronouns (game, command);
}

/*
 * run_element_finish()
 *
 * After an answered element: the history, the rest of the line a walk
 * holds back, the restart/restore/undo special case, and the element
 * `again` will repeat.
 */
static void
run_element_finish (scr_gameref_t game, scr_bool is_rerunning,
                    scr_bool was_undo_available)
{
  const scr_memo_setref_t memento = gs_get_memento (game);

  /*
   * If do_again is set, we'll come round with the prior command in line
   * element in a moment, so save nothing for that case.  Otherwise save the
   * command in the history.
   */
  if (!scr_strempty (run_line_element) && !game->do_again)
    {
      /*
       * If this is a failed redo, redo_sequence will be set but do_again will
       * be clear.  Suppress the save for this special case; otherwise, failed
       * redo commands get into the history, where they can cause problems
       * later on.
       */
      if (game->redo_sequence == 0)
        {
          scr_int timestamp;

          timestamp = var_get_elapsed_seconds (gs_get_vars (game));
          memo_save_command (memento, run_line_element, timestamp,
                             game->turns);
        }
      else
        game->redo_sequence = 0;
    }

  /*
   * This element set a `go <place>` walk going: its steps are read at the
   * prompt, so the rest of the line must not run first.  Pre-4.0 holds it
   * until the arrival; run400's first step empties the queue.  See
   * run_goto_rest.
   */
  if (run_goto_next < run_goto_steps.size () && run_line_buffer[0] != NUL)
    {
      if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
        run_goto_rest = run_line_buffer;
      run_line_buffer[0] = NUL;
    }

  /*
   * Special case restart and restore commands; throw out any remaining input
   * and return straight away.  Do the same if this was an undo, detected by
   * noting that undo is no longer available, where it was on entry.
   */
  if (game->do_restart || game->do_restore
      || (was_undo_available && !game->undo_available))
    {
      run_line_buffer[0] = NUL;
      run_cancel_goto_walk ();
      return;
    }

  /* If not empty, consider as saving for "again" calls and in the history. */
  if (!scr_strempty (run_line_element))
    {
      /*
       * Unless "again", note this line element as prior input.  "Again" shows
       * up as do_again set in the game, where it wasn't when we entered here.
       */
      if (!game->do_again && !is_rerunning)
        strncpy (run_prior_element, run_line_element, LINE_BUFFER_SIZE);

      /*
       * If this was a request to run a command from the history, copy that
       * command into run_prior_element for the next iteration.  The library
       * should have verified the value in redo_sequence, so fetching the
       * command string should not fail.
       */
      if (game->do_again && game->redo_sequence != 0)
        {
          const scr_char *redo_command;

          redo_command = memo_find_command (memento, game->redo_sequence);
          if (redo_command)
            strncpy (run_prior_element, redo_command, LINE_BUFFER_SIZE);
          else
            {
              scr_error ("run_player_input: invalid redo sequence request\n");
              game->do_again = FALSE;
            }
          game->redo_sequence = 0;
        }
    }
}

/*
 * run_player_input()
 *
 * Take a line of player input and buffer it.  Split the line into elements
 * separated by periods.  For the first element, try to match it to either a
 * task or a standard command, and return TRUE if it matched, FALSE otherwise.
 *
 * On subsequent calls, successively work with the next line element until
 * none remain.  In this case, prompt for more player input and continue as
 * above.
 *
 * For the case of "again" or "g", rerun the last successful command element.
 *
 * One extra special special case; if called with a game that is not running,
 * this is a signal to reset all noted line input to initial conditions, and
 * just return.  Sorry about the ugliness.
 */
scr_bool
run_player_input (scr_gameref_t game)
{
  scr_bool is_rerunning, was_undo_available, status;
  scr_bool is_new_line = FALSE;
  std::string command;

  /* Special case; reset statics if the game isn't running. */
  if (!game->is_running)
    {
      run_input_reset ();
      return TRUE;
    }

  /*
   * Save the settings of the game's do_again and undo_available flags for
   * later checks.
   */
  is_rerunning = game->do_again;
  was_undo_available = game->undo_available;

  /* See if the player asked to rerun a command element. */
  if (game->do_again)
    {
      if (!run_repeat_element (game))
        return FALSE;
    }
  else
    {
      is_new_line = run_read_line (game);
      run_cut_element (game);
    }

  /* `again` is no element of its own; see run_count_element(). */
  if (!is_rerunning)
    run_count_element (game);

  /* Copy the current game to the temporary undo buffer, along with the
     output that the turn just finished left it with. */
  gs_copy (game->temporary, game);
  run_temporary_text = pf_take_printed (gs_get_filter (game));
  game->player_moved_by_command = FALSE;

  run_element_command (game, command);
  run_element_begin (game, command.c_str (), is_new_line);

  /*
   * The repeat words are tested on the whole line before any task gets it:
   * run400 89FE2 and run390 45F094 sit above tasks(0) (45F48B), run380 441B79
   * and run370 43B3C9 likewise, but without `g`.  So a task that listens for
   * `g` can never see it typed from 3.90 on; shadowpeak's riddle (TASK 404,
   * answer `g`) repeats the previous command in run400 (2026-09-14).  Below
   * 3.90 the `g` row in the standard table stays a Scarier abbreviation and
   * keeps its place after the tasks.
   */
  if (!is_rerunning && run_is_repeat_word (game, run_line_element))
    status = lib_cmd_again (game);
  else
    /* Try the command line element against command matchers. */
    status = run_all_commands (game, command.c_str ());

  status = run_element_questions (game, command.c_str (), status);

  if (run_element_co_answer_400 (game, command.c_str (), status))
    {
      run_line_buffer[0] = NUL;
      return status;
    }
  if (!status)
    {
      run_element_not_understood (game, command.c_str ());
      return status;
    }
  run_element_note_undo (game, command.c_str (), was_undo_available);
  run_element_finish (game, is_rerunning, was_undo_available);
  return status;
}
