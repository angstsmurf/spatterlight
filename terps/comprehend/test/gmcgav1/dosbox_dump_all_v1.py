#!/usr/bin/env python3
"""Dump *every* v1 Comprehend DOS picture by driving the native interpreter.

==============================  WIP / PARKED  ==============================
NOT yet working.  Calibration (MODE=calib) is solid: it boots, finds the code
base (0x8240) and DGROUP (0xbb40) via the FUN_1000_0c4c signature, and the
filename template at 0x2c33 reads "a:rA.ms1" (prefix [0x2c35], letter
[0x2c36]=AL+'A', pic [0x243a]=pic*2; VRAM = real 0xB800).  The sweep does NOT
work yet -- two blockers:
  1. The injected FUN_1000_0c4c draws the SAME image regardless of [0x243a]
     (pic index ignored).  0c4c reads its offset table from DGROUP [0xdf0], but
     the live [0xdf0] bytes don't match the .MS1 raw header, so a cold call with
     just AL+[0x243a] doesn't re-seek per pic.  Next: drive the caller
     FUN_1000_0bda / FUN_1000_0c9f (which set the id encoding), or repopulate
     [0xdf0] from the .MS1 ourselves before each call.
  2. Pre-clearing 0xB8000 (write_memory) suppresses the draw (white->all-white,
     black->all-black); only an un-precleared draw modifies VRAM.  Investigate,
     or never preclear and rely on each stream's own op15 fill.
A register-write GDB stub fix now exists -> can replace the marching stub with
"reset EIP to one fixed stub per picture" (drive via a raw P packet through
GDBClient._send_packet).  See memory comprehend-v1-cga-graphics for the RE.
===========================================================================


Like test/gmcga/dosbox_capture_pics.py for v2: instead of playing the game to
each room (which keeps dropping into text mode), we drive NOVEL.EXE's picture
load+draw routine directly.  FUN_1000_0c4c(AL = file index) builds the filename
from the DGROUP template at 0x2c31 (file letter = AL+'A'), opens it, reads the
+4-biased 16-word offset table into 0xdf0, and draws picture [0x243a]/2 of it.

At the input-wait we patch a "marching" stub per picture:
    C6 06 3A 24 pp      MOV byte [0x243a], pic*2
    B0 ff               MOV AL, fileidx
    E8 rr rr            CALL 0x0c4c          (rel recomputed per location)
breakpoint at stub+9, dump CGA VRAM, advance 9 bytes.  We set the filename
template prefix ('R','O',...) ourselves so the same routine reaches every file.

Run with MODE=calib first to print the live template + offset table, then the
prefix offset/bytes below can be confirmed.  Outputs OUT/<name>.fb + manifest.
"""
import sys, os, time
sys.path.insert(0, "/Users/administrator/dosbox-x-remotedebug/tests/integration")

GAME  = os.environ.get("V1GAME", "/Users/administrator/Downloads/comprehend games/crimson-crown")
OUT   = os.environ.get("V1OUT", "/tmp/v1cap")
MODE  = os.environ.get("MODE", "calib")
CONF  = "/tmp/v1_dumpall.conf"
# V1ENGINE: optional interpreter folder to BOOT instead of GAME's own.  The v1
# draw engine + drawing tables (fill pattern/subindex/brush) are shared across
# the v1 DOS games, and pictures are loaded by filename from A:, so Crimson
# Crown's NOVEL.EXE (which boots cleanly -- no name-entry prompt -- and whose
# 0b9f/0bc3/0bda offsets this harness already knows) can load+draw ANOTHER v1
# game's .MS1 images.  We mount a merged dir = engine files + GAME's *.MS1.
ENGINE = os.environ.get("V1ENGINE", "")

