#!/usr/bin/env python3
"""Convert ADRIFT v5 XML to .taf files.

Compression logic matches ADRIFT FileIO.vb SaveFileToStream/Save500 and
adrift-5-rs/tools/taf2xml.py.
"""

from __future__ import annotations

import argparse
import copy
import re
import struct
import sys
import uuid
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from taf2xml import OBFUSCATION_KEY, V5_MAGIC, TafError, _parse_xml

DEFAULT_STANDARD_LIBRARY = SCRIPT_DIR / "libraries" / "StandardLibrary.amf"

ITEM_TAGS = frozenset(
    {
        "Folder",
        "Property",
        "Location",
        "Object",
        "Task",
        "Character",
        "Event",
        "Variable",
        "Group",
        "TextOverride",
        "Hint",
        "Synonym",
        # ADRIFT serializes UDFs as <Function> (FileIO.vb); not UserFunction.
        "Function",
        "Map",
    }
)

DEFAULT_PASSWORD = "        "

# Namespace for the per-probe IFIDs derived in _probe_ifid().  Arbitrary but
# fixed: changing it re-stamps every probe.
IFID_NAMESPACE = uuid.uuid5(uuid.NAMESPACE_DNS, "probes.adrift5.scarier.spatterlight")
SCORE_INCREMENT_RE = re.compile(
    r"""Score\s*=\s*"%Score%\s*\+\s*(\d+)""",
    re.IGNORECASE,
)


class Xml2TafError(Exception):
    pass


class _VBMath:
    """Emulate VB.NET Rnd/Randomize used by ADRIFT Dencode."""

    def __init__(self) -> None:
        self._rnd_seed = 0

    def rnd(self, number: float = 1.0) -> float:
        if number != 0.0:
            if number < 0.0:
                inumber = struct.unpack("<i", struct.pack("<f", number))[0]
                i64 = inumber & 0xFFFFFFFF
                self._rnd_seed = (i64 + (i64 >> 24)) & 0xFFFFFF
            self._rnd_seed = (self._rnd_seed * 0x43FD43FD + 0xC39EC3) & 0xFFFFFF
        return self._rnd_seed / 16777216.0

    def randomize(self, number: float) -> None:
        lvalue = struct.unpack("<i", struct.pack("<d", float(number))[4:8])[0]
        lvalue = ((lvalue & 0xFFFF) ^ (lvalue >> 16)) << 8
        self._rnd_seed = (self._rnd_seed & 0xFF0000FF) | lvalue


def dencode(text: str, offset: int) -> bytes:
    """Encode text with ADRIFT's Dencode obfuscation."""
    vb = _VBMath()
    vb.rnd(-1)
    vb.randomize(1976)
    for _ in range(max(0, offset - 1)):
        vb.rnd()

    result = bytearray()
    for ch in text:
        rnd_val = round(vb.rnd() * 255 - 0.5)
        result.append((ord(ch) ^ rnd_val) % 256)
    return bytes(result)


def _obfuscate(data: bytearray) -> None:
    for i, byte in enumerate(data):
        data[i] = byte ^ OBFUSCATION_KEY[i % len(OBFUSCATION_KEY)]


def _item_key(elem: ET.Element) -> str:
    return elem.findtext("Key") or elem.findtext("Name") or ""


def _mark_library(elem: ET.Element) -> None:
    library = elem.find("Library")
    if library is None:
        library = ET.SubElement(elem, "Library")
    library.text = "1"


def _folder_members(folder: ET.Element) -> list[str]:
    return [member.text for member in folder.findall("Member") if member.text]


def _add_folder_member(folder: ET.Element, key: str) -> None:
    if key and key not in _folder_members(folder):
        member = ET.SubElement(folder, "Member")
        member.text = key


def _set_character_property(character: ET.Element, key: str, value: str) -> None:
    for prop in character.findall("Property"):
        if prop.findtext("Key") == key:
            value_elem = prop.find("Value")
            if value_elem is None:
                value_elem = ET.SubElement(prop, "Value")
            value_elem.text = value
            return
    prop = ET.SubElement(character, "Property")
    ET.SubElement(prop, "Key").text = key
    ET.SubElement(prop, "Value").text = value


