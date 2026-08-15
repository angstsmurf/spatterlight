' Each verb looks for its noun in one place, and only there.
'
' Quest hands Disambiguate a scope per call site, and the four commands below
' each name a single one: ExecTake searches the current room
' (V4Game.Part2.cs:5132), ExecDrop the inventory (V4Game.cs:5531), and ExecGive
' both -- the item in the inventory and the recipient in the room (ibid.
' 4673, 4723).  So a noun that names one thing in hand and another on the floor
' is never a question: TAKE reaches for the floor, DROP for the hand.
'
' geas resolved every noun against inventory *and* room, so each of those pairs
' became a disambiguation menu Quest does not show -- and a menu eats the next
' line of a walkthrough as its answer.  The Wizard is the corpus case: it hands
' out yellow beads one at a time and each TAKE YELLOW BEAD offered every bead
' already in the player's pocket alongside the one just found.
'
' The misses are worth as much as the hits.  A carried noun is a miss for TAKE,
' and from 4.10 -- and only from 4.10 -- ExecTake then re-runs the lookup over
' the inventory to answer AlreadyTaken rather than BadThing (ibid. 5143-5167);
' a room noun is a miss for DROP and for the item half of GIVE, which answer
' NoItem from 3.91 and BadDrop / NoItem below it.  verbscope350.asl is the same
' game one gate down, where the two version-dependent answers change.
define game <verbscope>
    asl-version <410>
    start <Kitchen>

    startscript {
        give <red apple>
        give <spare key>
        give <cook doll>
    }
end define

define room <Kitchen>
    look <A tidy kitchen.>

    ' Two apples and two keys, one of each in hand and one of each on the
    ' table, all four answering to the same short name.
    define object <red apple>
        alt <apple>
        prefix <a>
    end define

    define object <green apple>
        alt <apple>
        prefix <a>
        take <You take the green apple off the table.>
    end define

    define object <spare key>
        alt <key>
        prefix <a>
    end define

    define object <brass key>
        alt <key>
        prefix <a>
        take <You take the brass key off the table.>
    end define

    define object <cook>
        prefix <the>
        gender <he>
        article <him>
        give <spare key> msg <"That's mine," says the cook, pocketing it.>
    end define

    ' Carried, and named "cook" as far as the parser is concerned: the
    ' recipient half of GIVE is looked for in the room, so this one is never
    ' a candidate for it.
    define object <cook doll>
        alt <cook>
        prefix <a>
    end define
end define
