/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- multiple-object reference resolution test.
 *
 * Self-contained (no third-party game data): builds a tiny ADRIFT-5 adventure
 * as in-memory XML, runs it through the real a5xml/a5model/a5run pipeline, and
 * drives a5run_input to exercise the %objects% plural-reference grammar
 * (clsUserSession.InputMatchesObjects + the ExecuteSingleAction per-item loop):
 *
 *   - "X and Y" explicit lists (two items, both moved),
 *   - the bare-"all" expansion over the player's seen objects, with per-item
 *     restriction filtering (a non-takeable object and the already-held ones are
 *     skipped),
 *   - the FailOverride shown when a "get all"-style command yields no item,
 *   - "all except Z" removing a named object from the set,
 *   - plural-noun matching ("take balls" -> every ball, each its own item),
 *   - the single-object case still routed through %objects% unchanged.
 *
 * The room holds a gold coin, a brass key, a lamp, a stone pillar (no Takeable
 * property -> scenery) and two balls (red, blue).  The TakeObjects task moves
 * %objects% one item at a time and lists them in its completion message.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../adrift5/a5model.h"
#include "../adrift5/a5run.h"

static const char *kXml =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<Adventure>\n"
"  <Title>Multiple-Object Test</Title>\n"
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
"    <Key>Coin1</Key><Article>the</Article><Prefix>gold</Prefix><Name>coin</Name>\n"
"    <Property><Key>StaticOrDynamic</Key><Value>Dynamic</Value></Property>\n"
"    <Property><Key>DynamicLocation</Key><Value>In Location</Value></Property>\n"
"    <Property><Key>InLocation</Key><Value>Room1</Value></Property>\n"
"    <Property><Key>Takeable</Key><Value></Value></Property>\n"
"  </Object>\n"
"  <Object>\n"
"    <Key>Key1</Key><Article>the</Article><Prefix>brass</Prefix><Name>key</Name>\n"
"    <Property><Key>StaticOrDynamic</Key><Value>Dynamic</Value></Property>\n"
"    <Property><Key>DynamicLocation</Key><Value>In Location</Value></Property>\n"
"    <Property><Key>InLocation</Key><Value>Room1</Value></Property>\n"
"    <Property><Key>Takeable</Key><Value></Value></Property>\n"
"  </Object>\n"
"  <Object>\n"
"    <Key>Lamp1</Key><Article>the</Article><Name>lamp</Name>\n"
"    <Property><Key>StaticOrDynamic</Key><Value>Dynamic</Value></Property>\n"
"    <Property><Key>DynamicLocation</Key><Value>In Location</Value></Property>\n"
"    <Property><Key>InLocation</Key><Value>Room1</Value></Property>\n"
"    <Property><Key>Takeable</Key><Value></Value></Property>\n"
"  </Object>\n"
"  <Object>\n"
"    <Key>Pillar1</Key><Article>the</Article><Prefix>stone</Prefix><Name>pillar</Name>\n"
"    <Property><Key>StaticOrDynamic</Key><Value>Dynamic</Value></Property>\n"
"    <Property><Key>DynamicLocation</Key><Value>In Location</Value></Property>\n"
"    <Property><Key>InLocation</Key><Value>Room1</Value></Property>\n"
"  </Object>\n"
"  <Object>\n"
"    <Key>Ball1</Key><Article>the</Article><Prefix>red</Prefix><Name>ball</Name>\n"
"    <Property><Key>StaticOrDynamic</Key><Value>Dynamic</Value></Property>\n"
"    <Property><Key>DynamicLocation</Key><Value>In Location</Value></Property>\n"
"    <Property><Key>InLocation</Key><Value>Room1</Value></Property>\n"
"    <Property><Key>Takeable</Key><Value></Value></Property>\n"
"  </Object>\n"
"  <Object>\n"
"    <Key>Ball2</Key><Article>the</Article><Prefix>blue</Prefix><Name>ball</Name>\n"
"    <Property><Key>StaticOrDynamic</Key><Value>Dynamic</Value></Property>\n"
"    <Property><Key>DynamicLocation</Key><Value>In Location</Value></Property>\n"
"    <Property><Key>InLocation</Key><Value>Room1</Value></Property>\n"
"    <Property><Key>Takeable</Key><Value></Value></Property>\n"
"  </Object>\n"
"  <Task>\n"
"    <Key>TakeObjects</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>1</Priority>\n"
"    <Repeatable>1</Repeatable>\n"
"    <Command>[take/get] %objects%</Command>\n"
"    <Restrictions>\n"
"      <Restriction><Object>ReferencedObject MustNot BeHeldByCharacter Player</Object></Restriction>\n"
"      <Restriction><Object>ReferencedObject Must HaveProperty Takeable</Object></Restriction>\n"
"    </Restrictions>\n"
"    <FailOverride><Description><Text>There is nothing worth taking here.</Text></Description></FailOverride>\n"
"    <Actions>\n"
"      <MoveObject>Object ReferencedObjects ToCarriedBy Player</MoveObject>\n"
"    </Actions>\n"
"    <CompletionMessage><Description><Text>You take %objects%.</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>Look</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>2</Priority>\n"
"    <Command>[look/l]</Command>\n"
"  </Task>\n"
"</Adventure>\n";

