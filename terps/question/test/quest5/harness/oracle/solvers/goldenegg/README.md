# Golden Egg solver

Tools that derived `overrides/The Quest for the Golden Egg.cmd` (the full win, 3845 commands).

- `live.py`: a live session with cheap rewind via standby processes. `GE_DRIVER=native` plays on
  native `aslx_replay` live mode (about 6x faster); verify the result on qvh before freezing it.
- `bot.py`: navigation, inventory, healing, food and combat helpers over a session.
- `serve.py` / `ask.py`: keep one Bot alive and run snippets against it:
  `GE_DRIVER=native python3 serve.py 47312 some.cmd`, then `python3 ask.py 47312 'b.status()'`.
- `d7.py`, `boss7.py`, `boss8.py`: the dungeon 7 and boss scripts (exec'd through ask.py `-f`).
- `notes/`: per-dungeon notes and the endgame/economy summary (with an "as played" section).

To resume from a prefix, take the first N lines of the override (after the `#` header)
as the serve.py script.
