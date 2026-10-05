-- Captures the ZX Spectrum goldens of a scene test (see scenetest.c) from the
-- original game running under MAME. It plays the same .scene script the
-- interpreter is tested with — typing each command on the emulated keyboard,
-- waiting for the screen to settle — and at every `!check <name>.scr` line saves
-- the display file ($4000-$5AFF, 6912 bytes) next to the scene as <name>.scr.
--
--   SCENE=groundtruth_scene/hulk_zx/hulk.scene mame spectrum \
--       -snapshot groundtruth_scene/hulk_zx/hulk.z80 \
--       -autoboot_script zx_capture.lua -nothrottle -video none -sound none
--
-- A tape image goes in with -cassette instead of -snapshot. LOAD "" is then
-- typed and the tape started before the scene's own steps (so that a scene can
-- catch the loading screen, with !hwwait and !dump).
--
-- Scene lines as seen from here:
--   plain line        typed, followed by ENTER (an empty line is just ENTER)
--   !hw <keys>        the same, for input only the original needs (its "load
--                     saved game?" prompt, ...)
--   !terp <line>      ignored (input only the interpreter needs)
--   !check <name>     dump the screen to <name> once it has settled
--   !hwwait <frames>  just let the game run
--   !hwpoke <addr>=<bytes>[*<n>]
--                     write hex bytes to memory, in memory order, n times in a
--                     row: `!hwpoke a7ec=23af*37` fills a table of 37 words.
--                     For pictures no script can walk to — point the game's
--                     picture table, or its room number, somewhere else
--   !hwsave <name> <addr> <length>
--                     save that many bytes of memory, from the hex address,
--                     as <name>: the game state behind a dumped screen
--   !hwnext           let it run until the picture has changed and come to
--                     rest again: the next frame of a slow animation
--   !dump <name>      dump the screen right now, settled or not; a later
--                     !check of the same name then compares against this dump
--                     instead of capturing again (animated pictures, which
--                     never settle: !hwnext, !dump, !tick, !check)
-- Everything else (!game, !tick, !slowdraw, comments) is skipped.
--
-- The original stops with <HIT ENTER> whenever its text window fills up, which
-- depends on how long the room description happens to be. Those pauses are
-- answered here, unless the scene's next input is an empty line anyway.

local scene = os.getenv("SCENE")
local dir = scene:match("^(.*/)") or ""
local steps, dumped = {}, {}
for line in io.lines(scene) do
    line = line:gsub("\r$", "")
    local check = line:match("^!check%s+(%S+)")
    local dump = line:match("^!dump%s+(%S+)")
    local wait = line:match("^!hwwait%s+(%d+)")
    local addr, bytes, times = line:match("^!hwpoke%s+(%x+)=(%x+)%*?(%d*)")
    local save, from, length = line:match("^!hwsave%s+(%S+)%s+(%x+)%s+(%d+)")
    if check then
        if check:match("%.scr$") and not dumped[check] then
            steps[#steps + 1] = { dump = check, settle = true }
        end
    elseif dump then
        dumped[dump] = true
        steps[#steps + 1] = { dump = dump }
    elseif wait then
        steps[#steps + 1] = { wait = tonumber(wait) }
    elseif addr then
        bytes = bytes:gsub("%x%x", function(byte) return string.char(tonumber(byte, 16)) end)
        steps[#steps + 1] = { poke = tonumber(addr, 16), bytes = bytes:rep(tonumber(times) or 1) }
    elseif save then
        steps[#steps + 1] = { save = save, from = tonumber(from, 16), length = tonumber(length) }
    elseif line == "!hwnext" then
        steps[#steps + 1] = { next = true }
    elseif line == "!hw" or line:match("^!hw ") then
        steps[#steps + 1] = { type = line:sub(5) }
    elseif line:sub(1, 1) ~= "!" and line:sub(1, 1) ~= "#" then
        steps[#steps + 1] = { type = line }
    end
end

local KEY_GAP = 8          -- frames between keys, so repeated letters register
local SETTLE_FULL = 100    -- frames the whole screen must stay unchanged, or
local SETTLE_PICTURE = 250 -- frames the picture area must (a cursor may blink)
local SETTLE_LIMIT = 5000
local FRAME_STILL = 8      -- frames an animation frame stays once fully drawn

local mem, nk, tape
local step, pos, timer = 1, 0, 0
local settling, last_full, last_pic, same_full, same_pic, waited

local function screen()
    return mem:read_range(0x4000, 0x5aff, 8)
end

-- The top 12 character rows, where the in-game pictures are drawn.
local function picture_area(scr)
    local t = { scr:sub(1, 0x800), scr:sub(0x1801, 0x1980) }
    for line = 0, 7 do
        t[#t + 1] = scr:sub(0x801 + line * 256, 0x880 + line * 256)
    end
    return table.concat(t)
end

-- "<HIT ENTER>" at the left of the bottom text row: the first nine bytes of
-- each of its eight pixel lines.
local HIT_ENTER =
    "\x12\x2f\xbe\x03\xe8\xbe\xfb\xc4\x00\x22\x22\x08\x02\x08\x88\x82\x22\x00" ..
    "\x42\x22\x08\x02\x0c\x88\x82\x21\x00\x83\xe2\x08\x03\xca\x88\xf3\xc0\x80" ..
    "\x42\x22\x08\x02\x09\x88\x82\x41\x00\x22\x22\x08\x02\x08\x88\x82\x22\x00" ..
    "\x12\x2f\x88\x03\xe8\x88\xfa\x24\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"

local function paused()
    local scr = screen()
    for line = 0, 7 do
        local at = 0x10e1 + line * 256
        if scr:sub(at, at + 8) ~= HIT_ENTER:sub(line * 9 + 1, line * 9 + 9) then
            return false
        end
    end
    return true
end

-- True if the next thing the scene types is an empty line.
local function enter_comes_next()
    for i = step, #steps do
        if steps[i].type then return steps[i].type == "" end
    end
    return false
end

local function start_settling()
    settling, last_full, last_pic, same_full, same_pic, waited = true, nil, nil, 0, 0, 0
end

-- True once the screen has stopped changing.
local function settled()
    local full = screen()
    local pic = picture_area(full)
    same_full = (full == last_full) and same_full + 1 or 0
    same_pic = (pic == last_pic) and same_pic + 1 or 0
    last_full, last_pic = full, pic
    waited = waited + 1
    if waited == SETTLE_LIMIT then
        print("zx_capture: the screen never settled at step " .. step)
    end
    return same_full >= SETTLE_FULL or same_pic >= SETTLE_PICTURE or waited >= SETTLE_LIMIT
end

local function frame()
    if not mem then
        mem = manager.machine.devices[":maincpu"].spaces["program"]
        nk = manager.machine.natkeyboard
        tape = manager.machine.cassettes[":cassette"]
        if tape and tape.exists then
            tape:stop()
            table.insert(steps, 1, { type = 'j""' }) -- LOAD ""
            table.insert(steps, 2, { play = true })
        end
        start_settling()
    end
    if settling then
        if not settled() then return end
        settling = false
        if paused() and not enter_comes_next() then
            nk:post("\r")
            start_settling()
            return
        end
    end
    local s = steps[step]
    if not s then
        manager.machine:exit()
        return
    end
    if s.type then
        if nk.is_posting then return end
        timer = timer + 1
        if timer < KEY_GAP then return end
        timer = 0
        pos = pos + 1
        if pos <= #s.type then
            nk:post(s.type:sub(pos, pos):lower())
            return
        end
        nk:post("\r")
        start_settling()
    elseif s.play then
        tape:play()
    elseif s.wait then
        timer = timer + 1
        if timer < s.wait then return end
        timer = 0
    elseif s.poke then
        for i = 1, #s.bytes do
            mem:write_u8(s.poke + i - 1, s.bytes:byte(i))
        end
    elseif s.save then
        local f = assert(io.open(dir .. s.save, "wb"))
        f:write(mem:read_range(s.from, s.from + s.length - 1, 8))
        f:close()
    elseif s.next then
        local pic = picture_area(screen())
        s.from = s.from or pic
        same_pic = (pic == last_pic) and same_pic + 1 or 0
        last_pic = pic
        if pic == s.from or same_pic < FRAME_STILL then return end
    elseif s.dump then
        if s.settle and not s.settled then
            s.settled = true
            start_settling()
            return
        end
        local f = assert(io.open(dir .. s.dump, "wb"))
        f:write(screen())
        f:close()
        print("zx_capture: wrote " .. dir .. s.dump)
    end
    step, pos = step + 1, 0
end

zx_capture_subscription = emu.add_machine_frame_notifier(frame)