# 16-byte signature = opening bytes of FUN_1000_0c4c (the load+draw routine):
# PUSH DX/SI/DI; LEA BX,[0xdf0]; MOV [0x90ec],BX; ADD AL,0x41; MOV [0x2c36],AL
SIG       = bytes.fromhex("525657 8d1ef00d 891eec90 0441 a2362c".replace(" ", ""))
DRAW_OFF  = 0x0c4c          # FUN_1000_0c4c, CS-relative
CODE_TO_DS = 0x3900         # DGROUP(0x1390) - CODE(0x1000) = 0x390 segs = 0x3900 lin
G_243A    = 0x243a          # pic*2
G_TMPL    = 0x2c31          # filename template
G_OFFTAB  = 0x0df0          # 16-word offset table of the loaded file
IMAGES_PER_FILE = 16        # pics per .MS1 file; graphic id g -> file (g-1)>>4, pic (g-1)&15


def ms1_npics(path):
    """Number of valid pictures in a v1 .MS1 (17-word offset table at off 4)."""
    import struct
    d = open(path, "rb").read()
    if len(d) < 38:
        return 0
    w = struct.unpack("<17H", d[4:4 + 34])
    n = 0
    for i in range(16):
        if w[i + 1] > w[i] and w[i + 1] <= len(d):
            n += 1
        else:
            break
    return n


def parse_gda_objbase(gda_path):
    """Parse a v1 Comprehend .GDA -> {object_graphic_id: base_room_graphic_id}.

    Each item has a starting room (_room) and a graphic (_graphic); each room
    has a graphic.  An object picture is drawn over its room's picture, and its
    flood fills are bounded by the room geometry, so to reproduce it faithfully
    we draw room._graphic first, then the object.  Items with _room==0xff are
    not placed at game start (carried/created later) -> no canonical room.
    Mirrors GameData::parse_header/parse_rooms/parse_items (v1 layout).
    """
    import struct
    d = open(gda_path, "rb").read()
    magic = struct.unpack("<H", d[0:2])[0]
    if magic not in (0x2000, 0x4800):       # v1 disk-order layout (CC/Transylvania)
        return {}, magic
    MW = (-0x5a00 + 4) & 0xffff              # parse_header_le16 adds _magicWord
    def le16(idx):                           # header pointer #idx (0-based), from off 4
        return (struct.unpack("<H", d[4 + idx * 2:6 + idx * 2])[0] + MW) & 0xffff
    # header pointer order (v1): 7 actions, vm, dict, wmap, wmapt, room_desc,
    # 8 room dirs, room_flags, room_graphics, item_loc, item_flags, item_word,
    # item_strings, item_graphics, ...
    rdN, rdS = le16(12), le16(13)
    room_graphics = le16(21)
    item_loc, item_flags, item_word = le16(22), le16(23), le16(24)
    item_graphics = le16(26)
    nr_rooms = rdS - rdN                     # rooms 1..nr_rooms
    nr_items = item_word - item_flags
    room_g = {r: d[room_graphics + (r - 1)] for r in range(1, nr_rooms + 1)}
    objbase = {}
    for i in range(nr_items):
        og = d[item_graphics + i]
        if og == 0:
            continue
        rm = d[item_loc + i]
        objbase[og] = room_g.get(rm, 0) if rm != 0xff else 0
    return objbase, magic


def vram_to_fb(vram):
    fb = bytearray(320 * 200)
    for y in range(200):
        base = (y & 1) * 0x2000 + (y >> 1) * 80
        o = y * 320
        for xb in range(80):
            b = vram[base + xb]
            fb[o] = (b >> 6) & 3; fb[o+1] = (b >> 4) & 3
            fb[o+2] = (b >> 2) & 3; fb[o+3] = b & 3
            o += 4
    return bytes(fb)


def read_big(gdb, addr, size, chunk=1024):
    out = b""
    while size > 0:
        n = min(chunk, size)
        out += gdb.read_memory(addr, n); addr += n; size -= n
    return out


