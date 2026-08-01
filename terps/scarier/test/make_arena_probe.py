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

    objs = cfg.get('objects', [])           # (prefix, short, initpos, weapon, prot, hitval, method, acc, wearable)
    statics = cfg.get('statics', [])        # (prefix, short, wheretype, roomOrParent)
                                            #   wheretype: 1 one-room (arg = room 0-based),
                                            #   3 all-rooms (arg unused), 4 NPC part
                                            #   (arg: 0 = player, N = NPC N 1-based)
    s(len(objs) + len(statics))
    for (pre, short, pos, wpn, prot, hv, meth, acc, wear) in objs:
        s(pre); s(short); s(0); s(0)
        s("A probe object."); s(pos); s(0); s(0); s("")
        s(0); s(0); s(0); s(wear); s(2); s(0)
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
        s("")                                   # Question
        restrs = t.get('restrs', [])
        s(len(restrs))
        for (v1, v2, v3, fail) in restrs:
            s(0); s(v1); s(v2); s(v3); s(fail)  # Type-0 object-location restriction
        s(0)                                    # Actions
        s("A".join(["#"] * len(restrs)))        # RestrMask: "#", "#A#", ...
    s(0)                                    # events

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

    s(0); s(0); s(0); s(0); s(0)            # groups syns vars alrs font
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
