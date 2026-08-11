/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- shared fixtures for the self-contained
 * a5_* unit tests (a5_disambig_test, a5_walk_test, a5_objects_test,
 * a5_save_test).
 *
 * The tests build tiny adventures as in-memory XML; the fragments below are
 * the pieces every such adventure repeats.  They are macros over string
 * literals so the game XML stays a single compile-time constant, and the
 * room argument must itself be a string literal.
 */

#ifndef A5_TEST_FIXTURES_H
#define A5_TEST_FIXTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../adrift5/a5model.h"

/* The standard second-person Player character, starting at `room`. */
#define A5_XML_PLAYER_AT(room) \
"  <Character>\n" \
"    <Key>Player</Key>\n" \
"    <Name>Anonymous</Name>\n" \
"    <Type>Player</Type>\n" \
"    <Perspective>SecondPerson</Perspective>\n" \
"    <Property><Key>CharacterLocation</Key><Value>At Location</Value></Property>\n" \
"    <Property><Key>CharacterAtLocation</Key><Value>" room "</Value></Property>\n" \
"  </Character>\n"

/* The property triple placing a dynamic object on the floor of `room`. */
#define A5_XML_DYNAMIC_IN(room) \
"    <Property><Key>StaticOrDynamic</Key><Value>Dynamic</Value></Property>\n" \
"    <Property><Key>DynamicLocation</Key><Value>In Location</Value></Property>\n" \
"    <Property><Key>InLocation</Key><Value>" room "</Value></Property>\n"

/* Parse `xml` and build the adventure model, printing a "<tag>: ... failed"
   diagnostic and returning NULL on error.  The document (and the buffer it
   annotates) become owned by the returned adventure. */
static a5_adventure_t *
a5_test_build_adventure (const char *xml, const char *tag)
{
  uint32_t len = (uint32_t) strlen (xml);
  char *buf = (char *) malloc (len + 1);
  memcpy (buf, xml, len + 1);

  a5_xml_doc_t *doc = a5xml_parse (buf, len);
  if (doc == NULL)
    {
      printf ("%s: XML parse failed\n", tag);
      return NULL;
    }
  a5_adventure_t *adv = a5model_from_doc (doc);
  if (adv == NULL)
    printf ("%s: model build failed\n", tag);
  return adv;
}

#endif /* A5_TEST_FIXTURES_H */
