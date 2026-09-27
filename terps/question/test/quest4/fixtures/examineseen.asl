' Only LOOK AT discovers a container.  EXAMINE does not, and neither does the
' `open` script command.
'
' Quest sets an object's "seen" property in exactly two places: DoLook
' (V4Game.cs:2141-2147) and, from ASL 4.10, DoAddRemove (ibid. 2125-2131).  From
' 3.91 that flag is half of what makes a container's contents reachable at all --
' UpdateVisibilityInContainers keeps an object available only while
' "surface or ((open or transparent) and seen)" holds of its parent
' (V4Game.Part2.cs:7666-7720) -- so a container the player has never looked *at*
' hides its contents however wide open it is.
'
' Neither of the two obvious ways to open one marks it seen:
'
'   - ExecExamine answers with the object's own `examine` action, property or
'     tag and returns on the spot, never reaching DoLook (V4Game.cs:5676-5716).
'     So X COFFIN describes the coffin without discovering it.
'   - The `open <X>` *script* statement calls DoOpenClose with showLook false
'     (V4Game.Part2.cs:574), so it sets "opened" and re-runs the visibility sweep
'     but never describes what it opened.  Only the OPEN *verb*'s property branch
'     passes true (V4Game.cs:2848), and this coffin takes the action branch,
'     which Quest leaves entirely to the script.
'
' Barbarian is the corpus case, and it is the reason this fixture exists: its
' Coffin4 is `container` with `open do <!intproc92>`, that procedure ends in
' `open <Coffin4>`, and the coffin carries an `examine` property.  Its silver
' coin and dusty scroll are both `parent <Coffin4>`, so the walkthrough has to
' LOOK AT the coffin before it can take either -- X COFFIN reads the inscription
' on the back and leaves them out of reach, exactly as below.
'
' The script below reaches for the coin with LOOK AT rather than TAKE on purpose.
' Both answer the same question -- can the parser see it? -- but ExecTake's
' implied removal is one of the places QuestViva's translation of Quest differs
' from the VB6 original (isInContainer is declared and never set,
' V4Game.Part2.cs:5176-5220), so a TAKE here would diff against the oracle for a
' reason that has nothing to do with the rule this fixture is about.
define game <examineseen>
    asl-version <410>
    start <Tomb>
end define

define room <Tomb>
    look <A small tomb.>

    define object <Coffin>
        look <A heavy stone coffin.>
        prefix <a stone>
        container
        open do <!openit>
        properties <examine = Written on the back of the coffin.>
    end define

    define object <Silver coin>
        look <A silver coin.>
        prefix <a>
        take
        parent <Coffin>
    end define
end define

define procedure <!openit>
    msg <You push the lid off.>
    open <Coffin>
end define
