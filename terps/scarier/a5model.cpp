/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- in-memory object model.  See a5model.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5blorb.h"
#include "a5deobf.h"
#include "a5model.h"

/* ------------------------------------------------------------------ helpers */

/* Collect the text of every direct child named `tag` into a new const char*[]. */
static const char **
a5_collect_text (const a5_xml_node_t *node, const char *tag, int *out_n)
{
  int n = a5xml_count (node, tag);
  const char **arr;
  a5_xml_node_t *c;
  int i = 0;

  *out_n = n;
  if (n == 0)
    return NULL;
  arr = (const char **) malloc ((size_t) n * sizeof *arr);
  for (c = node->first_child; c != NULL; c = c->next)
    if (strcmp (c->name, tag) == 0)
      arr[i++] = c->text;
  return arr;
}

/* Collect an entity's <Property> children into an a5_prop_t array. */
static a5_prop_t *
a5_collect_props (const a5_xml_node_t *node, int *out_n)
{
  int n = a5xml_count (node, "Property");
  a5_prop_t *arr;
  a5_xml_node_t *c;
  int i = 0;

  *out_n = n;
  if (n == 0)
    return NULL;
  arr = (a5_prop_t *) calloc ((size_t) n, sizeof *arr);
  for (c = node->first_child; c != NULL; c = c->next)
    {
      a5_xml_node_t *value;
      if (strcmp (c->name, "Property") != 0)
        continue;
      arr[i].key = a5xml_child_text (c, "Key");
      value = a5xml_child (c, "Value");
      if (value != NULL)
        {
          if (value->n_children > 0)
            arr[i].value_node = value;   /* rich (Description) value */
          else
            arr[i].value = value->text;  /* simple scalar */
        }
      /* else: flag property (both NULL) */
      i++;
    }
  return arr;
}

const a5_prop_t *
a5_prop_find (const a5_prop_t *props, int n, const char *key)
{
  int i;
  for (i = 0; i < n; i++)
    if (props[i].key != NULL && strcmp (props[i].key, key) == 0)
      return &props[i];
  return NULL;
}

/* ----------------------------------------------------------- per-type loads */

static void
a5_load_objects (a5_adventure_t *a)
{
  a5_xml_node_t *c;
  int i = 0;
  a->n_objects = a5xml_count (a->root, "Object");
  if (a->n_objects == 0)
    return;
  a->objects = (a5_object_t *) calloc ((size_t) a->n_objects, sizeof *a->objects);
  for (c = a->root->first_child; c != NULL; c = c->next)
    {
      a5_object_t *o;
      if (strcmp (c->name, "Object") != 0)
        continue;
      o = &a->objects[i++];
      o->node = c;
      o->key = a5xml_child_text (c, "Key");
      o->article = a5xml_child_text (c, "Article");
      o->prefix = a5xml_child_text (c, "Prefix");
      o->names = a5_collect_text (c, "Name", &o->n_names);
      o->props = a5_collect_props (c, &o->n_props);
    }
}

static void
a5_load_locations (a5_adventure_t *a)
{
  a5_xml_node_t *c;
  int i = 0;
  a->n_locations = a5xml_count (a->root, "Location");
  if (a->n_locations == 0)
    return;
  a->locations = (a5_location_t *) calloc ((size_t) a->n_locations, sizeof *a->locations);
  for (c = a->root->first_child; c != NULL; c = c->next)
    {
      a5_location_t *l;
      if (strcmp (c->name, "Location") != 0)
        continue;
      l = &a->locations[i++];
      l->node = c;
      l->key = a5xml_child_text (c, "Key");
      l->props = a5_collect_props (c, &l->n_props);
    }
}

static void
a5_load_characters (a5_adventure_t *a)
{
  a5_xml_node_t *c;
  int i = 0;
  a->n_characters = a5xml_count (a->root, "Character");
  if (a->n_characters == 0)
    return;
  a->characters = (a5_character_t *) calloc ((size_t) a->n_characters, sizeof *a->characters);
  for (c = a->root->first_child; c != NULL; c = c->next)
    {
      a5_character_t *ch;
      if (strcmp (c->name, "Character") != 0)
        continue;
      ch = &a->characters[i++];
      ch->node = c;
      ch->key = a5xml_child_text (c, "Key");
      ch->name = a5xml_child_text (c, "Name");
      ch->article = a5xml_child_text (c, "Article");
      ch->prefix = a5xml_child_text (c, "Prefix");
      ch->type = a5xml_child_text (c, "Type");
      ch->perspective = a5xml_child_text (c, "Perspective");
      ch->descriptors = a5_collect_text (c, "Descriptor", &ch->n_descriptors);
      ch->props = a5_collect_props (c, &ch->n_props);
    }
}

