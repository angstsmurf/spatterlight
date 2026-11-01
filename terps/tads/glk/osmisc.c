/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

/*
 * osmisc.c -- misc leftovers
 *
 * The contract of each os_* function here is in tads2/osifc.h; comments say
 * only what this port does.
 */

#include <string.h>
#include <time.h>

#include <unistd.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>	/* for Sleep() */
#endif

#include "os.h"

/* No mapping files here: the empty name */
void os_gen_charmap_filename(char *filename, char *internal_id, char *argv0)
{
    filename[0] = 0;
}


/* ------------------------------------------------------------------------ */
/* Seeds for TADS's random number generators */

#include "glk.h"
#include "oshtml.h"

#ifdef SPATTERLIGHT

#include "randomness.h"
extern glui32 gli_determinism;
extern int gli_sa_delays;

/*
 *   All randomness draws from the shared erkyrath_random()
 *   (terps/common_utils/randomness.c), the same RNG used by the Scott,
 *   TaylorMade, Plus and Comprehend ports.  These functions are TADS's seed
 *   sources (os_rand seeds the LCG; os_gen_rand_bytes seeds ISAAC, the default
 *   RNG), so all of TADS's randomness
 *   is reproducible under a fixed seed.  The generator is seeded once: the
 *   fixed seed 1234 under the determinism testing theme, otherwise 0 to let it
 *   pick a platform-native seed.
 */
static void ensure_erkyrath_seeded(void)
{
    static int seeded = 0;
    if (!seeded) {
        set_erkyrath_random(gli_determinism ? 1234 : 0);
        seeded = 1;
    }
}

#endif

time_t time(time_t *);

void os_rand(long *seed)
{
#ifdef SPATTERLIGHT
    ensure_erkyrath_seeded();
    *seed = (long)erkyrath_random();
#else
    time_t t;
    time( &t );
    *seed = (long)t;
#endif
}

#ifndef SPATTERLIGHT
/*
 *   Xorshift PRNG from http://www.jstatsoft.org/v08/i14/paper
 */
static uint32_t seed[4];

static unsigned long xorshift(void)
{
    uint32_t x;
    x = seed[0] ^ (seed[0] << 11);
    seed[0] = seed[1];
    seed[1] = seed[2];
    seed[2] = seed[3];

    return (seed[3] = (seed[3] ^ (seed[3] >> 19)) ^ (x ^ (x >> 8)));
}

static void xorinit(void)
{
    seed[0] = (uint32_t)time(NULL);
    seed[1] = seed[0] << 1;
    seed[2] = getpid();
    seed[3] = seed[1] & ~seed[2];
    xorshift();
}
#endif

void os_gen_rand_bytes(unsigned char *buf, size_t len)
{
#ifdef SPATTERLIGHT
    size_t ct;
    ensure_erkyrath_seeded();
    for (ct = 0; ct < len; ct++)
        buf[ct] = (unsigned char)(erkyrath_random() & 0xFF);
#else
    /* seed the Xorshift PRNG */
    xorinit();

    int ct, val;
    for (ct = 0; ct < len; ct++)
    {
        val = (int)xorshift();
        buf[ct] = (val - 1) & 0xFF;
    }
#endif
}


/* flush any buffered display output */
void os_flush(void)
{
    /* the status line's held-back spaces stay with it */
    if (os_get_status() == 0)
        oshtml_flush();
}

/* Nothing to do: Glk windows draw themselves */
void os_update_display()
{
}

/* ------------------------------------------------------------------------ */
/* Nothing to do: there's no terminal to put in plain mode */
void os_plain(void)
{
}
/* Nothing to do */
void os_nonstop_mode(int flag)
{
}

void os_sleep_ms(long delay_in_milliseconds)
{
#ifdef SPATTERLIGHT
    /*
     *   Sleeps are the game's delays, which the player can turn off.  Under
     *   the determinism testing theme the timer wait below moves the virtual
     *   clock on, and so does skipping it here.
     */
    if (!gli_sa_delays)
    {
        if (osglk_vclock())
            osglk_vclock_advance((unsigned long)delay_in_milliseconds);
        return;
    }
#endif
#ifdef GLK_TIMERS
    if (delay_in_milliseconds > 0)
        osglk_sleep_ms((unsigned long)delay_in_milliseconds);
    return;
#endif
#ifdef _WIN32
    Sleep(delay_in_milliseconds);
#else
    usleep((useconds_t)(delay_in_milliseconds * 1000));
#endif
}

/* Nothing to do: no pause before exit */
void os_expause(void)
{
}

/* Nothing to do: Glk has no break signal, and os_break is a loop guard */
void os_instbrk(int install)
{
}

/* Nothing to do: Glk's file prompt chooses the name */
void os_set_save_ext(const char *ext)
{
}

/* Display and file contents are UTF-8 where Glk has Unicode; filenames ASCII */
void os_get_charmap(char *mapname, int charmap_id)
{
#ifndef GLK_UNICODE
    strncpy(mapname, "asc7dflt", 32);
#else
    switch (charmap_id)
    {
        case OS_CHARMAP_DISPLAY:
        case OS_CHARMAP_FILECONTENTS:
            strncpy(mapname, "utf-8", 32);
            break;

        case OS_CHARMAP_FILENAME:
        default:
            strncpy(mapname, "asc7dflt", 32);
            break;
    }
#endif
}

/*
 *   A loop guard.  TADS 2 asks os_break after every 1000 p-code instructions.
 *   A game that runs 100 million of them (about five seconds) without reading
 *   input or writing text is taken to be stuck, and is stopped as if the
 *   player had pressed break: the engine undoes the command.  (Six, given a
 *   one-letter answer at its death prompt in HTML mode, loops forever without
 *   asking again.)
 */
#define STUCK_BREAKS 100000
static unsigned long idle_breaks;

void osglk_not_idle(void)
{
    idle_breaks = 0;
}

int os_break(void)
{
    if (++idle_breaks >= STUCK_BREAKS)
    {
        os_printz("\n[The game was stuck in a loop, so the last command "
                  "has been cancelled.]\n");
        idle_breaks = 0;
        return 1;
    }
    return 0;
}

/*
 *   Implementation of vasprintf for MinGW.
 *   NOTE: Because vasprintf is not standard, unconditionally define it
 *   here, but with a name that won't clash with versions that do exist.
 */
static int xvasprintf(char **sptr, const char *fmt, va_list argv)
{
    va_list argv_copy;
    int wanted;

    va_copy(argv_copy, argv);
    wanted = vsnprintf(*sptr = NULL, 0, fmt, argv);
    if((wanted < 0) || ((*sptr = malloc( 1 + wanted )) == NULL))
        return -1;

    return vsprintf(*sptr, fmt, argv_copy);
}

int os_asprintf(char **bufptr, const char *fmt, ...)
{
    int retval;
    va_list argv;
    va_start(argv, fmt);
    retval = xvasprintf(bufptr, fmt, argv);
    va_end(argv);
    return retval;
}

int os_vasprintf(char **bufptr, const char *fmt, va_list ap)
{
    return xvasprintf(bufptr, fmt, ap);
}
