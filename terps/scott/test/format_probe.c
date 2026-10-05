/* Loader fingerprint for the disk and tape image format tests.

   Runs DetectGame() -- the whole container pipeline: .z80/.sna/.tap/.tzx,
   .d64/.t64 with the unp64 depackers, .atr, .dsk/.do/.nib/.woz, the TI-99/4A
   dumps -- on one game file and prints a fingerprint of everything the loaders
   produced: which game was detected, the header, a CRC of each database table,
   and a CRC of every picture blob. The fingerprint holds no game text, so the
   goldens can be committed although the games cannot.

   A loader regression shows up as the one line that changed: a wrong offset
   in a disk extractor changes `usimages` and leaves `rooms` alone, a depacker
   regression changes everything or fails the load.

   With SCOTT_PROBE_VERBOSE set the tables are printed in full after the
   fingerprint, to see what a changed CRC is about.

   Disks that hold several games ask which one to load; the answer is read
   from stdin, as in the real interpreter.

   Reuses every engine object except scott.o's glk_main (renamed via
   -Dglk_main=scott_real_glk_main), same trick as death_test.c.

     usage: [SCOTT_PROBE_VERBOSE=1] scott_format_probe <game-file> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "glk.h"
#include "glkstart.h"

#include "detect_game.h"
#include "scott.h"
#include "scott_defines.h"
#include "scott_game_info.h"

#include "decompressz80.h"
#include "irmak.h"
#include "line_drawing.h"
#include "load_ti99_4a.h"
#include "sagagraphics.h"

extern int number_of_images;
extern int image_version;
extern uint8_t *descrambletable;

static int verbose = 0;

static uLong AddBytes(uLong crc, const void *data, size_t len)
{
    if (data == NULL || len == 0)
        return crc;
    return crc32(crc, (const Bytef *)data, (uInt)len);
}

/* A NULL string and an empty one must not hash alike, and neither may two
   strings run together, so every string is followed by a marker byte. */
static uLong AddString(uLong crc, const char *s)
{
    if (s == NULL)
        return AddBytes(crc, "\x01", 1);
    crc = AddBytes(crc, s, strlen(s));
    return AddBytes(crc, "\x00", 1);
}

static uLong AddInt(uLong crc, long value)
{
    uint8_t b[4] = { value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff,
        (value >> 24) & 0xff };
    return AddBytes(crc, b, 4);
}

static void Line(const char *name, int count, uLong crc)
{
    printf("%-10s %4d %08lx\n", name, count, crc);
}

static void PrintString(const char *label, int index, const char *s)
{
    if (verbose)
        printf("    %s[%d] = \"%s\"\n", label, index, s ? s : "(null)");
}