static void
a5_load_tasks (a5_adventure_t *a)
{
  a5_xml_node_t *c;
  int i = 0;
  a->n_tasks = a5xml_count (a->root, "Task");
  if (a->n_tasks == 0)
    return;
  a->tasks = (a5_task_t *) calloc ((size_t) a->n_tasks, sizeof *a->tasks);
  for (c = a->root->first_child; c != NULL; c = c->next)
    {
      a5_task_t *t;
      const char *prio, *rep;
      if (strcmp (c->name, "Task") != 0)
        continue;
      t = &a->tasks[i++];
      t->node = c;
      t->key = a5xml_child_text (c, "Key");
      prio = a5xml_child_text (c, "Priority");
      t->priority = (prio != NULL) ? strtol (prio, NULL, 10) : 0;
      t->type = a5xml_child_text (c, "Type");
      t->commands = a5_collect_text (c, "Command", &t->n_commands);
      rep = a5xml_child_text (c, "Repeatable");
      t->repeatable = (rep != NULL && strcmp (rep, "1") == 0);
      t->restrictions = a5xml_child (c, "Restrictions");
      t->actions = a5xml_child (c, "Actions");

      /* Specific-override linkage. */
      t->general_key = a5xml_child_text (c, "GeneralTask");
      t->override_type = a5xml_child_text (c, "SpecificOverrideType");
      t->n_specifics = a5xml_count (c, "Specific");
      if (t->n_specifics > 0)
        {
          a5_xml_node_t *s;
          int si = 0;
          t->specifics = (a5_specific_t *) calloc ((size_t) t->n_specifics,
                                                   sizeof *t->specifics);
          for (s = c->first_child; s != NULL; s = s->next)
            {
              if (strcmp (s->name, "Specific") != 0)
                continue;
              t->specifics[si].type = a5xml_child_text (s, "Type");
              t->specifics[si].keys = a5_collect_text (s, "Key",
                                                       &t->specifics[si].n_keys);
              si++;
            }
        }
    }
}

static void
a5_load_variables (a5_adventure_t *a)
{
  a5_xml_node_t *c;
  int i = 0;
  a->n_variables = a5xml_count (a->root, "Variable");
  if (a->n_variables == 0)
    return;
  a->variables = (a5_variable_t *) calloc ((size_t) a->n_variables, sizeof *a->variables);
  for (c = a->root->first_child; c != NULL; c = c->next)
    {
      a5_variable_t *v;
      if (strcmp (c->name, "Variable") != 0)
        continue;
      v = &a->variables[i++];
      v->node = c;
      v->key = a5xml_child_text (c, "Key");
      v->name = a5xml_child_text (c, "Name");
      v->type = a5xml_child_text (c, "Type");
      v->initial = a5xml_child_text (c, "InitialValue");
    }
}

/* Parse "1" or "1 To 5" (a FromTo range) into [*from, *to]. */
static void
a5_parse_range (const char *s, long *from, long *to)
{
  char *end;
  if (s == NULL) { *from = *to = 0; return; }
  *from = strtol (s, &end, 10);
  while (*end != '\0' && (*end < '0' || *end > '9') && *end != '-') end++;
  *to = (*end != '\0') ? strtol (end, NULL, 10) : *from;
}

/* The last whitespace-delimited token of `s` (an enum name), or "". */
static const char *
a5_last_token (const char *s)
{
  const char *p, *last = s ? s : "";
  for (p = last; p != NULL && *p; p++)
    if (*p == ' ' || *p == '\t')
      last = p + 1;
  return last;
}

static a5_se_when_t
a5_parse_se_when (const char *s)
{
  if (s != NULL && strcmp (s, "FromStartOfEvent") == 0) return A5_SE_FROM_START;
  if (s != NULL && strcmp (s, "BeforeEndOfEvent") == 0) return A5_SE_BEFORE_END;
  return A5_SE_FROM_LAST;
}

static a5_se_what_t
a5_parse_se_what (const char *s)
{
  if (s != NULL && strcmp (s, "SetLook") == 0)    return A5_SE_SETLOOK;
  if (s != NULL && strcmp (s, "ExecuteTask") == 0) return A5_SE_EXECTASK;
  if (s != NULL && strcmp (s, "UnsetTask") == 0)  return A5_SE_UNSETTASK;
  return A5_SE_DISPLAY;
}

