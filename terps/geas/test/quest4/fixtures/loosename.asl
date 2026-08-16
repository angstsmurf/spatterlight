' Naming an object by parts of its declaration the player is not supposed to
' know: finding 14 in the oracle's FINDINGS.md, fixed 2026-08-16.
'
' Quest matches a typed noun against ObjectAlias and AltNames, never against
' the declared name once an alias has replaced it: ObjectAlias is *initialised*
' to the name at creation (V4Game.Part2.cs:3237-3238) and *overwritten* by
' `alias', so an aliased object's declared name is an internal identifier and
' nothing more.  LOOK AT LOUVRE KEY, for an object declared <Louvre key> with
' `alias <Gold Key>', is "I can't see that here.", and so is LOOK AT CERAMIC
' TILE41 for a declared name that is the alias plus a QDK author's numbering
' digit.  geas used to accept both -- its partial matcher ran over the declared
' name as well as the alias -- and this fixture used to freeze that as a known
' divergence.  match_object now matches the alias alone, and the two refusals
' below are the fix's pin.
'
' The Silver Bracelet lines are the other half of the story, and they are
' ACCEPTED -- by both engines.  At ASL >= 410 Quest's abbreviation pass
' (V4Game.cs:4738-4760) matches the typed noun as a word-initial substring of
' prefix + alias + suffix, so SILVER and SILVER BRACELET both reach an object
' aliased "Silver" with `suffix <bracelet>'.  An earlier revision of this
' header claimed a real Quest refuses TAKE SILVER BRACELET; it does not -- the
' suffix rides along at 410 -- and the whole fixture was verified byte for
' byte against QuestViva (qv4, QVH_SEED=1) on 2026-08-16.
'
' The corpus used to hit the loose matcher in nine walkthroughs, each naming
' an object the way its author wrote the `define' header (use oar on boat2,
' speak to trader, take heatville chip...).  Those scripts now use the alias
' phrasings a real player would have to find, and the engines agree on every
' one of them.
'
' STRICT MODE ONLY.  What this fixture pins is the walkthrough-derivation
' matcher: geas_walkthrough_runner sets GEAS_STRICT_NAMES by default, and
' these refusals are what a derived script must be written against, because
' they are what a real Quest answers.  The *engine's* default is lenient
' again, a DELIBERATE deviation (lenient_names_ in geas-impl.hh): a player
' typing the declared name of exactly one object in scope is understood.
' internalname.asl pins that side, running with --lenient-names via its
' .args file.
define game <loosename>
    asl-version <410>
    start <Vault>
end define

define room <Vault>
    look <A vault.>

    ' The Treasure Hunt case: alias plus suffix.  Both LOOKs work, in Quest
    ' too: the 410 abbreviation pass includes the prefix and the suffix.
    define object <Silver Bracelet>
        alias <Silver>
        look <Round and smooth.>
        take
        prefix <a beautiful>
        suffix <bracelet>
    end define

    ' The Michael's Game case: an alias that shares no word with the name.
    ' The declared name must be refused.
    define object <Louvre key>
        alias <Gold Key>
        look <It has a miniature Vitruvian man.>
        take
    end define

    ' The Shipwrecked case: a declared name that is the alias plus a digit,
    ' which is how a QDK author numbers two objects that look alike.  The
    ' digit form must be refused -- "tile41" is not a word-initial substring
    ' of "Ceramic tile4", it is the alias overrun.
    define object <Ceramic tile41>
        alias <Ceramic tile4>
        look <A crude design, notched in the back.>
        take
    end define
end define
