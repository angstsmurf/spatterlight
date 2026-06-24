/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * scdump.c -- reusable structural-dump / trace instrumentation for SCARE.
 *
 * This is developer tooling used to reverse-engineer ADRIFT games when
 * deriving deterministic walkthroughs (see terps/scare/adrift-walkthroughs/).
 * It is gated behind the SCARE_DUMP_TOOLS build macro and is compiled ONLY
 * into the headless walkthrough harness (harness/build.sh passes
 * -DSCARE_DUMP_TOOLS).  A normal Spatterlight build never sees this file or
 * the call sites in sctasks.c / scnpcs.c, so there is zero footprint in the
 * shipping interpreter.
 *
 * The three entry points, each driven by an environment variable so a run can
 * opt in without recompiling:
 *
 *   SC_DUMP_TASKS   one-shot structural dump to stderr -- every task's command
 *                   pattern, Where (firing room), Repeatable flag, and each
 *                   restriction and action with its Type and Var1/Var2/Var3,
 *                   plus a container-index table and the full event table.
 *                   Object/task references are resolved to names.  Decoding
 *                   keys (verified against the original 3.90/4.0 Runner):
 *                     - action Type: 0 move-obj, 1 move-char, 2 obj-status,
 *                       3 change-var, 4 ChangeScore (Var1=points), 5 exec/unset,
 *                       6 EndGame (Var1: 0 win, 1 lose, 2 death, 3 stop),
 *                       7 change-battle.  No Type-4 => no score; no Type-6 =>
 *                       no ending.
 *                     - restriction Type: 0 obj-location, 1 obj-state,
 *                       2 task-state (Var1 is 1-based task; Var2 0=done,1=undone),
 *                       3 char, 4 variable (Var1-2 = variable index).
 *                     - object "inside" Var3 is a 1-based container index;
 *                       move-object actions move 1-based room destinations.
 *
 *   SC_TRACE_TASKS  enable SCARE's built-in task + restriction tracing (prints
 *                   the bracket expression and each restriction PASS/FAIL).
 *
 *   SC_TRACE_JUDY   per-turn one-line dump of every NPC's current room, for
 *                   pinning down a wandering NPC's deterministic walk.
 */

#include <stdio.h>
#include <stdlib.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"

#ifdef SCARE_DUMP_TOOLS

/* Resolve a restriction/action object reference to its global object index. */
static sc_int
scdump_resolve_object (sc_gameref_t game, sc_int kind, sc_int var1)
{
  /* kind: 0 = dynamic-object ref (var1>=3 -> dynamic var1-3),
   *       1 = stateful-object ref (var1>=1 -> stateful var1-1). */
  if (kind == 0 && var1 >= 3)
    return obj_dynamic_object (game, var1 - 3);
  if (kind == 1 && var1 >= 1)
    return obj_stateful_object (game, var1 - 1);
  return -1;
}

static const sc_char *
scdump_object_name (sc_gameref_t game, sc_int obj)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t k[3];

  if (obj < 0 || obj >= gs_object_count (game))
    return NULL;
  k[0].string = "Objects";
  k[1].integer = obj;
  k[2].string = "Short";
  return prop_get_string (bundle, "S<-sis", k);
}

/*
 * sc_dump_structure_once()
 *
 * One-shot (guarded by a static flag) structural dump, triggered by the
 * SC_DUMP_TASKS environment variable.  Also honours SC_TRACE_TASKS.
 */
