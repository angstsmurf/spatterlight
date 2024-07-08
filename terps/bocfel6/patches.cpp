// Copyright 2017-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "patches.h"
#include "memory.h"
#include "types.h"
#include "util.h"
#include "zterp.h"

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}
#endif

struct Replacement {
    uint32_t addr;
    uint32_t n;
    std::vector<uint8_t> in;
    std::vector<uint8_t> out;
    std::function<bool()> active = [] { return true; };
};

struct Patch {
    std::string title;
    const char (*serial)[sizeof header.serial + 1];
    uint16_t release;
    uint16_t checksum;
    std::vector<Replacement> replacements;
};

// The Bureaucracy patch assumes that timed input is available, as it
// uses that to simulate a sleep. If timed input is not available,
// though, it will instead just be a regular @read_char, which will
// block till the user hits a key. This is worse behavior than the
// default, which is to effectively not sleep at all. Timed input is
// only available with Glk, and then only if the Glk implementation
// supports timers.
#ifdef ZTERP_GLK
static bool bureaucracy_active()
{
    return glk_gestalt(gestalt_Timer, 0);
}
#else
static bool bureaucracy_active()
{
    return false;
}
#endif

static std::vector<Patch> patches = {
    // There are several patches for Beyond Zork.
    //
    // In four places it tries to treat a dictionary word as an object.
    // The offending code looks like:
    //
    // <REPLACE-SYN? ,W?CIRCLET ,W?FILM ,W?ZZZP>
    // <REPLACE-ADJ? ,W?CIRCLET ,W?SWIRLING ,W?ZZZP>
    //
    // Here “,W?CIRCLET” is the dictionary word "circlet"; what was
    // intended is “,CIRCLET”, the object. Given that the object number
    // of the circlet is easily discoverable, these calls can be
    // rewritten to use the object number instead of the incorrect
    // dictionary entry.
    //
    // Beyond Zork also uses platform-specific hacks to display
    // character graphics.
    //
    // On IBM PC, if the interpreter clears the pictures bit,
    // Beyond Zork assumes code page 437 is active, and it uses arrows
    // and box-drawing characters which are invalid or gibberish in
    // ZSCII.
    //
    // On Apple IIc, in much the same way, Beyond Zork prints out values
    // which are valid MouseText, but are gibberish in ZSCII.
    //
    // To work around this, replace invalid characters with valid
    // characters. Beyond Zork uses font 1 with IBM and font 3 with
    // Apple, so while the IBM patches have to use a crude conversion
    // (e.g. “U” and “D” for up and down arrows, and “+” for all
    // box-drawing corners), the Apple patches convert to appropriate
    // font 3 values.
    {
        "Beyond Zork", &"870915", 47, 0x3ff4,
        {
            // Circlet object
            { 0x2f8e6, 2, {0xa3, 0x9a}, {0x01, 0x86} },
            { 0x2f8f0, 2, {0xa3, 0x9a}, {0x01, 0x86} },
            { 0x2f902, 2, {0xa3, 0x9a}, {0x01, 0x86} },
            { 0x2f90c, 2, {0xa3, 0x9a}, {0x01, 0x86} },

            // IBM characters
            { 0xe58f, 1, {0xda}, {0x2b} },
            { 0xe595, 1, {0xc4}, {0x2d} },
            { 0xe59d, 1, {0xbf}, {0x2b} },
            { 0xe5a6, 1, {0xc0}, {0x2b} },
            { 0xe5ac, 1, {0xc4}, {0x2d} },
            { 0xe5b4, 1, {0xd9}, {0x2b} },
            { 0xe5c0, 1, {0xb3}, {0x7c} },
            { 0xe5c9, 1, {0xb3}, {0x7c} },
            { 0x3b211, 1, {0x18}, {0x55} },
            { 0x3b244, 1, {0x19}, {0x44} },
            { 0x3b340, 1, {0x1b}, {0x4c} },
            { 0x3b37f, 1, {0x1a}, {0x52} },

            // Apple IIc characters
            { 0xe5ee, 1, {0x4c}, {0x4b} },
            { 0xe5fd, 1, {0x5a}, {0x4e} },
            { 0xe604, 1, {0x5f}, {0x4d} },
            { 0xe86a, 1, {0x5a}, {0x4e} },
            { 0xe8a6, 1, {0x5f}, {0x4d} },
            { 0x3b220, 1, {0x4b}, {0x5c} },
            { 0x3b253, 1, {0x4a}, {0x5d} },
            { 0x3b34f, 1, {0x48}, {0x21} },
            { 0x3b38e, 1, {0x55}, {0x22} },
        },
    },

    {
        "Beyond Zork", &"870917", 49, 0x24d6,
        {
            // Circlet object
            { 0x2f8b6, 2, {0xa3, 0x9c}, {0x01, 0x86} },
            { 0x2f8c0, 2, {0xa3, 0x9c}, {0x01, 0x86} },
            { 0x2f8d2, 2, {0xa3, 0x9c}, {0x01, 0x86} },
            { 0x2f8dc, 2, {0xa3, 0x9c}, {0x01, 0x86} },

            // IBM characters
            { 0xe4c7, 1, {0xda}, {0x2b} },
            { 0xe4cd, 1, {0xc4}, {0x2d} },
            { 0xe4d5, 1, {0xbf}, {0x2b} },
            { 0xe4de, 1, {0xc0}, {0x2b} },
            { 0xe4e4, 1, {0xc4}, {0x2d} },
            { 0xe4ec, 1, {0xd9}, {0x2b} },
            { 0xe4f8, 1, {0xb3}, {0x7c} },
            { 0xe501, 1, {0xb3}, {0x7c} },
            { 0x3b1e1, 1, {0x18}, {0x55} },
            { 0x3b214, 1, {0x19}, {0x44} },
            { 0x3b310, 1, {0x1b}, {0x4c} },
            { 0x3b34f, 1, {0x1a}, {0x52} },

            // Apple IIc characters
            { 0xe526, 1, {0x4c}, {0x4b} },
            { 0xe535, 1, {0x5a}, {0x4e} },
            { 0xe53c, 1, {0x5f}, {0x4d} },
            { 0xe7a2, 1, {0x5a}, {0x4e} },
            { 0xe7de, 1, {0x5f}, {0x4d} },
            { 0x3b1f0, 1, {0x4b}, {0x5c} },
            { 0x3b223, 1, {0x4a}, {0x5d} },
            { 0x3b31f, 1, {0x48}, {0x21} },
            { 0x3b35e, 1, {0x55}, {0x22} },
        },
    },

    {
        "Beyond Zork", &"870923", 51, 0x0cbe,
        {
            // Circlet object
            { 0x2f762, 2, {0xa3, 0x8d}, {0x01, 0x86} },
            { 0x2f76c, 2, {0xa3, 0x8d}, {0x01, 0x86} },
            { 0x2f77e, 2, {0xa3, 0x8d}, {0x01, 0x86} },
            { 0x2f788, 2, {0xa3, 0x8d}, {0x01, 0x86} },

            // IBM characters
            { 0xe4af, 1, {0xda}, {0x2b} },
            { 0xe4b5, 1, {0xc4}, {0x2d} },
            { 0xe4bd, 1, {0xbf}, {0x2b} },
            { 0xe4c6, 1, {0xc0}, {0x2b} },
            { 0xe4cc, 1, {0xc4}, {0x2d} },
            { 0xe4d4, 1, {0xd9}, {0x2b} },
            { 0xe4e0, 1, {0xb3}, {0x7c} },
            { 0xe4e9, 1, {0xb3}, {0x7c} },
            { 0x3b08d, 1, {0x18}, {0x55} },
            { 0x3b0c0, 1, {0x19}, {0x44} },
            { 0x3b1bc, 1, {0x1b}, {0x4c} },
            { 0x3b1fb, 1, {0x1a}, {0x52} },

            // Apple IIc characters
            { 0xe50e, 1, {0x4c}, {0x4b} },
            { 0xe51d, 1, {0x5a}, {0x4e} },
            { 0xe524, 1, {0x5f}, {0x4d} },
            { 0xe78a, 1, {0x5a}, {0x4e} },
            { 0xe7c6, 1, {0x5f}, {0x4d} },
            { 0x3b09c, 1, {0x4b}, {0x5c} },
            { 0x3b0cf, 1, {0x4a}, {0x5d} },
            { 0x3b1cb, 1, {0x48}, {0x21} },
            { 0x3b20a, 1, {0x55}, {0x22} },
        },
    },

    {
        "Beyond Zork", &"871221", 57, 0xc5ad,
        {
            // Circlet object
            { 0x2fc72, 2, {0xa3, 0xba}, {0x01, 0x87} },
            { 0x2fc7c, 2, {0xa3, 0xba}, {0x01, 0x87} },
            { 0x2fc8e, 2, {0xa3, 0xba}, {0x01, 0x87} },
            { 0x2fc98, 2, {0xa3, 0xba}, {0x01, 0x87} },

            // IBM characters
            { 0xe58b, 1, {0xda}, {0x2b} },
            { 0xe591, 1, {0xc4}, {0x2d} },
            { 0xe599, 1, {0xbf}, {0x2b} },
            { 0xe5a2, 1, {0xc0}, {0x2b} },
            { 0xe5a8, 1, {0xc4}, {0x2d} },
            { 0xe5b0, 1, {0xd9}, {0x2b} },
            { 0xe5bc, 1, {0xb3}, {0x7c} },
            { 0xe5c5, 1, {0xb3}, {0x7c} },
            { 0x3b1dd, 1, {0x18}, {0x55} },
            { 0x3b210, 1, {0x19}, {0x44} },
            { 0x3b30c, 1, {0x1b}, {0x4c} },
            { 0x3b34b, 1, {0x1a}, {0x52} },

            // Apple IIc characters
            { 0xe5ea, 1, {0x4c}, {0x4b} },
            { 0xe5f9, 1, {0x5a}, {0x4e} },
            { 0xe600, 1, {0x5f}, {0x4d} },
            { 0xe866, 1, {0x5a}, {0x4e} },
            { 0xe8a2, 1, {0x5f}, {0x4d} },
            { 0x3b1ec, 1, {0x4b}, {0x5c} },
            { 0x3b21f, 1, {0x4a}, {0x5d} },
            { 0x3b31b, 1, {0x48}, {0x21} },
            { 0x3b35a, 1, {0x55}, {0x22} },
        },
    },

    {
        "Beyond Zork", &"880610", 60, 0xa49d,
        {
            // Circlet object
            { 0x2fbfe, 2, {0xa3, 0xc0}, {0x01, 0x87} },
            { 0x2fc08, 2, {0xa3, 0xc0}, {0x01, 0x87} },
            { 0x2fc1a, 2, {0xa3, 0xc0}, {0x01, 0x87} },
            { 0x2fc24, 2, {0xa3, 0xc0}, {0x01, 0x87} },

            // IBM characters
            { 0xe4e3, 1, {0xda}, {0x2b} },
            { 0xe4e9, 1, {0xc4}, {0x2d} },
            { 0xe4f1, 1, {0xbf}, {0x2b} },
            { 0xe4fa, 1, {0xc0}, {0x2b} },
            { 0xe500, 1, {0xc4}, {0x2d} },
            { 0xe508, 1, {0xd9}, {0x2b} },
            { 0xe514, 1, {0xb3}, {0x7c} },
            { 0xe51d, 1, {0xb3}, {0x7c} },
            { 0x3b169, 1, {0x18}, {0x55} },
            { 0x3b19c, 1, {0x19}, {0x44} },
            { 0x3b298, 1, {0x1b}, {0x4c} },
            { 0x3b2d7, 1, {0x1a}, {0x52} },

            // Apple IIc characters
            { 0xe542, 1, {0x4c}, {0x4b} },
            { 0xe551, 1, {0x5a}, {0x4e} },
            { 0xe558, 1, {0x5f}, {0x4d} },
            { 0xe7be, 1, {0x5a}, {0x4e} },
            { 0xe7fa, 1, {0x5f}, {0x4d} },
            { 0x3b178, 1, {0x4b}, {0x5c} },
            { 0x3b1ab, 1, {0x4a}, {0x5d} },
            { 0x3b2a7, 1, {0x48}, {0x21} },
            { 0x3b2e6, 1, {0x55}, {0x22} },
        },
    },

    // Bureaucracy has a routine meant to sleep for a certain amount of
    // time, but it implements this using a busy loop based on the
    // target machine, each machine having its own iteration count which
    // should approximate the number of seconds passed in. This doesn’t
    // work on modern machines at all, as they’re far too fast. A
    // replacement is provided here which does the following:
    //
    // [ Delay n;
    //     @mul n $0a -> n;
    //     @read_char 1 n Pause -> sp;
    //     @rtrue;
    // ];
    //
    // [ Pause;
    //     @rtrue;
    // ];
    {
        "Bureaucracy", &"870212", 86, 0xe024,
        {
            {
                0x2128c, 18,
                {0x02, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x1e, 0x00, 0xd0, 0x2f, 0xde, 0x5c, 0x00, 0x02, 0xd6, 0x2f, 0x03},
                {0x01, 0x00, 0x01, 0x56, 0x01, 0x0a, 0x01, 0xf6, 0x63, 0x01, 0x01, 0x84, 0xa7, 0x00, 0xb0, 0x00, 0x00, 0xb0},
                bureaucracy_active,
            }
        }
    },
    {
        "Bureaucracy", &"870602", 116, 0xfc65,
        {
            {
                0x212d0, 18,
                {0x02, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x1e, 0x00, 0xd0, 0x2f, 0xde, 0x64, 0x00, 0x02, 0xd6, 0x2f, 0x03},
                {0x01, 0x00, 0x01, 0x56, 0x01, 0x0a, 0x01, 0xf6, 0x63, 0x01, 0x01, 0x84, 0xb8, 0x00, 0xb0, 0x00, 0x00, 0xb0},
                bureaucracy_active,
            }
        }
    },
    {
        "Bureaucracy", &"880521", 160, 0x07f0,
        {
            {
                0x212c8, 18,
                {0x02, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x1e, 0x00, 0xd0, 0x2f, 0xdc, 0xce, 0x00, 0x02, 0xd6, 0x2f, 0x03},
                {0x01, 0x00, 0x01, 0x56, 0x01, 0x0a, 0x01, 0xf6, 0x63, 0x01, 0x01, 0x84, 0xb6, 0x00, 0xb0, 0x00, 0x00, 0xb0},
                bureaucracy_active,
            }
        }
    },

    // This is in a routine which iterates over all attributes of an
    // object, but due to an off-by-one error, attribute 48 (0x30) is
    // included, which is not valid, as the last attribute is 47 (0x2f);
    // there are 48 attributes but the count starts at 0.
    {
        "Sherlock", &"871214", 21, 0x79b2,
        {{ 0x223ac, 1, {0x30}, {0x2f} }},
    },
    {
        "Sherlock", &"880112", 22, 0xcb96,
        {{ 0x225a4, 1, {0x30}, {0x2f} }},
    },
    {
        "Sherlock", &"880127", 26, 0x26ba,
        {{ 0x22818, 1, {0x30}, {0x2f} }},
    },
    {
        "Sherlock", &"880324", 4, 0x7086,
        {{ 0x22498, 1, {0x30}, {0x2f} }},
    },

    // The operands to @get_prop here are swapped, so swap them back.
    {
        "Stationfall", &"870326", 87, 0x71ae,
        {{ 0xd3d4, 3, {0x31, 0x0c, 0x73}, {0x51, 0x73, 0x0c} }},
    },
    {
        "Stationfall", &"870430", 107, 0x2871,
        {{ 0xe3fe, 3, {0x31, 0x0c, 0x77}, {0x51, 0x77, 0x0c} }},
    },

    // The Solid Gold (V5) version of Wishbringer calls @show_status, but
    // that is an illegal instruction outside of V3. Convert it to @nop.
    {
        "Wishbringer", &"880706", 23, 0x4222,
        {{ 0x1f910, 1, {0xbc}, {0xb4} }},
    },

    // Robot Finds Kitten attempts to sleep with the following:
    //
    // [ Func junk;
    //     @aread junk 0 10 PauseFunc -> junk;
    // ];
    //
    // However, since “junk” is a local variable with value 0 instead of a
    // text buffer, this is asking to read from/write to address 0. This
    // works in some interpreters, but Bocfel is more strict, and aborts
    // the program. Rewrite this instead to:
    //
    // @read_char 1 10 PauseFunc -> junk;
    // @nop; ! This is for padding.
    {
        "Robot Finds Kitten", &"130320", 7, 0x4a18,
        {
            {
                0x4912, 8,
                {0xe4, 0x94, 0x05, 0x00, 0x0a, 0x12, 0x5a, 0x05},
                {0xf6, 0x53, 0x01, 0x0a, 0x12, 0x5a, 0x05, 0xb4},
            },
        }
    },
#ifdef SPATTERLIGHT
    // The Blorb demo “The Spy Who Came In From The Garden” seems to
    // always be in a state of disrepair. One particular version appears
    // to work better than most, but for a bad call to @sound_effect:
    //
    // [Routine number;
    //     @sound_effect number 2 255 4;
    // ];
    //
    // The “4” above is a routine to call, which is clearly invalid.
    // The easiest way to work around this is to just rewrite it to not
    // include the routine call; this becomes:
    //
    // @sound_effect number 2 255;
    // @nop; ! This is for padding.
    {
        "The Spy Who Came In From The Garden", &"980124", 1, 0x260,
        {
            {
                0xb6c2, 5,
                {0x95, 0x01, 0x02, 0xff, 0x01},
                {0x97, 0x01, 0x02, 0xff, 0xb4},
            },
        }
    },

    {
        "Journey", &"890706", 83, 0xd2b8,
        {
            // REFRESH-CHARACTER-COMMAND-AREA
            {
                0x50b1, 4,
                {0x54, 0x1e, 0x04, 0x00},
                {0xab, 0x01, 0x00, 0x00},
            },
            // GRAPHIC-STAMP
            {
                0x4975, 5,
                {0x4f, 0x5f, 0x00, 0x05, 0x4f},
                {0xb0, 0x4f,  0x5f, 0x00, 0x05},
            },

             // INIT-SCREEN
            {
                0x4da5, 4,
                {0x41, 0x8f, 0x06, 0x51},
                {0xb0, 0x00, 0x8f, 0x06},
            },

            // CHANGE-NAME hyphen
            {
                0x7a0f, 1,
                {0x2d},
                {0x5f},
            },

            // CHANGE-NAME hyphen 2
            {
                0x7a19, 1,
                {0x2d},
                {0x5f},
            },

            // DIVIDER
            {
                0x194c9, 4,
                {0xff, 0x7f, 0x01, 0xc5},
                {0xb0, 0xff, 0x7f, 0x01},
            },

            // WCENTER
            {
                0x4935, 6,
                {0xd9, 0x2f, 0x02, 0x81, 0x01, 0x02},
                {0xb0, 0xd9, 0x2f, 0x02, 0x81, 0x01},
            },

            // GET-COMMAND
            {
                0x4b65, 4,
                {0x42, 0xc8, 0x0c, 0x4b},
                {0x42, 0xc8, 0x0d, 0x4b},
            },

            // CHANGE-NAME
            {
                0x78dd, 3,
                {0x2d, 0x04, 0xb3},
                {0xb0, 0x2d, 0x04},
            },

            // READ-ELVISH
            {
                0x7bb8, 2,
                {0x2d, 0x05},
                {0xab, 0x01}, // RET L00
            },

            // PRINT-COLUMNS
            {
                0x5f7d, 5,
                {  0x2d, 0x04, 0x1e, 0xda, 0x1f },
                {  0xb0, 0x2d, 0x04, 0x1e, 0xda },
            },

            // ERASE-COMMAND
            {
                0x5298, 4,
                {  0xee, 0xbf, 0x01, 0xb0 },
                {  0xb0, 0xee, 0xbf, 0x01 },
            },

            // PRINT-CHARACTER-COMMANDS
            {
                0x5b59, 6,
                {  0x0d, 0x03, 0x05, 0x2d, 0x06, 0x1e },
                {  0xb0, 0x0d, 0x03, 0x05, 0x2d, 0x06 },
            },

            // Later versions of Journey change font by calling a routine named
            // CHANGE-FONT. (Earlier versions call the FONT/set_font opcode
            // directly.) This routine returns false unless the argument is 3 or 4,
            // meaning that it will never set the font back to the standard font 1
            // (unless font 3 or 4 are unavailable, when it will fall back to this.)

            // This is likely unintentional, and we want it to be able to set the font
            // in the main buffer text window back to 1, otherwise it will always use
            // fixed-width font 4. We patch this by replacing the RFALSE label
            // with a jump to the <FONT 1> instruction at the end of the routine.
            {
                0x4757, 1,
                {  0x40 },
                {  0x4b },
            },


        }
    },

    {
        "Shogun", &"890706", 322, 0x5c88,
        {

            // V-DEFINE
            {
                0x1193b, 4,
                { 0xed, 0x3f, 0xff, 0xff },
                { 0xb0, 0xed, 0x3f, 0xff },
            },

            // DO-HINTS
            {
                0x4abc9, 3,
                { 0xf1, 0x7f, 0x00 },
                { 0xb0, 0xf1, 0x7f },
            },

            // DISPLAY-BORDER
            {
                0x126a3, 3,
                { 0xc1, 0x95, 0x43 },
                { 0xb0, 0x95, 0x43 },
            },

            // GET-FROM-MENU
            {
                0x11e69, 2,
                { 0xff, 0x7f },
                { 0xab, 0x01 } // RET L00
            },


            // SCENE-SELECT
            {
                0x10cf5, 2,
                { 0xcf, 0x1f },
                { 0xb0, 0x1f }
            },

            // UPDATE-STATUS-LINE
            {
                0x12309, 1,

                { 0x0f },
                { 0xb0 }
            },

            // INTERLUDE-STATUS-LINE
            {
                0x124dd, 1,
                { 0xc1 },
                { 0xb0 }
            },

            // SETUP-TEXT-AND-STATUS
            {
                0x12201, 1,
                { 0xff },
                { 0xb0 } 
            },

            // CENTER-PIC-X
            {
                0x1272d, 1,
                { 0xbe },
                { 0xb0 }
            },

            // CENTER-PIC
            {
                0x12779, 1,
                { 0xbe },
                { 0xb0 }
            },

//            // MARGINAL-PIC
//            {
//                0x127f5, 1,
//                { 0xff },
//                { 0xb0 }
//            },


            // DISPLAY-MAZE
            {
                0x3db2d, 1,
                { 0x8f },
                { 0xb0 }
            },

            // DISPLAY-MAZE-PIC
            {
                0x3da15, 1,
                { 0xa0 },
                { 0xb0 }
            },

            // MAZE-MOUSE-F
            {
                0x3d8d5, 2,
                { 0xcf, 0x1f },
                { 0xab, 0x01 } // RET L00
            },
        }
    },

    {
        "Arthur", &"890714", 74, 0xd526,
        {
            // DO-HINTS
            {
                0x393b9, 3,
                { 0xf1, 0x7f, 0x00 },
                { 0xb0, 0xf1, 0x7f },
            }
        }
    },

    {
        "Zork Zero", &"890714", 393, 0x791c,
        {
            // UPDATE-STATUS-LINE
            {
                0xeb61, 4,
                { 0xeb, 0x7f, 0x01, 0xbe },
                { 0xb1, 0xeb, 0x7f, 0x01 }
            },

            // INIT-STATUS-LINE
            {
                0x1b9e1, 6,
                { 0x0d, 0x02, 0x01, 0x0d, 0x05, 0x40 },
                { 0xb0, 0x0d, 0x02, 0x01, 0x0d, 0x05 }
            },

            // V-DEFINE
            {
                0x12dd0, 6,
                { 0x98, 0x40, 0x00, 0xa0, 0x00, 0xca },
                { 0xb0, 0x98, 0x40, 0x00, 0xa0, 0x00 }
            },

            // DO-HINTS
            {
                0x35a40, 6,
                { 0xef, 0x1f, 0xff, 0xff, 0x00, 0x88 },
                { 0xef, 0x1f, 0xff, 0xff, 0x00, 0xb0 }
            },

            // DRAW-TOWER
            {
                0x327ad, 3,
                { 0xed, 0x7f, 0x07 },
                { 0xb0, 0x7f, 0x07 }
            },

            // B-MOUSE-PEG-PICK
            {
                0x3232d, 3,
                { 0xbe, 0x06, 0x4f },
                { 0xab, 0x02, 0x4f } // 0xab 0x02: RET L01
            },

            // B-MOUSE-WEIGHT-PICK
            {
                0x323b5, 3,
                { 0xda, 0x4f, 0xbd },
                { 0xab, 0x02, 0xbd } // 0xab 0x02: RET L01
            },

            // SETUP-PBOZ
            {
                0x225bd, 3,
                { 0x0d, 0x01, 0x02 },
                { 0xb0, 0x01, 0x02 }
            },

            // PBOZ-CLICK
            {
                0x22491, 3,
                { 0x0d, 0x05, 0x02 },
                { 0xab, 0x02, 0x02 }  // 0xab 0x02: RET L01
            },

            // DRAW-SN-BOXES
            {
                0x33fdd, 3,
                { 0x0d, 0x05, 0x01 },
                { 0xb0, 0x05, 0x01 }
            },

            // DRAW-PILE
            {
                0x3405d, 3,
                { 0xeb, 0x7f, 0x01 },
                { 0xb0, 0x7f, 0x01 }
            },

            // DRAW-FLOWERS
            {
                0x34101, 3,
                { 0x0d, 0x01, 0x01 },
                { 0xb0, 0x01, 0x01 }
            },

            // SETUP-FANUCCI
            {
                0x2a069, 3,
                { 0x98, 0x40, 0x00 },
                { 0xb0, 0x40, 0x00 }
            },

            // FANUCCI
            {
                0x2a2c9, 3,
                { 0xda, 0x4f, 0xbd },
                { 0xb0, 0x4f, 0xbd },
            }
        },

    }
#endif
};