def _game_location_keys(root: ET.Element) -> list[str]:
    keys: list[str] = []
    for location in root.findall("Location"):
        if location.findtext("Library") == "1":
            continue
        key = location.findtext("Key")
        if key:
            keys.append(key)
    return keys


def _configure_player_start(root: ET.Element) -> None:
    location_keys = _game_location_keys(root)
    if not location_keys:
        return

    for character in root.findall("Character"):
        if character.findtext("Key") != "Player":
            continue
        location_mode = None
        for prop in character.findall("Property"):
            if prop.findtext("Key") == "CharacterLocation":
                location_mode = (prop.findtext("Value") or "").strip()
                break
        if location_mode not in (None, "", "Hidden"):
            return
        _set_character_property(character, "CharacterLocation", "At Location")
        _set_character_property(character, "CharacterAtLocation", location_keys[0])
        return


def _populate_all_locations_group(root: ET.Element) -> None:
    location_keys = [loc.findtext("Key") or "" for loc in root.findall("Location")]
    location_keys = [key for key in location_keys if key]
    if not location_keys:
        return

    for group in root.findall("Group"):
        if group.findtext("Key") != "AllLocations":
            continue
        for member in list(group.findall("Member")):
            group.remove(member)
        for key in location_keys:
            member = ET.SubElement(group, "Member")
            member.text = key
        return


def _set_variable_initial_value(root: ET.Element, key: str, value: str) -> None:
    for variable in root.findall("Variable"):
        if variable.findtext("Key") == key:
            initial = variable.find("InitialValue")
            if initial is None:
                initial = ET.SubElement(variable, "InitialValue")
            initial.text = value
            return


def _compute_max_score(root: ET.Element) -> int:
    total = 0
    for task in root.findall("Task"):
        if task.findtext("Library") == "1":
            continue
        actions = task.find("Actions")
        if actions is None:
            continue
        for action in actions:
            if action.tag != "SetVariable" or not action.text:
                continue
            match = SCORE_INCREMENT_RE.search(action.text)
            if match:
                total += int(match.group(1))
    return total


def _configure_max_score(root: ET.Element) -> None:
    max_score = _compute_max_score(root)
    if max_score <= 0:
        return
    _set_variable_initial_value(root, "MaxScore", str(max_score))


def finalize_adventure(root: ET.Element) -> None:
    """Apply save-time fixes ADRIFT performs when writing a .taf file."""
    _configure_player_start(root)
    _populate_all_locations_group(root)
    _configure_max_score(root)


def embed_libraries(root: ET.Element, library_paths: list[Path]) -> None:
    """Merge library AMF/XML modules into an adventure, tagging items as library content."""
    existing = {
        (child.tag, _item_key(child))
        for child in root
        if child.tag in ITEM_TAGS and _item_key(child)
    }
    game_folders = [folder for folder in root.findall("Folder")]

    for library_path in library_paths:
        try:
            lib_root = ET.parse(library_path).getroot()
        except (ET.ParseError, OSError) as exc:
            raise Xml2TafError(f"cannot read library {library_path}: {exc}") from exc

        for child in lib_root:
            if child.tag not in ITEM_TAGS:
                continue
            key = _item_key(child)
            if not key:
                continue
            item_id = (child.tag, key)
            if item_id in existing:
                continue
            merged = copy.deepcopy(child)
            _mark_library(merged)
            root.append(merged)
            existing.add(item_id)

    root_folder = None
    library_folder = None
    for folder in root.findall("Folder"):
        folder_key = folder.findtext("Key")
        if folder_key == "ROOT":
            root_folder = folder
        elif folder_key == "LibraryFolder":
            library_folder = folder

    if root_folder is not None:
        for game_folder in game_folders:
            _add_folder_member(root_folder, game_folder.findtext("Key") or "")

    if library_folder is not None:
        for game_folder in game_folders:
            _add_folder_member(library_folder, game_folder.findtext("Key") or "")

    finalize_adventure(root)


def _serialize_adventure(
    root: ET.Element,
    *,
    has_bom: bool,
    has_declaration: bool,
) -> str:
    body = ET.tostring(root, encoding="unicode")
    body = body.replace("><", ">\r\n<")
    if has_declaration:
        body = '<?xml version="1.0" encoding="utf-8"?>\r\n' + body
    if has_bom:
        body = "\ufeff" + body
    return body


