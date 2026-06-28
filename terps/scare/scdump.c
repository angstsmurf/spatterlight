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
 *
 *   SC_DUMP_OBJLOC  one-shot, isolated per-object dump: starting position
 *                   (gs_object_position code: >=1 room+1, 0 held, -1 hidden,
 *                   -10/-20 in/on object, -100/-200/-300 worn-player/held-NPC/
 *                   worn-NPC), the room it is directly in (or -1), and its
 *                   Battle weapon/armour properties (HitValue, ProtectionValue,
 *                   Method).  Deliberately skips the heavy task/exit/walk
 *                   sections, so it is safe on games whose full SC_DUMP_TASKS
 *                   output is pathologically large.  Built for combat-
 *                   survivability route planning (which armour/weapon is where).
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

  /* SC_DUMP_OBJLOC: minimal, isolated object-location + battle-property dump
   * (does NOT touch the heavy task/exit/walk sections, so it is safe on games
   * whose full dump is large). */
  if (getenv ("SC_DUMP_OBJLOC") && !dumped)
    {
      dumped = TRUE;
      for (i = 0; i < gs_object_count (game); i++)
        {
          sc_vartype_t ok[4], bv;
          sc_int pos, r, room = -1, hit = 0, prot = 0, method = 0;
          const sc_char *s = scdump_object_name (game, i);
          pos = gs_object_position (game, i);
          for (r = 0; r < gs_room_count (game); r++)
            if (obj_directly_in_room (game, i, r)) { room = r; break; }
          ok[0].string = "Objects"; ok[1].integer = i; ok[2].string = "Battle";
          ok[3].string = "HitValue";        if (prop_get (bundle, "I<-siss", &bv, ok)) hit = bv.integer;
          ok[3].string = "ProtectionValue"; if (prop_get (bundle, "I<-siss", &bv, ok)) prot = bv.integer;
          ok[3].string = "Method";          if (prop_get (bundle, "I<-siss", &bv, ok)) method = bv.integer;
          fprintf (stderr, "OBJLOC obj=%ld pos=%ld room=%ld hit=%ld prot=%ld method=%ld [%s]\n",
                   i, pos, room, hit, prot, method, s ? s : "");
        }
      return;
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

  /* Static-object room membership (the "Where" list). Dynamic objects are
   * located via the debugger; statics have no single position, so list every
   * room each static is directly present in -- the only way to pin a locked
   * chest / fixed scenery to a room. */
  for (i = 0; i < gs_object_count (game); i++)
    {
      sc_vartype_t sk[3];
      sc_int r;
      const sc_char *s;
      sk[0].string = "Objects";
      sk[1].integer = i;
      sk[2].string = "Static";
      if (!prop_get_boolean (bundle, "B<-sis", sk))
        continue;
      s = scdump_object_name (game, i);
      fprintf (stderr, "STATIC obj=%ld [%s] rooms=", i, s ? s : "");
      for (r = 0; r < gs_room_count (game); r++)
        if (obj_directly_in_room (game, i, r))
          fprintf (stderr, "%ld ", r);
      fprintf (stderr, "\n");
    }

  /* Every object's Short name (for spotting ambiguous shared names that the
   * library bare-noun get/drop retry guard cares about). */
  for (i = 0; i < gs_object_count (game); i++)
    {
      const sc_char *s = scdump_object_name (game, i);
      fprintf (stderr, "OBJNAME obj=%ld [%s]\n", i, s ? s : "");
    }

  /* Lockable objects: the "Key" property is the *dynamic-object index* of the
   * key that opens it; obj_dynamic_object() maps it to a real object id. */
  for (i = 0; i < gs_object_count (game); i++)
    {
      sc_vartype_t kk[3];
      sc_int key_index, the_key, openable;
      kk[0].string = "Objects";
      kk[1].integer = i;
      kk[2].string = "Openable";
      openable = prop_get_integer (bundle, "I<-sis", kk);
      if (openable <= 0)
        continue;
      kk[2].string = "Key";
      key_index = prop_get_integer (bundle, "I<-sis", kk);
      if (key_index < 0)
        continue;
      the_key = obj_dynamic_object (game, key_index);
      fprintf (stderr, "LOCKKEY obj=%ld [%s] keyidx=%ld keyobj=%ld [%s]\n",
               i, scdump_object_name (game, i) ? scdump_object_name (game, i) : "",
               key_index, the_key,
               (the_key >= 0 && the_key < gs_object_count (game))
                 ? (scdump_object_name (game, the_key) ? scdump_object_name (game, the_key) : "")
                 : "?");
    }

  /* Surface / container index enumeration (chisel-of-the-ages hunt). */
  {
    sc_int si = 0, ci = 0;
    for (i = 0; i < gs_object_count (game); i++)
      {
        const sc_char *s = scdump_object_name (game, i);
        if (obj_is_surface (game, i))
          fprintf (stderr, "SURFACE idx=%ld obj=%ld [%s]\n", si++, i, s ? s : "");
        if (obj_is_container (game, i))
          fprintf (stderr, "CONTAINERIDX idx=%ld obj=%ld [%s]\n", ci++, i, s ? s : "");
      }
  }

  /* Synonyms (input-rewrite rules applied before task/library matching). */
  {
    sc_vartype_t yk[3];
    sc_int yc, yi;
    yk[0].string = "Synonyms";
    yc = prop_get_child_count (bundle, "I<-s", yk);
    for (yi = 0; yi < yc; yi++)
      {
        const sc_char *orig, *repl;
        yk[1].integer = yi;
        yk[2].string = "Original";
        orig = prop_get_string (bundle, "S<-sis", yk);
        yk[2].string = "Replacement";
        repl = prop_get_string (bundle, "S<-sis", yk);
        fprintf (stderr, "SYNONYM [%s] -> [%s]\n",
                 orig ? orig : "", repl ? repl : "");
      }
  }

  /* Tasks: command, Where, Repeatable, restrictions, actions. */
  for (t = 0; t < gs_task_count (game); t++)
    {
      sc_vartype_t k[5], vt;
      const sc_char *cmd;
      sc_int wtype, wroom, acount, rcount, rep;

      sc_int ccount, ci;
      k[0].string = "Tasks";
      k[1].integer = t;
      k[2].string = "Command";
      k[3].integer = 0;
      cmd = prop_get_string (bundle, "S<-sisi", k);
      ccount = prop_get_child_count (bundle, "I<-sis", k);

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

      /* Built-in author hints attached to this task (Question/Hint1=subtle/
       * Hint2=sledgehammer) -- the game's own context-sensitive walkthrough. */
      {
        const sc_char *hq = NULL, *h1 = NULL, *h2 = NULL;
        sc_vartype_t hv;
        k[2].string = "Question"; if (prop_get (bundle, "S<-sis", &hv, k)) hq = hv.string;
        k[2].string = "Hint1";    if (prop_get (bundle, "S<-sis", &hv, k)) h1 = hv.string;
        k[2].string = "Hint2";    if (prop_get (bundle, "S<-sis", &hv, k)) h2 = hv.string;
        if (hq && *hq) fprintf (stderr, "    HINTQ=[%s]\n", hq);
        if (h1 && *h1) fprintf (stderr, "    HINT1=[%s]\n", h1);
        if (h2 && *h2) fprintf (stderr, "    HINT2=[%s]\n", h2);
      }

      /* Print any extra command alternatives (Command[1..]) -- these are the
       * synonym/wildcard forms ADRIFT matches in addition to Command[0]. */
      for (ci = 1; ci < ccount; ci++)
        {
          const sc_char *ac;
          k[2].string = "Command";
          k[3].integer = ci;
          ac = prop_get_string (bundle, "S<-sisi", k);
          fprintf (stderr, "    ALTCMD[%ld]=[%s]\n", ci, ac ? ac : "");
        }

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
          else if (atype == 2)
            {
              /*
               * Object-status ACTIONS index the stateful-object list 0-based
               * (sctasks.c task_run_change_object_status: obj_stateful_object
               * (var1)), whereas the matching type-1 RESTRICTION is 1-based
               * (var1-1, with 0 = "the referenced object"; screstrs.c).  This
               * is a genuine ADRIFT schema asymmetry, not a bug.  Pass v1+1 so
               * the resolver's -1 cancels and we label the object the runtime
               * actually changes (e.g. Space Boy's task 44 "unlock door" sets
               * stateful obj 18 = the Room Door, not obj 17).
               */
              obj = scdump_resolve_object (game, 1, v1 + 1);
            }
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
        sc_int st, tn, ta, tf, o2, o2d, o3, o3d, t1, t2, pf;
        sc_vartype_t evt;
        const sc_char *en;

        st = tn = ta = tf = o2 = o2d = o3 = o3d = t1 = t2 = pf = 0;
        en = NULL;
        ek[1].integer = e;
        /* Use tolerant prop_get -- not every game defines every field. */
        ek[2].string = "Short";        if (prop_get (bundle, "S<-sis", &evt, ek)) en = evt.string;
        ek[2].string = "StarterType";  if (prop_get (bundle, "I<-sis", &evt, ek)) st = evt.integer;
        ek[2].string = "TaskNum";      if (prop_get (bundle, "I<-sis", &evt, ek)) tn = evt.integer;
        ek[2].string = "TaskAffected"; if (prop_get (bundle, "I<-sis", &evt, ek)) ta = evt.integer;
        ek[2].string = "TaskFinished"; if (prop_get (bundle, "I<-sis", &evt, ek)) tf = evt.integer;
        ek[2].string = "Time1";        if (prop_get (bundle, "I<-sis", &evt, ek)) t1 = evt.integer;
        ek[2].string = "Time2";        if (prop_get (bundle, "I<-sis", &evt, ek)) t2 = evt.integer;
        ek[2].string = "PauseTask";    if (prop_get (bundle, "I<-sis", &evt, ek)) pf = evt.integer;
        ek[2].string = "Obj2";         if (prop_get (bundle, "I<-sis", &evt, ek)) o2 = evt.integer;
        ek[2].string = "Obj2Dest";     if (prop_get (bundle, "I<-sis", &evt, ek)) o2d = evt.integer;
        ek[2].string = "Obj3";         if (prop_get (bundle, "I<-sis", &evt, ek)) o3 = evt.integer;
        ek[2].string = "Obj3Dest";     if (prop_get (bundle, "I<-sis", &evt, ek)) o3d = evt.integer;
        fprintf (stderr,
                 "EVENT %ld [%s] starter=%ld startTask=%ld affTask=%ld(fin=%ld)"
                 " time1=%ld time2=%ld pauseTask=%ld o2=%ld->%ld o3=%ld->%ld\n",
                 e, en ? en : "", st, tn, ta, tf, t1, t2, pf, o2, o2d, o3, o3d);
      }
  }

  /* NPCs and their walks (StartTask/CharTask/MeetChar/ObjectTask/Rooms). */
  {
    sc_vartype_t nk[6];
    sc_int n, ncount;

    nk[0].string = "NPCs";
    ncount = prop_get_child_count (bundle, "I<-s", nk);
    for (n = 0; n < ncount; n++)
      {
        sc_int sr = 0, w, wcount;
        const sc_char *nm = NULL;
        sc_vartype_t nv;
        nk[1].integer = n;
        nk[2].string = "Name";      if (prop_get (bundle, "S<-sis", &nv, nk)) nm = nv.string;
        nk[2].string = "StartRoom"; if (prop_get (bundle, "I<-sis", &nv, nk)) sr = nv.integer;
        fprintf (stderr, "NPC %ld [%s] startRoom=%ld\n", n, nm ? nm : "", sr - 1);

        nk[2].string = "Walks";
        wcount = prop_get_child_count (bundle, "I<-sis", nk);
        for (w = 0; w < wcount; w++)
          {
            sc_int loop = 0, st = 0, ct = 0, mo = 0, ot = 0, sp = 0, mc = 0;
            sc_int rc, s;
            nk[3].integer = w;
            nk[4].string = "BLoop";       if (prop_get (bundle, "B<-sisis", &nv, nk)) loop = nv.integer;
            nk[4].string = "StartTask";   if (prop_get (bundle, "I<-sisis", &nv, nk)) st = nv.integer;
            nk[4].string = "CharTask";    if (prop_get (bundle, "I<-sisis", &nv, nk)) ct = nv.integer;
            nk[4].string = "MeetObject";  if (prop_get (bundle, "I<-sisis", &nv, nk)) mo = nv.integer;
            nk[4].string = "ObjectTask";  if (prop_get (bundle, "I<-sisis", &nv, nk)) ot = nv.integer;
            nk[4].string = "StoppingTask";if (prop_get (bundle, "I<-sisis", &nv, nk)) sp = nv.integer;
            nk[4].string = "MeetChar";    if (prop_get (bundle, "I<-sisis", &nv, nk)) mc = nv.integer;
            fprintf (stderr,
                     "  WALK %ld loop=%ld startTask=%ld charTask=%ld(task%ld)"
                     " meetChar=%ld meetObj=%ld objTask=%ld(task%ld) stopTask=%ld\n",
                     w, loop, st, ct, ct - 1, mc, mo, ot, ot - 1, sp);
            nk[4].string = "Rooms";
            rc = prop_get_child_count (bundle, "I<-sisis", nk);
            for (s = 0; s < rc; s++)
              {
                sc_int rm = 0;
                nk[5].integer = s;
                if (prop_get (bundle, "I<-sisisi", &nv, nk)) rm = nv.integer;
                fprintf (stderr, "    step %ld dest=%ld\n", s, rm);
              }
          }
      }
  }

  /* Room exits + their task gates (dir order N,E,S,W,U,D,IN,OUT,NE,NW,SE,SW). */
  {
    sc_vartype_t rk[5];
    sc_int r, rcount;
    static const char *dirs[] =
      { "N","E","S","W","U","D","IN","OUT","NE","NW","SE","SW" };

    rk[0].string = "Rooms";
    rcount = prop_get_child_count (bundle, "I<-s", rk);
    for (r = 0; r < rcount; r++)
      {
        sc_int d;
        for (d = 0; d < 12; d++)
          {
            sc_int dest = 0, v1 = 0, v2 = 0, v3 = 0;
            sc_vartype_t rv;
            rk[1].integer = r;
            rk[2].string = "Exits";
            rk[3].integer = d;
            rk[4].string = "Dest"; if (prop_get (bundle, "I<-sisis", &rv, rk)) dest = rv.integer; else continue;
            rk[4].string = "Var1"; if (prop_get (bundle, "I<-sisis", &rv, rk)) v1 = rv.integer;
            rk[4].string = "Var2"; if (prop_get (bundle, "I<-sisis", &rv, rk)) v2 = rv.integer;
            rk[4].string = "Var3"; if (prop_get (bundle, "I<-sisis", &rv, rk)) v3 = rv.integer;
            if (dest > 0)
              fprintf (stderr, "EXIT room=%ld %s -> dest=%ld gateTask=%ld expectDone=%ld v3=%ld\n",
                       r, dirs[d], dest - 1, v1 - 1, v2, v3);
          }
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

  /* SC_TRACE_PLAYER: just the player's room each turn (maze-mapping aid). */
  if (getenv ("SC_TRACE_PLAYER"))
    {
      fprintf (stderr, "PLAYERROOM room=%ld\n", gs_playerroom (game));
      fflush (stderr);
    }

  if (!getenv ("SC_TRACE_JUDY"))
    return;
  for (npc = 0; npc < gs_npc_count (game); npc++)
    fprintf (stderr, "JUDYTRACE npc=%ld room=%ld\n",
             npc, gs_npc_location (game, npc) - 1);
  fflush (stderr);
}

#endif /* SCARE_DUMP_TOOLS */
