/***************************************************************************
 *  Bundled built-in copy of "Additional verbs" (stdverbs.lib), the standard
 *  Quest 4 verb library by Alex Warren, v1.0, Copyright 2007 Axe Software.
 *
 *  The .lib shipped with Quest itself, not with games, so a game that
 *  `!include`s it cannot find the file on disk under Geas.  readfile.cc
 *  splices this text in at that point instead (see handle_includes /
 *  is_stdverbs), exactly as it does for the type library.
 *
 *  Geas used to skip the include on the theory that it only supplied verbs
 *  Geas already had.  It does not: every line here is content the engine has
 *  no native equivalent for -- twenty-nine default verb responses, seven
 *  extra commands, and the multiple-commands-on-one-line splitter, which is
 *  the library's headline feature.  Without them a game that includes the
 *  library answers "buy the sword" with its own unrecognised-verb error where
 *  Quest answers "You can't buy the sword.", and drops the second half of
 *  "north, then east" on the floor.
 *
 *  Verbatim from the released source, which asks not to be modified; only the
 *  encoding differs (the original is ISO-8859-1, this is UTF-8).
 *
 *  Source: textadventures/quest, src/Legacy/Libraries/stdverbs.lib
 ***************************************************************************/

#ifndef GEAS_STDVERBS_BUILTIN_HH
#define GEAS_STDVERBS_BUILTIN_HH

static const char geas_builtin_stdverbs[] = R"GEASLIB(
!library
!asl-version <400>
!name <Additional verbs>
!version <1.0>
!author <Alex Warren>
! This library adds default responses for a number of common commands that players might use in a game. It also allows players to type multiple commands on the same command line.

' STDVERBS.LIB v1.0
' for Quest 4.0
' Copyright © 2007 Axe Software. Please do not modify this library.

!addto game
	verb <buy> msg <You can't buy #(quest.lastobject):article#.>
	verb <climb> msg <You can't climb #(quest.lastobject):article#.>
	verb <drink> msg <You can't drink #(quest.lastobject):article#.>
	verb <eat> msg <You can't eat #(quest.lastobject):article#.>
	verb <hit> msg <You can't hit #(quest.lastobject):article#.>
	verb <kill> msg <You can't kill #(quest.lastobject):article#.>
	verb <kiss> msg <I don't think #(quest.lastobject):article# would like that.>
	verb <knock> msg <You can't knock #(quest.lastobject):article#.>
	verb <lick> msg <You can't lick #(quest.lastobject):article#.>
	verb <listen to> msg <You listen, but #(quest.lastobject):article# makes no sound.>
	verb <lock> msg <You can't lock #(quest.lastobject):article#.>
	verb <move> msg <You can't move #(quest.lastobject):article#.>
	verb <pull> msg <You can't pull #(quest.lastobject):article#.>
	verb <push> msg <You can't push #(quest.lastobject):article#.>
	verb <read> msg <You can't read #(quest.lastobject):article#.>
	verb <search> msg <You can't search #(quest.lastobject):article#.>
	verb <show> msg <You can't show #(quest.lastobject):article#.>
	verb <sit on> msg <You can't sit on #(quest.lastobject):article#.>
	verb <smell; sniff> msg <You sniff, but #(quest.lastobject):article# doesn't smell of much.>
	verb <taste> msg <You can't taste #(quest.lastobject):article#.>
	verb <throw> msg <You can't throw #(quest.lastobject):article#.>
	verb <tie> msg <You can't tie #(quest.lastobject):article#.>
	verb <touch> msg <You can't touch #(quest.lastobject):article#.>
	verb <turn on> msg <You can't turn #(quest.lastobject):article# on.>
	verb <turn off> msg <You can't turn #(quest.lastobject):article# off.>
	verb <turn> msg <You can't turn #(quest.lastobject):article#.>
	verb <unlock> msg <You can't unlock #(quest.lastobject):article#.>
	verb <untie> msg <You can't untie #(quest.lastobject):article#.>
	verb <wear> msg <You can't wear #(quest.lastobject):article#.>

	command <ask #stdverbs.command#> msg <You get no reply.>
	command <listen> msg <You can't hear much.>
	command <jump> msg <You jump, but nothing happens.>
	command <tell #stdverbs.command#> msg <You get no reply.>
	command <sit down; sleep> msg <No time for lounging about now.>
	command <wait> msg <Time passes.>
	command <xyzzy> msg <Surprisingly, absolutely nothing happens.>

	command <#stdverbs.command#. #stdverbs.command2#; _
		#stdverbs.command#, then #stdverbs.command2#; _
		#stdverbs.command#, and then #stdverbs.command2#; _
		#stdverbs.command#, #stdverbs.command2#; _
		#stdverbs.command# and then #stdverbs.command2#; _
		#stdverbs.command# then #stdverbs.command2#> {
		exec <#stdverbs.command#>
		exec <#stdverbs.command2#>
	}

	command <#stdverbs.command#.> exec <#stdverbs.command#>
!end
)GEASLIB";

#endif
