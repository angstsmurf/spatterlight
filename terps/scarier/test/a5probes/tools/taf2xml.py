#!/usr/bin/env python3
"""Convert ADRIFT v5 .taf files to XML.

Decompression logic matches FrankenDrift FileIO.vb and adrift-5-rs/src/taf/mod.rs.
"""

from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

V5_MAGIC = bytes([60, 66, 63, 201, 106, 135, 194, 207, 146, 69, 62, 97])
V4_MAGIC = bytes([60, 66, 63, 201, 106, 135, 194, 207, 147, 69, 62, 97])
V39_MAGIC = bytes([60, 66, 63, 201, 106, 135, 194, 207, 148, 69, 55, 97])

# XOR key from FrankenDrift FileIO.vb ObfuscateByteArray.
OBFUSCATION_KEY = bytes(
    [
        41, 236, 221, 117, 23, 189, 44, 187, 161, 96, 4, 147, 90, 91, 172, 159, 244, 50, 249, 140,
        190, 244, 82, 111, 170, 217, 13, 207, 25, 177, 18, 4, 3, 221, 160, 209, 253, 69, 131, 37,
        132, 244, 21, 4, 39, 87, 56, 203, 119, 139, 231, 180, 190, 13, 213, 53, 153, 109, 202, 62,
        175, 93, 161, 239, 77, 0, 143, 124, 186, 219, 161, 175, 175, 212, 7, 202, 223, 77, 72, 83,
        160, 66, 88, 142, 202, 93, 70, 246, 8, 107, 55, 144, 122, 68, 117, 39, 83, 37, 183, 39,
        199, 188, 16, 155, 233, 55, 5, 234, 6, 11, 86, 76, 36, 118, 158, 109, 5, 19, 36, 239, 185,
        153, 115, 79, 164, 17, 52, 106, 94, 224, 118, 185, 150, 33, 139, 228, 49, 188, 164, 146, 88,
        91, 240, 253, 21, 234, 107, 3, 166, 7, 33, 63, 0, 199, 109, 46, 72, 193, 246, 216, 3, 154,
        139, 37, 148, 156, 182, 3, 235, 185, 60, 73, 111, 145, 151, 94, 169, 118, 57, 186, 165, 48,
        195, 86, 190, 55, 184, 206, 180, 93, 155, 111, 197, 203, 143, 189, 208, 202, 105, 121, 51,
        104, 24, 237, 203, 216, 208, 111, 48, 15, 132, 210, 136, 60, 51, 211, 215, 52, 102, 92, 227,
        232, 79, 142, 29, 204, 131, 163, 2, 217, 141, 223, 12, 192, 134, 61, 23, 214, 139, 230, 102,
        73, 158, 165, 216, 201, 231, 137, 152, 187, 230, 155, 99, 12, 149, 75, 25, 138, 207, 254, 85,
        44, 108, 86, 129, 165, 197, 200, 182, 245, 187, 1, 169, 128, 245, 153, 74, 170, 181, 83, 229,
        250, 11, 70, 243, 242, 123, 0, 42, 58, 35, 141, 6, 140, 145, 58, 221, 71, 35, 51, 4, 30, 210,
        162, 0, 229, 241, 227, 22, 252, 1, 110, 212, 123, 24, 90, 32, 37, 99, 142, 42, 196, 158, 123,
        209, 45, 250, 28, 238, 187, 188, 3, 134, 130, 79, 199, 39, 105, 70, 14, 0, 151, 234, 46, 56,
        181, 185, 138, 115, 54, 25, 183, 227, 149, 9, 63, 128, 87, 208, 210, 234, 213, 244, 91, 63,
        254, 232, 81, 44, 81, 51, 183, 222, 85, 142, 146, 218, 112, 66, 28, 116, 111, 168, 184, 161,
        4, 31, 241, 121, 15, 70, 208, 152, 116, 35, 43, 163, 142, 238, 58, 204, 103, 94, 34, 2, 97,
        217, 142, 6, 119, 100, 16, 20, 179, 94, 122, 44, 59, 185, 58, 223, 247, 216, 28, 11, 99, 31,
        105, 49, 98, 238, 75, 129, 8, 80, 12, 17, 134, 181, 63, 43, 145, 234, 2, 170, 54, 188, 228,
        22, 168, 255, 103, 213, 180, 91, 213, 143, 65, 23, 159, 66, 111, 92, 164, 136, 25, 143, 11, 99,
        81, 105, 165, 133, 121, 14, 77, 12, 213, 114, 213, 166, 58, 83, 136, 99, 135, 118, 205, 173,
        123, 124, 207, 111, 22, 253, 188, 52, 70, 122, 145, 167, 176, 129, 196, 63, 89, 225, 91, 165,
        13, 200, 185, 207, 65, 248, 8, 27, 211, 64, 1, 162, 193, 94, 231, 213, 153, 53, 111, 124, 81,
        25, 198, 91, 224, 45, 246, 184, 142, 73, 9, 165, 26, 39, 159, 178, 194, 0, 45, 29, 245, 161,
        97, 5, 120, 238, 229, 81, 153, 239, 165, 35, 114, 223, 83, 244, 1, 94, 238, 20, 2, 79, 140,
        137, 54, 91, 136, 153, 190, 53, 18, 153, 8, 81, 135, 176, 184, 193, 226, 242, 72, 164, 30, 159,
        164, 230, 51, 58, 212, 171, 176, 100, 17, 25, 27, 165, 20, 215, 206, 29, 102, 75, 147, 100, 221,
        11, 27, 32, 88, 162, 59, 64, 123, 252, 203, 93, 48, 237, 229, 80, 40, 77, 197, 18, 132, 173,
        136, 238, 54, 225, 156, 225, 242, 197, 140, 252, 17, 185, 193, 153, 202, 19, 226, 49, 112, 111,
        232, 20, 78, 190, 117, 38, 242, 125, 244, 24, 134, 128, 224, 47, 130, 45, 234, 119, 6, 90, 78,
        182, 112, 206, 76, 118, 43, 75, 134, 20, 107, 147, 162, 20, 197, 116, 160, 119, 107, 117, 238,
        116, 208, 115, 118, 144, 217, 146, 22, 156, 41, 107, 43, 21, 33, 50, 163, 127, 114, 254, 251,
        166, 247, 223, 173, 242, 222, 203, 106, 14, 141, 114, 11, 145, 107, 217, 229, 253, 88, 187, 156,
        153, 53, 233, 235, 255, 104, 141, 243, 146, 209, 33, 5, 109, 122, 72, 125, 240, 198, 131, 178,
        14, 40, 8, 15, 182, 95, 153, 169, 71, 77, 166, 38, 182, 97, 97, 113, 13, 244, 173, 138, 80, 215,
        215, 61, 107, 108, 157, 22, 35, 91, 244, 55, 213, 8, 142, 113, 44, 217, 52, 159, 206, 228, 171,
        68, 42, 250, 78, 11, 24, 215, 112, 252, 24, 249, 97, 54, 80, 202, 164, 74, 194, 131, 133, 235,
        88, 110, 81, 173, 211, 240, 68, 51, 191, 13, 187, 108, 44, 147, 18, 113, 30, 146, 253, 76, 235,
        247, 30, 219, 167, 88, 32, 97, 53, 234, 221, 75, 94, 192, 236, 188, 169, 160, 56, 40, 146, 60,
        61, 10, 62, 245, 10, 189, 184, 50, 43, 47, 133, 57, 0, 97, 80, 117, 6, 122, 207, 226, 253, 212,
        48, 112, 14, 108, 166, 86, 199, 125, 89, 213, 185, 174, 186, 20, 157, 178, 78, 99, 169, 2, 191,
        173, 197, 36, 191, 139, 107, 52, 154, 190, 88, 175, 63, 105, 218, 206, 230, 157, 22, 98, 107,
        174, 214, 175, 127, 81, 166, 60, 215, 84, 44, 107, 57, 251, 21, 130, 170, 233, 172, 27, 234,
        147, 227, 155, 125, 10, 111, 80, 57, 207, 203, 176, 77, 71, 151, 16, 215, 22, 165, 110, 228,
        47, 92, 69, 145, 236, 118, 68, 84, 88, 35, 252, 241, 250, 119, 215, 203, 59, 50, 117, 225, 86,
        2, 8, 137, 124, 30, 242, 99, 4, 171, 148, 68, 61, 55, 186, 55, 157, 9, 144, 147, 43, 252, 225,
        171, 206, 190, 83, 207, 191, 68, 155, 227, 47, 140, 142, 45, 84, 188, 20,
    ]
)