static void ProbeDatabase(void)
{
    uLong crc = crc32(0, NULL, 0);
    for (int i = 0; i <= GameHeader.NumRooms; i++) {
        crc = AddString(crc, Rooms[i].Text);
        crc = AddBytes(crc, Rooms[i].Exits, 6);
        crc = AddInt(crc, Rooms[i].Image);
        PrintString("room", i, Rooms[i].Text);
        if (verbose)
            printf("    exits[%d] = %d %d %d %d %d %d image=%d\n", i,
                Rooms[i].Exits[0], Rooms[i].Exits[1], Rooms[i].Exits[2],
                Rooms[i].Exits[3], Rooms[i].Exits[4], Rooms[i].Exits[5],
                Rooms[i].Image);
    }
    Line("rooms", GameHeader.NumRooms + 1, crc);

    crc = crc32(0, NULL, 0);
    for (int i = 0; i <= GameHeader.NumItems; i++) {
        crc = AddString(crc, Items[i].Text);
        crc = AddString(crc, Items[i].AutoGet);
        crc = AddInt(crc, Items[i].Location);
        crc = AddInt(crc, Items[i].InitialLoc);
        crc = AddInt(crc, Items[i].Flag);
        crc = AddInt(crc, Items[i].Image);
        PrintString("item", i, Items[i].Text);
        if (verbose)
            printf("    itemloc[%d] = %d flag=%d image=%d autoget=%s\n", i,
                Items[i].InitialLoc, Items[i].Flag, Items[i].Image,
                Items[i].AutoGet ? Items[i].AutoGet : "(null)");
    }
    Line("items", GameHeader.NumItems + 1, crc);

    if (Verbs != NULL && Nouns != NULL) {
        uLong vcrc = crc32(0, NULL, 0), ncrc = crc32(0, NULL, 0);
        for (int i = 0; i <= GameHeader.NumWords; i++) {
            vcrc = AddString(vcrc, Verbs[i]);
            ncrc = AddString(ncrc, Nouns[i]);
            PrintString("verb", i, Verbs[i]);
            PrintString("noun", i, Nouns[i]);
        }
        Line("verbs", GameHeader.NumWords + 1, vcrc);
        Line("nouns", GameHeader.NumWords + 1, ncrc);
    }

    crc = crc32(0, NULL, 0);
    for (int i = 0; i <= GameHeader.NumMessages; i++) {
        crc = AddString(crc, Messages[i]);
        PrintString("message", i, Messages[i]);
    }
    Line("messages", GameHeader.NumMessages + 1, crc);

    /* The TI-99/4A loader keeps the action tables as the cartridge's own
       bytecode; every other loader fills Actions[]. */
    if (Actions != NULL) {
        crc = crc32(0, NULL, 0);
        for (int i = 0; i <= GameHeader.NumActions; i++) {
            crc = AddInt(crc, Actions[i].Vocab);
            for (int j = 0; j < 5; j++)
                crc = AddInt(crc, Actions[i].Condition[j]);
            crc = AddInt(crc, Actions[i].Opcode[0]);
            crc = AddInt(crc, Actions[i].Opcode[1]);
            if (verbose)
                printf("    action[%d] = %d : %d %d %d %d %d : %d %d\n", i,
                    Actions[i].Vocab, Actions[i].Condition[0],
                    Actions[i].Condition[1], Actions[i].Condition[2],
                    Actions[i].Condition[3], Actions[i].Condition[4],
                    Actions[i].Opcode[0], Actions[i].Opcode[1]);
        }
        Line("actions", GameHeader.NumActions + 1, crc);
    }
    if (ti99_implicit_actions != NULL)
        Line("ti99impl", (int)ti99_implicit_extent,
            AddBytes(crc32(0, NULL, 0), ti99_implicit_actions, ti99_implicit_extent));
    if (ti99_explicit_actions != NULL)
        Line("ti99expl", (int)ti99_explicit_extent,
            AddBytes(crc32(0, NULL, 0), ti99_explicit_actions, ti99_explicit_extent));

    crc = crc32(0, NULL, 0);
    for (int i = 0; i < MAX_SYSMESS; i++) {
        crc = AddString(crc, sys[i]);
        PrintString("sys", i, sys[i]);
    }
    Line("sysmess", MAX_SYSMESS, crc);
}

