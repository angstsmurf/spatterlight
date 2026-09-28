#!/usr/bin/env python3
"""Bake Scarier's built-in per-game patch into a copy of the .taf itself.

`SCR_ASSUME_PATCHES=1` is unlike the other SCR_ASSUME_* switches: it is a
*data* change, the field-level corrections in sctafpar.cpp's PATCH_TABLE, not
a relaxation of the comparison.  So it is reproducible in the real Windows
Runner -- by making the same edits in the file the Runner loads.  That is what
this writes, so that a patched harness row can have a Runner transcript too.

The edits are read from sctafpar.cpp (harness/patchtable.py), never copied, so
the file this writes cannot drift from what the engine does.  A .taf is edited
as a line array: every untouched line is carried over byte for byte, and only
three shapes of change are made --

  * rewrite one value line (SET / SET_BOOL / STRING);
  * append element(s) to a tagged vector, bumping its count line (ADD);
  * insert the `#Room` line a ROOM_LIST0 gains when its Type goes 0 -> 1.

The result is then re-parsed and diffed against the original parse: the set of
differences has to be exactly the set of edits the table asks for (plus, for a
newly added element, the schema-mandated fields the table leaves at zero).

Usage:
    python3 make_patched_taf.py <in.taf> <out.taf>     # write the patched copy
    python3 make_patched_taf.py --check <in.taf>       # verify only, write nothing
"""
from __future__ import annotations

import os
import re
import sys

import patchtable
from tafschema import SCHEMAS
from taftool import pack as taf_pack
from taftool import unpack as taf_unpack
from tafpretty import Parser, TafPrettyError

MISSING = object()

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GAMES = os.path.join(ROOT, "games")
PATCHED = os.path.join(GAMES, "patched")
SWITCH = "SCR_ASSUME_PATCHES=1"
SOURCES = [os.path.join(HERE, "make_patched_taf.py"),
           os.path.join(HERE, "patchtable.py"),
           patchtable.SCTAFPAR]


class PatchError(Exception):
    pass


def key(parts) -> str:
    return "/".join(str(p) for p in parts)


def get_path(game, path: str):
    node = game
    for part in path.split("/"):
        if part.isdigit() and isinstance(node, list):
            index = int(part)
            if index >= len(node):
                return MISSING
            node = node[index]
        elif isinstance(node, dict):
            if part not in node:
                return MISSING
            node = node[part]
        else:
            return MISSING
    return node


def norm(value):
    return int(value) if isinstance(value, bool) else value


# --------------------------------------------------------------------------
# recording parser: the same walk as tafpretty, plus where each value sits
# --------------------------------------------------------------------------