static a5_ctrl_t
a5_parse_ctrl (const char *s)
{
  if (s != NULL && strcmp (s, "Stop") == 0)    return A5_CTRL_STOP;
  if (s != NULL && strcmp (s, "Suspend") == 0) return A5_CTRL_SUSPEND;
  if (s != NULL && strcmp (s, "Resume") == 0)  return A5_CTRL_RESUME;
  return A5_CTRL_START;
}

/* Decode the parsed-enum / range fields of one <Event> (clsEvent FileIO load). */
static void
a5_parse_event_body (a5_event_t *e, const a5_xml_node_t *c)
{
  const char *ws = a5xml_child_text (c, "WhenStart");
  const char *rep = a5xml_child_text (c, "Repeating");
  const char *rc = a5xml_child_text (c, "RepeatCountdown");
  a5_xml_node_t *ch;
  int si = 0, ki = 0;

  e->when_start = 1;                                  /* Immediately default */
  if (ws != NULL && strcmp (ws, "BetweenXandYTurns") == 0) e->when_start = 2;
  else if (ws != NULL && strcmp (ws, "AfterATask") == 0)   e->when_start = 3;
  a5_parse_range (a5xml_child_text (c, "StartDelay"), &e->start_from, &e->start_to);
  a5_parse_range (a5xml_child_text (c, "Length"),     &e->length_from, &e->length_to);
  e->repeating       = (rep != NULL && strcmp (rep, "1") == 0);
  e->repeat_countdown = (rc != NULL && strcmp (rc, "1") == 0);

  e->n_controls  = a5xml_count (c, "Control");
  e->n_subevents = a5xml_count (c, "SubEvent");
  if (e->n_controls > 0)
    e->controls = (a5_eventctrl_t *) calloc ((size_t) e->n_controls,
                                             sizeof *e->controls);
  if (e->n_subevents > 0)
    e->subevents = (a5_subevent_t *) calloc ((size_t) e->n_subevents,
                                             sizeof *e->subevents);

  for (ch = c->first_child; ch != NULL; ch = ch->next)
    {
      if (strcmp (ch->name, "Control") == 0)
        {
          /* "Start Completion Gamestart" -> control, completion flag, key. */
          const char *t = ch->text ? ch->text : "";
          char buf[64]; int n = 0;
          a5_eventctrl_t *cc = &e->controls[ki++];
          const char *p = t, *toks[3]; int nt = 0;
          /* split into up to 3 tokens by spaces, aliasing where possible */
          while (*p == ' ') p++;
          while (*p && nt < 3)
            {
              toks[nt++] = p;
              while (*p && *p != ' ') p++;
              while (*p == ' ') p++;
            }
          /* control enum (token 0), completion (token 1), task key (token 2) */
          if (nt >= 1)
            {
              n = 0;
              while (toks[0][n] && toks[0][n] != ' ' && n < 63) { buf[n] = toks[0][n]; n++; }
              buf[n] = '\0';
              cc->control = a5_parse_ctrl (buf);
            }
          cc->on_completion = 1;
          if (nt >= 2)
            cc->on_completion = (strncmp (toks[1], "UnCompletion", 12) != 0);
          /* The task key is the final token, which runs to the NUL-terminated
             end of the <Control> text, so it can alias directly. */
          if (nt >= 3)
            cc->task_key = toks[2];
        }
      else if (strcmp (ch->name, "SubEvent") == 0)
        {
          a5_subevent_t *se = &e->subevents[si++];
          const char *when = a5xml_child_text (ch, "When");
          a5_xml_node_t *act = a5xml_child (ch, "Action");
          a5_parse_range (when, &se->ft_from, &se->ft_to);
          se->when = a5_parse_se_when (a5_last_token (when));
          if (act != NULL)
            {
              if (a5xml_child (act, "Description") != NULL)
                { se->what = A5_SE_DISPLAY; se->description = act; }
              else
                {
                  /* "ExecuteTask Roller2" -> what, key. */
                  const char *txt = act->text ? act->text : "";
                  const char *sp = strchr (txt, ' ');
                  char wbuf[32]; int n = 0;
                  while (txt[n] && txt[n] != ' ' && n < 31) { wbuf[n] = txt[n]; n++; }
                  wbuf[n] = '\0';
                  se->what = a5_parse_se_what (wbuf);
                  /* The key is the final token (NUL-terminated end of <Action>). */
                  if (sp != NULL)
                    se->key = sp + 1;
                }
            }
        }
    }
}