static bool apply_patch(const Replacement &r)
{
    if (r.addr >= header.static_start &&
        r.addr + r.n < memory_size &&
        std::memcmp(&memory[r.addr], r.in.data(), r.n) == 0) {

        std::memcpy(&memory[r.addr], r.out.data(), r.n);

        return true;
    }

    return false;
}

void apply_patches()
{
    fprintf(stderr, "apply_patches: header.checksum %x\n", header.checksum);
    for (const auto &patch : patches) {
        if (std::memcmp(*patch.serial, header.serial, sizeof header.serial) == 0 &&
            patch.release == header.release &&
            patch.checksum == header.checksum) {

            for (const auto &replacement : patch.replacements) {
                if (replacement.active()) {
                    apply_patch(replacement);
                }
            }
        }
    }
}

static bool read_into(std::vector<uint8_t> &buf, long count)
{
    for (long i = 0; i < count; i++) {
        long b;
        bool valid;
        char *p = std::strtok(nullptr, " \t[],");
        if (p == nullptr) {
            return false;
        }
        b = parseint(p, 16, valid);
        if (!valid || b < 0 || b > 255) {
            return false;
        }
        buf.push_back(b);
    }

    return true;
}

// User patches have the form:
//
// addr count original... replacement...
//
// For example, one of the Stationfall fixes would look like this:
//
// 0xe3fe 3 0x31 0x0c 0x77 0x51 0x77 0x0c
//
// Square brackets and commas are treated as whitespace for the actual
// bytes, so for convenience this could also be written:
//
// 0xe3fe 3 [0x31, 0x0c, 0x77] [0x51, 0x77, 0x0c]
void apply_user_patch(std::string patchstr)
{
    char *p;
    bool valid;
    uint32_t addr, count;
    std::vector<uint8_t> in, out;

    p = std::strtok(&patchstr[0], " \t");
    if (p == nullptr) {
        throw PatchStatus::SyntaxError();
    }

    addr = (uint32_t)parseint(p, 16, valid);
    if (!valid) {
        throw PatchStatus::SyntaxError();
    }

    p = std::strtok(nullptr, " \t");
    if (p == nullptr) {
        throw PatchStatus::SyntaxError();
    }

    count = (uint32_t)parseint(p, 10, valid);
    if (!valid) {
        throw PatchStatus::SyntaxError();
    }

    if (!read_into(in, count) || !read_into(out, count)) {
        throw PatchStatus::SyntaxError();
    }

    Replacement replacement = {
        addr,
        count,
        in,
        out,
    };

    if (!apply_patch(replacement)) {
        throw PatchStatus::NotFound();
    }
}