class Recorder(Parser):
    """Parser that also notes the file line every value was read from."""

    def __init__(self, version: str, plain: bytes):
        super().__init__(version, plain)
        self.path: list = []
        self.terms: dict[str, dict] = {}
        self.vecs: dict[str, dict] = {}

    def _note(self, name, kind, line, value):
        self.terms[key(self.path + [name])] = {
            "line": line, "kind": kind, "value": value}

    def _terminal(self, terminal: str) -> None:
        line = self.stream.i
        super()._terminal(terminal)
        if terminal[0] not in "ZFTE":
            self._note(terminal[1:], terminal[0], line,
                       self._scope.get(terminal[1:]))

    def _special(self, special: str) -> None:
        # a Type 2 room list is one boolean line per room; note each one so a
        # SET_BOOL can rewrite a single room's flag
        line = self.stream.i
        super()._special(special)
        rooms = self._scope.get("Rooms")
        if special in ("{ROOM_LIST0}", "{ROOM_LIST1}") and \
                int(self._scope.get("Type", 0)) == 2 and rooms is not None:
            for index, value in enumerate(rooms):
                self.terms[key(self.path + ["Rooms", index])] = {
                    "line": line + index, "kind": "B", "value": value}

    def _class_ref(self, class_: str) -> None:
        m = re.match(r"<([^>]+)>(.*)$", class_)
        if not m:
            raise TafPrettyError(f"bad class {class_!r}")
        if m.group(1) == "_GAME_" or not m.group(2):
            super()._class_ref(class_)
            return
        self.path.append(m.group(2))
        try:
            super()._class_ref(class_)
        finally:
            self.path.pop()

    def _vector(self, vector: str, *, alternate: bool) -> None:
        element = vector[1:]
        tag = self._repeat_tag(element)
        count_line = self.stream.i
        raw = self._integer()
        count = raw + 1 if alternate else raw
        self._repeat(element, count)
        if tag is not None:
            self.vecs[key(self.path + [tag])] = {
                "count_line": count_line, "raw_count": raw, "count": count,
                "alternate": alternate, "element": element,
                "end": self.stream.i}

    def _repeat(self, element: str, count: int) -> None:
        # tafpretty's _repeat, with the path pushed so terminals land under it
        tag = self._repeat_tag(element)
        if tag is None:
            for _ in range(count):
                self._element(element)
            return

        items: list = []
        parent = self._scope
        self.path.append(tag)
        for index in range(count):
            self.path.append(index)
            if element[0] == "<":
                m = re.match(r"<([^>]+)>", element)
                assert m
                item: dict = {}
                self._scope = item
                self._class_body(m.group(1))
                self._scope = parent
                items.append(item)
            elif element[0] in "$#BM":
                line = self.stream.i
                value = self._read_terminal_value(element)
                self.terms[key(self.path)] = {
                    "line": line, "kind": element[0], "value": value}
                items.append(value)
            else:
                scratch: dict = {}
                self._scope = scratch
                self._element(element)
                self._scope = parent
                items.append(next(iter(scratch.values()))
                             if len(scratch) == 1 else scratch)
            self.path.pop()
        self.path.pop()
        parent[tag] = items


# --------------------------------------------------------------------------
# writing a new vector element
# --------------------------------------------------------------------------

def _elements(list_: str, separators: str):
    next_, n = 0, len(list_)
    while next_ < n:
        while next_ < n and list_[next_] in separators:
            next_ += 1
        if next_ >= n:
            break
        length = 0
        while next_ + length < n and list_[next_ + length] not in separators:
            length += 1
        yield list_[next_:next_ + length]
        next_ += length


def emit_element(version: str, element: str, values: dict) -> list[str]:
    """The file lines for one new vector element, from engine-level values."""
    schema, separator = SCHEMAS[version]
    if element[0] in "$#BM":
        # a scalar vector (a task's command list, say): the element IS the value
        value = values[None]
        if element[0] == "$":
            return [str(value)]
        if element[0] == "#":
            return [str(int(value))]
        if element[0] == "B":
            return ["1" if value else "0"]
        return str(value).split("\n") + [separator]
    m = re.match(r"<([^>]+)>", element)
    if not m:
        raise PatchError(f"cannot write a new {element!r} element")
    descriptor = schema[m.group(1)]

    # The 3.9 task-action Type is stored raw and read back with a +1 above 4;
    # write back the raw the reader would have had to see.
    raw = dict(values)
    if "|V390_TASK_ACTION:Type>4?#Type++|" in descriptor:
        engine_type = int(values.get("Type", 0))
        if engine_type == 5:
            raise PatchError("ADRIFT 3.9 has no task action type 5")
        raw["Type"] = engine_type - 1 if engine_type > 5 else engine_type

    lines: list[str] = []

    def test(expression: str) -> bool:
        if expression.startswith("!"):
            return not test(expression[1:])
        if expression[0] == "#":
            m2 = re.match(r"#([^=]+)=(-?\d+)$", expression)
            if not m2:
                raise PatchError(f"bad test {expression!r}")
            return int(values.get(m2.group(1), 0)) == int(m2.group(2))
        if expression[0] in "B$":
            return bool(values.get(expression[1:]))
        raise PatchError(f"cannot evaluate {expression!r} for a new element")

    def walk(list_: str, separators: str) -> None:
        for element_ in _elements(list_, separators):
            ch = element_[0]
            name = element_[1:]
            if ch == "#":
                lines.append(str(int(raw.get(name, 0))))
            elif ch == "$":
                lines.append(str(values.get(name, "")))
            elif ch == "B":
                lines.append("1" if values.get(name) else "0")
            elif ch == "M":
                lines.extend(str(values.get(name, "")).split("\n"))
                lines.append(separator)
            elif ch in "ZFTE":
                pass
            elif ch == "?":
                m2 = re.match(r"\?([^:]+):(.*)$", element_)
                if not m2:
                    raise PatchError(f"bad expression {element_!r}")
                if test(m2.group(1)):
                    walk(m2.group(2), ",")
            elif ch == "|":
                if element_ != "|V390_TASK_ACTION:Type>4?#Type++|":
                    raise PatchError(f"cannot write past fixup {element_!r}")
            else:
                raise PatchError(f"cannot write element {element_!r}")

    walk(descriptor, " ")
    return lines