static void ProbeGraphics(void)
{
    if (title_screen != NULL)
        Line("titletext", (int)strlen(title_screen),
            AddString(crc32(0, NULL, 0), title_screen));

    if (ZXLoadingScreen != NULL)
        Line("zxscreen", ZX_SCREEN_SIZE,
            AddBytes(crc32(0, NULL, 0), ZXLoadingScreen, ZX_SCREEN_SIZE));

    /* The US releases: one blob per picture, cut out of the disk image. */
    int count = 0;
    uLong crc = crc32(0, NULL, 0);
    for (USImage *img = USImages; img != NULL; img = img->next) {
        if (img->imagedata == NULL || img->datasize == 0)
            continue;
        count++;
        crc = AddInt(crc, img->usage);
        crc = AddInt(crc, img->index);
        crc = AddInt(crc, img->systype);
        crc = AddInt(crc, img->cropleft);
        crc = AddInt(crc, img->cropright);
        crc = AddInt(crc, (long)img->datasize);
        crc = AddBytes(crc, img->imagedata, img->datasize);
        if (verbose)
            printf("    usimage sys=%d usage=%d index=%d size=%zu crc=%08lx\n",
                img->systype, img->usage, img->index, img->datasize,
                AddBytes(crc32(0, NULL, 0), img->imagedata, img->datasize));
    }
    if (count)
        Line("usimages", count, crc);

    /* ReadApple2DOSFile() cuts 0x182 bytes of lookup table out of M2. */
    if (descrambletable != NULL)
        Line("descramble", 0x182, AddBytes(crc32(0, NULL, 0), descrambletable, 0x182));

    /* The UK releases: Irmak tile pictures, or Howarth vector pictures. */
    if (images != NULL && number_of_images > 0) {
        crc = crc32(0, NULL, 0);
        for (int i = 0; i < number_of_images; i++) {
            crc = AddInt(crc, images[i].width);
            crc = AddInt(crc, images[i].height);
            crc = AddInt(crc, images[i].xoff);
            crc = AddInt(crc, images[i].yoff);
            crc = AddInt(crc, (long)images[i].datasize);
            crc = AddBytes(crc, images[i].imagedata, images[i].datasize);
            if (verbose)
                printf("    irmak %d: %dx%d at %d,%d size=%zu\n", i,
                    images[i].width, images[i].height, images[i].xoff,
                    images[i].yoff, images[i].datasize);
        }
        printf("irmak      %4d %08lx version=%d\n", number_of_images, crc, image_version);
        Line("tiles", 256, AddBytes(crc32(0, NULL, 0), tiles, 256 * 8));
    }

    if (LineImages != NULL && Game->picture_format_version == 99) {
        crc = crc32(0, NULL, 0);
        for (int i = 0; i <= GameHeader.NumRooms; i++) {
            crc = AddInt(crc, LineImages[i].bgcolour);
            crc = AddInt(crc, (long)LineImages[i].size);
            crc = AddBytes(crc, LineImages[i].data, LineImages[i].size);
        }
        Line("lineimages", GameHeader.NumRooms + 1, crc);
    }
}

void glk_main(void)
{
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    if (Bottom == NULL) {
        fprintf(stderr, "format_probe: could not open window\n");
        return;
    }
    glk_set_window(Bottom);

    if (game_file == NULL) {
        fprintf(stderr, "usage: scott_format_probe <game-file>\n");
        return;
    }
    verbose = getenv("SCOTT_PROBE_VERBOSE") != NULL;

    /* Populate sys[] the way the real glk_main does before DetectGame(),
       which then overwrites the entries the game defines itself. */
    for (int i = 0; i < MAX_SYSMESS; i++)
        sys[i] = sysdict_i_am[i] ? sysdict_i_am[i] : sysdict[i];

    GameIDType id = DetectGame(game_file);

    /* Whatever a multi-game menu wrote to the window ends here. */
    printf("\n== fingerprint\n");

    if (id == UNKNOWN_GAME) {
        printf("UNKNOWN_GAME\n");
        glk_exit();
    }

    /* Names, not enum values: adding a game to GameIDType must not change
       every golden. */
    static const char *const sysnames[] = { "unknown", "msdos", "c64", "atari8",
        "apple2", "ti994a", "apple2-lines", "atari8-lines", "c64-tiny" };
    static const char *const typenames[] = { "none", "gremlins", "sherwood",
        "savage-island", "secret-mission", "seas-of-blood", "us", "old-style" };
    int type = Game ? (int)Game->type : 0;
    printf("game       %s\n", (Game && Game->Title) ? Game->Title : "-");
    printf("variant    sys=%s type=%s subtype=%d\n",
        (unsigned)CurrentSys < 9 ? sysnames[CurrentSys] : "?",
        (unsigned)type < 8 ? typenames[type] : "?", Game ? (int)Game->subtype : 0);
    printf("header     items=%d actions=%d words=%d rooms=%d maxcarry=%d start=%d "
           "treasures=%d wordlen=%d light=%d messages=%d treasureroom=%d\n",
        GameHeader.NumItems, GameHeader.NumActions, GameHeader.NumWords,
        GameHeader.NumRooms, GameHeader.MaxCarry, GameHeader.PlayerRoom,
        GameHeader.Treasures, GameHeader.WordLength, GameHeader.LightTime,
        GameHeader.NumMessages, GameHeader.TreasureRoom);
    printf("picture    width=%d height=%d\n", ImageWidth, ImageHeight);

    ProbeDatabase();
    ProbeGraphics();

    glk_exit();
}