static void
a5_load_events (a5_adventure_t *a)
{
  a5_xml_node_t *c;
  int i = 0;
  a->n_events = a5xml_count (a->root, "Event");
  if (a->n_events == 0)
    return;
  a->events = (a5_event_t *) calloc ((size_t) a->n_events, sizeof *a->events);
  for (c = a->root->first_child; c != NULL; c = c->next)
    {
      a5_event_t *e;
      if (strcmp (c->name, "Event") != 0)
        continue;
      e = &a->events[i++];
      e->node = c;
      e->key = a5xml_child_text (c, "Key");
      e->type = a5xml_child_text (c, "Type");
      a5_parse_event_body (e, c);
    }
}

static void
a5_load_groups (a5_adventure_t *a)
{
  a5_xml_node_t *c;
  int i = 0;
  a->n_groups = a5xml_count (a->root, "Group");
  if (a->n_groups == 0)
    return;
  a->groups = (a5_group_t *) calloc ((size_t) a->n_groups, sizeof *a->groups);
  for (c = a->root->first_child; c != NULL; c = c->next)
    {
      a5_group_t *g;
      if (strcmp (c->name, "Group") != 0)
        continue;
      g = &a->groups[i++];
      g->node = c;
      g->key = a5xml_child_text (c, "Key");
      g->type = a5xml_child_text (c, "Type");
      g->name = a5xml_child_text (c, "Name");
      g->members = a5_collect_text (c, "Member", &g->n_members);
    }
}

static void
a5_load_propdefs (a5_adventure_t *a)
{
  a5_xml_node_t *c;
  int i = 0;
  a->n_propdefs = a5xml_count (a->root, "Property");
  if (a->n_propdefs == 0)
    return;
  a->propdefs = (a5_propdef_t *) calloc ((size_t) a->n_propdefs, sizeof *a->propdefs);
  for (c = a->root->first_child; c != NULL; c = c->next)
    {
      a5_propdef_t *p;
      if (strcmp (c->name, "Property") != 0)
        continue;
      p = &a->propdefs[i++];
      p->node = c;
      p->key = a5xml_child_text (c, "Key");
      p->type = a5xml_child_text (c, "Type");
      p->property_of = a5xml_child_text (c, "PropertyOf");
      p->dependent_key = a5xml_child_text (c, "DependentKey");
    }
}

static void
a5_load_alrs (a5_adventure_t *a)
{
  a5_xml_node_t *c;
  int i = 0;
  a->n_alrs = a5xml_count (a->root, "TextOverride");
  if (a->n_alrs == 0)
    return;
  a->alrs = (a5_alr_t *) calloc ((size_t) a->n_alrs, sizeof *a->alrs);
  for (c = a->root->first_child; c != NULL; c = c->next)
    {
      a5_alr_t *r;
      if (strcmp (c->name, "TextOverride") != 0)
        continue;
      r = &a->alrs[i++];
      r->node = c;
      r->key = a5xml_child_text (c, "Key");
      r->old_text = a5xml_child_text (c, "OldText");
      r->new_text = a5xml_child (c, "NewText");
    }
}

static void
a5_load_udfs (a5_adventure_t *a)
{
  a5_xml_node_t *c;
  int i = 0;
  a->n_udfs = a5xml_count (a->root, "Function");
  if (a->n_udfs == 0)
    return;
  a->udfs = (a5_udf_t *) calloc ((size_t) a->n_udfs, sizeof *a->udfs);
  for (c = a->root->first_child; c != NULL; c = c->next)
    {
      a5_udf_t *u;
      if (strcmp (c->name, "Function") != 0)
        continue;
      u = &a->udfs[i++];
      u->node = c;
      u->key = a5xml_child_text (c, "Key");
      u->name = a5xml_child_text (c, "Name");
    }
}

/* ------------------------------------------------------------------- public */

a5_adventure_t *
a5model_from_doc (a5_xml_doc_t *doc)
{
  a5_adventure_t *a;
  a5_xml_node_t *root = a5xml_root (doc);

  if (root == NULL || strcmp (root->name, "Adventure") != 0)
    return NULL;

  a = (a5_adventure_t *) calloc (1, sizeof *a);
  if (a == NULL)
    return NULL;
  a->doc = doc;
  a->root = root;
  a->title = a5xml_child_text (root, "Title");
  a->author = a5xml_child_text (root, "Author");
  a->version = a5xml_child_text (root, "Version");
  a->introduction = a5xml_child (root, "Introduction");

  a5_load_propdefs (a);
  a5_load_locations (a);
  a5_load_objects (a);
  a5_load_tasks (a);
  a5_load_events (a);
  a5_load_characters (a);
  a5_load_variables (a);
  a5_load_groups (a);
  a5_load_alrs (a);
  a5_load_udfs (a);
  return a;
}