# --------------------------------------------------------------------------
# planning the line edits
# --------------------------------------------------------------------------

def line_integer(line: str) -> int:
    m = re.match(r"\s*([+-]?\d+)", line)
    if not m:
        raise PatchError(f"not an integer line: {line!r}")
    return int(m.group(1))


def plan(rec: Recorder, game: dict, lines: list[str], edits: list[dict]):
    """(replacements, insertions, notes) for one game's table entry."""
    replace: dict[int, str] = {}
    insert: dict[int, list[str]] = {}
    notes: list[str] = []
    appends: dict[str, dict[int, dict]] = {}

    for edit in edits:
        path, mode = edit["path"], edit["kind"]
        actual = get_path(game, path)
        if mode == "VERIFY":
            if actual != edit["from_string"]:
                raise PatchError("%s: VERIFY wanted %r, file has %r"
                                 % (path, edit["from_string"], actual))
            continue

        if mode in ("SET", "SET_BOOL", "STRING"):
            want = edit["from_string"] if mode == "STRING" else edit["from_integer"]
            if norm(actual) != want:
                raise PatchError("%s: wanted %r, file has %r"
                                 % (path, want, actual))
            record = rec.terms.get(path)
            if record is None:
                raise PatchError("%s: no file line holds this value" % path)
            at = record["line"]
            to = edit["to_string"] if mode == "STRING" else edit["to_integer"]
            if mode == "SET_BOOL" and record["kind"] != "B":
                raise PatchError("%s: %s cannot rewrite a %r line"
                                 % (path, mode, record["kind"]))
            if mode == "STRING":
                if record["kind"] != "$":
                    raise PatchError("%s: not a single-line string" % path)
                new = to
            elif record["kind"] == "B":
                was = line_integer(lines[at])
                new = "0" if to == 0 else str(was if was else 1)
            elif record["kind"] == "#":
                # the file holds the raw value; the table names the engine one
                delta = int(actual) - line_integer(lines[at])
                new = str(to - delta)
            else:
                raise PatchError("%s: cannot rewrite a %r line"
                                 % (path, record["kind"]))
            if at in replace and replace[at] != new:
                raise PatchError("%s: two edits want line %d" % (path, at))
            replace[at] = new
            notes.append("line %6d  %-44s %s -> %s"
                         % (at + 1, path, lines[at], new))
            continue

        if mode not in ("ADD", "ADD_STRING"):
            raise PatchError("unknown edit mode %r" % mode)
        if actual is not MISSING:
            raise PatchError("%s: ADD, but the file already has %r"
                             % (path, actual))
        value = edit["to_integer"] if mode == "ADD" else edit["to_string"]
        parts = path.split("/")

        # a ROOM_LIST0 that has just been given Type 1 gains one #Room line
        if parts[-1] == "Room" and key(parts[:-1] + ["Type"]) in rec.terms:
            type_record = rec.terms[key(parts[:-1] + ["Type"])]
            if replace.get(type_record["line"]) != "1":
                raise PatchError("%s: only a Type 1 room list holds a Room"
                                 % path)
            insert.setdefault(type_record["line"] + 1, []).append(str(value))
            notes.append("line %6d  %-44s + %s"
                         % (type_record["line"] + 2, path, value))
            continue

        # otherwise: a new element at the end of a tagged vector
        for cut in range(len(parts) - 1, 0, -1):
            if parts[cut].isdigit() and key(parts[:cut]) in rec.vecs:
                break
        else:
            raise PatchError("%s: nothing here to add to" % path)
        vector, index, field = key(parts[:cut]), int(parts[cut]), parts[cut + 1:]
        scalar = rec.vecs[vector]["element"][0] in "$#BM"
        if len(field) != (0 if scalar else 1):
            raise PatchError("%s: can only add a whole scalar, or one field of "
                             "a new element" % path)
        appends.setdefault(vector, {}).setdefault(index, {})[
            field[0] if field else None] = value

    for vector, byindex in sorted(appends.items()):
        record = rec.vecs[vector]
        wanted = list(range(record["count"], record["count"] + len(byindex)))
        if sorted(byindex) != wanted:
            raise PatchError("%s: new elements %s do not follow the %d there"
                             % (vector, sorted(byindex), record["count"]))
        new: list[str] = []
        for index in wanted:
            new += emit_element(rec.version, record["element"], byindex[index])
        insert.setdefault(record["end"], []).extend(new)
        count_at = record["count_line"]
        if count_at in replace:
            raise PatchError("%s: count line already rewritten" % vector)
        replace[count_at] = str(record["raw_count"] + len(byindex))
        notes.append("line %6d  %-44s %s -> %s  (+%d element(s), %d line(s))"
                     % (count_at + 1, vector + "/#", lines[count_at],
                        replace[count_at], len(byindex), len(new)))
    return replace, insert, notes


