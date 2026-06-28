# Silk Road Secrets (SRSintro) — walkthrough (0/0, INTRO DEMO — no ending)

**Game:** `SRSintro.taf` — *"Silk Road Secrets (Samarkand to Lop Nor)"* by
C. Henshaw. ADRIFT 3.9. You are **Beghram of Tokharia**, an exiled mercenary
summoned to Samarkand by its Khan, who offers you the legendary **Sword of
Nismus** (your ancestor's blade) in exchange for recovering some stolen
"Heavenly" beasts from China. This is an **introduction / demo only** — the
opening city of a planned larger game.

**Result:** there is **no win and no score.** A full structural dump
(`SC_DUMP_TASKS`) of all 37 tasks shows **zero `ACT type=6` (EndGame) actions
and zero `ACT type=4` (ChangeScore) actions** — so no victory/lose/death state
and no points exist (the same class as *IceCream* / *Invasion of the Second-Hand
Shirts* / *lifesimulation*). The demo just lets you wander the three-room
opening, equip yourself, and gather lore, then it stops. Banked tour:
`harness/srsintro_solution.txt`.

## Map (3 rooms)

```
        Zoroastrian Shrine (room 2)
              ^  NE (gated: requires the Khan's sword)   | SW back
              |
  Citadel  <--E/W-->  Marketplace (room 0, start)
  (room 1)            (NE -> Shrine, E -> Citadel)
```

- **Marketplace** (start) — the city bazaar; shopping NPCs (a horse trader, a
  dry-goods merchant/woman, a boy with a scroll). `E` → Citadel, `NE` → Shrine.
- **Citadel / Great Hall** (E of marketplace) — the Khan gives you the **Jan-wa**
  (a Chinese-crafted sword) and the cryptic mission, then is called away.
- **Zoroastrian Shrine** (NE of marketplace) — a fire-priest who answers
  `ask priest about <topic>` lore (beasts / omens / articles of protection /
  khan / mission). Leave with `SW` (the display direction; the exit table lists
  it as SE — labels are rotated, navigate by the game's own "you can move…"
  text).

**Gate:** the Marketplace→Shrine (`NE`) exit is gated on **task 1**
(`take the Jan-wa/sword/scabbard from the khan`), so you must visit the Khan and
accept the sword **before** the shrine opens.

## Tour (illustrative — there is nothing to "win")

```
(press a key past the intro)
info
east                 (to the Citadel; the Khan gives you the Jan-wa sword)
take sword from khan
west                 (back to the Marketplace)
ne                   (now open: to the Zoroastrian Shrine)
ask priest about mission
ask priest about beasts
ask priest about omens
ask priest about articles of protection
ask priest about khan
sit on ledge
sw                   (back to the Marketplace)
```

## Notes

- **It is an intro demo.** The marketplace economy is set up (a coin **pouch**
  you can open/close, `take dirhams`, `buy horse/food/provisions/scroll/water
  skin/tent`, `give the trader 15 dirhams`, `take the scroll from the boy`,
  `read the Xing scroll`), and combat verbs exist (`cut/hit … with sword`), but
  none of it leads anywhere scored or to an ending — it exists to seed the
  full game that would follow.
- The priest's `ask … about` topics (beasts/omens/articles of protection/
  khan/mission) are the substantive lore content of the demo and are worth
  reading; he refuses to discuss the quest directly but hints at the "Heavenly"
  beasts and an omen of violent death.
- **Faithful to the Runner.** No SCARE change and no combat-assist needed; this
  is simply an unfinished/intro release with no end state authored.

## Reproduce

```
sh harness/play.sh "games/SRSintro.taf" harness/srsintro_solution.txt
```
