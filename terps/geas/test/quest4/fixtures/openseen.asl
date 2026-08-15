' What "open" does to a container, and what it does not.
'
' ExecOpenClose (V4Game.cs:2702-2847) is a fixed ladder of tests, and every rung
' above the last one leaves the object exactly as it was:
'
'     not a container      -> "You can't open that."
'     already open/closed  -> "It is already open." / "It is already closed."
'     out of reach         -> "You can't open that." plus the reason
'     action -open-        -> run the script, and change nothing else
'     no -open- property   -> "You can't open that."
'     otherwise            -> its text, then DoOpenClose
'
' Only that last rung sets "opened", and only it discovers the container: it
' calls DoLook, which is one of the engine's two places that write the "seen"
' property (the other being the "add" half of DoAddRemove, from 4.10).
'
' Both halves matter, because the contents of a container are in scope only when
' it is open (or transparent, or a surface) AND seen -- the rule
' UpdateVisibilityInContainers applies at V4Game.Part2.cs:7710.  geas used to
' mark a container seen at the top of the dispatch, before it knew whether the
' object could be opened at all, so a *refused* open was enough to reveal what
' was inside.  The Blight of Elantria is the corpus case: its temple recess is
' revealed already `opened`, the walkthrough opens it rather than looking at it,
' and geas handed over the bronze key and the ruby Quest keeps behind its
' `badthing` text.  The two runs never came back into step.
'
' The recess here is that game in miniature.  `probe` reports the two flags the
' rule reads.  A scripted container (the crate) is the action rung: its script
' runs, and the crate is neither opened nor seen afterwards, so its contents stay
' out of reach until the player looks at it.  The box is the last rung and the
' control -- it opens, is discovered, and gives up its contents.
define game <openseen>
    asl-version <410>
    start <Vault>

    command <reveal> show <recess>
    command <probe> {
        if property <recess; seen> then msg <RECESS SEEN> else msg <RECESS UNSEEN>
        if property <recess; opened> then msg <RECESS OPEN> else msg <RECESS SHUT>
        if property <crate; seen> then msg <CRATE SEEN> else msg <CRATE UNSEEN>
        if property <crate; opened> then msg <CRATE OPEN> else msg <CRATE SHUT>
    }
end define

define room <Vault>
    look <A stone vault.>

    ' Revealed already open, and so still undiscovered: opening it again is
    ' refused, and the refusal must not discover it.
    define object <recess>
        look <A niche in the wall.>
        hidden
        container
        opened
        remove
    end define

    define object <bronze key>
        look <A bronze key.>
        parent <recess>
        take
    end define

    ' A plain object: not a container at all.
    define object <statue>
        look <A stone statue.>
    end define

    ' The action rung: the game's own script runs and nothing else happens.
    define object <crate>
        look <A packing crate.>
        container
        remove
        open msg <The lid creaks but does not shift.>
    end define

    define object <nail>
        look <A rusty nail.>
        parent <crate>
        take
    end define

    ' The property rung: this one really opens.
    define object <box>
        look <A cardboard box.>
        container
        remove
        open
        close
    end define

    define object <coin>
        look <A silver coin.>
        parent <box>
        take
    end define
end define