def apply_edits(lines: list[str], replace: dict, insert: dict) -> list[str]:
    out: list[str] = []
    for at, line in enumerate(lines):
        out += insert.get(at, [])
        out.append(replace.get(at, line))
    out += insert.get(len(lines), [])
    return out


# --------------------------------------------------------------------------
# verification
# --------------------------------------------------------------------------

def diff(a, b, path=(), out=None):
    if a is MISSING and isinstance(b, (dict, list)):
        a = {} if isinstance(b, dict) else []
    if b is MISSING and isinstance(a, (dict, list)):
        b = {} if isinstance(a, dict) else []
    if isinstance(a, dict) and isinstance(b, dict):
        for k in sorted(set(a) | set(b)):
            diff(a.get(k, MISSING), b.get(k, MISSING), path + (k,), out)
    elif isinstance(a, list) and isinstance(b, list):
        for i in range(max(len(a), len(b))):
            diff(a[i] if i < len(a) else MISSING,
                 b[i] if i < len(b) else MISSING, path + (i,), out)
    elif norm(a) != norm(b):
        out.append((key(path), a, b))
    return out


def verify(before: dict, after: dict, edits: list[dict]) -> list[str]:
    wanted = {}
    for edit in edits:
        if edit["kind"] in ("SET", "SET_BOOL"):
            wanted[edit["path"]] = (edit["from_integer"], edit["to_integer"])
        elif edit["kind"] == "STRING":
            wanted[edit["path"]] = (edit["from_string"], edit["to_string"])
        elif edit["kind"] == "ADD":
            wanted[edit["path"]] = (MISSING, edit["to_integer"])
        elif edit["kind"] == "ADD_STRING":
            wanted[edit["path"]] = (MISSING, edit["to_string"])
    notes = []
    seen = set()
    for path, was, now in diff(before, after, out=[]):
        if path in wanted:
            want_was, want_now = wanted[path]
            if norm(was) != norm(want_was) or norm(now) != norm(want_now):
                raise PatchError("%s: patched file has %r -> %r, table asks "
                                 "%r -> %r" % (path, was, now, want_was, want_now))
            seen.add(path)
        elif was is MISSING and now in (0, "", False):
            notes.append("%s: %r (schema default of a new element)" % (path, now))
        else:
            raise PatchError("%s: unasked-for change %r -> %r" % (path, was, now))
    for path in sorted(set(wanted) - seen):
        raise PatchError("%s: the table asks for an edit the file did not take"
                         % path)
    return notes


