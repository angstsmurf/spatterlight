/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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
 * Module notes:
 *
 * o Ensure module messages precisely match the real Runner ones.  This
 *   matters for ALRs.
 *
 * o Capacity checks on the player and on containers are implemented, but
 *   may not be right.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const scr_char NUL = '\0';
static const scr_char COMMA = ',';
enum
{ SECS_PER_MINUTE = 60,
  MINS_PER_HOUR = 60,
  SECS_PER_HOUR = 3600
};
enum { LIB_ALLOCATION_AVOIDANCE_SIZE = 128 };

/*
 * A gathered list of objects, NPCs, or directions, and the printer used for
 * one of its elements.  See lib_print_list() below.
 */
typedef std::vector<scr_int> lib_list_t;
typedef void (*lib_print_item_t) (scr_gameref_t game, scr_int item);

/* Trace flag, set before running. */
static scr_bool lib_trace = FALSE;


/*
 * The library proper, split by topic.  The fragments are one translation
 * unit and share this file's statics, so their order matters: each may use
 * anything defined in the ones before it.
 */
#include "sclibrar_print.inc"
#include "sclibrar_room.inc"
#include "sclibrar_meta.inc"
#include "sclibrar_go.inc"
#include "sclibrar_disambig.inc"
#include "sclibrar_examine.inc"
#include "sclibrar_dispatch.inc"
#include "sclibrar_take.inc"
#include "sclibrar_takefrom.inc"
#include "sclibrar_drop.inc"
#include "sclibrar_open.inc"
#include "sclibrar_topic.inc"
#include "sclibrar_put.inc"
#include "sclibrar_read.inc"
#include "sclibrar_battle.inc"
#include "sclibrar_verbs.inc"
#include "sclibrar_sitstand.inc"
#include "sclibrar_misc.inc"
#include "sclibrar_talk.inc"
#include "sclibrar_refuse.inc"
#include "sclibrar_verbobj.inc"
