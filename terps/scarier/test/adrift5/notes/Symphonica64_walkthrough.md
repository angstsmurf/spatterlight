# Symphonica 64 — Complete Walkthrough (★ WON 74/74)

*Chris Abbott, Anna Black & Damian Manning (2015). ADRIFT 5. IFID `ADRIFT-500-3BF2-4721-4C38-907C-8594B090F5F4`.*

A zany C64-nostalgia comedy. You play a composer who must collect **all 74 members of the
"Scores" group**, assemble the **Orchestra**, fetch the **gadget**, trade it to **Sir Stephen
Fry** for a Ticket, and enter Extensionland to win.

> **Verification status — this is the winning script.** Every command below is the exact,
> reproducible sequence from the harness-verified golden `symph_work/golden_cmds.txt`:
> **74/74 scores held, 0 failures, ends on `*** You have won ***`, and 100% godmode-free.**
> Run it headless with `python3 drive.py golden_cmds.txt` (SCORES HELD: 74 / FAILS: 0).
> The score-by-score sections in §4 are transcribed verbatim from that golden, so they are
> guaranteed to work in order.

---

## 0. The "two impossible codes" — the truth

The intro claims the game *"cannot be finished without two codes"* released only in long-gone 2013
Kickstarter videos. **This is a bluff. You need exactly one code, and it works:**

| Code | What it does | Needed? |
|------|--------------|---------|
| **`code 348783654876`** | Reveals the **Ocean Storybook** at Sidventure Port | **YES — mandatory** (no other source) |
| ~~`code 5F985GH84EFR`~~ | *Intended* to reveal the C64 Compendium | **NO — and it's broken** |

Code 2 is a dead command (its object `5f985gh84e` was never defined in the shipped data, so the
parser can never match it), but it doesn't matter: the item it was meant to deliver — the C64
Visual **Commpendibun** — is freely obtainable with `get commpendibun from sam` (at *22. Bitmap
Box Co.*). So the game is **fully winnable with only `code 348783654876`.**

### All secret codes (for completeness)
| Command | Effect |
|---------|--------|
| `code 348783654876` | Reveal **Storybook** at Sidventure Port *(mandatory)* |
| `code 56F6G7EF` | Spawn a **chicken** at the vending machine (part of the desert unlock) |
| `code xyzzy` | Gives you the **Mogwai** |
| `code ninja-be-gone` | Moves the **Litigious Ninja** (alternative desert opener) |
| `code honorabili` | Flavour, no state change |
| `code a laughing llama` | **GODMODE** — debug/scouting only; **NOT used anywhere in this walkthrough** |

---

## 1. Navigation

Two systems, usable at any prompt:

