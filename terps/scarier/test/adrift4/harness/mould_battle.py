#!/usr/bin/env python3
"""Re-derive the mould shapeshifting-imp battle for a fixed seed.

Each round the imp announces an attack shape via flavor text, then asks
"What do you want to turn your hand into?" with a 5-option menu whose
labels shift each turn (currently-held item shows as "(retain X)").
Empirically (from 4 real Wine-Runner transcripts recorded under 4 different
xoshiro seeds, ~40 data points, 100% consistent per (attack, weapon) pair):

  crowbar attack -> crowbar, hook, or knife all succeed; thin shield unreliable
  lasso attack    -> crowbar, hook, or knife all succeed; thin shield unreliable
  chain attack    -> hook is the only reliable counter; everything else fails
  bird attack     -> thin shield is the only reliable counter; everything else fails
  baseball attack -> baseball bat is assumed the reliable counter (untested
                      empirically -- no real transcript ever chose it against a
                      baseball attack -- but thematically obvious and the only
                      unexplored cell in an otherwise fully-consistent table)

So "hook" is a universal safe choice for crowbar/lasso/chain attacks, leaving
only bird (-> thin shield) and baseball (-> baseball bat) needing their own
response.

Usage, from test/adrift4/:
    python3 harness/mould_battle.py mould_solution <seed> <prefix_len>

`prefix_len` is how many leading (non-comment, non-blank) lines of
goldens/<solution>.txt to keep as-is before the battle starts -- find it by
running the prefix alone and checking where the first "What do you want to
turn your hand into?" battle-loop menu (the one immediately preceded by a
"You don't have time for anything else, apart from the fight." for any
off-battle command) appears.  It prints the full command list (prefix +
battle) to stdout once "Congratulations on winning" appears, or raises after
a round cap.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCARE = os.path.join(HERE, "scare")
GAME = os.path.join(ROOT, "games", "mould.taf")
WIN = "Congratulations on winning The Potter and the Mould"

COUNTER = {
    "crowbar": "crowbar",
    "lasso": "knife",
    "chain": "hook",
    "bird": "thin shield",
    "baseball": "baseball bat",
}


def classify_attack(line):
    low = line.lower()
    if "crowbar" in low:
        return "crowbar"
    if "bird" in low:
        return "bird"
    if "chain" in low:
        return "chain"
    if "lasso" in low:
        return "lasso"
    if "baseball" in low or "ball" in low:
        return "baseball"
    return None


def run(seed, cmds):
    env = dict(os.environ, SCR_ECHO_INPUT="1", SCR_RNG="xoshiro",
               SCR_SEED=str(seed), LC_ALL="C")
    proc = subprocess.run(
        [SCARE, GAME],
        input=("\n".join(cmds) + "\n").encode("latin-1"),
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
    return proc.stdout.decode("latin-1")


# The "(Press a key)" prompt at battle-start silently consumes exactly one
# real stdin line with no echo and no effect (confirmed empirically: feeding
# prefix + [X] leaves the very first post-battle menu UNANSWERED regardless
# of X, while prefix + [X, Y] answers it with Y, not X). So a fixed one-shot
# bridge token must sit between the prefix and the real per-round chase list
# in every run() call, or the first real battle answer is silently eaten.
BRIDGE = ["x"]


def _attack_paragraph(lines, i):
    """Join the blank-line-delimited paragraph(s) immediately before line
    index i (word-wrap can split "the imp ... morphs into a X" across two or
    more physical lines), and return the first one that looks like a battle
    attack announcement, or None."""
    paras = []
    cur = []
    for k in range(i - 1, max(i - 20, -1), -1):
        if lines[k].strip() == "":
            if cur:
                paras.append(" ".join(reversed(cur)))
                cur = []
        else:
            cur.append(lines[k])
    if cur:
        paras.append(" ".join(reversed(cur)))
    for p in paras:
        low = p.lower()
        if "the imp" in low and any(q in low for q in
                ("turns into", "shifts into", "morphs into", "turning into",
                 "into a")):
            return p
    return None


def next_move(out, n_answered):
    """Given full transcript so far and how many battle rounds have already
    been answered by real (non-EOF-artifact) input, find the next battle
    response, or None if the battle (or game) looks finished / stuck.

    IMPORTANT: at EOF (or on any input the battle loop doesn't recognise as
    a menu digit), SCARE does NOT block/exit -- it silently treats it as a
    no-op turn and lets the imp attack again, so a single run() call can
    show SEVERAL extra "What do you want to turn your hand into?" menus
    past the one actually awaiting our next real answer. So we must not
    just grab the LAST such menu in the transcript -- we must find the
    (n_answered)-th genuine battle menu (0-indexed, battle menus only, i.e.
    excluding the game's other unrelated same-header shape-choice menus)
    and treat THAT as the pending one."""
    lines = out.split("\n")
    menu_idxs = [i for i, l in enumerate(lines)
                 if l.strip() == "What do you want to turn your hand into?"]
    battle_idxs = [i for i in menu_idxs if _attack_paragraph(lines, i)]
    if n_answered >= len(battle_idxs):
        return None
    i = battle_idxs[n_answered]
    attack_line = _attack_paragraph(lines, i)
    atk = classify_attack(attack_line)
    weapon = COUNTER.get(atk)
    if weapon is None:
        return None
    menu = {}
    for k in range(1, 6):
        m = re.match(r"(\d) - (\(retain )?(.+?)\)?\s*$", lines[i + k].strip())
        if m:
            menu[m.group(3)] = m.group(1)
    digit = menu.get(weapon)
    if digit is None:
        return None
    return digit, atk, weapon


def main():
    if len(sys.argv) not in (4, 5):
        raise SystemExit(__doc__)
    solution, seed, prefix_len = sys.argv[1], sys.argv[2], int(sys.argv[3])
    max_rounds = int(sys.argv[4]) if len(sys.argv) == 5 else 150
    src = open(os.path.join(ROOT, "goldens", solution + ".txt"),
               encoding="latin-1").read()
    live = [l for l in src.split("\n") if not l.lstrip().startswith("#")]
    prefix = live[:prefix_len]

    chase = []
    ROUNDS = max_rounds
    for rnd in range(ROUNDS):
        out = run(seed, prefix + BRIDGE + chase)
        if WIN in out:
            sys.stderr.write("# WIN after %d battle rounds\n" % rnd)
            for c in prefix + BRIDGE + chase:
                print(c)
            return
        mv = next_move(out, len(chase))
        if mv is None:
            sys.stderr.write(
                "# stuck at round %d: no more battle menu found (or "
                "unrecognised attack) -- dumping tail\n" % rnd)
            sys.stderr.write("\n".join(out.split("\n")[-40:]) + "\n")
            raise SystemExit(1)
        digit, atk, weapon = mv
        sys.stderr.write("# round %d: %s attack -> %s (%s)\n"
                          % (rnd, atk, weapon, digit))
        chase.append(digit)
    if os.environ.get("MOULD_DUMP_FULL"):
        with open(os.environ["MOULD_DUMP_FULL"], "w", encoding="latin-1") as f:
            f.write(out)
    raise SystemExit("gave up after %d rounds" % ROUNDS)


if __name__ == "__main__":
    main()
