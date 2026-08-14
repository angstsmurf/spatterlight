' Runtime object creation and typing, both formerly silent no-ops.
'
' "create object <name[; room]>" appends a live object: named by the first
' field, placed in the second (or nowhere), alias/gender/article defaulted,
' and -- from ASL 4.10 -- the `default` type's properties flattened on, as a
' declared object gets them (ExecuteCreate, V4Game.cs:5278-5325).
'
' "type <obj; typename>" flattens the named type's properties and actions
' onto the object at runtime and records the membership, which `if type`
' consults (ExecType, V4Game.cs:4425-4476).
define game <CreateObject>
 asl-version <410>
 start <Start>
 command <probe> msg <$objectproperty(widget;glitter)$ w=$objectproperty(widget;weight)$>
 command <check> if type <widget; shiny> then msg <SHINY YES> else msg <SHINY NO>
 startscript {
  create object <widget; Start>
  type <widget; shiny>
 }
end define

define room <Start>
end define

define type <shiny>
 glitter = It sparkles.
 action <rub> msg <It gleams.>
end define

define type <default>
 properties <weight=1>
end define