**Zone teleports** (jump to a zone's entrance, keeping inventory):

| Command | Entrance | Requires |
|---------|----------|----------|
| `zone symphonica` | Ben And Ratt back room (main-street hub) | always |
| `zone wastelands` | Sidventure Port | always |
| `zone fantasy` | Cheesecake Mountain | **Canaccessp** (give Storybook to the pirate) |
| `zone oriental` | IK Temple | **Canaccessp** |
| `zone desert` | Desert 17 | **Canaccessd** (see the DESERT UNLOCK section) |
| `zone wilderness` | The Whit Taker | **Canaccessw** (give Compendium + ZX81 + Medal to Gaz) |

**Walking** the grid with compass directions (`n e s w u d`). The numbered shops (01…37) and the
lanes between them form the central hub; themed zones connect through their teleport entrances.

**Verbs** (from in-game HELP): `give X to Y`, `get X from Y`,
`take / drop / open / put / use / show / play / attack … with … / eat / buy / talk to / examine /
smell / wave / dig`. *"Most characters only want one thing."*

> **Two crucial parser footguns this script relies on:**
> - **Dative giving.** `give <obj> to <name>` can fuzzy-match the *wrong* NPC's task. Always use
>   the dative form **`give <char> the <obj>`** (e.g. `give minstrel the hewson soundtrack`) so the
>   character binds first.
> - **Disambiguate items by full adjective.** When two things share a noun, use the full name
>   (`take underground diamond`, `take hewson soundtrack`, `take 476 droid`) or `take <adj> score`.

---

## 2. The winning path, at a glance

1. **Open fantasy/oriental:** `code 348783654876` → `zone wastelands` → `take storybook` → give it
   to the pirate on Sievey Street. Canaccessp set.
2. **Open the desert** (chicken → ink, firebird → feather → quill pen, sign & give the contract to
   the ninja). Canaccessd set.
3. **Open the wilderness** (auto → piano → grand-theft-auto → Penn; give Commpendibun + ZX81 + Medal
   to Gaz at Top Of Tower). Canaccessw set.
4. **Sweep all six zones** collecting the 74 scores (§4). Many scores *drop on the floor* when
   awarded — always `take` them afterwards.
5. **Endgame** (§3) once all 74 are held.

---

## 3. Endgame (harness-verified, godmode-free)

With **all 74 scores held**:

1. Go to **Symphonic Penn Avenue** (Location1317) and head **South** — the exit is gated on
   `%NumberOfScoresHeld% >= %NumberOfScoresInGame%` (all 74), so it only opens now — into the
   **Amphitheatre Symphonie 64** (Location1273).
2. `take orchestra` — the expensive orchestra is waiting here at this point in the game.
3. Go **Down** into the **Royal Albert Hall** (gated on holding the Orchestra).
4. `take gadget` (the fascinating retro gadget).
5. `drop orchestra` — going back **Up** requires *not* carrying it ("leave it here with the
   scores"). Then **Up**, **North**, back to Penn Avenue.
6. Walk (via `zone wastelands`) to **Sir Stephen Fry** at *Us Us Boom Boom Bus Debrief*
   (Location1364, reached through the wastelands map via L1363 East).
7. `give fry the gadget` → he hands you the **hot Ticket**.
8. `49587948EFG` → *poof* → **`*** You have won ***`**. *(This code needs only the Ticket in hand.)*

The exact commands are the last two sections of §4.

---

## 4. The complete verified command script

Transcribed section-by-section from the winning golden (2238 commands total). Type them in order;
each block's heading names the score or milestone it accomplishes. Blank/`i`/`score` lines are just
status checks and are safe to skip.


### 1. Setup — opening moves & the mandatory code

```
code 348783654876
zone wastelands
take storybook
zone symphonica
East
South
West
West
North
North
East
North
North
give storybook to pirate
zone wastelands
South
South
South
South
South
South
East
East
East
North
North
North
North
North
West
West
South
East
North
North
North
West
South
take deal
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
North
West
West
West
West
South
give deal to follinator
North
East
North
take rampack
South
East
East
East
South
East
South
East
East
East
East
South
West
give rampack to hibbett
zone wastelands
South
South
West
West
West
take edge
zone fantasy
North
North
North
West
South
South
South
South
East
give edge to tim
zone symphonica
East
South
West
West
West
West
take drum
East
East
East
East
North
give drum to ben
South
West
West
North
North
North
North
West
West
West
South
West
West
West
West
South
South
South
South
East
take scarf
West
North
North
North
North
East
East
East
South
West
give scarf to crispy
take shades score
zone oriental
East
South
East
East
North
West
West
South
take missile
North
East
East
South
South
West
give missile to tentacle
take guybrush score
zone fantasy
take cheesecake
zone wastelands
South
South
South
South
South
South
East
East
East
North
North
North
North
North
North
East
East
give cheesecake to stesher
take capes score
zone fantasy
North
North
North
North
attack gari with bow
take renegade score
West
West
West
South
South
South
take kentilla score
North
North
North
East
East
East
South
West
South
West
South
West
South
take deflektor score
```

### 2. Score26: give a sidderie to The Knights Who Say SID

```
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
North
West
West
West
North
take siddery
zone fantasy
North
North
North
West
South
South
South
South
South
South
give siddery to knights
take bell score
```

### 3. DESERT UNLOCK (chicken->ink, firebird->feather->pen, sign contract)

```
code 56F6G7EF
zone wastelands
North
West
squeeze blue chicken
take egg
break blue egg
zone oriental
East
South
West
South
East
East
South
South
West
North
take firebird
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
West
West
West
South
West
West
South
South
West
South
West
put firebird in dryer
take feather
use feather on blue ink
East
South
sign vague contract with quill pen
give contract to ninja
```

### 4. Score54: give a token to Sir Paul of Normandy

```
zone desert
West
South
West
North
North
West
West
North
East
East
East
North
West
West
West
take token
South
give token to paul
```

### 5. Score38: attack Howard Kistler with an 8-bit weapon

```
zone desert
West
South
South
West
West
West
take weapon
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
North
West
West
West
West
West
West
West
South
West
attack victim with weapon
```

### 6. Score40: use rude note on CRT -> THRUST score

```
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
North
West
West
West
North
take note
zone wastelands
East
North
use rude note on crt
```

### 7. Sid2: undiscovered ancient SID @ Dodgy Geezer Pyramids (desert)

```
zone desert
West
South
West
North
North
East
East
East
East
East
take undiscovered sid
```

### 8. Roll/Score53/Score64 batch

```
zone desert
West
South
West
North
North
East
East
get music roll from rick
zone symphonica
East
South
West
West
North
North
West
West
West
West
West
West
West
West
West
South
East
get miami vice score from squad
West
West
West
North
wave at sidwave
```

### 9. Score50: germ auto-present -> give to Kistler(victim)

```
South
East
East
North
North
North
West
West
North
West
West
West
West
South
West
give germ to victim
take germ score
```

### 10. Score16: trouser-dye chain (Turner->Steve; dirty->Archie->soggy->Steve->white->Sam->red->karateka) -> potplant

```
zone wastelands
South
South
West
West
South
South
West
West
take steve turner
zone symphonica
East
South
West
West
North
North
West
West
West
West
West
West
West
West
West
South
West
West
West
South
South
South
South
West
give steve turner to steve
zone desert
East
South
East
North
East
East
East
South
West
Down
South
take dirty trousers
zone oriental
give dirty trousers to archie
zone symphonica
East
South
West
West
North
North
West
West
West
West
West
West
West
West
West
South
West
West
West
South
South
South
South
West
give soggy trousers to steve
East
North
North
North
North
East
East
East
North
North
North
West
West
South
West
West
South
South
East
North
give white trousers to sam
zone wastelands
South
South
South
South
South
South
East
East
East
North
North
North
North
North
North
East
East
give red trousers to karateka
get ik score from pot plant
i
```

### 11. Score31: Unicorn -> Tel Memore (Trophy -> Filipp)

```
West
West
South
West
West
South
East
North
North
North
West
take trophy
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
South
West
give trophy to filipp
East
North
East
South
East
East
East
East
East
North
give unicorn to tel
take cybernoid score
```

### 12. Sid3: SID Chip -> Landroid (Thing1 -> Mat)

```
zone desert
East
South
East
North
East
East
East
South
South
South
West
South
take thing
zone wastelands
give thing to mat
East
North
give sid chip to landroid
```

### 13. Score22(Hunting): duck->Toni->Duck1->dog@Ducking Hunt

```
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
North
West
North
take duck
zone desert
West
South
South
West
West
South
East
East
East
give duck to toni
zone oriental
East
South
East
give duck to dog
take hunting score
```

### 14. Score2(Egyptienne): bovver remover@1245->use on bovver@142->take hovver->mow grass->camel->Jeff@1143

```
zone symphonica
East
South
West
West
North
North
West
West
West
North
take bovver remover
zone fantasy
North
North
North
West
South
West
North
West
use bovver remover on bovver
take hovver
mow grass
zone desert
West
South
West
West
North
West
give camel to jeff
take egyptienne score
```

### 15. Score5(Loader): wibbles-r-us@1275->Ben fixes@1242->show to Mart@1238->take loader score

```
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
North
West
West
West
West
West
West
West
North
take wibbles-r-us
South
East
East
East
East
East
East
East
East
East
East
East
East
East
South
South
South
South
East
East
North
give wibbles-r-us to ben
South
West
West
North
North
East
East
South
South
show wibbles-r-us to mart
take loader score
```

### 16. Score41(dig Doug@1106)+Remix2(dig dirt@144,take remix) via Spade@138

```
zone fantasy
North
take spade
zone desert
East
South
East
North
East
East
East
South
South
West
West
dig doug
zone fantasy
North
North
North
West
dig dirt
take remix
```

### 17. Score6(delta:talk rob@1239)+Score9(Commando:moustache rob->Mart@1238); enter 1239 via Portrait(talk trev@134,wear)

```
South
South
South
talk to trev
wear portrait
zone symphonica
East
South
West
West
North
North
East
East
North
talk to rob
take delta score
get moustache from rob
```

### 18. Soundtrack1(Genesis): Rob now Amazing Rob -> talk drops soundtrack

```
talk to rob
take soundtrack
South
South
South
give moustache to mart
```

### 19. KondoScore+Score14(Giana): joint@1242+organ@1271->stand@1239->play geoff capes score->barrel->gorilla@152 (squishes plumber AJauntyPlu)

```
North
North
West
West
South
South
East
East
North
take joint
South
West
West
North
North
North
North
West
West
West
South
West
West
West
West
South
West
North
take organ
South
East
North
East
East
East
East
North
East
East
East
South
South
East
East
North
put joint on stand
put organ on stand
play geoff capes score on organ
take barrel
South
zone wastelands
South
South
South
South
South
South
East
East
East
North
North
North
North
give barrel to gorilla
take kondo score
take giana score
```

### 20. Score51(attack Kistler@1272 with barrel): replay geoff score->barrel

```
zone symphonica
East
South
West
West
North
North
East
East
North
play geoff capes score on organ
take barrel
South
West
West
North
North
West
West
West
South
West
North
West
West
West
West
West
West
West
South
West
attack victim with barrel
```

### 21. Score28(pianistic Crispy Huelsbeck relay): talk mike@1270->blank->give patrick@1248->rough->give mike@1270

```
East
South
South
East
North
North
East
South
talk to mike
take blank score
North
East
North
East
East
East
South
West
give blank score to patrick
East
North
West
West
West
South
West
South
give rough score to mike
```

### 22. Score20(Frontier): orchestra@1265->give uncle art@1251->take frontier score (multiword name)

```
North
East
North
East
East
East
South
East
take orchestra
West
South
West
South
South
South
South
West
South
give orchestra to uncle art
take frontier score
```

### 23. WILDERNESS unlock (auto@victim->piano->Auto2->Penn@Zzap; Commpendibun@Sam+Zx81@135+Medal@Thompson181 -> Gaz@TopOfTower) + Score55(glasses@128->biker@14->crisps->bowl@1248)

```
North
East
North
North
North
North
North
North
West
West
North
West
West
West
West
South
West
get auto from victim
East
North
East
East
East
East
East
East
East
South
East
South
East
East
North
use auto on piano
South
West
West
North
West
West
West
West
South
West
West
South
South
East
North
get commpendibun from sam
zone fantasy
North
North
North
West
South
South
South
South
South
West
West
take glasses
East
East
North
North
North
North
West
South
West
South
East
take zx81
zone wastelands
South
South
South
South
South
South
East
East
East
North
West
West
open thompson
take medal
open medal
zone symphonica
East
South
West
West
South
South
South
give auto to penn
East
South
give commpendibun to gaz
give zx81 to gaz
give medal to gaz
zone wilderness
East
South
South
South
South
South
give glasses to biker
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
South
West
put crisps in bowl
```

### 24. Score52(surreal foot): Psyclapse@179->Mat@163->Jetpac; get foot from ledge@129(Foothills#1, jetpac-gated); attack victim@1272 with foot

```
zone wastelands
South
South
South
South
South
South
East
East
East
North
West
West
North
West
West
South
East
take psyclapse
West
North
East
East
South
East
East
South
West
West
West
North
North
North
North
North
North
give psyclapse to mat
zone fantasy
North
North
North
West
South
South
South
South
South
West
get foot from ledge
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
North
West
West
West
West
West
West
West
South
West
attack victim with foot
```

### 25. Score25(Cannon Fodder)+Score34(Wizball): open war@1243->get platoon from war->take war->give war to jops@1240 (spawns JovialJops)->take fodder; talk to mart@1238->wizballs->give wizballs to jops->take high

```
East
North
East
East
East
East
East
East
East
East
East
East
East
East
East
East
East
East
open war
get platoon from war
take war
West
South
South
West
South
South
West
South
give war to jops
take fodder score
North
North
North
East
East
East
South
South
talk to mart
North
North
West
West
South
South
West
South
give wizballs to jops
take high score
```

### 26. Score23(Uridium)+Score49(Paradroid) droid-swap: 476 droid@1165(999 room)->drop in Location22(476); 999 droid@22->drop in Location1165(999)

```
zone desert
West
South
West
North
North
West
West
West
West
North
take 476 droid
zone wilderness
East
East
South
South
South
East
East
North
take 999 droid
drop 476 droid
take uridium score
zone desert
West
South
West
North
North
West
West
West
West
North
drop 999 droid
take paradroid score
```

### 27. Score43(hamster): hamster@165+lettuce@149 -> put hamster in microwave@1196 (->golden hamster) -> give lettuce to hamster (->Hamster3) -> carry to Anna@1261 (Task12 loctrigger fires)

```
zone wastelands
South
South
South
South
take hamster
zone fantasy
North
North
North
North
North
West
take lettuce
zone oriental
East
South
West
South
East
East
South
South
South
West
put hamster in microwave
give lettuce to hamster
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
West
West
South
```

### 28. Score44(purple tentacle): pull cow@1199 (DerangedPu follows) -> carry to Location1216 Oriental Plans (PurpleTent loctrigger fires)

```
zone oriental
East
South
East
East
South
West
pull cow
East
North
West
West
West
South
East
East
South
```

### 29. Score4(Thing on a Spring): spring@157 -> give spring to thing with no spring@132 (follower) -> carry to Anna@1261 (AuntieAnna1 loctrigger) -> take spring score. Multiword NPC noun.

```
zone wastelands
South
South
South
South
South
take spring
zone fantasy
North
North
North
West
South
South
South
South
give spring to thing with no spring
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
West
West
South
take spring score
```

### 30. Score62(Doom): after Score44 the missile-holding DerangedPu is at 1216 -> StartArmag auto-produces Doom -> drop doom@171(River Bank of Doom) -> take doom score. Depends on Score45(guybrush give=missile onto tentacle)+Score44.

```
zone wastelands
South
South
South
South
South
South
East
East
East
North
North
North
drop doom
take doom score
```

### 31. Score24(mule): take mule score@1138(Mule For Hire). Score39(Cauldron II): take cauldron score@1266(Witch's Hair Lock Shop, in pumpkin, no open needed).

```
zone desert
East
take mule score
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
West
West
North
take cauldron score
```

### 32. Score3(Hubbard,in sofa)+Score37(Sensible Soccer): enter 1239(portrait still worn), open sofa->take hubbard score; carry Wizmore@1255->give to team->take soccer score.

```
South
West
South
South
South
West
South
East
take wizmore
West
North
East
East
East
East
East
East
East
East
East
East
East
North
open sofa
take hubbard score
give wizmore to team
take soccer score
```

### 33. Score32(Sensible/stew): get stew@1240, put stew in cauldron@1255(Wiz And Nifta, cauldron+wizmore share room), take sensible score.

```
South
West
West
South
South
West
South
take stew
North
North
North
West
West
West
West
West
West
West
West
South
East
put stew in cauldron
take sensible score
```

### 34. 1365 cluster (Dizzy's Treasure Island): give offer(already carried from L2109 Bird Sanctuary)to victim@1272->money; money->grannell@1269->apple; get turner from steve@1257; L159 Down(needs apple)->1365. Score35(give apple to not dizzy,take dizzy score), Score48(use turner on jobs->get apple score from woz), Remix3(dig sand,take remix).

```
West
North
North
North
West
West
North
West
West
West
West
South
West
give offer to victim
East
North
East
East
East
East
South
East
East
South
South
South
West
West
North
give money to grannell
South
West
South
South
South
South
West
get turner from steve
zone wastelands
West
West
give apple to not dizzy
take dizzy score
use turner on jobs
get apple score from woz
dig sand
take remix
```

### 35. Score46(Spellbound): Magic Knight follows from 1365 fall; climb Up to L159, walk to Location145(Spellbound)->Knight auto-awards Spellbound score.

```
Up
zone fantasy
North
North
North
look
```

### 36. Score21+Score67 (Uncle Chris@Location1101 Abbott's Bit, desert): take atari 400@1305(Vera Partner Lane); talk to chris(Score21 Master of Magic), give atari 400 to chris(Score67). NOTE: Atari needs full 'atari 400' noun; UncleChris cloc bug->XML says Location1101.

```
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
West
West
West
take atari 400
zone desert
East
South
East
North
East
East
East
South
South
South
West
talk to chris
give atari 400 to chris
```

### 37. Score63(Trap): use turner(still carried) on steve@Location1267(32.Kentilla Fried Chicken=SteveTheSo)->spawns Steve the Geordie; talk to steve->Trap score.

```
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
North
West
North
i
use turner on steve
look
talk to steve
```

### 38. Score59 (Hydrax): examine Garou@Location8/Grues (GarouTheNo follows), walk him into Location1364/Moon -> WolfMeetsM transforms to fluffy doggie following; get hydrax score from doggie

```
zone wilderness
North
North
examine garou
zone wastelands
South
South
South
South
South
South
East
East
East
North
North
North
North
North
West
West
South
East
North
North
North
West
North
North
East
East
get hydrax score from doggie
```

### 39. Score65 (Amstrad game): zone symphonica->take concept@L1237(Monty Mole Travel); zone wastelands->give concept to salesman@L1187/Commodore Quay (Ownership pun, salesman flees); West to yacht L161; talk to amstrad user (follows); East East In to L1124 pub -> GoingIntoT hides all 3 owner-groups; take amstrad game score

```
zone symphonica
East
South
South
take concept
zone wastelands
North
West
give concept to salesman
West
talk to amstrad user
East
East
In
take amstrad game score
```

### 40. Score33 (Lullaby): play gilligan's gold score on piano@L1246(Dunn And Dunn) -> Grandulator opens, take gold; carry to L1112/Gill Igans Gold -> drop gold -> reveals lullaby score -> take lullaby (NOT 'take lullaby score')

```
zone symphonica
East
South
West
West
North
North
West
West
West
West
North
play gilligan's gold score on piano
take gold
zone desert
West
South
South
West
West
drop gold
take lullaby
```

### 41. Score10 (Euronator single): take 80s loop@L126 + gold-singing lesson@L137 (both fantasy); give loop + give lesson to 'bandau spallet'@L1269/Van Grassy Knoll (FULL two-word noun; bandau/spallet alone fail) -> band writes cheesy 80s single -> give single to euronator@L1291/Nicol Lane

```
zone fantasy
North
North
North
West
South
West
South
West
South
take 80s loop
zone fantasy
North
North
North
West
South
South
take gold-singing lesson
zone symphonica
East
South
West
West
North
North
West
West
West
West
West
West
West
West
West
South
West
West
North
give loop to bandau spallet
give lesson to bandau spallet
zone symphonica
East
South
West
West
North
North
North
give single to euronator
```

### 42. Score58 (Tomb Raider): take synth@L1159/Marcel Dune (desert); go to Sids Castle L113 -> Rory (permanent follower) scares off Dragon32 (RoryAndThe/RoryAndThe1 auto-fire) -> East to Courtyard L112 -> North to Treasure Room L111 -> give synth to conz (Sir Conz)

```
zone desert
West
South
West
North
North
West
West
West
take synth
zone fantasy
North
North
North
West
South
South
South
South
South
South
East
North
give synth to conz
```

### 43. Score42 (One Man and his Droid): take hubbard flute@L1283(symphonica entrance); wear moustache (already wearing portrait); go to Max Hall L1329 -> play hubbard flute (PlayAHubba: needs MightyMax present + wearing Portrait+Moustache) -> Max gives score

```
zone symphonica
take hubbard flute
wear moustache
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
West
West
West
South
West
West
South
South
East
East
East
East
South
play hubbard flute
```

### 44. Score18: playlist->FastLoaders relay (moustache->Matt Gray Sanxion SID->Andy dodgy code->Waz Code of Ninja->FastLoaders; talk to Moon for playlist)

```
zone wastelands
South
South
South
South
South
South
East
East
East
North
North
North
North
North
West
West
South
East
North
North
North
West
North
North
East
East
talk to moon
zone desert
West
South
West
North
North
West
West
West
West
give matt the moustache
take sanxion sid
zone symphonica
East
South
West
West
North
North
West
West
West
West
West
West
West
West
West
South
West
West
West
South
West
give andy the sanxion sid
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
North
West
West
West
West
West
West
West
South
West
give waz the code
zone symphonica
East
give fastloaders the code of the ninja
give fastloaders the playlist
```

### 45. Score61 (Pitfall): Bandersnatch@L179->Mat@L163 gives Zorkmid->give executive@Twiddyville->enters theatre->N take score

```
zone wastelands
South
South
South
South
South
South
East
East
East
North
West
West
North
West
West
South
East
take bandersnatch
zone wastelands
give mat the bandersnatch
zone symphonica
East
South
West
West
North
North
North
North
East
give executive the zorkmid
North
take score
```

### 46. Remix1 (ancient Feekzoid): 001 droid from Bush@L159 -> use on His Droid@L11 -> obedient droid -> give One Man@L10 -> bent crook -> give geezer@L1151 -> N into pyramid -> take remix

```
zone wastelands
West
West
take droid
zone wilderness
East
North
East
East
use 001 droid on his droid
zone wilderness
East
North
East
give man the obedient droid
zone desert
West
South
West
North
North
East
East
East
East
East
give geezer the crook
North
take remix
```

### 47. Score17: atoms@L1210 -> give Atombender@L197 -> flangehammer -> attack Atombender (flees) -> S into HQ -> take C2N -> press play on tape -> talk to ppot

```
zone oriental
East
South
East
East
North
West
West
South
South
take atoms
zone desert
East
South
East
North
East
East
East
South
South
South
West
West
West
give atombender the atoms
attack atombender with flangehammer
South
take c2n
press play on tape
talk to ppot
```

### 48. Score7 (Defender of Crown): crazy mayhem@L139 -> put on pedestal@L1250 -> Sanxion poster -> Courtyard (Rory cleared dragon) -> attack knight with poster -> take score

```
zone fantasy
North
North
take crazy mayhem
zone symphonica
East
South
West
West
North
North
West
West
West
West
West
West
West
West
West
South
West
West
West
South
West
put crazy mayhem on pedestal
zone fantasy
North
North
North
West
South
South
South
South
South
South
East
attack knight with poster
look
take score
```

### 49. Score47 (Knight Tyme): crown@Courtyard -> give Lords of Midnight@L141 -> alarm clock -> use clock on Boz@L1264 -> voiceover -> give idiot@L1233 (he leaves) -> S into Spellbound Magic -> take score

```
zone fantasy
North
North
North
West
South
South
South
South
South
South
East
take crown
zone fantasy
North
North
North
West
South
West
give lords the crown
zone symphonica
East
South
West
West
North
North
North
North
West
West
North
use clock on boz
zone symphonica
East
South
West
give idiot the voiceover
South
take score
```

### 50. Score29 (Last Ninja/grow): talk to Whit Taker@L7 -> zombie torso -> give grues@L8 -> grue dung; take dung auto-makes Contract2 (smeggy contract in scope via ninja@patch); take seeds@L1192 -> put seeds+contract on kale patch@L187 -> GrowTheLas -> take ninja score

```
zone wilderness
talk to whit taker
take torso
zone wilderness
North
North
give grues the torso
take dung
zone wastelands
South
South
West
West
West
take seeds
zone wastelands
South
South
South
South
South
South
East
East
East
North
West
West
South
West
West
West
North
North
North
put seeds on patch
put contract on patch
take ninja score
```

### 51. Score30 (blender mega-quest): Portrait1->Clive=ZX book->Matt Smith opens Manic Mine; 7 ingredients (frog/mario/marshmallow/willy/arachnid/gribblies/mole) in blender+close+use=Smoothie; stir with flag; give hut(Tramiel) the squash. Spider: wait 8-10t at L1201 for ickle, attack with foot (needs engine ToSameLocationAs carrier-chain fix), take arachnid.

```
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
South
West
North
West
West
West
West
West
West
West
West
take clivey portrait
zone wastelands
give clive the clivey portrait
zone wastelands
South
South
South
South
South
South
East
East
East
North
North
North
North
take mario
zone wastelands
North
West
West
take mole
zone oriental
East
South
East
East
North
East
East
z
z
z
z
z
z
z
z
z
z
attack spider with foot
take arachnid
zone oriental
East
South
East
East
South
West
pull cow
take marshmallow
zone fantasy
North
North
North
North
West
take frog
zone desert
West
South
South
West
West
West
South
give matt the book
South
West
take underground diamond
North
attack willy with foot
take willy
zone desert
West
South
South
West
West
South
East
East
East
East
North
zone wilderness
South
South
South
South
South
West
West
North
North
North
give rockford the underground diamond
zone symphonica
East
South
West
West
North
North
West
West
West
West
West
West
West
West
West
South
South
South
South
West
South
put boulder on gribblies
take gribblies
zone symphonica
East
South
West
West
North
North
North
North
West
West
West
North
open blender
put frog in blender
put mario in blender
put marshmallow in blender
put willy in blender
put arachnid in blender
put gribblies in blender
put mole in blender
close blender
use blender
stir squash with flag
give hut the squash
take very early c64 score
```

### 52. Score36 (74th): Hewson soundtrack -> minstrel@L1190 (Firelord follows) -> Lords of Fire L143

```
zone wastelands
South
South
West
West
South
South
West
West
South
take hewson soundtrack
zone wastelands
North
give minstrel the hewson soundtrack
zone fantasy
North
North
North
West
South
West
North
```

### 53. ENDGAME: amphitheatre -> Albert Hall gadget -> Sir Stephen ticket -> code 49587948EFG -> WIN

```
zone symphonica
East
South
West
West
North
North
West
West
West
West
West
West
West
West
West
South
West
South
take orchestra
Down
take gadget
drop orchestra
Up
North
zone wastelands
South
South
South
South
South
South
East
East
East
North
North
North
North
North
West
West
South
East
North
North
North
West
North
North
East
East
give fry the gadget
```

### 54. all 74 held; final win code

```
score
49587948EFG
```

---

## 5. Running it yourself

Headless (from `terps/scarier/test/adrift5/harness/symph_work/`):

```
python3 drive.py golden_cmds.txt
# -> SCORES HELD: 74
#    FAILS: 0
```

The engine (`./test/a5run_dump symphonica.blorb`) is built with
`make -f Makefile.headless a5run` from `terps/scarier/`. One engine fix was required for this
walkthrough: `act_move_object`'s `ToSameLocationAs <character>` now resolves a follower's room
through its carrier chain, so the arachnid squashed *while riding the player* (Score30) is no
longer lost — see commit *"Scarier a5: ToSameLocationAs resolves follower carrier chain"*.
