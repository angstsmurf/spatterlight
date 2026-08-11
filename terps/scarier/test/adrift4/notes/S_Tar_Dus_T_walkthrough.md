# S Tar Dus T — walkthrough

- **Engine:** ADRIFT 3.90 (`S_Tar_Dus.taf`, 42,463 bytes). You are Jillian, a
  teenaged girl clubbed unconscious on the way to school by a screaming old
  woman, and you wake on a black-and-white checkerboard under a sky full of
  stars. **21 rooms, 48 objects, 42 tasks, 6 NPCs, no events**, no variables.
- **Result:** ★ **WON** (ending T35, the Plant Lady). **There is no score at
  all** — not one `ACT type=4` in the file — so "maximum" means the ending
  that requires the most of the game to have been played.
- **Solution:** `goldens/stardust_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `You decide to go with the plant lady and`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS` +
  `SCR_DUMP_OBJLOC`.

## Four endings, one command

Everything ends on `sw` from the Open Area, and **all four endings are gated
on T31**, the magic words. What separates them is which optional acts you have
done; SCARE tries the tasks in file order, so the first one whose restrictions
all pass is the ending you get.

| Task | Needs done | Needs **not** done | Ending |
|---|---|---|---|
| 33 | flush toilet, drink water, candy to old man | needle in box, wear bracelet | you move into the shack with the squirrel and the old man, who adopts you in place of the daughter a cyclops ate |
| 34 | wear bracelet, eat candy, candy to Vampy | needle in box, drink water | you marry the vampire and move into the mansion |
| 35 | needle in box, drink tea, candy to Plant Lady | wear bracelet, drink water | **★ you become one of the plant lady's kind and live in the mansion's garden** |
| 36 | — (none of the above matched) | — | **LOSS** — you step through the portal, wake from a two-week coma, *"Better luck next time."* |

Stepping through the portal home is the losing move; staying with your new
friends is the win. T36's `ACT type=6 v1=1` is the only non-zero one in the
file.

**T35 is the one worth taking.** It is the only ending whose conditions cost
you nothing but two *mistakes* — drinking the lake water, and keeping the
bracelet you are meant to trade — so every optional scene in the game can
still be played on the way there: flushing the toilet, feeding the old man,
feeding Vampy, eating the candy, talking to all six characters. The other two
force real content to be left out.

## Route

```
Open Area    e / in
Inside Shack take bracelet / take statue
             out / w / w
Merchant     talk to merchant / give merchant the bracelet   -> candy
             w
Banana Moon  talk to old man
             w / in
Outhouse     take tp / take paper
             out / e / e / e / s
Tree         talk to squirrel / take branch
             give candy to squirrel      -> the squirrel now follows you
             talk to squirrel / eat candy
             s
Tea Time     take cup
             n / n / n
Outside Mans enter mansion               (needs the squirrel)
             w
Sewing Room  take needle / take needle box
             e / e
Tea Room     take tea packet
             w / out / s / s / s
Tea Time     prick finger / pour blood
             make tea / pour tea / drink tea
             n / n / n / enter mansion / n
Ballroom     use statue                  -> through the Dark Mirror
Mirror Ballr talk to vampy / give cup to vampy / give candy to vampy
             s / w
Music Room   take flute
             e / e
Library      take spellbook
             w / n / e
Corridor     talk to ghost / play music  -> the Ghost is banished
             w
Mirror Ballr talk to vampy               -> back to the real Ballroom, with Vampy
             make walking stick with star statue and branch
             s / out / s / w / w
Banana Moon  give walking stick to old man   -> the old man now follows you
             talk to old man / give candy to old man
             e / e / s / s
Tea Time     s                           -> the whole party is moved to the Lake
Lake         take missing page           (it fell IN the lake)
             n / n / n / w / w / w / in
Outhouse     flush toilet / put needle in box
             out / e / e / e / n / enter mansion / n / n
Garden       use tp / blabber flabber boogie gaboo  -> the Plant Lady wakes
             give candy to plant lady / talk to plant lady
             s / s / out / s
Open Area    sw                          (EndGame win)
```

## One piece of candy feeds five characters

`give merchant the bracelet` is the only trade in the game and the candy it
buys is **never consumed**: T2 (squirrel), T6 (eat it yourself), T26 (Vampy),
T28 (the old man) and T30 (the Plant Lady) each only *check* that you are
carrying it. So the same half-eaten sweet does the rounds all game.

Feeding the squirrel is what starts the whole chain. Its walk's `startTask` is
T2 and its single step is "follow the player", so from then on it tags along —
and **T3 `enter mansion` needs it in the room**. That is what the room
description means by *"Maybe if you had someone to help you enter the mansion
you could get in."* Every one of the four friends works the same way: Vampy's
walk starts on T19, the old man's on T24, the Plant Lady's on T31.

## The three TPs are the same object

`obj7 [TP]` is the toilet paper in the outhouse, the thing you have to be
holding to `play music` for the Ghost, **and** the "depetrification charm" of
T29 (`use Dep Cha` / `use TP`). Nothing in the game hints that the charm that
wakes the Plant Lady is a roll of loo roll; the joke *is* the puzzle. Take it
on the first pass through the outhouse — it is a long walk back.

## The missing page is in the lake

T25 (`s` at Tea Time with the old man present) sweeps the whole party off to
the Lake and drops the missing spellbook page with an `ACT type=0` of type
"into object", `v3=2`. For those moves SCARE indexes the container list
**directly** — `obj_container_object(game, var3)`, no `-1` — so `v3=2` is
`CONTAINER idx=2` = `obj30 [lake]`, not `idx=1`, the outhouse hole. Take the
page the moment you are dumped at the lake; looking for it in the outhouse
just gets *"Take what?"*.

## The cup carries blood and then tea

One tea cup does both jobs: `prick finger` (needle held) puts a drop in the
tea pot, `pour blood` fills the cup, and `make tea` / `pour tea` / `drink tea`
reuse it. **Drink the tea before crossing the mirror** — T13 is one of T35's
three requirements, and `give cup to vampy` hands the cup over for good.

## The statue is a one-way ticket

`use statue` at the Dark Mirror crosses into the mirror world and back (T14 /
T20), but T14 is restricted on T19 *not* being done — and T19, `talk to vampy`
with the spellbook in hand, is what brings Vampy home. Worse, **T23 consumes
the statue** to build the walking stick. So the mirror world has to be
finished in one visit: flute from the Music Room, spellbook from the Library,
`play music` for the Ghost in the Corridor, then Vampy.

## Notes

- **No events, no clock, no `<waitkey>`** (`SCR_MARK_WAITKEY=1` finds none),
  and no name prompt — the game calls you Jillian.
- **`talk to X` is doubled all over the file** — T1/T37 for the squirrel,
  T15/T19/T38 for Vampy, T21/T22/T27/T41 for the old man, T32 for the Plant
  Lady — with the pairs distinguished by whether the favour has been done yet.
  Talking twice, before and after, is how the game tells its story.
- **`give cup to vampy` is optional.** No ending requires it; its only effect
  is to switch off T15, the "Vampy asks you for blood" response (`RESTR
  type=2 v1=17 v2=1` — T16 must *not* be done). You hand it over because a
  vampire asked and it would be rude not to.
- **The Ghost never moves** and neither does the Merchant; the other four NPCs
  only ever follow.
