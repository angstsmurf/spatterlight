# Veteran Experience — walkthrough

- **Engine:** ADRIFT **3** (native). Robert Street (as Robert Rafgon), ADRIFT
  2nd 3-Hour Comp 2004 (2nd place). Win the wrestling championship by
  dishonourably taking out every opponent before the match.
- **Result:** **WIN** — ends `…the winner by default is the Veteran.
  Congratulations on finally fulfilling your destiny.` Solution:
  `harness/veteran_solution.txt`; golden `veteran_solution.expected.txt`.
- **Walkthrough source:** *Key & Compass* (native-ADRIFT).

## Derivation

The mechanical extraction fails at the Monster: `hit The Monster with crowbar`
reports "You are not holding the crowbar." The archive's `take it` grabs the
**bag**, not the crowbar inside it — so the fix is to insert **`take crowbar`**
after opening the bag. With the crowbar in hand the rest runs 1:1.

Exercises the ADRIFT **3** code path. Route: crowbar from your locker → teddy
bear from the Youth → tacks + ladder from under the ring → Monster's room, give
teddy bear then `hit The Monster with crowbar`, take the acid → High Flyer's
room, give ladder + `put tacks on mattress`, take & `wear mask` → return to the
Youth and `throw acid at The Youth` (must be wearing the mask) to win.
