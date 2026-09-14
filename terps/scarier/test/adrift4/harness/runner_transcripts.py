#!/usr/bin/env python3
"""Build test/adrift4/runner_transcripts/: one real-Runner transcript per wired
walkthrough row, driven under vbrng xoshiro with the golden's own feed, seed,
popup answers and file version.

    python3 harness/runner_transcripts.py plan      # feeds + plan.tsv for all rows
    python3 harness/runner_transcripts.py harvest   # reuse xoshiro captures that already match
    python3 harness/runner_transcripts.py jobs [out] # xoshiro_par.sh job file for the rest
    python3 harness/runner_transcripts.py collect   # compare fresh drives, copy, manifest

Per row:
  * Runner  -- by .taf header bytes 8-10: 4.00 run400x, 3.90 run390x,
               3.80 run380x, 3.70 run370x.
  * seed    -- the row's SCR_SEED, else 1234 (Scarier's xoshiro default,
               scinterf.cpp; vbrng's default too).
  * feed    -- make_wine_cmdfile.py with SCR_RNG=xoshiro (the harness's own
               default, run_v4_walkthroughs.sh), written to
               ~/adrift-battle/runner/wine/runner_transcripts_cmds/<tag>.txt;
               its PRE and POPUP_ANSWERS go into the job row.
  * env     -- the row's SCR_* settings are applied to the compare.  The Runner
               has no equivalent of SCR_SKIP_WAITKEY (the feed carries the
               pause answers instead) or of SCR_ASSUME_COMBAT/_MOVES.

"harvest" reuses a transcript only when a session log records that
xoshiro_par.sh drove it with the row's current seed AND it compares identical
against the feed rebuilt from the current golden -- anything else is re-driven.
Drive the job file with
    LOAD_SLEEP=600 VBRNG_TRACE_OFF=1 ./xoshiro_par.sh <jobs> 5
(gmylm's 15 MB .taf needs the long load cap).  Never rm a glob in the pfx.
"""
import concurrent.futures
import filecmp
import glob
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "runner_transcripts")
WINE = os.path.expanduser("~/adrift-battle/runner/wine")
PFX = os.path.join(WINE, "pfx", "drive_c", "adrift")
FEEDS_REL = "runner_transcripts_cmds"
FEEDS = os.path.join(WINE, FEEDS_REL)
PLAN = os.path.join(OUT, "plan.tsv")
MANIFEST = os.path.join(OUT, "manifest.tsv")
SESSIONS = os.path.expanduser("~/.claude/projects/-Users-administrator-spatterlight")
JOB_SUFFIX = "_rt"

EXES = {b"\x93\x45\x3e": ("4.00", "run400x.exe"),
        b"\x94\x45\x37": ("3.90", "run390x.exe"),
        b"\x94\x45\x36": ("3.80", "run380x.exe"),
        b"\x94\x45\x39": ("3.70", "run370x.exe")}
PLAN_FIELDS = ["tag", "taf", "version", "exe", "seed", "pre", "popups", "env", "marker", "draws"]
NATIVE_CORPUS = os.path.join(WINE, "transcripts_v4_corpus_2026-09-08")


def rows():
    with open(os.path.join(HERE, "run_v4_walkthroughs.sh"), encoding="latin-1") as fh:
        for line in fh:
            if re.match(r"^[^#\s][^|]*_solution\.txt\|", line):
                fields = line.rstrip("\n").split("|")
                env = [a for field in fields[3:] for a in field.split()]
                yield fields[0][:-len("_solution.txt")], fields[1], fields[2], env


def read_plan():
    with open(PLAN, encoding="utf-8") as fh:
        head = fh.readline().rstrip("\n").split("\t")
        return [dict(zip(head, l.rstrip("\n").split("\t"))) for l in fh if l.strip()]


