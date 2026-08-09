# The Annihilation of think.com — walkthrough (WON, 35/35)

*The Annihilation of think.com* (ADRIFT **3.90**) — a tiny, linear satire in
which "Herald the dog" and his brainwashed teachers have taken over the
children's site **think.com**, destroying its icons and codes. You log in from
your bedroom, fight your way up through the site's pages to Herald's office, and
defeat him to restore think.com.

- **Result:** WON, **full 35/35**, deterministic (seed 1234).
- Solution: `goldens/think2_solution.txt`.
- Win marker: *"Think.com has been restored and everyone got their icons back…"*
  (task 9, type-6 EndGame, var1=0).
- No Battle System (the two "fights" are scripted multiple-choice menus).

## This game surfaced a real SCARE engine bug (now fixed)

The game was **unwinnable in SCARE** before this session — *not* a game-data dead
end, but a command-parser divergence:

- The very first and only exit from the bedroom is the task command
  **`login to think.com`** (it moves the player to Amanda's page and scores +5;
  `south`/`west` in the bedroom are flavor tasks that don't move you).
- SCARE split player input on **any** bare `.` or `,` (`SEPARATORS = ".,"`), so
  `login to think.com` was chopped into `login to think` + `com` and never
  matched the task — the bedroom was a sealed room.
- The ADRIFT Runner does **not** do this. Reverse-engineering the 3.90 Runner's
  input splitter (`run390.txt`, the routine around `0x5EC80`) shows it normalises
  multiple commands on **`","`**, **`". "` (period *followed by a space*)** and
  **`"then"`**. A period embedded in a word (`think.com`, or a decimal like
  `3.5`) is part of the command, never a separator.

**Fix** (`scrunner.c`): replaced the `strchr(SEPARATORS, …)` tests with
`run_is_separator()` — a comma always separates; a period separates only when
followed by whitespace or end-of-line. Verified that `look. look` and
`look,look` still run two commands while `login to think.com` is now one. This
is an engine fix that applies to Spatterlight proper, not just the headless
harness.

## The route (the map is the think.com "site")

Rooms: `0` Your bedroom · `1` Amanda's page · `2` Connors page (the paper) ·
`3` Mr others page (Guard) · `4` Mrs Mac Intires page · `5` Main think.com page
(Mrs Assface) · `6` Herald's Corridor · `7` Herald's stairs · `8` Herald's tower
room · `9` Herald's office (Herald the dog).

```
(name: Hero)         start-up prompts (this game uses both — see corpus notes)
(gender: male)
login to think.com   +5  → Amanda's page (room 1)
east                     → Connors page (the paper is at your feet)
take paper
north                    → Mr others page (Guard blocks the way)
say icons are banned     guard checks the paper and lets you pass
east                     → Mrs Mac Intires page
put paper on Mrs Mac Intire  +10  she joins you ("you must stop Herald")
out                      → Main think.com page; Mrs Assface attacks with a gun
2                        +5  DUCK (choice 1 = "take machine gun" = death)
                         (3 blank lines below feed the duck text's <waitkey> pauses)


n                        → Herald's Corridor (room 6)
n                        → Herald's stairs (room 7)
u                        → Herald's tower room (room 8)
w                        → Herald's office (room 9); Herald attacks
4                        +15  DEFEND (the purple-shield counter kills Herald) = WIN
```

Scoring (max 35): `login` +5 · `put paper on Mrs Mac Intire` +10 · `2` (duck) +5
· `4` (defend) +15. The two combat menus are pure scripted choices — in Mrs
Assface's room only `2` survives (`1` = death); in Herald's office only `4` wins
(`1` and `3` are deaths, `2` "beg" does nothing).

## Headless-harness note (`<waitkey>`)

The author embeds three `<waitkey>` ("press a key") pauses in the *duck* text.
The real Spatterlight (Glk) handles these as single keypresses. The headless
ANSI harness stands in for each keypress by consuming one input line, so the
banked solution has **three blank lines after `2`**. (A new opt-in
`SC_SKIP_WAITKEY=1` makes the harness ignore these pauses for clean
one-line-per-command derivation; the faithful default — and this solution — keep
the keypress behaviour, so `sh harness/play.sh` reproduces 35/35 with no flag.)

Verified deterministic 3× (identical +5/+10/+5/+15 and win text).
