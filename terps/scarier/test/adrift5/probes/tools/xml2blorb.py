#!/usr/bin/env python3
"""Convert ADRIFT v5 XML to a .blorb package with its images and sounds.

Writes the layout the ADRIFT Generator's blorb export does (the corpus'
Generator-built blorbs all share it):

    RIdx    Exec 0, then Pict/Snd 1..N in one shared number sequence
    ADRI    the .taf bytes, babel size field "0000", payload obfuscated
    PNG     one chunk per media file, the whole file, chunk type by format
    ...
    IFmd    the iFiction record, carrying <compilerversion>

The adventure XML gains a <FileMappings> block mapping each resource number to
the src path the game's <img>/<audio> tags name.  Media files are found by that
path's last component in --media-dir, since the src is usually the author's
absolute Windows path.  Images are numbered before sounds, each in order of
first appearance, as in GrandpaRanchV5.blorb (Pict 1-10, Snd 11-14).

The IFmd carries <compilerversion>, so a Runner (FileIO.vb:751) or blorb2taf.py
treats the payload as obfuscated, as for a real Generator export.
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from taf2xml import TafError, _parse_xml
from xml2taf import (
    DEFAULT_PASSWORD,
    Xml2TafError,
    _default_libraries,
    _probe_ifid,
    _xml_escape,
    embed_libraries,
    pack_adventure,
)

MEDIA_TAG_RE = re.compile(
    r"""<(img|audio)\b[^>]*?\bsrc\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+))""",
    re.IGNORECASE,
)

# Chunk type by file extension: the Blorb spec's, as the Generator writes them.
PICT_TYPES = {".png": b"PNG ", ".jpg": b"JPEG", ".jpeg": b"JPEG"}
SND_TYPES = {".wav": b"WAVE", ".mp3": b"MP3 ", ".ogg": b"OGGV", ".mod": b"MOD "}


class BlorbBuildError(Exception):
    pass


def _media_sources(root: ET.Element) -> tuple[list[str], list[str]]:
    """Every distinct <img> and <audio> src in the adventure's text, in order
    of first appearance."""
    images: list[str] = []
    sounds: list[str] = []
    for elem in root.iter():
        for match in MEDIA_TAG_RE.finditer(elem.text or ""):
            src = match.group(2) or match.group(3) or match.group(4) or ""
            found = images if match.group(1).lower() == "img" else sounds
            if src and src not in found:
                found.append(src)
    return images, sounds


def _basename(src: str) -> str:
    return re.split(r"[\\/:]", src)[-1]


def _find_media(src: str, media_dir: Path) -> Path:
    """The file a src names, by its last path component, case-insensitively
    (a Windows author's capitals need not match the files on disk)."""
    name = _basename(src)
    exact = media_dir / name
    if exact.is_file():
        return exact
    for candidate in media_dir.iterdir():
        if candidate.name.lower() == name.lower() and candidate.is_file():
            return candidate
    raise BlorbBuildError(f"no file {name!r} in {media_dir} for src {src!r}")


def _chunk_type(path: Path, types: dict[str, bytes], kind: str) -> bytes:
    chunk_type = types.get(path.suffix.lower())
    if chunk_type is None:
        raise BlorbBuildError(
            f"unsupported {kind} format {path.suffix!r} ({path.name}); "
            f"expected one of {', '.join(sorted(types))}"
        )
    return chunk_type


def _add_file_mappings(root: ET.Element, sources: list[str]) -> None:
    mappings = ET.SubElement(root, "FileMappings")
    for number, src in enumerate(sources, start=1):
        mapping = ET.SubElement(mappings, "Mapping")
        ET.SubElement(mapping, "Resource").text = str(number)
        ET.SubElement(mapping, "File").text = src


def _ifmd_xml(root: ET.Element, ifid_seed: str) -> bytes:
    title = root.findtext("Title") or "Untitled"
    author = root.findtext("Author") or "Anonymous"
    release_date = (root.findtext("LastUpdated") or "")[:10]
    release = "<release><version>1</version>"
    if release_date:
        release += f"<releasedate>{release_date}</releasedate>"
    release += (
        "<compiler>ADRIFT 5</compiler>"
        "<compilerversion>5.0.36</compilerversion>"
        "</release>"
    )
    return (
        '<?xml version="1.0" encoding="utf-8"?>'
        '<ifindex version="1.0" '
        'xmlns="http://babel.ifarchive.org/protocol/iFiction/">'
        "<story>"
        "<identification>"
        f"<ifid>{_probe_ifid(ifid_seed)}</ifid>"
        "<format>adrift</format>"
        "</identification>"
        "<bibliographic>"
        f"<title>{_xml_escape(title)}</title>"
        f"<author>{_xml_escape(author)}</author>"
        "<language>en-GB</language>"
        "</bibliographic>"
        f"<releases><attached>{release}</attached></releases>"
        "</story>"
        "</ifindex>"
    ).encode("utf-8")


def _iff_chunk(chunk_type: bytes, data: bytes) -> bytes:
    pad = b"\0" if len(data) % 2 else b""
    return chunk_type + struct.pack(">I", len(data)) + data + pad


def build_blorb(
    resources: list[tuple[bytes, bytes, bytes]],
    trailing: list[tuple[bytes, bytes]],
) -> bytes:
    """Lay out an IFRS FORM.  ``resources`` are (usage, chunk type, data),
    indexed in RIdx under numbers 0.. per usage as given (Exec first, then
    the shared Pict/Snd sequence from 1); ``trailing`` are unindexed chunks
    appended after them."""
    ridx_size = 8 + 4 + 12 * len(resources)
    offset = 12 + ridx_size
    entries = bytearray(struct.pack(">I", len(resources)))
    body = bytearray()
    for number, (usage, chunk_type, data) in enumerate(resources):
        entries += usage + struct.pack(">II", number, offset)
        chunk = _iff_chunk(chunk_type, data)
        body += chunk
        offset += len(chunk)
    for chunk_type, data in trailing:
        body += _iff_chunk(chunk_type, data)
    form = b"IFRS" + _iff_chunk(b"RIdx", bytes(entries)) + bytes(body)
    return b"FORM" + struct.pack(">I", len(form)) + form


def xml_to_blorb(
    xml: str,
    *,
    library_paths: list[Path],
    media_dir: Path,
    password: str = DEFAULT_PASSWORD,
    ifid_seed: str = "",
) -> bytes:
    root, has_bom, has_declaration = _parse_xml(xml)
    images, sounds = _media_sources(root)
    if not images and not sounds:
        raise BlorbBuildError("the adventure names no <img> or <audio> src")
    if root.find("FileMappings") is not None:
        raise BlorbBuildError("the adventure already has <FileMappings>")

    media: list[tuple[bytes, bytes, bytes]] = []
    for src in images:
        path = _find_media(src, media_dir)
        media.append((b"Pict", _chunk_type(path, PICT_TYPES, "image"), path.read_bytes()))
    for src in sounds:
        path = _find_media(src, media_dir)
        media.append((b"Snd ", _chunk_type(path, SND_TYPES, "sound"), path.read_bytes()))

    embed_libraries(root, library_paths)
    _add_file_mappings(root, images + sounds)
    exec_data = pack_adventure(
        root,
        has_bom=has_bom,
        has_declaration=has_declaration,
        babel=b"",
        password=password,
    )
    ifmd = _ifmd_xml(root, ifid_seed or root.findtext("Title") or "Untitled")
    return build_blorb([(b"Exec", b"ADRI", exec_data)] + media, [(b"IFmd", ifmd)])


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Convert ADRIFT v5 XML to a .blorb with its media files.",
    )
    parser.add_argument("input_file", help="path to the XML file")
    parser.add_argument(
        "--output-file",
        "-o",
        help="write .blorb to this file instead of deriving the name from the input",
    )
    parser.add_argument(
        "--media-dir",
        "-m",
        help="directory holding the files the <img>/<audio> srcs name "
        "(default: the output file's directory)",
    )
    parser.add_argument(
        "--library",
        "-l",
        action="append",
        default=[],
        dest="libraries",
        help="library AMF/XML file to embed (StandardLibrary is always included)",
    )
    parser.add_argument(
        "--password",
        default=DEFAULT_PASSWORD,
        help="8-character adventure password (default: eight spaces)",
    )
    args = parser.parse_args(argv)

    input_path = Path(args.input_file)
    try:
        xml = input_path.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"error: cannot read {input_path}: {exc}", file=sys.stderr)
        return 1

    library_paths = _default_libraries(args.libraries)
    missing = [str(path) for path in library_paths if not path.exists()]
    if not library_paths or missing:
        print(
            "error: library file(s) not found: " + ", ".join(missing or ["StandardLibrary"]),
            file=sys.stderr,
        )
        return 1

    output_path = (
        Path(args.output_file) if args.output_file else input_path.with_suffix(".blorb")
    )
    media_dir = Path(args.media_dir) if args.media_dir else output_path.parent

    try:
        blorb = xml_to_blorb(
            xml,
            library_paths=library_paths,
            media_dir=media_dir,
            password=args.password,
            ifid_seed=output_path.stem,
        )
    except (BlorbBuildError, Xml2TafError, TafError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    try:
        output_path.write_bytes(blorb)
    except OSError as exc:
        print(f"error: cannot write {output_path}: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