a5_adventure_t *
a5model_load (const char *path)
{
  FILE *fp;
  long size;
  uint8_t *file_buf, *payload;
  uint32_t file_len, payload_len, header, region_len, xml_len;
  uint8_t *xml;
  a5_blorb_chunk_t chunk;
  a5_xml_doc_t *doc;
  a5_adventure_t *adv;

  fp = fopen (path, "rb");
  if (fp == NULL)
    return NULL;
  fseek (fp, 0, SEEK_END);
  size = ftell (fp);
  fseek (fp, 0, SEEK_SET);
  if (size <= 0)
    {
      fclose (fp);
      return NULL;
    }
  file_buf = (uint8_t *) malloc ((size_t) size);
  if (file_buf == NULL || fread (file_buf, 1, (size_t) size, fp) != (size_t) size)
    {
      free (file_buf);
      fclose (fp);
      return NULL;
    }
  fclose (fp);
  file_len = (uint32_t) size;

  if (a5blorb_find_exec (file_buf, file_len, &chunk))
    {
      payload = (uint8_t *) chunk.data;
      payload_len = chunk.size;
    }
  else
    {
      payload = file_buf;
      payload_len = file_len;
    }

  if (payload_len < 30)
    {
      free (file_buf);
      return NULL;
    }
  header = (payload[12] == '0' && payload[13] == '0'
            && payload[14] == '0' && payload[15] == '0') ? 16 : 12;
  region_len = payload_len - header - 14;

  a5_deobfuscate (payload, header, region_len);
  xml = a5_inflate (payload + header, region_len, &xml_len);
  free (file_buf);
  if (xml == NULL)
    return NULL;

  doc = a5xml_parse ((char *) xml, xml_len);
  if (doc == NULL)
    {
      free (xml);
      return NULL;
    }

  adv = a5model_from_doc (doc);
  if (adv == NULL)
    a5xml_free (doc);
  return adv;
}

void
a5model_free (a5_adventure_t *a)
{
  int i;
  if (a == NULL)
    return;
  for (i = 0; i < a->n_objects; i++)
    {
      free ((void *) a->objects[i].names);
      free (a->objects[i].props);
    }
  for (i = 0; i < a->n_locations; i++)
    free (a->locations[i].props);
  for (i = 0; i < a->n_characters; i++)
    {
      free ((void *) a->characters[i].descriptors);
      free (a->characters[i].props);
    }
  for (i = 0; i < a->n_tasks; i++)
    {
      int s;
      free ((void *) a->tasks[i].commands);
      for (s = 0; s < a->tasks[i].n_specifics; s++)
        free ((void *) a->tasks[i].specifics[s].keys);
      free (a->tasks[i].specifics);
    }
  for (i = 0; i < a->n_events; i++)
    {
      free (a->events[i].subevents);
      free (a->events[i].controls);
    }
  for (i = 0; i < a->n_groups; i++)
    free ((void *) a->groups[i].members);
  free (a->objects);
  free (a->locations);
  free (a->characters);
  free (a->tasks);
  free (a->variables);
  free (a->events);
  free (a->groups);
  free (a->propdefs);
  free (a->alrs);
  free (a->udfs);
  a5xml_free (a->doc);
  free (a);
}

/* ------------------------------------------------------------- key lookups */

const a5_object_t *
a5model_object (const a5_adventure_t *a, const char *key)
{
  int i;
  for (i = 0; i < a->n_objects; i++)
    if (a->objects[i].key != NULL && strcmp (a->objects[i].key, key) == 0)
      return &a->objects[i];
  return NULL;
}

const a5_location_t *
a5model_location (const a5_adventure_t *a, const char *key)
{
  int i;
  for (i = 0; i < a->n_locations; i++)
    if (a->locations[i].key != NULL && strcmp (a->locations[i].key, key) == 0)
      return &a->locations[i];
  return NULL;
}

const a5_character_t *
a5model_character (const a5_adventure_t *a, const char *key)
{
  int i;
  for (i = 0; i < a->n_characters; i++)
    if (a->characters[i].key != NULL && strcmp (a->characters[i].key, key) == 0)
      return &a->characters[i];
  return NULL;
}

const a5_task_t *
a5model_task (const a5_adventure_t *a, const char *key)
{
  int i;
  for (i = 0; i < a->n_tasks; i++)
    if (a->tasks[i].key != NULL && strcmp (a->tasks[i].key, key) == 0)
      return &a->tasks[i];
  return NULL;
}

const a5_variable_t *
a5model_variable (const a5_adventure_t *a, const char *key)
{
  int i;
  for (i = 0; i < a->n_variables; i++)
    if (a->variables[i].key != NULL && strcmp (a->variables[i].key, key) == 0)
      return &a->variables[i];
  return NULL;
}
