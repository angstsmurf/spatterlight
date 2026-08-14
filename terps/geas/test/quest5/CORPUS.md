# Quest 5 corpus — 88 `.quest` files = 86 wired entries (84 distinct games + 2 version duplicates: Baleful Backwash 2018 re-release, The Acreage pub 6.29 revision) + 2 unwired extra copies (Serpent's Eye v1.1.3, `speakeasy5.quest`), 79 Quest 5 walkthrough files

**25 of the 84 distinct games have a matching walkthrough.** These are the ready-made regression scripts.
Every game in the "without a walkthrough" table has no published walkthrough anywhere;
ALL are now driven by source-derived `overrides/` scripts in the oracle — see
`terps/geas/test/quest5/harness/oracle/overrides/README.md`. The corpus is **complete**: all
86 wired entries have a frozen golden and check_golden.sh runs 86/86.

This file describes what the corpus *is*. `games.manifest.tsv` beside it is the
machine-readable half — the sha256 pin and download source for every file — and
`../GAMES.md` explains how to fetch and verify against it. The games themselves
live in the gitignored `games/`, and are never committed.

See `~/Downloads/Quest 5 walkthroughs/README.md` for sources, the Wayback workaround, folder layout, and known data bugs. That folder's top level holds only Quest 5 walkthroughs; `_pre-quest5/` (Quest 1–4) and `_not-quest5/` (QuestJS) are set aside.

## Games with a walkthrough

