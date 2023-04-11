/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2002-2011  Simon Baldwin, simon_baldwin@yahoo.com
 * Mac portions Copyright (C) 2002  Ben Hines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Glk interface for Level 9
 * -------------------------
 *
 * This module contains the the Glk porting layer for the Level 9
 * interpreter.  It defines the Glk arguments list structure, the
 * entry points for the Glk library framework to use, and all
 * platform-abstracted I/O to link to Glk's I/O.
 *
 * The following items are omitted from this Glk port:
 *
 *  o Glk tries to assert control over _all_ file I/O.  It's just too
 *    disruptive to add it to existing code, so for now, the Level 9
 *    interpreter is still dependent on stdio and the like.
 *
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>

#include "level9.h"

#include "glk.h"

L9UINT16 random_array[100];

extern glui32 gli_determinism;


/*
 * True and false definitions -- usually defined in glkstart.h, but we need
 * them early, so we'll define them here too.  We also need NULL, but that's
 * normally from stdio.h or one of it's cousins.
 */
#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

/* File path delimiter, used to be #defined in v2 interpreter. */
#if defined(_Windows) || defined(__MSDOS__) \
    || defined (_WIN32) || defined (__WIN32__)
static const char GLN_FILE_DELIM = '\\';
#else
static const char GLN_FILE_DELIM = '/';
#endif


/*---------------------------------------------------------------------*/
/*  Module variables, miscellaneous other stuff                        */
/*---------------------------------------------------------------------*/

/* Glk Level 9 port version number. */
static const glui32 GLN_PORT_VERSION = 0x00020201;

/*
 * We use a maximum of three Glk windows, one for status, one for pictures,
 * and one for everything else.  The status and pictures windows may be
 * NULL, depending on user selections and the capabilities of the Glk
 * library.
 */
static winid_t gln_main_window = NULL,
               gln_status_window = NULL,
               gln_graphics_window = NULL;

/*
 * Transcript stream and input log.  These are NULL if there is no current
 * collection of these strings.
 */
static strid_t gln_transcript_stream = NULL,
               gln_inputlog_stream = NULL;

/* Input read log stream, for reading back an input log. */
static strid_t gln_readlog_stream = NULL;

/* Note about whether graphics is possible, or not. */
static int gln_graphics_possible = TRUE;

/* Options that may be turned off by command line flags. */
static int gln_graphics_enabled = TRUE,
           gln_intercept_enabled = TRUE,
           gln_prompt_enabled = TRUE,
           gln_loopcheck_enabled = TRUE,
           gln_abbreviations_enabled = TRUE,
           gln_commands_enabled = TRUE;

/* Reason for stopping the game, used to detect restarts and ^C exits. */
static enum {
  STOP_NONE, STOP_FORCE, STOP_RESTART, STOP_EXIT
}
gln_stop_reason = STOP_NONE;

/* Level 9 standard input prompt string. */
static const char * const GLN_INPUT_PROMPT = "> ";

/*
 * Typedef equivalents for interpreter types (uncapitalized to avoid appearing
 * as macros), and some internal interpreter symbols symbols used for our own
 * deviant purposes.
 */
typedef L9BOOL gln_bool;
typedef L9BYTE gln_byte;
typedef L9UINT16 gln_uint16;
typedef L9UINT32 gln_uint32;

extern void save (void);
extern void restore (void);
extern gln_bool Cheating;
extern gln_byte *startdata;
extern gln_uint32 FileSize;

/* Forward declarations of event wait and other miscellaneous functions. */
static void gln_event_wait (glui32 wait_type, event_t * event);
static void gln_event_wait_2 (glui32 wait_type_1,
                              glui32 wait_type_2, event_t * event);

static void gln_watchdog_tick (void);
static void gln_standout_string (const char *message);

static int gln_confirm (const char *prompt);


/*---------------------------------------------------------------------*/
/*  Glk port utility functions                                         */
/*---------------------------------------------------------------------*/

/*
 * gln_fatal()
 *
 * Fatal error handler.  The function returns, expecting the caller to
 * abort() or otherwise handle the error.
 */
static void
gln_fatal (const char *string)
{
  /*
   * If the failure happens too early for us to have a window, print
   * the message to stderr.
   */
  if (!gln_main_window)
    {
      fprintf (stderr, "\n\nINTERNAL ERROR: %s\n", string);

      fprintf (stderr, "\nPlease record the details of this error, try to"
                       " note down everything you did to cause it, and email"
                       " this information to simon_baldwin@yahoo.com.\n\n");
      return;
    }

  /* Cancel all possible pending window input events. */
  glk_cancel_line_event (gln_main_window, NULL);
  glk_cancel_char_event (gln_main_window);

  /* Print a message indicating the error. */
  glk_set_window (gln_main_window);
  glk_set_style (style_Normal);
  glk_put_string ("\n\nINTERNAL ERROR: ");
  glk_put_string ((char *) string);

  glk_put_string ("\n\nPlease record the details of this error, try to"
                  " note down everything you did to cause it, and email"
                  " this information to simon_baldwin@yahoo.com.\n\n");
}


/*
 * gln_malloc()
 * gln_realloc()
 *
 * Non-failing malloc and realloc; call gln_fatal and exit if memory
 * allocation fails.
 */
static void *
gln_malloc (size_t size)
{
  void *pointer;

  pointer = malloc (size);
  if (!pointer)
    {
      gln_fatal ("GLK: Out of system memory");
      glk_exit ();
    }

  return pointer;
}

static void *
gln_realloc (void *ptr, size_t size)
{
  void *pointer;

  pointer = realloc (ptr, size);
  if (!pointer)
    {
      gln_fatal ("GLK: Out of system memory");
      glk_exit ();
    }

  return pointer;
}


/*
 * gln_strncasecmp()
 * gln_strcasecmp()
 *
 * Strncasecmp and strcasecmp are not ANSI functions, so here are local
 * definitions to do the same jobs.
 *
 * They're global here so that the core interpreter can use them; otherwise
 * it tries to use the non-ANSI str[n]icmp() functions.
 */
int
gln_strncasecmp (const char *s1, const char *s2, size_t n)
{
  size_t index;

  for (index = 0; index < n; index++)
    {
      int diff;

      diff = glk_char_to_lower (s1[index]) - glk_char_to_lower (s2[index]);
      if (diff < 0 || diff > 0)
        return diff < 0 ? -1 : 1;
    }

  return 0;
}

int
gln_strcasecmp (const char *s1, const char *s2)
{
  size_t s1len, s2len;
  int result;

  s1len = strlen (s1);
  s2len = strlen (s2);

  result = gln_strncasecmp (s1, s2, s1len < s2len ? s1len : s2len);
  if (result < 0 || result > 0)
    return result;
  else
    return s1len < s2len ? -1 : s1len > s2len ? 1 : 0;
}


/*---------------------------------------------------------------------*/
/*  Glk port stub graphics functions                                   */
/*---------------------------------------------------------------------*/

/*
 * If we're working with a very stripped down, old, or lazy Glk library
 * that neither offers Glk graphics nor graphics stubs functions, here
 * we define our own stubs, to avoid link-time errors.
 */
#ifndef GLK_MODULE_IMAGE
static glui32
glk_image_draw (winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
  return FALSE;
}
static glui32
glk_image_draw_scaled (winid_t win, glui32 image, glsi32 val1, glsi32 val2,
                       glui32 width, glui32 height)
{
  return FALSE;
}
static glui32
glk_image_get_info (glui32 image, glui32 * width, glui32 * height)
{
  return FALSE;
}
static void
glk_window_flow_break (winid_t win)
{
}
static void
glk_window_erase_rect (winid_t win, glsi32 left, glsi32 top,
                       glui32 width, glui32 height)
{
}
static void
glk_window_fill_rect (winid_t win, glui32 color, glsi32 left, glsi32 top,
                      glui32 width, glui32 height)
{
}
static void
glk_window_set_background_color (winid_t win, glui32 color)
{
}
#endif


/*---------------------------------------------------------------------*/
/*  Glk port CRC functions                                             */
/*---------------------------------------------------------------------*/

/* CRC table initialization polynomial. */
static const gln_uint16 GLN_CRC_POLYNOMIAL = 0xa001;


/*
 * gln_get_buffer_crc()
 *
 * Return the CRC of the bytes buffer[0..length-1].
 *
 * This algorithm is selected to match the CRCs used in L9cut.  Because of
 * the odd way CRCs are padded when L9cut calculates the CRC, this function
 * allows a count of NUL padding bytes to be included within the return CRC.
 */
static gln_uint16
gln_get_buffer_crc (const void *void_buffer, size_t length, size_t padding)
{
  static int is_initialized = FALSE;
  static gln_uint16 crc_table[UCHAR_MAX + 1];

  const char *buffer = (const char *) void_buffer;
  gln_uint16 crc;
  size_t index;

  /* Build the static CRC lookup table on first call. */
  if (!is_initialized)
    {
      for (index = 0; index < UCHAR_MAX + 1; index++)
        {
          int bit;

          crc = (gln_uint16) index;
          for (bit = 0; bit < CHAR_BIT; bit++)
            crc = crc & 1 ? GLN_CRC_POLYNOMIAL ^ (crc >> 1) : crc >> 1;

          crc_table[index] = crc;
        }

      is_initialized = TRUE;

      /* CRC lookup table self-test, after is_initialized set -- recursion. */
      assert (gln_get_buffer_crc ("123456789", 9, 0) == 0xbb3d);
    }

  /* Start with zero in the crc, then update using table entries. */
  crc = 0;
  for (index = 0; index < length; index++)
    crc = crc_table[(gln_uint16)(crc ^ buffer[index]) & UCHAR_MAX] ^ (crc >> CHAR_BIT);

  /* Add in any requested NUL padding bytes. */
  for (index = 0; index < padding; index++)
    crc = crc_table[(gln_uint16)(crc & UCHAR_MAX)] ^ (crc >> CHAR_BIT);

  return crc;
}


/*---------------------------------------------------------------------*/
/*  Glk port game identification data and identification functions     */
/*---------------------------------------------------------------------*/

/*
 * The game's name, suitable for printing out on a status line, or other
 * location where game information is relevant.  It's generated on demand,
 * and may be re-requested when, say, the game changes, perhaps by moving to
 * the next part of a multipart game.
 */
static const char *gln_gameid_game_name = NULL;


/*
 * The following game database is obtained from L9cut's l9data_d.h, and
 * lets us find a game's name from its data CRC.  Entries marked "WANTED" in
 * l9data_d.h, and file commentary, have been removed for brevity, and the
 * file has been reformatted (patchlevel data removed).
 *
 * The version of l9data_d.h used is 050 (22 Oct 2002).
 */
typedef const struct
{
  const gln_uint16 length;    /* Datafile length in bytes */
  const gln_byte checksum;    /* 8-bit checksum, last datafile byte */
  const gln_uint16 crc;       /* 16-bit CRC, L9cut-internal */
  const char * const name;    /* Game title and platform */
} gln_game_table_t;
typedef gln_game_table_t *gln_game_tableref_t;

static gln_game_table_t GLN_GAME_TABLE[] = {
  {0x5323, 0xb7, 0x8af7, "Adventure Quest (Amstrad CPC/Spectrum)"},

  {0x630e, 0x8d, 0x7d7d, "Dungeon Adventure (Amstrad CPC)"},
  {0x630e, 0xbe, 0x3374, "Dungeon Adventure (MSX)"},

  {0x5eb9, 0x30, 0xe99a, "Lords of Time (Amstrad CPC)"},
  {0x5eb9, 0x5d, 0xc098, "Lords of Time (MSX)"},
  {0x5eb9, 0x6e, 0xc689, "Lords of Time (Spectrum)"},

  {0x5fab, 0x5c, 0xa309, "Snowball (Amstrad CPC)"},
  {0x5fab, 0x2f, 0x8aa2, "Snowball (MSX)"},

  {0x60c4, 0x28, 0x0154, "Return to Eden (Amstrad CPC/Commodore 64[v1])"},
  {0x6064, 0x01, 0x5b3c, "Return to Eden (BBC[v1])"},
  {0x6064, 0x95, 0x510c, "Return to Eden (Commodore 64[v2])"},
  {0x6064, 0xda, 0xe610, "Return to Eden (Commodore 64[v2] *corrupt*)"},
  {0x6064, 0xbd, 0x73ec, "Return to Eden (Atari *corrupt*)"},
  {0x6047, 0x6c, 0x17ab, "Return to Eden (BBC[v2])"},
  {0x5ca1, 0x33, 0x1c43, "Return to Eden (Spectrum[v1])"},
  {0x5cb7, 0x64, 0x0790, "Return to Eden (Spectrum[v2])"},
  {0x5cb7, 0xfe, 0x3533, "Return to Eden (MSX)"},

  {0x34b3, 0x20, 0xccda, "Erik the Viking (BBC/Commodore 64)"},
  {0x34b3, 0x53, 0x8f00, "Erik the Viking (Spectrum)"},
  {0x34b3, 0xc7, 0x9058, "Erik the Viking (Amstrad CPC)"},

  {0x63be, 0xd6, 0xcf5d,
   "Emerald Isle (Atari/Commodore 64/Amstrad CPC/Spectrum)"},
  {0x63be, 0x0a, 0x21ed, "Emerald Isle (MSX *corrupt*)"},
  {0x378c, 0x8d, 0x3a21, "Emerald Isle (BBC)"},

  {0x506c, 0xf0, 0xba72, "Red Moon (BBC/Commodore 64/Amstrad CPC/MSX)"},
  {0x505d, 0x32, 0x2dcf, "Red Moon (Spectrum)"},

  {0x772b, 0xcd, 0xa503, "Worm in Paradise (Spectrum 128)"},
  {0x546c, 0xb7, 0x9420, "Worm in Paradise (Spectrum 48)"},
  {0x6d84, 0xf9, 0x49ae, "Worm in Paradise (Commodore 64 *corrupt*)"},
  {0x6d84, 0xc8, 0x943f, "Worm in Paradise (Commodore 64 *fixed*)"},
  {0x6030, 0x47, 0x46ad, "Worm in Paradise (Amstrad CPC)"},
  {0x5828, 0xbd, 0xe7cb, "Worm in Paradise (BBC)"},

  {0x7410, 0x5e, 0x60be, "Price of Magik (Spectrum 128)"},
  {0x5aa4, 0xc1, 0x10a0, "Price of Magik (Spectrum 48[v1])"},
  {0x5aa4, 0xc1, 0xeda4, "Price of Magik (Spectrum 48[v2])"},
  {0x6fc6, 0x14, 0xf9b6, "Price of Magik (Commodore 64)"},
  {0x5aa4, 0xc1, 0xbbf4, "Price of Magik (Amstrad CPC)"},
  {0x5671, 0xbc, 0xff35, "Price of Magik (BBC)"},

  {0x76f4, 0x5e, 0x1fe5, "Colossal Adventure /JoD (Amiga/PC)"},
  {0x76f4, 0x5a, 0xcf4b, "Colossal Adventure /JoD (ST)"},
  {0x6e60, 0x83, 0x18e0, "Adventure Quest /JoD (Amiga/PC)"},
  {0x6e5c, 0xf6, 0xd356, "Adventure Quest /JoD (ST)"},
  {0x6f0c, 0x95, 0x1f64, "Dungeon Adventure /JoD (Amiga/PC/ST)"},

  {0x6f70, 0x40, 0xbd91, "Colossal Adventure /JoD (MSX)"},

  {0x6f6e, 0x78, 0x28cd, "Colossal Adventure /JoD (Spectrum 128)"},
  {0x6970, 0xd6, 0xa820, "Adventure Quest /JoD (Spectrum 128)"},
  {0x6de8, 0x4c, 0xd795, "Dungeon Adventure /JoD (Spectrum 128)"},

  {0x6f4d, 0xcb, 0xe8f2,
   "Colossal Adventure /JoD (Amstrad CPC128[v1]/Spectrum +3)"},
  {0x6f6a, 0xa5, 0x8dd2, "Colossal Adventure /JoD (Amstrad CPC128[v2])"},
  {0x6968, 0x32, 0x0c01, "Adventure Quest /JoD (Amstrad CPC128/Spectrum +3)"},
  {0x6dc0, 0x63, 0x5d95, "Dungeon Adventure /JoD (Amstrad CPC128/Spectrum +3)"},

  {0x5e31, 0x7c, 0xaa54, "Colossal Adventure /JoD (Amstrad CPC64)"},
  {0x5b50, 0x66, 0x1800, "Adventure Quest /JoD (Amstrad CPC64)"},
  {0x58a6, 0x24, 0xb50f, "Dungeon Adventure /JoD (Amstrad CPC64)"},

  {0x6c8e, 0xb6, 0x9be3, "Colossal Adventure /JoD (Commodore 64)"},
  {0x63b6, 0x2e, 0xef38, "Adventure Quest /JoD (Commodore 64)"},
  {0x6bd2, 0x65, 0xa41f, "Dungeon Adventure /JoD (Commodore 64)"},

  {0x5b16, 0x3b, 0xe2aa, "Colossal Adventure /JoD (Atari)"},
  {0x5b58, 0x50, 0x332e, "Adventure Quest /JoD (Atari)"},
  {0x593a, 0x80, 0x7a34, "Dungeon Adventure /JoD (Atari)"},

  {0x5a8e, 0xf2, 0x7cca, "Colossal Adventure /JoD (Spectrum 48)"},
  {0x5ace, 0x11, 0xdc12, "Adventure Quest /JoD (Spectrum 48)"},
  {0x58a3, 0x38, 0x8ce4, "Dungeon Adventure /JoD (Spectrum 48)"},

  {0x7b31, 0x6e, 0x2e2b, "Snowball /SD (Amiga/ST)"},
  {0x7d16, 0xe6, 0x5438, "Return to Eden /SD (Amiga/ST)"},
  {0x7cd9, 0x0c, 0x4df1, "Worm in Paradise /SD (Amiga/ST)"},

  {0x7b2f, 0x70, 0x6955, "Snowball /SD (Mac/PC/Spectrum 128)"},
  {0x7b2f, 0x70, 0x6f6c, "Snowball /SD (Amstrad CPC/Spectrum +3)"},
  {0x7d14, 0xe8, 0xfbab, "Return to Eden /SD (PC)"},
  {0x7cff, 0xf8, 0x6044, "Return to Eden /SD (Amstrad CPC/Spectrum +3)"},
  {0x7cf8, 0x24, 0x9c1c, "Return to Eden /SD (Mac)"},
  {0x7c55, 0x18, 0xdaee, "Return to Eden /SD (Spectrum 128)"},
  {0x7cd7, 0x0e, 0x4feb,
   "Worm in Paradise /SD (Amstrad CPC/Mac/PC/Spectrum 128/Spectrum +3)"},

  {0x7363, 0x65, 0xa0ab, "Snowball /SD (Commodore 64)"},
  {0x772f, 0xca, 0x8602, "Return to Eden /SD (Commodore 64)"},
  {0x788d, 0x72, 0x888a, "Worm in Paradise /SD (Commodore 64)"},

  {0x6bf8, 0x3f, 0xc9f7, "Snowball /SD (Atari)"},
  {0x60f7, 0x68, 0xc2bc, "Return to Eden /SD (Atari)"},
  {0x6161, 0xf3, 0xe6d7, "Worm in Paradise /SD (Atari)"},

  {0x67a3, 0x9d, 0x1d05, "Snowball /SD (Apple ][)"},
  {0x639c, 0x8b, 0x06e2, "Return to Eden /SD (Apple ][)"},
  {0x60dd, 0xf2, 0x5bb8, "Worm in Paradise /SD (Apple ][)"},

  {0x6541, 0x02, 0x2e6c, "Snowball /SD (Spectrum 48)"},
  {0x5f43, 0xca, 0x828c, "Return to Eden /SD (Spectrum 48)"},
  {0x5ebb, 0xf1, 0x4dec, "Worm in Paradise /SD (Spectrum 48)"},

  {0x8333, 0xb7, 0xe2ac, "Adrian Mole I, pt. 1 (Commodore 64)"},
  {0x844d, 0x50, 0x5353, "Adrian Mole I, pt. 2 (Commodore 64)"},
  {0x8251, 0x5f, 0x862a, "Adrian Mole I, pt. 3 (Commodore 64)"},
  {0x7a78, 0x5e, 0x6ea3, "Adrian Mole I, pt. 4 (Commodore 64)"},

  {0x7c6f, 0x0f, 0xba24, "Adrian Mole I, pt. 1 (Amstrad CPC)"},

  {0x72fa, 0x8b, 0x6f12, "Adrian Mole I, pt. 1 (Spectrum)"},
  {0x738e, 0x5b, 0x7e3d, "Adrian Mole I, pt. 2 (Spectrum)"},
  {0x7375, 0xe5, 0x3f3e, "Adrian Mole I, pt. 3 (Spectrum)"},
  {0x78d5, 0xe3, 0xcd7d, "Adrian Mole I, pt. 4 (Spectrum)"},

  {0x3a31, 0xe5, 0x0bdb, "Adrian Mole I, pt. 1 (BBC)"},
  {0x37f1, 0x77, 0xd231, "Adrian Mole I, pt. 2 (BBC)"},
  {0x3900, 0x1c, 0x5d9a, "Adrian Mole I, pt. 3 (BBC)"},
  {0x3910, 0xac, 0x07f9, "Adrian Mole I, pt. 4 (BBC)"},
  {0x3ad6, 0xa7, 0x95d2, "Adrian Mole I, pt. 5 (BBC)"},
  {0x38a5, 0x0f, 0xdefc, "Adrian Mole I, pt. 6 (BBC)"},
  {0x361e, 0x7e, 0xfd9f, "Adrian Mole I, pt. 7 (BBC)"},
  {0x3934, 0x75, 0xe141, "Adrian Mole I, pt. 8 (BBC)"},
  {0x3511, 0xcc, 0xd829, "Adrian Mole I, pt. 9 (BBC)"},
  {0x38dd, 0x31, 0x2534, "Adrian Mole I, pt. 10 (BBC)"},
  {0x39c0, 0x44, 0x89df, "Adrian Mole I, pt. 11 (BBC)"},
  {0x3a12, 0x8f, 0xc2bd, "Adrian Mole I, pt. 12 (BBC)"},

  {0x7931, 0xb9, 0xc51b, "Adrian Mole II, pt. 1 (Commodore 64/Amstrad CPC)"},
  {0x7cdf, 0xa5, 0x43e3, "Adrian Mole II, pt. 2 (Commodore 64/Amstrad CPC)"},
  {0x7a0c, 0x97, 0x4bea, "Adrian Mole II, pt. 3 (Commodore 64/Amstrad CPC)"},
  {0x7883, 0xe2, 0xee0e, "Adrian Mole II, pt. 4 (Commodore 64/Amstrad CPC)"},

  {0x6841, 0x4a, 0x94e7, "Adrian Mole II, pt. 1 (Spectrum)"},
  {0x6bc0, 0x62, 0xab3d, "Adrian Mole II, pt. 2 (Spectrum)"},
  {0x692c, 0x21, 0x2015, "Adrian Mole II, pt. 3 (Spectrum)"},
  {0x670a, 0x94, 0xa2a6, "Adrian Mole II, pt. 4 (Spectrum)"},

  {0x593a, 0xaf, 0x30e9, "Adrian Mole II, pt. 1 (BBC)"},
  {0x57e6, 0x8a, 0xc41a, "Adrian Mole II, pt. 2 (BBC)"},
  {0x5819, 0xcd, 0x1ba0, "Adrian Mole II, pt. 3 (BBC)"},
  {0x579b, 0xad, 0xa723, "Adrian Mole II, pt. 4 (BBC)"},

  {0x765d, 0xcd, 0xfc02, "The Archers, pt. 1 (Commodore 64)"},
  {0x6e58, 0x07, 0xbffc, "The Archers, pt. 2 (Commodore 64)"},
  {0x7e98, 0x6a, 0x95e5, "The Archers, pt. 3 (Commodore 64)"},
  {0x81e2, 0xd5, 0xb278, "The Archers, pt. 4 (Commodore 64)"},

  {0x6ce5, 0x58, 0x46de, "The Archers, pt. 1 (Spectrum)"},
  {0x68da, 0xc1, 0x3b8e, "The Archers, pt. 2 (Spectrum)"},
  {0x6c67, 0x9a, 0x9a6a, "The Archers, pt. 3 (Spectrum)"},
  {0x6d91, 0xb9, 0x12a7, "The Archers, pt. 4 (Spectrum)"},

  {0x5834, 0x42, 0xcc9d, "The Archers, pt. 1 (BBC)"},
  {0x56dd, 0x51, 0xe582, "The Archers, pt. 2 (BBC)"},
  {0x5801, 0x53, 0xf2ef, "The Archers, pt. 3 (BBC)"},
  {0x54a4, 0x01, 0xc0ab, "The Archers, pt. 4 (BBC)"},

  {0x579e, 0x97, 0x9faa, "Lords of Time /T&M GD (BBC)"},
  {0x5500, 0x50, 0xca75, "Red Moon /T&M GD (BBC)"},
  {0x579a, 0x2a, 0x9373, "Price of Magik /T&M GD (BBC)"},

  {0x4fd2, 0x9d, 0x799a, "Lancelot, pt. 1 GD (BBC)"},
  {0x4dac, 0xa8, 0x86ed, "Lancelot, pt. 2 GD (BBC)"},
  {0x4f96, 0x22, 0x30f8, "Lancelot, pt. 3 GD (BBC)"},

  {0x55ce, 0xa1, 0xba12, "Scapeghost, pt. 1 GD (BBC)"},
  {0x54a6, 0xa9, 0xc9f3, "Scapeghost, pt. 2 GD (BBC)"},
  {0x51bc, 0xe3, 0x89c3, "Scapeghost, pt. 3 GD (BBC)"},

  {0x46ec, 0x64, 0x2300, "Knight Orc, pt. 1 GD (Amstrad CPC/Spectrum +3)"},
  {0x6140, 0x18, 0x4f66, "Knight Orc, pt. 2 GD (Amstrad CPC/Spectrum +3)"},
  {0x640e, 0xc1, 0xfc69, "Knight Orc, pt. 3 GD (Amstrad CPC/Spectrum +3)"},

  {0x5ff0, 0xf8, 0x3a13, "Gnome Ranger, pt. 1 GD (Amstrad CPC/Spectrum +3)"},
  {0x6024, 0x01, 0xaaa9, "Gnome Ranger, pt. 2 GD (Amstrad CPC/Spectrum +3)"},
  {0x6036, 0x3d, 0x6c6c, "Gnome Ranger, pt. 3 GD (Amstrad CPC/Spectrum +3)"},

  {0x69fe, 0x56, 0xecfb, "Lords of Time /T&M GD (Amstrad CPC/Spectrum +3)"},
  {0x6888, 0x8d, 0x7f6a, "Red Moon /T&M GD (Amstrad CPC/Spectrum +3)"},
  {0x5a50, 0xa9, 0xa5fa, "Price of Magik /T&M GD (Amstrad CPC/Spectrum +3)"},

  {0x5c7a, 0x44, 0x460e, "Lancelot, pt. 1 GD (Amstrad CPC/Spectrum +3)"},
  {0x53a2, 0x1e, 0x2fae, "Lancelot, pt. 2 GD (Amstrad CPC/Spectrum +3)"},
  {0x5914, 0x22, 0x4a31, "Lancelot, pt. 3 GD (Amstrad CPC/Spectrum +3)"},

  {0x5a38, 0xf7, 0x876e, "Ingrid's Back, pt. 1 GD (Amstrad CPC/Spectrum +3)"},
  {0x531a, 0xed, 0xcf3f, "Ingrid's Back, pt. 2 GD (Amstrad CPC/Spectrum +3)"},
  {0x57e4, 0x19, 0xb354, "Ingrid's Back, pt. 3 GD (Amstrad CPC/Spectrum +3)"},

  {0x5cbc, 0xa5, 0x0dbe, "Scapeghost, pt. 1 GD (Amstrad CPC/Spectrum +3)"},
  {0x5932, 0x4e, 0xb2f5, "Scapeghost, pt. 2 GD (Amstrad CPC/Spectrum +3)"},
  {0x5860, 0x95, 0x3227, "Scapeghost, pt. 3 GD (Amstrad CPC/Spectrum +3)"},

  {0x74e0, 0x92, 0x885e, "Knight Orc, pt. 1 GD (Spectrum 128)"},
  {0x6dbc, 0x97, 0x6f55, "Knight Orc, pt. 2 GD (Spectrum 128)"},
  {0x7402, 0x07, 0x385f, "Knight Orc, pt. 3 GD (Spectrum 128)"},

  {0x52aa, 0xdf, 0x7b5b, "Gnome Ranger, pt. 1 GD (Spectrum 128)"},
  {0x6ffa, 0xdb, 0xdde2, "Gnome Ranger, pt. 2 GD (Spectrum 128)"},
  {0x723a, 0x69, 0x039b, "Gnome Ranger, pt. 3 GD (Spectrum 128)"},

  {0x6f1e, 0xda, 0x2ce0, "Lords of Time /T&M GD (Spectrum 128)"},
  {0x6da0, 0xb8, 0x3802, "Red Moon /T&M GD (Spectrum 128)"},
  {0x6108, 0xdd, 0xefe7, "Price of Magik /T&M GD (Spectrum 128)"},

  {0x768c, 0xe8, 0x8fc6, "Lancelot, pt. 1 GD (Spectrum 128)"},
  {0x76b0, 0x1d, 0x0fcd, "Lancelot, pt. 2 GD (Spectrum 128)"},
  {0x765e, 0x4f, 0x3b73, "Lancelot, pt. 3 GD (Spectrum 128)"},

  {0x76a0, 0x3a, 0xb803, "Ingrid's Back, pt. 1 GD (Spectrum 128)"},
  {0x7674, 0x0b, 0xe92f, "Ingrid's Back, pt. 2 GD (Spectrum 128)"},
  {0x765e, 0xba, 0x086d, "Ingrid's Back, pt. 3 GD (Spectrum 128)"},

  {0x762e, 0x82, 0x8848, "Scapeghost, pt. 1 GD (Spectrum 128)"},
  {0x5bd6, 0x35, 0x79ef, "Scapeghost, pt. 2 GD (Spectrum 128)"},
  {0x6fa8, 0xa4, 0x62c2, "Scapeghost, pt. 3 GD (Spectrum 128)"},

  {0xbb93, 0x36, 0x6a05, "Knight Orc, pt. 1 (Amiga)"},
  {0xbb6e, 0xad, 0x4d40, "Knight Orc, pt. 1 (ST)"},
  {0xc58e, 0x4a, 0x4e9d, "Knight Orc, pt. 2 (Amiga/ST)"},
  {0xcb9a, 0x0f, 0x0804, "Knight Orc, pt. 3 (Amiga/ST)"},

  {0xbb6e, 0xa6, 0x9753, "Knight Orc, pt. 1 (PC)"},
  {0xc58e, 0x43, 0xe9ce, "Knight Orc, pt. 2 (PC)"},
  {0xcb9a, 0x08, 0x6c36, "Knight Orc, pt. 3 (PC)"},

  {0x898a, 0x43, 0xfc8b, "Knight Orc, pt. 1 (Apple ][)"},
  {0x8b9f, 0x61, 0x7288, "Knight Orc, pt. 2 (Apple ][)"},
  {0x8af9, 0x61, 0x7542, "Knight Orc, pt. 3 (Apple ][)"},

  {0x8970, 0x6b, 0x3c7b, "Knight Orc, pt. 1 (Commodore 64 Gfx)"},
  {0x8b90, 0x4e, 0x098c, "Knight Orc, pt. 2 (Commodore 64 Gfx)"},
  {0x8aea, 0x4e, 0xca54, "Knight Orc, pt. 3 (Commodore 64 Gfx)"},

  {0x86d0, 0xb7, 0xadbd, "Knight Orc, pt. 1 (Spectrum 48)"},
  {0x8885, 0x22, 0xe293, "Knight Orc, pt. 2 (Spectrum 48)"},
  {0x87e5, 0x0e, 0xdc33, "Knight Orc, pt. 3 (Spectrum 48)"},

  {0xb1a9, 0x80, 0x5fb7, "Gnome Ranger, pt. 1 (Amiga/ST)"},
  {0xab9d, 0x31, 0xbe6d, "Gnome Ranger, pt. 2 (Amiga/ST)"},
  {0xae28, 0x87, 0xb6b6, "Gnome Ranger, pt. 3 (Amiga/ST)"},

  {0xb0ec, 0xc2, 0x0053, "Gnome Ranger, pt. 1 (ST[v1])"},
  {0xaf82, 0x83, 0x19f7, "Gnome Ranger, pt. 2 (ST[v1])"},

  {0xb1aa, 0xad, 0xaf47, "Gnome Ranger, pt. 1 (PC)"},
  {0xb19e, 0x92, 0x8f96, "Gnome Ranger, pt. 1 (ST[v2])"},
  {0xab8b, 0xbf, 0x31e6, "Gnome Ranger, pt. 2 (PC/ST[v2])"},
  {0xae16, 0x81, 0x8741, "Gnome Ranger, pt. 3 (PC/ST[v2])"},

  {0xad41, 0xa8, 0x42c5, "Gnome Ranger, pt. 1 (Commodore 64 TO)"},
  {0xa735, 0xf7, 0x2e08, "Gnome Ranger, pt. 2 (Commodore 64 TO)"},
  {0xa9c0, 0x9e, 0x0d70, "Gnome Ranger, pt. 3 (Commodore 64 TO)"},

  {0x908e, 0x0d, 0x58a7, "Gnome Ranger, pt. 1 (Commodore 64 Gfx)"},
  {0x8f6f, 0x0a, 0x411a, "Gnome Ranger, pt. 2 (Commodore 64 Gfx)"},
  {0x9060, 0xbb, 0xe75d, "Gnome Ranger, pt. 3 (Commodore 64 Gfx)"},

  {0x8aab, 0xc0, 0xde5f, "Gnome Ranger, pt. 1 (Spectrum 48)"},
  {0x8ac8, 0x9a, 0xc89b, "Gnome Ranger, pt. 2 (Spectrum 48)"},
  {0x8a93, 0x4f, 0x10cc, "Gnome Ranger, pt. 3 (Spectrum 48)"},

  {0xb57c, 0x44, 0x7779, "Lords of Time /T&M (PC)"},
  {0xa69e, 0x6c, 0xb268, "Red Moon /T&M (PC)"},
  {0xbac7, 0x7f, 0xddb2, "Price of Magik /T&M (PC)"},

  {0xb579, 0x89, 0x3e89, "Lords of Time /T&M (ST)"},
  {0xa698, 0x41, 0xcaca, "Red Moon /T&M (ST)"},
  {0xbac4, 0x80, 0xa750, "Price of Magik /T&M (ST)"},

  {0xb576, 0x2a, 0x7239, "Lords of Time /T&M (Amiga)"},
  {0xa692, 0xd1, 0x6a99, "Red Moon /T&M (Amiga)"},
  {0xbaca, 0x3a, 0x221b, "Price of Magik /T&M (Amiga)"},

  {0xb563, 0x6a, 0x0c5c, "Lords of Time /T&M (Mac)"},
  {0xa67c, 0xb8, 0xff41, "Red Moon /T&M (Mac)"},
  {0xbab2, 0x87, 0x09f5, "Price of Magik /T&M (Mac)"},

  {0xb38c, 0x37, 0x9f8e, "Lords of Time /T&M (Commodore 64 TO)"},
  {0xa4e2, 0xa6, 0x016d, "Red Moon /T&M (Commodore 64 TO)"},
  {0xb451, 0xa8, 0x2682, "Price of Magik /T&M (Commodore 64 TO)"},

  {0x9070, 0x43, 0x45d4, "Lords of Time /T&M (Commodore 64 Gfx)"},
  {0x903f, 0x6b, 0x603e, "Red Moon /T&M (Commodore 64 Gfx)"},
  {0x8f51, 0xb2, 0x6c9a, "Price of Magik /T&M (Commodore 64 Gfx)"},

  {0x8950, 0xa1, 0xbb16, "Lords of Time /T&M (Spectrum 48)"},
  {0x8813, 0x11, 0x22de, "Red Moon /T&M (Spectrum 48)"},
  {0x8a60, 0x2a, 0x29ed, "Price of Magik /T&M (Spectrum 48)"},

  {0xb260, 0xe5, 0xc5b2, "Lords of Time /T&M (PC/ST *USA*)"},
  {0xa3a4, 0xdf, 0x6732, "Red Moon /T&M (PC/ST *USA*)"},
  {0xb7a0, 0x7e, 0x2226, "Price of Magik /T&M (PC/ST *USA*)"},

  {0xb257, 0xf8, 0xfbd5, "Lords of Time /T&M (Amiga *USA*)"},
  {0xa398, 0x82, 0xd031, "Red Moon /T&M (Amiga *USA*)"},
  {0xb797, 0x1f, 0x84a9, "Price of Magik /T&M (Amiga *USA*)"},

  {0x8d78, 0x3a, 0xba6e, "Lords of Time /T&M (Commodore 64 Gfx *USA*)"},
  {0x8d56, 0xd3, 0x146a, "Red Moon /T&M (Commodore 64 Gfx *USA*)"},
  {0x8c46, 0xf0, 0xcaf6, "Price of Magik /T&M (Commodore 64 Gfx *USA*)"},

  {0xc0cf, 0x4e, 0xb7fa, "Lancelot, pt. 1 (Amiga/PC/ST)"},
  {0xd5e9, 0x6a, 0x4192, "Lancelot, pt. 2 (Amiga/PC/ST)"},
  {0xbb8f, 0x1a, 0x7487, "Lancelot, pt. 3 (Amiga/PC/ST)"},

  {0xc0bd, 0x57, 0x6ef1, "Lancelot, pt. 1 (Mac)"},
  {0xd5d7, 0x99, 0x770b, "Lancelot, pt. 2 (Mac)"},
  {0xbb7d, 0x17, 0xbc42, "Lancelot, pt. 3 (Mac)"},

  {0xb4c9, 0x94, 0xd784, "Lancelot, pt. 1 (Commodore 64 TO)"},
  {0xb729, 0x51, 0x8ee5, "Lancelot, pt. 2 (Commodore 64 TO)"},
  {0xb702, 0xe4, 0x1809, "Lancelot, pt. 3 (Commodore 64 TO)"},

  {0x8feb, 0xba, 0xa800, "Lancelot, pt. 1 (Commodore 64 Gfx)"},
  {0x8f6b, 0xfa, 0x0f7e, "Lancelot, pt. 2 (Commodore 64 Gfx)"},
  {0x8f71, 0x2f, 0x0ddc, "Lancelot, pt. 3 (Commodore 64 Gfx)"},

  {0x8ade, 0xf2, 0xfffb, "Lancelot, pt. 1 (Spectrum 48)"},
  {0x8b0e, 0xfb, 0x0bab, "Lancelot, pt. 2 (Spectrum 48)"},
  {0x8ab3, 0xc1, 0xcb62, "Lancelot, pt. 3 (Spectrum 48)"},

  {0xbba4, 0x94, 0x0871, "Lancelot, pt. 1 (Amiga/PC *USA*)"},
  {0xd0c0, 0x56, 0x8c48, "Lancelot, pt. 2 (Amiga/PC *USA*)"},
  {0xb6ac, 0xc6, 0xaea0, "Lancelot, pt. 3 (Amiga/PC *USA*)"},

  {0x8afc, 0x07, 0x8321, "Lancelot, pt. 1 (Commodore 64 Gfx *USA*)"},
  {0x8aec, 0x13, 0x6791, "Lancelot, pt. 2 (Commodore 64 Gfx *USA*)"},
  {0x8aba, 0x0d, 0x5602, "Lancelot, pt. 3 (Commodore 64 Gfx *USA*)"},

  {0xd19b, 0xad, 0x306d, "Ingrid's Back, pt. 1 (PC)"},
  {0xc5a5, 0xfe, 0x3c98, "Ingrid's Back, pt. 2 (PC)"},
  {0xd7ae, 0x9e, 0x1878, "Ingrid's Back, pt. 3 (PC)"},

  {0xd188, 0x13, 0xdc60, "Ingrid's Back, pt. 1 (Amiga)"},
  {0xc594, 0x03, 0xea95, "Ingrid's Back, pt. 2 (Amiga)"},
  {0xd79f, 0xb5, 0x1661, "Ingrid's Back, pt. 3 (Amiga)"},

  {0xd183, 0x83, 0xef72, "Ingrid's Back, pt. 1 (ST)"},
  {0xc58f, 0x65, 0xf337, "Ingrid's Back, pt. 2 (ST)"},
  {0xd79a, 0x57, 0x49c5, "Ingrid's Back, pt. 3 (ST)"},

  {0xb770, 0x03, 0x9a03, "Ingrid's Back, pt. 1 (Commodore 64 TO)"},
  {0xb741, 0xb6, 0x2aa5, "Ingrid's Back, pt. 2 (Commodore 64 TO)"},
  {0xb791, 0xa1, 0xd065, "Ingrid's Back, pt. 3 (Commodore 64 TO)"},

  {0x9089, 0xce, 0xc5e2, "Ingrid's Back, pt. 1 (Commodore 64 Gfx)"},
  {0x908d, 0x80, 0x30c7, "Ingrid's Back, pt. 2 (Commodore 64 Gfx)"},
  {0x909e, 0x9f, 0xdecc, "Ingrid's Back, pt. 3 (Commodore 64 Gfx)"},

  {0x8ab7, 0x68, 0xee57, "Ingrid's Back, pt. 1 (Spectrum 48)"},
  {0x8b1e, 0x84, 0x2538, "Ingrid's Back, pt. 2 (Spectrum 48)"},
  {0x8b1c, 0xa8, 0x9262, "Ingrid's Back, pt. 3 (Spectrum 48)"},

  {0xbeab, 0x2d, 0x94d9, "Scapeghost, pt. 1 (Amiga)"},
  {0xc132, 0x14, 0x7adc, "Scapeghost, pt. 1 (Amiga *bak*)"},
  {0xbe94, 0xcc, 0x04b8, "Scapeghost, pt. 1 (PC/ST)"},
  {0x99bd, 0x65, 0x032e, "Scapeghost, pt. 2 (Amiga/PC/ST)"},
  {0xbcb6, 0x7a, 0x7d4f, "Scapeghost, pt. 3 (Amiga/PC/ST)"},

  {0x9058, 0xcf, 0x9748, "Scapeghost, pt. 1 (Commodore 64 Gfx)"},
  {0x8f43, 0xc9, 0xeefd, "Scapeghost, pt. 2 (Commodore 64 Gfx)"},
  {0x90ac, 0x68, 0xb4a8, "Scapeghost, pt. 3 (Commodore 64 Gfx)"},

  {0x8a21, 0xf4, 0xd9e4, "Scapeghost, pt. 1 (Spectrum 48)"},
  {0x8a12, 0xe3, 0xc2ff, "Scapeghost, pt. 2 (Spectrum 48)"},
  {0x8a16, 0xcc, 0x4f3b, "Scapeghost, pt. 3 (Spectrum 48)"},

  {0x3ebb, 0x00, 0xf6dc, "Champion of the Raj (English) 1/2 GD (Amiga)"},
  {0x0fd8, 0x00, 0xf250, "Champion of the Raj (English) 2/2 GD (Amiga)"},

  {0x3e8f, 0x00, 0x5599, "Champion of the Raj (English) 1/2 GD (ST)"},

  {0x3e4f, 0x00, 0xb202, "Champion of the Raj (English) 1/2 GD (PC)"},
  {0x14a3, 0x00, 0xa288, "Champion of the Raj (English) 2/2 GD (PC)"},

  {0x1929, 0x00, 0xd4b2, "Champion of the Raj (demo), 1/2 GD (ST)"},
  {0x40e0, 0x02, 0x080d, "Champion of the Raj (demo), 2/2 GD (ST)"},

  {0x4872, 0x00, 0x9515, "Champion of the Raj (German) 1/2 GD (Amiga)"},
  {0x11f5, 0x00, 0xbf39, "Champion of the Raj (German) 2/2 GD (Amiga)"},

  {0x4846, 0x00, 0xd9c1, "Champion of the Raj (German) 1/2 GD (ST)"},
  {0x11f5, 0x00, 0x7aa4, "Champion of the Raj (German) 2/2 GD (ST)"},

  {0x110f, 0x00, 0x4b57, "Champion of the Raj (French) 2/2 GD (ST)"},

  {0x0000, 0x00, 0x0000, NULL}
};


