# Endgame + economy (agent report, condensed)
## Endgame
- All 8 winds → gold_hammer in Shrine (`take hammer`).
- Get sleep mask: Grand Kingdom Day Spa (off Box Street Road) first visit (table). Carry light (lamp gets broken).
- Amphitheater (Lamp there? we carry Lamp — drop it?): `use hammer on lamp` (NOT `break lamp with hammer`) → broken lamp w/ golden egg. `wear mask` (not during 0-300), `take egg`.
- wait (`z`) until Black Knight enters (he wanders outside rooms from Fountain Meadow at 1080; spawn only when he enters YOUR room; window 1081-1139). Wait in Amphitheater/Forest Glade.
- `break egg` immediately. ~83s timers (drained) → note_kingsnote10 appears. `take note`, `read note` (30s → beatgame). `wear mask`, `remove mask` (bug fix; teleports to Amphitheater). `drop note` in Amphitheater → You Won!!!
- `give egg to knight` = lose. Don't fight knight (400hp).
## Combat/economy
- Armour: head x4, torso x6, others x2. damageresistant items −1/hit each.
- Weapons: Armoury Sir Jack (Town Center): bronze dagger $37, crossbow 2d4. Silver Dagger D3 r10; Gold Dagger code likedagger 1d9; Silver Sword D6 r5; Gold Sword D7 r1 1d11; King's sword D8 r51 2d15 (+8 `say I will`; do NOT sharpen). Grabthar's hammer: `say grabthar` (free) −5 to-hit except crab.
- Sharpening: Ericka, Red Lands Blade (off Street of Citrus): `give X to smith`, `tell smith to sharpen` $35 dagger/$65 sword. Training Thine Gym.
- Armour codes: glitter (gold vest 3), rattle (snake vest), hardheart (stone vest), hiss (snake boots), snakeinmyboot, thesharks (gorget dmgres), letitgo (zinc bracelet rollhelper), shocking (eel socks), superhealth (B12). Gold Helmet D5 r11, Gold Shield D5 r0, Stone Boots D6 entrance, Gold Gauntlets D4 r5. Belt of Truth $37 Red Bird (fire+arrow res, dmgres).
- Healing: bottle 10 sips, `drink from bottle` 5+extras; refill Fountain Meadow/brickchapel. Spring water +3 (Glen, turn dry fountain on). Food. Seaweed 15. Red diamond stones +5 max HP each (code herring).
- Money codes: cisforcash $1000 (!), mrbig, cashorcheck, allthebills, kingsransom, moneymoneymoney, fifties...
- Sleep safe in INDOOR town rooms. Night 0-300 sleepiness; outside dark 1260-300.
## As played (the frozen override, 2026-10-18)
- The gold medallion appears only after `search giant cave dragon` (plain "search dragon" is ambiguous with Dragon's Treasure); `use gold medallion` at the Shrine, `look`, `take golden hammer`.
- Worn items and container contents count toward the carry limit (`GetAllChildObjects`), so drop the rings, the silver dagger and the gold necklace before the mask juggling or `remove mask` fails. Don't try to drop things while masked: the room is dark.
- Time skip: buy Sleep Mist Spray at the Day Spa ($80, 4 uses; menu `1`), then with the mask on use it (pick the carried spray, the last "Sleep..." menu option) for +240 each; wearing the mask at 0-300 also gives +60 via FallAsleep.
- After `break egg`, `look` before `take note` (otherwise "I can't see that").
- King's Sword unobtainable: kingsnote5 (needed for the hermit's dynamite) comes only from the first `chest_random_treasure` opened, and the shared `firsttime` flag leaves the room-51 chest empty.