| Game | Author | ASL | MB | Walkthrough file(s) |
|---|---|---|---|---|
| A Christmas Game - The Sequel | Luke A. Jones | 580 | 0.5 | A Christmas Game - The Sequel - walkthrough.txt; A Christmas Game- The Sequel.html; A Christmas Game- The Sequel.txt |
| A Christmas Game | Luke A. Jones | 580 | 2.1 | A Christmas Game - The Sequel - walkthrough.txt; A Christmas Game - walkthrough.txt; A Christmas Game- The Sequel.html; A Christmas Game- The Sequel.txt; A Christmas Game.html; A Christmas Game.txt |
| A Hobbit Trek | Crispin | 540 | 0.1 | A Hobbit Trek - walkthrough.txt; A Hobbit Trek.html; A Hobbit Trek.txt |
| Basilica de Sangre | Bitter Karella | 580 | 0.5 | Basilica de Sangre - Walkthrough and map.html; Basilica de Sangre - Walkthrough and map.txt; Basilica de Sangre - Walkthrough.doc; Basilica de Sangre - walkthrough.txt |
| Dream Pieces 2 - The Lego Box | Iam Curio | 550 | 2.3 | Dream Pieces 2 - The Lego Box - walkthrough.txt; Dream Pieces 2- The Lego Box.html; Dream Pieces 2- The Lego Box.txt |
| Dream Pieces | Iam Curio | 540 | 0.3 | Dream Pieces - walkthrough.txt; Dream Pieces.html; Dream Pieces.txt |
| Eight characters, a number, and a happy ending | K.G. Orphanides | 550 | 0.1 | Eight characters, a number, and a happy ending.html; Eight characters, a number, and a happy ending.txt |
| Escape From the Mechanical Bathhouse | Nathaniel Spence | 580 | 1.0 | Escape From the Mechanical Bathhouse - walkthrough.txt; Escape From the Mechanical Bathhouse.html; Escape From the Mechanical Bathhouse.txt |
| Guttersnipe- Carnival of Regrets | Bitter Karella | 550 | 9.3 | Guttersnipe - Carnival of Regrets - walkthrough.txt; Guttersnipe- Carnival of Regrets.html; Guttersnipe- Carnival of Regrets.txt |
| Guttersnipe- St. Hesper's Asylum for the Criminally Mischievous | Bitter Karella | 550 | 0.4 | Guttersnipe - St Hesper's Asylum for the Criminally Mischievous - walkthrough.txt; Guttersnipe- St. Hesper's Asylum for the Criminally Mischievous - Author's walkthrough.html; Guttersnipe- St. Hesper's Asylum for the Criminally Mischievous - Author's walkthrough.txt; Guttersnipe- St. Hesper's Asylum for the Criminally Mischievous - Walkthrough and map.html; Guttersnipe- St. Hesper's Asylum for the Criminally Mischievous - Walkthrough and map.txt |
| Guttersnipe- The Baleful Backwash | Bitter Karella | 550 | 0.7 | Guttersnipe - The Baleful Backwash - walkthrough.txt; Guttersnipe- The Baleful Backwash.html; Guttersnipe- The Baleful Backwash.txt |
| Guttersnipe- The Baleful Backwash (2018 re-release) | Bitter Karella | 550 | 0.7 | Guttersnipe - The Baleful Backwash - walkthrough.txt (needs adapting — see the oracle override) |
| Hawk the Hunter | Jonathan B. Himes | 580 | 0.3 | Hawk the Hunter - hints (textadventures page).txt; Hawk the Hunter - hints-walkthrough.txt |
| I Contain Multitudes | Wonaglot | 580 | 2.0 | I Contain Multitudes - walkthrough.txt; I Contain Multitudes.txt |
| Jacqueline, Jungle Queen! | ? | 550 | 0.6 | Jacqueline Jungle Queen - walkthrough.txt; Jacqueline, Jungle Queen! - Walkthrough (HTML).html; Jacqueline, Jungle Queen! - Walkthrough (HTML).txt; Jacqueline, Jungle Queen! - Walkthrough and map.html; Jacqueline, Jungle Queen! - Walkthrough and map.txt |
| Night House | Bitter Karella | 580 | 9.3 | Night House - Walkthrough.html; Night House - Walkthrough.txt |
| One Night Stand | Giannis G. Georgiou | 550 | 0.8 | One Night Stand - Author's walkthrough.txt; One Night Stand - Walkthrough and map.html; One Night Stand - Walkthrough and map.txt; One Night Stand - walkthrough (author, IF Archive).txt; One Night Stand - walkthrough.txt |
| Poppet | Bitter Karella | 580 | 1.4 | Poppet - Author's walkthrough.docx; Poppet - Author's walkthrough.txt; Poppet - Walkthrough and map.html; Poppet - Walkthrough and map.txt; Poppet - walkthrough.txt |
| Quest for the Serpent's Eye | Lazygamedesigner82 | 580 | 7.6 | Quest for the Serpent's Eye - walkthrough.pdf; Quest for the Serpent's Eye - walkthrough.txt; Quest for the Serpent's Eye.pdf |
| The Bony King of Nowhere | Luke A. Jones | 550 | 0.6 | The Bony King of Nowhere - walkthrough.txt; The Bony King of Nowhere.html; The Bony King of Nowhere.txt |
| The Brutal Murder of Jenny Lee | ? | 580 | 0.1 | The Brutal Murder of Jenny Lee.pdf |
| The Deer Trail | Justin Squires | 580 | 7.8 | The Deer Blind Walkthrough.txt (author; named after the working title "The Deer Blind") |
| The Mouse Who Woke Up For Christmas | Luke A. Jones | 550 | 4.7 | The Mouse Who Woke Up For Christmas - Walkthrough and map.html; The Mouse Who Woke Up For Christmas - Walkthrough and map.txt; The Mouse Who Woke Up For Christmas - Walkthrough.doc; The Mouse Who Woke Up For Christmas - walkthrough (author, IF Archive).txt; The Mouse Who Woke Up For Christmas - walkthrough.txt |
| The Myothian Falcon | The Pixie | 500 | 0.1 | The Myothian Falcon.txt |
| What Once Was | Luke A. Jones (with some coding magic from K.V.) | 550 | 0.4 | What Once Was - walkthrough.txt; What Once Was.docx; What Once Was.txt |
| Whitefield Academy of Witchcraft | Steph Cherrywell | 550 | 1.7 | Whitefield Academy of Witchcraft - walkthrough.txt; Whitefield Academy of Witchcraft.html; Whitefield Academy of Witchcraft.txt |

## Games without a walkthrough