/*
 * The following patch database is obtained from L9cut's l9data_p.h, and
 * allows CRCs from patched games to be translated into original CRCs for
 * lookup in the game database above.  Some file commentary has been removed
 * for brevity, and unused patch fields deleted.
 *
 * The version of l9data_p.h used is 012 (22 May 2001).
 */
typedef const struct
{
  const gln_uint16 length;        /* Datafile length in bytes */
  const gln_byte orig_checksum;   /* 8-bit checksum, last datafile byte */
  const gln_uint16 orig_crc;      /* 16-bit CRC, L9cut-internal */
  const gln_byte patch_checksum;  /* 8-bit checksum, last datafile byte */
  const gln_uint16 patch_crc;     /* 16-bit CRC, L9cut-internal */
} gln_patch_table_t;
typedef gln_patch_table_t *gln_patch_tableref_t;

static gln_patch_table_t GLN_PATCH_TABLE[] = {
  /* Price of Magik (Spectrum128) */
  {0x7410, 0x5e, 0x60be, 0x70, 0x6cef},

  /* Price of Magik (Commodore 64) */
  {0x6fc6, 0x14, 0xf9b6, 0x26, 0x3326},

  /* Price of Magik (Spectrum48) */
  {0x5aa4, 0xc1, 0xeda4, 0xd3, 0xed35},
  {0x5aa4, 0xc1, 0xeda4, 0xc1, 0x8a65},

  /* Colossal Adventure /JoD (Amiga/PC) */
  {0x76f4, 0x5e, 0x1fe5, 0xea, 0x1305},
  {0x76f4, 0x5e, 0x1fe5, 0xb5, 0x901f},
  {0x76f4, 0x5e, 0x1fe5, 0x5e, 0x6ea1},

  /* Colossal Adventure /JoD (ST) */
  {0x76f4, 0x5a, 0xcf4b, 0xe6, 0x012a},
  {0x76f4, 0x5a, 0xcf4b, 0xb1, 0x40b1},

  /* Adventure Quest /JoD (Amiga/PC) */
  {0x6e60, 0x83, 0x18e0, 0x4c, 0xcfb0},
  {0x6e60, 0x83, 0x18e0, 0xfa, 0x9b3b},
  {0x6e60, 0x83, 0x18e0, 0x83, 0x303d},

  /* Adventure Quest /JoD (ST) */
  {0x6e5c, 0xf6, 0xd356, 0xbf, 0xede7},
  {0x6e5c, 0xf6, 0xd356, 0x6d, 0x662d},

  /* Dungeon Adventure /JoD (Amiga/PC/ST) */
  {0x6f0c, 0x95, 0x1f64, 0x6d, 0x2443},
  {0x6f0c, 0x95, 0x1f64, 0x0c, 0x6066},
  {0x6f0c, 0x95, 0x1f64, 0x96, 0xdaca},
  {0x6f0c, 0x95, 0x1f64, 0x95, 0x848d},

  /* Colossal Adventure /JoD (Spectrum128) */
  {0x6f6e, 0x78, 0x28cd, 0xf8, 0xda5f},
  {0x6f6e, 0x78, 0x28cd, 0x77, 0x5b4e},

  /* Adventure Quest /JoD (Spectrum128) */
  {0x6970, 0xd6, 0xa820, 0x3b, 0x1870},
  {0x6970, 0xd6, 0xa820, 0xd5, 0x13c4},

  /* Dungeon Adventure /JoD (Spectrum128) */
  {0x6de8, 0x4c, 0xd795, 0xa2, 0x3eea},
  {0x6de8, 0x4c, 0xd795, 0x4b, 0xad30},

  /* Colossal Adventure /JoD (Amstrad CPC) */
  {0x6f4d, 0xcb, 0xe8f2, 0x4b, 0xb384},
  {0x6f4d, 0xcb, 0xe8f2, 0xca, 0x96e7},

  /* Adventure Quest /JoD (Amstrad CPC) */
  {0x6968, 0x32, 0x0c01, 0x97, 0xdded},
  {0x6968, 0x32, 0x0c01, 0x31, 0xe8c2},

  /* Dungeon Adventure /JoD (Amstrad CPC) */
  {0x6dc0, 0x63, 0x5d95, 0xb9, 0xc963},
  {0x6dc0, 0x63, 0x5d95, 0x62, 0x79f7},

  /* Colossal Adventure /JoD (Commodore 64) */
  {0x6c8e, 0xb6, 0x9be3, 0x36, 0x6971},
  {0x6c8e, 0xb6, 0x9be3, 0xb5, 0xeba0},

  /* Adventure Quest /JoD (Commodore 64) */
  {0x63b6, 0x2e, 0xef38, 0x93, 0x4e68},
  {0x63b6, 0x2e, 0xef38, 0x2d, 0x54dc},

  /* Dungeon Adventure /JoD (Commodore 64) */
  {0x6bd2, 0x65, 0xa41f, 0xbb, 0x4260},
  {0x6bd2, 0x65, 0xa41f, 0x64, 0xdf5a},

  /* Colossal Adventure /JoD (Spectrum48) */
  {0x5a8e, 0xf2, 0x7cca, 0x72, 0x8e58},
  {0x5a8e, 0xf2, 0x7cca, 0xf1, 0x0c89},
  {0x5a8e, 0xf2, 0x7cca, 0xf2, 0x2c96},

  /* Adventure Quest /JoD (Spectrum48) */
  {0x5ace, 0x11, 0xdc12, 0x76, 0x8663},
  {0x5ace, 0x11, 0xdc12, 0x10, 0xa757},
  {0x5ace, 0x11, 0xdc12, 0x11, 0xf118},

  /* Dungeon Adventure /JoD (Spectrum48) */
  {0x58a3, 0x38, 0x8ce4, 0x8e, 0xb61a},
  {0x58a3, 0x38, 0x8ce4, 0x37, 0x34c0},
  {0x58a3, 0x38, 0x8ce4, 0x38, 0xa1ee},

  /* Snowball /SD (Amiga/ST) */
  {0x7b31, 0x6e, 0x2e2b, 0xe5, 0x6017},

  /* Return to Eden /SD (Amiga/ST) */
  {0x7d16, 0xe6, 0x5438, 0x5d, 0xc770},

  /* Worm in Paradise /SD (Amiga/ST) */
  {0x7cd9, 0x0c, 0x4df1, 0x83, 0xe997},

  /* Snowball /SD (PC/Spectrum128) */
  {0x7b2f, 0x70, 0x6955, 0xe7, 0x0af4},
  {0x7b2f, 0x70, 0x6955, 0x70, 0x1179},

  /* Return to Eden /SD (PC) */
  {0x7d14, 0xe8, 0xfbab, 0x5f, 0xeab9},
  {0x7d14, 0xe8, 0xfbab, 0xe8, 0xe216},

  /* Return to Eden /SD (Amstrad CPC) */
  {0x7cff, 0xf8, 0x6044, 0x6f, 0xbb57},

  /* Return to Eden /SD (Spectrum128) */
  {0x7c55, 0x18, 0xdaee, 0x8f, 0x01fd},

  /* Worm in Paradise /SD (Amstrad CPC/PC/Spectrum128) */
  {0x7cd7, 0x0e, 0x4feb, 0x85, 0x4eae},
  {0x7cd7, 0x0e, 0x4feb, 0x0e, 0xb02c},

  /* Snowball /SD (Commodore 64) */
  {0x7363, 0x65, 0xa0ab, 0xdc, 0xca6a},

  /* Return to Eden /SD (Commodore 64) */
  {0x772f, 0xca, 0x8602, 0x41, 0x9bd0},

  /* Worm in Paradise /SD (Commodore 64) */
  {0x788d, 0x72, 0x888a, 0xe9, 0x4cce},

  /* Snowball /SD (Atari) */
  {0x6bf8, 0x3f, 0xc9f7, 0x96, 0x1908},

  /* Return to Eden /SD (Atari) */
  {0x60f7, 0x68, 0xc2bc, 0xdf, 0xd3ae},

  /* Worm in Paradise /SD (Atari) */
  {0x6161, 0xf3, 0xe6d7, 0x6a, 0xe232},

  /* Snowball /SD (Spectrum48) */
  {0x6541, 0x02, 0x2e6c, 0x79, 0xb80c},
  {0x6541, 0x02, 0x2e6c, 0x02, 0x028a},

  /* Return to Eden /SD (Spectrum48) */
  {0x5f43, 0xca, 0x828c, 0x41, 0x9f5e},
  {0x5f43, 0xca, 0x828c, 0xca, 0x6e1b},

  /* Worm in Paradise /SD (Spectrum48) */
  {0x5ebb, 0xf1, 0x4dec, 0x68, 0x4909},
  {0x5ebb, 0xf1, 0x4dec, 0xf1, 0xcc1a},

  /* Knight Orc, pt. 1 (Amiga) */
  {0xbb93, 0x36, 0x6a05, 0xad, 0xe52d},

  /* Knight Orc, pt. 1 (ST) */
  {0xbb6e, 0xad, 0x4d40, 0x24, 0x3bcd},

  /* Knight Orc, pt. 2 (Amiga/ST) */
  {0xc58e, 0x4a, 0x4e9d, 0xc1, 0xe2bf},

  /* Knight Orc, pt. 3 (Amiga/ST) */
  {0xcb9a, 0x0f, 0x0804, 0x86, 0x6487},

  /* Knight Orc, pt. 1 (PC) */
  {0xbb6e, 0xa6, 0x9753, 0x1d, 0x2e7f},
  {0xbb6e, 0xa6, 0x9753, 0xa6, 0x001d},

  /* Knight Orc, pt. 2 (PC) */
  {0xc58e, 0x43, 0xe9ce, 0xba, 0x5e4c},
  {0xc58e, 0x43, 0xe9ce, 0x43, 0xa8f0},

  /* Knight Orc, pt. 3 (PC) */
  {0xcb9a, 0x08, 0x6c36, 0x7f, 0xf0d4},
  {0xcb9a, 0x08, 0x6c36, 0x08, 0x2d08},

  /* Knight Orc, pt. 1 (Commodore 64 Gfx) */
  {0x8970, 0x6b, 0x3c7b, 0xe2, 0xb6f3},

  /* Knight Orc, pt. 1 (Spectrum48) */
  {0x86d0, 0xb7, 0xadbd, 0x2e, 0x43e1},

  /* Gnome Ranger, pt. 1 (Amiga/ST) */
  {0xb1a9, 0x80, 0x5fb7, 0xf7, 0x5c6c},

  /* Gnome Ranger, pt. 2 (Amiga/ST) */
  {0xab9d, 0x31, 0xbe6d, 0xa8, 0xcb96},

  /* Gnome Ranger, pt. 3 (Amiga/ST) */
  {0xae28, 0x87, 0xb6b6, 0xfe, 0x760c},

  /* Gnome Ranger, pt. 1 (PC) */
  {0xb1aa, 0xad, 0xaf47, 0x24, 0x5cfd},
  {0xb1aa, 0xad, 0xaf47, 0xad, 0xe0ed},

  /* Gnome Ranger, pt. 1 (ST-var) */
  {0xb19e, 0x92, 0x8f96, 0x09, 0x798c},

  /* Gnome Ranger, pt. 2 (PC/ST-var) */
  {0xab8b, 0xbf, 0x31e6, 0x36, 0x811c},
  {0xab8b, 0xbf, 0x31e6, 0xbf, 0x8ff3},

  /* Gnome Ranger, pt. 3 (PC/ST-var) */
  {0xae16, 0x81, 0x8741, 0xf8, 0x47fb},
  {0xae16, 0x81, 0x8741, 0x81, 0xc8eb},

  /* Gnome Ranger, pt. 1 (Commodore 64 TO) */
  {0xad41, 0xa8, 0x42c5, 0x1f, 0x7d1e},

  /* Gnome Ranger, pt. 2 (Commodore 64 TO) */
  {0xa735, 0xf7, 0x2e08, 0x6e, 0x780e},

  /* Gnome Ranger, pt. 3 (Commodore 64 TO) */
  {0xa9c0, 0x9e, 0x0d70, 0x15, 0x3e6b},

  /* Gnome Ranger, pt. 1 (Commodore 64 Gfx) */
  {0x908e, 0x0d, 0x58a7, 0x84, 0xab1d},

  /* Gnome Ranger, pt. 2 (Commodore 64 Gfx) */
  {0x8f6f, 0x0a, 0x411a, 0x81, 0x12bc},

  /* Gnome Ranger, pt. 3 (Commodore 64 Gfx) */
  {0x9060, 0xbb, 0xe75d, 0x32, 0x14e7},

  /* Lords of Time /T&M (PC) */
  {0xb57c, 0x44, 0x7779, 0xbb, 0x31a6},
  {0xb57c, 0x44, 0x7779, 0x44, 0xea72},

  /* Red Moon /T&M (PC) */
  {0xa69e, 0x6c, 0xb268, 0xe3, 0x4cef},
  {0xa69e, 0x6c, 0xb268, 0x6c, 0x3799},

  /* Price of Magik /T&M (PC) */
  {0xbac7, 0x7f, 0xddb2, 0xf6, 0x6ab3},
  {0xbac7, 0x7f, 0xddb2, 0x7f, 0x905c},

  /* Lords of Time /T&M (ST) */
  {0xb579, 0x89, 0x3e89, 0x00, 0xa2b7},

  /* Red Moon /T&M (ST) */
  {0xa698, 0x41, 0xcaca, 0xb8, 0xeeac},

  /* Price of Magik /T&M (ST) */
  {0xbac4, 0x80, 0xa750, 0xf7, 0xe030},

  /* Lords of Time /T&M (Amiga) */
  {0xb576, 0x2a, 0x7239, 0xa1, 0x2ea6},

  /* Red Moon /T&M (Amiga) */
  {0xa692, 0xd1, 0x6a99, 0x48, 0x50ff},

  /* Price of Magik /T&M (Amiga) */
  {0xbaca, 0x3a, 0x221b, 0xb1, 0x55bb},

  /* Lords of Time /T&M (Commodore 64 TO) */
  {0xb38c, 0x37, 0x9f8e, 0xae, 0xc6b1},

  /* Red Moon /T&M (Commodore 64 TO) */
  {0xa4e2, 0xa6, 0x016d, 0x1d, 0x31ab},

  /* Price of Magik /T&M (Commodore 64 TO) */
  {0xb451, 0xa8, 0x2682, 0x1f, 0x5de2},

  /* Lords of Time /T&M (Commodore 64 Gfx) */
  {0x9070, 0x43, 0x45d4, 0xba, 0x02eb},

  /* Red Moon /T&M (Commodore 64 Gfx) */
  {0x903f, 0x6b, 0x603e, 0xe2, 0x9f59},

  /* Price of Magik /T&M (Commodore 64 Gfx) */
  {0x8f51, 0xb2, 0x6c9a, 0x29, 0xde3b},

  /* Lords of Time /T&M (Spectrum48) */
  {0x8950, 0xa1, 0xbb16, 0x18, 0x2828},
  {0x8950, 0xa1, 0xbb16, 0xa1, 0x1ea2},

  /* Red Moon /T&M (Spectrum48) */
  {0x8813, 0x11, 0x22de, 0x88, 0x18b8},
  {0x8813, 0x11, 0x22de, 0x11, 0xd0cd},

  /* Price of Magik /T&M (Spectrum48) */
  {0x8a60, 0x2a, 0x29ed, 0xa1, 0x5e4d},

  /* Lancelot, pt. 1 (Amiga/PC/ST) */
  {0xc0cf, 0x4e, 0xb7fa, 0xc5, 0x4400},

  /* Lancelot, pt. 2 (Amiga/PC/ST) */
  {0xd5e9, 0x6a, 0x4192, 0xe1, 0x3b1e},

  /* Lancelot, pt. 3 (Amiga/PC/ST) */
  {0xbb8f, 0x1a, 0x7487, 0x91, 0x877d},

  /* Lancelot, pt. 1 (Commodore 64 TO) */
  {0xb4c9, 0x94, 0xd784, 0x0b, 0x203e},

  /* Lancelot, pt. 2 (Commodore 64 TO) */
  {0xb729, 0x51, 0x8ee5, 0xc8, 0xf1c9},

  /* Lancelot, pt. 3 (Commodore 64 TO) */
  {0xb702, 0xe4, 0x1809, 0x5b, 0x25b2},

  /* Lancelot, pt. 1 (Commodore 64 Gfx) */
  {0x8feb, 0xba, 0xa800, 0x31, 0x5bfa},

  /* Lancelot, pt. 2 (Commodore 64 Gfx) */
  {0x8f6b, 0xfa, 0x0f7e, 0x71, 0x75f2},

  /* Lancelot, pt. 3 (Commodore 64 Gfx) */
  {0x8f71, 0x2f, 0x0ddc, 0xa6, 0x3e87},

  /* Ingrid's Back, pt. 1 (PC) */
  {0xd19b, 0xad, 0x306d, 0x24, 0x4504},
  {0xd19b, 0xad, 0x306d, 0xad, 0x878e},

  /* Ingrid's Back, pt. 2 (PC) */
  {0xc5a5, 0xfe, 0x3c98, 0x75, 0x8950},
  {0xc5a5, 0xfe, 0x3c98, 0xfe, 0x8b7b},

  /* Ingrid's Back, pt. 3 (PC) */
  {0xd7ae, 0x9e, 0x1878, 0x15, 0xadb0},
  {0xd7ae, 0x9e, 0x1878, 0x9e, 0xaf9b},

  /* Ingrid's Back, pt. 1 (Amiga) */
  {0xd188, 0x13, 0xdc60, 0x8a, 0x755c},

  /* Ingrid's Back, pt. 2 (Amiga) */
  {0xc594, 0x03, 0xea95, 0x7a, 0xb5a8},

  /* Ingrid's Back, pt. 3 (Amiga) */
  {0xd79f, 0xb5, 0x1661, 0x2c, 0xbf5d},

  /* Ingrid's Back, pt. 1 (ST) */
  {0xd183, 0x83, 0xef72, 0xfa, 0xb04f},

  /* Ingrid's Back, pt. 2 (ST) */
  {0xc58f, 0x65, 0xf337, 0xdc, 0x900a},

  /* Ingrid's Back, pt. 3 (ST) */
  {0xd79a, 0x57, 0x49c5, 0xce, 0xe0f9},

  /* Ingrid's Back, pt. 1 (Commodore 64 TO) */
  {0xb770, 0x03, 0x9a03, 0x7a, 0xdc6a},

  /* Ingrid's Back, pt. 2 (Commodore 64 TO) */
  {0xb741, 0xb6, 0x2aa5, 0x2d, 0x5a6c},

  /* Ingrid's Back, pt. 3 (Commodore 64 TO) */
  {0xb791, 0xa1, 0xd065, 0x18, 0xaa0c},

  /* Ingrid's Back, pt. 1 (Commodore 64 Gfx) */
  {0x9089, 0xce, 0xc5e2, 0x44, 0xeff4},

  /* Ingrid's Back, pt. 2 (Commodore 64 Gfx) */
  {0x908d, 0x80, 0x30c7, 0xf6, 0x2a11},

  /* Ingrid's Back, pt. 3 (Commodore 64 Gfx) */
  {0x909e, 0x9f, 0xdecc, 0x15, 0xf4da},

  {0x0000, 0x00, 0x0000, 0x00, 0x0000},
};


