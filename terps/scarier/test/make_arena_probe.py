#!/usr/bin/env python3
"""Parameterized ADRIFT 4.0 arena-probe generator (plain body only).
Derived from terps/scarier/test/make_battle_taf.py; pack with taftool.py."""
import sys

SEP = "\xbd\xd0"

def build(cfg):
    L = []
    def s(x): L.append(str(x))
    def ml(x): L.append(x); L.append(SEP)

    ml(cfg.get('intro', "A synthetic arena probe."))
    s(0)                                    # StartRoom
    ml("You have won.")
    s(cfg['name']); s("SCARE probe"); s("I don't understand.")
    s(2); s(1); s(0); s(1); s(1); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
    s("Player"); s(0); s("A test fighter.")
    s(0); s(0); s(0); s(0); s(100); s(100)  # Task Position ParentObject Gender MaxSize MaxWt
    p = cfg['player']                       # (stam, strLo,strHi, accLo,accHi, defLo,defHi, agiLo,agiHi, recovery)
    s(p[0]); s(p[0])
    s(p[1]); s(p[2]); s(p[3]); s(p[4]); s(p[5]); s(p[6]); s(p[7]); s(p[8])
    s(p[9])                                 # Recovery
    s(0); s(0); s(0); s(0); s(0); s(0); s(0); s(0); s(0)  # compass..graphics
    s(0); s(""); s(0); s(0); s(0)           # StatusBox txt iUnk1 iUnk2 Embedded

    rooms = cfg['rooms']                    # list of (short, long, exits{dir_index: dest_room_0based})
    s(len(rooms))
    for (short, long_, exits) in rooms:
        s(short); s(long_)
        for d in range(8):
            if d in exits:
                s(exits[d] + 1); s(0); s(0); s(0)
            else:
                s(0)
        s(0); s(0)                          # Alts, bHideOnMap

    objs = cfg.get('objects', [])           # (prefix, short, initpos, weapon, prot, hitval, method, acc, wearable
                                            #  [, container])  initpos: 1 held, 4 room 1
    statics = cfg.get('statics', [])        # (prefix, short, wheretype, roomOrParent)
                                            #   wheretype: 1 one-room (arg = room 0-based),
                                            #   3 all-rooms (arg unused), 4 NPC part
                                            #   (arg: 0 = player, N = NPC N 1-based)
    s(len(objs) + len(statics))
    for o in objs:
        (pre, short, pos, wpn, prot, hv, meth, acc, wear) = o[:9]
        cont = o[9] if len(o) > 9 else 0
        parent = o[10] if len(o) > 10 else 0    # pos 2 = inside container `parent`
        s(pre); s(short); s(0); s(0)
        s("A probe object."); s(pos); s(0); s(0); s("")
        s(cont); s(0); s(100 if cont else 0); s(wear); s(2); s(parent)
        s(0); s(0); s(0); s(0)
        s(wpn); s(0); s(0)
        s(prot); s(hv); s(meth); s(acc)
        s(""); s(0)
    for (pre, short, wtype, arg) in statics:
        s(pre); s(short); s(0); s(1)        # Prefix Short Aliases Static=1
        s("A probe static."); s(0); s(0); s(0); s("")  # Desc InitPos Task TaskNotDone AltDesc
        s(wtype)                            # Where.Type
        if wtype == 1:
            s(arg + 1)                      # single room, 1-based
        s(0); s(0); s(0)                    # Container Surface Capacity
        if wtype == 4:
            s(arg)                          # Parent: 0 = player, N = NPC N
        s(0); s(0); s(0)                    # Openable SitLie Readable
        s(0); s(0)                          # CurrentState ListFlag
        s(0); s(0); s(0); s(0)              # OBJ_BATTLE prot/hit/method/acc
        s(""); s(0)                         # InRoomDesc OnlyWhenNotMoved

    tasks = cfg.get('tasks', [])            # dicts: commands=[...], complete="",
                                            #   restrs=[(var1,var2,var3,failmsg)] (type-0
                                            #   object-location), repeatable=1
    s(len(tasks))
    for t in tasks:
        cmds = t['commands']
        s(len(cmds))
        for c in cmds: s(c)
        s(t['complete']); s(""); s(""); s("")   # CompleteText Reverse Repeat Additional
        s(0); s(t.get('repeatable', 1)); s(0)   # ShowRoomDesc Repeatable Reversible
        s(0)                                    # ReverseCommands
        s(3)                                    # Where: all rooms
        q = t.get('question', "")
        s(q)                                    # Question (+2 hints if set)
        if q:
            s(t.get('hint1', "")); s(t.get('hint2', ""))
        restrs = t.get('restrs', [])
        s(len(restrs))
        for (v1, v2, v3, fail) in restrs:
            s(0); s(v1); s(v2); s(v3); s(fail)  # Type-0 object-location restriction
        actions = t.get('actions', [])          # raw field tuples, e.g. type-3
        s(len(actions))                         # set-var: (3, varidx0, 5, 0, expr, 0)
        for a in actions:
            for f in a: s(f)
        # RestrMask: "#", "#A#", ... -- or an explicit mixed-connector mask
        # like "#A#A#O#" via the optional 'mask' key.
        s(t.get('mask', "A".join(["#"] * len(restrs))))
    events = cfg.get('events', [])          # (short, taskAffected_1based
                                            #  [, time1, time2, restart])
    s(len(events))
    for e in events:
        (short, aff) = e[:2]
        t1 = e[2] if len(e) > 2 else 0
        t2 = e[3] if len(e) > 3 else 0
        restart = e[4] if len(e) > 4 else 1
        s(short); s(1)                      # Short StarterType=immediate
        s(restart); s(0)                    # RestartType TaskFinished
        s(t1); s(t2)                        # Time1 Time2
        s(""); s(""); s("")                 # Start/Look/FinishText
        s(3)                                # Where: all rooms
        s(0); s(0)                          # PauseTask PauserCompleted
        s(0); s("")                         # PrefTime1 PrefText1
        s(0); s(0)                          # ResumeTask ResumerCompleted
        s(0); s("")                         # PrefTime2 PrefText2
        s(0); s(0); s(0); s(0); s(0); s(0)  # Obj2/Dest Obj3/Dest Obj1/Dest
        s(aff)                              # TaskAffected

    npcs = cfg['npcs']                      # (name, room0based, att, stam, strLo,strHi, accLo,accHi, defLo,defHi, agiLo,agiHi, speed, recovery [, killedtask, staminatask])
    s(len(npcs))
    for n in npcs:
        (name, room, att, stam, sl, sh, al, ah, dl, dh, gl, gh, speed, rec) = n[:14]
        ktask = n[14] if len(n) > 14 else 0     # 1-based task refs, 0 = none
        stask = n[15] if len(n) > 15 else 0
        s(name); s("a"); s(0)
        s("A probe NPC."); s(room + 1); s(""); s(0)
        s(0); s(0); s(0)                    # topics walks showenterexit
        s(name + " is here, looking dangerous."); s(0)
        s(att)
        s(stam); s(stam)
        s(sl); s(sh); s(al); s(ah); s(dl); s(dh); s(gl); s(gh)
        s(speed); s(ktask); s(rec); s(stask)  # Speed KilledTask Recovery StaminaTask

    s(0); s(0)                              # groups syns
    vars_ = cfg.get('vars', [])             # (name, type 0=int/1=str, value-string)
    s(len(vars_))
    for (name, vtype, val) in vars_:
        s(name); s(vtype); s(val)
    s(0); s(0)                              # alrs font
    s("2026")
    return ("\r\n".join(L) + "\r\n").encode("latin-1")

