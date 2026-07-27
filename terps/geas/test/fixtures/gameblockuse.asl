! "use <item>" with no target, below ASL 280, where the handler lives in the
! *game* block.
!
! Below 280 ExecUse is a different piece of code from the one bareuseroom.asl
! pins, and it is shorter than one would guess (V4Game.Part2.cs:5472-5497,
! braces elided because readfile.cc de-inlines brace blocks before it strips
! comments, so a lone brace in a comment would open a block):
!
!     for (int i = 1, loopTo = r.NumberUse; i <= loopTo; i++)
!         if (LCase(r.Use[i].Text) == LCase(useObject))
!             foundUseScript = true; useScript = r.Use[i].Script; break;
!     if (!foundUseScript)
!         FindLine(_gameBlock, "use", useObject, ref useScript, ref found)
!     if (!found) PlayerError(defaultuse) else ExecuteScript(useScript)
!
! Two things follow, and the second is the one geas got wrong.  The room's list
! is searched first, exactly as it is from 280 to 409.  But the fallback is a
! FindLine over the game block, and the object's own "use" action is not in the
! chain at all -- so below 280 an object that declares its own "use" and nothing
! else cannot be used, and the game's defaultuse error is what the player gets.
! geas had no game-block lookup and fell through to the object, which is the
! opposite of Quest on both halves.
!
! The lookup needs its own code rather than get_obj_action because readfile does
! not rewrite game-block lines into "action <...>" form -- the game block keeps
! its lines verbatim, which is also why the description fallback reads them
! directly.  Hence geas_implementation::find_game_block_line, Quest's FindLine:
! first line whose first token is "use" and whose first parameter matches the
! object name case-insensitively, with the rest of the line as the script, and
! nested define blocks skipped.
!
! Dom's "MagicSword Part 1: Legacy of Earth" (ASL 217) is unwinnable without it.
! It declares the use scripts for the Sword, the Staff, the Immune Potion and the
! healing Potion in the game block, and the Immune Potion is the witch's gift and
! the only thing that sets %Immune%; Hallucination Forest kills anyone who walks
! in without it, so the whole second half of the game hangs off one line of
! fallback.
!
! Below: the torch pins room-beats-game-block, the flask pins the game-block
! fallback itself (and that the match is case-insensitive), the lamp pins that an
! object's own handler is *not* consulted here, the candle pins that the game
! block wins over an object handler rather than merely tying with it, and the
! nested define block pins that FindLine skips sub-blocks -- the "use <lamp>"
! line buried in it must not be found.  gameblockuse280.asl is the same source at
! 280, where the object's handler wins and the game block is never read.
define game <gameblockuse>
    asl-version <217>
    start <r1>
    error <defaultuse;You might want to, but you can't.>

    use <torch> msg <The game-block handler for the torch ran.>
    use <Flask> msg <The game-block handler for the flask ran.>
    use <candle> msg <The game-block handler for the candle ran.>

    ! FindLine skips nested blocks, so this one is invisible.
    define variable <counter>
        type numeric
        value <0>
        use <lamp> msg <The nested handler ran -- it must not.>
    end define
end define

define room <r1>
    look <Room one.>
    west <r2>
    ! The room's list is still searched first below 280.
    use <torch> msg <The room handler for the torch ran.>

    define object <torch>
        look msg <A torch.>
        take
        use msg <The torch's own handler ran -- it must not.>
    end define

    ! Only the game block knows what to do with this one.
    define object <flask>
        look msg <A flask.>
        take
    end define

    ! Its own handler is not in the chain below 280, so this is the defaultuse
    ! error even though the object plainly declares a use.
    define object <lamp>
        look msg <A lamp.>
        take
        use msg <The lamp's own handler ran -- it must not.>
    end define

    ! Both declare a handler; the game block's is the one that runs.
    define object <candle>
        look msg <A candle.>
        take
        use msg <The candle's own handler ran -- it must not.>
    end define
end define

! No room-level list here, so the game block is reached for the torch too.
define room <r2>
    look <Room two.>
    east <r1>
end define
