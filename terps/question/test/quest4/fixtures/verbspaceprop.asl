' A verb named with a trailing space still finds its property -- on purpose.
'
' A DELIBERATE deviation from Quest.  ExecVerb takes a lone verb name as it
' stands (V4Game.cs:2968-2972, 2985), so `verb <waste the >' is reached only
' by typing two spaces, and then looks for a property called `waste the '
' while the object's own property name was trimmed when it was read.  Quest
' answers WASTE THE  THUG below with "You can't waste the  that." (checked in
' qv4); Question prints the property.  Every other line is the same in both.
' Bob's Adventure's gangster is the witness.
define game <verbspaceprop>
	asl-version <400>
	start <room>
	verb <waste the > msg <You can't waste the  that.>
	verb <waste> msg <You can't waste that.>
	verb <zap ; fry > msg <You can't zap that.>
	verb <hug : cuddle > msg <You can't hug that.>
end define

define room <room>
	look <A room.>
	define object <thug>
		properties <waste the =He falls.; zap =Zapped.; cuddle =Cuddled.>
	end define
end define
