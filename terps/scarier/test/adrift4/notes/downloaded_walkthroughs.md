# Downloaded walkthroughs: bundled files and source URLs

Kept from the old walkthrough index (formerly `downloaded/INDEX.md`) for the
parts other notes cite. The files named here live in `downloaded/`, kept
exactly as they were downloaded.

## Walkthroughs bundled in game archives

**Added 2026-08-11:** `SilkNoil_walkthrough.txt` — not from the IFDB harvest.
The author shipped it inside the game's own archive (`sn_zip.zip`), which is
how it was missed; it is ten commands long and *Silk Noil* is wired from it.
Worth checking every game archive for a bundled walkthrough before deriving
one by hand. `WheelsMustTurn_walkthrough.txt` (from `zip_w105.zip`) arrived
the same way the same day, and *The Wheels Must Turn* is wired from it.

**Added 2026-08-17:** three more of the same kind, pulled out of the
InsideADRIFT comp archives that the corpus already sources the .taf files from,
and all three games are now wired verbatim from them:

| file | bundled in | member |
| --- | --- | --- |
| `Door_walkthrough.txt` | `SummerCompGames08.zip` | `games/doordocs/walkthru.txt` |
| `MarlinAffairPrologue_walkthrough.txt` | `SummerCompGames08.zip` | `games/junedocs/june_walkthrough.txt` |
| `CanItBeAllSoSimple_walkthrough.txt` | `SummerComp05.zip` | `SummerComp05/cibass/Walkthrough.txt` |

Two footguns both of the last two hit: the games page their prose with keypress
prompts, so the rows need `SCR_SKIP_WAITKEY=1` or the prompts silently eat
solution lines; and the CIBASS file has no trailing newline, which welds its
last `wait` onto the harness's appended `quit`. Grep the transcript's `> `
echoes against the solution file before concluding a bundled walkthrough is
broken.

## Source URLs

| Game | IFDB | File(s) | Source |
| --- | --- | --- | --- |
| Mangiasaur | [mgk7ueugc7vxis](https://ifdb.org/viewgame?id=mgk7ueugc7vxis) | `Mangiasaur_clubfloyd.html` | http://www.allthingsjacq.com/intfic_clubfloyd_20120202.html  (session also covers Kerkerkruip; the page's own title says February 12, 2012 -- the `0202` in the URL is upstream's, not a typo here) |
