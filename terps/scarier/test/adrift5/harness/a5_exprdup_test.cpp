/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- identical `<#..#>` bodies in one block.
 *
 * The runner's ReplaceExpressions (Global.vb:510) collects its regex matches
 * up front and then `sText.Replace(m.Value, EvaluateExpression(..))`s each --
 * a replace-ALL, so every identical tag in a block shows the FIRST match's
 * value while the later matches still evaluate (their OneOf draws, keeping
 * the RNG stream in step) and land nowhere.  The Last Expedition's "<#oneOf(
 * "Eight","Seven",..)#> minutes later .. <#same#> minutes later" and Lost
 * Coastlines' "<# Oneof("two","three","four")#> return empty handed...
 * <#same#> do not return at all" print one number twice.
 *
 * Synthetic in-memory adventure, no game data.  Room1's description and an
 * AggregateOutput task (the deferred, end-of-Display draw path) each carry
 * A A B: two identical tags and a third distinct one.  Room2 carries A A' B
 * where A' differs from A only by whitespace -- a different m.Value, so an
 * independent draw.  Across reseeds: Room1's A pair must match, and its B
 * must equal Room2's B (the repeat DID draw, the stream stays aligned).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "../../../adrift5/a5model.h"
#include "../../../adrift5/a5rand.h"
#include "../../../adrift5/a5run.h"
#include "a5_test_fixtures.h"

static int failures = 0;

#define A "&lt;# oneof(\"alpha\",\"bravo\",\"charlie\",\"delta\",\"echo\",\"foxtrot\",\"golf\",\"hotel\") #&gt;"
#define A2 "&lt;#oneof(\"alpha\",\"bravo\",\"charlie\",\"delta\",\"echo\",\"foxtrot\",\"golf\",\"hotel\")#&gt;"
#define B "&lt;# rand(0,999999) #&gt;"

static const char *kXml =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<Adventure>\n"
"  <Title>Expression Duplicate Test</Title>\n"
A5_XML_PLAYER_AT ("Room1")
"  <Location>\n"
"    <Key>Room1</Key>\n"
"    <ShortDescription><Description><Text>The Wall</Text></Description></ShortDescription>\n"
"    <LongDescription><Description><Text>Scrawl: " A " " A " " B " end.</Text></Description></LongDescription>\n"
"  </Location>\n"
"  <Location>\n"
"    <Key>Room2</Key>\n"
"    <ShortDescription><Description><Text>The Yard</Text></Description></ShortDescription>\n"
"    <LongDescription><Description><Text>Scrawl: " A " " A2 " " B " end.</Text></Description></LongDescription>\n"
"  </Location>\n"
"  <Task>\n"
"    <Key>Look</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>2</Priority>\n"
"    <Repeatable>1</Repeatable>\n"
"    <Command>[look/l]</Command>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>GoYard</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>3</Priority>\n"
"    <Command>yard</Command>\n"
"    <CompletionMessage><Description><Text>You head for the yard.</Text></Description></CompletionMessage>\n"
"    <Actions><MoveCharacter>Character Player ToLocation Room2</MoveCharacter></Actions>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>Scrounge</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>1</Priority>\n"
"    <Repeatable>1</Repeatable>\n"
"    <AggregateOutput>1</AggregateOutput>\n"
"    <Command>scrounge</Command>\n"
"    <CompletionMessage><Description><Text>Scrawl: " A " " A " " B " end.</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"  <Task>\n"
"    <Key>Scrounge2</Key>\n"
"    <Type>General</Type>\n"
"    <Priority>4</Priority>\n"
"    <Repeatable>1</Repeatable>\n"
"    <AggregateOutput>1</AggregateOutput>\n"
"    <Command>rummage</Command>\n"
"    <CompletionMessage><Description><Text>Scrawl: " A " " A2 " " B " end.</Text></Description></CompletionMessage>\n"
"  </Task>\n"
"</Adventure>\n";