def _probe_ifid(seed: str) -> str:
    """Deterministic IFID for a probe, shaped like a real ADRIFT 5 one.

    The Runner writes "ADRIFT-500-" followed by a v4 GUID whose first group is
    cut down to four hex digits (all 148 IFID-bearing games in the ADRIFT 5
    corpus are "ADRIFT-500-" + 4-4-4-4-12), so mimic that rather than emitting
    a full UUID.  Deriving it from the probe's name instead of uuid4() keeps a
    no-change rebuild byte-identical, which matters because the .taf files are
    committed and FrankenDrift's FD_CACHE is keyed by their contents.
    """
    digest = uuid.uuid5(IFID_NAMESPACE, seed)
    guid = uuid.UUID(int=digest.int, version=4)
    return f"ADRIFT-500-{str(guid)[4:]}".upper()


def _babel_xml(root: ET.Element, ifid_seed: str) -> str:
    title = root.findtext("Title") or "Untitled"
    author = root.findtext("Author") or "Anonymous"
    ifid = _probe_ifid(ifid_seed)
    return (
        '<ifindex xmlns="http://babel.ifarchive.org/protocol/iFiction/" '
        'version="1.0">'
        "<story>"
        "<identification>"
        f"<ifid>{ifid}</ifid>"
        "<format>adrift</format>"
        "</identification>"
        "<bibliographic>"
        f"<title>{_xml_escape(title)}</title>"
        f"<author>{_xml_escape(author)}</author>"
        "<language>en-GB</language>"
        "</bibliographic>"
        "<cover />"
        "</story>"
        "</ifindex>"
    )


def _xml_escape(text: str) -> str:
    return (
        text.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def xml_to_taf(
    xml: str,
    *,
    library_paths: list[Path],
    password: str = DEFAULT_PASSWORD,
    ifid_seed: str = "",
) -> bytes:
    root, has_bom, has_declaration = _parse_xml(xml)
    embed_libraries(root, library_paths)

    xml_text = _serialize_adventure(
        root,
        has_bom=has_bom,
        has_declaration=has_declaration,
    )
    xml_bytes = xml_text.encode("utf-8")

    compressed = bytearray(zlib.compress(xml_bytes, level=9))
    _obfuscate(compressed)

    babel = _babel_xml(root, ifid_seed or root.findtext("Title") or "Untitled")
    babel = babel.encode("utf-8")
    babel_length = len(babel)
    babel_size = f"{babel_length:04X}"

    while len(password) < 8:
        password += " "
    password_text = password[:4] + "Wild" + password[4:8]

    header = V5_MAGIC
    babel_header = babel_size.encode("ascii") + babel
    password_offset = len(compressed) + 13 + babel_length + 4

    parts = [
        header,
        babel_header,
        bytes(compressed),
        dencode(password_text, password_offset),
        b"\r\n",
    ]
    return b"".join(parts)


def _default_libraries(extra: list[str]) -> list[Path]:
    libraries: list[Path] = []
    if DEFAULT_STANDARD_LIBRARY.exists():
        libraries.append(DEFAULT_STANDARD_LIBRARY)

    seen = {path.resolve() for path in libraries}
    for entry in extra:
        path = Path(entry)
        resolved = path.resolve()
        if resolved not in seen:
            libraries.append(path)
            seen.add(resolved)
    return libraries


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Convert ADRIFT v5 XML to a .taf file.",
    )
    parser.add_argument("input_file", help="path to the XML file")
    parser.add_argument(
        "--output-file",
        "-o",
        help="write .taf to this file instead of deriving the name from the input",
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
    if not library_paths:
        print(
            "error: no libraries found; expected "
            f"{DEFAULT_STANDARD_LIBRARY}",
            file=sys.stderr,
        )
        return 1

    missing = [str(path) for path in library_paths if not path.exists()]
    if missing:
        print(
            "error: library file(s) not found: " + ", ".join(missing),
            file=sys.stderr,
        )
        return 1

    if args.output_file:
        output_path = Path(args.output_file)
    else:
        output_path = input_path.with_suffix(".taf")

    try:
        taf_bytes = xml_to_taf(
            xml,
            library_paths=library_paths,
            password=args.password,
            ifid_seed=output_path.stem,
        )
    except (Xml2TafError, TafError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    try:
        output_path.write_bytes(taf_bytes)
    except OSError as exc:
        print(f"error: cannot write {output_path}: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
