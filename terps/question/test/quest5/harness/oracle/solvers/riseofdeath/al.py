# exec'd in rserve.py: walk to Al'Akir, bind R to Extinguish Life, and win the fight.
#
# Al'Akir (FoeType 27): 200M health, heals FoeMaxHealth/20 every third turn and resets
# FoeArmor to 0.1 at that turn if it is above 2.52. Under 100M he turns impervious, so
# the kill is Extinguish Life (Souls * 20 chaos damage) with ~5M souls.
# Q has to stay Tendril Lash (moves cannot be rebound mid-fight): Haunt (E, +0.15 armor,
# no damage) builds his armor to 2.5, then Lash does about 5M a turn.
CAP = 2.52


def al_move():
    hp, arm = b.num("Enemy.FoeHealth"), float(b.ev("Enemy.FoeArmor"))
    if hp < 100000000:
        return "r"
    if arm + 0.15 <= CAP:
        return "E"
    return "q"


b.goto("Air2")
b.heal()
if b.ev(Y + "RWords") != "Extinguish Life":
    for c in ("s", "east", "r"):
        b.send(c)
    b.walk("b")
    b.heal()
print("R =", b.ev(Y + "RWords"), b.where(), "souls", b.num(Y + "Souls"))
b.walk("up")
b.heal()
b.send("up")
print(b.where(), len(b.s.cmds))
b.s.checkpoint()
b.send("down")
print(b.where(), b.ev("Enemy.FoeType"))
turns = b.fight(al_move, limit=600)
print("fight over after", turns, "at", b.where(), len(b.s.cmds))
b.status()
