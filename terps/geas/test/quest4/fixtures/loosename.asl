' Naming an object by parts of its declaration the player is not supposed to
' know: finding 14 in the oracle's FINDINGS.md.
'
' Quest builds one display name per object and matches against that alone.
' `alias' replaces the declared name outright (V4Game.Part2.cs, SetUpObjectName),
' so once an object has one, its declared name is an internal identifier and
' nothing more: TAKE SILVER BRACELET, for an object declared <Silver Bracelet>
' with `alias <Silver>' and `suffix <bracelet>', is answered "I can't see that
' here." in a real Quest 4.1.5.  Only TAKE SILVER works.  The prefix and the
' suffix are decoration on the room listing; neither is a name.
'
' geas matches the declared name as well, and matches the alias, and lets a
' prefix or suffix ride along, so every one of the four lines below is accepted.
' That is a superset of Quest's matching, which is why it never loses a game a
' player could otherwise finish -- and why it went unnoticed for so long.  It
' does lose the *refusal*: a game that relies on an object being unnameable
' until it is renamed behaves differently here.
'
' The corpus used to catch this in nine walkthroughs, each of which named an
' object the way its author had written the `define' header rather than the way
' a player would have to.  Those scripts have since been reworded to phrasings
' both engines accept -- a desync at turn 30 hides everything after it, and a
' whole transcript spent proving one known divergence is a poor trade for one
' that proves the other several hundred turns.  This fixture is what replaced
' them: it holds the divergence still, and the day the matcher is tightened the
' four "I can't see that here." lines it should print will appear here.
define game <loosename>
    asl-version <410>
    start <Vault>
end define

define room <Vault>
    look <A vault.>

    ' The Treasure Hunt case: alias plus suffix.
    define object <Silver Bracelet>
        alias <Silver>
        look <Round and smooth.>
        take
        prefix <a beautiful>
        suffix <bracelet>
    end define

    ' The Michael's Game case: an alias that shares no word with the name.
    define object <Louvre key>
        alias <Gold Key>
        look <It has a miniature Vitruvian man.>
        take
    end define

    ' The Shipwrecked case: a declared name that is the alias plus a digit,
    ' which is how a QDK author numbers two objects that look alike.
    define object <Ceramic tile41>
        alias <Ceramic tile4>
        look <A crude design, notched in the back.>
        take
    end define
end define
