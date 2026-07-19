/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Phase 5 save/restore round-trip test.
 *
 * Self-contained (no third-party game data): builds a tiny ADRIFT-5 adventure
 * as in-memory XML, runs it through the real a5xml/a5model/a5run pipeline, then
 * proves a5run_save / a5run_restore capture enough state for a byte-identical
 * continuation:
 *
 *   1. Run A: play the intro + a "pre-split" command sequence (take an object,
 *      roll a RAND variable, tick a timed event), then a5run_save -> S.
 *   2. Continue run A with a "continuation" sequence and record its output.
 *   3. Run B: a fresh a5run_new (NO intro), a5run_restore(S), then the *same*
 *      continuation sequence.
 *   4. Assert B's per-turn output is byte-identical to A's.
 *
 * This exercises every serialised facet at once: object location (take/drop),
 * a numeric variable + the RNG state (the rolled value must match across the
 * restore), the event timer (the clock strike must land on the same turn), the
 * "seen" sets and completed tasks (via the room view), and that a restored run
 * needs no intro replay.  It is the save analogue of a5_walk_test / a5objtest.
 *
 * A final section exercises the multi-level undo stack built on the same
 * serialisation (a5run_snapshot / a5run_undo / a5run_undo_forget): walking back
 * several turns replays byte-identically (RNG included), the stack drains to a
 * hard failure, and the depth cap holds.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../adrift5/a5deobf.h"
#include "../adrift5/a5model.h"
#include "../adrift5/a5run.h"

static int failures = 0;

static const char *kXml =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<Adventure>\n"
"  <Title>Save Test</Title>\n"
"  <Character>\n"
"    <Key>Player</Key>\n"
"    <Name>Anonymous</Name>\n"
"    <Type>Player</Type>\n"
"    <Perspective>SecondPerson</Perspective>\n"
"    <Property><Key>CharacterLocation</Key><Value>At Location</Value></Property>\n"
"    <Property><Key>CharacterAtLocation</Key><Value>Room1</Value></Property>\n"
"  </Character>\n"
"  <Location>\n"
"    <Key>Room1</Key>\n"
"    <ShortDescription><Description><Text>The Vault</Text></Description></ShortDescription>\n"
"    <LongDescription><Description><Text>A small stone vault.</Text></Description></LongDescription>\n"
"  </Location>\n"
"  <Object>\n"
"    <Key>Coin</Key>\n"
"    <Article>a</Article>\n"
"    <Name>coin</Name>\n"
"    <Property><Key>StaticOrDynamic</Key><Value>Dynamic</Value></Property>\n"
"    <Property><Key>DynamicLocation</Key><Value>In Location</Value></Property>\n"
"    <Property><Key>InLocation</Key><Value>Room1</Value></Property>\n"
"  </Object>\n"
"  <Variable>\n"
"    <Key>RollVar</Key><Name>rollvar</Name><Type>Numeric</Type>\n"
"  </Variable>\n"
"  <Task>\n"
"    <Key>RollTask</Key><Type>General</Type><Priority>1</Priority><Repeatable>1</Repeatable>\n"
"    <Command>roll</Command>\n"
"    <Actions><SetVariable>RollVar = \"RAND (1, 1000000)\"</SetVariable></Actions>\n"
"    <CompletionMessage><Description><Text>You rolled %rollvar%.</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>GetCoin</Key><Type>General</Type><Priority>2</Priority>\n"
"    <Command>get coin</Command>\n"
"    <Actions><MoveObject>Object Coin ToCarriedBy Player</MoveObject></Actions>\n"
"    <CompletionMessage><Description><Text>Taken.</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>DropCoin</Key><Type>General</Type><Priority>3</Priority>\n"
"    <Command>drop coin</Command>\n"
"    <Actions><MoveObject>Object Coin ToSameLocationAs Player</MoveObject></Actions>\n"
"    <CompletionMessage><Description><Text>Dropped.</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>WaitTask</Key><Type>General</Type><Priority>4</Priority><Repeatable>1</Repeatable>\n"
"    <Command>wait</Command>\n"
"    <CompletionMessage><Description><Text>Time passes.</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>Look</Key><Type>General</Type><Priority>5</Priority>\n"
"    <Command>[look/l]</Command>\n"
"  </Task>\n"
"  <Event>\n"
"    <Key>Clock</Key>\n"
"    <Type>TurnBased</Type>\n"
"    <WhenStart>Immediately</WhenStart>\n"
"    <Length>100</Length>\n"
"    <SubEvent>\n"
"      <When>6 FromStartOfEvent</When>\n"
"      <Action><Description><Text>The clock strikes.</Text></Description></Action>\n"
"      <OnlyApplyAt>Room1</OnlyApplyAt>\n"
"    </SubEvent>\n"
"  </Event>\n"
"</Adventure>\n";