class TafError(Exception):
    pass


def _version_string(header: bytes) -> str:
    if header == V5_MAGIC:
        return "Version 5.00"
    if header == V4_MAGIC:
        return "Version 4.00"
    if header == V39_MAGIC:
        return "Version 3.90"
    return header.decode("utf-8", errors="replace")


def _deobfuscate(data: bytearray) -> None:
    for i, byte in enumerate(data):
        data[i] = byte ^ OBFUSCATION_KEY[i % len(OBFUSCATION_KEY)]


def taf_to_xml(data: bytes) -> str:
    if len(data) < 12:
        raise TafError("not an ADRIFT text adventure file")

    version_str = _version_string(data[0:12])
    if not version_str.startswith("Version "):
        raise TafError("not an ADRIFT text adventure file")

    try:
        header_version = float(version_str.replace("Version ", ""))
    except ValueError as exc:
        raise TafError(f"unsupported ADRIFT version: {version_str}") from exc

    if header_version < 5.0:
        raise TafError(f"unsupported ADRIFT version: {version_str}")

    babel_extra = 0
    obfuscate = True
    comp_start = 12

    if len(data) >= 24:
        size_chunk = data[12:16].decode("utf-8", errors="replace")
        check_chunk = data[16:24].decode("utf-8", errors="replace")
        # FrankenDrift: sSize = "0000" OrElse sCheck = "<ifindex".
        # Also accept "<?xml" — IFmd often includes an XML declaration.
        if (
            size_chunk == "0000"
            or check_chunk.startswith("<ifindex")
            or check_chunk.startswith("<?xml")
        ):
            babel_len = int(size_chunk.strip(), 16)
            babel_extra = babel_len + 4
            comp_start = 16 + babel_len
        else:
            comp_start = 12
            obfuscate = False

    comp_len = len(data) - 26 - babel_extra
    if comp_len <= 0 or comp_start + comp_len > len(data):
        raise TafError("invalid ADRIFT text adventure file")

    compressed = bytearray(data[comp_start : comp_start + comp_len])
    if obfuscate:
        _deobfuscate(compressed)

    try:
        xml_bytes = zlib.decompress(bytes(compressed))
    except zlib.error as exc:
        raise TafError(f"decompression failed: {exc}") from exc

    return xml_bytes.decode("utf-8", errors="replace")


