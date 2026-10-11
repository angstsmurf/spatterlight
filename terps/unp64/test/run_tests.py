#!/usr/bin/env python3
"""Regression tests for unp64: does every sample still unpack to the same bytes?

  run_tests.py [ID-PREFIX...]   check (all samples, or the named ones)
  run_tests.py --bless          rewrite expected.tsv from the samples
  run_tests.py --list           one line per row: status, id, source
  run_tests.py --trace          one line per sample: id, hash of every CPU
                                step, number of steps, result (unp64_trace)
  run_tests.py --chain          check that the -c switch (unpack the output
                                again, until that fails) gives what calling
                                unp64() again on its own output gives

A sample is one program that unp64() was handed while the Scott and Taylor
test suites loaded their C64 images (capture.sh), with the switches it was
handed with: samples/<id>.prg and samples/<id>.sw. expected.tsv has one row per
sample: what it unpacks to (length and SHA-256), or that unp64() fails on it,
which is how the callers find out that they have peeled off the last layer.

The samples are cut out of copyrighted games and not in the repository. Rows
whose sample is not in samples/ are skipped, so the suite passes (testing
nothing) on a bare checkout. See README.md.
"""

import argparse
import concurrent.futures
import hashlib
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROBE = os.path.join(HERE, "unp64_probe")
SAMPLES = os.path.join(HERE, "samples")
EXPECTED = os.path.join(HERE, "expected.tsv")
HEADER = "# id\tswitches\tinput sha256\tresult\toutput length\toutput sha256\tseen in\n"
TIMEOUT = 120


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def read_expected():
    rows = {}
    if not os.path.exists(EXPECTED):
        return rows
    with open(EXPECTED, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            if len(fields) != 7:
                sys.exit(f"{EXPECTED}:{n}: expected 7 columns, got {len(fields)}")
            rows[fields[0]] = fields
    return rows


def sample_switches(sample_id):
    path = os.path.join(SAMPLES, sample_id + ".sw")
    if not os.path.exists(path):
        return ""
    with open(path, encoding="latin-1") as f:
        return f.read()


def sources():
    """id -> the images the sample was seen in (samples/log.tsv)."""
    seen = {}
    path = os.path.join(SAMPLES, "log.tsv")
    if os.path.exists(path):
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                fields = line.rstrip("\n").split("\t")
                if len(fields) == 2:
                    seen.setdefault(fields[0], set()).add(fields[1])
    return seen


def unpack(sample_id):
    """The middle columns of a row: switches, input hash, result, length, hash."""
    path = os.path.join(SAMPLES, sample_id + ".prg")
    switches = sample_switches(sample_id)
    with open(path, "rb") as f:
        input_hash = sha256(f.read())
    try:
        done = subprocess.run([PROBE, path] + ([switches] if switches else []),
                              capture_output=True, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        return [switches or "-", input_hash, "timeout", "-", "-"]
    if done.returncode == 0:
        return [switches or "-", input_hash, "ok", str(len(done.stdout)), sha256(done.stdout)]
    if done.returncode == 1:
        return [switches or "-", input_hash, "fail", "-", "-"]
    return [switches or "-", input_hash, f"crash({done.returncode})", "-", "-"]


def run_probe(data, switches):
    """Unpack bytes: the output, or None if unp64() fails."""
    done = subprocess.run([PROBE, "/dev/stdin"] + ([switches] if switches else []),
                          input=data, capture_output=True, timeout=TIMEOUT)
    return done.stdout if done.returncode == 0 else None


def chain(prefixes):
    """-c against doing the same by hand, one call per layer."""
    have = sorted(name[:-4] for name in os.listdir(SAMPLES) if name.endswith(".prg")) \
        if os.path.isdir(SAMPLES) else []
    have = [i for i in have if not prefixes or any(i.startswith(p) for p in prefixes)]

    def one(sample_id):
        switches = sample_switches(sample_id)
        with open(os.path.join(SAMPLES, sample_id + ".prg"), "rb") as f:
            data = f.read()
        at_once = run_probe(data, (switches + " -c").strip())
        by_hand = None
        layers = 0
        while layers < 16:
            unpacked = run_probe(data, switches if layers == 0 else "")
            if unpacked is None:
                break
            by_hand = data = unpacked
            layers += 1
        return at_once == by_hand, layers

    failed = deep = 0
    with concurrent.futures.ThreadPoolExecutor() as pool:
        for sample_id, (same, layers) in zip(have, pool.map(one, have)):
            deep += layers > 1
            if not same:
                failed += 1
                print(f"FAIL {sample_id}  -c differs from {layers} calls in a row")
    print(f"unp64 -c tests: {len(have) - failed} passed, {failed} failed"
          f" ({deep} samples of more than one layer)")
    return 1 if failed else 0


def trace(prefixes):
    """Print the step hash of every sample; diff the output of two builds."""
    tracer = os.path.join(HERE, "unp64_trace")
    if not os.path.exists(tracer):
        sys.exit("unp64_trace is not built: make unp64_trace")
    have = sorted(name[:-4] for name in os.listdir(SAMPLES) if name.endswith(".prg")) \
        if os.path.isdir(SAMPLES) else []
    have = [i for i in have if not prefixes or any(i.startswith(p) for p in prefixes)]

    def one(sample_id):
        switches = sample_switches(sample_id)
        done = subprocess.run([tracer, os.path.join(SAMPLES, sample_id + ".prg")]
                              + ([switches] if switches else []),
                              capture_output=True, timeout=TIMEOUT)
        return done.stdout.decode().strip() or f"crash({done.returncode})"

    with concurrent.futures.ThreadPoolExecutor() as pool:
        for sample_id, line in zip(have, pool.map(one, have)):
            print(f"{sample_id}\t{line}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ids", nargs="*", metavar="ID-PREFIX")
    ap.add_argument("--bless", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--trace", action="store_true")
    ap.add_argument("--chain", action="store_true")
    opt = ap.parse_args()

    if opt.trace:
        return trace(opt.ids)
    if not os.path.exists(PROBE):
        sys.exit("unp64_probe is not built: make unp64_probe")
    if opt.chain:
        return chain(opt.ids)

    have = sorted(name[:-4] for name in os.listdir(SAMPLES) if name.endswith(".prg")) \
        if os.path.isdir(SAMPLES) else []
    expected = read_expected()

    if opt.bless:
        if not have:
            sys.exit("no samples in samples/: sh capture.sh")
        seen = sources()
        with concurrent.futures.ThreadPoolExecutor() as pool:
            results = list(pool.map(unpack, have))
        rows = dict(expected)
        for sample_id, result in zip(have, results):
            note = ", ".join(sorted(seen.get(sample_id, []))) or \
                (expected[sample_id][6] if sample_id in expected else "-")
            rows[sample_id] = [sample_id] + result + [note]
        with open(EXPECTED, "w", encoding="utf-8") as f:
            f.write(HEADER)
            for fields in sorted(rows.values(), key=lambda r: (r[6], r[0])):
                f.write("\t".join(fields) + "\n")
        print(f"unp64 tests: {len(have)} rows blessed, {len(rows)} in expected.tsv")
        return 0

    wanted = [i for i in sorted(set(have) | set(expected))
              if not opt.ids or any(i.startswith(p) for p in opt.ids)]
    todo = [i for i in wanted if i in have and i in expected]
    with concurrent.futures.ThreadPoolExecutor() as pool:
        results = dict(zip(todo, pool.map(unpack, todo)))

    passed = failed = skipped = unknown = 0
    for sample_id in wanted:
        if sample_id not in expected:
            status = "NEW"
            unknown += 1
        elif sample_id not in have:
            status = "skip"
            skipped += 1
        elif results[sample_id] == expected[sample_id][1:6]:
            status = "ok"
            passed += 1
        else:
            status = "FAIL"
            failed += 1
        note = expected[sample_id][6] if sample_id in expected else "not in expected.tsv"
        if opt.list or status in ("FAIL", "NEW"):
            print(f"{status:5}{sample_id}  {note}")
        if status == "FAIL":
            names = HEADER[2:].rstrip("\n").split("\t")[1:6]
            for name, want, got in zip(names, expected[sample_id][1:6], results[sample_id]):
                if want != got:
                    print(f"       {name}: expected {want}, got {got}")

    summary = f"unp64 tests: {passed} passed, {failed} failed"
    if skipped:
        summary += f", {skipped} skipped (sample not in samples/)"
    if unknown:
        summary += f", {unknown} samples without a row (make bless)"
    print(summary)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
