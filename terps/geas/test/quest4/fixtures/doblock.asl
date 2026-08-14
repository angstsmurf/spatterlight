! The "do" keyword followed by an inline brace block instead of a procedure name.
!
! Both engines de-inline a multi-line brace block by hoisting it into an internal
! procedure and appending a call to whatever came before the opening brace:
!
!     _lines[j] = startOfOrig + " do " + procName + " " + afterLastBrace;
!                                                   ' V4Game.cs:668
!
! and identically in readfile.cc's pass 3.  When the source already ended in
! "do", the rewritten line therefore reads "... else do do <!intproc3>" -- two
! "do"s, the author's and the loader's.  Quest doesn't care, because it reads the
! procedure name with GetParameter, which scans the line for the first parameter
! in angle brackets (V4Game.Part2.cs:5754-5756).  geas insisted the parameter be
! the token straight after "do", so every such block was reported as "Expected
! parameter after do" and silently skipped.
!
! "The Devil's Bargain" writes all of its nested conditionals in that style, so
! its <Answers> procedure printed the opening insult and then nothing: the reply
! naming your favourite colour, and the re-ask that rejects a colour outside the
! author's list, both lived in else-branch blocks that never ran.
!
! (Comments here avoid an unpaired opening brace of their own: readfile.cc
! de-inlines before it strips comments, so a lone brace in a comment is read as
! the start of a block.  Quest's loader has the same ordering.)
define game <doblock>
    asl-version <410>
    start <r>
    startscript do <check>
end define

define procedure <check>
    setstring <colour;grey>
    ! a block with no "if" involved
    do {
        msg <bare block ran>
    }
    ! a then-branch block with a following else
    if is <#colour#;grey> then do {
        msg <then-block ran>
        msg <still in the then-block>
    } else msg <NOT REACHED: else>
    ! else-branch blocks nested two deep, the shape <Answers> uses
    if is <#colour#;black> then msg <NOT REACHED: black> else do {
        if is <#colour#;grey> then msg <grey matched at depth 2> else do {
            msg <NOT REACHED: fallback>
        }
        msg <back at depth 1>
    }
    ! "do" with a real procedure name still works, arguments included
    do <named>
    do <withargs(seven)>
end define

define procedure <named>
    msg <named procedure ran>
end define

define procedure <withargs>
    msg <named procedure got $parameter(1)$>
end define

define room <r>
    look <A room.>
end define
