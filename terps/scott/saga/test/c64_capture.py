#!/usr/bin/env python3
# Captures the Commodore 64 goldens of a scene test (see scenetest.c) from the
# original game running under VICE. The C64 counterpart of zx_capture.lua: it
# plays the same .scene script the interpreter is tested with, feeding each
# command to the emulated keyboard, and at every `!check <name>.c64` line saves
# the screen next to the scene as <name>.c64 (8000 bytes of hires bitmap + 1000
# bytes of screen RAM, the layout scenetest reads).
#
#   c64_capture.py groundtruth_scene/gremlins_c64/gremlins.scene
#
# x64sc is started (in warp mode, with the scene's `!game` file autostarted) and
# driven over its binary monitor. Set X64SC to use a particular binary.
#
# Scene lines as seen from here:
#   plain line        typed, followed by RETURN (an empty line is just RETURN)
#   !hw <keys>        the same, for input only the original needs
#   !hwkey <keys>     typed without the RETURN (single-key prompts)
#   !terp <line>      ignored (input only the interpreter needs)
#   !check <name>     save the screen as <name> once it has settled
#   !hwwait <secs>    just let the game run (seconds of real time, in warp mode)
#   !dump <name>      save the screen right now; a later !check of the same
#                     name then compares against this dump
#   !hwpoke <addr>=<bytes>[*<n>]
#                     write hex bytes to RAM, in memory order, n times in a
#                     row (as in zx_capture.lua): for pictures no script can
#                     walk to, point the game's picture table somewhere else
#   !hwmem <name>     save the 64K of RAM as <name> (to find such a table)
# Everything else (!game, !tick, !slowdraw, comments) is skipped.
#
# The original stops with <HIT RETURN> whenever its text window fills up, and
# the key that answers it is lost. Those pauses are answered here, unless the
# scene's next input is an empty line anyway.
#
# The screen is taken from VICE's rendered frame, not from memory, so it does
# not matter where the game keeps its bitmap or how it splits the screen.

import os, socket, struct, subprocess, sys, time

PORT = 6510
POLL = 0.5          # seconds between looks at the screen: entering the
                    # monitor stops the emulation, so looking often slows it
SETTLE = 3          # looks in a row the screen must stay unchanged (cursor aside)
SETTLE_LIMIT = 40   # looks before giving up on a screen that keeps changing


class Vice:
    def __init__(self, sock):
        self.sock = sock
        self.reqid = 0

    def recv(self, n):
        data = b''
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                raise EOFError('VICE closed the monitor connection')
            data += chunk
        return data

    def command(self, cmd, body=b''):
        self.reqid += 1
        self.sock.sendall(struct.pack('<BBIIB', 2, 2, len(body), self.reqid, cmd) + body)
        while True:
            _, _, length, rtype, err, reqid = struct.unpack('<BBIBBI', self.recv(12))
            data = self.recv(length)
            if reqid == self.reqid:
                if err:
                    raise RuntimeError('VICE command %#x failed: error %#x' % (cmd, err))
                return data

    def resume(self):
        self.command(0xaa)

    def type(self, text):
        # PETSCII: unshifted letters are the ASCII capitals.
        text = text.upper().encode('ascii')
        self.command(0x72, bytes([len(text)]) + text)
        self.resume()

    def poke(self, addr, data):
        # No side effects, main memory, bank 1 (RAM)
        self.command(0x02, struct.pack('<BHHBH', 0, addr, addr + len(data) - 1, 0, 1) + data)
        self.resume()

    def ram(self):
        d = self.command(0x01, struct.pack('<BHHBH', 0, 0, 0xffff, 0, 1))
        self.resume()
        return d[2:]

    def frame(self):
        """VICE's rendered frame: its pixels (colour numbers), the width of a
        buffer line, and where the 320x200 display area starts."""
        d = self.command(0x84, b'\x01\x00')
        self.resume()
        head = struct.unpack_from('<I', d)[0]
        width, _, left, top = struct.unpack_from('<4H', d, 4)
        return d[4 + head + 4:], width, left, top

    def screen(self, shift=0):
        """The display area as 200 rows of colour numbers, `shift` lines down."""
        buf, width, left, top = self.frame()
        top += shift
        return [buf[(top + y) * width + left:(top + y) * width + left + 320] for y in range(200)]


def encode(rows):
    """Rendered frame -> bitmap + screen RAM, and the number of cells that
    do not fit (hires cells have two colours)."""
    bitmap, colours, bad = bytearray(8000), bytearray(1000), 0
    for cell in range(1000):
        cx, cy = cell % 40 * 8, cell // 40 * 8
        px = [rows[cy + y][cx:cx + 8] for y in range(8)]
        used = sorted(set(b''.join(px)))
        if len(used) > 2:
            bad += 1
        paper = used[0] & 15
        ink = used[1] & 15 if len(used) > 1 else paper
        colours[cell] = ink << 4 | paper
        for y in range(8):
            byte = 0
            for x in range(8):
                byte = byte << 1 | (px[y][x] != used[0])
            bitmap[cell * 8 + y] = byte
    return bytes(bitmap + colours), bad


