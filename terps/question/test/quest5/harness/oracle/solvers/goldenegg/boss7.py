import re


def boss7(b):
    """D7 giant snake: sword it, and lob lit dynamite into any open mouth."""
    if b.room() != "dungeon_seven_room19":
        b.send("down")
    snake = None
    for turn in range(80):
        es = [e for e in b.enemies() if e[1].endswith("giant snake")]
        if not es:
            return
        snake = es[0]
        dyn = [o for o in b.inv() if o.startswith("dynamite")]
        open_heads = [h for h in ("asp", "adder", "viper")
                      if b.ev(f"ListContains(ScopeReachable(), boss_{h}_snake_head) and boss_{h}_snake_head.isopen") == "True"]
        if b.hp() < 45 and b.foods():
            b.heal(b.hp() + 1)
            continue
        if b.hp() < 15:
            raise Failed(f"low hp {b.hp()}")
        if open_heads and dyn and b.perturb % 3 != 2:
            b.send("light dynamite")
            if "Please choose" in b.last:
                b.send("1")
            b.send("throw dynamite")
            if "Please choose" in b.last:
                b.send("1")
            continue
        if b.perturb and turn == (b.perturb - 1) // 3:
            b.send("wait")
            continue
        b.send("attack giant snake")
    raise Failed("boss fight did not end")