CONFIGS = {
 'M3': dict(name="Probe M3",
    player=(200,10,10,60,60,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[("a","blaster",1,1,0,30,3,20,0)],
    npcs=[("Robot",0,2,35,0,1,0,0,0,0,0,0,0,0)]),
 'SP2': dict(name="Probe SP2",
    player=(200,10,10,60,60,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[],
    npcs=[("Robot",0,2,250,5,5,50,50,0,0,0,0,2,0)]),
 'SP3': dict(name="Probe SP3",
    player=(200,10,10,60,60,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[],
    npcs=[("Robot",0,2,250,5,5,50,50,0,0,0,0,3,0)]),
 'TS': dict(name="Probe TS",
    player=(300,10,10,60,60,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[],
    npcs=[("Aly",0,1,250,3,3,50,50,0,0,0,0,0,0),
          ("Foe",0,2,250,5,5,50,50,0,0,0,0,0,0),
          ("Bystander",0,0,250,7,7,50,50,0,0,0,0,0,0)]),
 # Wield/status surface probe: two held weapons with split best-ness (axe =
 # higher HitValue, sword = higher Accuracy) + an unseen NPC in a second room.
 # Player Acc 20-20 so a weapon's accuracy bonus is visible in status rolls.
 'WS': dict(name="Probe WS",
    player=(200,10,10,20,20,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{}),("Back Room","A back room.",{})],
    objects=[("a","sword",1,1,0,10,1,15,0),
             ("an","axe",1,1,0,20,0,5,0)],
    npcs=[("Robot",0,2,250,0,1,0,0,0,0,0,0,0,0),
          ("Ghost",1,0,50,0,1,0,0,0,0,0,0,0,0)]),
 # Status-layout probe (the pWS cosmetics follow-up): the Robot always hits
 # (Acc 50 vs player Agi 0) for exactly 5 (Str 5-5 vs Def 0), so the player's
 # live stamina drops below max and the status table's Stamina cells become
 # distinguishable; its own 30 stamina takes visible damage for the NPC table.
 # The rock is an in-room weapon for the wield-not-carried wording.
 'WS2': dict(name="Probe WS2",
    player=(200,10,10,20,20,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[("a","sword",1,1,0,10,1,15,0),
             ("a","rock",4,1,0,3,0,0,0)],
    npcs=[("Robot",0,2,30,5,5,50,50,0,0,0,0,0,0)]),
 # Throw-drop probe (settled live 2026-08-01): a method-5 throw moves the
 # weapon to the room and deals base-Strength-only damage (HitValue ignored).
 'TD': dict(name="Probe TD",
    player=(200,10,10,60,60,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[("a","spear",1,1,0,5,5,0,0)],
    npcs=[("Robot",0,2,250,0,1,0,0,0,0,0,0,0,0)]),
 # Body-part-static probe (§4 RUNNER_TESTS_TODO): `head` is a static Where-
 # type-4 part of Robot (NPC 1), `arm` a part of the player.  probe1/2/3 put
 # the referenced object through a Var1=2 object-location restriction --
 # is-hidden (0,0), visible-to-player (3,0), NOT-hidden (6,0).  Scarier
 # positions parts at OBJ_PART_NPC; the Runner's statics have no location
 # field, so its Var1=2 reads them as hidden.
 'BP': dict(name="Probe BP",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    npcs=[("Robot",0,0,250,0,0,0,0,0,0,0,0,0,0)],
    statics=[("a","head",4,1),
             ("an","arm",4,0)],
    tasks=[dict(commands=["probe1 %object%"], complete="P1 HIDDEN PASS.",
                restrs=[(2,0,0,"P1 HIDDEN FAIL.")]),
           dict(commands=["probe2 %object%"], complete="P2 VISIBLE PASS.",
                restrs=[(2,3,0,"P2 VISIBLE FAIL.")]),
           dict(commands=["probe3 %object%"], complete="P3 NOTHIDDEN PASS.",
                restrs=[(2,6,0,"P3 NOTHIDDEN FAIL.")])]),
 # Two-room variant: Robot (and so his head) is in the Back Room, the player
 # is not.  Asks whether visible-to tracks the parent NPC's location, and
 # whether the Runner's %object% scope filter will match an absent NPC's part
 # at all.
 'BP2': dict(name="Probe BP2",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{1:1}),
           ("Back Room","A back room.",{3:0})],
    npcs=[("Robot",1,0,250,0,0,0,0,0,0,0,0,0,0)],
    statics=[("a","head",4,1)],
    tasks=[dict(commands=["probe1 %object%"], complete="P1 HIDDEN PASS.",
                restrs=[(2,0,0,"P1 HIDDEN FAIL.")]),
           dict(commands=["probe2 %object%"], complete="P2 VISIBLE PASS.",
                restrs=[(2,3,0,"P2 VISIBLE FAIL.")]),
           dict(commands=["probe3 %object%"], complete="P3 NOTHIDDEN PASS.",
                restrs=[(2,6,0,"P3 NOTHIDDEN FAIL.")])]),
 # KilledTask/StaminaTask probe (§1 remainder): Robot dies to one 4-damage
 # blow and carries KilledTask -> task 1; Droid (stamina 100, threshold 10)
 # carries StaminaTask -> task 2, which under Scarier's rule fires on every
 # hit leaving 0 < stamina < max/10 (hits 23 and 24 of Str 4-4 leave 8 and 4;
 # hit 25 kills).
 'KT': dict(name="Probe KT",
    player=(200,4,4,60,60,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    npcs=[("Robot",0,2,4,0,0,0,0,0,0,0,0,0,0,1,0),
          ("Droid",0,2,100,0,0,0,0,0,0,0,0,0,0,0,2)],
    tasks=[dict(commands=["zzkilled"], complete="KILLEDTASK FIRED."),
           dict(commands=["zzstamina"], complete="STAMINATASK FIRED.")]),
 # Expression-division rounding probe (§4): each `/` reduction rounds
 # immediately -- run400 computes Round((a/b) + 0.000001), VB6 banker's
 # rounding with an epsilon that biases halves toward +infinity.
 # Expected (P-code-derived): 5/2=3, -5/2=-2, 7/2=4, -7/2=-3, 1/2=1,
 # -1/2=0, 4/2=2, 22/7=3.
 'DIV': dict(name="Probe DIV",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    npcs=[],
    vars=[("v1",0,"0"),("v2",0,"0"),("v3",0,"0"),("v4",0,"0"),
          ("v5",0,"0"),("v6",0,"0"),("v7",0,"0"),("v8",0,"0")],
    tasks=[dict(commands=["divtest"],
                complete="R= %v1% %v2% %v3% %v4% %v5% %v6% %v7% %v8% =R",
                actions=[(3,0,5,0,"5/2",0),
                         (3,1,5,0,"-5/2",0),
                         (3,2,5,0,"7/2",0),
                         (3,3,5,0,"-7/2",0),
                         (3,4,5,0,"1/2",0),
                         (3,5,5,0,"-1/2",0),
                         (3,6,5,0,"4/2",0),
                         (3,7,5,0,"22/7",0)])]),
 # Second-round division probe: v1/v3/v5 hold true negative values, so
 # %v1%/2 is a genuine negative dividend with no unary minus in the
 # expression -- this separates "round half away from zero" from "unary
 # minus reduces after the division" (both print -3 for the literal -5/2).
 'DIV2': dict(name="Probe DIV2",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    npcs=[],
    vars=[("v1",0,"0"),("v2",0,"0"),("v3",0,"0"),("v4",0,"0"),
          ("v5",0,"0"),("v6",0,"0"),("v7",0,"0"),("v8",0,"0")],
    tasks=[dict(commands=["divtest"],
                complete="R= %v1% %v2% %v3% %v4% %v5% %v6% %v7% %v8% =R",
                actions=[(3,0,5,0,"0-5",0),
                         (3,1,5,0,"%v1%/2",0),
                         (3,2,5,0,"0-7",0),
                         (3,3,5,0,"%v3%/2",0),
                         (3,4,5,0,"0-1",0),
                         (3,5,5,0,"%v5%/2",0),
                         (3,6,5,0,"-5/2",0),
                         (3,7,5,0,"3/2",0)])]),
 # Zero-word wildcard probe (§4 last row of RUNNER_TESTS_TODO): can the
 # Runner's mid-command `*` match ZERO words (TheADRIFTProject's TASK 41
 # `put * pill in cup` vs the input `put pill in cup`)?  foo/qux/yop test the
 # mid/trailing/leading positions unrestricted; `zap * pill` carries an
 # always-failing restriction WITH a FailMessage (any-object-hidden in a game
 # with no dynamic objects) to separate "no match" from "matched, restriction
 # failed" on the zero-word input.
 'ST': dict(name="Probe ST",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    npcs=[],
    tasks=[dict(commands=["foo * bar"], complete="MIDSTAR FIRED."),
           dict(commands=["qux *"], complete="TRAILSTAR FIRED."),
           dict(commands=["* yop"], complete="LEADSTAR FIRED."),
           dict(commands=["zap * pill"], complete="ZAPPASS.",
                restrs=[(1,0,0,"ZAPFAIL.")])]),
 # Fail-message selection probe (the sequel to ST): when a matched task's
 # restriction expression fails, WHICH failing restriction's FailMessage does
 # the Runner print?  Scarier (SCARE) keeps the lowest-indexed failing one;
 # TheADRIFTProject's TASK 41 transcript suggests run400 keeps the LAST
 # failing one (and falls through to the library when its message is empty).
 # (1,0,0) = "any object is hidden" -> always FAILS here (no dynamic objects);
 # (0,0,0) = "no object is hidden" -> always PASSES.  Masks are all-AND.
 'FM': dict(name="Probe FM",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    npcs=[],
    tasks=[dict(commands=["alpha"], complete="APASS.",
                restrs=[(1,0,0,"AFIRST."),(1,0,0,"ASECOND.")]),
           dict(commands=["beta"], complete="BPASS.",
                restrs=[(1,0,0,""),(1,0,0,"BSECOND.")]),
           dict(commands=["gamma"], complete="CPASS.",
                restrs=[(1,0,0,"CFIRST."),(1,0,0,"")]),
           dict(commands=["delta"], complete="DPASS.",
                restrs=[(1,0,0,"DFIRST."),(0,0,0,"DSECOND.")])]),
 # Mixed-connector fail-message probe: replicate TheADRIFTProject TASK 41's
 # exact mask `#A#A#O#` (R1 A R2 A (R3 O R4)).  epsilon = the game's live
 # shape: R1 pass, R2 FAIL (with message), R3 pass, R4 FAIL (empty message);
 # the whole expression fails on R2, yet the author's comp transcript shows
 # run400 falling through to the library.  zeta = same but R4's message
 # non-empty, to see WHICH message a mixed mask surfaces.  eta = OR-group
 # both failing, AND-half passing (expression fails inside the OR group).
 'FM2': dict(name="Probe FM2",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    npcs=[],
    tasks=[dict(commands=["epsilon"], complete="EPASS.", mask="#A#A#O#",
                restrs=[(0,0,0,"EONE."),(1,0,0,"ETWO."),
                        (0,0,0,"ETHREE."),(1,0,0,"")]),
           dict(commands=["zeta"], complete="ZPASS.", mask="#A#A#O#",
                restrs=[(0,0,0,"ZONE."),(1,0,0,"ZTWO."),
                        (0,0,0,"ZTHREE."),(1,0,0,"ZFOUR.")]),
           dict(commands=["eta"], complete="HPASS.", mask="#A#A#O#",
                restrs=[(0,0,0,"HONE."),(0,0,0,"HTWO."),
                        (1,0,0,"HTHREE."),(1,0,0,"HFOUR.")])]),
 # Third-round probe for the TheADRIFTProject fallthrough (reproduced live on
 # our run400 via a .tas transplant): which ingredient makes run400 NOT match
 # `put * pill in cup` on `put pill in cup`?  rho/omega = zero-word `*` with a
 # MULTI-WORD literal tail (unrestricted / restricted); tau/sig = same but
 # preceded by the game's other alternatives (an untypeable #command and a
 # choice-form that fails at its last word), unrestricted / restricted.
 'FM3': dict(name="Probe FM3",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    npcs=[],
    tasks=[dict(commands=["rho * pill in cup"], complete="RHOPASS."),
           dict(commands=["omega * pill in cup"], complete="OPASS.",
                restrs=[(1,0,0,"OFAIL.")]),
           dict(commands=["#zzztau",
                          "[tau]{the}[pill]{in/to/with}{the/a/an}[slime/goo]",
                          "tau * pill in cup"], complete="TAUPASS."),
           dict(commands=["#zzzsig",
                          "[sig]{the}[pill]{in/to/with}{the/a/an}[slime/goo]",
                          "sig * pill in cup"], complete="SPASS.",
                restrs=[(1,0,0,"SFAIL.")])]),
 # Fourth-round probe -- the hypothesis that reconciles everything: when a
 # matched task's restrictions FAIL, run400 gives the LIBRARY the input and
 # prints the fail message only if the library can't handle it (all earlier
 # probe verbs were library-gibberish, so the orders were indistinguishable).
 # take/wear collide with the library (success and refusal respectively);
 # `get * rock` passes its (absent) restrictions, testing that a PASSING task
 # still claims the input ahead of the library.
 'FM4': dict(name="Probe FM4",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[("a","rock",4,0,0,0,0,0,0)],
    npcs=[],
    tasks=[dict(commands=["take * rock"], complete="TAKEPASS.",
                restrs=[(1,0,0,"TFAIL.")]),
           dict(commands=["wear * rock"], complete="WEARPASS.",
                restrs=[(1,0,0,"WFAIL.")]),
           dict(commands=["get * rock"], complete="GETPASS.")]),
 # Fifth-round bisect: run400 does NOT fall through for simple replicas
 # (take/wear fail messages beat the library), yet the real TASK 41 does.
 # Suspect: its command LIST.  A = the verbatim eight commands; B = just the
 # nested-brace choice alternative + a plain starred one; C = the
 # `mix slime * pill *` shape; D = eight commands with no nested braces.
 # All carry one always-failing restriction with a distinct message; if an
 # input falls through to the library instead of printing the message, that
 # task never matched in run400.
 'FM5': dict(name="Probe FM5",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    npcs=[],
    tasks=[dict(commands=["#Put Pill IN Slime",
                          "[put/drop/mix]{the}[pill/drift{-}{o}{-}jump]{in/to/with}{the/a/an}[slime/goo/gooze/ooze]",
                          "mix slime * pill *",
                          "put * pill in cup",
                          "drop * pill * slime *",
                          "drop * pill * cup *",
                          "mix * pill * slime *",
                          "mix * slime * pill *"],
                complete="APASS.", restrs=[(1,0,0,"AFAIL.")]),
           dict(commands=["#zb",
                          "[put/drop/mix]{the}[wand/drift{-}{o}{-}jump]{in/to/with}{the/a/an}[slime/goo]",
                          "put * wand in hat"],
                complete="BPASS.", restrs=[(1,0,0,"BFAIL.")]),
           dict(commands=["#zc",
                          "mix slime * coin *",
                          "put * coin in jar"],
                complete="CPASS.", restrs=[(1,0,0,"CFAIL.")]),
           dict(commands=["#zd",
                          "[put/drop/mix]{the}[fork/spoon]{in/to/with}{the/a/an}[slime/goo]",
                          "mix slime * fork *",
                          "put * fork in mug",
                          "drop * fork * slime *",
                          "drop * fork * mug *",
                          "mix * fork * slime *",
                          "mix * slime * fork *"],
                complete="DPASS.", restrs=[(1,0,0,"DFAIL.")])]),
 # Sixth round: full verbatim replica of TheADRIFTProject's TASK 40/41 --
 # sixteen dynamic objects so the restriction indices line up (v1=5 -> dyn2
 # cup, v1=9 -> dyn6 pill, v1=18 -> dyn15 slime), tin before cup so the cup
 # is the SECOND container (restriction v3=2), verbatim commands,
 # restrictions, mask, hints, and non-repeatable flag.
 'FM6': dict(name="Probe FM6",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[("a","tin",1,0,0,0,0,0,0,1),
             ("a","brick",4,0,0,0,0,0,0),
             ("a","cup",1,0,0,0,0,0,0,1),
             ("a","dime",4,0,0,0,0,0,0),
             ("an","egg",4,0,0,0,0,0,0),
             ("a","fig",4,0,0,0,0,0,0),
             ("a","pill",1,0,0,0,0,0,0),
             ("a","hook",4,0,0,0,0,0,0),
             ("an","iron",4,0,0,0,0,0,0),
             ("a","jug",4,0,0,0,0,0,0),
             ("a","kite",4,0,0,0,0,0,0),
             ("a","lamp",4,0,0,0,0,0,0),
             ("a","mat",4,0,0,0,0,0,0),
             ("a","nut",4,0,0,0,0,0,0),
             ("an","oar",4,0,0,0,0,0,0),
             ("a","slime",4,0,0,0,0,0,0)],
    npcs=[],
    tasks=[dict(commands=["#Take Slime",
                          "[get/take/pick up]{the/a/an}[slime/ooze/gooze]",
                          "put * slime * cup *",
                          "scoop * slime *",
                          "scoop * slime * cup *"],
                complete="SCOOPPASS.",
                restrs=[(18,3,0,"No slime around here."),
                        (5,1,0,"You need something to put the slime in.")]),
           dict(commands=["#Put Pill IN Slime",
                          "[put/drop/mix]{the}[pill/drift{-}{o}{-}jump]{in/to/with}{the/a/an}[slime/goo/gooze/ooze]",
                          "mix slime * pill *",
                          "put * pill in cup",
                          "drop * pill * slime *",
                          "drop * pill * cup *",
                          "mix * pill * slime *",
                          "mix * slime * pill *"],
                complete="PILLPASS.", repeatable=0,
                question="How to I make a radioactive substance for a bomb?",
                hint1="You need the right ingredients.",
                hint2="Put the slime in the coffee cup and mix the pill in.",
                restrs=[(5,1,0,"Not yet.  You don't have everything you need."),
                        (18,4,2," Not yet."),
                        (9,1,0,"You don't have the small pill."),
                        (9,4,2,"")],
                mask="#A#A#O#")]),
 # Minimal pair vs FM5-A: identical task (verbatim commands, one quantifier
 # always-fail restriction, repeatable) but the pill and cup EXIST (held,
 # cup a container), so the library put-in is fully resolvable.  If run400
 # falls through here, the discriminator is library resolvability, not the
 # restriction types/mask/rep/hints.  The jug (non-container) and the
 # in-room slime give the library-refusal variants.
 'FM7': dict(name="Probe FM7",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[("a","pill",1,0,0,0,0,0,0),
             ("a","cup",1,0,0,0,0,0,0,1),
             ("a","jug",1,0,0,0,0,0,0),
             ("a","slime",4,0,0,0,0,0,0)],
    npcs=[],
    tasks=[dict(commands=["#Put Pill IN Slime",
                          "[put/drop/mix]{the}[pill/drift{-}{o}{-}jump]{in/to/with}{the/a/an}[slime/goo/gooze/ooze]",
                          "mix slime * pill *",
                          "put * pill in cup",
                          "drop * pill * slime *",
                          "drop * pill * cup *",
                          "mix * pill * slime *",
                          "mix * slime * pill *"],
                complete="XPASS.", restrs=[(1,0,0,"XFAIL.")])]),
 # Zero-length always-restarting checker event, 4.0 half (the 3.9 half is
 # make_39_fwprobe.py variant b): pill starts inside the cup, the checker
 # task's restriction is true from turn one.  Does the event fire at start
 # only (run390's answer), every turn, or per-turn relative to the command?
 # `take pill` gives the falling edge.
 'EV': dict(name="Probe EV",
    player=(200,0,0,0,0,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[("a","pill",2,0,0,0,0,0,0,0,0),
             ("a","cup",1,0,0,0,0,0,0,1),
             ("a","slime",4,0,0,0,0,0,0)],
    npcs=[],
    tasks=[dict(commands=["zzpillcheck"], complete="PILLCHECK FIRED.",
                restrs=[(3,4,1,"")])],
    events=[("Pill Check", 1)]),
 'RC': dict(name="Probe RC",
    player=(200,10,10,60,60,0,0,0,0,3),
    rooms=[("Test Arena","A bare arena.",{1:1}),
           ("Safe Room","A quiet room.",{3:0})],
    objects=[],
    npcs=[("Robot",0,2,250,5,5,50,50,0,0,0,0,0,0)]),
}

if __name__ == '__main__':
    which = sys.argv[1]
    out = sys.argv[2]
    open(out, 'wb').write(build(CONFIGS[which]))
    print("wrote", out)
