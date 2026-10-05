' A leading "the " is dropped from a typed noun, and an exact name may carry
' one the player left out (Disambiguate, V4Game.cs:4699-4720).
define game <thenoun>
	asl-version <400>
	start <room>
	verb <poke> msg <You can't poke that.>
	command <prod #@thing#> msg <prodded #thing#>
end define

define room <room>
	look <A room.>
	define object <rat>
		look <A rat.>
		take
	end define
	define object <bridge>
		alias <The Bridge>
		look <A bridge.>
	end define
	define object <red rose>
		look <A rose.>
	end define
	define object <theatre>
		look <A theatre.>
	end define
end define
