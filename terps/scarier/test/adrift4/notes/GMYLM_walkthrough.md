# Give Me Your Lunch Money — walkthrough (**WIN, no score**)

- **Author:** "DCBSupafly", 2010. *A Tale of the Exploits of an 8 Year-Old
  Inventor* — you are bullied for your lunch money, so you booby-trap the
  playground.
- **Engine:** **ADRIFT 4.00** (`xxd -p -l 12 games/GMYLM_2010.taf | cut -c17-22`
  → `93453e`).
- **Result:** **WON** — the game prints `- - - Victory! - - -` and then a credits
  page. No score. Wired as
  `gmylm_solution.txt|GMYLM_2010.taf|Victory! - - -|SCR_SKIP_WAITKEY=1`.
- **Source:** the author's bundled `WALKTHRU` (copy kept as
  `downloaded/GMYLM_walkthrough.txt`) — **prose, not commands**. It says so
  itself: "Commands below will not function if used verbatim, but should be taken
  as general instruction." The 65-line solution here is that prose turned into
  real input, then checked against the game.

## Two things the harness needs

- **`SCR_SKIP_WAITKEY=1`.** The intro is paged with keypress prompts; without it
  they eat the first commands of the solution.
- **The marker drops a leading dash.** The banner is `- - - Victory! - - -`, but
  `run_v4_walkthroughs.sh` passes markers straight to `grep -F`, and an argument
  beginning with `-` is read as an option. The row greps `Victory! - - -`
  instead.

## The shape of it

Four traps have to be built and armed on the playground east of the school, and
then triggered from the crawl tube the next morning.

```
w / w / open box / get fishing line / e     trap 1: the fishing line
show pockets                                the bullies at the park
n / turn on spigot                          mud in the front yard...
n / n / n / open cabinet / get bucket
s / s / s / get mud                         ...needs the laundry-room bucket
s / e / e / set up mud                      trap 2 armed
w / w / n / get hose / n / w / n
x table / get watermelon
use hose with watermelon                    trap 3: the filled watermelon
e / u / s / x rare bears / get underwear     trap 4: Sis' Rare Bears underwear
n / d / s / s / s / e / e
set up watermelon / set up underwear / set up fishing line
w / w / n / n / n / u / n / go to bed
climb tube / wait x6 / pull strings x4       the next morning
```

`set up` is the game's own verb for arming a trap, and it is what the game asks
for in so many words ("For now, I need to set up my traps here on the
playground"); each one answers with its own rigging text — "I prop the mud
bucket up on the top of the monkey bars…". The four `pull strings` at the end
are one per trap.

The route is seed-independent: it still wins under `SCR_SEED=7`, so the row
carries no `SCR_SEED`.
