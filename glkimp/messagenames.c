//
//  messagenames.c
//  Spatterlight
//
//  Created by Administrator on 2026-04-04.
//
//  Names of the glkimp protocol messages, for logging. Each entry is keyed
//  on its enum value, so the table cannot fall out of step with protocol.h:
//  a message added there without a name here reads as NULL, not as the name
//  of its neighbour.

#include <stddef.h>
#include <stdint.h>

#include "glk.h"
#include "protocol.h"
#include "messagenames.h"

#define NAME(m) [m] = #m

const char *msgnames[] = {
    NAME(NOREPLY),         NAME(OKAY),             NAME(ERROR),       NAME(HELLO),
    NAME(PROMPTOPEN),      NAME(PROMPTSAVE),       NAME(NEWWIN),      NAME(DELWIN),
    NAME(SIZWIN),          NAME(CLRWIN),           NAME(MOVETO),      NAME(PRINT),
    NAME(UNPRINT),         NAME(MAKETRANSPARENT),  NAME(STYLEHINT),   NAME(CLEARHINT),
    NAME(STYLEMEASURE),    NAME(SETBGND),          NAME(REFRESH),     NAME(SETTITLE),
    NAME(AUTOSAVE),        NAME(RESET),            NAME(BANNERCOLS),  NAME(BANNERLINES),
    NAME(TIMER),           NAME(INITCHAR),         NAME(CANCELCHAR),
    NAME(INITLINE),        NAME(CANCELLINE),       NAME(SETECHO),     NAME(TERMINATORS),
    NAME(INITMOUSE),       NAME(CANCELMOUSE),      NAME(FILLRECT),    NAME(FINDIMAGE),
    NAME(LOADIMAGE),       NAME(SIZEIMAGE),        NAME(DRAWIMAGE),   NAME(FLOWBREAK),
    NAME(NEWCHAN),         NAME(DELCHAN),          NAME(FINDSOUND),   NAME(LOADSOUND),
    NAME(SETVOLUME),       NAME(PLAYSOUND),        NAME(STOPSOUND),   NAME(PAUSE),
    NAME(UNPAUSE),         NAME(BEEP),
    NAME(SETLINK),         NAME(INITLINK),         NAME(CANCELLINK),  NAME(SETZCOLOR),
    NAME(SETREVERSE),      NAME(QUOTEBOX),         NAME(SHOWERROR),   NAME(CANPRINT),
    NAME(PURGEIMG),        NAME(MENUITEM),

    NAME(NEXTEVENT),       NAME(EVTARRANGE),       NAME(EVTREDRAW),   NAME(EVTLINE),
    NAME(EVTKEY),          NAME(EVTMOUSE),         NAME(EVTTIMER),    NAME(EVTHYPER),
    NAME(EVTSOUND),        NAME(EVTVOLUME),        NAME(EVTPREFS),    NAME(EVTQUIT),
    NAME(EVTTEST)
};

const size_t msgnames_count = sizeof msgnames / sizeof msgnames[0];