static int failures = 0;

/* Assert a captured output string contains (or does not contain) a needle. */
static void
expect (const char *cmd, const char *out, const char *needle, int want)
{
  int has = (strstr (out, needle) != NULL);
  if (has != want)
    {
      printf ("FAIL: \"%s\" -> output %s \"%s\"\n        got: %s\n",
              cmd, want ? "should contain" : "should NOT contain", needle, out);
      failures++;
    }
}

static a5_run_t *
fresh (a5_adventure_t *adv)
{
  return a5run_new (adv);
}

int
main (void)
{
  uint32_t len = (uint32_t) strlen (kXml);
  char *buf = (char *) malloc (len + 1);
  memcpy (buf, kXml, len + 1);

  a5_xml_doc_t *doc = a5xml_parse (buf, len);
  if (doc == NULL) { printf ("a5_objects_test: XML parse failed\n"); return 1; }
  a5_adventure_t *adv = a5model_from_doc (doc);
  if (adv == NULL) { printf ("a5_objects_test: model build failed\n"); return 1; }

  char *out;

  /* --- Scenario A: explicit "and" list, "all" expansion, FailOverride. --- */
  {
    a5_run_t *run = fresh (adv);

    /* An explicit two-object list -> two items, both moved, listed together. */
    out = a5run_input (run, "take coin and lamp");
    expect ("take coin and lamp", out, "You take gold coin and lamp.", 1);
    free (out);

    /* "all" grabs the remaining *takeable, not-yet-held* objects, in model
       order: the brass key and the two balls.  The held coin/lamp and the
       scenery pillar (no Takeable property) are skipped -- proving both the
       restriction filter and that the per-item move of coin/lamp happened. */
    out = a5run_input (run, "take all");
    expect ("take all", out, "You take brass key, red ball and blue ball.", 1);
    expect ("take all", out, "pillar", 0);
    free (out);

    /* Nothing takeable is left unheld -> the FailOverride claims the turn. */
    out = a5run_input (run, "take all");
    expect ("take all", out, "There is nothing worth taking here.", 1);
    free (out);

    a5run_free (run);
  }

  /* --- Scenario B: "all except <noun>". --- */
  {
    a5_run_t *run = fresh (adv);

    /* "all except lamp" takes everything takeable but the lamp. */
    out = a5run_input (run, "take all except lamp");
    expect ("take all except lamp", out, "lamp", 0);
    expect ("take all except lamp", out, "gold coin", 1);
    free (out);

    /* The lamp really was left behind: a plain "take all" now takes just it.
       A single-item %objects% renders the entity KEY (FD ReplaceFunctions,
       MatchingPossibilities(0)); the synthetic lamp's key is "Lamp1". */
    out = a5run_input (run, "take all");
    expect ("take all", out, "You take Lamp1.", 1);
    free (out);

    a5run_free (run);
  }

  /* --- Scenario C: plural-noun matching ("balls" -> every ball). --- */
  {
    a5_run_t *run = fresh (adv);

    out = a5run_input (run, "take balls");
    expect ("take balls", out, "You take red ball and blue ball.", 1);
    free (out);

    /* Both balls are now held: the next "all" takes the others, not the balls. */
    out = a5run_input (run, "take all");
    expect ("take all", out, "ball", 0);
    expect ("take all", out, "You take gold coin, brass key and lamp.", 1);
    free (out);

    a5run_free (run);
  }

  /* --- Scenario D: a single typed noun still routes through %objects%. --- */
  {
    a5_run_t *run = fresh (adv);

    out = a5run_input (run, "take coin");
    /* Single item: bare %objects% renders the key "Coin1" (FD-faithful), not a
       "a and b" list.  The point of this scenario is that one typed noun still
       routes through the %objects% (plural) grammar as a single reference. */
    expect ("take coin", out, "You take Coin1.", 1);
    expect ("take coin", out, " and ", 0);           /* not a list for one item */
    free (out);

    a5run_free (run);
  }

  a5model_free (adv);

  if (failures == 0)
    printf ("a5_objects_test: all checks passed\n");
  else
    printf ("a5_objects_test: %d FAILURE(S)\n", failures);
  return failures == 0 ? 0 : 1;
}
