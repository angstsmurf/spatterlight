# exec'd in serve.py: hit-and-run against the D8 dragon, from wherever b is now.
# Kit that won: gold sword, zinc bracelet (rollhelper), ice stone (blocks BurnItems), several
# gold rings as melt decoys. Never open the dragon's offered chest (3d30+7 trap).
b.v = False
LOW = 100
KNIGHT = (1050, 1145)          # Black Knight roams outdoors 1080-1140: don't travel then
b.goto("dungeon_eight_room51")
b.send("east")
D = [k for k in b.kids() if b.ev(f'GetBoolean({k},"isenemydragonboss")') == "True"][0]
A = "attack " + b.ev(D + ".alias")
log = []
for t in range(400):
    if b.ev(D + ".dead") == "True":
        print("DEAD at turn", t, "n", len(b.s.cmds)); break
    if "sword_gold1" not in b.inv():
        print("SWORD LOST at turn", t); break
    if b.hp() < LOW:
        b.send("west")
        if b.room() != "dungeon_eight_room51":
            print("could not retreat", b.room()); break
        tm = b.time()
        if KNIGHT[0] <= tm <= KNIGHT[1]:
            while b.time() <= KNIGHT[1] and b.hp() < LOW + 5:
                if not b.foods():
                    break
                b.heal(b.hp() + 1)
            while b.time() <= KNIGHT[1]:
                b.send("wait")
            log.append(("knightwait", b.hp(), b.time()))
            if b.hp() < LOW:
                b.goto("Fountain Meadow"); b.drink(); b.goto("dungeon_eight_room51")
        else:
            b.goto("Fountain Meadow")
            b.drink()
            b.goto("dungeon_eight_room51")
        log.append(("heal", b.hp(), b.time(), len(b.s.cmds)))
        b.send("east")
        continue
    dh = b.num(D + ".hitpoints")
    t0 = b.send(A)
    log.append((dh, b.num(D + ".hitpoints"), b.hp()))
    if "burns up" in t0 or "hide your items" in t0:
        log.append(("EVENT", t0.strip()[-200:]))
print([l for l in log if l[0] == "EVENT"])
print(log[-12:])
b.status()
print(b.num(D + ".hitpoints"), b.ev("game.scripterrors") if False else "")
