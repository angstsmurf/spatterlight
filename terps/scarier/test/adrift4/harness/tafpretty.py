#!/usr/bin/env python3
"""Schema-aware pretty-printer for ADRIFT 3.7–4.0 .taf files.

Unpacks a .taf (via taftool.unpack), walks the same property descriptors as
sctafpar.cpp, and emits labeled text.  Invent-only version fixups that do not
consume TAF lines are skipped; fixups that mutate values controlling later
reads (notably 3.9 task-action Type++) or that themselves read lines are
applied.

Usage:
    python3 tafpretty.py game.taf              # write pretty text to stdout
    python3 tafpretty.py game.taf -o out.txt   # write to a file
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Any

from tafschema import (
    ROOMLIST_ALL_ROOMS,
    ROOMLIST_NO_ROOMS,
    ROOMLIST_NPC_PART,
    ROOMLIST_ONE_ROOM,
    ROOMLIST_SOME_ROOMS,
    SCHEMAS,
)
from taftool import unpack as taf_unpack


class TafPrettyError(Exception):
    pass


class LineStream:
    def __init__(self, plain: bytes):
        # Match sctaffil slab finalization: CR LF pairs become field breaks.
        text = plain.decode("latin-1")
        if text.endswith("\r\n"):
            text = text[:-2]
        elif text.endswith("\n"):
            text = text[:-1]
        self.lines = text.split("\r\n")
        self.i = 0
        self._last: str | None = None
        self._use_pushback = False

    @property
    def line_no(self) -> int:
        return self.i

    def remaining(self) -> int:
        return len(self.lines) - self.i + (1 if self._use_pushback else 0)

    def get(self) -> str:
        # Mirrors parse_get_taf_string(): pushback re-returns the last line and
        # still advances the line counter (parse_tafline++).
        if self._use_pushback:
            assert self._last is not None
            self._use_pushback = False
            self.i += 1
            return self._last
        if self.i >= len(self.lines):
            raise TafPrettyError(f"out of TAF data at line {self.i}")
        self._last = self.lines[self.i]
        self.i += 1
        return self._last

    def pushback(self) -> None:
        if self._use_pushback or self._last is None:
            raise TafPrettyError("too much pushback")
        self._use_pushback = True
        self.i -= 1


class Parser:
    def __init__(self, version: str, plain: bytes):
        if version not in SCHEMAS:
            raise TafPrettyError(f"unsupported version {version}")
        self.version = version
        self.schema, self.separator = SCHEMAS[version]
        self.stream = LineStream(plain)
        self.game: dict[str, Any] = {}
        self._scope: dict[str, Any] = self.game

    # --- TAF readers -----------------------------------------------------

    def _string(self) -> str:
        return self.stream.get()

    def _integer(self) -> int:
        line = self.stream.get()
        m = re.match(r"\s*([+-]?\d+)", line)
        if not m:
            raise TafPrettyError(
                f"invalid integer {line!r} at line {self.stream.line_no}"
            )
        return int(m.group(1))

    def _boolean(self) -> bool:
        return self._integer() != 0

    def _multiline(self) -> str:
        parts = [self.stream.get()]
        while True:
            line = self.stream.get()
            if line == self.separator:
                break
            parts.append(line)
        return "\n".join(parts)

    # --- property access for conditionals --------------------------------

    def _get(self, name: str, default: Any = None) -> Any:
        return self._scope.get(name, default)

    def _put(self, name: str, value: Any) -> None:
        self._scope[name] = value

    def _global_bool(self, name: str) -> bool:
        globals_ = self.game.get("Globals") or {}
        return bool(globals_.get(name))

    def _room_count(self) -> int:
        rooms = self.game.get("Rooms")
        return len(rooms) if isinstance(rooms, list) else 0

    # --- schema walker ---------------------------------------------------

    def parse(self) -> dict[str, Any]:
        self._class_body("_GAME_")
        return self.game

    def _class_body(self, class_name: str) -> None:
        desc = self.schema.get(class_name)
        if desc is None:
            raise TafPrettyError(f"class not described: {class_name}")
        self._descriptor(desc)

    def _descriptor(self, descriptor: str) -> None:
        self._element_list(descriptor, " ")

    def _element_list(self, list_: str, separators: str) -> None:
        next_ = 0
        n = len(list_)
        while next_ < n:
            while next_ < n and list_[next_] in separators:
                next_ += 1
            if next_ >= n:
                break
            length = 0
            while next_ + length < n and list_[next_ + length] not in separators:
                length += 1
            if length == 0:
                raise TafPrettyError(f"bad list at {list_[next_]!r}")
            element = list_[next_ : next_ + length]
            self._element(element)
            next_ += length

    def _element(self, element: str) -> None:
        ch = element[0]
        if ch == "[":
            self._array(element)
        elif ch == "V":
            self._vector(element, alternate=False)
        elif ch == "W":
            self._vector(element, alternate=True)
        elif ch == "<":
            self._class_ref(element)
        elif ch == "?":
            self._expression(element)
        elif ch == "{":
            self._special(element)
        elif ch == "|":
            self._fixup(element)
        elif ch in "#ZBFT$EMisb":
            self._terminal(element)
        else:
            raise TafPrettyError(f"bad element type {ch!r} in {element!r}")

    def _array(self, array: str) -> None:
        m = re.match(r"\[(\d+)\](.+)$", array)
        if not m:
            raise TafPrettyError(f"bad array {array!r}")
        count = int(m.group(1))
        self._repeat(m.group(2), count)

    def _vector(self, vector: str, *, alternate: bool) -> None:
        count = self._integer()
        if alternate:
            count += 1
        self._repeat(vector[1:], count)

    def _repeat(self, element: str, count: int) -> None:
        """Repeat an element count times, collecting into a named list."""
        tag = self._repeat_tag(element)
        if tag is None:
            for _ in range(count):
                self._element(element)
            return

        items: list[Any] = []
        parent = self._scope
        for _ in range(count):
            if element[0] == "<":
                m = re.match(r"<([^>]+)>", element)
                assert m
                item: dict[str, Any] = {}
                self._scope = item
                self._class_body(m.group(1))
                self._scope = parent
                items.append(item)
            elif element[0] in "$#BM":
                items.append(self._read_terminal_value(element))
            else:
                scratch: dict[str, Any] = {}
                self._scope = scratch
                self._element(element)
                self._scope = parent
                if len(scratch) == 1:
                    items.append(next(iter(scratch.values())))
                else:
                    items.append(scratch)
        parent[tag] = items

    def _repeat_tag(self, element: str) -> str | None:
        if element[0] == "<":
            m = re.match(r"<([^>]+)>(.*)$", element)
            return m.group(2) if m and m.group(2) else None
        if element[0] in "$#BM":
            return element[1:]
        return None

    def _class_ref(self, class_: str) -> None:
        m = re.match(r"<([^>]+)>(.*)$", class_)
        if not m:
            raise TafPrettyError(f"bad class {class_!r}")
        class_name, tag = m.group(1), m.group(2)
        if class_name == "_GAME_" or not tag:
            self._class_body(class_name)
            return
        nested: dict[str, Any] = {}
        self._scope[tag] = nested
        parent = self._scope
        self._scope = nested
        self._class_body(class_name)
        self._scope = parent

    def _expression(self, expression: str) -> None:
        m = re.match(r"\?([^:]+):(.*)$", expression)
        if not m:
            raise TafPrettyError(f"bad expression {expression!r}")
        test, rest = m.group(1), m.group(2)
        if test.startswith("!"):
            present = not self._test(test[1:])
        else:
            present = self._test(test)
        if present:
            self._element_list(rest, ",")

    def _test(self, test: str) -> bool:
        ch = test[0]
        if ch == "B":
            return bool(self._get(test[1:]))
        if ch == "#":
            m = re.match(r"#([^=]+)=(-?\d+)$", test)
            if not m:
                raise TafPrettyError(f"bad = compare {test!r}")
            return int(self._get(m.group(1), 0)) == int(m.group(2))
        if ch == "$":
            val = self._get(test[1:], "")
            return bool(val)
        if ch == "G":
            return self._global_bool(test[1:])
        raise TafPrettyError(f"bad expression {test!r}")

    def _read_terminal_value(self, terminal: str) -> Any:
        ch = terminal[0]
        if ch == "#":
            return self._integer()
        if ch == "Z":
            return 0
        if ch == "B":
            return self._boolean()
        if ch == "F":
            return False
        if ch == "T":
            return True
        if ch == "$":
            return self._string()
        if ch == "E":
            return ""
        if ch == "M":
            return self._multiline()
        if ch == "i":
            return self._integer()
        if ch == "b":
            return self._boolean()
        if ch == "s":
            return self._string()
        raise TafPrettyError(f"bad terminal {terminal!r}")

    def _terminal(self, terminal: str) -> None:
        name = terminal[1:]
        ch = terminal[0]
        value = self._read_terminal_value(terminal)
        # Defaults and ignores: still record so password etc. show up.
        if ch in "ib":
            # Ignored by the engine property store; keep for pretty output.
            self._put(name, value)
        elif ch == "s":
            self._put(name, value)
        elif ch in "ZFTE":
            # Not present in the TAF file — omit from pretty output.
            return
        else:
            self._put(name, value)

    # --- specials --------------------------------------------------------

    def _special(self, special: str) -> None:
        if special == "{V400_RESOURCE}":
            # Offset calculation only; no TAF bytes.
            return
        if special in (
            "{V400_ROOM_EXIT:#Dest_#Var1_#Var2_#Var3}",
            "{V390_V380_ROOM_EXIT:#Dest_#Var1_#Var2_ZVar3}",
        ):
            desc = (
                "#Dest #Var1 #Var2 #Var3"
                if "V400" in special
                else "#Dest #Var1 #Var2"  # ZVar3 invents 0; omit
            )
            flag = self._integer()
            if flag != 0:
                self.stream.pushback()
                # Parse into current exit dict (scope is the exit item).
                self._descriptor(desc)
            else:
                # Mark absent exit for the list slot.
                self._scope.clear()
                self._scope["_absent"] = True
            return
        if special in ("{ROOM_LIST0}", "{ROOM_LIST1}"):
            type_ = int(self._get("Type", 0))
            if type_ in (ROOMLIST_NO_ROOMS, ROOMLIST_ALL_ROOMS, ROOMLIST_NPC_PART):
                return
            if type_ == ROOMLIST_ONE_ROOM:
                self._terminal("#Room")
                return
            if type_ == ROOMLIST_SOME_ROOMS:
                n = self._room_count()
                if special == "{ROOM_LIST1}":
                    n += 1
                self._put("Rooms", [self._boolean() for _ in range(n)])
                return
            raise TafPrettyError(f"bad room list type {type_}")
        if special == "{OBJECT:#Parent}":
            where = self._get("Where") or {}
            if int(where.get("Type", 0)) == ROOMLIST_NPC_PART:
                self._terminal("#Parent")
            return
        if special == "{WALK:#Rooms_#Times}":
            n = int(self._get("NumStops", 0))
            rooms = []
            times = []
            for _ in range(n):
                rooms.append(self._integer())
                times.append(self._integer())
            self._put("Rooms", rooms)
            self._put("Times", times)
            return
        if special == "{ROOM_GROUP:[]BList}":
            n = self._room_count()
            flags = [self._boolean() for _ in range(n)]
            self._put("List", flags)
            self._put("List2", [i for i, v in enumerate(flags) if v])
            return
        raise TafPrettyError(f"no handler for special {special!r}")

    # --- fixups that affect TAF consumption or later conditionals --------

    def _fixup(self, fixup: str) -> None:
        if fixup == "|V390_TASK_ACTION:Type>4?#Type++|":
            t = int(self._get("Type", 0))
            if t > 4:
                self._put("Type", t + 1)
            return
        if fixup == "|V390_TASK_ACTION:$Expr_#Var5|":
            if int(self._get("Var2", 0)) == 5:
                self._descriptor("$Expr")  # ZVar5 omitted (not in file)
            else:
                self._descriptor("#Var5")  # EExpr omitted
            return
        if fixup == "|V400_TASK_RESTR:Type>4?#Var1,#Var2,#Var3|":
            if int(self._get("Type", 0)) > 4:
                self._descriptor("#Var1 #Var2 #Var3")
            return
        if fixup == "|V390_TASK_RESTR:Var1>0?#Var1++|":
            v = int(self._get("Var1", 0))
            if v > 0:
                self._put("Var1", v + 1)
            return
        # All other fixups invent 4.0-shaped properties or remap without
        # reading further TAF lines.  Safe to ignore for a file dump.
        return


def parse_plain(version: str, plain: bytes) -> dict[str, Any]:
    parser = Parser(version, plain)
    game = parser.parse()
    leftover = parser.stream.remaining()
    if leftover:
        # Trailing blank from split edge cases: tolerate a single empty line.
        if leftover == 1 and parser.stream.get() == "":
            leftover = parser.stream.remaining()
        if leftover:
            raise TafPrettyError(
                f"parse left {leftover} unread line(s) "
                f"(at file line ~{parser.stream.line_no})"
            )
    return game


def _format_scalar(value: Any) -> str | None:
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        if "\n" in value:
            return None
        if value == "":
            return '""'
        if value.startswith(" ") or value.endswith(" "):
            return repr(value)
        if re.search(r'[:#{}[\]]', value):
            return repr(value)
        return value
    return repr(value)


def _emit(obj: Any, out: list[str], indent: int = 0, key: str | None = None) -> None:
    pad = "  " * indent
    if isinstance(obj, dict):
        if obj.get("_absent"):
            if key is not None:
                out.append(f"{pad}{key}: -")
            return
        # Skip empty nested dicts (e.g. unused RESOURCE nodes).
        meaningful = {k: v for k, v in obj.items() if not k.startswith("_")}
        if not meaningful:
            if key is not None:
                out.append(f"{pad}{key}: {{}}")
            return
        if key is not None:
            out.append(f"{pad}{key}:")
            ind = indent + 1
        else:
            ind = indent
        for k, v in meaningful.items():
            _emit(v, out, ind, key=k)
        return
    if isinstance(obj, list):
        label = key if key is not None else "List"
        out.append(f"{pad}{label}: ({len(obj)})")
        for i, item in enumerate(obj):
            if isinstance(item, dict):
                if item.get("_absent"):
                    out.append(f"{pad}  [{i}]: -")
                else:
                    out.append(f"{pad}  [{i}]:")
                    for k, v in item.items():
                        if k.startswith("_"):
                            continue
                        _emit(v, out, indent + 2, key=k)
            elif isinstance(item, str) and "\n" in item:
                out.append(f"{pad}  [{i}]: |")
                for line in item.split("\n"):
                    out.append(f"{pad}    {line}")
            else:
                formatted = _format_scalar(item)
                if formatted is None and isinstance(item, str):
                    out.append(f"{pad}  [{i}]: |")
                    for line in item.split("\n"):
                        out.append(f"{pad}    {line}")
                else:
                    out.append(f"{pad}  [{i}]: {formatted}")
        return
    if isinstance(obj, str) and "\n" in obj:
        assert key is not None
        out.append(f"{pad}{key}: |")
        for line in obj.split("\n"):
            out.append(f"{pad}  {line}")
        return
    assert key is not None
    formatted = _format_scalar(obj)
    if formatted is None and isinstance(obj, str):
        out.append(f"{pad}{key}: |")
        for line in obj.split("\n"):
            out.append(f"{pad}  {line}")
    else:
        out.append(f"{pad}{key}: {formatted}")


def pretty_print(version: str, plain: bytes, *, source: str | None = None) -> str:
    game = parse_plain(version, plain)
    lines: list[str] = []
    title = ""
    author = ""
    globals_ = game.get("Globals") or {}
    if isinstance(globals_, dict):
        title = str(globals_.get("GameName") or "")
        author = str(globals_.get("GameAuthor") or "")
    header = f"# ADRIFT {version}"
    if title:
        header += f"  {title}"
    if author:
        header += f"  ({author.strip()})"
    lines.append(header)
    if source:
        lines.append(f"# source: {source}")
    lines.append("")
    _emit(game, lines, 0, key=None)
    lines.append("")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Pretty-print an ADRIFT 3.7–4.0 .taf as labeled text.",
    )
    parser.add_argument("input_file", help="path to the .taf file")
    parser.add_argument(
        "-o",
        "--output-file",
        help="write pretty text to this file instead of stdout",
    )
    args = parser.parse_args(argv)

    input_path = Path(args.input_file)
    try:
        version, plain, _, _ = taf_unpack(str(input_path))
    except (ValueError, OSError) as exc:
        print(f"error: cannot unpack {input_path}: {exc}", file=sys.stderr)
        return 1

    try:
        text = pretty_print(version, plain, source=str(input_path))
    except TafPrettyError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.output_file:
        try:
            Path(args.output_file).write_text(text, encoding="utf-8")
        except OSError as exc:
            print(f"error: cannot write {args.output_file}: {exc}", file=sys.stderr)
            return 1
    else:
        try:
            sys.stdout.write(text)
        except BrokenPipeError:
            return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
