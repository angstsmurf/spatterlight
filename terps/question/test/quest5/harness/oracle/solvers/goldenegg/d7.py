SP = "/private/tmp/claude-502/-Users-administrator-spatterlight/0e2a7302-77d1-4a17-8f44-216f69b3c417/scratchpad"
TABLE_ITEMS = globals().get("TABLE_LEFT") or ["small rock", "gold bracelet", "gold dagger", "sandals", "vitamin"]


def topup(b):
    while b.hp() < 90 and b.num("bottle1.full") > 0:
        h = b.hp()
        b.send("drink from bottle")
        if b.hp() <= h:
            break
    if b.hp() < 60:
        b.heal(60)


def step(target):
    def run(b):
        b.goto(target, minhp=0)
        b.fight(minhp=20, eatbelow=45)
        b.cure()
    return run


b.v = False
b.fightopts = dict(minhp=20, eatbelow=45)
START = globals().get('START', 1)
for r in range(START, 19):
    room = f"dungeon_seven_room{r}"
    if b.ev(f"player.parent = {room}") != "True":
        topup(b)
        b.attempt(step(room), tries=40, floor=30, label=room)
    tables = [k for k in b.kids() if k.startswith("stone_table")]
    for t in tables:
        if not b.kids(t) and TABLE_ITEMS:
            it = TABLE_ITEMS.pop(0)
            print(room, "table", t, b.send(f"put {it} on table").strip().splitlines()[-1])
    print(b.status())
    b.save(f"{SP}/d7_{r}.cmd")
