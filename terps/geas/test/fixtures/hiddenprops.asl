' Quest hides an object at load time only if its definition carries a *bare*
' "hidden" line.  The loop that reads an object definition raises a local
' `hidden' flag on Strings.Trim (_lines[j]) == "hidden" alone
' (V4Game.Part2.cs:3238-3248); a "properties <hidden; ...>" line in the same
' loop just records the property (V4Game.Part2.cs:3378-3381), as does a type
' that carries it.  Then, when the definition ends (V4Game.Part2.cs:3550-3553):
'
'     o.DefinitionSectionEnd = j;
'     if (!hidden)
'     {
'         o.Exists = true;
'     }
'
' -- so every non-bare source of hidden-ness is undone on the spot and the
' object starts off visible.  Writing the property during play *does* hide the
' object (SetObjectProperty case "hidden", V4Game.cs:4169-4179); it is only the
' load-time write that gets reversed.
'
' geas rewrites a bare tag line into "properties <hidden>" while reading the
' file (readfile.cc), which is what made the two spellings indistinguishable
' downstream and hid both.  readfile now marks the bare form and GeasState
' reverses the rest, so:
'
'   bare      hidden                            -> hidden at start
'   viaprops  properties <hidden>               -> visible at start
'   viatype   type carrying properties <hidden> -> visible at start
'   both      bare tag *and* properties         -> hidden at start (bare wins)
'
' Slackers Inc.'s "Revenge of the Shadow Masters" is the corpus case: its
' Mug o' Grog is `properties <hidden; Drink; buy; steal>' with no bare tag and
' nothing ever calls `show' on it, so before this it could not be bought -- and
' the grog is the base of the Drink of Immortality that unlocks the second half
' of the demo.
'
' Known divergence, pinned by the "props" lines below: geas reverses a load-time
' hidden by recording "not hidden", so `if property <X; hidden>' reads false at
' the start for viaprops/viatype where Quest, which keeps the property and only
' clears Exists, would read true.  geas has one "hidden" property doing the work
' of Quest's separate Exists flag, so the two cannot both be exact; initial
' visibility is the one games depend on.  After a runtime reveal/conceal the
' newest write wins and the two agree again, which the rest of the transcript
' shows.

define game <HiddenProps>
 asl-version <410>
 start <bar>

 command <props> {
  if property <bare; hidden> then msg <bare: hidden> else msg <bare: not hidden>
  if property <viaprops; hidden> then msg <viaprops: hidden> else msg <viaprops: not hidden>
  if property <viatype; hidden> then msg <viatype: hidden> else msg <viatype: not hidden>
  if property <both; hidden> then msg <both: hidden> else msg <both: not hidden>
 }
 ' show/hide are the Exists axis, the same one the "hidden" property drives;
 ' reveal/conceal are Quest's separate Visible flag and would not do here.
 command <letout> {
  show <bare>
  show <both>
 }
 command <shutin> hide <viaprops>
end define

define type <lurker>
 properties <hidden>
end define

define room <bar>
 look <The bar.>

 define object <bare>
  look msg <A bare-tag object.>
  hidden
 end define

 define object <viaprops>
  look msg <A properties-line object.>
  properties <hidden>
 end define

 define object <viatype>
  look msg <A type-hidden object.>
  type <lurker>
 end define

 define object <both>
  look msg <A both-ways object.>
  hidden
  properties <hidden>
 end define
end define
