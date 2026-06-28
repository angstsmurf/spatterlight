# Phoenix Destiny: Book One — walkthrough

- **Author:** Chris Tyson ("Eternal Adriftware").
- **Engine:** ADRIFT 4, with a heavy custom RPG layer (character creation,
  classes & spells, survival vitals, a real-time clock, and a town economy).
- **Result:** **No win is possible — and there is no score (0/0).** This is an
  **unfinished beta:** a large, lovingly-built RPG *town* with the actual
  adventure never implemented. Walkthrough below + a deterministic
  systems-tour solution `harness/phoenix_destiny_solution.txt`.

## The promised quest vs. what's actually in the file

The intro is a long cut-scene: the phoenix-god **Kilik** gives you a golden
**amulet** containing his soul and tells you to carry it to **Opus**, the god of
eternal life, atop **Mount Yuko**, or time itself will end. None of that exists
in the game data:

- All **34 rooms are the Town of Azeroth** (your hut, the surrounding road, and
  the town's shops/streets/guild). There is **no Mount Yuko, no Opus, no amulet-
  delivery task, no mountain route** — the dump confirms it.
- A full structural dump (177 tasks) shows **zero `ChangeScore` actions** ⇒ no
  score, and **no win** `EndGame` (no type-6 with var1=0 "WinText").
- The only `EndGame` actions are **one timed lose** (`#endgame`, var1=1, fires
  when the in-game *year* variable reaches **1241** — about five years after the
  Jan-1236 start, i.e. effectively unreachable) and **four survival deaths**
  (`#nofood`, `#nowater`, `#noenergy`, `#nostomach`). So the *only* outcomes a
  player can actually reach are death by starvation/thirst/exhaustion.

In short: faithful to the data and the original Runner, **Book One has no
ending you can win and nothing to score** — the story stops at the town.

## What *is* implemented (and works)

This is the interesting part — it's a remarkably complete RPG town engine:

- **Character creation** — name, gender, **race** (Human / Elf / Dwarve),
  **class** (Warrior / Wizard / Ranger), and numeric height / weight / age.
- **Survival vitals** that deplete over time: Energy, Food, Water, Stomach
  capacity, plus an Alcohol level (and a hangover mechanic). Let any of the
  first four hit zero and you die. Refill them at the **stream** (`drink water`),
  the **bakery**/**tavern** (bread, steak, chicken, potato — but watch Stomach
  capacity), etc.
- **A real-time clock** (minute/hour/day/month/year tasks) — and **shops keep
  opening hours**: try to buy before they open and you get *"The shop isn't open
  yet."*
- **A town economy**: the **Bank** offers loans (`take out loan`,
  `repay N coins`) and a **stock market** (`buy share` / `sell share`); you start
  with **100 gold**, but gear and training cost far more (e.g. the Warrior class
  costs **312 gold** from Besk), so you must work the economy first.
- **Three class trees** with their own trainers, classrooms (`read the tutor` /
  `read the book`), level-ups, and a deep **spell/skill system** for Wizards
  (manaball, fireball, lightning, heal, drain, tribeam, summon, armageddon) and
  **Rangers** (bows, longbows, crossbows, quivers, ammunition). Warriors get a
  starting sword from the guild.
- **Shops**: Armoury (vests/armour/shields), Blacksmith (knives→katana),
  Tavern (drink/food), Madam Karlan's Potions, Bakery, Jewellery, Corentus's
  Magic Store, Ringlock's Bow Shop, the Traveller's Shop (bags), and the Bank.
- NPCs to `chat to` everywhere (Berlock, Jewel, Ormula, Karag, the bartenders,
  Father Aldin in the Visions Room, Besk the guild master, …).

## The tour (deterministic, ends with a clean quit)

`harness/phoenix_destiny_solution.txt` — 13 leading blank lines clear the intro
cut-scene (it takes exactly 13 key-presses), then:

```
Hero            <- name
male            <- gender
Human           <- race
Warrior         <- class
height 180
weight 80
age 25
west            <- Outside Your Hut
southeast       <- By The Stream
drink water     <- refill Water vital (it rises)
northwest       <- back outside the hut
west            <- At The Front Gate
west            <- The Main Road
south           <- "you start walking down the road… to Azeroth"
south           <- Idan Road, into town
south
south           <- At An Intersection (the town hub)
date            <- shows the clock (1st Jan 1236, ~07:36)
vitals          <- shows Energy/Food/Water/Stomach/Alcohol
wealth          <- Gold: 100
```

From the hub (`At An Intersection`) every shop and the guild are a step or two
away (N = the streets to the guild/bank/magic-store/bow-shop, E/S/W = the shops),
but with no quest and no score there is nothing to complete — the tour simply
demonstrates the engine and stops. Verified identical 3× under the deterministic
harness.

### Reproducing the systems (optional, for the curious)

- **Water/food**: `drink water` at the stream; buy & `eat` bread/steak/etc. in
  town once shops open. Watch `vitals` — Stomach capacity caps how much you can
  eat at once.
- **Money**: at the Bank, `take out loan` and/or `buy share` … `sell share`;
  this is the only way to afford armour, weapons, potions, or class training.
- **Class**: once you can pay, `join warrior class` (312g) and `recieve sword`
  at the guild Entrance Room; Wizards/Rangers train and buy spells/bows in their
  respective shops.

## Honest notes

- **No win and no score are reachable** because none exist in the data (no
  `ChangeScore`, no var1=0 `EndGame`; the Opus/Mount-Yuko half of the story is
  simply not built). Faithful to the data and the original ADRIFT Runner — not a
  SCARE limitation.
- **Two start-up prompts** (name + gender) plus the custom race/class/stat
  prompts; SCARE honours the name/gender prompts (commits `8d9c9426`/`2e74a6e6`).
  The intro cut-scene consumes exactly **13** key-presses before the name prompt.
- The Battle System is present (spells, weapons, bows all use it) but no fight is
  required or rewarded, so `SC_ASSUME_COMBAT` is irrelevant.
- Deterministic seed-1234 harness; the tour reproduces the same clock/vitals/gold
  every run.