def save(vice, path):
    """The games scroll the picture up or down a line or two to hide their
    screen split. Find the shift that puts it back on the cell grid."""
    best = None
    for shift in (0, -1, 1, -2, 2, -3, 3, 4):
        data, bad = encode(vice.screen(shift))
        if best is None or bad < best[1]:
            best = (data, bad, shift)
        if bad == 0:
            break
    data, bad, shift = best
    with open(path, 'wb') as f:
        f.write(data)
    print('c64_capture: wrote %s%s%s' % (path, ' (picture %+d lines)' % -shift if shift else '',
                                         ', %d cells with more than two colours' % bad if bad else ''))


def settle(vice):
    """Wait until the screen has stopped changing. A blinking cursor does not
    count: changes that fit in a box the size of a character."""
    last, same = None, 0
    for look in range(SETTLE_LIMIT):
        time.sleep(POLL)
        now = vice.screen()
        if last is not None:
            ys = [y for y in range(200) if now[y] != last[y]]
            xs = [x for y in ys for x in range(320) if now[y][x] != last[y][x]]
            cursor = not ys or (ys[-1] - ys[0] < 8 and max(xs) - min(xs) < 8)
            same = same + 1 if cursor else 0
        last = now
        if same >= SETTLE:
            return now
    print('c64_capture: the screen never settled')
    return last


# "<HIT RETURN>" at the left of the bottom text line: the seven pixel rows of
# its first twelve characters (ROM character set).
HIT_RETURN = (
    0x0e663c7e007c7e7e667c6670, 0x186618180066601866667618, 0x306618180066601866667e0c,
    0x607e1818007c7818667c7e06, 0x306618180078601866786e0c, 0x18661818006c6018666c6618,
    0x0e663c1800667e183c666670)


def paused(rows):
    def row_bits(row):
        paper = max(set(row), key=row.count)
        bits = 0
        for colour in row[:96]:
            bits = bits << 1 | (colour != paper)
        return bits
    # The text may sit a few lines off the cell grid (see save()).
    return any(all(row_bits(rows[top + i]) == HIT_RETURN[i] for i in range(7))
               for top in range(186, 194))


def main():
    scene = os.path.abspath(sys.argv[1])
    folder = os.path.dirname(scene)
    steps, dumped, game = [], set(), None
    for line in open(scene, encoding='latin-1').read().splitlines():
        word, _, rest = line.partition(' ')
        rest = rest.strip()
        if word == '!game':
            game = rest
        elif word == '!check':
            name = rest.split()[0]
            if name.endswith('.c64') and name not in dumped:
                steps.append(('check', name))
        elif word == '!dump':
            dumped.add(rest)
            steps.append(('dump', rest))
        elif word == '!hwwait':
            steps.append(('wait', float(rest)))
        elif word == '!hwpoke':
            addr, _, data = rest.partition('=')
            data, _, times = data.partition('*')
            steps.append(('poke', (int(addr, 16), bytes.fromhex(data) * int(times or 1))))
        elif word == '!hwmem':
            steps.append(('mem', rest))
        elif word == '!hwkey':
            steps.append(('type', rest))
        elif word == '!hw':
            steps.append(('type', rest + '\r'))
        elif not line.startswith(('!', '#')):
            steps.append(('type', line + '\r'))

    x64sc = os.environ.get('X64SC', 'x64sc')
    emulator = subprocess.Popen(
        [x64sc, '-default', '-warp', '-sounddev', 'dummy', '-binarymonitor',
         '-binarymonitoraddress', '127.0.0.1:%d' % PORT, '-autostart', os.path.join(folder, game)],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for attempt in range(100):
            try:
                sock = socket.create_connection(('127.0.0.1', PORT), timeout=30)
                break
            except OSError:
                time.sleep(0.2)
        else:
            sys.exit('c64_capture: could not reach the VICE monitor')
        vice = Vice(sock)
        vice.resume()

        def wait(step):
            """Let the screen settle, answering <HIT RETURN> pauses."""
            typed = [arg for kind, arg in steps[step:] if kind == 'type']
            while paused(settle(vice)) and typed[:1] != ['\r']:
                vice.type('\r')

        wait(0)
        for step, (kind, arg) in enumerate(steps):
            if kind == 'type':
                vice.type(arg)
                wait(step + 1)
            elif kind == 'wait':
                time.sleep(arg)
            elif kind == 'poke':
                vice.poke(*arg)
            elif kind == 'mem':
                with open(os.path.join(folder, arg), 'wb') as f:
                    f.write(vice.ram())
            else:
                if kind == 'check':
                    wait(step + 1)
                save(vice, os.path.join(folder, arg))
        try:
            vice.command(0xbb)
        except (EOFError, OSError):
            pass
    finally:
        try:
            emulator.wait(timeout=5)
        except subprocess.TimeoutExpired:
            emulator.kill()


main()