/* Words between "Scrawl:" and " end." */
static std::vector<std::string>
scrawl_words (const char *out)
{
  std::vector<std::string> w;
  const char *s = out ? strstr (out, "Scrawl:") : NULL;
  const char *e = s ? strstr (s, " end.") : NULL;
  if (s == NULL || e == NULL)
    return w;
  std::string seg (s + 7, (size_t) (e - (s + 7)));
  size_t i = 0;
  while (i < seg.size ())
    {
      while (i < seg.size () && seg[i] == ' ') i++;
      size_t j = i;
      while (j < seg.size () && seg[j] != ' ') j++;
      if (j > i) w.push_back (seg.substr (i, j - i));
      i = j;
    }
  return w;
}

static void
check (const char *what, const std::vector<std::string> &w, const char *out)
{
  if (w.size () != 3)
    {
      failures++;
      printf ("FAIL: %s -> expected 3 scrawl words, got %zu in [%s]\n",
              what, w.size (), out ? out : "(null)");
    }
}

/* Run one command after reseeding; returns its output and the RNG draws it cost. */
static char *
drive (a5_run_t *run, unsigned seed, const char *cmd, long *draws)
{
  a5rand_seed (seed);
  long before = a5rand_draw_count;
  char *out = a5run_input (run, cmd);
  *draws = a5rand_draw_count - before;
  return out;
}

int
main (void)
{
  for (unsigned seed = 1; seed <= 40; seed++)
    {
      a5_adventure_t *adv = a5_test_build_adventure (kXml, "a5_exprdup_test");
      if (adv == NULL)
        return 1;
      a5_run_t *run = a5run_new (adv);
      free (a5run_intro (run));
      long d1, d2, d3, d4;

      /* Room1 description: A A B via the inline path. */
      char *o1 = drive (run, seed, "look", &d1);
      std::vector<std::string> w1 = scrawl_words (o1);
      check ("look Room1", w1, o1);

      /* AggregateOutput completions: A A B / A A' B via the deferred draw path. */
      char *o2 = drive (run, seed, "scrounge", &d2);
      std::vector<std::string> w2 = scrawl_words (o2);
      check ("scrounge", w2, o2);
      char *o4 = drive (run, seed, "rummage", &d4);
      std::vector<std::string> w4 = scrawl_words (o4);
      check ("rummage", w4, o4);

      /* Room2: A A' B -- independent second draw, same stream consumption. */
      free (a5run_input (run, "yard"));
      char *o3 = drive (run, seed, "look", &d3);
      std::vector<std::string> w3 = scrawl_words (o3);
      check ("look Room2", w3, o3);

      if (w1.size () == 3 && w2.size () == 3 && w3.size () == 3 && w4.size () == 3)
        {
          if (w1[0] != w1[1])
            { failures++; printf ("FAIL: seed %u Room1 repeat shows a different value: %s %s\n", seed, w1[0].c_str (), w1[1].c_str ()); }
          if (w2[0] != w2[1])
            { failures++; printf ("FAIL: seed %u deferred repeat shows a different value: %s %s\n", seed, w2[0].c_str (), w2[1].c_str ()); }
          /* The repeat must still DRAW: an A A B block costs exactly what an
             A A' B block costs on the same path (the runner calls
             EvaluateExpression per match, so the stream stays aligned). */
          if (d1 != d3)
            { failures++; printf ("FAIL: seed %u inline repeat draw count %ld != independent %ld\n", seed, d1, d3); }
          if (d2 != d4)
            { failures++; printf ("FAIL: seed %u deferred repeat draw count %ld != independent %ld\n", seed, d2, d4); }
          if (d1 < 3 || d2 < 3)
            { failures++; printf ("FAIL: seed %u too few draws (inline %ld, deferred %ld)\n", seed, d1, d2); }
        }
      free (o1); free (o2); free (o3); free (o4);
      a5run_free (run);
    }
  if (failures)
    printf ("a5_exprdup_test: %d FAILURE(S)\n", failures);
  else
    printf ("a5_exprdup_test: all checks passed (40 seeds, inline + deferred paths)\n");
  return failures ? 1 : 0;
}