| Game | Author | ASL | MB |
|---|---|---|---|
| All Visitors Welcome | Bitter Karella | 550 | 0.1 |
| ARC II | Daniel Gao | 580 | 16.5 |
| Attack On Frightside | Farley Sweet | 550 | 0.0 |
| Balaclava | nahuel denegri | 550 | 0.1 |
| Bear's Epic Quest | Hospes | 500 | 0.1 |
| Behind the Door | eejitlikeme | 550 | 0.5 |
| Beowulf | anonymous | 500 | 1.7 |
| Caught! | T H Duncan | 550 | 4.9 |
| cuttings | nahuel denegri | 550 | 6.3 |
| Defeating The Monster The Ranking Of The Class | Social Dragon 368 | 580 | 0.1 |
| Dracula | Rod Pike, Remade Unofficially 2014 by Jon Bardi | 550 | 4.3 |
| EELTAIL | Mick Green | 550 | 6.4 |
| El asesino durmiente (Micky) | CandyVonBitter | 550 | 0.3 |
| Everyman | Simon Deimel | 540 | 0.1 |
| Exit the Room | jerkdavi | 580 | 0.1 |
| First Times | Hero Robb | 520 | 9.8 |
| Fountain of Eternal Youth v1.3 | anonymous | 580 | 5.1 |
| Frankenstein's Tomb | Craig Dutton | 550 | 3.2 |
| Gilleholmen | Joachim Parrow | 580 | 89.1 |
| Great Depression Man (Middle Class) | ? (school history project) | 580 | 0.7 |
| HMS Victory | Peter Edwards | 550 | 6.4 |
| Iron John | Θανάσης Χρυσού | 580 | 0.2 |
| Its election time in Pakistan- Go rich boy, go! | Jay Haque | 540 | 0.6 |
| L Too (2.17) | ? | 580 | 3.0 |
| La Aventura de Parodi | Lucas Cavanna Quero | 550 | 0.7 |
| Lost Cavern | Simon Richards | 580 | 0.7 |
| Lost in the Shadows of Time | K.A. Williams | 540 | 4.1 |
| Medievalist's Quest | Cein Chavez | 550 | 122.0 |
| Moquette | Alex Warren | 550 | 0.7 |
| MOUNTAIN SKI 2.0 | Patricia | 580 | 15.8 |
| Nearco II (Remake) - La perdición de la Ninfa - Vr Offline | Jhames | 500 | 7.5 |
| Oracle Feels Confused | ratakim | 580 | 5.9 |
| Quest for Loot and Something Else | Mugi4ok | 550 | 1.3 |
| Santa Carcossa Nights | Bitter Karella | 580 | 4.0 |
| Signos | M4u | 520 | 2.2 |
| spondre | Jay Nabonne | 550 | 1.1 |
| Sueña un pequeño sueño | Juan Cristóbal Aedo Olivares | 540 | 0.0 |
| Sword and Spell | Caleb Wilson | 550 | 0.1 |
| Texture | Ralf Thissen | 550 | 0.4 |
| The Acreage | ? | 580 | 0.4 |
| The Acreage (pub 6.29 revision) | ? | 580 | 0.4 |
| The Eye of Mandival | Father Thyme | 580 | 0.3 |
| The Gift of the Magi | O. Henry | 550 | 0.1 |
| The Last Hero | Wayde Bairstow | 550 | 0.1 |
| The Legend of Robin Hood | Craig Dutton | 540 | 0.2 |
| The Lunastone | Craig Dutton | 550 | 12.2 |
| The Shack | Mat Cooper / System Masters | 580 | 18.8 |
| The Tree | Father Thyme | 580 | 1.6 |
| The Zen Garden | Privateer | 530 | 7.2 |
| To Kindle A Light | J. Vasilevic | 550 | 0.8 |
| Tombs and Mummies | Matthew Warner | 580 | 0.9 |
| Train2019 | anonymous | 550 | 0.1 |
| Treasure of the Ghost-King | Craig Dutton | 550 | 0.6 |
| WAKE | Timothy Sibiski ("Asyranok") | 540 | 5.4 |
| Warriors | anonymous (Warrior Cats fan game) | 550 | 9.2 |
| Welcome to Pineview | Filthy & Free Publishing | 550 | 17.0 |
| Welcome to the Paris Hotel! | CentaurOfAttn | 550 | 0.5 |
| Woo Rebooted 3.6 | Longshot & DavyB | 580 | 7.2 |
| Xanadu - In the Compound - Revenge | Kevin Schaeffer | 550 | 24.9 |
| Xanadu - The World's Only Hope | Kevin Schaeffer | 550 | 0.4 |

