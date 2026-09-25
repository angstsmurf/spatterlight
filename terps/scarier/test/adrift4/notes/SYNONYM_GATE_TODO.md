# DONE (2026-09-25): port the Runner's synonym gate (unblocked Dolg, re-checked 4 held games)

Background: see `Dolg_walkthrough.md`, "Why it was blocked, and what was ported". It was measured live in Wine run390 on 2026-09-25.

- **run390 rule:** `If c(orig, line) Then line = Replace(line, orig, LCase(repl), 1, -1, 0)` (45F18C-45F206).
  - The gate `c()` (4334B0) looks only at the FIRST InStr hit that starts the line or follows a space. If the next character is not a space, a comma or a full stop, it returns FALSE.
  - When the gate passes, the Replace is a SUBSTRING replace, which also hits letters inside other words.
- **Scarier before 2026-09-25:** `pf_filter_input()` (`scprintf.cpp`) rewrote every whole-word match, unconditionally.

## Steps

1. [x] **Confirm the rule in the other Runners.** run380 and run400 have the same loop and the same gate; run370 has no synonym table. No version differences.
2. [x] **Port it into `pf_filter_input()`.** `pf_find_folded()`, `pf_runner_word_gate()`, `pf_replace_binary()`, `pf_lcase()`, `pf_apply_synonym()` in `scprintf.cpp`. The Original is used raw (no Trim, no LCase), so a trailing-space or capitalised Original behaves as in the Runner.
3. [x] **Re-check with the existing Wine test.** `позвонить в звонок` -> «Я не понимаю, что вы хотите!», `дернуть за шнурок` opens the door, `войти в дом` enters the house. The hint line («No hints currently available.», ALR'd) is also matched now (`lib_cmd_hints`, wording split at 3.80).
4. [x] **Run the full suite.** 618 PASS. The changed goldens (cs2, shablon; govard after its re-spelling) were each driven in Wine run390 before being re-blessed.
5. [x] **Re-derive Dolg.** `goldens/dolg_solution.txt` + `.expected.txt`, row `dolg_solution.txt|Dolg.taf|я и расплатился с Барни|`, WIN 10/35; Wine run390 identical on every turn (`runner_transcripts/dolg.txt`).
6. [x] **Re-check the 4 held games.** `harness/syn.py` rewritten with the Runner model.
   - cs2: `св` -> `ne` (a capitalised Original passes the gate and replaces nothing; the Runner answers «Да?»); `выглянуть в окно` now unrewritten. Wine: identical word for word (the compare's "10+ differing" is its `> ` prompt artefact).
   - shablon: `выбросить пачку в урну` now reaches the game's task 60 (no fine, wallet 178$). Wine: identical.
   - wanderersgow: unchanged. Wine: identical.
   - govard: seven lines re-spelled (`смотреть стол`, `оставить X` x4, `put ножик ...`, `влезть`), WIN 300/310. The Wine drive (needs `INTRO_QUIET=25 PRE_SLEEP=30`) exposed two further run390 rules, both ported the same day (3.9 weapon question is a turn; pre-4.0 room-alt holding tests the object's own position), after which the dice ladder was re-derived. Second Wine drive: identical on every turn except two documented Runner artefacts (cp1251 UCase dog line, phantom take-all item); Runner wins 300/310. See `Govard_walkthrough.md`.
   - All wired in `run_v4_walkthroughs.sh`.
7. [x] **Update the notes and memory.** `Dolg_walkthrough.md` rewritten; memory `adrift4-synonym-gate-runner-rule`.
