#!/usr/bin/env python3
"""Extract an ADRIFT v5 .taf from a .blorb package.

Matches FrankenDrift Blorb.vb / FileIO.vb Blorb load: find the Exec resource
(chunk type ADRI), and if babel lives in IFmd with a "0000" placeholder in
Exec, re-embed babel into a normal .taf layout.

A blorb and a bare .taf disagree about obfuscation, so the payload usually has
to be re-XORed on the way out:

  * in a blorb it is the *iFiction metadata* that decides, not the payload
    layout -- ``bDeObfuscate = MetaData Is Nothing OrElse
    MetaData.OuterXml.Contains("compilerversion")`` (FileIO.vb:751).  A blorb
    the ADRIFT Generator wrote, or one with no IFmd chunk at all, is
    obfuscated; one rewrapped with a Babel-written IFmd is plain deflate.
  * in a bare .taf it is the layout: pre-5.0.20 (no babel size field) is read
    as plaintext, 5.0.20+ is obfuscated (FileIO.vb:800/816).

So a pre-5.0.20 payload out of a Generator blorb gets de-obfuscated, and a
Babel-rewrapped 5.0.20+ payload gets obfuscated, or the .taf is unloadable.

--layout also extracts the Runner window layout the Generator embeds
(Blorb.vb DataChunk, the pane arrangement the game opens with).
"""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

from taf2xml import OBFUSCATION_KEY


class BlorbError(Exception):
    pass


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def _iter_chunks(blorb: bytes) -> list[tuple[bytes, int, bytes]]:
    """Parse IFF chunks inside an IFRS FORM.

    Ignores the top-level FORM length (ADRIFT often writes it wrong) and walks
    until EOF, matching FrankenDrift clsBlorb.
    """
    if len(blorb) < 12:
        raise BlorbError("too short to be a Blorb file")
    if blorb[0:4] != b"FORM":
        raise BlorbError("not an IFF FORM file")
    if blorb[8:12] != b"IFRS":
        raise BlorbError("not an IFRS (Blorb) file")

    chunks: list[tuple[bytes, int, bytes]] = []
    pos = 12
    while pos + 8 <= len(blorb):
        chunk_id = blorb[pos : pos + 4]
        length = _u32(blorb, pos + 4)
        data_start = pos + 8
        data_end = data_start + length
        if data_end > len(blorb):
            raise BlorbError(
                f"chunk {chunk_id!r} at {pos} overruns file "
                f"(len={length}, file={len(blorb)})"
            )
        chunks.append((chunk_id, pos, blorb[data_start:data_end]))
        pos = data_end + (length % 2)
    return chunks


def _load_ridx(ridx_data: bytes) -> list[tuple[bytes, int, int]]:
    if len(ridx_data) < 4:
        raise BlorbError("truncated RIdx chunk")
    num = _u32(ridx_data, 0)
    need = 4 + 12 * num
    if len(ridx_data) < need:
        raise BlorbError("RIdx chunk shorter than declared resource count")
    entries: list[tuple[bytes, int, int]] = []
    for i in range(num):
        base = 4 + i * 12
        usage = ridx_data[base : base + 4]
        number = _u32(ridx_data, base + 4)
        start = _u32(ridx_data, base + 8)
        entries.append((usage, number, start))
    return entries


def _chunk_at(blorb: bytes, start: int) -> tuple[bytes, bytes]:
    if start + 8 > len(blorb):
        raise BlorbError(f"resource start {start} past end of file")
    chunk_id = blorb[start : start + 4]
    length = _u32(blorb, start + 4)
    data_start = start + 8
    data_end = data_start + length
    if data_end > len(blorb):
        raise BlorbError(f"resource at {start} overruns file")
    return chunk_id, blorb[data_start:data_end]


def _exec_and_ifmd(blorb: bytes) -> tuple[bytes, bytes | None]:
    chunks = _iter_chunks(blorb)
    if not chunks or chunks[0][0] != b"RIdx":
        raise BlorbError("first chunk must be RIdx")

    entries = _load_ridx(chunks[0][2])
    exec_entry = next(
        (e for e in entries if e[0] == b"Exec" and e[1] == 0),
        None,
    )
    if exec_entry is None:
        # Fall back to first Exec, or first ADRI chunk.
        exec_entry = next((e for e in entries if e[0] == b"Exec"), None)

    exec_data: bytes | None = None
    if exec_entry is not None:
        chunk_id, exec_data = _chunk_at(blorb, exec_entry[2])
        if chunk_id != b"ADRI":
            raise BlorbError(
                f"Exec resource is {chunk_id!r}, expected ADRI (ADRIFT)"
            )
    else:
        for chunk_id, _pos, data in chunks:
            if chunk_id == b"ADRI":
                exec_data = data
                break

    if exec_data is None:
        raise BlorbError("no ADRI Exec resource found")

    ifmd = next((data for cid, _pos, data in chunks if cid == b"IFmd"), None)
    return exec_data, ifmd


def _layout_xml(blorb: bytes) -> bytes | None:
    """The embedded Runner window layout: a TEXT (or BINA) data chunk whose
    body begins "RLAY", not listed in RIdx (Blorb.vb, DataChunk).  Returns the
    SOAP-serialised Infragistics XML past the "RLAY" tag, or None."""
    for chunk_id, _pos, data in _iter_chunks(blorb):
        if chunk_id in (b"TEXT", b"BINA") and data[:4] == b"RLAY":
            return data[4:]
    return None


