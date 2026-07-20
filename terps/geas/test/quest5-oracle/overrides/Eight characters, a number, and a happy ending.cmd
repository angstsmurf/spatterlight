# Eight characters, a number, and a happy ending (K. Grey / wyrdsmith.co.uk) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0 -- the genuine "Good" (happy) ending: COMPUTER SELF-DESTRUCT
# + password 28831010 destroys the Nova Blade before it can ignite the Iastrai sun ("Sacrifice
# and saviour: myth and reality of the Nova Blade" epilogue, "The game has ended.").
#
# SOURCE: the author's own game guide ("Eight characters, a number, and a happy ending.txt"),
# section 'FULL WALKTHRU: "Good" ending' -- a linear script, followed here nearly verbatim.
# (The guide is listed as hints-mode in corpus.tsv because most of it is Q&A, but its tail
# contains three full walkthroughs; this override wires the Good one. The other two endings --
# "ineffectual" [8 clone suicides] and "bad" [declare war on day 3] -- are mutually exclusive
# and not driven.)
#
# Deviations from the raw walkthrough (all verified against game.aslx, seed 1234):
#   * "Look X" -> "examine X" throughout: the game defines only "look at/x/examine/exam/ex
#     #object#", so bare "look document file" etc. would not parse.
#   * Dropped the walkthrough's duplicate "Examine file" (same object as "Look document file")
#     and its duplicate "Get journal" (one take suffices; "touch lock" auto-opens the chest,
#     so the journal is takeable immediately).
#   * "turn on game board" (the walkthrough's "Turn on game") is now driven. It used to be
#     dropped: the three board objects (game board/board1/board2) mutually SwitchOn each other
#     in their onswitchon scripts, and stock QuestViva re-fires changed<switchedon> even on a
#     same-value write, so it recursed to the 20-error breaker (state=Wedged). Real Quest fires
#     changed<attr> only on a real value change, which terminates the cycle; the oracle now does
#     too (patch_questviva.py section 7), and the native engine carries the same guard. Kept
#     before "press play" as in the walkthrough; mechanically the board's switched-on state
#     doesn't gate anything ("press play" only checks the "message 3 read" flag).
#   * "computer iastrai" in lowercase: the command LCase()s its argument, so the game's own
#     case ("Iastrai") branch is dead code; the lowercase branch sets "knowledge of Iastrai".
#   * The final password 28831010 answers the "Please provide password:" get-input prompt
#     raised by "computer self-destruct" (protest date from the photo's note + REMEMBER).
east
lie on examination table
w
examine document file
examine computer manual
s
open green drawer
get photograph
examine photograph
read photo
open blue drawer
get letter
read letter
open white drawer
get gun
open black drawer
read leaflet
n
w
open chest
touch lock
get journal
wear uniform
read journal
e
n
switch on comms screen
read message 1
read message 2
read message 3
examine game board
turn on game board
press play
read message 4
computer who am i?
computer clones
computer where am i?
computer mission
computer iastrai
remember
examine photograph
read note
computer self-destruct
28831010
