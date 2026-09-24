# Dung Beetles Are Aliens! — walkthrough (**UNWINNABLE, intro build**)

- **Source:** IntroComp 2009 entry (`dbaa!(intro).taf`). Comic science-fiction
  story: an entomologist, Dr. Malcolm Fitzwold, runs his life and research
  through a home computer terminal ("MAWINDEX") while a Zegathean war slug
  blockades his own front doorstep demanding tribute.
- **Engine:** **ADRIFT 3.90**, second person. Two halves: a menu-driven
  computer-terminal "room" (MAWINDEX) and a small physical world (the
  Laboratory and the Doorstep) reached by typing `X` (exit) at the MAWINDEX
  main menu.
- **Content review:** comic sci-fi, no sexual content, no minors.
- **Result:** **UNWINNABLE** in this intro build, and deliberately so. Per
  `SCR_DUMP_TASKS`, only 15 restrictions/actions in the whole game reference
  the Zegathean war slug object, and every single one of them gates on its
  "demanding tribute" state; no task anywhere ever moves it out of that
  state. Task 383, the catch-all `give <object> to slug` handler, always
  responds "Bring me something else!" regardless of what is offered, and the
  two `get slug`/`get the death ray gun` tasks are hard-wired to "To do so
  would be suicide!". The Mayor's auto-reply mail message ("You are a
  kook!... deal with them as personal or private matters") confirms this
  stonewalling is the intended fiction, but no tribute item that satisfies it
  exists anywhere in this intro's content — the meadow south of the doorstep
  is exactly as unreachable as the Laboratory's north/up exits, which the
  game explicitly labels "not available in the intro version." Wired as
  `dbaa_solution.txt|dbaa!(intro).taf|Bring me something else!`, no env.

## Two parser gotchas worth knowing

- Bare `i` at the MAWINDEX main menu resolves to the built-in `inventory`
  verb rather than the `<I>NTRODUCTION/INFORMATION` menu option. Typing
  `intro` (the full word) reaches the intended submenu instead.
- The Introduction/Information submenu's `<H>int decryption matrix` and the
  Network Access submenu's `<D>etail disabled function(s)` are dead-ends by
  design (matching the game's in-fiction "the hints haven't been decoded
  yet" / "quarantined" framing), not missing content.

## The walkthrough

Tours every MAWINDEX submenu in turn — mailbox (reading the Mayor's
dismissive reply to the slug complaint), archives (an insect-research log),
the word processor (drafting and previewing a message), the
introduction/information menu (tutorial toggle, verb list, hint-matrix
stub), network access (a flavor news article on the "unidentified object"
that seeds the game's plot, plus the disabled-functions notice), all four
domestic tasks (dump trash, fan status, lock/unlock entrances, sterilize
fridge), and the error-terminated "Energize UFO Laser Defence Unit" option —
before exiting to core gameplay. In the Laboratory, takes the flashlight and
Geiger counter off the specimen table and confirms both interior exits
(north to the back room, up to the bedroom) are stubbed out as
intro-unavailable. Outside on the Doorstep, the Zegathean war slug blocks the
way south; the walkthrough examines it, runs one exchange of its dialogue
tree, tries to hand over the flashlight as tribute, and is refused — the
true, deterministic dead end of this preview build.