def _parse_xml(xml: str) -> tuple[ET.Element, bool, bool]:
    has_bom = xml.startswith("\ufeff")
    body = xml[1:] if has_bom else xml
    has_declaration = body.lstrip().startswith("<?xml")

    try:
        root = ET.fromstring(body)
    except ET.ParseError as exc:
        raise TafError(f"invalid XML: {exc}") from exc

    return root, has_bom, has_declaration


def _keep_library_item(child: ET.Element) -> bool:
    """Return True if a library-stamped item still holds adventure-specific state.

    Group memberships (DarkLocations, LightSources, …) are often left on
    ``<Library>1</Library>`` items. ``AllLocations`` is excluded: its members are
    rebuilt from every location on save.
    """
    if child.tag != "Group":
        return False
    if child.findtext("Key") == "AllLocations":
        return False
    return child.find("Member") is not None


def strip_library_items(root: ET.Element) -> None:
    """Remove stock library items, keeping groups that list adventure members."""
    for child in list(root):
        if child.findtext("Library") != "1":
            continue
        if _keep_library_item(child):
            continue
        root.remove(child)


def _serialize_xml(
    root: ET.Element,
    *,
    has_bom: bool,
    has_declaration: bool,
    pretty: bool,
) -> str:
    if pretty:
        ET.indent(root, space="  ")
    result = ET.tostring(root, encoding="unicode")
    if has_declaration:
        result = '<?xml version="1.0" encoding="utf-8"?>\n' + result
    if has_bom:
        result = "\ufeff" + result
    return result


def format_xml(xml: str, *, include_library: bool, pretty: bool) -> str:
    if include_library and not pretty:
        return xml

    root, has_bom, has_declaration = _parse_xml(xml)
    if not include_library:
        strip_library_items(root)
    return _serialize_xml(
        root,
        has_bom=has_bom,
        has_declaration=has_declaration,
        pretty=pretty,
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Convert an ADRIFT v5 .taf file to XML.",
    )
    parser.add_argument("input_file", help="path to the .taf file")
    parser.add_argument(
        "--output-file",
        "-o",
        help="write XML to this file instead of stdout",
    )
    parser.add_argument(
        "--pretty-print",
        action="store_true",
        help="indent nested tags with two spaces",
    )
    parser.add_argument(
        "--include-library",
        action="store_true",
        help=(
            "include all standard-library items (by default they are stripped, "
            "except groups that still list adventure members)"
        ),
    )
    args = parser.parse_args(argv)

    input_path = Path(args.input_file)
    try:
        data = input_path.read_bytes()
    except OSError as exc:
        print(f"error: cannot read {input_path}: {exc}", file=sys.stderr)
        return 1

    try:
        xml = format_xml(
            taf_to_xml(data),
            include_library=args.include_library,
            pretty=args.pretty_print,
        )
    except TafError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.output_file:
        try:
            Path(args.output_file).write_text(xml, encoding="utf-8")
        except OSError as exc:
            print(f"error: cannot write {args.output_file}: {exc}", file=sys.stderr)
            return 1
    else:
        try:
            sys.stdout.write(xml)
        except BrokenPipeError:
            return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
