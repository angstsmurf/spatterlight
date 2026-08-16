' quest.objects and quest.formatobjects: two lists out of one object walk, and
' they are punctuated differently -- finding 15 in the oracle's FINDINGS.md.
'
' quest.formatobjects is the text of the room's own sentence: the name in bold,
' the suffix after it, and " and " before the last item.  quest.objects is the
' plain list a game is meant to reuse -- prefix and name, joined with ", " and
' nothing else.  It carries no suffix and never carries an " and ", in any of
' the three routines that write it (ShowRoomInfo, V4Game.Part2.cs:3799-3822;
' ShowRoomInfoV2, ibid. 1884-1896; UpdateObjectList, ibid. 7521-7550).  geas
' used to build the two side by side off one set of separators, so a game that
' printed #quest.objects# itself got "un hueso, una piedra and una roca" --
' which is exactly what "Nearco" prints.
'
' The " and " belongs to the room display alone.  Quest rebuilds the pair from
' UpdateObjectList after every show, hide, create, move and take, and that
' routine joins with ", " throughout, so a script that changes what is visible
' and then reads #quest.formatobjects# gets an unpunctuated list back.  geas
' regenerates on read and so always punctuated; "Sleepover" shows the seam,
' its <bed2> description being two `show' lines and then the list.
'
' The two are also set before a description tag runs, not instead of it:
' ShowRoomInfo fills them a couple of hundred lines above the point where it
' looks for the tag (ibid. 3799-3830 against 3924-3956).  So <Study> below
' reads the punctuated list on its first line and the flat one after its
' `show', from inside one description.
'
' And the punctuated pair outlives the display by nothing at all: ShowRoomInfo's
' last act, once the default display has printed or the description tag has run,
' is a call to UpdateObjectList (ibid. 3974, and 2156 in the pre-2.80 path).  So
' the " and " is readable from inside a description tag and from nowhere else --
' which is why `probe' below reads the comma list on the turn after LOOK, with
' nothing having moved in between.
define game <objectslist>
    asl-version <410>
    start <Hall>
    command <probe> do <probe>
    command <reveal> do <reveal>
end define

define room <Hall>
    look <A hall.>

    ' A suffix, which the sentence gets and the plain list does not.
    define object <rock>
        prefix <a>
        suffix <of granite>
    end define

    define object <coin>
        prefix <a>
    end define

    ' An alias, which both lists take in place of the declared name.
    define object <lamp>
        alias <brass lamp>
        prefix <a>
    end define

    ' Hidden, so `reveal' has something to change.
    define object <mouse>
        prefix <a>
        hidden
    end define

    place <Study>
end define

define room <Study>
    description {
        msg <before objects: #quest.objects#>
        msg <before format: #quest.formatobjects#>
        show <ledger>
        msg <after objects: #quest.objects#>
        msg <after format: #quest.formatobjects#>
    }

    define object <shelf>
        prefix <a>
    end define

    define object <chair>
        prefix <a>
    end define

    define object <ledger>
        prefix <a>
        suffix <bound in calf>
        hidden
    end define
end define

define procedure <probe>
    msg <objects: #quest.objects#>
    msg <format: #quest.formatobjects#>
end define

define procedure <reveal>
    show <mouse>
    msg <objects: #quest.objects#>
    msg <format: #quest.formatobjects#>
end define
