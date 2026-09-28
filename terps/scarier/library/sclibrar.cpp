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
#include "sclibrar.h"


/*
 * The core of the library, split by topic.  These fragments are one
 * translation unit and share their statics, so their order matters: each
 * may use anything defined in the ones before it.  The peripheral topics
 * (sclibrar_print.cpp, _room, _meta, _go, _topic, _read, _verbs, _sitstand,
 * _misc and _talk) are translation units of their own; sclibrar.h declares
 * what crosses between them and this file.
 */
#include "sclibrar_disambig.inc"
#include "sclibrar_examine.inc"
#include "sclibrar_dispatch.inc"
#include "sclibrar_take.inc"
#include "sclibrar_takefrom.inc"
#include "sclibrar_drop.inc"
#include "sclibrar_open.inc"
#include "sclibrar_put.inc"
#include "sclibrar_battle.inc"
#include "sclibrar_refuse.inc"
#include "sclibrar_verbobj.inc"