def plan_one(row):
    tag, taf, marker, env = row
    with open(os.path.join(ROOT, "games", taf), "rb") as fh:
        version, exe = EXES[fh.read(11)[8:11]]
    seed = "1234"
    for assignment in env:
        if assignment.startswith("SCR_SEED="):
            seed = assignment.split("=", 1)[1]
    run_env = dict(os.environ, SCR_RNG="xoshiro")
    done = subprocess.run([sys.executable, os.path.join(HERE, "make_wine_cmdfile.py"),
                           tag, os.path.join(FEEDS, tag + ".txt")],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=run_env)
    text = done.stdout.decode("latin-1")
    pre = re.search(r"PRE=(\d+)", text)
    popups = re.search(r'POPUP_ANSWERS="([^"]*)"', text)
    if done.returncode or not pre:
        sys.exit("make_wine_cmdfile.py failed for %s:\n%s" % (tag, text))
    # How many xoshiro draws the golden's route makes.  Zero means the seed
    # cannot reach the transcript, so a native-RNG capture is as good as a
    # xoshiro one (see harvest).
    draw_env = dict(run_env, SCR_SEED=seed, SCR_TRACE_RAND="1")
    for assignment in env:
        name, _, value = assignment.partition("=")
        draw_env[name] = value
    with open(os.path.join(ROOT, "goldens", tag + "_solution.txt"), "rb") as fh:
        trace = subprocess.run([os.path.join(HERE, "scare"), os.path.join(ROOT, "games", taf)],
                               stdin=fh, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                               env=draw_env).stderr
    return {"tag": tag, "taf": taf, "version": version, "exe": exe, "seed": seed,
            "pre": pre.group(1), "popups": popups.group(1).replace("|", "~") if popups else "",
            "env": " ".join(env), "marker": marker,
            "draws": str(len(re.findall(rb"RND #", trace)))}


