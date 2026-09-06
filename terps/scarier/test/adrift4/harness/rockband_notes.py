"""Re-derive Rock Band's note sequence: the game shows a randomly coloured note
each turn and the player has to press the matching button, so the walkthrough's
fixed colour list is only valid for one RNG thread.  Replays the route one turn
at a time and answers whatever note is actually on screen."""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCARE = os.path.join(HERE, "scare")
GAME = os.path.join(ROOT, "games", "Rock Band.taf")
NOTE = re.compile(r"You see a (\w+) note!")

def play(seed, cmds):
    env = dict(os.environ, LC_ALL="C", SCR_SEED=str(seed), SCR_SKIP_WAITKEY="1")
    p = subprocess.run([SCARE, GAME],
                       input=("\n".join(cmds) + "\nquit\ny\n").encode("latin-1"),
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
    return p.stdout.decode("latin-1").replace("\r", "")

def main():
    seed = sys.argv[1] if len(sys.argv) > 1 else "1"
    live = [l for l in open(os.path.join(ROOT, "goldens", "rockband_solution.txt"),
                            encoding="latin-1").read().split("\n")
            if l.strip() and not l.lstrip().startswith("#")]
    head = live[:live.index("play the game") + 1]
    tail = live[live.index("north"):]
    notes = []
    for _ in range(40):
        out = play(seed, head + notes)
        seen = NOTE.findall(out)
        if len(seen) <= len(notes):          # no new note: the song is over
            break
        notes.append("use %s button" % seen[-1].lower())
    print("# %d notes at seed %s" % (len(notes), seed))
    print("\n".join(notes))
    out = play(seed, head + notes + tail)
    print("# win marker:",
          "You did it! You stopped Gigantor and saved the world (and Rock Band!)" in out)

if __name__ == "__main__":
    main()