## Quest 5 walkthroughs with no game on disk (0)

Kept deliberately; game retrievable via the Wayback recipe in the walkthroughs README.


## Notes

### Corrections to `terps/geas/TODO-quest5.md`

1. **§7's "free regression scripts" assumption is false.** It says "many `.quest` games ship `<walkthrough>` elements". **None of the games here do.** It is an editor-side testing feature authors essentially never populate. Regression scripts must come from the external walkthroughs harvested here.
2. **§7 names textadventures.co.uk as the corpus source.** Direct access is Cloudflare-blocked (403 to any scripted client), **but the Wayback Machine is not** — see the README for the working recipe. 11 games here exist nowhere else. The corpus is therefore **not size-capped**: IFDB's `system:Quest` filter returns **631 Quest games**, any of which can be pulled the same way.
3. **`The_House_on_Highfield_Lane` is QuestJS ("Quest 6", pure JavaScript), not Quest 5.** No `game.aslx`. TODO-quest5.md explicitly rejects QuestJS, so the planned engine can never run it. Moved to `_not-quest5/`.

### Corpus facts

- **Walkthrough coverage is believed complete.** Of the 631 Quest games on IFDB, exactly 35 have a walkthrough link. Cross-referencing Welbourn's full index (1340 entries) and `if-archive/solutions/` (499 files) against all 631 titles found **zero** additional. Don't re-hunt.
- **plover.net's `idx_quest.html` cannot prune by version string alone**: a bare `Quest` (no number) means "version unrecorded", not "pre-5" — 15 of the first 49 verified Quest 5 games were labelled bare `Quest` there. The pre-5 walkthrough split was done by triangulating bare-`Quest` + index-year (2001–2009) + `.asl`/`.cas` format, with ASL headers as ground truth.
- ASL versions present: 500 (4 games), 520 (2 games), 530 (1 game), 540 (8 games), 550 (41 games), 580 (29 games) — `Core.aslx` branches on the declared version, so this is real compat coverage.
- **`_not-quest5/`** (games folder) holds Quest 3/4 `.asl`/`.cas` games (which belong to the *existing* Geas engine) and the QuestJS outlier.
- Every `.quest` here is verified: a real zip containing `game.aslx`. Three deliberate
  version duplicates:
  - `Quest for the Serpent's Eye v1.1.3.quest` (newer release, kept alongside the frozen
    v1.1.1 corpus copy; the frozen override replays on it to THE END with errors=0 —
    16 prose-polish diff lines, same path). NOT separately wired.
  - `Guttersnipe- The Baleful Backwash (2018 re-release).quest` (the author's later build,
    Quest 5.8.6794.18055 / 2018-08-12, shipped as `speakeasy5.quest`, cover
    `backwash512.png`; the frozen corpus copy is Quest 5.7.6404.15496 / 2018-03-24, cover
    `backwashs.png`). Unlike Serpent's Eye the frozen script does NOT replay on it —
    renamed objects and a rebuilt trapdoor lock break the poker key — so it IS separately
    wired, with its own override + golden.
  `speakeasy5.quest` is still present as well and is **byte-identical** to the re-release
  copy (same md5) — a leftover of the rename, not a third build. It is deliberately not
  a corpus row.
  - `The Acreage (pub 6.29 revision).quest` (the author's later build, Quest
    5.10.9635.26171 against the frozen copy's 5.9.9166.36226, 177 objects against 173,
    new opening and much reworded prose). Like Baleful Backwash the frozen script does
    NOT replay on it — Desmond's conversation menu lost a topic and renumbered, and an
    out-of-range menu answer silently swallows the rest of the run — so it IS separately
    wired, with its own override + golden.

  Three games were renamed to their in-game titles when they were wired, since the corpus
  row and the golden are keyed on the file's basename: `behind_the_door_final.quest` →
  `Behind the Door.quest`, `Park.quest` → `All Visitors Welcome.quest`, and
  `The eye of Mandival..quest` → `The Eye of Mandival.quest`.
