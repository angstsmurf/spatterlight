"""Keep one Bot alive and run Python snippets against it over a localhost TCP port.

    python3 serve.py PORT [script.cmd]      # loads the script, then serves
    python3 ask.py PORT 'b.goto("Shrine")'   # or: ask.py PORT -f snippet.py

Each request is exec'd with `b` (the Bot) in scope; stdout and any traceback
come back as the reply. This avoids re-replaying ~1000 commands per probe.
"""
import contextlib, io, os, socket, sys, traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bot import *  # noqa: E402,F403

port = int(sys.argv[1])
b = Bot.load(sys.argv[2]) if len(sys.argv) > 2 else Bot()


def reload():
    """Pick up edits to bot.py without losing the live game."""
    import importlib, bot
    importlib.reload(bot)
    b.__class__ = bot.Bot
    ns.update({k: v for k, v in vars(bot).items() if not k.startswith("_")})


ns = {"b": b, "reload": reload, **{k: v for k, v in globals().items() if not k.startswith("_")}}

srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", port))
srv.listen(1)
print("READY", len(b.s.cmds), flush=True)
while True:
    c, _ = srv.accept()
    with c:
        data = b""
        while not data.endswith(b"\0"):
            chunk = c.recv(65536)
            if not chunk:
                break
            data += chunk
        code = data.rstrip(b"\0").decode()
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            try:
                exec(code, ns)
            except Exception:
                traceback.print_exc(file=out)
        c.sendall(out.getvalue().encode() + b"\0")
