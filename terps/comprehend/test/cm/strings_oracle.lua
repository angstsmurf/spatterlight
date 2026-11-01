-- strings_oracle.lua -- make The Coveted Mirror's own interpreter, running in
-- MAME, decode every string of both tables and write them out as text, in the
-- format of COMPREHEND_STRDUMP. strings_oracle.sh diffs our decoder against it.
--
-- Setup: MAME `apple2e` with the game booted (side B, S for standard hi-res,
-- side A swapped in at "turn thy diskette over" and left in the drive) and
-- sitting at a command prompt -- the throne-room savestate `cmstart` is what was
-- used. Run this file through the mame-mcp Lua bridge or MAME's Lua console with
-- the global OUT set to the output path. It takes about ten seconds. It patches
-- the game in RAM, so reload the savestate afterwards.
--
-- How it works (addresses from the Ghidra project of the game's RAM):
--   $5698  print a string ref: $5a38 = index low, $5a39 = table byte (AND #$7f).
--          Table < 2 is the main table (S1) at ($5a36), the entry found by
--          walking null-terminated entries; otherwise bank (table-2)*4 + idx/64
--          (files MA..ML) is loaded from disk to $1700, the entry taken from the
--          64-slot offset table at the start of the bank. Ends with a CR.
--   $57cd  the 5-bit decoder: code $1e = capital shift, $1f = special shift, and
--          a capital shift on the space code (table slot $36) calls $56e5 to print
--          the current item's name -- patched to emit '@', our placeholder for it.
--   $58d1  per-character output (word buffer, wrap at 39 columns) -- patched to
--          append the raw character to a buffer at $2000 (hi-res page 1).
-- A dispatcher in free RAM at $1f00 polls a GO flag, calls $5698 for the queued
-- ref, appends $ff and raises DONE; the CPU's PC is pointed at it once. The
-- trailing CR of each string is stripped and embedded CRs are written as \n.

OUT = OUT or "/tmp/original_strings.txt"

local m = manager.machine
local cpu = m.devices[":maincpu"]
local sp = cpu.spaces["program"]

local function poke(addr, bytes)
  for i, b in ipairs(bytes) do sp:write_u8(addr + i - 1, b) end
end

-- $1eb0: STA $2000 (self-modified operand); INC ptr lo; BNE; INC ptr hi; RTS
poke(0x1eb0, {0x8D,0x00,0x20, 0xEE,0xB1,0x1E, 0xD0,0x03, 0xEE,0xB2,0x1E, 0x60})
-- $1ec0: LDA #'@'; JMP $1eb0  (replaces the item-name printer)
poke(0x1ec0, {0xA9,0x40, 0x4C,0xB0,0x1E})
-- $1f00: dispatcher. $1ff0 GO, $1ff1 DONE, $1ff2 index, $1ff3 table byte.
poke(0x1f00, {0xAD,0xF0,0x1F, 0xF0,0xFB, 0xA9,0x00, 0x8D,0xF0,0x1F,
              0xAD,0xF2,0x1F, 0x8D,0x38,0x5A, 0xAD,0xF3,0x1F, 0x8D,0x39,0x5A,
              0x20,0x98,0x56, 0xA9,0xFF, 0x20,0xB0,0x1E,
              0xA9,0x01, 0x8D,0xF1,0x1F, 0x4C,0x00,0x1F})
poke(0x1ff0, {0, 0, 0, 0})
poke(0x58d1, {0x4C,0xB0,0x1E})   -- JMP $1eb0
poke(0x5835, {0x20,0xC0,0x1E})   -- JSR $1ec0
cpu.state["PC"].value = 0x1f00
m.video.throttled = false

local lines = {}

-- Print the refs (table byte, first index, count) into a fresh buffer and
-- collect them as "<label> <n> <text>" lines starting at string number n0.
local function run(tb, first, count, label, n0)
  sp:write_u8(0x1eb1, 0x00); sp:write_u8(0x1eb2, 0x20)
  for i = first, first + count - 1 do
    sp:write_u8(0x1ff2, i); sp:write_u8(0x1ff3, tb)
    sp:write_u8(0x1ff1, 0); sp:write_u8(0x1ff0, 1)
    local ok = false
    for _ = 1, 400 do
      emu.wait(0.05)
      if sp:read_u8(0x1ff1) == 1 then ok = true break end
    end
    if not ok then error(string.format("timeout on %02x/%d", tb, i)) end
  end
  local len = sp:read_u8(0x1eb1) + 256 * sp:read_u8(0x1eb2) - 0x2000
  local cur, n = {}, n0
  for a = 0x2000, 0x2000 + len - 1 do
    local b = sp:read_u8(a)
    if b == 0xff then
      if cur[#cur] == "\r" then cur[#cur] = nil end
      lines[#lines + 1] = string.format("%s %d %s", label, n,
        (table.concat(cur)):gsub("\r", "\\n"))
      cur = {}; n = n + 1
    else
      cur[#cur + 1] = string.char(b)
    end
  end
end

run(0x80, 0, 155, "S1", 0)        -- the main table has 155 strings
for t = 0, 2 do                   -- S2: 12 banks of 64 = 768 refs
  run(0x82 + t, 0, 128, "S2", t * 256)
  run(0x82 + t, 128, 128, "S2", t * 256 + 128)
end

m.video.throttled = true
local f = assert(io.open(OUT, "wb"))
f:write(table.concat(lines, "\n"), "\n")
f:close()
return string.format("%d strings -> %s", #lines, OUT)