def plan():
    os.makedirs(FEEDS, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        planned = list(pool.map(plan_one, rows()))
    with open(PLAN, "w", encoding="utf-8") as fh:
        fh.write("\t".join(PLAN_FIELDS) + "\n")
        for p in planned:
            fh.write("\t".join(p[k] for k in PLAN_FIELDS) + "\n")
    print("planned %d rows -> %s" % (len(planned), PLAN))


def compare(p, transcript):
    """(identical?, verdict line, full report) for one transcript."""
    args = [sys.executable, os.path.join(HERE, "compare_wine_transcript.py"),
            "--taf", os.path.join(ROOT, "games", p["taf"]),
            "--feed", os.path.join(FEEDS, p["tag"] + ".txt"),
            "--runner", transcript, "--limit", "10",
            "--env", "SCR_RNG=xoshiro", "--env", "SCR_SEED=" + p["seed"]]
    for assignment in p["env"].split():
        if not assignment.startswith("SCR_SEED="):
            args += ["--env", assignment]
    for answer in p["popups"].split("~") if p["popups"] else []:
        args += ["--popup", answer]
    done = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    report = done.stdout.decode("utf-8", "replace")
    if done.returncode == 0:
        verdict = [l for l in report.splitlines() if l.startswith("identical on every turn")][-1]
    else:
        lost = re.search(r"RULE 2 -- (\d+) command\(s\) the Runner never echoed", report)
        pausecount = re.search(r'The (\d+) "lost" command\(s\) below are a PAUSE-COUNT', report)
        turns = sum(1 for l in report.splitlines()
                    if re.match(r"turn \d+ ", l) and "re-synchronised" not in l
                    and "WHITESPACE ONLY" not in l and "keypress prompt" not in l)
        stopped = "+" if "... stopping after" in report else ""
        verdict = "%s%s differing turn(s)" % (turns, stopped)
        if lost:
            verdict += ", %s lost command(s)" % lost.group(1)
        if pausecount:
            verdict += ", %s pause-count realignment(s)" % pausecount.group(1)
    return done.returncode == 0, verdict, report


def plain_text(path):
    with open(path, "rb") as fh:
        raw = fh.read()
    if raw.startswith(b"{\\rtf"):
        raw = subprocess.run(["textutil", "-convert", "txt", "-stdout", path],
                             stdout=subprocess.PIPE).stdout
    return raw.decode("latin-1")


def has_marker(path, marker):
    squash = lambda s: re.sub(r"\s+", " ", s)
    return squash(marker) in squash(plain_text(path))


def provenance():
    """{transcript file: (job tag, seed)} for every xoshiro_par.sh run a session log saw."""
    pattern = re.compile(r"(?<![A-Za-z0-9_\\])([A-Za-z0-9_]+): rc=0 transcript=(Adrift_\d+_[A-Za-z0-9_]+\.(?:txt|rtf))"
                         r" seed=(\d+) -- DONE \d+ commands")
    runs = {}
    for log in glob.glob(os.path.join(SESSIONS, "*.jsonl")):
        with open(log, encoding="utf-8", errors="replace") as fh:
            # tool output sits JSON-escaped in the log: unescape newlines and
            # tabs, or the `n` of `\n` / `t` of `\t` glues onto the job tag
            text = fh.read().replace("\\n", "\n").replace("\\t", "\t")
            for m in pattern.finditer(text):
                runs[m.group(2)] = (m.group(1), m.group(3))
    return runs


def write_manifest(entries):
    fields = ["tag", "file", "source", "version", "exe", "seed", "pre", "popups", "env",
              "win_marker", "verdict"]
    with open(MANIFEST, "w", encoding="utf-8") as fh:
        fh.write("\t".join(fields) + "\n")
        for tag in sorted(entries):
            fh.write("\t".join(entries[tag].get(k, "") for k in fields) + "\n")


def read_manifest():
    if not os.path.exists(MANIFEST):
        return {}
    with open(MANIFEST, encoding="utf-8") as fh:
        head = fh.readline().rstrip("\n").split("\t")
        return {d["tag"]: d for d in
                (dict(zip(head, l.rstrip("\n").split("\t"))) for l in fh if l.strip())}


def adopt(p, transcript, source, identical, verdict, report, entries):
    ext = os.path.splitext(transcript)[1]
    for old in (".txt", ".rtf"):
        stale = os.path.join(OUT, p["tag"] + old)
        if os.path.exists(stale):
            os.remove(stale)
    name = p["tag"] + ext
    shutil.copy2(transcript, os.path.join(OUT, name))
    diffs = os.path.join(OUT, "compare", p["tag"] + ".txt")
    if identical:
        if os.path.exists(diffs):
            os.remove(diffs)
    else:
        os.makedirs(os.path.dirname(diffs), exist_ok=True)
        with open(diffs, "w", encoding="utf-8") as fh:
            fh.write(report)
    entries[p["tag"]] = dict(p, file=name, source=source + ":" + os.path.basename(transcript),
                             win_marker="yes" if has_marker(transcript, p["marker"]) else "no",
                             verdict=verdict)


def harvest():
    planned = {p["tag"]: p for p in read_plan()}
    runs = provenance()
    candidates = {}
    for transcript, (job, seed) in runs.items():
        path = os.path.join(PFX, transcript)
        if not os.path.exists(path) or not os.path.getsize(path):
            continue
        for tag in (job, job[:-2] if job.endswith("_x") else None,
                    job[:-len(JOB_SUFFIX)] if job.endswith(JOB_SUFFIX) else None):
            if tag in planned and planned[tag]["seed"] == seed:
                candidates.setdefault(tag, []).append(("xoshiro", path))
    # A route that draws nothing is seed-blind: the 2026-09-08 native-RNG
    # corpus capture of it stands in for a xoshiro drive if it compares
    # identical (tried after the xoshiro captures).
    with open(os.path.join(NATIVE_CORPUS, "MANIFEST_tag_transcript_exe.txt")) as fh:
        for line in fh:
            tag, transcript, _ = line.rstrip("\n").split("|")
            path = os.path.join(NATIVE_CORPUS, transcript)
            if tag in planned and planned[tag]["draws"] == "0" and os.path.getsize(path):
                candidates.setdefault(tag, []).append(("native-0-draws", path))
    entries = read_manifest()

    def best(tag):
        ranked = sorted(candidates[tag], key=lambda c: (c[0] != "xoshiro", -os.path.getmtime(c[1])))
        for source, path in ranked:
            identical, verdict, report = compare(planned[tag], path)
            if identical:
                return tag, source, path, verdict, report
        return tag, None, None, None, None

    todo = [t for t in candidates if t not in entries]
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        for tag, source, path, verdict, report in pool.map(best, todo):
            if path:
                adopt(planned[tag], path, "harvested-" + source, True, verdict, report, entries)
                print("harvested %-32s %s" % (tag, os.path.basename(path)))
            else:
                print("no match  %-32s (%d candidate(s))" % (tag, len(candidates[tag])))
    write_manifest(entries)
    print("%d/%d rows in %s" % (len(entries), len(planned), OUT))


def jobs(out):
    entries = read_manifest()
    lines = []
    for p in read_plan():
        if p["tag"] in entries and entries[p["tag"]]["verdict"].startswith("identical"):
            continue
        copy = os.path.join(PFX, "w_%s.taf" % p["tag"])
        source = os.path.join(ROOT, "games", p["taf"])
        if not os.path.exists(copy):
            shutil.copy2(source, copy)
        elif not filecmp.cmp(copy, source, shallow=False):
            sys.exit("%s differs from games/%s; refusing to overwrite" % (copy, p["taf"]))
        lines.append("|".join([p["tag"] + JOB_SUFFIX, os.path.basename(copy),
                               "%s/%s.txt" % (FEEDS_REL, p["tag"]), p["exe"], p["seed"],
                               p["pre"], p["popups"]]))
    with open(out, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    print("%d job(s) -> %s" % (len(lines), out))


def collect():
    planned = {p["tag"]: p for p in read_plan()}
    entries = read_manifest()
    summary = os.path.join(WINE, "par", "summary_xoshiro.txt")
    done = []
    with open(summary, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            job, rc, transcript = line.rstrip("\n").split("|")[:3]
            tag = job[:-len(JOB_SUFFIX)]
            path = os.path.join(PFX, transcript)
            if tag in planned and rc == "0" and transcript and os.path.getsize(path):
                done.append((tag, path))
            else:
                print("FAILED    %-32s %s" % (tag, line.strip()))

    def check(item):
        tag, path = item
        return (tag, path) + compare(planned[tag], path)

    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        for tag, path, identical, verdict, report in pool.map(check, done):
            old = entries.get(tag)
            # keep an identical transcript over a later, worse drive
            if old and old["verdict"].startswith("identical") and not identical:
                print("kept      %-32s (new drive: %s)" % (tag, verdict))
                continue
            adopt(planned[tag], path, "driven", identical, verdict, report, entries)
            print("%-9s %-32s %s" % ("identical" if identical else "DIFFERS", tag, verdict))
    write_manifest(entries)
    missing = sorted(set(planned) - set(entries))
    print("%d/%d rows in %s; missing: %s" % (len(entries), len(planned), OUT,
                                             " ".join(missing) or "none"))
    check_ignored(entries)


def check_ignored(entries):
    # A game whose golden is gitignored (explicit text) must not have its
    # Runner transcript or compare report committed either.
    def ignored(paths):
        out = subprocess.run(["git", "check-ignore", "--"] + paths, cwd=ROOT,
                             capture_output=True, text=True).stdout
        return set(out.split("\n")) - {""}
    goldens = {"goldens/%s_solution.txt" % tag: tag for tag in entries}
    explicit = [goldens[g] for g in ignored(list(goldens))]
    own = []
    for tag in explicit:
        own += ["runner_transcripts/" + entries[tag]["file"], "runner_transcripts/compare/%s.txt" % tag]
    for path in sorted(set(own) - ignored(own)):
        print("NOT IGNORED %s (its golden is gitignored; add it to .gitignore)" % path)


if __name__ == "__main__":
    command = sys.argv[1] if len(sys.argv) > 1 else ""
    if command == "plan":
        plan()
    elif command == "harvest":
        harvest()
    elif command == "jobs":
        jobs(sys.argv[2] if len(sys.argv) > 2 else os.path.join(WINE, "xoshiro_jobs_runner_transcripts.txt"))
    elif command == "collect":
        collect()
    else:
        sys.exit(__doc__)