def main():
    from dosbox_debug import DOSBoxInstance
    import shutil, glob as _glob
    os.makedirs(OUT, exist_ok=True)

    mount = GAME
    if ENGINE:
        # merged mount: all engine files, then GAME's .MS1 images on top
        mount = "/tmp/v1_merged_mount"
        if os.path.isdir(mount):
            shutil.rmtree(mount)
        shutil.copytree(ENGINE, mount)
        for p in _glob.glob(os.path.join(GAME, "*.MS1")) + \
                 _glob.glob(os.path.join(GAME, "*.ms1")):
            shutil.copy(p, mount)        # overwrite engine game's images
        print(f"merged mount: engine={os.path.basename(ENGINE)} "
              f"images={os.path.basename(GAME)} -> {mount}", flush=True)

    open(CONF, "w").write(f"""[sdl]
autolock=false
[dosbox]
quit warning=false
gdbserver=true
gdbserver port=2159
qmpserver=true
qmpserver port=4444
[machine]
machine=cga
[autoexec]
MOUNT A "{mount}" -t floppy
A:
""")
    dbx = DOSBoxInstance(
        executable="/Users/administrator/dosbox-x-remotedebug/src/dosbox-x",
        config=CONF, working_dir="/Users/administrator/dosbox-x-remotedebug")
    dbx.start()
    gdb, qmp = dbx.gdb, dbx.qmp
    try: gdb.continue_()
    except Exception: pass

    def wait_text(needle, timeout=30):
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                if any(needle in l for l in gdb.screen_dump()): return True
            except Exception: pass
            time.sleep(0.4)
        return False

    assert wait_text("A:\\"), "no A: prompt"
    print("at A: prompt", flush=True)
    qmp.type_text("novel1\r", delay=0.05)
    time.sleep(5)                       # title draws (loads overlay)
    qmp.type_text(" ", delay=0.05)      # leave title -> start room
    time.sleep(4)

    gdb.halt()
    code_lin = None
    for base in range(0x600, 0xA0000, 0x1000):
        try:
            blob = read_big(gdb, base, 0x1000 + 16)
        except Exception:
            continue
        i = blob.find(SIG)
        if i >= 0:
            code_lin = base + i - DRAW_OFF
            break
    assert code_lin is not None, "FUN_1000_0c4c signature not found"
    ds_lin = code_lin + CODE_TO_DS
    print(f"code base {code_lin:#x}, DS base {ds_lin:#x}", flush=True)

    def gread(off, n): return read_big(gdb, ds_lin + off, n)

    regs = gdb.read_registers()
    print(f"halted eip={regs.eip:#x} cs={regs.cs:#x} -> wait_off={regs.eip:#x}", flush=True)
    print("template @0x2c31:", gread(G_TMPL, 16), flush=True)
    print("[0x243a]:", gread(G_243A, 1).hex(),
          " [0x2178]:", gread(0x2178, 2).hex(),
          " [0x2389](vram seg):", gread(0x2389, 2).hex(),
          " [0x2387](pic seg):", gread(0x2387, 2).hex(), flush=True)

    if MODE == "calib":
        print("calibration done", flush=True)
        try: dbx.stop()
        except Exception: pass
        return

    # ------------------------------------------------------------------
    # We drive the high-level picture entry points -- the proper callers that
    # do LOAD + DRAW (not just load):
    #   FUN_1000_0b9f  ROOM pics: if (AL != [0x2439]) { [0x2439]=AL;
    #                  [0x2c35]='r'; CALL 0bda (load); ... } CALL 1ff7 (draw)
    #   FUN_1000_0bc3  OBJECT pics: same but [0x2439]=AL|0x80, [0x2c35]='o'
    # 0bda sets [0x2178]=0x22 (offset-table byte count) + [0x243a]=pic-in-file
    # and derives the file letter, so 0c4c re-resolves the table per pic (this
    # is what made the pic index take effect -- old blocker #1).  1ff7 then runs
    # the overlay interpreter that paints 0xB800 (0c4c itself only LOADS the
    # stream into 0xdf0; the draw is a separate overlay call -- old blocker #2:
    # there was never a draw, not a VRAM-write problem).
    # A single linear id drives file+pic: id = file*16 + pic + 1 (file 0..7 =
    # letter A..H, pic 0..15).  The routine sets [0x2439]/[0x2c35] itself; we
    # only pre-write [0x2439]=0 to defeat the "same pic" dedupe guard.
    ENTRY_ROOM = 0x0b9f
    ENTRY_OBJ  = 0x0bc3
    G_2439 = 0x2439                      # global picture-id byte (dedupe guard)
    PRECLEAR = os.environ.get("PRECLEAR", "black")   # black|white|none
    # NB: the game clears the picture canvas to BLACK before drawing; preclearing
    # white makes the stream's flood-fills seed against the wrong colour and run
    # wild (validated: black -> picture window pixel-exact vs goldens).

    def preclear():
        if PRECLEAR == "none":
            return
        fill = b"\xff" if PRECLEAR == "white" else b"\x00"
        for i in range(0, 0x4000, 1024):
            gdb.write_memory(0xB8000 + i, fill * 1024)

    def draw_id(entry_off, pic_id, march):
        """Plant `MOV AL,id; CALL entry` at flat `march`, run to march+5."""
        gdb.write_memory(ds_lin + G_2439, b"\x00")   # != any id -> force redraw
        cur_off = march - code_lin
        rel = (entry_off - (cur_off + 5)) & 0xFFFF
        stub = bytes([0xB0, pic_id & 0xFF, 0xE8, rel & 0xFF, (rel >> 8) & 0xFF])
        gdb.write_memory(march, stub)
        gdb.set_breakpoint(march + 5)
        gdb.continue_()
        gdb.wait_for_stop(timeout=20)
        gdb.remove_breakpoint(march + 5)
        r = gdb.read_registers()
        return r.eip

    cur = int(os.environ.get("MARCH_BASE", str(regs.eip)))   # flat linear
    print(f"march base (flat) = {cur:#x}  off={cur-code_lin:#x}", flush=True)

    if MODE == "diag":
        # Sanity: does the injected load+draw actually paint 0xB800?
        # Optional DIAG_BASE_ID draws a ROOM base first (object over its room).
        is_obj = os.environ.get("DIAG_PREFIX", "r") == "o"
        entry = ENTRY_OBJ if is_obj else ENTRY_ROOM
        pic_id = int(os.environ.get("DIAG_ID", "1"))   # file0 pic0 (RA pic0)
        base_id = int(os.environ.get("DIAG_BASE_ID", "0"))
        preclear()
        pre = read_big(gdb, 0xB8000, 0x4000)
        open(f"{OUT}/diag_preclear.fb", "wb").write(pre)     # raw CGA VRAM
        if base_id:
            draw_id(ENTRY_ROOM, base_id, cur); cur += 5      # room base, no preclear
        eip = draw_id(entry, pic_id, cur)
        post = read_big(gdb, 0xB8000, 0x4000)
        open(f"{OUT}/diag_postdraw.fb", "wb").write(post)    # raw CGA VRAM
        diff = sum(1 for a, b in zip(pre, post) if a != b)
        print(f"diag id={pic_id} entry={entry:#x} eip={eip:#x} (want {cur+5:#x})", flush=True)
        print(f"  preclear unique bytes: {len(set(pre))}  "
              f"postdraw unique bytes: {len(set(post))}", flush=True)
        print(f"  VRAM bytes CHANGED by draw: {diff}/{len(pre)} "
              f"({'DRAW MISSED VRAM' if diff == 0 else 'draw painted VRAM'})", flush=True)
        print(f"  template now: {gread(G_TMPL, 16)}", flush=True)
        print(f"  [0xdf0] head now: {gread(0x0df0, 16).hex()}", flush=True)
        try: dbx.stop()
        except Exception: pass
        return

    # ---- sweep every picture via the high-level entry points ----
    # Auto-detect the R?.MS1 (rooms) and O?.MS1 (objects) files present and how
    # many pictures each holds, and parse the .GDA for the object->room mapping.
    import glob
    def files(prefix):                     # 'R' -> [('A','/path/RA.MS1'), ...]
        out = []
        for fi in range(8):
            letter = chr(ord('A') + fi)
            for cand in (f"{prefix}{letter}.MS1", f"{prefix.lower()}{letter.lower()}.ms1"):
                p = os.path.join(GAME, cand)
                if os.path.exists(p):
                    out.append((fi, letter, p)); break
        return out
    room_files = files("R")
    obj_files = files("O")

    gda = sorted(glob.glob(os.path.join(GAME, "*.GDA")) +
                 glob.glob(os.path.join(GAME, "*.gda")))
    gda = [g for g in gda if not os.path.basename(g).upper().startswith("CHARSET")]
    objbase = {}
    for g in gda:                          # CC has CC1/CC2; merge their maps
        m, magic = parse_gda_objbase(g)
        for og, base in m.items():         # placed (nonzero) wins; disk 1 first
            if base and not objbase.get(og):
                objbase[og] = base
            objbase.setdefault(og, 0)
        print(f"parsed {os.path.basename(g)} (magic {magic:#06x}): "
              f"{len(m)} item graphics", flush=True)
    # fallback room id for objects not placed at game start (room 0xff)
    FALLBACK = int(os.environ.get("FALLBACK_ROOM_ID", "1"))

    manifest = open(f"{OUT}/manifest.tsv", "w")
    manifest.write("name\ttag\tfile\tpic\tid\tbase_id\n")

    # rooms: each over a black canvas
    for fi, letter, path in room_files:
        for pic in range(ms1_npics(path)):
            pic_id = fi * IMAGES_PER_FILE + pic + 1
            name = f"R{letter}_{pic:02d}"
            preclear()
            eip = draw_id(ENTRY_ROOM, pic_id, cur)
            if eip != cur + 5:
                print(f"  WARN {name}: eip {eip:#x} != {cur+5:#x}", flush=True)
            open(f"{OUT}/{name}.fb", "wb").write(read_big(gdb, 0xB8000, 0x4000))
            manifest.write(f"{name}\tR\tR{letter}\t{pic}\t{pic_id}\t0\n"); manifest.flush()
            print(f"  {name} (id={pic_id})", flush=True)
            cur += 5

    # objects: each composited over its room base (room._graphic), so the
    # object's flood fills are bounded by the room geometry and the background
    # matches.  base_id 0 means "no placed room" -> use the fallback.
    for fi, letter, path in obj_files:
        for pic in range(ms1_npics(path)):
            obj_id = fi * IMAGES_PER_FILE + pic + 1
            base_id = objbase.get(obj_id, 0) or FALLBACK
            name = f"O{letter}_{pic:02d}"
            preclear()
            draw_id(ENTRY_ROOM, base_id, cur); cur += 5      # room base, no preclear
            eip = draw_id(ENTRY_OBJ, obj_id, cur)
            if eip != cur + 5:
                print(f"  WARN {name}: eip {eip:#x} != {cur+5:#x}", flush=True)
            open(f"{OUT}/{name}.fb", "wb").write(read_big(gdb, 0xB8000, 0x4000))
            placed = "placed" if objbase.get(obj_id, 0) else "FALLBACK"
            manifest.write(f"{name}\tO\tO{letter}\t{pic}\t{obj_id}\t{base_id}\n")
            manifest.flush()
            print(f"  {name} (id={obj_id}) over room id={base_id} [{placed}]", flush=True)
            cur += 5

    manifest.close()
    print("done", flush=True)
    try: dbx.stop()
    except Exception: pass


if __name__ == "__main__":
    main()