/*
 * gln_gameid_lookup_game()
 * gln_gameid_lookup_patch()
 *
 * Look up and return game table and patch table entries given a game's
 * length, checksum, and CRC.  Returns the entry, or NULL if not found.
 */
static gln_game_tableref_t
gln_gameid_lookup_game (gln_uint16 length, gln_byte checksum,
                        gln_uint16 crc, int ignore_crc)
{
  gln_game_tableref_t game;

  for (game = GLN_GAME_TABLE; game->length; game++)
    {
      if (game->length == length && game->checksum == checksum
          && (ignore_crc || game->crc == crc))
        break;
    }

  return game->length ? game : NULL;
}

static gln_patch_tableref_t
gln_gameid_lookup_patch (gln_uint16 length, gln_byte checksum,
                         gln_uint16 crc)
{
  gln_patch_tableref_t patch;

  for (patch = GLN_PATCH_TABLE; patch->length; patch++)
    {
      if (patch->length == length && patch->patch_checksum == checksum
          && patch->patch_crc == crc)
        break;
    }

  return patch->length ? patch : NULL;
}


/*
 * gln_gameid_identify_game()
 *
 * Identify a game from its data length, checksum, and CRC.  Returns the
 * entry of the game in the game table, or NULL if not found.
 *
 * This function uses startdata and FileSize from the core interpreter.
 * These aren't advertised symbols, so be warned.
 */
static gln_game_tableref_t
gln_gameid_identify_game (void)
{
  gln_uint16 length, crc;
  gln_byte checksum;
  int is_version2;
  gln_game_tableref_t game;
  gln_patch_tableref_t patch;

  /* If the data file appears too short for a header, give up now. */
  if (FileSize < 30)
    return NULL;

  /*
   * Find the version of the game, and the length of game data.  This logic
   * is taken from L9cut, with calcword() replaced by simple byte comparisons.
   * If the length exceeds the available data, fail.
   */
  assert (startdata);
  is_version2 = startdata[4] == 0x20 && startdata[5] == 0x00
                && startdata[10] == 0x00 && startdata[11] == 0x80
                && startdata[20] == startdata[22]
                && startdata[21] == startdata[23];

  length = is_version2
           ? startdata[28] | startdata[29] << CHAR_BIT
           : startdata[0] | startdata[1] << CHAR_BIT;
  if (length >= FileSize)
    return NULL;

  /* Calculate or retrieve the checksum, in a version specific way. */
  if (is_version2)
    {
      int index;

      checksum = 0;
      for (index = 0; index < length + 1; index++)
        checksum += startdata[index];
    }
  else
    checksum = startdata[length];

  /*
   * Generate a CRC for this data.  When L9cut calculates a CRC, it's using a
   * copy taken up to length + 1 and then padded with two NUL bytes, so we
   * mimic that here.
   */
  crc = gln_get_buffer_crc (startdata, length + 1, 2);

  /*
   * See if this is a patched file.  If it is, look up the game based on the
   * original CRC and checksum.  If not, use the current CRC and checksum.
   */
  patch = gln_gameid_lookup_patch (length, checksum, crc);
  game = gln_gameid_lookup_game (length,
                                 patch ? patch->orig_checksum : checksum,
                                 patch ? patch->orig_crc : crc,
                                 FALSE);

  /* If no game identified, retry without the CRC.  This is guesswork. */
  if (!game)
    game = gln_gameid_lookup_game (length, checksum, crc, TRUE);

  return game;
}


/*
 * gln_gameid_get_game_name()
 *
 * Return the name of the game, or NULL if not identifiable.
 *
 * This function uses startdata from the core interpreter.  This isn't an
 * advertised symbol, so be warned.
 */
static const char *
gln_gameid_get_game_name (void)
{
  /*
   * If no game name yet known, attempt to identify the game.  If it can't
   * be identified, set the cached game name to "" -- this special value
   * indicates that the game is an unknown one, but suppresses repeated
   * attempts to identify it on successive calls.
   */
  if (!gln_gameid_game_name)
    {
      gln_game_tableref_t game;

      /*
       * If the interpreter hasn't yet loaded a game, startdata is NULL
       * (uninitialized, global).  In this case, we return NULL, allowing
       * for retries until a game is loaded.
       */
      if (!startdata)
        return NULL;

      game = gln_gameid_identify_game ();
      gln_gameid_game_name = game ? game->name : "";
    }

  /* Return the game's name, or NULL if it was unidentifiable. */
  assert (gln_gameid_game_name);
  return strlen (gln_gameid_game_name) > 0 ? gln_gameid_game_name : NULL;
}


/*
 * gln_gameid_game_name_reset()
 *
 * Clear the saved game name, forcing a new lookup when next queried.  This
 * function should be called by actions that may cause the interpreter to
 * change game file, for example os_set_filenumber().
 */
static void
gln_gameid_game_name_reset (void)
{
  gln_gameid_game_name = NULL;
}


/*---------------------------------------------------------------------*/
/*  Glk port bitmap picture functions                                  */
/*---------------------------------------------------------------------*/

/* R,G,B color triple definition. */
typedef struct
{
  int red, green, blue;
} gln_rgb_t;
typedef gln_rgb_t *gln_rgbref_t;

/*
 * Maximum number of regions to consider in a single repaint pass.  A
 * couple of hundred seems to strike the right balance between not too
 * sluggardly picture updates, and responsiveness to input during graphics
 * rendering, when combined with short timeouts.
 */
static const int GLN_REPAINT_LIMIT = 256;

/*
 * Graphics timeout; we like an update call after this period (ms).  In
 * practice, this timeout may actually be shorter than the time taken
 * to reach the limit on repaint regions, but because Glk guarantees that
 * user interactions (in this case, line events) take precedence over
 * timeouts, this should be okay; we'll still see a game that responds to
 * input each time the background repaint function yields.
 *
 * Setting this value is tricky.  We'd like it to be the shortest possible
 * consistent with getting other stuff done, say 10ms.  However, Xglk has
 * a granularity of 50ms on checking for timeouts, as it uses a 1/20s
 * timeout on X select.  This means that the shortest timeout we'll ever
 * get from Xglk will be 50ms, so there's no point in setting this shorter
 * than that.  With luck, other Glk libraries will be more efficient than
 * this, and can give us higher timer resolution; we'll set 50ms here, and
 * hope that no other Glk library is worse.
 */
static const glui32 GLN_GRAPHICS_TIMEOUT = 50;

/*
 * Count of timeouts to wait on.  Waiting after a repaint smooths the
 * display where the frame is being resized, by helping to avoid graphics
 * output while more resize events are received; around 1/2 second seems
 * okay.
 */
static const int GLN_GRAPHICS_REPAINT_WAIT = 10;

/* Pixel size multiplier for image size scaling. */
static const int GLN_GRAPHICS_PIXEL = 2;

/* Proportion of the display to use for graphics. */
static const glui32 GLN_GRAPHICS_PROPORTION = 30;

/*
 * Special title picture number, requiring its own handling, and count of
 * timeouts to wait on after fully rendering the title picture (~2 seconds).
 */
static const int GLN_GRAPHICS_TITLE_PICTURE = 0,
                 GLN_GRAPHICS_TITLE_WAIT = 40;

/*
 * Border and shading control.  For cases where we can't detect the back-
 * ground color of the main window, there's a default, white, background.
 * Bordering is black, with a 1 pixel border, 2 pixel shading, and 8 steps
 * of shading fade.
 */
static const glui32 GLN_GRAPHICS_DEFAULT_BACKGROUND = 0x00ffffff,
                    GLN_GRAPHICS_BORDER_COLOR = 0x00000000;
static const int GLN_GRAPHICS_BORDER = 1,
                 GLN_GRAPHICS_SHADING = 2,
                 GLN_GRAPHICS_SHADE_STEPS = 8;

/*
 * Guaranteed unused pixel value.  This value is used to fill the on-screen
 * buffer on new pictures or repaints, resulting in a full paint of all
 * pixels since no off-screen, real picture, pixel will match it.
 */
static const int GLN_GRAPHICS_UNUSED_PIXEL = 0xff;

/* Graphics file directory, and type of graphics found in it. */
static char *gln_graphics_bitmap_directory = NULL;
static BitmapType gln_graphics_bitmap_type = NO_BITMAPS;

/* The current picture id being displayed. */
enum { GLN_PALETTE_SIZE = 32 };
static gln_byte *gln_graphics_bitmap = NULL;
static gln_uint16 gln_graphics_width = 0,
                  gln_graphics_height = 0;
static Colour gln_graphics_palette[GLN_PALETTE_SIZE]; /* = { 0, ... }; */
static int gln_graphics_picture = -1;

/*
 * Flags set on new picture, and on resize or arrange events, and a flag
 * to indicate whether background repaint is stopped or active.
 */
static int gln_graphics_new_picture = FALSE,
           gln_graphics_repaint = FALSE,
           gln_graphics_active = FALSE;

/*
 * State to monitor the state of interpreter graphics.  The values of the
 * enumerations match the modes supplied by os_graphics().
 */
static enum {
  GLN_GRAPHICS_OFF = 0,
  GLN_GRAPHICS_LINE_MODE = 1,
  GLN_GRAPHICS_BITMAP_MODE = 2
}
gln_graphics_interpreter_state = GLN_GRAPHICS_OFF;


/*
 * Pointer to the two graphics buffers, one the off-screen representation
 * of pixels, and the other tracking on-screen data.  These are temporary
 * graphics malloc'ed memory, and should be free'd on exit.
 */
static gln_byte *gln_graphics_off_screen = NULL,
                *gln_graphics_on_screen = NULL;

/*
 * The number of colors used in the palette by the current picture.  Because
 * of the way it's queried, we risk a race, with admittedly a very low
 * probability, with the updater.  So, it's initialized instead to the
 * largest possible value.  The real value in use is inserted on the first
 * picture update timeout call for a new picture.
 */
static int gln_graphics_color_count = GLN_PALETTE_SIZE;


/*
 * gln_graphics_open()
 *
 * If it's not open, open the graphics window.  Returns TRUE if graphics
 * was successfully started, or already on.
 */
static int
gln_graphics_open (void)
{
  if (!gln_graphics_window)
    {
      gln_graphics_window = glk_window_open (gln_main_window,
                                             winmethod_Above
                                             | winmethod_Proportional,
                                             GLN_GRAPHICS_PROPORTION,
                                             wintype_Graphics, 0);
    }

  return gln_graphics_window != NULL;
}


/*
 * gln_graphics_close()
 *
 * If open, close the graphics window and set back to NULL.
 */
static void
gln_graphics_close (void)
{
  if (gln_graphics_window)
    {
      glk_window_close (gln_graphics_window, NULL);
      gln_graphics_window = NULL;
    }
}


/*
 * gln_graphics_start()
 *
 * If graphics enabled, start any background picture update processing.
 */
static void
gln_graphics_start (void)
{
  if (gln_graphics_enabled)
    {
      /* If not running, start the updating "thread". */
      if (!gln_graphics_active)
        {
          glk_request_timer_events (GLN_GRAPHICS_TIMEOUT);
          gln_graphics_active = TRUE;
        }
    }
}


/*
 * gln_graphics_stop()
 *
 * Stop any background picture update processing.
 */
static void
gln_graphics_stop (void)
{
  /* If running, stop the updating "thread". */
  if (gln_graphics_active)
    {
      glk_request_timer_events (0);
      gln_graphics_active = FALSE;
    }
}


/*
 * gln_graphics_are_displayed()
 *
 * Return TRUE if graphics are currently being displayed, FALSE otherwise.
 */
static int
gln_graphics_are_displayed (void)
{
  return gln_graphics_window != NULL;
}


/*
 * gln_graphics_paint()
 *
 * Set up a complete repaint of the current picture in the graphics window.
 * This function should be called on the appropriate Glk window resize and
 * arrange events.
 */
static void
gln_graphics_paint (void)
{
  if (gln_graphics_enabled && gln_graphics_are_displayed ())
    {
      /* Set the repaint flag, and start graphics. */
      gln_graphics_repaint = TRUE;
      gln_graphics_start ();
    }
}


/*
 * gln_graphics_restart()
 *
 * Restart graphics as if the current picture is a new picture.  This
 * function should be called whenever graphics is re-enabled after being
 * disabled.
 */
static void
gln_graphics_restart (void)
{
  if (gln_graphics_enabled && gln_graphics_are_displayed ())
    {
      /* Set the new picture flag, and start graphics. */
      gln_graphics_new_picture = TRUE;
      gln_graphics_start ();
    }
}


/*
 * gln_graphics_count_colors()
 *
 * Analyze an image, and return an overall count of how many colors out of
 * the palette are used.
 */
static int
gln_graphics_count_colors (gln_byte bitmap[],
                           gln_uint16 width, gln_uint16 height)
{
  int x, y, count;
  long usage[GLN_PALETTE_SIZE], index_row;
  assert (bitmap);

  /*
   * Traverse the image, counting each pixel usage.  For the y iterator,
   * maintain an index row as an optimization to avoid multiplications in
   * the loop.
   */
  count = 0;
  memset (usage, 0, sizeof (usage));
  for (y = 0, index_row = 0; y < height; y++, index_row += width)
    {
      for (x = 0; x < width; x++)
        {
          long index;

          /* Get the pixel index, and update the count for this color. */
          index = index_row + x;
          usage[bitmap[index]]++;

          /* If color usage is now 1, note new color encountered. */
          if (usage[bitmap[index]] == 1)
            count++;
        }
    }

  return count;
}


/*
 * gln_graphics_split_color()
 * gln_graphics_combine_color()
 *
 * General graphics helper functions, to convert between RGB and Glk glui32
 * color representations.
 */
static void
gln_graphics_split_color (glui32 color, gln_rgbref_t rgb_color)
{
  assert (rgb_color);

  rgb_color->red   = (color >> 16) & 0xff;
  rgb_color->green = (color >> 8) & 0xff;
  rgb_color->blue  = color & 0xff;
}

static glui32
gln_graphics_combine_color (gln_rgbref_t rgb_color)
{
  glui32 color;
  assert (rgb_color);

  color = (rgb_color->red << 16) | (rgb_color->green << 8) | rgb_color->blue;
  return color;
}


/*
 * gln_graphics_clear_and_border()
 *
 * Clear the graphics window, and border and shade the area where the
 * picture is going to be rendered.  This attempts a small raised effect
 * for the picture, in keeping with modern trends.
 */
static void
gln_graphics_clear_and_border (winid_t glk_window,
                               int x_offset, int y_offset,
                               int pixel_size, gln_uint16 width,
                               gln_uint16 height)
{
  glui32 background, fade_color, shading_color;
  gln_rgb_t rgb_background, rgb_border, rgb_fade;
  int index;
  assert (glk_window);

  /*
   * Try to detect the background color of the main window, by getting the
   * background for Normal style (Glk offers no way to directly get a window's
   * background color).  If we can get it, we'll match the graphics window
   * background to it.  If we can't, we'll default the color to white.
   */
  if (!glk_style_measure (gln_main_window,
                          style_Normal, stylehint_BackColor, &background))
    {
      /*
       * Unable to get the main window background, so assume, and default
       * graphics to white.
       */
      background = GLN_GRAPHICS_DEFAULT_BACKGROUND;
    }

  /*
   * Set the graphics window background to match the main window background,
   * as best as we can tell, and clear the window.
   */
  glk_window_set_background_color (glk_window, background);
  glk_window_clear (glk_window);
#if !(defined(GARGLK) || defined(SPATTERLIGHT))
  /*
   * For very small pictures, just border them, but don't try and
   * do any shading.  Failing this check is probably highly unlikely.
   */
  if (width < 2 * GLN_GRAPHICS_SHADE_STEPS
      || height < 2 * GLN_GRAPHICS_SHADE_STEPS)
    {
      /* Paint a rectangle bigger than the picture by border pixels. */
      glk_window_fill_rect (glk_window,
                            GLN_GRAPHICS_BORDER_COLOR,
                            x_offset - GLN_GRAPHICS_BORDER,
                            y_offset - GLN_GRAPHICS_BORDER,
                            width * pixel_size + GLN_GRAPHICS_BORDER * 2,
                            height * pixel_size + GLN_GRAPHICS_BORDER * 2);
      return;
    }
#endif
  /*
   * Paint a rectangle bigger than the picture by border pixels all round,
   * and with additional shading pixels right and below.  Some of these
   * shading pixels are later overwritten by the fading loop below.  The
   * picture will sit over this rectangle.
   */
  glk_window_fill_rect (glk_window,
                        GLN_GRAPHICS_BORDER_COLOR,
                        x_offset - GLN_GRAPHICS_BORDER,
                        y_offset - GLN_GRAPHICS_BORDER,
                        width * pixel_size + GLN_GRAPHICS_BORDER * 2
                          + GLN_GRAPHICS_SHADING,
                        height * pixel_size + GLN_GRAPHICS_BORDER * 2
                          + GLN_GRAPHICS_SHADING);

  /*
   * Split the main window background color and the border color into
   * components.
   */
  gln_graphics_split_color (background, &rgb_background);
  gln_graphics_split_color (GLN_GRAPHICS_BORDER_COLOR, &rgb_border);

  /*
   * Generate the incremental color to use in fade steps.  Here we're
   * assuming that the border is always darker than the main window
   * background (currently valid, as we're using black).
   */
  rgb_fade.red = (rgb_background.red - rgb_border.red)
                 / GLN_GRAPHICS_SHADE_STEPS;
  rgb_fade.green = (rgb_background.green - rgb_border.green)
                 / GLN_GRAPHICS_SHADE_STEPS;
  rgb_fade.blue = (rgb_background.blue - rgb_border.blue)
                 / GLN_GRAPHICS_SHADE_STEPS;

  /* Combine RGB fade into a single incremental Glk color. */
  fade_color = gln_graphics_combine_color (&rgb_fade);

  /* Fade in edge, from background to border, shading in stages. */
  shading_color = background;
  for (index = 0; index < GLN_GRAPHICS_SHADE_STEPS; index++)
    {
      /* Shade the two border areas with this color. */
      glk_window_fill_rect (glk_window, shading_color,
                            x_offset + width * pixel_size
                              + GLN_GRAPHICS_BORDER,
                            y_offset + index - GLN_GRAPHICS_BORDER,
                            GLN_GRAPHICS_SHADING, 1);
      glk_window_fill_rect (glk_window, shading_color,
                            x_offset + index - GLN_GRAPHICS_BORDER,
                            y_offset + height * pixel_size
                              + GLN_GRAPHICS_BORDER,
                            1, GLN_GRAPHICS_SHADING);

      /* Update the shading color for the fade next iteration. */
      shading_color -= fade_color;
    }
}


/*
 * gln_graphics_convert_palette()
 *
 * Convert a Level 9 bitmap color palette to a Glk one.
 */
static void
gln_graphics_convert_palette (Colour ln_palette[], glui32 glk_palette[])
{
  int index;
  assert (ln_palette && glk_palette);

  for (index = 0; index < GLN_PALETTE_SIZE; index++)
    {
      Colour colour;
      gln_rgb_t gln_color;

      /* Convert color from Level 9 to internal RGB, then to Glk color. */
      colour = ln_palette[index];
      gln_color.red   = colour.red;
      gln_color.green = colour.green;
      gln_color.blue  = colour.blue;
      glk_palette[index] = gln_graphics_combine_color (&gln_color);
    }
}


/*
 * gln_graphics_position_picture()
 *
 * Given a picture width and height, return the x and y offsets to center
 * this picture in the current graphics window.
 */
static void
gln_graphics_position_picture (winid_t glk_window, int pixel_size,
                               gln_uint16 width, gln_uint16 height,
                               int *x_offset, int *y_offset)
{
  glui32 window_width, window_height;
  assert (glk_window && x_offset && y_offset);

  /* Measure the current graphics window dimensions. */
  glk_window_get_size (glk_window, &window_width, &window_height);

    if (window_height < height * pixel_size + GLN_GRAPHICS_BORDER * 2 + GLN_GRAPHICS_SHADING)
    {
        glk_window_close(gln_graphics_window, NULL);
        gln_graphics_window = glk_window_open (gln_main_window,
                                           winmethod_Above
                                           | winmethod_Fixed,
                                           height * pixel_size + GLN_GRAPHICS_BORDER * 2 + GLN_GRAPHICS_SHADING + 20,
                                           wintype_Graphics, 0);
        glk_window_get_size (gln_graphics_window, &window_width, &window_height);
    }

  /*
   * Calculate and return an x and y offset to use on point plotting, so that
   * the image centers inside the graphical window.
   */
  *x_offset = ((int) window_width - width * pixel_size) / 2;
  *y_offset = ((int) window_height - height * pixel_size) / 2;
}


