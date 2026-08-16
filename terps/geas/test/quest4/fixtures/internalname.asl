' The player-facing half of loosename.asl: geas's internal-name leniency,
' a DELIBERATE deviation from Quest (finding 14 in the oracle's FINDINGS.md).
'
' Quest matches a typed noun against ObjectAlias and AltNames only -- an
' `alias' *replaces* the declared name in Disambiguate's eyes (V4Game.cs:
' 4704-4767) -- so an aliased object's `define' identifier is untypeable in a
' real Quest.  Early geas accepted it anyway, on purpose: a player who types
' `louvre key' at an object declared <Louvre key> has left no doubt what they
' mean, and refusing them helps nobody.  That leniency is the engine's default
' again, as a last-resort pass in get_obj_name that runs only after the exact
' and partial alias passes have both found nothing, and keeps its answer only
' when it names exactly ONE object in scope -- so it can turn a refusal into a
' success but can never ask a disambiguation question Quest would not have
' asked, and never pick a different object than Quest picked.
'
' The doubt rule is the pin here: `red gem1' and `red gem2' below are two
' QDK-numbered declarations whose aliases share no word with them.  LOOK AT
' RED GEM1 is unambiguous and works; LOOK AT RED GEM word-matches both
' declared names, so the leniency forfeits and the refusal stands, exactly as
' in Quest.  LOOK AT GEM1 shows the word-run half, which rides the same
' abbreviations gate as the alias's ("define options / abbreviations").
'
' Only when deriving Quest-compatible walkthroughs must all of this be off --
' a script that leans on a noun Quest refuses will not replay in a real
' Quest -- so geas_walkthrough_runner sets GEAS_STRICT_NAMES by default and
' this fixture's .args file carries --lenient-names to get the engine's own
' default back.  loosename.asl is the strict-mode pin: same shapes, refusals
' expected.
define game <internalname>
    asl-version <410>
    start <Vault>
end define

define room <Vault>
    look <A vault.>

    ' The Michael's Game case: an alias that shares no word with the name.
    ' Both the alias and the unique declared name resolve.
    define object <Louvre key>
        alias <Gold Key>
        look <It has a miniature Vitruvian man.>
    end define

    ' Two look-alike declarations, QDK-numbered.  Each full name is unique
    ' and resolves; the shared stem `red gem' matches both and is refused.
    define object <red gem1>
        alias <ruby>
        look <Deep red, pigeon-blood.>
    end define

    define object <red gem2>
        alias <garnet>
        look <Brownish red, rough-cut.>
    end define
end define
