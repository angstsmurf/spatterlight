' Load-time keyword case, the readfile.cc/geasfile.cc half of the audit that
' keywordcase.asl covers for the runner.  Quest finds block headers and nearly
' every block keyword with the lowercasing BeginsWith ("define room ",
' "define type ", "north ", "alias ", "look ", "properties ") -- so a
' mixed-case header or keyword works there, and now works here: readfile folds
' the block type and any keyword followed by more text, and geasfile's
' cache/key readers compare CI.
'
' The deliberate case-SENSITIVE spot is pinned by the ghost: a *bare-word*
' flag line is compared raw in Quest (Trim(line) == "hidden",
' V4Game.Part2.cs:3242), so its capitalised "Hidden" must NOT hide it, while
' the gem's lowercase one must.
define game <BlockCase>
 asl-version <410>
 start <Study>
end define

define Room <Study>
 North <Hall>
 define object <key>
  Alias <brass key>
  Look msg <A small brass key.>
  Type <shiny>
 end define
 define object <ghost>
  Hidden
 end define
 define object <gem>
  hidden
 end define
end define

define room <Hall>
 look <A bare hall.>
end define

define Type <shiny>
 Action <rub> msg <It gleams.>
end define
