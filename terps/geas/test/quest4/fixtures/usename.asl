! A catch-all use script is told which object filled the blank through a string
! variable, and the name Quest sets is "quest.use.object.name".  ExecUse assigns
! it in both directions: for the target's `use anything` it holds the held item's
! name, and for the held item's `use on anything` it holds the target's
! (V4Game.Part2.cs:5439-5460).
!
! "ChristmaKwanzakkah" is the game that cares -- every crowd and every boss in it
! is a `use anything` whose script compares `#(quest.use.object.name):weapon#`
! against a threshold, so under the wrong variable name every weapon reads as
! having no power and nothing in the game can be attacked at all.
define game <usename>
    asl-version <350>
    start <Range>
end define

define room <Range>
    define object <rifle>
        take
        properties <weapon=2>
    end define

    define object <wand>
        take
        properties <weapon=5>
        use on anything msg <waved at #quest.use.object.name#, power #(quest.use.object.name):weapon#>
    end define

    define object <target>
        use anything msg <hit by #quest.use.object.name#, power #(quest.use.object.name):weapon#>
    end define

    define object <rock>
        properties <weapon=3>
    end define
end define