/*
 * gln_graphics_is_vertex()
 *
 * Given a point, return TRUE if that point is the vertex of a fillable
 * region.  This is a helper function for layering pictures.  When assign-
 * ing layers, we want to weight the colors that have the most complex
 * shapes, or the largest count of isolated areas, heavier than simpler
 * areas.
 *
 * By painting the colors with the largest number of isolated areas or
 * the most complex shapes first, we help to minimize the number of fill
 * regions needed to render the complete picture.
 */
static int
gln_graphics_is_vertex (gln_byte off_screen[],
                        gln_uint16 width, gln_uint16 height, int x, int y)
{
  gln_byte pixel;
  int above, below, left, right;
  long index_row;
  assert (off_screen);

  /* Use an index row to cut down on multiplications. */
  index_row = y * width;

  /* Find the color of the reference pixel. */
  pixel = off_screen[index_row + x];
  assert (pixel < GLN_PALETTE_SIZE);

  /*
   * Detect differences between the reference pixel and its upper, lower, left
   * and right neighbors.  Mark as different if the neighbor doesn't exist,
   * that is, at the edge of the picture.
   */
  above = (y == 0 || off_screen[index_row - width + x] != pixel);
  below = (y == height - 1 || off_screen[index_row + width + x] != pixel);
  left = (x == 0 || off_screen[index_row + x - 1] != pixel);
  right = (x == width - 1 || off_screen[index_row + x + 1] != pixel);

  /*
   * Return TRUE if this pixel lies at the vertex of a rectangular, fillable,
   * area.  That is, if two adjacent neighbors aren't the same color (or if
   * absent -- at the edge of the picture).
   */
  return ((above || below) && (left || right));
}


/*
 * gms_graphics_compare_layering_inverted()
 * gln_graphics_assign_layers()
 *
 * Given two sets of image bitmaps, and a palette, this function will
 * assign layers palette colors.
 *
 * Layers are assigned by first counting the number of vertices in the
 * color plane, to get a measure of the complexity of shapes displayed in
 * this color, and also the raw number of times each palette color is
 * used.  This is then sorted, so that layers are assigned to colors, with
 * the lowest layer being the color with the most complex shapes, and
 * within this (or where the count of vertices is zero) the most used color.
 *
 * The function compares pixels in the two image bitmaps given, these
 * being the off-screen and on-screen buffers, and generates counts only
 * where these bitmaps differ.  This ensures that only pixels not yet
 * painted are included in layering.
 *
 * As well as assigning layers, this function returns a set of layer usage
 * flags, to help the rendering loop to terminate as early as possible.
 *
 * By painting lower layers first, the paint can take in larger areas if
 * it's permitted to include not-yet-validated higher levels.  This helps
 * minimize the amount of Glk areas fills needed to render a picture.
 */
typedef struct {
  long complexity;  /* Count of vertices for this color. */
  long usage;       /* Color usage count. */
  int color;        /* Color index into palette. */
} gln_layering_t;

static int
gln_graphics_compare_layering_inverted (const void *void_first,
                                        const void *void_second)
{
  gln_layering_t * first = (gln_layering_t *) void_first;
  gln_layering_t * second = (gln_layering_t *) void_second;

  /*
   * Order by complexity first, then by usage, putting largest first.  Some
   * colors may have no vertices at all when doing animation frames, but
   * rendering optimization relies on the first layer that contains no areas
   * to fill halting the rendering loop.  So it's important here that we order
   * indexes so that colors that render complex shapes come first, non-empty,
   * but simpler shaped colors next, and finally all genuinely empty layers.
   */
  return second->complexity > first->complexity ? 1 :
         first->complexity > second->complexity ? -1 :
         second->usage > first->usage ? 1 :
         first->usage > second->usage ? -1 : 0;
}

static void
gln_graphics_assign_layers (gln_byte off_screen[], gln_byte on_screen[],
                            gln_uint16 width, gln_uint16 height,
                            int layers[], long layer_usage[])
{
  int index, x, y;
  long index_row;
  gln_layering_t layering[GLN_PALETTE_SIZE];
  assert (off_screen && on_screen && layers && layer_usage);

  /* Clear initial complexity and usage counts, and set initial colors. */
  for (index = 0; index < GLN_PALETTE_SIZE; index++)
    {
      layering[index].complexity = 0;
      layering[index].usage = 0;
      layering[index].color = index;
    }

  /*
   * Traverse the image, counting vertices and pixel usage where the pixels
   * differ between the off-screen and on-screen buffers.  Optimize by
   * maintaining an index row to avoid multiplications.
   */
  for (y = 0, index_row = 0; y < height; y++, index_row += width)
    {
      for (x = 0; x < width; x++)
        {
          long index;

          /*
           * Get the index for this pixel, and update complexity and usage
           * if off-screen and on-screen pixels differ.
           */
          index = index_row + x;
          if (on_screen[index] != off_screen[index])
            {
              if (gln_graphics_is_vertex (off_screen, width, height, x, y))
                layering[off_screen[index]].complexity++;

              layering[off_screen[index]].usage++;
            }
        }
    }

  /*
   * Sort counts to form color indexes.  The primary sort is on the shape
   * complexity, and within this, on color usage.
   */
  qsort (layering, GLN_PALETTE_SIZE,
         sizeof (*layering), gln_graphics_compare_layering_inverted);

  /*
   * Assign a layer to each palette color, and also return the layer usage
   * for each layer.
   */
  for (index = 0; index < GLN_PALETTE_SIZE; index++)
    {
      layers[layering[index].color] = index;
      layer_usage[index] = layering[index].usage;
    }
}


/*
 * gln_graphics_paint_region()
 *
 * This is a partially optimized point plot.  Given a point in the graphics
 * bitmap, it tries to extend the point to a color region, and fill a number
 * of pixels in a single Glk rectangle fill.  The goal here is to reduce the
 * number of Glk rectangle fills, which tend to be extremely inefficient
 * operations for generalized point plotting.
 *
 * The extension works in image layers; each palette color is assigned* a
 * layer, and we paint each layer individually, starting at the lowest.  So,
 * the region is free to fill any invalidated pixel in a higher layer, and
 * all pixels, invalidated or already validated, in the same layer.  In
 * practice, it is good enough to look for either invalidated pixels or pixels
 * in the same layer, and construct a region as large as possible from these,
 * then on marking points as validated, mark only those in the same layer as
 * the initial point.
 *
 * The optimization here is not the best possible, but is reasonable.  What
 * we do is to try and stretch the region horizontally first, then vertically.
 * In practice, we might find larger areas by stretching vertically and then
 * horizontally, or by stretching both dimensions at the same time.  In
 * mitigation, the number of colors in a picture is small (16), and the
 * aspect ratio of pictures makes them generally wider than they are tall.
 *
 * Once we've found the region, we render it with a single Glk rectangle fill,
 * and mark all the pixels in this region that match the layer of the initial
 * given point as validated.
 */
static void
gln_graphics_paint_region (winid_t glk_window, glui32 palette[], int layers[],
                           gln_byte off_screen[], gln_byte on_screen[],
                           int x, int y, int x_offset, int y_offset,
                           int pixel_size, gln_uint16 width, gln_uint16 height)
{
  gln_byte pixel;
  int layer, x_min, x_max, y_min, y_max, x_index, y_index;
  long index_row;
  assert (glk_window && palette && layers && off_screen && on_screen);

  /* Find the color and layer for the initial pixel. */
  pixel = off_screen[y * width + x];
  layer = layers[pixel];
  assert (pixel < GLN_PALETTE_SIZE);

  /*
   * Start by finding the extent to which we can pull the x coordinate and
   * still find either invalidated pixels, or pixels in this layer.
   *
   * Use an index row to remove multiplications from the loops.
   */
  index_row = y * width;
  for (x_min = x; x_min - 1 >= 0; x_min--)
    {
      long index = index_row + x_min - 1;

      if (on_screen[index] == off_screen[index]
          && layers[off_screen[index]] != layer)
        break;
    }
  for (x_max = x; x_max + 1 < width; x_max++)
    {
      long index = index_row + x_max + 1;

      if (on_screen[index] == off_screen[index]
          && layers[off_screen[index]] != layer)
        break;
    }

  /*
   * Now try to stretch the height of the region, by extending the y
   * coordinate as much as possible too.  Again, we're looking for pixels
   * that are invalidated or ones in the same layer.  We need to check
   * across the full width of the current region.
   *
   * As above, an index row removes multiplications from the loops.
   */
  for (y_min = y, index_row = (y - 1) * width;
       y_min - 1 >= 0; y_min--, index_row -= width)
    {
      for (x_index = x_min; x_index <= x_max; x_index++)
        {
          long index = index_row + x_index;

          if (on_screen[index] == off_screen[index]
              && layers[off_screen[index]] != layer)
            goto break_y_min;
        }
    }
break_y_min:

  for (y_max = y, index_row = (y + 1) * width;
       y_max + 1 < height; y_max++, index_row += width)
    {
      for (x_index = x_min; x_index <= x_max; x_index++)
        {
          long index = index_row + x_index;

          if (on_screen[index] == off_screen[index]
              && layers[off_screen[index]] != layer)
            goto break_y_max;
        }
    }
break_y_max:

  /* Fill the region using Glk's rectangle fill. */
  glk_window_fill_rect (glk_window, palette[pixel],
                        x_min * pixel_size + x_offset,
                        y_min * pixel_size + y_offset,
                        (x_max - x_min + 1) * pixel_size,
                        (y_max - y_min + 1) * pixel_size);

  /*
   * Validate each pixel in the reference layer that was rendered by the
   * rectangle fill.  We don't validate pixels that are not in this layer
   * (and are by definition in higher layers, as we've validated all lower
   * layers), since although we colored them, we did it for optimization
   * reasons, and they're not yet colored correctly.
   *
   * Maintain an index row as an optimization to avoid multiplication.
   */
  index_row = y_min * width;
  for (y_index = y_min; y_index <= y_max; y_index++)
    {
      for (x_index = x_min; x_index <= x_max; x_index++)
        {
          long index;

          /*
           * Get the index for x_index,y_index.  If the layers match, update
           * the on-screen buffer.
           */
          index = index_row + x_index;
          if (layers[off_screen[index]] == layer)
            {
              assert (off_screen[index] == pixel);
              on_screen[index] = off_screen[index];
            }
        }

      /* Update row index component on change of y. */
      index_row += width;
    }
}

static void
gln_graphics_paint_everything (winid_t glk_window,
			glui32 palette[],
			gln_byte off_screen[],
			int x_offset, int y_offset,
			gln_uint16 width, gln_uint16 height)
{
	gln_byte		pixel;			/* Reference pixel color */
	int		x, y;

	for (y = 0; y < height; y++)
	{
	    for (x = 0; x < width; x ++)
	    {
		pixel = off_screen[ y * width + x ];
		glk_window_fill_rect (glk_window,
			palette[ pixel ],
			x * GLN_GRAPHICS_PIXEL + x_offset,
			y * GLN_GRAPHICS_PIXEL + y_offset,
			GLN_GRAPHICS_PIXEL, GLN_GRAPHICS_PIXEL);
	    }
	}
}

/*
 * gln_graphics_timeout()
 *
 * This is a background function, called on Glk timeouts.  Its job is to
 * repaint some of the current graphics image.  On successive calls, it
 * does a part of the repaint, then yields to other processing.  This is
 * useful since the Glk primitive to plot points in graphical windows is
 * extremely slow; this way, the repaint doesn't block game play.
 *
 * The function should be called on Glk timeout events.  When the repaint
 * is complete, the function will turn off Glk timers.
 *
 * The function uses double-buffering to track how much of the graphics
 * buffer has been rendered.  This helps to minimize the amount of point
 * plots required, as only the differences between the two buffers need
 * to be rendered.
 */
static void
gln_graphics_timeout (void)
{
  static glui32 palette[GLN_PALETTE_SIZE];   /* Precomputed Glk palette */
  static int layers[GLN_PALETTE_SIZE];       /* Assigned image layers */
  static long layer_usage[GLN_PALETTE_SIZE]; /* Image layer occupancies */

  static int deferred_repaint = FALSE;       /* Local delayed repaint flag */
  static int ignore_counter;                 /* Count of calls ignored */

  static int x_offset, y_offset;             /* Point plot offsets */
  static int yield_counter;                  /* Yields in rendering */
  static int saved_layer;                    /* Saved current layer */
  static int saved_x, saved_y;               /* Saved x,y coord */

  static int total_regions;                  /* Debug statistic */

  gln_byte *on_screen;                       /* On-screen image buffer */
  gln_byte *off_screen;                      /* Off-screen image buffer */
  long picture_size;                         /* Picture size in pixels */
  int layer;                                 /* Image layer iterator */
  int x, y;                                  /* Image iterators */
  int regions;                               /* Count of regions painted */

  /* Ignore the call if the current graphics state is inactive. */
  if (!gln_graphics_active)
    return;
  assert (gln_graphics_window);

  /*
   * On detecting a repaint request, note the flag in a local static variable,
   * then set up a graphics delay to wait until, hopefully, the resize, if
   * that's what caused it, is complete, and return.  This makes resizing the
   * window a lot smoother, since it prevents unnecessary region paints where
   * we are receiving consecutive Glk arrange or redraw events.
   */
  if (gln_graphics_repaint)
    {
      deferred_repaint = TRUE;
      gln_graphics_repaint = FALSE;
      ignore_counter = GLN_GRAPHICS_REPAINT_WAIT - 1;
      return;
    }

  /*
   * If asked to ignore a given number of calls, decrement the ignore counter
   * and return having done nothing more.  This lets us delay graphics
   * operations by a number of timeouts, providing partial protection from
   * resize event "storms".
   *
   * Note -- to wait for N timeouts, set the count of timeouts to be ignored
   * to N-1.
   */
  assert (ignore_counter >= 0);
  if (ignore_counter > 0)
    {
      ignore_counter--;
      return;
    }

  /* Calculate the picture size, and synchronize screen buffer pointers. */
  picture_size = gln_graphics_width * gln_graphics_height;
  off_screen = gln_graphics_off_screen;
  on_screen = gln_graphics_on_screen;

  /*
   * If we received a new picture, set up the local static variables for that
   * picture -- convert the color palette, and initialize the off_screen
   * buffer to be the base picture.
   */
  if (gln_graphics_new_picture)
    {
      /* Initialize the off_screen buffer to be a copy of the base picture. */
      free (off_screen);
      off_screen = gln_malloc (picture_size * sizeof (*off_screen));
      memcpy (off_screen, gln_graphics_bitmap,
              picture_size * sizeof (*off_screen));

      /* Note the buffer for freeing on cleanup. */
      gln_graphics_off_screen = off_screen;

      /*
       * Pre-convert all the picture palette colors into their corresponding
       * Glk colors.
       */
      gln_graphics_convert_palette (gln_graphics_palette, palette);

      /* Save the color count for possible queries later. */
      gln_graphics_color_count =
          gln_graphics_count_colors (off_screen,
                                     gln_graphics_width, gln_graphics_height);
    }

  /*
   * For a new picture, or a repaint of a prior one, calculate new values for
   * the x and y offsets used to draw image points, and set the on-screen
   * buffer to an unused pixel value, in effect invalidating all on-screen
   * data.  Also, reset the saved image scan coordinates so that we scan for
   * unpainted pixels from top left starting at layer zero, and clear the
   * graphics window.
   */
  if (gln_graphics_new_picture || deferred_repaint)
    {
      /*
       * Calculate the x and y offset to center the picture in the graphics
       * window.
       */
      gln_graphics_position_picture (gln_graphics_window,
                                     GLN_GRAPHICS_PIXEL,
                                     gln_graphics_width, gln_graphics_height,
                                     &x_offset, &y_offset);

      /*
       * Reset all on-screen pixels to an unused value, guaranteed not to
       * match any in a real picture.  This forces all pixels to be repainted
       * on a buffer/on-screen comparison.
       */
      free (on_screen);
      on_screen = gln_malloc (picture_size * sizeof (*on_screen));
      memset (on_screen, GLN_GRAPHICS_UNUSED_PIXEL,
              picture_size * sizeof (*on_screen));

      /* Note the buffer for freeing on cleanup. */
      gln_graphics_on_screen = on_screen;

      /*
       * Assign new layers to the current image.  This sorts colors by usage
       * and puts the most used colors in the lower layers.  It also hands us
       * a count of pixels in each layer, useful for knowing when to stop
       * scanning for layers in the rendering loop.
       */
#ifndef GARGLK
      gln_graphics_assign_layers (off_screen, on_screen,
                                  gln_graphics_width, gln_graphics_height,
                                  layers, layer_usage);
#endif

      /* Clear the graphics window. */
      gln_graphics_clear_and_border (gln_graphics_window,
                                     x_offset, y_offset,
                                     GLN_GRAPHICS_PIXEL,
                                     gln_graphics_width, gln_graphics_height);

      /* Start a fresh picture rendering pass. */
      yield_counter = 0;
      saved_layer = 0;
      saved_x = 0;
      saved_y = 0;
      total_regions = 0;

      /* Clear the new picture and deferred repaint flags. */
      gln_graphics_new_picture = FALSE;
      deferred_repaint = FALSE;
    }

#ifndef GARGLK
  /*
   * Make a portion of an image pass, from lower to higher image layers,
   * scanning for invalidated pixels that are in the current image layer we
   * are painting.  Each invalidated pixel gives rise to a region paint,
   * which equates to one Glk rectangle fill.
   *
   * When the limit on regions is reached, save the current image pass layer
   * and coordinates, and yield control to the main game playing code by
   * returning.  On the next call, pick up where we left off.
   *
   * As an optimization, we can leave the loop on the first empty layer we
   * encounter.  Since layers are ordered by complexity and color usage, all
   * layers higher than the first unused one will also be empty, so we don't
   * need to scan them.
   */
  regions = 0;
  for (layer = saved_layer;
       layer < GLN_PALETTE_SIZE && layer_usage[layer] > 0; layer++)
    {
      long index_row;

      /*
       * As an optimization to avoid multiplications in the loop, maintain a
       * separate index row.
       */
      index_row = saved_y * gln_graphics_width;
      for (y = saved_y; y < gln_graphics_height; y++)
        {
          for (x = saved_x; x < gln_graphics_width; x++)
            {
              long index;

              /* Get the index for this pixel. */
              index = index_row + x;
              assert (index < picture_size * sizeof (*off_screen));

              /*
               * Ignore pixels not in the current layer, and pixels not
               * currently invalid (that is, ones whose on-screen represen-
               * tation matches the off-screen buffer).
               */
              if (layers[off_screen[index]] == layer
                  && on_screen[index] != off_screen[index])
                {
                  /*
                   * Rather than painting just one pixel, here we try to
                   * paint the maximal region we can for the layer of the
                   * given pixel.
                   */
                  gln_graphics_paint_region (gln_graphics_window,
                                             palette, layers,
                                             off_screen, on_screen,
                                             x, y, x_offset, y_offset,
                                             GLN_GRAPHICS_PIXEL,
                                             gln_graphics_width,
                                             gln_graphics_height);

                  /*
                   * Increment count of regions handled, and yield, by
                   * returning, if the limit on paint regions is reached.
                   * Before returning, save the current layer and scan
                   * coordinates, so we can pick up here on the next call.
                   */
                  regions++;
                  if (regions >= GLN_REPAINT_LIMIT)
                    {
                      yield_counter++;
                      saved_layer = layer;
                      saved_x = x;
                      saved_y = y;
                      total_regions += regions;
                      return;
                    }
                }
            }

          /* Reset the saved x coordinate on y increment. */
          saved_x = 0;

          /* Update the index row on change of y. */
          index_row += gln_graphics_width;
        }

      /* Reset the saved y coordinate on layer change. */
      saved_y = 0;
    }

  /*
   * If we reach this point, then we didn't get to the limit on regions
   * painted on this pass.  In that case, we've finished rendering the
   * image.
   */
  assert (regions < GLN_REPAINT_LIMIT);
  total_regions += regions;

#else
  gln_graphics_paint_everything
      (gln_graphics_window,
       palette, off_screen,
       x_offset, y_offset,
       gln_graphics_width,
       gln_graphics_height);
#endif

  /* Stop graphics; there's no more to be done until something restarts us. */
  gln_graphics_stop ();
}

/*
 * gln_graphics_locate_bitmaps()
 *
 * Given the name of the game file being run, try to set up the graphics
 * directory and bitmap type for that game.  If none available, set the
 * directory to NULL, and bitmap type to NO_BITMAPS.
 */
static void
gln_graphics_locate_bitmaps (const char *gamefile)
{
  const char *basename;
  char *dirname;
  BitmapType bitmap_type;

  /* Find the start of the last element of the filename passed in. */
  basename = strrchr (gamefile, GLN_FILE_DELIM);
  basename = basename ? basename + 1 : gamefile;

  /* Take a copy of the directory part of the filename. */
  dirname = gln_malloc (basename - gamefile + 1);
  strncpy (dirname, gamefile, basename - gamefile);
  dirname[basename - gamefile] = '\0';

  /*
   * Use the core interpreter to search for suitable bitmaps.  If none found,
   * free allocated memory and return noting none available.
   */
  bitmap_type = DetectBitmaps (dirname);
  if (bitmap_type == NO_BITMAPS)
    {
      free (dirname);
      gln_graphics_bitmap_directory = NULL;
      gln_graphics_bitmap_type = NO_BITMAPS;
      return;
    }

  /* Record the bitmap details for later use. */
  gln_graphics_bitmap_directory = dirname;
  gln_graphics_bitmap_type = bitmap_type;
}


/*
 * gln_graphics_handle_title_picture()
 *
 * Picture 0 is special, normally the title picture.  Unless we handle it
 * specially, the next picture comes along and instantly overwrites it.
 * Here, then, we try to delay until the picture has rendered, allowing the
 * delay to be broken with a keypress.
 */
static void
gln_graphics_handle_title_picture (void)
{
  event_t event;
  int count;

  gln_standout_string ("\n[ Press any key to skip the title picture... ]\n\n");

  /* Wait until a keypress or graphics rendering is complete. */
  glk_request_char_event (gln_main_window);
  do
    {
      gln_event_wait_2 (evtype_CharInput, evtype_Timer, &event);

      /*
       * If a character was pressed, return.  This will let the game
       * progress, probably into showing the next bitmap.
       */
      if (event.type == evtype_CharInput)
        {
          gln_watchdog_tick ();
          return;
        }
    }
  while (gln_graphics_active);

  /*
   * Now wait another couple of seconds, or until a keypress.  We'll do this
   * in graphics timeout chunks, so that if graphics restarts while we're
   * delaying, and it requests timer events and overwrites ours, we wind up
   * with the identical timer event period to the one we're expecting anyway.
   */
  glk_request_timer_events (GLN_GRAPHICS_TIMEOUT);
  for (count = 0; count < GLN_GRAPHICS_TITLE_WAIT; count++)
    {
      gln_event_wait_2 (evtype_CharInput, evtype_Timer, &event);

      if (event.type == evtype_CharInput)
        break;
    }

  /*
   * While we waited, a Glk arrange or redraw event could have triggered
   * graphics into repainting, and using timers.  To handle this, stop timers
   * only if graphics is inactive.  If active, graphics will stop timers
   * itself when it finishes rendering.  We can't stop timers here while
   * graphics is active; that will hang the graphics "thread".
   */
  if (!gln_graphics_active)
    glk_request_timer_events (0);

  /* Cancel possible pending character event, and continue on. */
  glk_cancel_char_event (gln_main_window);
  gln_watchdog_tick ();
}


/*
 * os_show_bitmap()
 *
 * Called by the main interpreter when it wants us to display a picture.
 *
 * The function gets the picture bitmap, palette, and dimensions, and saves
 * them, and the picture id, in module variables for the background rendering
 * function.
 */
void
os_show_bitmap (int picture, int x, int y)
{
  Bitmap *bitmap;
  long picture_bytes;

  /*
   * If interpreter graphics are disabled, the only way we can get into here
   * is using #picture.  It seems that the interpreter won't always deliver
   * correct bitmaps with #picture when in text mode, so it's simplest here
   * if we just ignore those calls.
   */
  if (gln_graphics_interpreter_state != GLN_GRAPHICS_BITMAP_MODE)
    return;

  /* Ignore repeat calls for the currently displayed picture. */
  if (picture == gln_graphics_picture)
    return;

  /*
   * Get the core interpreter's bitmap for the requested picture.  If this
   * returns NULL, the picture doesn't exist, so ignore the call silently.
   */
  bitmap = DecodeBitmap (gln_graphics_bitmap_directory,
                         gln_graphics_bitmap_type, picture, x, y);
  if (!bitmap)
    return;

  /*
   * Note the last thing passed to os_show_bitmap, to avoid possible repaints
   * of the current picture.
   */
  gln_graphics_picture = picture;

  /* Calculate the picture size in bytes. */
  picture_bytes = bitmap->width * bitmap->height * sizeof (*bitmap->bitmap);

  /*
   * Save the picture details for the update code.  Here we take a complete
   * local copy of the bitmap, dimensions, and palette.  The core interpreter
   * may return a palette with fewer colors than our maximum, so unused local
   * palette entries are set to zero.
   */
  free (gln_graphics_bitmap);
  gln_graphics_bitmap = gln_malloc (picture_bytes);
  memcpy (gln_graphics_bitmap, bitmap->bitmap, picture_bytes);
  gln_graphics_width = bitmap->width;
  gln_graphics_height = bitmap->height;
  memset (gln_graphics_palette, 0, sizeof (gln_graphics_palette));
  memcpy (gln_graphics_palette, bitmap->palette,
          bitmap->npalette * sizeof (bitmap->palette[0]));

  /*
   * If graphics are enabled, both at the Glk level and in the core
   * interpreter, ensure the window is displayed, set the appropriate flags,
   * and start graphics update.  If they're not enabled, the picture details
   * will simply stick around in module variables until they are required.
   */
  if (gln_graphics_enabled
      && gln_graphics_interpreter_state == GLN_GRAPHICS_BITMAP_MODE)
    {
      /*
       * Ensure graphics on, then set the new picture flag and start the
       * updating "thread".  If this is the title picture, start special
       * handling.
       */
      if (gln_graphics_open ())
        {
          gln_graphics_new_picture = TRUE;
          gln_graphics_start ();

          if (picture == GLN_GRAPHICS_TITLE_PICTURE)
            gln_graphics_handle_title_picture ();
        }
    }
}


/*
 * gln_graphics_picture_is_available()
 *
 * Return TRUE if the graphics module data is loaded with a usable picture,
 * FALSE if there is no picture available to display.
 */
static int
gln_graphics_picture_is_available (void)
{
  return gln_graphics_bitmap != NULL;
}


/*
 * gln_graphics_get_picture_details()
 *
 * Return the width and height of the currently loaded picture.  The function
 * returns FALSE if no picture is loaded, otherwise TRUE, with picture details
 * in the return arguments.
 */
static int
gln_graphics_get_picture_details (int *width, int *height)
{
  if (gln_graphics_picture_is_available ())
    {
      if (width)
        *width = gln_graphics_width;
      if (height)
        *height = gln_graphics_height;

      return TRUE;
    }

  return FALSE;
}


/*
 * gln_graphics_get_rendering_details()
 *
 * Returns the type of bitmap in use (if any), as a string, the count of
 * colors in the picture, and a flag indicating if graphics is active (busy).
 * The function return FALSE if graphics is not enabled or if not being
 * displayed, otherwise TRUE with the bitmap type, color count, and active
 * flag in the return arguments.
 *
 * This function races with the graphics timeout, as it returns information
 * set up by the first timeout following a new picture.  There's a very
 * very small chance that it might win the race, in which case out-of-date
 * values are returned.
 */
