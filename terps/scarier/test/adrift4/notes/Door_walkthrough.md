# Door — walkthrough (**WIN, no score, 5 moves**)

- **Author:** Eric T. Dorrath (Delron), for the InsideADRIFT Summer Competition
  2008. One room — a kitchen — and one joke.
- **Engine:** **ADRIFT 4.00** (`xxd -p -l 12 games/door.taf | cut -c17-22` →
  `93453e`). The readme says "Adrift 4.00 R51".
- **Result:** **WON** in five commands — "You head south. You have escaped. /
  THE END". No score of any kind. Wired as
  `door_solution.txt|door.taf|You head south. You have escaped.`, no env.
- **Source:** the author's own `walkthru.txt`, bundled inside
  `SummerCompGames08.zip` at `games/doordocs/walkthru.txt`. Copy kept as
  `downloaded/Door_walkthrough.txt`. Followed **verbatim**, all five lines, no
  corrections needed.

## The game

```
open unit
get jar
unlock door with jar
open door
south
```

That is the whole walkthrough and the whole game. The unit next to the cooker
holds a packet of coffee and a glass jar; unlocking the back door with the jar
answers

> When is a door not a door?  When it is a jar!!!
>
> You hear the back door unlocking.

and opening it adds "I guess it is now ajar." The fridge, washing machine,
window, table, dog basket and fuse box in the room description are all pure
scenery — nothing on the route touches them.

## Why it was not found earlier

`Door` never had an IFDB walkthrough link, because the author shipped the
walkthrough inside the comp archive rather than publishing it separately. Same
story as *Silk Noil* and *The Wheels Must Turn* — see the note at the top of
`downloaded/INDEX.md`.
