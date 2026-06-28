/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Phase 1 harness (A5_DUMP_MODEL).
 *
 * Loads an ADRIFT 5 Blorb/.taf all the way into the object model and prints a
 * structured summary plus a few full records, so the parse + model can be
 * eyeballed against the XML and against frankendrift.
 *
 *   ./a5model_dump <game.blorb>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5model.h"

static const char *
nz (const char *s)
{
  return s != NULL ? s : "(none)";
}

/* Print up to `limit` chars of text on one line, escaping newlines. */
static void
print_snippet (const char *s, int limit)
{
  int i;
  if (s == NULL)
    {
      printf ("(none)");
      return;
    }
  for (i = 0; s[i] != '\0' && i < limit; i++)
    {
      if (s[i] == '\n')
        printf ("\\n");
      else if (s[i] == '\r')
        ;
      else
        putchar (s[i]);
    }
  if (s[i] != '\0')
    printf ("...");
}

static void
dump_props (const a5_prop_t *props, int n)
{
  int i;
  for (i = 0; i < n; i++)
    {
      printf ("      %s", nz (props[i].key));
      if (props[i].value != NULL)
        printf (" = %s", props[i].value);
      else if (props[i].value_node != NULL)
        printf (" = <text>");
      else
        printf (" (flag)");
      printf ("\n");
    }
}

int
main (int argc, char **argv)
{
  a5_adventure_t *a;
  int i;

  if (argc != 2)
    {
      fprintf (stderr, "usage: %s <game.blorb|game.taf>\n", argv[0]);
      return 2;
    }

  a = a5model_load (argv[1]);
  if (a == NULL)
    {
      fprintf (stderr, "a5model_dump: failed to load %s\n", argv[1]);
      return 1;
    }

  printf ("=== Adventure ===\n");
  printf ("Title:   %s\n", nz (a->title));
  printf ("Author:  %s\n", nz (a->author));
  printf ("Version: %s\n", nz (a->version));
  printf ("\nCounts:\n");
  printf ("  Locations   %d\n", a->n_locations);
  printf ("  Objects     %d\n", a->n_objects);
  printf ("  Characters  %d\n", a->n_characters);
  printf ("  Tasks       %d\n", a->n_tasks);
  printf ("  Variables   %d\n", a->n_variables);
  printf ("  Events      %d\n", a->n_events);
  printf ("  Groups      %d\n", a->n_groups);
  printf ("  Properties  %d\n", a->n_propdefs);
  printf ("  ALRs        %d\n", a->n_alrs);
  printf ("  UDFs        %d\n", a->n_udfs);

  printf ("\n=== First 3 Locations ===\n");
  for (i = 0; i < a->n_locations && i < 3; i++)
    {
      const a5_location_t *l = &a->locations[i];
      printf ("  [%s]\n", nz (l->key));
      dump_props (l->props, l->n_props);
    }

  printf ("\n=== First 3 Objects ===\n");
  for (i = 0; i < a->n_objects && i < 3; i++)
    {
      const a5_object_t *o = &a->objects[i];
      int j;
      printf ("  [%s] article='%s' prefix='%s' names=[", nz (o->key),
              nz (o->article), nz (o->prefix));
      for (j = 0; j < o->n_names; j++)
        printf ("%s%s", j ? ", " : "", o->names[j]);
      printf ("]\n");
      dump_props (o->props, o->n_props);
    }

  printf ("\n=== Characters ===\n");
  for (i = 0; i < a->n_characters; i++)
    {
      const a5_character_t *ch = &a->characters[i];
      printf ("  [%s] name='%s' type=%s perspective=%s nprops=%d\n",
              nz (ch->key), nz (ch->name), nz (ch->type),
              nz (ch->perspective), ch->n_props);
    }

  printf ("\n=== First 8 Tasks ===\n");
  for (i = 0; i < a->n_tasks && i < 8; i++)
    {
      const a5_task_t *t = &a->tasks[i];
      int j;
      printf ("  [%s] type=%s prio=%ld rep=%d cmds=[", nz (t->key),
              nz (t->type), t->priority, t->repeatable);
      for (j = 0; j < t->n_commands; j++)
        printf ("%s\"%s\"", j ? ", " : "", t->commands[j]);
      printf ("] restr=%s actions=%s\n",
              t->restrictions ? "y" : "n", t->actions ? "y" : "n");
    }

  printf ("\n=== First 10 Variables ===\n");
  for (i = 0; i < a->n_variables && i < 10; i++)
    {
      const a5_variable_t *v = &a->variables[i];
      printf ("  [%s] name='%s' type=%s initial=%s\n", nz (v->key),
              nz (v->name), nz (v->type), nz (v->initial));
    }

  printf ("\n=== Groups ===\n");
  for (i = 0; i < a->n_groups; i++)
    {
      const a5_group_t *g = &a->groups[i];
      printf ("  [%s] type=%s name='%s' members=%d\n", nz (g->key),
              nz (g->type), nz (g->name), g->n_members);
    }

  printf ("\n=== ALRs (TextOverride) ===\n");
  for (i = 0; i < a->n_alrs; i++)
    {
      printf ("  [%s] old='", nz (a->alrs[i].key));
      print_snippet (a->alrs[i].old_text, 40);
      printf ("' new=%s\n", a->alrs[i].new_text ? "<text>" : "(none)");
    }

  printf ("\n=== Player sanity ===\n");
  {
    const a5_character_t *p = a5model_character (a, "Player");
    if (p != NULL)
      {
        const a5_prop_t *loc = a5_prop_find (p->props, p->n_props,
                                             "CharacterAtLocation");
        printf ("  Player '%s' starts at %s\n", nz (p->name),
                loc ? nz (loc->value) : "(unknown)");
      }
    else
      printf ("  (no Player character?)\n");
  }

  a5model_free (a);
  return 0;
}
