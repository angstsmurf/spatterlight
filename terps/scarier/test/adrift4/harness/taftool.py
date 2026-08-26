#!/usr/bin/env python3
"""Unpack / repack ADRIFT 3.7–4.0 .taf files so the matching Runner still accepts them.

A v4.0 .taf is:

    [14-byte signature][8 ASCII digits][zlib stream][15-byte trailer]

  * the digits are  len(file) - 14  in decimal;
  * the trailer is  0x00 + 12 bytes + CR LF.

The 12 bytes are the author's 8-character password stored as
``pw[0:4] + "Wild" + pw[4:8]`` (ADRIFT's author is Campbell Wild), obfuscated
with the same Visual Basic PRNG the v3.9/3.8 files use, indexed by ABSOLUTE
FILE OFFSET: the stream is advanced len(file)-14 times, then applied to the
twelve bytes.  run400.exe decodes it at load time and refuses the file with
"Error - Not an Adventure file." unless characters 5..8 spell "Wild".
(P-code: the Randomize 1976 / skip loop / Mid(s,5,4)<>"Wild" test at 0008EA62
in run400.txt.)  SCARE ignores both fields, so a file SCARE loads happily can
still be rejected by the Runner -- always repack with this.

A v3.9 / v3.8 / v3.7 .taf is:

    [14-byte signature][PRNG-XOR body]

There is no zlib and no sized trailer.  The VB PRNG (same constants as above)
is advanced 14 times for the signature, then each body byte is XOR'd with the
next draw.  The password line lives at the end of the *plain* body
(``pw[0:4] + "Wild" + pw[4:8]``); run390 checks Mid(...,5,4)=="Wild" after
decrypt.  Multi-line text fields use a ``**`` separator line rather than the
v4.0 ``\\xbd\\xd0`` marker.

Usage:
    python3 taftool.py unpack game.taf plain.bin       # write the plain body
    python3 taftool.py pack plain.bin donor.taf out.taf  # version (+ v4 password) from donor
"""
from __future__ import annotations

import sys
import zlib

SIG_400 = bytes.fromhex("3c423fc96a87c2cf93453e6139fa")
SIG_390 = bytes.fromhex("3c423fc96a87c2cf9445376139fa")
SIG_380 = bytes.fromhex("3c423fc96a87c2cf9445366139fa")
SIG_370 = bytes.fromhex("3c423fc96a87c2cf9445396139fa")

SIG_BY_VERSION = {
    "4.00": SIG_400,
    "3.90": SIG_390,
    "3.80": SIG_380,
    "3.70": SIG_370,
}
VERSION_BY_SIG = {sig: ver for ver, sig in SIG_BY_VERSION.items()}


def _stream(n: int) -> bytes:
    """First n values of the VB6 PRNG after Rnd(-1); Randomize 1976."""
    s = 0x00A09E86
    out = bytearray()
    for _ in range(n):
        s = (s * 0x43FD43FD + 0x00C39EC3) & 0x00FFFFFF
        out.append((255 * s) // 0x1000000)
    return bytes(out)


def _detect_version(header: bytes) -> str:
    if len(header) < 14:
        raise ValueError("not an ADRIFT taf (short file)")
    ver = VERSION_BY_SIG.get(header[:14])
    if ver is None:
        raise ValueError("not an ADRIFT 3.7–4.0 taf (unknown signature)")
    return ver


def unpack(path: str) -> tuple[str, bytes, int | None, bytes | None]:
    """Return (version, plain, v4_lead, v4_password_block).

    For 3.x, lead and password_block are None (password is the last plain line).
    """
    data = open(path, "rb").read()
    version = _detect_version(data)
    if version == "4.00":
        decomp = zlib.decompressobj()
        plain = decomp.decompress(data[22:])
        length = len(data)
        keystream = _stream(length)
        blk = bytes(
            data[length - 14 + i] ^ keystream[length - 14 + i] for i in range(12)
        )
        return version, plain, decomp.unused_data[0], blk

    # 3.7 / 3.8 / 3.9: skip 14 PRNG draws for the signature, XOR the rest.
    keystream = _stream(len(data))
    plain = bytes(data[i] ^ keystream[i] for i in range(14, len(data)))
    return version, plain, None, None


def pack(
    plain: bytes,
    version: str,
    out: str,
    *,
    lead: int | None = None,
    blk: bytes | None = None,
) -> int:
    """Write a .taf for ``version``.  v4.0 requires lead + password blk."""
    sig = SIG_BY_VERSION.get(version)
    if sig is None:
        raise ValueError(f"unsupported ADRIFT version: {version}")

    if version == "4.00":
        if lead is None or blk is None or len(blk) != 12:
            raise ValueError("v4.0 pack needs lead byte and 12-byte password block")
        compressed = zlib.compress(plain, 6)  # level 6: what ZlibTool.ocx emits
        length = 22 + len(compressed) + 15
        keystream = _stream(length)
        ob = bytes(blk[i] ^ keystream[length - 14 + i] for i in range(12))
        data = (
            sig
            + ("%08d" % (length - 14)).encode()
            + compressed
            + bytes([lead])
            + ob
            + b"\r\n"
        )
        assert len(data) == length
        open(out, "wb").write(data)
        return length

    # 3.x: signature in the clear, body XOR'd after skipping 14 draws.
    keystream = _stream(14 + len(plain))
    obf = bytes(plain[i] ^ keystream[14 + i] for i in range(len(plain)))
    data = sig + obf
    open(out, "wb").write(data)
    return len(data)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    if sys.argv[1] == "unpack":
        if len(sys.argv) != 4:
            sys.exit(__doc__)
        version, plain, lead, blk = unpack(sys.argv[2])
        open(sys.argv[3], "wb").write(plain)
        if version == "4.00":
            print(
                "version %s, password block %r, lead %02x, %d bytes"
                % (version, blk, lead, len(plain))
            )
        else:
            print("version %s, %d bytes" % (version, len(plain)))
    elif sys.argv[1] == "pack":
        if len(sys.argv) != 5:
            sys.exit(__doc__)
        version, _, lead, blk = unpack(sys.argv[3])
        plain = open(sys.argv[2], "rb").read()
        print(pack(plain, version, sys.argv[4], lead=lead, blk=blk))
    else:
        sys.exit(__doc__)
