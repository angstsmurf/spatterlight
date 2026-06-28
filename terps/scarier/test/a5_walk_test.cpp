/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Phase 4 walks + event sub-events test.
 *
 * Self-contained (no third-party game data): builds two tiny ADRIFT-5
 * adventures as in-memory XML, runs them through the real a5xml/a5model/a5run
 * pipeline, and drives a5run_input over the turn loop.  This is the walk/
 * sub-event analogue of a5_disambig_test; it guards the clsWalk and
 * clsEvent.RunSubEvent ports that have no coverage in the shipped corpus
 * (the corpus walks are invisible "Follow Player" walks, and it ships no
 * SetLook sub-events at all):
 *
 *   Adventure A (walks):
 *     - a StartActive looping patrol walk that moves an NPC between two rooms,
 *     - the ShowEnterExit "<Name> enters/exits ..." narration with the correct
 *       DirectionTo/IsAdjacent prose ("to the north" / "from the north"),
 *     - a ComesAcross sub-walk firing a DisplayMessage gated by OnlyApplyAt
 *       (the guard "nods" only when it reaches the player's room),
 *     - the loop restart (exit / enter / exit / enter ... each turn).
 *
 *   Adventure B (event sub-events):
 *     - a SetLook sub-event whose text is appended to the room view (LookText),
 *     - a DisplayMessage sub-event shown only when the OnlyApplyAt gate matches
 *       the player's location (the Room1 bell shows, the Room2 horn does not).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5model.h"
#include "a5run.h"

static int failures = 0;

/* Assert a captured turn output contains (want=1) / does not contain (want=0)
   `needle`. */
static void
expect (const char *cmd, const char *out, const char *needle, int want)
{
  int has = strstr (out, needle) != NULL;
  if (has != want)
    {
      printf ("FAIL: \"%s\" -> output %s \"%s\"\n        got: [%s]\n",
              cmd, want ? "should contain" : "should NOT contain", needle, out);
      failures++;
    }
}

/* ------------------------------------------------------------- Adventure A */

static const char *kWalkXml =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<Adventure>\n"
"  <Title>Walk Test</Title>\n"
"  <Character>\n"
"    <Key>Player</Key>\n"
"    <Name>Anonymous</Name>\n"
"    <Type>Player</Type>\n"
"    <Perspective>SecondPerson</Perspective>\n"
"    <Property><Key>CharacterLocation</Key><Value>At Location</Value></Property>\n"
"    <Property><Key>CharacterAtLocation</Key><Value>Room1</Value></Property>\n"
"  </Character>\n"
"  <Character>\n"
"    <Key>Guard</Key>\n"
"    <Name>the guard</Name>\n"
"    <Article>the</Article>\n"
"    <Type>NonPlayer</Type>\n"
"    <Descriptor>guard</Descriptor>\n"
"    <Property><Key>CharacterLocation</Key><Value>At Location</Value></Property>\n"
"    <Property><Key>CharacterAtLocation</Key><Value>Room2</Value></Property>\n"
"    <Property><Key>Known</Key><Value>Selected</Value></Property>\n"
"    <Property><Key>ShowEnterExit</Key></Property>\n"
"    <Walk>\n"
"      <Description>Patrol</Description>\n"
"      <Loops>1</Loops>\n"
"      <StartActive>1</StartActive>\n"
"      <Step>Room1 1</Step>\n"
"      <Step>Room2 1</Step>\n"
"      <Activity>\n"
"        <When>ComesAcross %Player%</When>\n"
"        <Action><Description><Text>The guard nods at you.</Text></Description></Action>\n"
"        <OnlyApplyAt>Room1</OnlyApplyAt>\n"
"      </Activity>\n"
"    </Walk>\n"
"  </Character>\n"
"  <Location>\n"
"    <Key>Room1</Key>\n"
"    <ShortDescription><Description><Text>The Hall</Text></Description></ShortDescription>\n"
"    <LongDescription><Description><Text>A long hall.</Text></Description></LongDescription>\n"
"    <Movement><Direction>North</Direction><Destination>Room2</Destination></Movement>\n"
"  </Location>\n"
"  <Location>\n"
"    <Key>Room2</Key>\n"
"    <ShortDescription><Description><Text>The Yard</Text></Description></ShortDescription>\n"
"    <LongDescription><Description><Text>An open yard.</Text></Description></LongDescription>\n"
"    <Movement><Direction>South</Direction><Destination>Room1</Destination></Movement>\n"
"  </Location>\n"
"  <Task>\n"
"    <Key>WaitTask</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>1</Priority>\n"
"    <Repeatable>1</Repeatable>\n"
"    <Command>wait</Command>\n"
"    <CompletionMessage><Description><Text>You wait.</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"</Adventure>\n";

static void
test_walks (void)
{
  uint32_t len = (uint32_t) strlen (kWalkXml);
  char *buf = (char *) malloc (len + 1);
  memcpy (buf, kWalkXml, len + 1);
  a5_xml_doc_t *doc = a5xml_parse (buf, len);
  if (!doc) { printf ("a5_walk_test: walk XML parse failed\n"); failures++; return; }
  a5_adventure_t *adv = a5model_from_doc (doc);
  if (!adv) { printf ("a5_walk_test: walk model build failed\n"); failures++; return; }
  a5_run_t *run = a5run_new (adv);
  free (a5run_intro (run));        /* StartActive walk steps the guard to Room1 */

  /* The patrol loops Room1 <-> Room2.  On an exit turn the guard leaves to the
     north and does not nod; on an enter turn it arrives from the north and nods
     (the ComesAcross sub-walk, gated to the player's Room1). */
  char *o1 = a5run_input (run, "wait");
  expect ("wait#1", o1, "The guard exits to the north.", 1);
  expect ("wait#1", o1, "nods", 0);
  free (o1);

  char *o2 = a5run_input (run, "wait");
  expect ("wait#2", o2, "The guard enters from the north.", 1);
  expect ("wait#2", o2, "The guard nods at you.", 1);
  free (o2);

  char *o3 = a5run_input (run, "wait");        /* loop restart -> exits again */
  expect ("wait#3", o3, "The guard exits to the north.", 1);
  expect ("wait#3", o3, "nods", 0);
  free (o3);

  a5run_free (run);
  a5model_free (adv);
}

/* ------------------------------------------------------------- Adventure B */

static const char *kEventXml =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<Adventure>\n"
"  <Title>Event SubEvent Test</Title>\n"
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
"    <ShortDescription><Description><Text>The Crypt</Text></Description></ShortDescription>\n"
"    <LongDescription><Description><Text>A still crypt.</Text></Description></LongDescription>\n"
"  </Location>\n"
"  <Location>\n"
"    <Key>Room2</Key>\n"
"    <ShortDescription><Description><Text>Elsewhere</Text></Description></ShortDescription>\n"
"    <LongDescription><Description><Text>Somewhere else.</Text></Description></LongDescription>\n"
"  </Location>\n"
"  <Task>\n"
"    <Key>WaitTask</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>1</Priority>\n"
"    <Repeatable>1</Repeatable>\n"
"    <Command>wait</Command>\n"
"    <CompletionMessage><Description><Text>You wait.</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>Look</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>2</Priority>\n"
"    <Command>[look/l]</Command>\n"
"  </Task>\n"
"  <Event>\n"
"    <Key>Event1</Key>\n"
"    <Type>TurnBased</Type>\n"
"    <WhenStart>Immediately</WhenStart>\n"
"    <Length>3</Length>\n"
"    <SubEvent>\n"
"      <When>1 FromStartOfEvent</When>\n"
"      <What>SetLook</What>\n"
"      <Action><Description><Text>A cold draft chills the air.</Text></Description></Action>\n"
"      <OnlyApplyAt>Room1</OnlyApplyAt>\n"
"    </SubEvent>\n"
"    <SubEvent>\n"
"      <When>2 FromStartOfEvent</When>\n"
"      <Action><Description><Text>A bell tolls.</Text></Description></Action>\n"
"      <OnlyApplyAt>Room1</OnlyApplyAt>\n"
"    </SubEvent>\n"
"    <SubEvent>\n"
"      <When>2 FromStartOfEvent</When>\n"
"      <Action><Description><Text>A distant horn sounds.</Text></Description></Action>\n"
"      <OnlyApplyAt>Room2</OnlyApplyAt>\n"
"    </SubEvent>\n"
"  </Event>\n"
"</Adventure>\n";

static void
test_event_subevents (void)
{
  uint32_t len = (uint32_t) strlen (kEventXml);
  char *buf = (char *) malloc (len + 1);
  memcpy (buf, kEventXml, len + 1);
  a5_xml_doc_t *doc = a5xml_parse (buf, len);
  if (!doc) { printf ("a5_walk_test: event XML parse failed\n"); failures++; return; }
  a5_adventure_t *adv = a5model_from_doc (doc);
  if (!adv) { printf ("a5_walk_test: event model build failed\n"); failures++; return; }
  a5_run_t *run = a5run_new (adv);
  free (a5run_intro (run));

  /* Turn 1 (from-start 1): the SetLook fires -- no immediate output, but its
     text now rides on the room view via the LookText stack. */
  char *w1 = a5run_input (run, "wait");
  expect ("wait#1", w1, "draft", 0);               /* SetLook never self-displays */
  free (w1);

  /* Turn 2 (from-start 2): the Room1 bell shows (gate matches); the Room2 horn
     is gated out by its OnlyApplyAt=Room2. */
  char *w2 = a5run_input (run, "wait");
  expect ("wait#2", w2, "A bell tolls.", 1);
  expect ("wait#2", w2, "distant horn", 0);
  free (w2);

  /* LOOK: the SetLook text persists on the look stack and is appended to the
     room view. */
  char *lk = a5run_input (run, "look");
  expect ("look", lk, "A cold draft chills the air.", 1);
  free (lk);

  a5run_free (run);
  a5model_free (adv);
}

int
main (void)
{
  test_walks ();
  test_event_subevents ();
  if (failures == 0)
    printf ("a5_walk_test: all checks passed\n");
  else
    printf ("a5_walk_test: %d FAILURE(S)\n", failures);
  return failures == 0 ? 0 : 1;
}