static a5_adventure_t *
build_adv (void)
{
  uint32_t len = (uint32_t) strlen (kXml);
  char *buf = (char *) malloc (len + 1);
  memcpy (buf, kXml, len + 1);
  a5_xml_doc_t *doc = a5xml_parse (buf, len);
  if (!doc) { printf ("a5_save_test: XML parse failed\n"); failures++; return NULL; }
  a5_adventure_t *adv = a5model_from_doc (doc);
  if (!adv) { printf ("a5_save_test: model build failed\n"); failures++; return NULL; }
  return adv;
}

/* Pre-split: take the coin, roll once (advances the RNG), tick the event. */
static const char *kPreSplit[] = { "get coin", "roll", "wait", "wait" };
/* Continuation: ticks that cross the clock strike, a roll (RNG round-trip),
   look (room view: seen sets + carried state), drop, look. */
static const char *kCont[] = { "wait", "wait", "roll", "look", "drop coin", "look" };

int
main (void)
{
  a5_adventure_t *adv = build_adv ();
  size_t i;
  char *save;
  size_t save_len = 0;
  std::vector<std::string> outA, outB;
  int saw_clock = 0, saw_roll = 0;

  if (adv == NULL)
    return 1;

  /* --- Run A: intro + pre-split, then save. --- */
  a5_run_t *runA = a5run_new (adv);
  free (a5run_intro (runA));
  for (i = 0; i < sizeof kPreSplit / sizeof *kPreSplit; i++)
    free (a5run_input (runA, kPreSplit[i]));

  save = a5run_save (runA, &save_len);
  /* The save is now a FrankenDrift-compatible <Game> document carrying the
     full-fidelity Scarier snapshot in a <ScarierExt> child. */
  if (save == NULL || save_len == 0
      || strstr (save, "<Game>") == NULL
      || strstr (save, "<ScarierExt>") == NULL)
    { printf ("FAIL: a5run_save produced no/invalid output\n"); failures++; }

  /* --- Run A continuation. --- */
  for (i = 0; i < sizeof kCont / sizeof *kCont; i++)
    {
      char *o = a5run_input (runA, kCont[i]);
      outA.push_back (o ? o : "");
      if (strstr (o ? o : "", "The clock strikes.")) saw_clock = 1;
      if (strstr (o ? o : "", "You rolled "))         saw_roll = 1;
      free (o);
    }

  /* --- Run B: fresh, restore, continuation (no intro). --- */
  a5_run_t *runB = a5run_new (adv);
  if (!a5run_restore (runB, save, save_len))
    { printf ("FAIL: a5run_restore rejected the save buffer\n"); failures++; }
  for (i = 0; i < sizeof kCont / sizeof *kCont; i++)
    {
      char *o = a5run_input (runB, kCont[i]);
      outB.push_back (o ? o : "");
      free (o);
    }
  /* --- FrankenDrift-interop framing checks (no external runner needed). --- */
  {
    /* (1) zlib deflate/inflate is byte-exact (the on-disk framing). */
    uint32_t zlen = 0, xlen = 0;
    uint8_t *z = a5_deflate ((const uint8_t *) save, (uint32_t) save_len, &zlen);
    uint8_t *x = z ? a5_inflate (z, zlen, &xlen) : NULL;
    if (!z || !x || xlen != save_len || memcmp (x, save, save_len) != 0)
      { printf ("FAIL: zlib deflate/inflate not byte-exact\n"); failures++; }
    if (z && (z[0] != 0x78))
      { printf ("FAIL: save is not a zlib stream (magic %02x)\n", z[0]); failures++; }
    free (z);
    free (x);

    /* (2) A foreign FrankenDrift save (our <Game> with the <ScarierExt> block
       stripped) still restores from the FrankenDrift fields alone: the carried
       coin must stay carried (out of the room view). */
    std::string g (save, save_len);
    size_t a = g.find ("<ScarierExt>"), b = g.find ("</ScarierExt>");
    if (a == std::string::npos || b == std::string::npos)
      { printf ("FAIL: save lacks the <ScarierExt> block\n"); failures++; }
    else
      {
        std::string fd = g.substr (0, a)
                       + g.substr (b + strlen ("</ScarierExt>"));
        a5_run_t *runF = a5run_new (adv);
        if (!a5run_restore (runF, fd.c_str (), fd.size ()))
          { printf ("FAIL: foreign (no-ScarierExt) restore rejected\n"); failures++; }
        char *look = a5run_input (runF, "look");
        if (look && strstr (look, "coin"))
          { printf ("FAIL: foreign restore put the carried coin back in the room\n"); failures++; }
        free (look);
        a5run_free (runF);
      }
  }

  free (save);

  /* --- Compare. --- */
  for (i = 0; i < outA.size (); i++)
    if (outA[i] != outB[i])
      {
        printf ("FAIL: continuation turn %zu (\"%s\") diverged after restore\n"
                "  A: [%s]\n  B: [%s]\n",
                i, kCont[i], outA[i].c_str (), outB[i].c_str ());
        failures++;
      }

  /* The continuation must actually exercise the event timer and the RNG, else
     the round-trip proves little. */
  if (!saw_clock)
    { printf ("FAIL: the timed event never fired in the continuation\n"); failures++; }
  if (!saw_roll)
    { printf ("FAIL: the roll task never produced output\n"); failures++; }

  /* --- Multi-level undo: snapshot before each turn (as the Glk frontend
     does), walk back three turns, and prove the replay is byte-identical --
     including a fresh RAND draw, so the RNG walked back too.  Then drain the
     stack to a hard 0, check the depth cap, and check undo_forget. --- */
  {
    static const char *kUndoSeq[] = { "roll", "wait", "roll", "get coin", "look" };
    const size_t n = sizeof kUndoSeq / sizeof *kUndoSeq;
    std::vector<std::string> first;
    int undos;
    a5_run_t *runC = a5run_new (adv);

    free (a5run_intro (runC));
    for (i = 0; i < n; i++)
      {
        char *o;
        a5run_snapshot (runC);
        o = a5run_input (runC, kUndoSeq[i]);
        first.push_back (o ? o : "");
        free (o);
      }
    for (i = 0; i < 3; i++)
      if (!a5run_undo (runC))
        { printf ("FAIL: undo %zu of 3 returned no snapshot\n", i + 1); failures++; }
    /* The post-undo room redisplay (a5run_look) must show the location and --
       proven by the byte-identical replay below -- must not perturb the state
       or the RNG (this game's descriptions draw nothing). */
    {
      char *look = a5run_look (runC);
      if (look == NULL || strstr (look, "A small stone vault.") == NULL)
        { printf ("FAIL: a5run_look after undo missed the room view: [%s]\n",
                  look ? look : "(null)"); failures++; }
      free (look);
    }
    /* Replay the last three commands (the first is a re-roll). */
    for (i = n - 3; i < n; i++)
      {
        char *o;
        a5run_snapshot (runC);
        o = a5run_input (runC, kUndoSeq[i]);
        if (first[i] != (o ? o : ""))
          {
            printf ("FAIL: undo replay of \"%s\" diverged\n  1st: [%s]\n  2nd: [%s]\n",
                    kUndoSeq[i], first[i].c_str (), o ? o : "");
            failures++;
          }
        free (o);
      }
    /* Two pre-undo snapshots remain under the three replay ones: the stack
       must drain in exactly five undos. */
    undos = 0;
    while (a5run_undo (runC))
      undos++;
    if (undos != 5)
      { printf ("FAIL: undo stack drained after %d undos, want 5\n", undos); failures++; }
    /* Depth cap: push more snapshots than the stack keeps; only the newest
       A5_UNDO_DEPTH (16, matching the v4 memo ring) survive. */
    for (i = 0; i < 20; i++)
      {
        a5run_snapshot (runC);
        free (a5run_input (runC, "wait"));
      }
    undos = 0;
    while (a5run_undo (runC))
      undos++;
    if (undos != 16)
      { printf ("FAIL: capped undo stack drained after %d undos, want 16\n", undos); failures++; }
    /* undo_forget clears the whole stack. */
    a5run_snapshot (runC);
    a5run_snapshot (runC);
    a5run_undo_forget (runC);
    if (a5run_undo (runC))
      { printf ("FAIL: undo succeeded after undo_forget\n"); failures++; }
    a5run_free (runC);
  }

  a5run_free (runA);
  a5run_free (runB);
  a5model_free (adv);

  if (failures == 0)
    printf ("a5_save_test: all checks passed\n");
  else
    printf ("a5_save_test: %d failure(s)\n", failures);
  return failures ? 1 : 0;
}