static int
gln_graphics_get_rendering_details (const char **bitmap_type,
                                    int *color_count, int *is_active)
{
  if (gln_graphics_enabled && gln_graphics_are_displayed ())
    {
      /*
       * Convert the detected bitmap type into a string and return it.
       * A NULL bitmap string implies no bitmaps.
       */
      if (bitmap_type)
        {
          const char *return_type;

          switch (gln_graphics_bitmap_type)
            {
            case AMIGA_BITMAPS:
              return_type = "Amiga";
              break;
            case PC1_BITMAPS:
              return_type = "IBM PC(1)";
              break;
            case PC2_BITMAPS:
              return_type = "IBM PC(2)";
              break;
            case C64_BITMAPS:
              return_type = "Commodore 64";
              break;
            case BBC_BITMAPS:
              return_type = "BBC B";
              break;
            case CPC_BITMAPS:
              return_type = "Amstrad CPC/Spectrum";
              break;
            case MAC_BITMAPS:
              return_type = "Macintosh";
              break;
            case ST1_BITMAPS:
              return_type = "Atari ST(1)";
              break;
            case ST2_BITMAPS:
              return_type = "Atari ST(2)";
              break;
            case NO_BITMAPS:
            default:
              return_type = NULL;
              break;
            }

          *bitmap_type = return_type;
        }

      /*
       * Return the color count noted by timeouts on the first timeout
       * following a new picture.  We might return the one for the prior
       * picture.
       */
      if (color_count)
        *color_count = gln_graphics_color_count;

      /* Return graphics active flag. */
      if (is_active)
        *is_active = gln_graphics_active;

      return TRUE;
    }

  return FALSE;
}


/*
 * gln_graphics_interpreter_enabled()
 *
 * Return TRUE if it looks like interpreter graphics are turned on, FALSE
 * otherwise.
 */
static int
gln_graphics_interpreter_enabled (void)
{
  return gln_graphics_interpreter_state != GLN_GRAPHICS_OFF;
}


/*
 * gln_graphics_cleanup()
 *
 * Free memory resources allocated by graphics functions.  Called on game
 * end.
 */
static void
gln_graphics_cleanup (void)
{
  free (gln_graphics_bitmap);
  gln_graphics_bitmap = NULL;
  free (gln_graphics_off_screen);
  gln_graphics_off_screen = NULL;
  free (gln_graphics_on_screen);
  gln_graphics_on_screen = NULL;
  free (gln_graphics_bitmap_directory);
  gln_graphics_bitmap_directory = NULL;

  gln_graphics_bitmap_type = NO_BITMAPS;
  gln_graphics_picture = -1;
}


/*---------------------------------------------------------------------*/
/*  Glk port line drawing picture adapter functions                    */
/*---------------------------------------------------------------------*/

/*
 * Graphics color table.  These eight colors are selected into the four-
 * color palette by os_setcolour().  The standard Amiga palette is rather
 * over-vibrant, so to soften it a bit this table uses non-primary colors.
 */
static const gln_rgb_t GLN_LINEGRAPHICS_COLOR_TABLE[] = {
  { 47,  79,  79},  /* DarkSlateGray  [Black] */
  {238,  44,  44},  /* Firebrick2     [Red] */
  { 67, 205, 128},  /* SeaGreen3      [Green] */
  {238, 201,   0},  /* Gold2          [Yellow] */
  { 92, 172, 238},  /* SteelBlue2     [Blue] */
  {139,  87,  66},  /* LightSalmon4   [Brown] */
  {175, 238, 238},  /* PaleTurquoise  [Cyan] */
  {245, 245, 245},  /* WhiteSmoke     [White] */
};

/*
 * Structure of a Seed Fill segment entry, and a growable stack-based array
 * of segments pending fill.  When length exceeds size, size is increased
 * and the array grown.
 */
typedef struct
{
  int y;   /* Segment y coordinate */
  int xl;  /* Segment x left hand side coordinate */
  int xr;  /* Segment x right hand side coordinate */
  int dy;  /* Segment y delta */
} gln_linegraphics_segment_t;

static gln_linegraphics_segment_t * gln_linegraphics_fill_segments = NULL;
static int gln_linegraphics_fill_segments_allocation = 0,
           gln_linegraphics_fill_segments_length = 0;


/*
 * gln_linegraphics_create_context()
 *
 * Initialize a new constructed bitmap graphics context for line drawn
 * graphics.
 */
static void
gln_linegraphics_create_context (void)
{
  int width, height;
  long picture_bytes;

  /* Get the picture size, and calculate the bytes in the bitmap. */
  GetPictureSize (&width, &height);
  picture_bytes = width * height * sizeof (*gln_graphics_bitmap);

  /*
   * Destroy any current bitmap, and begin a fresh one.  Here we set the
   * bitmap and the palette to all zeroes; this equates to all black.
   */
  free (gln_graphics_bitmap);
  gln_graphics_bitmap = gln_malloc (picture_bytes);
  memset (gln_graphics_bitmap, 0, picture_bytes);
  gln_graphics_width = width;
  gln_graphics_height = height;
  memset (gln_graphics_palette, 0, sizeof (gln_graphics_palette));

  /* Set graphics picture number to -1; this is not a real game bitmap. */
  gln_graphics_picture = -1;
}


/*
 * gln_linegraphics_clear_context()
 *
 * Clear the complete graphical drawing area, setting all pixels to zero,
 * and resetting the palette to all black as well.
 */
static void
gln_linegraphics_clear_context (void)
{
  long picture_bytes;

  /* Get the picture size, and zero all bytes in the bitmap. */
  picture_bytes = gln_graphics_width
                  * gln_graphics_height * sizeof (*gln_graphics_bitmap);
  memset (gln_graphics_bitmap, 0, picture_bytes);

  /* Clear palette colors to all black. */
  memset (gln_graphics_palette, 0, sizeof (gln_graphics_palette));
}


/*
 * gln_linegraphics_set_palette_color()
 *
 * Copy the indicated main color table entry into the palette.
 */
static void
gln_linegraphics_set_palette_color (int colour, int index)
{
  const gln_rgb_t *entry;
  assert (colour < GLN_PALETTE_SIZE);
  assert (index < sizeof (GLN_LINEGRAPHICS_COLOR_TABLE)
                  / sizeof (GLN_LINEGRAPHICS_COLOR_TABLE[0]));

  /* Copy the color table entry to the constructed game palette. */
  entry = GLN_LINEGRAPHICS_COLOR_TABLE + index;
  gln_graphics_palette[colour].red   = entry->red;
  gln_graphics_palette[colour].green = entry->green;
  gln_graphics_palette[colour].blue  = entry->blue;
}


/*
 * gln_linegraphics_get_pixel()
 * gln_linegraphics_set_pixel()
 *
 * Return and set the bitmap pixel at x,y.
 */
static gln_byte
gln_linegraphics_get_pixel (int x, int y)
{
  assert (x >= 0 && x < gln_graphics_width
          && y >= 0 && y < gln_graphics_height);

  return gln_graphics_bitmap[y * gln_graphics_width + x];
}

static void
gln_linegraphics_set_pixel (int x, int y, gln_byte color)
{
  assert (x >= 0 && x < gln_graphics_width
          && y >= 0 && y < gln_graphics_height);

  gln_graphics_bitmap[y * gln_graphics_width + x] = color;
}


/*
 * gln_linegraphics_plot_clip()
 * gln_linegraphics_draw_line_if()
 *
 * Draw a line from x1,y1 to x2,y2 in colour1, where the existing pixel
 * colour is colour2.  The function uses Bresenham's algorithm.  The second
 * function, gln_graphics_plot_clip, is a line drawing helper; it handles
 * clipping, and the requirement to plot a point only if it matches colour2.
 */
static void
gln_linegraphics_plot_clip (int x, int y, int colour1, int colour2)
{
  /*
   * Clip the plot if the value is outside the context.  Otherwise, plot the
   * pixel as colour1 if it is currently colour2.
   */
  if (x >= 0 && x < gln_graphics_width && y >= 0 && y < gln_graphics_height)
    {
      if (gln_linegraphics_get_pixel (x, y) == colour2)
        gln_linegraphics_set_pixel (x, y, colour1);
    }
}

static void
gln_linegraphics_draw_line_if (int x1, int y1, int x2, int y2,
                               int colour1, int colour2)
{
  int x, y, dx, dy, incx, incy, balance;

  /* Ignore any odd request where there will be no colour changes. */
  if (colour1 == colour2)
    return;

  /* Normalize the line into deltas and increments. */
  if (x2 >= x1)
    {
      dx = x2 - x1;
      incx = 1;
    }
  else
    {
      dx = x1 - x2;
      incx = -1;
    }

  if (y2 >= y1)
    {
      dy = y2 - y1;
      incy = 1;
    }
  else
    {
      dy = y1 - y2;
      incy = -1;
    }

  /* Start at x1,y1. */
  x = x1;
  y = y1;

  /* Decide on a direction to progress in. */
  if (dx >= dy)
    {
      dy <<= 1;
      balance = dy - dx;
      dx <<= 1;

      /* Loop until we reach the end point of the line. */
      while (x != x2)
        {
          gln_linegraphics_plot_clip (x, y, colour1, colour2);
          if (balance >= 0)
            {
              y += incy;
              balance -= dx;
            }
          balance += dy;
          x += incx;
        }
      gln_linegraphics_plot_clip (x, y, colour1, colour2);
    }
  else
    {
      dx <<= 1;
      balance = dx - dy;
      dy <<= 1;

      /* Loop until we reach the end point of the line. */
      while (y != y2)
        {
          gln_linegraphics_plot_clip (x, y, colour1, colour2);
          if (balance >= 0)
            {
              x += incx;
              balance -= dy;
            }
          balance += dx;
          y += incy;
        }
      gln_linegraphics_plot_clip (x, y, colour1, colour2);
    }
}


/*
 * gln_linegraphics_push_fill_segment()
 * gln_linegraphics_pop_fill_segment()
 * gln_linegraphics_fill_4way_if()
 *
 * Area fill algorithm, set a region to colour1 if it is currently set to
 * colour2.  This function is a derivation of Paul Heckbert's Seed Fill,
 * from "Graphics Gems", Academic Press, 1990, which fills 4-connected
 * neighbors.
 * 
 * The main modification is to make segment stacks growable, through the
 * helper push and pop functions.  There is also a small adaptation to
 * check explicitly for color2, to meet the Level 9 API.
 */
static void
gln_linegraphics_push_fill_segment (int y, int xl, int xr, int dy)
{
  /* Clip points outside the graphics context. */
  if (!(y + dy < 0 || y + dy >= gln_graphics_height))
    {
      int length, allocation;

      length = ++gln_linegraphics_fill_segments_length;
      allocation = gln_linegraphics_fill_segments_allocation;

      /* Grow the segments stack if required, successively doubling. */
      if (length > allocation)
        {
          size_t bytes;

          allocation = allocation == 0 ? 1 : allocation << 1;

          bytes = allocation * sizeof (*gln_linegraphics_fill_segments);
          gln_linegraphics_fill_segments =
              gln_realloc (gln_linegraphics_fill_segments, bytes);
        }

      /* Push top of segments stack. */
      gln_linegraphics_fill_segments[length - 1].y  = y;
      gln_linegraphics_fill_segments[length - 1].xl = xl;
      gln_linegraphics_fill_segments[length - 1].xr = xr;
      gln_linegraphics_fill_segments[length - 1].dy = dy;

      /* Write back local dimensions copies. */
      gln_linegraphics_fill_segments_length = length;
      gln_linegraphics_fill_segments_allocation = allocation;
    }
}

static void
gln_linegraphics_pop_fill_segment (int *y, int *xl, int *xr, int *dy)
{
  int length;
  assert (gln_linegraphics_fill_segments_length > 0);

  length = --gln_linegraphics_fill_segments_length;

  /* Pop top of segments stack. */
  *y  = gln_linegraphics_fill_segments[length].y;
  *xl = gln_linegraphics_fill_segments[length].xl;
  *xr = gln_linegraphics_fill_segments[length].xr;
  *dy = gln_linegraphics_fill_segments[length].dy;
}

static void
gln_linegraphics_fill_4way_if (int x, int y, int colour1, int colour2)
{
  /* Ignore any odd request where there will be no colour changes. */
  if (colour1 == colour2)
    return;

  /* Clip fill requests to visible graphics region. */
  if (x >= 0 && x < gln_graphics_width && y >= 0 && y < gln_graphics_height)
    {
      int left, x1, x2, dy, x_lo, x_hi;

      /*
       * Level 9 API; explicit check for a match against colour2.  This also
       * covers the standard Seed Fill check that old pixel value should not
       * equal colour1, because of the color1 == colour2 comparison above.
       */
      if (gln_linegraphics_get_pixel (x, y) != colour2)
        return;

      /*
       * Set up inclusive window dimension to ease algorithm translation.
       * The original worked with inclusive rectangle limits.
       */
      x_lo = 0;
      x_hi = gln_graphics_width - 1;

      /*
       * The first of these is "needed in some cases", the second is the seed
       * segment, popped first.
       */
      gln_linegraphics_push_fill_segment (y, x, x, 1);
      gln_linegraphics_push_fill_segment (y + 1, x, x, -1);

      while (gln_linegraphics_fill_segments_length > 0)
        {
          /* Pop segment off stack and add delta to y coord. */
          gln_linegraphics_pop_fill_segment (&y, &x1, &x2, &dy);
          y += dy;

          /*
           * Segment of scan line y-dy for x1<=x<=x2 was previously filled,
           * now explore adjacent pixels in scan line y.
           */
          for (x = x1;
               x >= x_lo && gln_linegraphics_get_pixel (x, y) == colour2;
               x--)
            {
              gln_linegraphics_set_pixel (x, y, colour1);
            }

          if (x >= x1)
            goto skip;

          left = x + 1;
          if (left < x1)
            {
              /* Leak on left? */
              gln_linegraphics_push_fill_segment (y, left, x1 - 1, -dy);
            }

          x = x1 + 1;
          do
            {
              for (;
                   x <= x_hi && gln_linegraphics_get_pixel (x, y) == colour2;
                   x++)
                {
                  gln_linegraphics_set_pixel (x, y, colour1);
                }

              gln_linegraphics_push_fill_segment (y, left, x - 1, dy);

              if (x > x2 + 1)
                {
                  /* Leak on right? */
                  gln_linegraphics_push_fill_segment (y, x2 + 1, x - 1, -dy);
                }

skip:         for (x++;
                   x <= x2 && gln_linegraphics_get_pixel (x, y) != colour2;
                   x++)
                ;

              left = x;
            }
          while (x <= x2);
        }
    }
}


/*
 * os_cleargraphics()
 * os_setcolour()
 * os_drawline()
 * os_fill()
 *
 * Interpreter entry points for line drawing graphics.  All calls to these
 * are ignored if line drawing mode is not set.
 */
void
os_cleargraphics (void)
{
  if (gln_graphics_interpreter_state == GLN_GRAPHICS_LINE_MODE)
    gln_linegraphics_clear_context ();
}

void
os_setcolour (int colour, int index)
{
  if (gln_graphics_interpreter_state == GLN_GRAPHICS_LINE_MODE)
    gln_linegraphics_set_palette_color (colour, index);
}

void
os_drawline (int x1, int y1, int x2, int y2, int colour1, int colour2)
{
  if (gln_graphics_interpreter_state == GLN_GRAPHICS_LINE_MODE)
    gln_linegraphics_draw_line_if (x1, y1, x2, y2, colour1, colour2);
}

void
os_fill (int x, int y, int colour1, int colour2)
{
  if (gln_graphics_interpreter_state == GLN_GRAPHICS_LINE_MODE)
    gln_linegraphics_fill_4way_if (x, y, colour1, colour2);
}


/*
 * gln_linegraphics_process()
 *
 * Process as many graphics opcodes as are available, constructing the
 * resulting image as a bitmap.  When complete, treat as normal bitmaps.
 */
static void
gln_linegraphics_process (void)
{
  /*
   * If interpreter graphics are not set to line mode, ignore any call that
   * arrives here.
   */
  if (gln_graphics_interpreter_state == GLN_GRAPHICS_LINE_MODE)
    {
      int opcodes_count;

      /* Run all the available graphics opcodes. */
      for (opcodes_count = 0; RunGraphics (); )
        {
          opcodes_count++;
          glk_tick ();
        }

      /* 
       * If graphics is enabled and we created an image with graphics
       * opcodes above, open a graphics window and start bitmap display.
       */
      if (gln_graphics_enabled && opcodes_count > 0)
        {
          if (gln_graphics_open ())
            {
              /* Set the new picture flag, and start the updating "thread". */
              gln_graphics_new_picture = TRUE;
              gln_graphics_start ();
            }
        }
    }
}


/*
 * gln_linegraphics_cleanup()
 *
 * Free memory resources allocated by line graphics functions.  Called on
 * game end.
 */
static void
gln_linegraphics_cleanup (void)
{
  free (gln_linegraphics_fill_segments);
  gln_linegraphics_fill_segments = NULL;

  gln_linegraphics_fill_segments_allocation = 0;
  gln_linegraphics_fill_segments_length = 0;
}


/*---------------------------------------------------------------------*/
/*  Glk picture dispatch (bitmap or line), and timer arbitration       */
/*---------------------------------------------------------------------*/

/*
 * Note of the current set graphics mode, to detect changes in mode from
 * the core interpreter.
 */
static int gln_graphics_current_mode = -1;

/* Note indicating if the graphics "thread" is temporarily suspended. */
static int gln_graphics_suspended = FALSE;


/*
 * os_graphics()
 *
 * Called by the main interpreter to turn graphics on and off.  Mode 0
 * turns graphics off, mode 1 is line drawing graphics, and mode 2 is
 * bitmap graphics.
 *
 * This function tracks the current state of interpreter graphics setting
 * using gln_graphics_interpreter_state.
 */
void
os_graphics (int mode)
{
  /* Ignore the call unless it changes the graphics mode. */
  if (mode != gln_graphics_current_mode)
    {
      /* Set tracked interpreter state given the input mode. */
      switch (mode)
        {
        case 0:
          gln_graphics_interpreter_state = GLN_GRAPHICS_OFF;
          break;

        case 1:
          gln_graphics_interpreter_state = GLN_GRAPHICS_LINE_MODE;
          break;

        case 2:
          /* If no graphics bitmaps were detected, ignore this call. */
          if (!gln_graphics_bitmap_directory
              || gln_graphics_bitmap_type == NO_BITMAPS)
            return;

          gln_graphics_interpreter_state = GLN_GRAPHICS_BITMAP_MODE;
          break;
        }

      /* Given the interpreter state, update graphics activities. */
      switch (gln_graphics_interpreter_state)
        {
        case GLN_GRAPHICS_OFF:

          /* If currently displaying graphics, stop and close window. */
          if (gln_graphics_enabled && gln_graphics_are_displayed ())
            {
              gln_graphics_stop ();
              gln_graphics_close ();
            }
          break;

        case GLN_GRAPHICS_LINE_MODE:
        case GLN_GRAPHICS_BITMAP_MODE:

          /* Create a new graphics context on switch to line mode. */
          if (gln_graphics_interpreter_state == GLN_GRAPHICS_LINE_MODE)
            gln_linegraphics_create_context ();

          /*
           * If we have a picture loaded already, restart graphics. If not,
           * we'll delay this until one is supplied by a call to
           * os_show_bitmap().
           */
          if (gln_graphics_enabled && gln_graphics_bitmap)
            {
              if (gln_graphics_open ())
                gln_graphics_restart ();
            }
          break;
        }

      /* Note the current mode so changes can be detected. */
      gln_graphics_current_mode = mode;
    }
}


/*
 * gln_arbitrate_request_timer_events()
 *
 * Shim function for glk_request_timer_events(), this function should be
 * called by other functional areas in place of the main timer event setting
 * function.  It suspends graphics if busy when setting timer events, and
 * resumes graphics if necessary when clearing timer events.
 *
 * On resuming, it calls the graphics timeout function to simulate the
 * timeout that has (probably) been missed.  This also ensures that tight
 * loops that enable then disable timers using this function don't lock out
 * the graphics completely.
 *
 * Use only in paired calls, the first non-zero, the second zero, and use
 * no graphics functions between calls.
 */
static void
gln_arbitrate_request_timer_events (glui32 millisecs)
{
  if (millisecs > 0)
    {
      /* Setting timer events; suspend graphics if currently active. */
      if (gln_graphics_active)
        {
          gln_graphics_suspended = TRUE;
          gln_graphics_stop ();
        }

      /* Set timer events as requested. */
      glk_request_timer_events (millisecs);
    }
  else
    {
      /*
       * Resume graphics if currently suspended, otherwise cancel timer
       * events as requested by the caller.
       */
      if (gln_graphics_suspended)
        {
          gln_graphics_suspended = FALSE;
          gln_graphics_start ();

          /* Simulate the "missed" graphics timeout. */
          gln_graphics_timeout ();
        }
      else
        glk_request_timer_events (0);
    }
}


/*---------------------------------------------------------------------*/
/*  Glk port infinite loop detection functions                         */
/*---------------------------------------------------------------------*/

/* Short timeout to wait purely in order to get the display updated. */
static const glui32 GLN_WATCHDOG_FIXUP = 50;

/*
 * Timestamp of the last watchdog tick call, and timeout.  This is used to
 * monitor the elapsed time since the interpreter made an I/O call.  If it
 * remains silent for long enough, set by the timeout, we'll offer the
 * option to end the game.  A timeout of zero disables the watchdog.
 */
static time_t gln_watchdog_monitor = 0;
static double gln_watchdog_timeout_secs = 0.0;

/*
 * To save thrashing in time(), we want to check for timeouts less frequently
 * than we're polled.  Here's the control for that.
 */
static int gln_watchdog_check_period = 0,
           gln_watchdog_check_counter = 0;


/*
 * gln_watchdog_start()
 * gln_watchdog_stop()
 *
 * Start and stop watchdog monitoring.
 */
static void
gln_watchdog_start (int timeout, int period)
{
  assert (timeout > 0 && period > 0);

  gln_watchdog_timeout_secs = (double) timeout;
  gln_watchdog_check_period = period;
  gln_watchdog_check_counter = period;
  gln_watchdog_monitor = time (NULL);
}

static void
gln_watchdog_stop (void)
{
  gln_watchdog_timeout_secs = 0;
}


/*
 * gln_watchdog_tick()
 *
 * Set the watchdog timestamp to the current system time.
 *
 * This function should be called just before almost every os_* function
 * returns to the interpreter, as a means of timing how long the interpreter
 * dwells in running game code.
 */
static void
gln_watchdog_tick (void)
{
  gln_watchdog_monitor = time (NULL);
}


/*
 * gln_watchdog_has_timed_out()
 *
 * Check to see if too much time has elapsed since the last tick.  If it has,
 * offer the option to stop the game, and if accepted, return TRUE.  Otherwise,
 * if no timeout, or if the watchdog is disabled, return FALSE.
 *
 * This function only checks every N calls; it's called extremely frequently
 * from opcode handling, and will thrash in time() if it checks on each call.
 */
static int
gln_watchdog_has_timed_out (void)
{
  /* If loop detection is off or the timeout is set to zero, do nothing. */
  if (gln_loopcheck_enabled && gln_watchdog_timeout_secs > 0)
    {
      time_t now;
      double delta_time;

      /*
       * Wait until we've seen enough calls to make a timeout check.  If we
       * haven't, return FALSE, otherwise reset the counter and continue.
       */
      if (--gln_watchdog_check_counter > 0)
        return FALSE;
      else
        gln_watchdog_check_counter = gln_watchdog_check_period;

      /*
       * Determine how much time has passed, and offer to end the game if it
       * exceeds the allowable timeout.
       */
      now = time (NULL);
      delta_time = difftime (now, gln_watchdog_monitor);

      if (delta_time >= gln_watchdog_timeout_secs)
        {
          if (gln_confirm ("\nThe game may be in an infinite loop.  Do you"
                           " want to stop it? [Y or N] "))
            {
              gln_watchdog_tick ();
              return TRUE;
            }

          /*
           * If we have timers, set a really short timeout and let it expire.
           * This is to force a display update with the response of the
           * confirm -- without this, we may not get a screen update for a
           * while since at this point the game isn't, by definition, doing
           * any input or output.  If we don't have timers, no biggie.
           */
          if (glk_gestalt (gestalt_Timer, 0))
            {
              event_t event;

              gln_arbitrate_request_timer_events (GLN_WATCHDOG_FIXUP);
              gln_event_wait (evtype_Timer, &event);
              gln_arbitrate_request_timer_events (0);
            }

          /* Reset the monitor and drop into FALSE return -- stop rejected. */
          gln_watchdog_tick ();
        }
    }

  /* No timeout indicated, or offer rejected by the user. */
  return FALSE;
}


/*---------------------------------------------------------------------*/
/*  Glk port status line functions                                     */
/*---------------------------------------------------------------------*/

/* Default width used for non-windowing Glk status lines. */
static const int GLN_DEFAULT_STATUS_WIDTH = 74;


/*
 * gln_status_update()
 *
 * Update the information in the status window with the current contents of
 * the current game identity string, or a default string if no game identity
 * could be established.
 */
static void
gln_status_update (void)
{
  glui32 width, height;
  assert (gln_status_window);

  glk_window_get_size (gln_status_window, &width, &height);
  if (height > 0)
    {
      const char *game_name;

      glk_window_clear (gln_status_window);
      glk_window_move_cursor (gln_status_window, 0, 0);
      glk_set_window (gln_status_window);

      /*
       * Try to establish a game identity to display; if none, use a standard
       * message instead.
       */
      game_name = gln_gameid_get_game_name ();
      glk_put_string (game_name ? (char *) game_name
                                : "Glk Level 9 version 5.1");

      glk_set_window (gln_main_window);
    }
}


/*
 * gln_status_print()
 *
 * Print the current contents of the game identity out in the main window,
 * if it has changed since the last call.  This is for non-windowing Glk
 * libraries.
 *
 * To save memory management hassles, this function uses the CRC functions
 * to detect changes of game identity string, and gambles a little on the
 * belief that two games' strings won't have the same CRC.
 */
static void
gln_status_print (void)
{
  static int is_initialized = FALSE;
  static gln_uint16 crc = 0;

  const char *game_name;

  /* Get the current game name, and do nothing if none available. */
  game_name = gln_gameid_get_game_name ();
  if (game_name)
    {
      gln_uint16 new_crc;

     /*
      * If not the first call and the game identity string has not changed,
      * again, do nothing.
      */
      new_crc = gln_get_buffer_crc (game_name, strlen (game_name), 0);
      if (!is_initialized || new_crc != crc)
        {
          int index;

#if !(defined(GARGLK) || defined(SPATTERLIGHT))
          /* Set fixed width font to try to preserve status line formatting. */
          glk_set_style (style_Preformatted);
#endif

          /* Bracket, and output the extracted game name. */
          glk_put_string ("[ ");
          glk_put_string ((char *) game_name);

          for (index = strlen (game_name);
               index <= GLN_DEFAULT_STATUS_WIDTH; index++)
            glk_put_char (' ');
          glk_put_string (" ]\n");

          crc = new_crc;
          is_initialized = TRUE;
        }
    }
}


/*
 * gln_status_notify()
 *
 * Front end function for updating status.  Either updates the status window
 * or prints the status line to the main window.
 */
static void
gln_status_notify (void)
{
  if (gln_status_window)
    gln_status_update ();
  else
    gln_status_print ();
}


/*
 * gln_status_redraw()
 *
 * Redraw the contents of any status window with the buffered status string.
 * This function should be called on the appropriate Glk window resize and
 * arrange events.
 */
