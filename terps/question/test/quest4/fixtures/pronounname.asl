' An object called by a pronoun wins over the pronoun -- on purpose.
'
' A DELIBERATE deviation from Quest.  Disambiguate tests the noun against "it",
' "them", "him" and "her" before it looks at any object (V4Game.cs:4655-4697),
' so in Quest an object named `him' can never be referred to: CASH HIM and
' LOOK AT HIM below both answer "I don't know what 'him' you are referring
' to.", before and after the rock is looked at (checked in qv4).  Bob's
' Adventure names its beggar `him' and its lady `Her', which is the witness.
' Question takes an exact, unique match on the name the player sees as the
' object.  CASH IT is an ordinary pronoun in both engines; it differs here only
' because the object before it was reachable.
define game <pronounname>
	asl-version <400>
	start <room>
	verb <cash> msg <You can't cash that.>
end define
define room <room>
	look <A room.>
	define object <him>
		properties <cash=You give him 50 cents.>
		look <A beggar.>
	end define
	define object <rock>
		look <A rock.>
	end define
end define
