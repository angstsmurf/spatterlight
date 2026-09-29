# Runner probe transcripts cited by Scarier's comments

These are the real Windows ADRIFT Runner transcripts behind the "measured"
notes in Scarier's source (`runner_probes/<name>` in a comment means this
folder). They were recorded under Wine: synthetic probes built by
`../harness/make_*probe.py`, plus short targeted runs of real games. The
whole-game twins of the wired walkthroughs live in `../runner_transcripts/`.

Each file is a byte-identical copy of the Wine harness's own archive
transcript, so the line, turn and cell numbers quoted in the comments hold.
`provenance.tsv` maps each name back to the archive file it came from.

## Names

    <subject>.<runner>[.<variant>].<ext>

- `<subject>`: for a probe, the probe name without its version prefix
  (`p4ONIN.taf` → `onin`, `p37TKA`/`p38TKA`/`p39TKA` → `tka`). For a real
  game, its `runner_transcripts/` tag (`humbug`, `xfiles`) or a short slug.
- `<runner>`: `run370`, `run380`, `run390` or `run400`, the Runner that
  produced the file. Most runs used the xoshiro-hooked `run3x0x`/`run400x`
  builds; the comment says which where it matters.
- `<variant>`: present only when one subject and Runner has several runs. It
  is a feed name, a short description, or `b`/`c`/... in recording order.
- `.txt`: the live transcript from run390/run400. `.rtf`: the Save Transcript
  output from run370/run380; read it with `textutil -convert txt -stdout`.
  A few `*.scrollback.txt` / `*dump.txt` files are UTF-16 window dumps.

Transcripts of games whose goldens are gitignored for explicit text stay
local here too (see `../.gitignore`), the same as in `runner_transcripts/`.
