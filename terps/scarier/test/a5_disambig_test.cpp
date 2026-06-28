/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Phase 4 general-reference disambiguation test.
 *
 * Self-contained (no third-party game data): builds a tiny ADRIFT-5 adventure
 * as in-memory XML, runs it through the real a5xml/a5model pipeline, and drives
 * a5run_input to exercise the disambiguation prompt + cross-turn resolution.
 *
 * The adventure is a single room holding two objects that share the noun "key"
 * (a brass key and an iron key), plus an unrestricted EXAMINE general task.  So
 * "examine key" is genuinely ambiguous -- with no RNG-gated catch-all to shadow
 * it, the prompt path is deterministic.  This is the disambiguation analogue of
 * a5parse_test, and it guards:
 *
 *   - the "Which key?  The brass key or the iron key." prompt (AmbWord +
 *     definite-name list + ToProper + pSpace's two spaces),
 *   - clarifier resolution on the next turn ("brass" -> runs EXAMINE on Key1),
 *   - a fresh command interrupting a pending prompt ("look" wins, prompt drops),
 *   - restriction-based refinement collapsing an "ambiguity" to one candidate
 *     (a second key already held fails the take restriction, so "take key"
 *     resolves uniquely instead of prompting).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5model.h"
#include "a5run.h"

/* A minimal ADRIFT-5 adventure: player in Room1 with a brass key + an iron key
   (both static, both at Room1), an unrestricted EXAMINE task, and a TAKE task
   whose restriction is "the object must not already be held". */
static const char *kXml =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<Adventure>\n"
"  <Title>Disambiguation Test</Title>\n"
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
"    <LongDescription><Description><Text>A bare stone vault.</Text></Description></LongDescription>\n"
"  </Location>\n"
"  <Object>\n"
"    <Key>Key1</Key>\n"
"    <Article>the</Article>\n"
"    <Prefix>brass</Prefix>\n"
"    <Name>key</Name>\n"
"    <Property><Key>StaticOrDynamic</Key><Value>Dynamic</Value></Property>\n"
"    <Property><Key>DynamicLocation</Key><Value>In Location</Value></Property>\n"
"    <Property><Key>InLocation</Key><Value>Room1</Value></Property>\n"
"  </Object>\n"
"  <Object>\n"
"    <Key>Key2</Key>\n"
"    <Article>the</Article>\n"
"    <Prefix>iron</Prefix>\n"
"    <Name>key</Name>\n"
"    <Property><Key>StaticOrDynamic</Key><Value>Dynamic</Value></Property>\n"
"    <Property><Key>DynamicLocation</Key><Value>In Location</Value></Property>\n"
"    <Property><Key>InLocation</Key><Value>Room1</Value></Property>\n"
"  </Object>\n"
"  <Task>\n"
"    <Key>ExamineObjects</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>1</Priority>\n"
"    <Repeatable>1</Repeatable>\n"
"    <Command>[examine/x] %object%</Command>\n"
"    <CompletionMessage><Description><Text>Examined: %object1%</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>TakeObjects</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>2</Priority>\n"
"    <Repeatable>1</Repeatable>\n"
"    <Command>[take/get] %object%</Command>\n"
"    <Restrictions>\n"
"      <Restriction><Object>ReferencedObject MustNot BeHeldByCharacter Player</Object></Restriction>\n"
"      <BracketSequence>#</BracketSequence>\n"
"    </Restrictions>\n"
"    <Actions>\n"
"      <MoveObject>Object ReferencedObject ToCarriedBy Player</MoveObject>\n"
"    </Actions>\n"
"    <CompletionMessage><Description><Text>Taken: %object1%</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>Look</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>3</Priority>\n"
"    <Command>[look/l]</Command>\n"
"  </Task>\n"
"</Adventure>\n";

static int failures = 0;

/* Run one command and check the output contains (or does not contain) a needle. */
static void
check (a5_run_t *run, const char *cmd, const char *needle, int want)
{
  char *out = a5run_input (run, cmd);
  int has = (strstr (out, needle) != NULL);
  if (has != want)
    {
      printf ("FAIL: \"%s\" -> output %s \"%s\"\n        got: %s\n",
              cmd, want ? "should contain" : "should NOT contain", needle, out);
      failures++;
    }
  free (out);
}

/* Like check() but exact-match the whole (trimmed) output. */
static void
check_exact (a5_run_t *run, const char *cmd, const char *want)
{
  char *out = a5run_input (run, cmd);
  if (strcmp (out, want) != 0)
    {
      printf ("FAIL: \"%s\"\n  want: [%s]\n  got:  [%s]\n", cmd, want, out);
      failures++;
    }
  free (out);
}

int
main (void)
{
  uint32_t len = (uint32_t) strlen (kXml);
  char *buf = (char *) malloc (len + 1);
  memcpy (buf, kXml, len + 1);

  a5_xml_doc_t *doc = a5xml_parse (buf, len);
  if (doc == NULL) { printf ("a5_disambig_test: XML parse failed\n"); return 1; }
  a5_adventure_t *adv = a5model_from_doc (doc);
  if (adv == NULL) { printf ("a5_disambig_test: model build failed\n"); return 1; }
  a5_run_t *run = a5run_new (adv);

  /* 1. An ambiguous reference raises the prompt (exact wording). */
  check_exact (run, "examine key",
               "Which key?  The brass key or the iron key.");

  /* 2. A clarifier narrows to one candidate and runs the remembered task. */
  check (run, "brass", "Examined: brass key", 1);

  /* 3. Re-ask, then interrupt with a fresh command: the command wins, the
        pending prompt is dropped. */
  check_exact (run, "examine key",
               "Which key?  The brass key or the iron key.");
  check (run, "look", "The Vault", 1);
  /* the prompt must be gone now */
  check (run, "look", "Which key", 0);

  /* 4. Restriction-based refinement: once the brass key is held, "take key"
        is no longer ambiguous -- the take restriction (MustNot be held) rules
        out the held brass key, leaving the iron key as the unique match. */
  check (run, "take brass key", "Taken: brass key", 1);     /* now held */
  check (run, "take key", "Taken: iron key", 1);            /* refined unique */
  check (run, "take key", "Which key", 0);

  a5run_free (run);
  a5model_free (adv);

  if (failures == 0)
    printf ("a5_disambig_test: all checks passed\n");
  else
    printf ("a5_disambig_test: %d FAILURE(S)\n", failures);
  return failures == 0 ? 0 : 1;
}
