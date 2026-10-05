' A command whose pattern fits claims the input even when its #@object# names
' nothing (ExecUserCommand, V4Game.Part2.cs:2405-2413): one error, and neither
' the game block's command nor the built-in verb gets a second go at it.
define game <cmdclaim>
	asl-version <400>
	start <room>
	command <eat #@food#> msg <game eats #food#>
	command <poke #@thing#> msg <poked #thing#>
	error <badthing; No such thing.>
end define

define room <room>
	look <A room.>
	command <eat #@food#> msg <room eats #food#>
	define object <bun>
		look <A bun.>
	end define
end define