static void
gln_status_redraw (void)
{
  if (gln_status_window)
    {
      winid_t parent;

      /*
       * Rearrange the status window, without changing its actual arrangement
       * in any way.  This is a hack to work round incorrect window repainting
       * in Xglk; it forces a complete repaint of affected windows on Glk
       * window resize and arrange events, and works in part because Xglk
       * doesn't check for actual arrangement changes in any way before
       * invalidating its windows.  The hack should be harmless to Glk
       * libraries other than Xglk, moreover, we're careful to activate it
       * only on resize and arrange events.
       */
      parent = glk_window_get_parent (gln_status_window);
      glk_window_set_arrangement (parent,
                                  winmethod_Above | winmethod_Fixed, 1, NULL);

      gln_status_update ();
    }
}


/*---------------------------------------------------------------------*/
/*  Glk port output functions                                          */
/*---------------------------------------------------------------------*/

/*
 * Flag for if the user entered "help" as their last input, or if hints have
 * been silenced as a result of already using a Glk command.
 */
static int gln_help_requested = FALSE,
           gln_help_hints_silenced = FALSE;

/*
 * Output buffer.  We receive characters one at a time, and it's a bit
 * more efficient for everyone if we buffer them, and output a complete
 * string on a flush call.
 */
static char *gln_output_buffer = NULL;
static int gln_output_allocation = 0,
           gln_output_length = 0;

/*
 * Output activity flag.  Set when os_printchar() is called, and queried
 * periodically by os_readchar().  Helps os_readchar() judge whether it must
 * request input, or when it's being used as a crude scroll control.
 */
static int gln_output_activity = FALSE;

/*
 * Flag to indicate if the last buffer flushed looked like it ended in a
 * "> " prompt.  Some later games switch to this mode after a while, and
 * it's nice not to duplicate this prompt with our own.
 */
static int gln_output_prompt = FALSE;


/*
 * gln_output_notify()
 *
 * Register recent text output from the interpreter.  This function is
 * called by os_printchar().
 */
static void
gln_output_notify (void)
{
  gln_output_activity = TRUE;
}


/*
 * gln_recent_output()
 *
 * Return TRUE if the interpreter has recently output text, FALSE otherwise.
 * Clears the flag, so that more output text is required before the next
 * call returns TRUE.
 */
static int
gln_recent_output (void)
{
  int result;

  result = gln_output_activity;
  gln_output_activity = FALSE;

  return result;
}


/*
 * gln_output_register_help_request()
 * gln_output_silence_help_hints()
 * gln_output_provide_help_hint()
 *
 * Register a request for help, and print a note of how to get Glk command
 * help from the interpreter unless silenced.
 */
static void
gln_output_register_help_request (void)
{
  gln_help_requested = TRUE;
}

static void
gln_output_silence_help_hints (void)
{
  gln_help_hints_silenced = TRUE;
}

static void
gln_output_provide_help_hint (void)
{
  if (gln_help_requested && !gln_help_hints_silenced)
    {
      glk_set_style (style_Emphasized);
      glk_put_string ("[Try 'glk help' for help on special interpreter"
                      " commands]\n");

      gln_help_requested = FALSE;
      glk_set_style (style_Normal);
    }
}


/*
 * gln_game_prompted()
 *
 * Return TRUE if the last game output appears to have been a "> " prompt.
 * Once called, the flag is reset to FALSE, and requires more game output
 * to set it again.
 */
static int
gln_game_prompted (void)
{
  int result;

  result = gln_output_prompt;
  gln_output_prompt = FALSE;

  return result;
}


/*
 * gln_detect_game_prompt()
 *
 * See if the last non-newline-terminated line in the output buffer seems
 * to be a prompt, and set the game prompted flag if it does, otherwise
 * clear it.
 */
static void
gln_detect_game_prompt (void)
{
  int index;

  gln_output_prompt = FALSE;

  /*
   * Search for a prompt across any last unterminated buffered line; a prompt
   * is any non-space character on that line.
   */
  for (index = gln_output_length - 1;
       index >= 0 && gln_output_buffer[index] != '\n'; index--)
    {
      if (gln_output_buffer[index] != ' ')
        {
          gln_output_prompt = TRUE;
          break;
        }
    }
}


/*
 * gln_output_delete()
 *
 * Delete all buffered output text.  Free all malloc'ed buffer memory, and
 * return the buffer variables to their initial values.
 */
static void
gln_output_delete (void)
{
  free (gln_output_buffer);
  gln_output_buffer = NULL;
  gln_output_allocation = gln_output_length = 0;
}


/*
 * gln_output_flush()
 *
 * Flush any buffered output text to the Glk main window, and clear the
 * buffer.  Check in passing for game prompts that duplicate our's.
 */
static void
gln_output_flush (void)
{
  assert (glk_stream_get_current ());

  if (gln_output_length > 0)
    {
      /*
       * See if the game issued a standard prompt, then print the buffer to
       * the main window.  If providing a help hint, position that before
       * the game's prompt (if any).
       */
      gln_detect_game_prompt ();

      if (gln_output_prompt)
        {
          int index;

          for (index = gln_output_length - 1;
               index >= 0 && gln_output_buffer[index] != '\n'; )
            index--;

          glk_put_buffer (gln_output_buffer, index + 1);
          gln_output_provide_help_hint ();
          glk_put_buffer (gln_output_buffer + index + 1,
                          gln_output_length - index - 1);
        }
      else
        {
          glk_put_buffer (gln_output_buffer, gln_output_length);
          gln_output_provide_help_hint ();
        }

      gln_output_delete ();
    }
}


/*
 * os_printchar()
 *
 * Buffer a character for eventual printing to the main window.
 */
void
os_printchar (char c)
{
  int bytes;
  assert (gln_output_length <= gln_output_allocation);

  /* Grow the output buffer if necessary. */
  for (bytes = gln_output_allocation; bytes < gln_output_length + 1; )
    bytes = bytes == 0 ? 1 : bytes << 1;

  if (bytes > gln_output_allocation)
    {
      gln_output_buffer = gln_realloc (gln_output_buffer, bytes);
      gln_output_allocation = bytes;
    }

  /*
   * Add the character to the buffer, handling return as a newline, and
   * note that the game created some output.
   */
  gln_output_buffer[gln_output_length++] = (c == '\r' ? '\n' : c);
  gln_output_notify ();
}


/*
 * gln_styled_string()
 * gln_styled_char()
 * gln_standout_string()
 * gln_standout_char()
 * gln_normal_string()
 * gln_normal_char()
 * gln_header_string()
 * gln_banner_string()
 *
 * Convenience functions to print strings in assorted styles.  A standout
 * string is one that hints that it's from the interpreter, not the game.
 */
static void
gln_styled_string (glui32 style, const char *message)
{
  assert (message);

  glk_set_style (style);
  glk_put_string ((char *) message);
  glk_set_style (style_Normal);
}

static void
gln_styled_char (glui32 style, char c)
{
  char buffer[2];

  buffer[0] = c;
  buffer[1] = '\0';
  gln_styled_string (style, buffer);
}

static void
gln_standout_string (const char *message)
{
  gln_styled_string (style_Emphasized, message);
}

static void
gln_standout_char (char c)
{
  gln_styled_char (style_Emphasized, c);
}

static void
gln_normal_string (const char *message)
{
  gln_styled_string (style_Normal, message);
}

static void
gln_normal_char (char c)
{
  gln_styled_char (style_Normal, c);
}

static void
gln_header_string (const char *message)
{
  gln_styled_string (style_Header, message);
}

static void
gln_banner_string (const char *message)
{
  gln_styled_string (style_Subheader, message);
}


/*
 * os_flush()
 *
 * Handle a core interpreter call to flush the output buffer.  Because Glk
 * only flushes its buffers and displays text on glk_select(), we can ignore
 * these calls as long as we call glk_output_flush() when reading line or
 * character input.
 *
 * Taking os_flush() at face value can cause game text to appear before status
 * line text where we are working with a non-windowing Glk, so it's best
 * ignored where we can.
 */
void
os_flush (void)
{
}


/*---------------------------------------------------------------------*/
/*  Glk command escape functions                                       */
/*---------------------------------------------------------------------*/

/*
 * gln_command_script()
 *
 * Turn game output scripting (logging) on and off.
 */
