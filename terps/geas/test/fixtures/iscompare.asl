' "is" is a field split, not a search for an operator.
'
' ExecuteIfIs (V4Game.cs:7450-7578) cuts the parameter at the first ';' and, if
' there is a second one, at that too: with three fields the middle field *is* the
' operator and everything past it is the right-hand side, with two fields the
' operator is "=".  Only "=", "!=", "gt", "lt", "gt=" and "lt=" are operators;
' anything else in that middle field is an error and the condition is false.
' "=" and "!=" compare the two sides as strings, case-insensitively; the four
' inequalities convert with Val(), which reads a leading number and calls
' everything else zero.
'
' geas instead searched the whole parameter for the operator as a substring, so
' an operand that ended in those letters just before the separator hijacked the
' comparison.  "bolt;bolt" contains "lt;", so
'
'     if is <#quest.remove.object.name#; bolt> then ...
'
' compared "bo" with "bolt" as numbers -- 0 < 0, false -- and a container in
' takefromcontainer that meant to refuse one of its two items refused neither.
' The words that trip it are ordinary ones: anything ending in "lt" or "gt", and
' anything at all before an explicit "!=" operator, since "!=;" was searched for
' the same way.  Nothing warned; the branch just went the other way.
'
' The names below are the trap cases: "bolt"/"salt" end in lt, "weight" in gt,
' and the two-field compares of them must still be plain string equality.  The
' three-field ones then check each operator, including the explicit "=" that the
' old code could only read as part of the right-hand side, and the "Val() calls
' it zero" rule for a non-numeric inequality.  ("if (a lt= b)" is written by
' preprocess as the same three-field form, so this covers that path too.)

define game <IsCompare>
 asl-version <410>
 start <Room>

 startscript {
  set string <s; bolt>
  set string <t; weight>
  set numeric <n; 7>
 }

 command <two> {
  if is <#s#; bolt> then msg <bolt = bolt: yes> else msg <bolt = bolt: no>
  if is <#s#; salt> then msg <bolt = salt: yes> else msg <bolt = salt: no>
  if is <#t#; weight> then msg <weight = weight: yes> else msg <weight = weight: no>
  if is <#t#; freight> then msg <weight = freight: yes> else msg <weight = freight: no>
  if is <#s#; BOLT> then msg <bolt = BOLT: yes> else msg <bolt = BOLT: no>
 }

 command <three> {
  if is <#s#; =; bolt> then msg <explicit =: yes> else msg <explicit =: no>
  if is <#s#; !=; salt> then msg <bolt != salt: yes> else msg <bolt != salt: no>
  if is <#s#; !=; bolt> then msg <bolt != bolt: yes> else msg <bolt != bolt: no>
  if is <%n%; gt; 3> then msg <7 gt 3: yes> else msg <7 gt 3: no>
  if is <%n%; lt; 3> then msg <7 lt 3: yes> else msg <7 lt 3: no>
  if is <%n%; gt=; 7> then msg <7 gt= 7: yes> else msg <7 gt= 7: no>
  if is <%n%; lt=; 7> then msg <7 lt= 7: yes> else msg <7 lt= 7: no>
  ' Val() of a word is zero, so this is 0 lt 3 -- true, and not an error the
  ' player ever sees (Quest only logs it).
  if is <#s#; lt; 3> then msg <bolt lt 3: yes> else msg <bolt lt 3: no>
  ' An unrecognised operator is false, whichever way round the operands are.
  if is <#s#; likes; bolt> then msg <bad operator: yes> else msg <bad operator: no>
 }

 command <paren> {
  ' preprocess turns this into the three-field form.
  if (%n% >= 7) then msg <paren gt=: yes> else msg <paren gt=: no>
  if (#s# = bolt) then msg <paren =: yes> else msg <paren =: no>
 }
end define

define room <Room>
 look <A plain room.>
end define
