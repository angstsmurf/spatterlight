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
    s(len(objs))
    for (pre, short, pos, wpn, prot, hv, meth, acc, wear) in objs:
        s(pre); s(short); s(0); s(0)
        s("A probe object."); s(pos); s(0); s(0); s("")
        s(0); s(0); s(0); s(wear); s(2); s(0)
        s(0); s(0); s(0); s(0)
        s(wpn); s(0); s(0)
        s(prot); s(hv); s(meth); s(acc)
        s(""); s(0)

    s(0); s(0)                              # tasks, events

    npcs = cfg['npcs']                      # (name, room0based, att, stam, strLo,strHi, accLo,accHi, defLo,defHi, agiLo,agiHi, speed, recovery)
    s(len(npcs))
    for n in npcs:
        (name, room, att, stam, sl, sh, al, ah, dl, dh, gl, gh, speed, rec) = n
        s(name); s("a"); s(0)
        s("A probe NPC."); s(room + 1); s(""); s(0)
        s(0); s(0); s(0)                    # topics walks showenterexit
        s(name + " is here, looking dangerous."); s(0)
        s(att)
        s(stam); s(stam)
        s(sl); s(sh); s(al); s(ah); s(dl); s(dh); s(gl); s(gh)
        s(speed); s(0); s(rec); s(0)        # Speed KilledTask Recovery StaminaTask

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
 # Throw-drop probe (settled live 2026-08-01): a method-5 throw moves the
 # weapon to the room and deals base-Strength-only damage (HitValue ignored).
 'TD': dict(name="Probe TD",
    player=(200,10,10,60,60,0,0,0,0,0),
    rooms=[("Test Arena","A bare arena.",{})],
    objects=[("a","spear",1,1,0,5,5,0,0)],
    npcs=[("Robot",0,2,250,0,1,0,0,0,0,0,0,0,0)]),
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