def _inflates(data: bytes) -> bool:
    try:
        zlib.decompressobj().decompress(data)
    except zlib.error:
        return False
    return True


def _reobfuscate(data: bytes) -> bytes:
    """XOR with the fixed key -- its own inverse, so this both scrambles and
    unscrambles (a5deobf.cpp, FileIO.ObfuscateByteArray)."""
    return bytes(
        b ^ OBFUSCATION_KEY[i % len(OBFUSCATION_KEY)] for i, b in enumerate(data)
    )


def _blorb_obfuscated(ifmd: bytes | None) -> bool:
    """The Runner's blorb rule (FileIO.vb:751): no iFiction chunk, or one the
    ADRIFT Generator wrote (it carries <compilerversion>), means obfuscated.
    An IFmd from anywhere else -- Jacaranda Jim's 2011 release was rewrapped
    with a Babel-written one -- means plain deflate."""
    return ifmd is None or b"compilerversion" in ifmd


def _babel_size_field(babel_len: int) -> bytes:
    """Four-character uppercase hex length, zero-padded (ADRIFT style)."""
    field = f"{babel_len:X}".upper()
    if len(field) > 4:
        raise BlorbError(f"babel metadata too large ({babel_len} bytes)")
    return field.zfill(4).encode("ascii")


def _babel_from_ifmd(ifmd: bytes) -> bytes:
    """Normalize IFmd to ADRIFT TAF babel (starts with ``<ifindex``, no XML decl)."""
    text = ifmd.lstrip()
    if text.startswith(b"<?xml"):
        end = text.find(b"?>")
        if end != -1:
            text = text[end + 2 :].lstrip()
    return text


def blorb_to_taf(blorb: bytes) -> bytes:
    """Extract TAF bytes from an ADRIFT blorb."""
    exec_data, ifmd = _exec_and_ifmd(blorb)

    if len(exec_data) < 30:
        raise BlorbError("ADRI Exec resource too short")

    # Which layout the payload is in, i.e. where the deflate stream starts.
    # 5.0.20+ inserts a babel <ifindex> block after a 4-hex-digit size field at
    # offset 12; "0000" means the block is absent (which is what a blorb Exec
    # carries, its babel having been hoisted into the IFmd chunk).  Pre-5.0.20
    # has the deflate stream at offset 12 flat.  Both end in a 14-byte trailer.
    new_layout = exec_data[12:16] == b"0000" or exec_data[16:24] == b"<ifindex"
    header = 16 + int(exec_data[12:16], 16) if new_layout else 12
    if header + 14 > len(exec_data):
        raise BlorbError("Exec payload shorter than its babel size field")

    # The .taf we are about to write is read back by the layout rule: pre-5.0.20
    # plaintext, 5.0.20+ obfuscated.  The blorb we are reading used the metadata
    # rule instead.  Where the two disagree, re-XOR the payload region.
    region = exec_data[header:-14]
    if _blorb_obfuscated(ifmd) != new_layout:
        region = _reobfuscate(region)
    # ...and confirm it, by doing what the .taf reader will do with what we wrote.
    if not _inflates(_reobfuscate(region) if new_layout else region):
        raise BlorbError(
            "Exec payload does not inflate, obfuscated or otherwise"
        )
    exec_data = exec_data[:header] + region + exec_data[-14:]

    # A blorb Exec keeps its babel in the IFmd chunk and leaves the size field
    # a "0000" placeholder; a .taf has nowhere else to put it, so put it back.
    if exec_data[12:16] == b"0000" and ifmd is not None:
        babel = _babel_from_ifmd(ifmd)
        size_field = _babel_size_field(len(babel))
        return exec_data[0:12] + size_field + babel + exec_data[16:]

    return exec_data


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Extract an ADRIFT v5 .taf from a .blorb package.",
    )
    parser.add_argument("input_file", help="path to the .blorb file")
    parser.add_argument(
        "--output-file",
        "-o",
        help="write .taf to this file (default: input with .taf suffix)",
    )
    parser.add_argument(
        "--layout",
        nargs="?",
        const="",
        metavar="FILE",
        help="also extract the embedded Runner window layout to FILE "
        "(default: input with .layout.xml suffix)",
    )
    args = parser.parse_args(argv)

    input_path = Path(args.input_file)
    try:
        blorb = input_path.read_bytes()
    except OSError as exc:
        print(f"error: cannot read {input_path}: {exc}", file=sys.stderr)
        return 1

    try:
        taf = blorb_to_taf(blorb)
        layout = _layout_xml(blorb) if args.layout is not None else None
    except BlorbError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.layout is not None and layout is None:
        print("error: no Runner layout chunk in blorb", file=sys.stderr)
        return 1

    output_path = (
        Path(args.output_file) if args.output_file else input_path.with_suffix(".taf")
    )
    try:
        output_path.write_bytes(taf)
    except OSError as exc:
        print(f"error: cannot write {output_path}: {exc}", file=sys.stderr)
        return 1

    if layout is not None:
        layout_path = (
            Path(args.layout)
            if args.layout
            else input_path.with_suffix(".layout.xml")
        )
        try:
            layout_path.write_bytes(layout)
        except OSError as exc:
            print(f"error: cannot write {layout_path}: {exc}", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