static void
gln_command_script (const char *argument)
{
  assert (argument);

  if (gln_strcasecmp (argument, "on") == 0)
    {
      frefid_t fileref;

      if (gln_transcript_stream)
        {
          gln_normal_string ("Glk transcript is already on.\n");
          return;
        }

      fileref = glk_fileref_create_by_prompt (fileusage_Transcript
                                              | fileusage_TextMode,
                                              filemode_WriteAppend, 0);
      if (!fileref)
        {
          gln_standout_string ("Glk transcript failed.\n");
          return;
        }

      gln_transcript_stream = glk_stream_open_file (fileref,
                                                     filemode_WriteAppend, 0);
      glk_fileref_destroy (fileref);
      if (!gln_transcript_stream)
        {
          gln_standout_string ("Glk transcript failed.\n");
          return;
        }

      glk_window_set_echo_stream (gln_main_window, gln_transcript_stream);

      gln_normal_string ("Glk transcript is now on.\n");
    }

  else if (gln_strcasecmp (argument, "off") == 0)
    {
      if (!gln_transcript_stream)
        {
          gln_normal_string ("Glk transcript is already off.\n");
          return;
        }

      glk_stream_close (gln_transcript_stream, NULL);
      gln_transcript_stream = NULL;

      glk_window_set_echo_stream (gln_main_window, NULL);

      gln_normal_string ("Glk transcript is now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gln_normal_string ("Glk transcript is ");
      gln_normal_string (gln_transcript_stream ? "on" : "off");
      gln_normal_string (".\n");
    }

  else
    {
      gln_normal_string ("Glk transcript can be ");
      gln_standout_string ("on");
      gln_normal_string (", or ");
      gln_standout_string ("off");
      gln_normal_string (".\n");
    }
}


/*
 * gln_command_inputlog()
 *
 * Turn game input logging on and off.
 */
static void
gln_command_inputlog (const char *argument)
{
  assert (argument);

  if (gln_strcasecmp (argument, "on") == 0)
    {
      frefid_t fileref;

      if (gln_inputlog_stream)
        {
          gln_normal_string ("Glk input logging is already on.\n");
          return;
        }

      fileref = glk_fileref_create_by_prompt (fileusage_InputRecord
                                              | fileusage_BinaryMode,
                                              filemode_WriteAppend, 0);
      if (!fileref)
        {
          gln_standout_string ("Glk input logging failed.\n");
          return;
        }

      gln_inputlog_stream = glk_stream_open_file (fileref,
                                                   filemode_WriteAppend, 0);
      glk_fileref_destroy (fileref);
      if (!gln_inputlog_stream)
        {
          gln_standout_string ("Glk input logging failed.\n");
          return;
        }

      gln_normal_string ("Glk input logging is now on.\n");
    }

  else if (gln_strcasecmp (argument, "off") == 0)
    {
      if (!gln_inputlog_stream)
        {
          gln_normal_string ("Glk input logging is already off.\n");
          return;
        }

      glk_stream_close (gln_inputlog_stream, NULL);
      gln_inputlog_stream = NULL;

      gln_normal_string ("Glk input log is now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gln_normal_string ("Glk input logging is ");
      gln_normal_string (gln_inputlog_stream ? "on" : "off");
      gln_normal_string (".\n");
    }

  else
    {
      gln_normal_string ("Glk input logging can be ");
      gln_standout_string ("on");
      gln_normal_string (", or ");
      gln_standout_string ("off");
      gln_normal_string (".\n");
    }
}


/*
 * gln_command_readlog()
 *
 * Set the game input log, to read input from a file.
 */
static void
gln_command_readlog (const char *argument)
{
  assert (argument);

  if (gln_strcasecmp (argument, "on") == 0)
    {
      frefid_t fileref;

      if (gln_readlog_stream)
        {
          gln_normal_string ("Glk read log is already on.\n");
          return;
        }

      fileref = glk_fileref_create_by_prompt (fileusage_InputRecord
                                              | fileusage_BinaryMode,
                                              filemode_Read, 0);
      if (!fileref)
        {
          gln_standout_string ("Glk read log failed.\n");
          return;
        }

      if (!glk_fileref_does_file_exist (fileref))
        {
          glk_fileref_destroy (fileref);
          gln_standout_string ("Glk read log failed.\n");
          return;
        }

      gln_readlog_stream = glk_stream_open_file (fileref, filemode_Read, 0);
      glk_fileref_destroy (fileref);
      if (!gln_readlog_stream)
        {
          gln_standout_string ("Glk read log failed.\n");
          return;
        }

      gln_normal_string ("Glk read log is now on.\n");
    }

  else if (gln_strcasecmp (argument, "off") == 0)
    {
      if (!gln_readlog_stream)
        {
          gln_normal_string ("Glk read log is already off.\n");
          return;
        }

      glk_stream_close (gln_readlog_stream, NULL);
      gln_readlog_stream = NULL;

      gln_normal_string ("Glk read log is now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gln_normal_string ("Glk read log is ");
      gln_normal_string (gln_readlog_stream ? "on" : "off");
      gln_normal_string (".\n");
    }

  else
    {
      gln_normal_string ("Glk read log can be ");
      gln_standout_string ("on");
      gln_normal_string (", or ");
      gln_standout_string ("off");
      gln_normal_string (".\n");
    }
}


/*
 * gln_command_abbreviations()
 *
 * Turn abbreviation expansions on and off.
 */
static void
gln_command_abbreviations (const char *argument)
{
  assert (argument);

  if (gln_strcasecmp (argument, "on") == 0)
    {
      if (gln_abbreviations_enabled)
        {
          gln_normal_string ("Glk abbreviation expansions are already on.\n");
          return;
        }

      gln_abbreviations_enabled = TRUE;
      gln_normal_string ("Glk abbreviation expansions are now on.\n");
    }

  else if (gln_strcasecmp (argument, "off") == 0)
    {
      if (!gln_abbreviations_enabled)
        {
          gln_normal_string ("Glk abbreviation expansions are already off.\n");
          return;
        }

      gln_abbreviations_enabled = FALSE;
      gln_normal_string ("Glk abbreviation expansions are now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gln_normal_string ("Glk abbreviation expansions are ");
      gln_normal_string (gln_abbreviations_enabled ? "on" : "off");
      gln_normal_string (".\n");
    }

  else
    {
      gln_normal_string ("Glk abbreviation expansions can be ");
      gln_standout_string ("on");
      gln_normal_string (", or ");
      gln_standout_string ("off");
      gln_normal_string (".\n");
    }
}


/*
 * gln_command_graphics()
 *
 * Enable or disable graphics more permanently than is done by the main
 * interpreter.  Also, print out a few brief details about the graphics
 * state of the program.
 */
static void
gln_command_graphics (const char *argument)
{
  assert (argument);

  if (!gln_graphics_possible)
    {
      gln_normal_string ("Glk graphics are not available.\n");
      return;
    }

  if (gln_strcasecmp (argument, "on") == 0)
    {
      if (gln_graphics_enabled)
        {
          gln_normal_string ("Glk graphics are already on.\n");
          return;
        }

      gln_graphics_enabled = TRUE;

      /* If a picture is loaded, call the restart function to repaint it. */
      if (gln_graphics_picture_is_available ())
        {
          if (!gln_graphics_open ())
            {
              gln_normal_string ("Glk graphics error.\n");
              return;
            }
          gln_graphics_restart ();
        }

      gln_normal_string ("Glk graphics are now on.\n");
    }

  else if (gln_strcasecmp (argument, "off") == 0)
    {
      if (!gln_graphics_enabled)
        {
          gln_normal_string ("Glk graphics are already off.\n");
          return;
        }

      /*
       * Set graphics to disabled, and stop any graphics processing.  Close
       * the graphics window.
       */
      gln_graphics_enabled = FALSE;
      gln_graphics_stop ();
      gln_graphics_close ();

      gln_normal_string ("Glk graphics are now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gln_normal_string ("Glk graphics are available,");
      gln_normal_string (gln_graphics_enabled
                         ? " and enabled.\n" : " but disabled.\n");

      if (gln_graphics_picture_is_available ())
        {
          int width, height;

          if (gln_graphics_get_picture_details (&width, &height))
            {
              char buffer[16];

              gln_normal_string ("There is a picture loaded, ");

              sprintf (buffer, "%d", width);
              gln_normal_string (buffer);
              gln_normal_string (" by ");

              sprintf (buffer, "%d", height);
              gln_normal_string (buffer);

              gln_normal_string (" pixels.\n");
            }
        }

      if (!gln_graphics_interpreter_enabled ())
        gln_normal_string ("Interpreter graphics are disabled.\n");

      if (gln_graphics_enabled && gln_graphics_are_displayed ())
        {
          const char *bitmap_type;
          int color_count, is_active;

          if (gln_graphics_get_rendering_details (&bitmap_type,
                                                  &color_count, &is_active))
            {
              char buffer[16];

              gln_normal_string ("Graphics are ");
              gln_normal_string (is_active ? "active, " : "displayed, ");

              sprintf (buffer, "%d", color_count);
              gln_normal_string (buffer);
              gln_normal_string (" colours");

              if (bitmap_type)
                {
                  gln_normal_string (", ");
                  gln_normal_string (bitmap_type);
                  gln_normal_string (" bitmaps");
                }
              gln_normal_string (".\n");
            }
          else
            gln_normal_string ("Graphics are being displayed.\n");
        }

      if (gln_graphics_enabled && !gln_graphics_are_displayed ())
        gln_normal_string ("Graphics are not being displayed.\n");
    }

  else
    {
      gln_normal_string ("Glk graphics can be ");
      gln_standout_string ("on");
      gln_normal_string (", or ");
      gln_standout_string ("off");
      gln_normal_string (".\n");
    }
}


/*
 * gln_command_loopchecks()
 *
 * Turn loop checking (for game infinite loops) on and off.
 */
static void
gln_command_loopchecks (const char *argument)
{
  assert (argument);

  if (gln_strcasecmp (argument, "on") == 0)
    {
      if (gln_loopcheck_enabled)
        {
          gln_normal_string ("Glk loop detection is already on.\n");
          return;
        }

      gln_loopcheck_enabled = TRUE;
      gln_normal_string ("Glk loop detection is now on.\n");
    }

  else if (gln_strcasecmp (argument, "off") == 0)
    {
      if (!gln_loopcheck_enabled)
        {
          gln_normal_string ("Glk loop detection is already off.\n");
          return;
        }

      gln_loopcheck_enabled = FALSE;
      gln_normal_string ("Glk loop detection is now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gln_normal_string ("Glk loop detection is ");
      gln_normal_string (gln_loopcheck_enabled ? "on" : "off");
      gln_normal_string (".\n");
    }

  else
    {
      gln_normal_string ("Glk loop detection can be ");
      gln_standout_string ("on");
      gln_normal_string (", or ");
      gln_standout_string ("off");
      gln_normal_string (".\n");
    }
}


/*
 * gln_command_locals()
 *
 * Turn local interpretation of "quit" etc. on and off.
 */
static void
gln_command_locals (const char *argument)
{
  assert (argument);

  if (gln_strcasecmp (argument, "on") == 0)
    {
      if (gln_intercept_enabled)
        {
          gln_normal_string ("Glk local commands are already on.\n");
          return;
        }

      gln_intercept_enabled = TRUE;
      gln_normal_string ("Glk local commands are now on.\n");
    }

  else if (gln_strcasecmp (argument, "off") == 0)
    {
      if (!gln_intercept_enabled)
        {
          gln_normal_string ("Glk local commands are already off.\n");
          return;
        }

      gln_intercept_enabled = FALSE;
      gln_normal_string ("Glk local commands are now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gln_normal_string ("Glk local commands are ");
      gln_normal_string (gln_intercept_enabled ? "on" : "off");
      gln_normal_string (".\n");
    }

  else
    {
      gln_normal_string ("Glk local commands can be ");
      gln_standout_string ("on");
      gln_normal_string (", or ");
      gln_standout_string ("off");
      gln_normal_string (".\n");
    }
}


/*
 * gln_command_prompts()
 *
 * Turn the extra "> " prompt output on and off.
 */
static void
gln_command_prompts (const char *argument)
{
  assert (argument);

  if (gln_strcasecmp (argument, "on") == 0)
    {
      if (gln_prompt_enabled)
        {
          gln_normal_string ("Glk extra prompts are already on.\n");
          return;
        }

      gln_prompt_enabled = TRUE;
      gln_normal_string ("Glk extra prompts are now on.\n");

      /* Check for a game prompt to clear the flag. */
      gln_game_prompted ();
    }

  else if (gln_strcasecmp (argument, "off") == 0)
    {
      if (!gln_prompt_enabled)
        {
          gln_normal_string ("Glk extra prompts are already off.\n");
          return;
        }

      gln_prompt_enabled = FALSE;
      gln_normal_string ("Glk extra prompts are now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gln_normal_string ("Glk extra prompts are ");
      gln_normal_string (gln_prompt_enabled ? "on" : "off");
      gln_normal_string (".\n");
    }

  else
    {
      gln_normal_string ("Glk extra prompts can be ");
      gln_standout_string ("on");
      gln_normal_string (", or ");
      gln_standout_string ("off");
      gln_normal_string (".\n");
    }
}


/*
 * gln_command_print_version_number()
 * gln_command_version()
 *
 * Print out the Glk library version number.
 */
static void
gln_command_print_version_number (glui32 version)
{
  char buffer[64];

  sprintf (buffer, "%lu.%lu.%lu",
          (unsigned long) version >> 16,
          (unsigned long) (version >> 8) & 0xff,
          (unsigned long) version & 0xff);
  gln_normal_string (buffer);
}

static void
gln_command_version (const char *argument)
{
  glui32 version;
  assert (argument);

  gln_normal_string ("This is version ");
  gln_command_print_version_number (GLN_PORT_VERSION);
  gln_normal_string (" of the Glk Level 9 port.\n");

  version = glk_gestalt (gestalt_Version, 0);
  gln_normal_string ("The Glk library version is ");
  gln_command_print_version_number (version);
  gln_normal_string (".\n");
}


/*
 * gln_command_commands()
 *
 * Turn command escapes off.  Once off, there's no way to turn them back on.
 * Commands must be on already to enter this function.
 */
static void
gln_command_commands (const char *argument)
{
  assert (argument);

  if (gln_strcasecmp (argument, "on") == 0)
    {
      gln_normal_string ("Glk commands are already on.\n");
    }

  else if (gln_strcasecmp (argument, "off") == 0)
    {
      gln_commands_enabled = FALSE;
      gln_normal_string ("Glk commands are now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gln_normal_string ("Glk commands are ");
      gln_normal_string (gln_commands_enabled ? "on" : "off");
      gln_normal_string (".\n");
    }

  else
    {
      gln_normal_string ("Glk commands can be ");
      gln_standout_string ("on");
      gln_normal_string (", or ");
      gln_standout_string ("off");
      gln_normal_string (".\n");
    }
}


/*
 * gln_command_license()
 *
 * Print licensing terms.
 */
static void
gln_command_license (const char *argument)
{
  assert (argument);

  gln_normal_string ("This program is free software; you can redistribute it"
                      " and/or modify it under the terms of version 2 of the"
                      " GNU General Public License as published by the Free"
                      " Software Foundation.\n\n");

  gln_normal_string ("This program is distributed in the hope that it will be"
                      " useful, but ");
  gln_standout_string ("WITHOUT ANY WARRANTY");
  gln_normal_string ("; without even the implied warranty of ");
  gln_standout_string ("MERCHANTABILITY");
  gln_normal_string (" or ");
  gln_standout_string ("FITNESS FOR A PARTICULAR PURPOSE");
  gln_normal_string (".  See the GNU General Public License for more"
                      " details.\n\n");

  gln_normal_string ("You should have received a copy of the GNU General"
                      " Public License along with this program; if not, write"
                      " to the Free Software Foundation, Inc., 51 Franklin"
                      " Street, Fifth Floor, Boston, MA 02110-1301 USA\n\n");

  gln_normal_string ("Please report any bugs, omissions, or misfeatures to ");
  gln_standout_string ("simon_baldwin@yahoo.com");
  gln_normal_string (".\n");
}


/* Glk subcommands and handler functions. */
typedef const struct
{
  const char * const command;                     /* Glk subcommand. */
  void (* const handler) (const char *argument);  /* Subcommand handler. */
  const int takes_argument;                       /* Argument flag. */
} gln_command_t;
typedef gln_command_t *gln_commandref_t;

static void gln_command_summary (const char *argument);
static void gln_command_help (const char *argument);

static gln_command_t GLN_COMMAND_TABLE[] = {
  {"summary",        gln_command_summary,        FALSE},
  {"script",         gln_command_script,         TRUE},
  {"inputlog",       gln_command_inputlog,       TRUE},
  {"readlog",        gln_command_readlog,        TRUE},
  {"abbreviations",  gln_command_abbreviations,  TRUE},
  {"graphics",       gln_command_graphics,       TRUE},
  {"loopchecks",     gln_command_loopchecks,     TRUE},
  {"locals",         gln_command_locals,         TRUE},
  {"prompts",        gln_command_prompts,        TRUE},
  {"version",        gln_command_version,        FALSE},
  {"commands",       gln_command_commands,       TRUE},
  {"license",        gln_command_license,        FALSE},
  {"help",           gln_command_help,           TRUE},
  {NULL, NULL, FALSE}
};


/*
 * gln_command_summary()
 *
 * Report all current Glk settings.
 */
static void
gln_command_summary (const char *argument)
{
  gln_commandref_t entry;
  assert (argument);

  /*
   * Call handlers that have status to report with an empty argument,
   * prompting each to print its current setting.
   */
  for (entry = GLN_COMMAND_TABLE; entry->command; entry++)
    {
      if (entry->handler == gln_command_summary
            || entry->handler == gln_command_license
            || entry->handler == gln_command_help)
        continue;

      entry->handler ("");
    }
}


/*
 * gln_command_help()
 *
 * Document the available Glk commands.
 */
static void
gln_command_help (const char *command)
{
  gln_commandref_t entry, matched;
  assert (command);

  if (strlen (command) == 0)
    {
      gln_normal_string ("Glk commands are");
      for (entry = GLN_COMMAND_TABLE; entry->command; entry++)
        {
          gln_commandref_t next;

          next = entry + 1;
          gln_normal_string (next->command ? " " : " and ");
          gln_standout_string (entry->command);
          gln_normal_string (next->command ? "," : ".\n\n");
        }

      gln_normal_string ("Glk commands may be abbreviated, as long as"
                          " the abbreviation is unambiguous.  Use ");
      gln_standout_string ("glk help");
      gln_normal_string (" followed by a Glk command name for help on that"
                          " command.\n");
      return;
    }

  matched = NULL;
  for (entry = GLN_COMMAND_TABLE; entry->command; entry++)
    {
      if (gln_strncasecmp (command, entry->command, strlen (command)) == 0)
        {
          if (matched)
            {
              gln_normal_string ("The Glk command ");
              gln_standout_string (command);
              gln_normal_string (" is ambiguous.  Try ");
              gln_standout_string ("glk help");
              gln_normal_string (" for more information.\n");
              return;
            }
          matched = entry;
        }
    }
  if (!matched)
    {
      gln_normal_string ("The Glk command ");
      gln_standout_string (command);
      gln_normal_string (" is not valid.  Try ");
      gln_standout_string ("glk help");
      gln_normal_string (" for more information.\n");
      return;
    }

  if (matched->handler == gln_command_summary)
    {
      gln_normal_string ("Prints a summary of all the current Glk Level 9"
                          " settings.\n");
    }

  else if (matched->handler == gln_command_script)
    {
      gln_normal_string ("Logs the game's output to a file.\n\nUse ");
      gln_standout_string ("glk script on");
      gln_normal_string (" to begin logging game output, and ");
      gln_standout_string ("glk script off");
      gln_normal_string (" to end it.  Glk Level 9 will ask you for a file"
                          " when you turn scripts on.\n");
    }

  else if (matched->handler == gln_command_inputlog)
    {
      gln_normal_string ("Records the commands you type into a game.\n\nUse ");
      gln_standout_string ("glk inputlog on");
      gln_normal_string (", to begin recording your commands, and ");
      gln_standout_string ("glk inputlog off");
      gln_normal_string (" to turn off input logs.  You can play back"
                          " recorded commands into a game with the ");
      gln_standout_string ("glk readlog");
      gln_normal_string (" command.\n");
    }

  else if (matched->handler == gln_command_readlog)
    {
      gln_normal_string ("Plays back commands recorded with ");
      gln_standout_string ("glk inputlog on");
      gln_normal_string (".\n\nUse ");
      gln_standout_string ("glk readlog on");
      gln_normal_string (".  Command play back stops at the end of the"
                          " file.  You can also play back commands from a"
                          " text file created using any standard editor.\n");
    }

  else if (matched->handler == gln_command_abbreviations)
    {
      gln_normal_string ("Controls abbreviation expansion.\n\nGlk Level 9"
                          " automatically expands several standard single"
                          " letter abbreviations for you; for example, \"x\""
                          " becomes \"examine\".  Use ");
      gln_standout_string ("glk abbreviations on");
      gln_normal_string (" to turn this feature on, and ");
      gln_standout_string ("glk abbreviations off");
      gln_normal_string (" to turn it off.  While the feature is on, you"
                          " can bypass abbreviation expansion for an"
                          " individual game command by prefixing it with a"
                          " single quote.\n");
    }

  else if (matched->handler == gln_command_graphics)
    {
      gln_normal_string ("Turns interpreter graphics on and off.\n\nUse ");
      gln_standout_string ("glk graphics on");
      gln_normal_string (" to enable interpreter graphics, and ");
      gln_standout_string ("glk graphics off");
      gln_normal_string (" to turn graphics off and close the graphics window."
                         "  This control works slightly differently to the"
                         " 'graphics' command in Level 9 games themselves; the"
                         " game's 'graphics' command may disable new images,"
                         " but leave old ones displayed.  For graphics to be"
                         " displayed, they must be turned on in both the game"
                         " and the interpreter.\n");
    }

  else if (matched->handler == gln_command_loopchecks)
    {
      gln_normal_string ("Controls game infinite loop monitoring.\n\n"
                         "Some Level 9 games can enter an infinite loop if they"
                         " have nothing better to do.  A game might do this"
                         " after it has ended, should you decline its offer"
                         " to rerun.  To avoid the need to kill the interpreter"
                         " completely if a game does this, Glk Level 9 monitors"
                         " a game's input and output, and offers the option to"
                         " end the program gracefully if a game is silent for"
                         " a few seconds.  Use ");
      gln_standout_string ("glk loopchecks on");
      gln_normal_string (" to turn this feature on, and ");
      gln_standout_string ("glk loopchecks off");
      gln_normal_string (" to turn it off.\n");
    }

  else if (matched->handler == gln_command_locals)
    {
      gln_normal_string ("Controls interception of selected game commands.\n\n"
                         "Some Level 9 games were written for cassette tape"
                         " based microprocessor systems, and the way in which"
                         " they save, restore, and restart games can reflect"
                         " this.  There is also often no straightforward way"
                         " to quit from a game.\n\nTo make playing a Level 9"
                         " game appear similar to other systems, Glk Level 9"
                         " will trap the commands 'quit', 'restart', 'save',"
                         " 'restore', and 'load' (a synonym for 'restore') and"
                         " handle them locally within the interpreter.  Use ");
      gln_standout_string ("glk locals on");
      gln_normal_string (" to turn this feature on, and ");
      gln_standout_string ("glk locals off");
      gln_normal_string (" to turn it off.\n");
    }

  else if (matched->handler == gln_command_prompts)
    {
      gln_normal_string ("Controls extra input prompting.\n\n"
                          "Glk Level 9 can issue a replacement '>' input"
                          " prompt if it detects that the game hasn't prompted"
                          " after, say, an empty input line.  Use ");
      gln_standout_string ("glk prompts on");
      gln_normal_string (" to turn this feature on, and ");
      gln_standout_string ("glk prompts off");
      gln_normal_string (" to turn it off.\n");
    }

  else if (matched->handler == gln_command_version)
    {
      gln_normal_string ("Prints the version numbers of the Glk library"
                          " and the Glk Level 9 port.\n");
    }

  else if (matched->handler == gln_command_commands)
    {
      gln_normal_string ("Turn off Glk commands.\n\nUse ");
      gln_standout_string ("glk commands off");
      gln_normal_string (" to disable all Glk commands, including this one."
                          "  Once turned off, there is no way to turn Glk"
                          " commands back on while inside the game.\n");
    }

  else if (matched->handler == gln_command_license)
    {
      gln_normal_string ("Prints Glk Level 9's software license.\n");
    }

  else if (matched->handler == gln_command_help)
    gln_command_help ("");

  else
    gln_normal_string ("There is no help available on that Glk command."
                        "  Sorry.\n");
}


/*
 * gln_command_escape()
 *
 * This function is handed each input line.  If the line contains a specific
 * Glk port command, handle it and return TRUE, otherwise return FALSE.
 */
static int
gln_command_escape (const char *string)
{
  int posn;
  char *string_copy, *command, *argument;
  assert (string);

  /*
   * Return FALSE if the string doesn't begin with the Glk command escape
   * introducer.
   */
  posn = strspn (string, "\t ");
  if (gln_strncasecmp (string + posn, "glk", strlen ("glk")) != 0)
    return FALSE;

  /* Take a copy of the string, without any leading space or introducer. */
  string_copy = gln_malloc (strlen (string + posn) + 1 - strlen ("glk"));
  strcpy (string_copy, string + posn + strlen ("glk"));

  /*
   * Find the subcommand; the first word in the string copy.  Find its end,
   * and ensure it terminates with a NUL.
   */
  posn = strspn (string_copy, "\t ");
  command = string_copy + posn;
  posn += strcspn (string_copy + posn, "\t ");
  if (string_copy[posn] != '\0')
    string_copy[posn++] = '\0';

  /*
   * Now find any argument data for the command, ensuring it too terminates
   * with a NUL.
   */
  posn += strspn (string_copy + posn, "\t ");
  argument = string_copy + posn;
  posn += strcspn (string_copy + posn, "\t ");
  string_copy[posn] = '\0';

  /*
   * Try to handle the command and argument as a Glk subcommand.  If it
   * doesn't run unambiguously, print command usage.  Treat an empty command
   * as "help".
   */
  if (strlen (command) > 0)
    {
      gln_commandref_t entry, matched;
      int matches;

      /*
       * Search for the first unambiguous table command string matching
       * the command passed in.
       */
      matches = 0;
      matched = NULL;
      for (entry = GLN_COMMAND_TABLE; entry->command; entry++)
        {
          if (gln_strncasecmp (command, entry->command, strlen (command)) == 0)
            {
              matches++;
              matched = entry;
            }
        }

      /* If the match was unambiguous, call the command handler. */
      if (matches == 1)
        {
          gln_normal_char ('\n');
          matched->handler (argument);

          if (!matched->takes_argument && strlen (argument) > 0)
            {
              gln_normal_string ("[The ");
              gln_standout_string (matched->command);
              gln_normal_string (" command ignores arguments.]\n");
            }
        }

      /* No match, or the command was ambiguous. */
      else
        {
          gln_normal_string ("\nThe Glk command ");
          gln_standout_string (command);
          gln_normal_string (" is ");
          gln_normal_string (matches == 0 ? "not valid" : "ambiguous");
          gln_normal_string (".  Try ");
          gln_standout_string ("glk help");
          gln_normal_string (" for more information.\n");
        }
    }
  else
    {
      gln_normal_char ('\n');
      gln_command_help ("");
    }

  /* The string contained a Glk command; return TRUE. */
  free (string_copy);
  return TRUE;
}


/*
 * gln_command_intercept()
 *
 * The Level 9 games handle the commands "quit" and "restart" oddly, and
 * somewhat similarly.  Both prompt "Press SPACE to play again", and then
 * ignore all characters except space.  This makes it especially hard to exit
 * from a game without killing the interpreter process.  They also handle
 * "restore" via an odd security mechanism which has no real place here (the
 * base Level 9 interpreter sidesteps this with its "#restore" command, and
 * has some bugs in "save").
 *
 * To try to improve these, here we'll catch and special case the input lines
 * "quit", "save", "restore", and "restart".  "Load" is a synonym for
 * "restore".
 *
 * On "quit" or "restart", the function sets the interpreter stop reason
 * code, stops the current game run.  On "save" or "restore" it calls the
 * appropriate internal interpreter function.
 *
 * The return value is TRUE if an intercepted command was found, otherwise
 * FALSE.
 */
static int
gln_command_intercept (char *string)
{
  int posn, result;
  char *string_copy, *trailing;
  assert (string);

  result = FALSE;

  /* Take a copy of the string, excluding any leading whitespace. */
  posn = strspn (string, "\t ");
  string_copy = gln_malloc (strlen (string + posn) + 1);
  strcpy (string_copy, string + posn);

  /*
   * Find the space or NUL after the first word, and check that anything
   * after it the first word is whitespace only.
   */
  posn = strcspn (string_copy, "\t ");
  trailing = string_copy + posn;
  if (trailing[strspn (trailing, "\t ")] == '\0')
    {
      /* Terminate the string copy for easy comparisons. */
      string_copy[posn] = '\0';

      /* If this command was "quit", confirm, then call StopGame(). */
      if (gln_strcasecmp (string_copy, "quit") == 0)
        {
          if (gln_confirm ("\nDo you really want to stop? [Y or N] "))
            {
              gln_stop_reason = STOP_EXIT;
              StopGame ();
            }
          result = TRUE;
        }

      /* If this command was "restart", confirm, then call StopGame(). */
      else if (gln_strcasecmp (string_copy, "restart") == 0)
        {
          if (gln_confirm ("\nDo you really want to restart? [Y or N] "))
            {
              gln_stop_reason = STOP_RESTART;
              StopGame ();
            }
          result = TRUE;
        }

      /* If this command was "save", simply call save(). */
      else if (gln_strcasecmp (string_copy, "save") == 0)
        {
          gln_standout_string ("\nSaving using interpreter\n\n");
          save ();
          result = TRUE;
        }

      /* If this command was "restore" or "load", call restore(). */
      else if (gln_strcasecmp (string_copy, "restore") == 0
               || gln_strcasecmp (string_copy, "load") == 0)
        {
          gln_standout_string ("\nRestoring using interpreter\n\n");
          restore ();
          result = TRUE;
        }
    }

  free (string_copy);
  return result;
}


/*---------------------------------------------------------------------*/
/*  Glk port input functions                                           */
/*---------------------------------------------------------------------*/

/* Ctrl-C and Ctrl-U character constants. */
static const char GLN_CONTROL_C = '\003',
                  GLN_CONTROL_U = '\025';

/*
 * os_readchar() call count limit, after which we really read a character.
 * Also, call count limit on os_stoplist calls, after which we poll for a
 * character press to stop the listing, and a stoplist poll timeout.
 */
static const int GLN_READCHAR_LIMIT = 1024,
                 GLN_STOPLIST_LIMIT = 10;
static const glui32 GLN_STOPLIST_TIMEOUT = 50;

/* Quote used to suppress abbreviation expansion and local commands. */
static const char GLN_QUOTED_INPUT = '\'';


/*
 * Note of when the interpreter is in list output.  The last element of any
 * list generally lacks a terminating newline, and unless we do something
 * special with it, it'll look like a valid prompt to us.
 */
static int gln_inside_list = FALSE;


/* Table of single-character command abbreviations. */
typedef const struct
{
  const char abbreviation;       /* Abbreviation character. */
  const char * const expansion;  /* Expansion string. */
} gln_abbreviation_t;
typedef gln_abbreviation_t *gln_abbreviationref_t;

static gln_abbreviation_t GLN_ABBREVIATIONS[] = {
  {'c', "close"},    {'g', "again"},  {'i', "inventory"},
  {'k', "attack"},   {'l', "look"},   {'p', "open"},
  {'q', "quit"},     {'r', "drop"},   {'t', "take"},
  {'x', "examine"},  {'y', "yes"},    {'z', "wait"},
  {'\0', NULL}
};


/*
 * gln_expand_abbreviations()
 *
 * Expand a few common one-character abbreviations commonly found in other
 * game systems, but not always normal in Level 9 games.
 */
static void
gln_expand_abbreviations (char *buffer, int size)
{
  char *command, abbreviation;
  const char *expansion;
  gln_abbreviationref_t entry;
  assert (buffer);

  /* Ignore anything that isn't a single letter command. */
  command = buffer + strspn (buffer, "\t ");
  if (!(strlen (command) == 1
        || (strlen (command) > 1 && isspace (command[1]))))
    return;

  /* Scan the abbreviations table for a match. */
  abbreviation = glk_char_to_lower ((unsigned char) command[0]);
  expansion = NULL;
  for (entry = GLN_ABBREVIATIONS; entry->expansion; entry++)
    {
      if (entry->abbreviation == abbreviation)
        {
          expansion = entry->expansion;
          break;
        }
    }

  /*
   * If a match found, check for a fit, then replace the character with the
   * expansion string.
   */
  if (expansion)
    {
      if (strlen (buffer) + strlen (expansion) - 1 >= size)
        return;

      memmove (command + strlen (expansion) - 1, command, strlen (command) + 1);
      memcpy (command, expansion, strlen (expansion));

#if !(defined(GARGLK) || defined(SPATTERLIGHT))
      gln_standout_string ("[");
      gln_standout_char (abbreviation);
      gln_standout_string (" -> ");
      gln_standout_string (expansion);
      gln_standout_string ("]\n");
#endif
    }
}


/*
 * gln_output_endlist()
 *
 * The core interpreter doesn't terminate lists with a newline, so we take
 * care of that here; a fixup for input functions.
 */
static void
gln_output_endlist (void)
{
  if (gln_inside_list)
    {
      /*
       * Supply the missing newline, using os_printchar() so that list output
       * doesn't look like a prompt when we come to flush it.
       */
      os_printchar ('\n');

      gln_inside_list = FALSE;
    }
}


/*
 * os_input()
 *
 * Read a line from the keyboard.  This function makes a special case of
 * some command strings, and will also perform abbreviation expansion.
 */
gln_bool
os_input (char *buffer, int size)
{
  event_t event;
  assert (buffer);

  /* If doing linemode graphics, run all graphic opcodes available. */
  gln_linegraphics_process ();

  /*
   * Update the current status line display, flush any pending buffered
   * output, and terminate any open list.
   */
  gln_status_notify ();
  gln_output_endlist ();
  gln_output_flush ();

  /*
   * Level 9 games tend not to issue a prompt after reading an empty
   * line of input, and the Adrian Mole games don't issue a prompt at
   * all when outside the 1/2/3 menuing system.  This can make for a
   * very blank looking screen.
   *
   * To slightly improve things, if it looks like we didn't get a
   * prompt from the game, do our own.
   */
  if (gln_prompt_enabled && !gln_game_prompted ())
    {
      gln_normal_char ('\n');
      gln_normal_string (GLN_INPUT_PROMPT);
    }

  /*
   * If we have an input log to read from, use that until it is exhausted.  On
   * end of file, close the stream and resume input from line requests.
   */
  if (gln_readlog_stream)
    {
      glui32 chars;

      /* Get the next line from the log stream. */
      chars = glk_get_line_stream (gln_readlog_stream, buffer, size);
      if (chars > 0)
        {
          /* Echo the line just read in input style. */
          glk_set_style (style_Input);
          glk_put_buffer (buffer, chars);
          glk_set_style (style_Normal);

          /* Tick the watchdog, and return. */
          gln_watchdog_tick ();
          return TRUE;
        }

      /*
       * We're at the end of the log stream.  Close it, and then continue
       * on to request a line from Glk.
       */
      glk_stream_close (gln_readlog_stream, NULL);
      gln_readlog_stream = NULL;
    }

  /*
   * No input log being read, or we just hit the end of file on one.  Revert
   * to normal line input; start by getting a new line from Glk.
   */
  glk_request_line_event (gln_main_window, buffer, size - 1, 0);
  gln_event_wait (evtype_LineInput, &event);

  /* Terminate the input line with a NUL. */
  assert (event.val1 <= size - 1);
  buffer[event.val1] = '\0';

  /*
   * If neither abbreviations nor local commands are enabled, nor game
   * command interceptions, use the data read above without further massaging.
   */
  if (gln_abbreviations_enabled
      || gln_commands_enabled || gln_intercept_enabled)
    {
      char *command;

      /*
       * If the first non-space input character is a quote, bypass all
       * abbreviation expansion and local command recognition, and use the
       * unadulterated input, less introductory quote.
       */
      command = buffer + strspn (buffer, "\t ");
      if (command[0] == GLN_QUOTED_INPUT)
        {
          /* Delete the quote with memmove(). */
          memmove (command, command + 1, strlen (command));
        }
      else
        {
          /* Check for, and expand, and abbreviated commands. */
          if (gln_abbreviations_enabled)
            gln_expand_abbreviations (buffer, size);

          /*
           * Check for standalone "help", then for Glk port special commands;
           * suppress the interpreter's use of this input for Glk commands by
           * returning FALSE.
           */
          if (gln_commands_enabled)
            {
              int posn;

              posn = strspn (buffer, "\t ");
              if (gln_strncasecmp (buffer + posn, "help", strlen ("help"))== 0)
                {
                  if (strspn (buffer + posn + strlen ("help"), "\t ")
                      == strlen (buffer + posn + strlen ("help")))
                    {
                      gln_output_register_help_request ();
                    }
                }

              if (gln_command_escape (buffer))
                {
                  gln_output_silence_help_hints ();
                  gln_watchdog_tick ();
                  return FALSE;
                }
            }

          /*
           * Check for locally intercepted commands, again returning FALSE if
           * one is handled.
           */
          if (gln_intercept_enabled)
            {
              if (gln_command_intercept (buffer))
                {
                  gln_watchdog_tick ();
                  return FALSE;
                }
            }
        }
    }

  /*
   * If there is an input log active, log this input string to it. Note that
   * by logging here we get any abbreviation expansions but we won't log glk
   * special commands, nor any input read from a current open input log.
   */
  if (gln_inputlog_stream)
    {
      glk_put_string_stream (gln_inputlog_stream, buffer);
      glk_put_char_stream (gln_inputlog_stream, '\n');
    }

  gln_watchdog_tick ();
  return TRUE;
}


/*
 * os_readchar()
 *
 * Poll the keyboard for characters, and return the character code of any key
 * pressed, or 0 if none pressed.
 *
 * Simple though this sounds, it's tough to do right in a timesharing OS, and
 * requires something close to an abuse of Glk.
 *
 * The initial, tempting, implementation is to wait inside this function for
 * a key press, then return the code.  Unfortunately, this causes problems in
 * the Level 9 interpreter.  Here's why: the interpreter is a VM emulating a
 * single-user microprocessor system.  On such a system, it's quite okay for
 * code to spin in a loop waiting for a keypress; there's nothing else
 * happening on the system, so it can burn CPU.  To wait for a keypress, game
 * code might first wait for no-keypress (0 from this function), then a
 * keypress (non-0), then no-keypress again (and it does indeed seem to do
 * just this).  If, in os_readchar(), we simply wait for and return key codes,
 * we'll never return a 0, so the above wait for a keypress in the game will
 * hang forever.
 *
 * To make matters more complex, some Level 9 games poll for keypresses as a
 * way for a user to halt scrolling.  For these polls, we really want to
 * return 0, otherwise the output grinds to a halt.  Moreover, some games even
 * use key polling as a crude form of timeout - poll and increment a counter,
 * and exit when either os_readchar() returns non-0, or after some 300 or so
 * polls.
 *
 * So, this function MUST return 0 sometimes, and real key codes other times.
 * The solution adopted is best described as expedient.  Depending on what Glk
 * provides in the way of timers, we'll do one of two things:
 *
 *   o If we have timers, we'll set up a timeout, and poll for a key press
 *     within that timeout.  As a way to smooth output for games that use key
 *     press polling for scroll control, we'll ignore calls until we get two
 *     in a row without intervening character output.
 *
 *   o If we don't have timers, then we'll return 0 most of the time, and then
 *     really wait for a key one time out of some number.  A game polling for
 *     keypresses to halt scrolling will probably be to the point where it
 *     cannot continue without user input at this juncture, and once we've
 *     rejected a few hundred calls we can now really wait for Glk key press
 *     event, and avoid a spinning loop.  A game using key polling as crude
 *     timing may, or may not, time out in the calls for which we return 0.
 *
 * Empirically, this all seems to work.  The only odd behaviour is with the
 * DEMO mode of Adrian Mole where Glk has no timers, and this is primarily
 * because the DEMO mode relies on the delay of keyboard polling for part of
 * its effect; on a modern system, the time to call through here is nowhere
 * near the time consumed by the original platform.  The other point of note
 * is that this all means that we can't return characters from any readlog
 * with this function; its timing stuff and its general polling nature make
 * it impossible to connect to readlog, so it just won't work at all with the
 * Adrian Mole games, Glk timers or otherwise.
 */
char
os_readchar (int millis)
{
  static int call_count = 0;

  event_t event;
  char character;

  /* If doing linemode graphics, run all graphic opcodes available. */
  gln_linegraphics_process ();

  /*
   * Here's the way we try to emulate keyboard polling for the case of no Glk
   * timers.  We'll say nothing is pressed for some number of consecutive
   * calls, then continue after that number of calls.
   */
  if (!glk_gestalt (gestalt_Timer, 0))
    {
      if (++call_count < GLN_READCHAR_LIMIT)
        {
          /* Call tick as we may be outside an opcode loop. */
          glk_tick ();
          gln_watchdog_tick ();
          return 0;
        }
      else
        call_count = 0;
    }

  /*
   * If we have Glk timers, we can smooth game output with games that contin-
   * uously use this input function by pretending that there is no keypress
   * if the game printed output since the last call.  This helps with the
   * Adrian Mole games, which check for a keypress at the end of a line as a
   * way to temporarily halt scrolling.
   */
  if (glk_gestalt (gestalt_Timer, 0))
    {
      if (gln_recent_output ())
        {
          /* Call tick as we may be outside an opcode loop. */
          glk_tick ();
          gln_watchdog_tick ();
          return 0;
        }
    }

  /*
   * Now flush any pending buffered output.  We do it here rather than earlier
   * as it only needs to be done when we're going to request Glk input, and
   * we may have avoided this with the checks above.
   */
  gln_status_notify ();
  gln_output_endlist ();
  gln_output_flush ();

  /*
   * Set up a character event request, and a timeout if the Glk library can
   * do them, and wait until one or the other occurs.  Loop until we read an
   * acceptable ASCII character (if we don't time out).
   */
  do
    {
      glk_request_char_event (gln_main_window);
      if (glk_gestalt (gestalt_Timer, 0))
        {
          gln_arbitrate_request_timer_events (millis);
          gln_event_wait_2 (evtype_CharInput, evtype_Timer, &event);
          gln_arbitrate_request_timer_events (0);

          /*
           * If the event was a timeout, cancel the unfilled character
           * request, and return no-keypress value.
           */
          if (event.type == evtype_Timer)
            {
              glk_cancel_char_event (gln_main_window);
              gln_watchdog_tick ();
              return 0;
            }
        }
      else
        gln_event_wait (evtype_CharInput, &event);
    }
  while (event.val1 > UCHAR_MAX && event.val1 != keycode_Return);

  /* Extract the character from the event, converting Return, no echo. */
  character = event.val1 == keycode_Return ? '\n' : event.val1;

  /*
   * Special case ^U as a way to press a key on a wait, yet return a code to
   * the interpreter as if no key was pressed.  Useful if scrolling stops
   * where there are no Glk timers, to get scrolling started again.  ^U is
   * always active.
   */
  if (character == GLN_CONTROL_U)
    {
      gln_watchdog_tick ();
      return 0;
    }

  /*
   * Special case ^C to quit the program.  Without this, there's no easy way
   * to exit from a game that never uses os_input(), but instead continually
   * uses just os_readchar().  ^C handling can be disabled with command line
   * options.
   */
  if (gln_intercept_enabled && character == GLN_CONTROL_C)
    {
      if (gln_confirm ("\n\nDo you really want to stop? [Y or N] "))
        {
          gln_stop_reason = STOP_EXIT;
          StopGame ();

          gln_watchdog_tick ();
          return 0;
        }
    }

  /*
   * If there is a transcript stream, send the input to it as a single line
   * string, otherwise it won't be visible in the transcript.
   */
  if (gln_transcript_stream)
    {
      glk_put_char_stream (gln_transcript_stream, character);
      glk_put_char_stream (gln_transcript_stream, '\n');
    }

  /* Finally, return the single character read. */
  gln_watchdog_tick ();
  return character;
}


/*
 * os_stoplist()
 *
 * This is called from #dictionary listings to poll for a request to stop
 * the listing.  A check for keypress is usual at this point.  However, Glk
 * cannot check for keypresses without a delay, which slows listing consid-
 * erably, since it also adjusts and renders the display.  As a compromise,
 * then, we'll check for keypresses on a small percentage of calls, say one
 * in ten, which means that listings happen with only a short delay, but
 * there's still an opportunity to stop them.
 *
 * This applies only where the Glk library has timers.  Where it doesn't, we
 * can't check for keypresses without blocking, so we do no checks at all,
 * and let lists always run to completion.
 */
gln_bool
os_stoplist (void)
{
  static int call_count = 0;

  event_t event;
  int is_stop_confirmed;

  /* Note that the interpreter is producing a list. */
  gln_inside_list = TRUE;

  /*
   * If there are no Glk timers, then polling for a keypress but continuing
   * on if there isn't one is not an option.  So flush output, return FALSE,
   * and just keep listing on to the end.
   */
  if (!glk_gestalt (gestalt_Timer, 0))
    {
      gln_output_flush ();
      gln_watchdog_tick ();
      return FALSE;
    }

  /* Increment the call count, and return FALSE if under the limit. */
  if (++call_count < GLN_STOPLIST_LIMIT)
    {
      /* Call tick as we may be outside an opcode loop. */
      glk_tick ();
      gln_watchdog_tick ();
      return FALSE;
    }
  else
    call_count = 0;

  /* Flush any pending buffered output, delayed to here in case avoidable. */
  gln_output_flush ();

  /*
   * Look for a keypress, with a very short timeout in place, in a similar
   * way as done for os_readchar() above.
   */
  glk_request_char_event (gln_main_window);
  gln_arbitrate_request_timer_events (GLN_STOPLIST_TIMEOUT);
  gln_event_wait_2 (evtype_CharInput, evtype_Timer, &event);
  gln_arbitrate_request_timer_events (0);

  /*
   * If the event was a timeout, cancel the unfilled character request, and
   * return FALSE to continue listing.
   */
  if (event.type == evtype_Timer)
    {
      glk_cancel_char_event (gln_main_window);
      gln_watchdog_tick ();
      return FALSE;
    }

  /* Keypress detected, so offer to stop listing. */
  assert (event.type == evtype_CharInput);
  is_stop_confirmed = gln_confirm ("\n\nStop listing? [Y or N] ");

  /*
   * As we've output a newline, we no longer consider that we're inside a
   * list.  Clear the flag, and also clear prompt detection by polling it.
   */
  gln_inside_list = FALSE;
  gln_game_prompted ();

  /* Return TRUE if stop was confirmed, FALSE to keep listing. */
  gln_watchdog_tick ();
  return is_stop_confirmed;
}


/*
 * gln_confirm()
 *
 * Print a confirmation prompt, and read a single input character, taking
 * only [YyNn] input.  If the character is 'Y' or 'y', return TRUE.
 */
static int
gln_confirm (const char *prompt)
{
  event_t event;
  unsigned char response;
  assert (prompt);

  /*
   * Print the confirmation prompt, in a style that hints that it's from the
   * interpreter, not the game.
   */
  gln_standout_string (prompt);

  /* Wait for a single 'Y' or 'N' character response. */
  response = ' ';
  do
    {
      glk_request_char_event (gln_main_window);
      gln_event_wait (evtype_CharInput, &event);

      if (event.val1 <= UCHAR_MAX)
        response = glk_char_to_upper (event.val1);
    }
  while (!(response == 'Y' || response == 'N'));

  /* Echo the confirmation response, and a blank line. */
  glk_set_style (style_Input);
  glk_put_string (response == 'Y' ? "Yes" : "No");
  glk_set_style (style_Normal);
  glk_put_string ("\n\n");

  return response == 'Y';
}


/*---------------------------------------------------------------------*/
/*  Glk port event functions                                           */
/*---------------------------------------------------------------------*/

/*
 * gln_event_wait_2()
 * gln_event_wait()
 *
 * Process Glk events until one of the expected type, or types, arrives.
 * Return the event of that type.
 */
static void
gln_event_wait_2 (glui32 wait_type_1, glui32 wait_type_2, event_t * event)
{
  assert (event);

  do
    {
      glk_select (event);

      switch (event->type)
        {
        case evtype_Arrange:
        case evtype_Redraw:
          /* Refresh any sensitive windows on size events. */
          gln_status_redraw ();
          gln_graphics_paint ();
          break;

        case evtype_Timer:
          /* Do background graphics updates on timeout. */
          gln_graphics_timeout ();
          break;
        }
    }
  while (event->type != wait_type_1 && event->type != wait_type_2);
}

static void
gln_event_wait (glui32 wait_type, event_t * event)
{
  assert (event);
  gln_event_wait_2 (wait_type, evtype_None, event);
}


/*---------------------------------------------------------------------*/
/*  Glk port file functions                                            */
/*---------------------------------------------------------------------*/

/*
 * os_save_file ()
 * os_load_file ()
 *
 * Save the current game state to a file, and load a game state.
 */
gln_bool
os_save_file (gln_byte * ptr, int bytes)
{
  frefid_t fileref;
  strid_t stream;
  assert (ptr);

  /* Flush any pending buffered output. */
  gln_output_flush ();

  fileref = glk_fileref_create_by_prompt (fileusage_SavedGame,
                                          filemode_Write, 0);
  if (!fileref)
    {
      gln_watchdog_tick ();
      return FALSE;
    }

  stream = glk_stream_open_file (fileref, filemode_Write, 0);
  if (!stream)
    {
      glk_fileref_destroy (fileref);
      gln_watchdog_tick ();
      return FALSE;
    }

  /* Write game state. */
  glk_put_buffer_stream (stream, (char *)ptr, bytes);

  glk_stream_close (stream, NULL);
  glk_fileref_destroy (fileref);

  gln_watchdog_tick ();
  return TRUE;
}

gln_bool
os_load_file (gln_byte * ptr, int *bytes, int max)
{
  frefid_t fileref;
  strid_t stream;
  assert (ptr && bytes);

  /* Flush any pending buffered output. */
  gln_output_flush ();

  fileref = glk_fileref_create_by_prompt (fileusage_SavedGame,
                                          filemode_Read, 0);
  if (!fileref)
    {
      gln_watchdog_tick ();
      return FALSE;
    }

  /*
   * Reject the file reference if we're expecting to read from it, and the
   * referenced file doesn't exist.
   */
  if (!glk_fileref_does_file_exist (fileref))
    {
      glk_fileref_destroy (fileref);
      gln_watchdog_tick ();
      return FALSE;
    }

  stream = glk_stream_open_file (fileref, filemode_Read, 0);
  if (!stream)
    {
      glk_fileref_destroy (fileref);
      gln_watchdog_tick ();
      return FALSE;
    }

  /* Restore saved game data. */
  *bytes = glk_get_buffer_stream (stream, (char *)ptr, max);

  glk_stream_close (stream, NULL);
  glk_fileref_destroy (fileref);

  gln_watchdog_tick ();
  return TRUE;
}


/*---------------------------------------------------------------------*/
/*  Glk port multi-file game functions                                 */
/*---------------------------------------------------------------------*/

/*
 * os_get_game_file ()
 *
 * This function is a bit of a cheat.  It's called when the emulator has
 * detected a request from the game to restart the tape, on a tape-based
 * game.  Ordinarily, we should prompt the player for the name of the
 * system file containing the next game part.  Unfortunately, Glk doesn't
 * make this at all easy.  The requirement is to return a filename, but Glk
 * hides these inside fileref_t's, and won't let them out.
 *
 * Theoretically, according to the porting guide, this function should
 * prompt the user for a new game file name, that being the next part of the
 * game just (presumably) completed.
 *
 * However, the newname passed in is always the current game file name, as
 * level9.c ensures this for us.  If we search for, and find, and then inc-
 * rement, the last digit in the filename passed in, we wind up with, in
 * all likelihood, the right file path.  This is icky.
 *
 * This function is likely to be a source of portability problems on
 * platforms that don't implement a file path/name mechanism that matches
 * the expectations of the Level 9 base interpreter fairly closely.
 */
gln_bool
os_get_game_file (char *newname, int size)
{
  char *basename;
  int index, digit, file_number;
  FILE *stream;
  assert (newname);

  /* Find the last element of the filename passed in. */
  basename = strrchr (newname, GLN_FILE_DELIM);
  basename = basename ? basename + 1 : newname;

  /* Search for the last numeric character in the basename. */
  digit = -1;
  for (index = strlen (basename) - 1; index >= 0; index--)
    {
      if (isdigit (basename[index]))
        {
          digit = index;
          break;
        }
    }
  if (digit == -1)
    {
      gln_watchdog_tick ();
      return FALSE;
    }

  /*
   * Convert the digit to a file number and increment it.  Fail if the new
   * file number is outside 1..9.
   */
  file_number = basename[digit] - '0' + 1;
  if (file_number < 1 || file_number > 9)
    {
      gln_watchdog_tick ();
      return FALSE;
    }

  /* Write the new number back into the file. */
  basename[digit] = file_number + '0';

  /* Flush pending output, then display the filename generated. */
  gln_output_flush ();
  gln_game_prompted ();
  gln_standout_string ("\nNext load file: ");
  gln_standout_string (basename);
  gln_standout_string ("\n\n");

  /*
   * Try to confirm access to the file.  Otherwise, if we return TRUE but the
   * interpreter can't open the file, it stops the game, and we then lose any
   * chance to save it before quitting.
   */
  stream = fopen (newname, "rb");
  if (!stream)
    {
      /* Restore newname to how it was, and return fail. */
      basename[digit] = file_number - 1 + '0';
      gln_watchdog_tick ();
      return FALSE;
    }
  fclose (stream);

  /* Encourage game name re-lookup, and return success. */
  gln_gameid_game_name_reset ();
  gln_watchdog_tick ();
  return TRUE;
}


/*
 * os_set_filenumber()
 *
 * This function returns the next file in a game series for a disk-based
 * game (typically, gamedat1.dat, gamedat2.dat...).  It finds a single digit
 * in a filename, and resets it to the new value passed in.  The implemen-
 * tation here is based on the generic interface version, and with the same
 * limitations, specifically being limited to file numbers in the range 0
 * to 9, since it works on only the last digit character in the filename
 * buffer passed in.
 *
 * This function may also be a source of portability problems on platforms
 * that don't use "traditional" file path schemes.
 */
void
os_set_filenumber (char *newname, int size, int file_number)
{
  char *basename;
  int index, digit;
  assert (newname);

  /* Do nothing if the file number is beyond what we can handle. */
  if (file_number < 0 || file_number > 9)
    {
      gln_watchdog_tick ();
      return;
    }

  /* Find the last element of the new filename. */
  basename = strrchr (newname, GLN_FILE_DELIM);
  basename = basename ? basename + 1 : newname;

  /* Search for the last numeric character in the basename. */
  digit = -1;
  for (index = strlen (basename) - 1; index >= 0; index--)
    {
      if (isdigit (basename[index]))
        {
          digit = index;
          break;
        }
    }
  if (digit == -1)
    {
      gln_watchdog_tick ();
      return;
    }

  /* Reset the digit in the file name. */
  basename[digit] = file_number + '0';

  /* Flush pending output, then display the filename generated. */
  gln_output_flush ();
  gln_game_prompted ();
  gln_standout_string ("\nNext disk file: ");
  gln_standout_string (basename);
  gln_standout_string ("\n\n");

  /* Encourage game name re-lookup, and return. */
  gln_gameid_game_name_reset ();
  gln_watchdog_tick ();
}


/*
 * os_open_script_file()
 *
 * Handles player calls to the "#play" meta-command.  Because we have our
 * own way of handling scripts, this function is a stub.
 */
FILE *
os_open_script_file (void) {
  return NULL;
}


/*---------------------------------------------------------------------*/
/*  Functions intercepted by link-time wrappers                        */
/*---------------------------------------------------------------------*/

/*
 * __wrap_toupper()
 * __wrap_tolower()
 *
 * Wrapper functions around toupper() and tolower().  The Linux linker's
 * --wrap option will convert calls to mumble() to __wrap_mumble() if we
 * give it the right options.  We'll use this feature to translate all
 * toupper() and tolower() calls in the interpreter code into calls to
 * Glk's versions of these functions.
 *
 * It's not critical that we do this.  If a linker, say a non-Linux one,
 * won't do --wrap, then just do without it.  It's unlikely that there
 * will be much noticeable difference.
 */
int
__wrap_toupper (int ch)
{
  unsigned char uch;

  uch = glk_char_to_upper ((unsigned char) ch);
  return (int) uch;
}

int
__wrap_tolower (int ch)
{
  unsigned char lch;

  lch = glk_char_to_lower ((unsigned char) ch);
  return (int) lch;
}


/*---------------------------------------------------------------------*/
/*  main() and options parsing                                         */
/*---------------------------------------------------------------------*/

/*
 * Watchdog timeout -- we'll wait for five seconds of silence from the core
 * interpreter before offering to stop the game forcibly, and we'll check
 * it every 10,240 opcodes.
 */
static const int GLN_WATCHDOG_TIMEOUT = 5,
                 GLN_WATCHDOG_PERIOD = 10240;

/*
 * The following values need to be passed between the startup_code and main
 * functions.
 */
static char *gln_gamefile = NULL,      /* Name of game file. */
            *gln_game_message = NULL;  /* Error message. */


/*
 * gln_establish_picture_filename()
 *
 * Given a game name, try to create an (optional) graphics data file. For
 * an input "file" X, the function looks for X.PIC or X.pic, then for
 * PICTURE.DAT or picture.dat in the same directory as X.  If the input file
 * already ends with a three-letter extension, it's stripped first.
 *
 * The function returns NULL if a graphics file is not available.  It's not
 * fatal for this to be the case.  Filenames are malloc'ed, and need to be
 * freed by the caller.
 *
 * The function uses fopen() rather than access() since fopen() is an ANSI
 * standard function, and access() isn't.
 */
static void
gln_establish_picture_filename (char *name, char **graphics)
{
  char *base, *directory_end, *graphics_file;
  FILE *stream = NULL;
  assert (name && graphics);

  /* Take a destroyable copy of the input filename. */
  base = gln_malloc (strlen (name) + 1);
  strcpy (base, name);

  /* If base has an extension .LEV, .SNA, or similar, remove it. */
  if (strrchr (base, '.'))
    {
      base[strlen (base) - strlen (strrchr (base, '.'))] = '\0';
    }

  /* Allocate space for the return graphics file. */
  graphics_file = gln_malloc (strlen (base) + strlen (".___") + 1);

  /* Form a candidate graphics file, using a .PIC extension. */
  if (!stream)
    {
      strcpy (graphics_file, base);
      strcat (graphics_file, ".PIC");
      stream = fopen (graphics_file, "rb");
    }

  if (!stream)
    {
      strcpy (graphics_file, base);
      strcat (graphics_file, ".pic");
      stream = fopen (graphics_file, "rb");
    }

  /* Form a candidate graphics file, using a .CGA extension. */
  if (!stream)
    {
      strcpy (graphics_file, base);
      strcat (graphics_file, ".CGA");
      stream = fopen (graphics_file, "rb");
    }

  if (!stream)
    {
      strcpy (graphics_file, base);
      strcat (graphics_file, ".cga");
      stream = fopen (graphics_file, "rb");
    }

  /* Form a candidate graphics file, using a .HRC extension. */
  if (!stream)
    {
      strcpy (graphics_file, base);
      strcat (graphics_file, ".HRC");
      stream = fopen (graphics_file, "rb");
    }

  if (!stream)
    {
      strcpy (graphics_file, base);
      strcat (graphics_file, ".hrc");
      stream = fopen (graphics_file, "rb");
    }

  /* No access to graphics file. */
  if (!stream)
    {
      free (graphics_file);
      graphics_file = NULL;
    }

  if (stream)
    fclose (stream);

  /* If we found a graphics file, return its name immediately. */
  if (graphics_file)
    {
      *graphics = graphics_file;
      free (base);
      return;
    }

  /* Retry with base set to the game file directory part only. */
  directory_end = strrchr (base, GLN_FILE_DELIM);
  directory_end = directory_end ? directory_end + 1 : base;
  base[directory_end - base] = '\0';

  /* Again, allocate space for the return graphics file. */
  graphics_file = gln_malloc (strlen (base) + strlen ("PICTURE.DAT") + 1);

  /* As above, form a candidate graphics file. */
  strcpy (graphics_file, base);
  strcat (graphics_file, "PICTURE.DAT");
  stream = fopen (graphics_file, "rb");
  if (!stream)
    {
      /* Retry, using picture.dat extension instead. */
      strcpy (graphics_file, base);
      strcat (graphics_file, "picture.dat");
      stream = fopen (graphics_file, "rb");
      if (!stream)
        {
          /*
           * No access to this graphics file.  In this case, free memory
           * and reset graphics file to NULL.
           */
          free (graphics_file);
          graphics_file = NULL;
        }
    }
  if (stream)
    fclose (stream);

  /*
   * Return whatever we found for the graphics file (NULL if none found),
   * and free base.
   */
  *graphics = graphics_file;
  free (base);
}


/*
 * gln_startup_code()
 * gln_main()
 *
 * Together, these functions take the place of the original main().  The
 * first one is called from glkunix_startup_code(), to parse and generally
 * handle options.  The second is called from glk_main(), and does the real
 * work of running the game.
 */
static int
gln_startup_code (int argc, char *argv[])
{
  int argv_index;

  /* Handle command line arguments. */
  for (argv_index = 1;
       argv_index < argc && argv[argv_index][0] == '-'; argv_index++)
    {
      if (strcmp (argv[argv_index], "-ni") == 0)
        {
          gln_intercept_enabled = FALSE;
          continue;
        }
      if (strcmp (argv[argv_index], "-nc") == 0)
        {
          gln_commands_enabled = FALSE;
          continue;
        }
      if (strcmp (argv[argv_index], "-na") == 0)
        {
          gln_abbreviations_enabled = FALSE;
          continue;
        }
      if (strcmp (argv[argv_index], "-np") == 0)
        {
          gln_graphics_enabled = FALSE;
          continue;
        }
      if (strcmp (argv[argv_index], "-ne") == 0)
        {
          gln_prompt_enabled = FALSE;
          continue;
        }
      if (strcmp (argv[argv_index], "-nl") == 0)
        {
          gln_loopcheck_enabled = FALSE;
          continue;
        }
      return FALSE;
    }

  /*
   * Get the name of the game file.  Since we need this in our call from
   * glk_main, we need to keep it in a module static variable.  If the game
   * file name is omitted, then here we'll set the pointerto NULL, and
   * complain about it later in main.  Passing the message string around
   * like this is a nuisance...
   */
  if (argv_index == argc - 1)
    {
      gln_gamefile = argv[argv_index];
      gln_game_message = NULL;
#ifdef GARGLK
    {
      char *s;
      s = strrchr(gln_gamefile, '\\');
      if (s) garglk_set_story_name(s+1);
      s = strrchr(gln_gamefile, '/');
      if (s) garglk_set_story_name(s+1);
    }
#endif
    }
  else
    {
      gln_gamefile = NULL;
      if (argv_index < argc - 1)
        gln_game_message = "More than one game file was given"
                           " on the command line.";
      else
        gln_game_message = "No game file was given on the command line.";
    }

  /* All startup options were handled successfully. */
  return TRUE;
}

static void
gln_main (void)
{
  char *graphics_file = NULL;
  int is_running;

  /* Ensure Level 9 internal types have the right sizes. */
  if (!(sizeof (gln_byte) == 1
      && sizeof (gln_uint16) == 2 && sizeof (gln_uint32) == 4))
    {
      gln_fatal ("GLK: Types sized incorrectly, recompilation is needed");
      glk_exit ();
    }

  /* Create the main Glk window, and set its stream as current. */
  gln_main_window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
  if (!gln_main_window)
    {
      gln_fatal ("GLK: Can't open main window");
      glk_exit ();
    }
  glk_window_clear (gln_main_window);
  glk_set_window (gln_main_window);
  glk_set_style (style_Normal);

  /* If there's a problem with the game file, complain now. */
  if (!gln_gamefile)
    {
      assert (gln_game_message);
      gln_header_string ("Glk Level 9 Error\n\n");
      gln_normal_string (gln_game_message);
      gln_normal_char ('\n');
      glk_exit ();
    }

  /*
   * Given the basic game name, try to come up with a usable graphics
   * filenames.  The graphics file may be null.
   */
  gln_establish_picture_filename (gln_gamefile, &graphics_file);

  /*
   * Check Glk library capabilities, and note pictures are impossible if the
   * library can't offer both graphics and timers.  We need timers to create
   * the background "thread" for picture updates.
   */
  gln_graphics_possible = glk_gestalt (gestalt_Graphics, 0)
                          && glk_gestalt (gestalt_Timer, 0);

  /*
   * If pictures are impossible, clear pictures enabled flag.  That is, act
   * as if -np was given on the command line, even though it may not have
   * been.  If pictures are impossible, they can never be enabled.
   */
  if (!gln_graphics_possible)
    gln_graphics_enabled = FALSE;

  /* If pictures are possible, search for bitmap graphics. */
  if (gln_graphics_possible)
    gln_graphics_locate_bitmaps (gln_gamefile);

  /* Try to create a one-line status window.  We can live without it. */
/*
  gln_status_window = glk_window_open (gln_main_window,
                                       winmethod_Above | winmethod_Fixed,
                                       1, wintype_TextGrid, 0);
*/

  /*
   * The main interpreter uses rand(), but never seeds the random number
   * generator.  This can lead to predictability in games that might be
   * better with a little less, so here, we'll seed the random number
   * generator ourselves.
   */
  if (!gli_determinism)
      srand (time (NULL));
  else {
      /* When implementing predictable random number generation, I found that at least The Growing Pains of Adrian Mole breaks if a) the randomseed global never changes and b) it is set to certain values. So we generate a set array of 100  values here that are known to work */
      srand (1234);
      L9UINT16 seed = 2;
      for (int i = 0; i < 99; i++) {
          seed=(((seed<<8) + 0x0a - seed) <<2) + seed + 1;
          random_array[i] = seed;
      }
  }

  /* Repeat this game until no more restarts requested. */
  do
    {
      glk_window_clear (gln_main_window);

      /*
       * In a multi-file game, restarting may mean reverting back to part one
       * of the game.  So we have to encourage a re-lookup of the game name
       * at this point.
       */
      gln_gameid_game_name_reset ();

      /* Load the game, sending in any established graphics file. */
      errno = 0;
      if (!LoadGame (gln_gamefile, graphics_file))
        {
          if (gln_status_window)
            glk_window_close (gln_status_window, NULL);
          gln_header_string ("Glk Level 9 Error\n\n");
          gln_normal_string ("Can't find, open, or load game file '");
          gln_normal_string (gln_gamefile);
          gln_normal_char ('\'');
          if (errno != 0)
            {
              gln_normal_string (": ");
              gln_normal_string (strerror (errno));
            }
          gln_normal_char ('\n');

          /*
           * Nothing more to be done, so we'll free interpreter allocated
           * memory, then break rather than exit, to run memory cleanup and
           * close any open streams.
           */
          FreeMemory ();
          break;
        }

      /* Print out a short banner. */
      gln_header_string ("\nLevel 9 Interpreter, version 5.1\n");
      gln_banner_string ("Written by Glen Summers and David Kinder\n"
                         "Glk interface by Simon Baldwin\n\n");

      /*
       * Set the stop reason indicator to none.  A game will then exit with a
       * reason if we call StopGame(), or none if it exits of its own accord
       * (or with the "#quit" command, say).
       */
      gln_stop_reason = STOP_NONE;

      /* Start, or restart, watchdog checking. */
      gln_watchdog_start (GLN_WATCHDOG_TIMEOUT, GLN_WATCHDOG_PERIOD);

      /* Run the game until StopGame called, or RunGame() returns FALSE. */
      do
        {
          is_running = RunGame ();
          glk_tick ();

          /* Poll for watchdog timeout. */
          if (is_running && gln_watchdog_has_timed_out ())
            {
              gln_stop_reason = STOP_FORCE;
              StopGame ();
              break;
            }
        }
      while (is_running);

      /* Stop watchdog functions, and flush any pending buffered output. */
      gln_watchdog_stop ();
      gln_status_notify ();
      gln_output_flush ();

      /* Free interpreter allocated memory. */
      FreeMemory ();

      /*
       * Unset any "stuck" game 'cheating' flag.  This can get stuck on if
       * exit is forced from the #cheat mode in the Adrian Mole games, which
       * otherwise loop infinitely.  Unsetting the flag here permits restarts;
       * without this, the core interpreter remains permanently in silent
       * #cheat mode.
       */
      Cheating = FALSE;

      /*
       * If the stop reason is none, something in the game stopped itself, or
       * the user entered "#quit".  If the stop reason is force, the user
       * terminated because of an apparent infinite loop.  For both of these,
       * offer the choice to restart, or not (equivalent to exit).
       */
      if (gln_stop_reason == STOP_NONE || gln_stop_reason == STOP_FORCE)
        {
          gln_standout_string (gln_stop_reason == STOP_NONE
                               ? "\nThe game has exited.\n"
                               : "\nGame exit was forced.  The current game"
                                 " state is unrecoverable.  Sorry.\n");

          if (gln_confirm ("\nDo you want to restart? [Y or N] "))
            gln_stop_reason = STOP_RESTART;
          else
            gln_stop_reason = STOP_EXIT;
        }
    }
  while (gln_stop_reason == STOP_RESTART);

  /* Free any temporary memory that may have been used by graphics. */
  gln_graphics_cleanup ();
  gln_linegraphics_cleanup ();

  /* Close any open transcript, input log, and/or read log. */
  if (gln_transcript_stream)
    {
      glk_stream_close (gln_transcript_stream, NULL);
      gln_transcript_stream = NULL;
    }
  if (gln_inputlog_stream)
    {
      glk_stream_close (gln_inputlog_stream, NULL);
      gln_inputlog_stream = NULL;
    }
  if (gln_readlog_stream)
    {
      glk_stream_close (gln_readlog_stream, NULL);
      gln_readlog_stream = NULL;
    }

  /* Free any graphics file path. */
  free (graphics_file);
}


/*---------------------------------------------------------------------*/
/*  Linkage between Glk entry/exit calls and the real interpreter      */
/*---------------------------------------------------------------------*/

/*
 * Safety flags, to ensure we always get startup before main, and that
 * we only get a call to main once.
 */
static int gln_startup_called = FALSE,
           gln_main_called = FALSE;

/*
 * glk_main()
 *
 * Main entry point for Glk.  Here, all startup is done, and we call our
 * function to run the game.
 */
void
glk_main (void)
{
  assert (gln_startup_called && !gln_main_called);
  gln_main_called = TRUE;

  /* Call the interpreter main function. */
  gln_main ();
}


/*---------------------------------------------------------------------*/
/*  Glk linkage relevant only to the UNIX platform                     */
/*---------------------------------------------------------------------*/
#ifdef TRUE

#include "glkstart.h"

/*
 * Glk arguments for UNIX versions of the Glk interpreter.
 */
glkunix_argumentlist_t glkunix_arguments[] = {
  {(char *) "-nc", glkunix_arg_NoValue,
   (char *) "-nc        No local handling for Glk special commands"},
  {(char *) "-na", glkunix_arg_NoValue,
   (char *) "-na        Turn off abbreviation expansions"},
  {(char *) "-ni", glkunix_arg_NoValue,
   (char *) "-ni        No local handling for 'quit', 'restart',"
                        " 'save', and 'restore'"},
  {(char *) "-np", glkunix_arg_NoValue,
   (char *) "-np        Turn off pictures"},
  {(char *) "-ne", glkunix_arg_NoValue,
   (char *) "-ne        Turn off additional interpreter prompt"},
  {(char *) "-nl", glkunix_arg_NoValue,
   (char *) "-nl        Turn off infinite loop detection"},
  {(char *) "", glkunix_arg_ValueCanFollow,
   (char *) "filename   game to run"},
  {NULL, glkunix_arg_End, NULL}
};


/*
 * glkunix_startup_code()
 *
 * Startup entry point for UNIX versions of Glk interpreter.  Glk will
 * call glkunix_startup_code() to pass in arguments.  On startup, we call
 * our function to parse arguments and generally set stuff up.
 */
int
glkunix_startup_code (glkunix_startup_t * data)
{
  assert (!gln_startup_called);
  gln_startup_called = TRUE;

#ifdef GARGLK
  garglk_set_program_name("Level 9 5.1");
  garglk_set_program_info(
      "Level 9 5.1 by Glen Summers, David Kinder\n"
      "Alan Staniforth, Simon Baldwin and Dieter Baron\n"
      "Glk Graphics support by Tor Andersson\n");
#endif

  return gln_startup_code (data->argc, data->argv);
}
#endif /* __unix */


/*---------------------------------------------------------------------*/
/*  Glk linkage relevant only to the Mac platform                      */
/*---------------------------------------------------------------------*/
#ifdef TARGET_OS_MAC

#include "macglk_startup.h"

/* Additional Mac variables. */
static strid_t gln_mac_gamefile = NULL;
static short gln_savedVRefNum = 0;
static long gln_savedDirID = 0;


/*
 * gln_mac_whenselected()
 * gln_mac_whenbuiltin()
 * macglk_startup_code()
 *
 * Startup entry points for Mac versions of Glk interpreter.  Glk will
 * call macglk_startup_code() for details on what to do when the app-
 * lication is selected.  On selection, an argv[] vector is built, and
 * passed to the normal interpreter startup code, after which, Glk will
 * call glk_main().
 */
static Boolean
gln_mac_whenselected (FSSpec * file, OSType filetype)
{
  static char *argv[2];
  assert (!gln_startup_called);
  gln_startup_called = TRUE;

  /* Set the WD to where the file is, so later fopens work. */
  if (HGetVol (0, &gln_savedVRefNum, &gln_savedDirID) != 0)
    {
      gln_fatal ("GLK: HGetVol failed");
      return FALSE;
    }
  if (HSetVol (0, file->vRefNum, file->parID) != 0)
    {
      gln_fatal ("GLK: HSetVol failed");
      return FALSE;
    }

  /* Put a CString version of the PString name into argv[1]. */
  argv[1] = gln_malloc (file->name[0] + 1);
  BlockMoveData (file->name + 1, argv[1], file->name[0]);
  argv[1][file->name[0]] = '\0';
  argv[2] = NULL;

  return gln_startup_code (2, argv);
}

static Boolean
gln_mac_whenbuiltin (void)
{
  /* Not implemented yet. */
  return TRUE;
}

Boolean
macglk_startup_code (macglk_startup_t * data)
{
  static OSType gln_mac_gamefile_types[] = { 'LVL9' };

  data->startup_model = macglk_model_ChooseOrBuiltIn;
  data->app_creator = 'cAGL';
  data->gamefile_types = gln_mac_gamefile_types;
  data->num_gamefile_types = sizeof (gln_mac_gamefile_types)
    / sizeof (*gln_mac_gamefile_types);
  data->savefile_type = 'BINA';
  data->datafile_type = 0x3f3f3f3f;
  data->gamefile = &gln_mac_gamefile;
  data->when_selected = gln_mac_whenselected;
  data->when_builtin = gln_mac_whenbuiltin;
  /* macglk_setprefs(); */
  return TRUE;
}
#endif /* TARGET_OS_MAC */
