# Routes rewritten for stock SCARE 1.3.10

Each file here is the route of the same name in `../../goldens/`, edited until
it reaches the row's marker in an unmodified SCARE 1.3.10. The originals win in
Scarier and fail in SCARE. They are evidence for `SCARIER_vs_SCARE.md`, not
part of any test suite: Scarier is not expected to win with them.

Every route was run nine times in stock SCARE and reached its marker each
time. Comment lines and blank lines of the originals are dropped.

The `bluesky` and `duchess` routes are not committed, like their originals in
`../../goldens/` (see `../../.gitignore`).

| Route | Game | Edit | Why SCARE needs it |
|---|---|---|---|
| `bluesky` | Blue Sky | six `z` before `give burger to maria` | events tick before characters; Maria leaves a turn early and has to come round again |
| `showtime` | Showtime at the Gallows | three `z` before going north through the gate past the hounds | same tick order |
| `duchess` | Duchess of Desire | one trailing `z` | same tick order |
| `fugitive` | The Fugitive | two `z` before boarding the train | the train event runs a different number of turns |
| `journ2` | The Long Journey Home | one `z` before the second `s` | the creature's event is a turn behind |
| `vampire` | The Vampire With A Conscience | one `z` before `open container` | the scene is set a turn later |
| `jasonevans` | Jason Evans 1 | two `z` before `break statue` | the father's attack falls on another turn |
| `circus` | Menagerie! | two `z` before `buy popcorn`, ten before `buy crackers` | the vendors walk a different round |
| `deathhouse` | House of Death | the three runs of `look` shortened from 8/6/5 to 2/1/0 | the repeating zombie events have another phase |
| `blood` | Fire in the Blood | one `z` before `give bone to timmy`; `open package`, `take wad of money` before `give wad to tom` | the dog's patrol is a turn off; an object inside a held container does not count as held |
| `troll` | Troll! | `put crumbs in basin` for the first `put breadcrumbs in basin`; `take soaked breadcrumbs`, `take few breadcrumbs` before `put soaked breadcrumbs in lake` | a failing task hides the library; held container again |
| `cave` | Cave of Wonders | `remove poppy` before `give poppy to meg` | a worn object does not count as held |
| `cloak` | Cloak of Darkness | `remove cloak` before `hang cloak` | a worn object does not count as held |