# --------------------------------------------------------------------------

def parse(version: str, plain: bytes):
    rec = Recorder(version, plain)
    game = rec.parse()
    leftover = rec.stream.remaining()
    if leftover == 1 and rec.stream.get() == "":
        leftover = rec.stream.remaining()
    if leftover:
        raise PatchError("parse left %d unread line(s)" % leftover)
    return rec, game


def make(source: str, out: str | None, quiet: bool = False) -> str:
    version, plain, lead, blk = taf_unpack(source)
    text = plain.decode("latin-1")
    tail = "\r\n" if text.endswith("\r\n") else ("\n" if text.endswith("\n") else "")
    lines = text[:len(text) - len(tail)].split("\r\n")
    if "\r\n".join(lines) + tail != text:
        raise PatchError("line split does not round-trip")

    rec, game = parse(version, plain)
    globals_ = game.get("Globals") or {}
    name = globals_.get("GameName", "")
    author = globals_.get("GameAuthor", "")
    entry = patchtable.find(name, author)
    if entry is None:
        raise PatchError("no patch table entry for %r by %r" % (name, author))

    replace, insert, notes = plan(rec, game, lines, entry["edits"])
    patched = apply_edits(lines, replace, insert)
    new_plain = ("\r\n".join(patched) + tail).encode("latin-1")
    _, after = parse(version, new_plain)
    defaults = verify(game, after, entry["edits"])

    if not quiet:
        print("%s  (ADRIFT %s)" % (source, version))
        print("  %s -- %s" % (entry["array"], entry["summary"]))
        for note in notes:
            print("  " + note)
        for note in defaults:
            print("  also " + note)
    if out:
        size = taf_pack(new_plain, version, out, lead=lead, blk=blk)
        if not quiet:
            print("  wrote %s (%d bytes)" % (out, size))
    return entry["array"]


def game_for(tag: str, taf: str, env):
    """The .taf a wired row is really played on, and the env that goes with it.

    A patched row's correction is *data* -- the field edits in sctafpar.cpp's
    PATCH_TABLE -- so for a Runner comparison it is baked into a copy of the
    file instead of asked of the engine, and both sides then play exactly the
    same game.  Every other row is handed its game unchanged.

    (For a 3.7-3.9 file this is not quite free: that codec's PRNG is advanced
    once per file byte, so the draw the loader leaves behind -- what seeds a
    random event's start turn, scutils.cpp scr_vb_rnd -- moves when the patch
    changes the file's length.  The real Runner reads the same file and moves
    with it, so the two sides still agree; it is the *golden*, taken on the
    original file with the engine patching it, that such a row can differ
    from.  Of the 21 patched rows, 19 play identically either way; TAOT3 and
    The Spirit's Flight are the two whose patch grows the file.)
    """
    source = os.path.join(GAMES, taf)
    if SWITCH not in env:
        return source, list(env)
    out = os.path.join(PATCHED, tag + ".taf")
    newest = max(os.path.getmtime(f) for f in SOURCES + [source])
    if not os.path.exists(out) or os.path.getmtime(out) < newest:
        os.makedirs(PATCHED, exist_ok=True)
        make(source, out, quiet=True)
    return out, [a for a in env if a != SWITCH]


def main(argv: list[str]) -> int:
    check = "--check" in argv
    argv = [a for a in argv if a != "--check"]
    if len(argv) != (1 if check else 2):
        sys.exit(__doc__)
    try:
        make(argv[0], None if check else argv[1])
    except (PatchError, TafPrettyError) as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