void
sc_dump_structure_once (sc_gameref_t game)
{
  static sc_bool dumped = FALSE;
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_int t, i;

  if (getenv ("SC_TRACE_TASKS"))
    {
      task_debug_trace (TRUE);
      restr_debug_trace (TRUE);
    }

  if (!getenv ("SC_DUMP_TASKS") || dumped)
    return;
  dumped = TRUE;

  /* Container-index table (1-based "inside" Var3 maps through this). */
  for (i = 0; i < 64; i++)
    {
      sc_int oo = obj_container_object (game, i);
      const sc_char *s;
      if (oo < 0 || oo >= gs_object_count (game))
        break;
      s = scdump_object_name (game, oo);
      fprintf (stderr, "CONTAINER idx=%ld obj=%ld [%s]\n", i, oo, s ? s : "");
    }

  /* Tasks: command, Where, Repeatable, restrictions, actions. */
  for (t = 0; t < gs_task_count (game); t++)
    {
      sc_vartype_t k[5], vt;
      const sc_char *cmd;
      sc_int wtype, wroom, acount, rcount, rep;

      k[0].string = "Tasks";
      k[1].integer = t;
      k[2].string = "Command";
      k[3].integer = 0;
      cmd = prop_get_string (bundle, "S<-sisi", k);

      k[2].string = "Where";
      k[3].string = "Type";
      wtype = prop_get_integer (bundle, "I<-siss", k);
      wroom = -1;
      if (wtype == ROOMLIST_ONE_ROOM)
        {
          k[3].string = "Room";
          wroom = prop_get_integer (bundle, "I<-siss", k);
        }

      k[2].string = "Repeatable";
      rep = prop_get_boolean (bundle, "B<-sis", k);
      k[2].string = "Actions";
      acount = prop_get_child_count (bundle, "I<-sis", k);
      k[2].string = "Restrictions";
      rcount = prop_get_child_count (bundle, "I<-sis", k);

      fprintf (stderr,
               "TASK %ld where=%ld room=%ld restr=%ld rep=%ld cmd=[%s]\n",
               t, wtype, wroom, rcount, rep, cmd ? cmd : "");

      for (i = 0; i < rcount; i++)
        {
          sc_int rtype, v1, v2, v3, obj;
          char nm[96];

          nm[0] = '\0';
          k[2].string = "Restrictions";
          k[3].integer = i;
          k[4].string = "Type";
          rtype = prop_get_integer (bundle, "I<-sisis", k);
          v1 = v2 = v3 = -999;
          k[4].string = "Var1";
          if (prop_get (bundle, "I<-sisis", &vt, k)) v1 = vt.integer;
          k[4].string = "Var2";
          if (prop_get (bundle, "I<-sisis", &vt, k)) v2 = vt.integer;
          k[4].string = "Var3";
          if (prop_get (bundle, "I<-sisis", &vt, k)) v3 = vt.integer;

          obj = -1;
          if (rtype == 0) obj = scdump_resolve_object (game, 0, v1);
          else if (rtype == 1) obj = scdump_resolve_object (game, 1, v1);
          if (obj >= 0)
            {
              const sc_char *s = scdump_object_name (game, obj);
              snprintf (nm, sizeof nm, " obj%ld=[%s]", obj, s ? s : "");
            }
          else if (rtype == 2 && v1 >= 1 && v1 - 1 < gs_task_count (game))
            {
              /* Task-state ref is 1-based; resolve to its command. */
              sc_vartype_t tk[4];
              const sc_char *s;
              tk[0].string = "Tasks"; tk[1].integer = v1 - 1;
              tk[2].string = "Command"; tk[3].integer = 0;
              s = prop_get_string (bundle, "S<-sisi", tk);
              snprintf (nm, sizeof nm, " task%ld=[%s]", v1 - 1, s ? s : "");
            }
          fprintf (stderr, "    RESTR type=%ld v1=%ld v2=%ld v3=%ld%s\n",
                   rtype, v1, v2, v3, nm);
        }

      for (i = 0; i < acount; i++)
        {
          sc_int atype, v1, v2, v3, obj;
          char nm[96];

          nm[0] = '\0';
          k[2].string = "Actions";
          k[3].integer = i;
          k[4].string = "Type";
          atype = prop_get_integer (bundle, "I<-sisis", k);
          v1 = v2 = v3 = -999;
          k[4].string = "Var1";
          if (prop_get (bundle, "I<-sisis", &vt, k)) v1 = vt.integer;
          k[4].string = "Var2";
          if (prop_get (bundle, "I<-sisis", &vt, k)) v2 = vt.integer;
          k[4].string = "Var3";
          if (prop_get (bundle, "I<-sisis", &vt, k)) v3 = vt.integer;

          obj = -1;
          if (atype == 0) obj = scdump_resolve_object (game, 0, v1);
          else if (atype == 2) obj = scdump_resolve_object (game, 1, v1);
          if (obj >= 0)
            {
              const sc_char *s = scdump_object_name (game, obj);
              snprintf (nm, sizeof nm, " obj%ld=[%s]", obj, s ? s : "");
            }
          fprintf (stderr, "    ACT type=%ld v1=%ld v2=%ld v3=%ld%s\n",
                   atype, v1, v2, v3, nm);
        }
    }

  /* Events: starter, starter task, affected task, object moves. */
  {
    sc_vartype_t ek[3];
    sc_int e, ecount;

    ek[0].string = "Events";
    ecount = prop_get_child_count (bundle, "I<-s", ek);
    for (e = 0; e < ecount; e++)
      {
        sc_int st, tn, ta, tf, o2, o2d, o3, o3d;
        const sc_char *en;

        ek[1].integer = e;
        ek[2].string = "Short";        en = prop_get_string (bundle, "S<-sis", ek);
        ek[2].string = "StarterType";  st = prop_get_integer (bundle, "I<-sis", ek);
        ek[2].string = "TaskNum";      tn = prop_get_integer (bundle, "I<-sis", ek);
        ek[2].string = "TaskAffected"; ta = prop_get_integer (bundle, "I<-sis", ek);
        ek[2].string = "TaskFinished"; tf = prop_get_integer (bundle, "I<-sis", ek);
        ek[2].string = "Obj2";         o2 = prop_get_integer (bundle, "I<-sis", ek);
        ek[2].string = "Obj2Dest";     o2d = prop_get_integer (bundle, "I<-sis", ek);
        ek[2].string = "Obj3";         o3 = prop_get_integer (bundle, "I<-sis", ek);
        ek[2].string = "Obj3Dest";     o3d = prop_get_integer (bundle, "I<-sis", ek);
        fprintf (stderr,
                 "EVENT %ld [%s] starter=%ld startTask=%ld affTask=%ld(fin=%ld)"
                 " o2=%ld->%ld o3=%ld->%ld\n",
                 e, en ? en : "", st, tn, ta, tf, o2, o2d, o3, o3d);
      }
  }

  fflush (stderr);
}

/*
 * sc_dump_npc_trace()
 *
 * Per-turn one-line dump of every NPC's room, triggered by SC_TRACE_JUDY.
 * Call once per turn (from npc_tick_npcs, after NPCs have moved).
 */
void
sc_dump_npc_trace (sc_gameref_t game)
{
  sc_int npc;

  if (!getenv ("SC_TRACE_JUDY"))
    return;
  for (npc = 0; npc < gs_npc_count (game); npc++)
    fprintf (stderr, "JUDYTRACE npc=%ld room=%ld\n",
             npc, gs_npc_location (game, npc) - 1);
  fflush (stderr);
}

#endif /* SCARE_DUMP_TOOLS */
