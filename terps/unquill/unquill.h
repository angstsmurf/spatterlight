/* UnQuill: Disassemble and play games written with the "Quill" adventure game system
    Copyright (C) 1996,1999	John Elliott
    Copyright (C) 2005,		Tor Andersson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "glk.h"
#include "glkstart.h"

#define VERSION "0.8.5/tor"
#define BUILDDATE "2005-09-06"

typedef unsigned char uchar;     /* for brevity */
typedef unsigned short ushort;   /* -- ditto -- */

#define NIL  100
#define LOC  101
#define MSG  102
#define OBJ  103
#define SWAP 104
#define PLC  105

void glk_printf(char *fmt, ...);
void myreadline(char *buf, int cap);
int getch(void);

uchar zmem(ushort);
ushort zword(ushort);
void clrscr(void);
void oneitem(ushort,uchar);
void listitems(char *,ushort,ushort);
void listwords(ushort);
void listcond(ushort);
void listconn(ushort,uchar);
void listpos(ushort,uchar);
void listmap(ushort,uchar);
void listudgs(ushort);
void listfont(ushort);
void listgfx(void);
void opword(ushort);
void opcond(ushort *);
void opact(ushort *);
void opch32(char);
void opcact(char,char *);
void expch (uchar, ushort *);
void expdict (uchar, ushort *);

char present(uchar);
uchar doproc(ushort,uchar,uchar);
void listat(uchar);
void initgame(ushort);
void playgame(ushort);
void sysmess(uchar);
uchar ffeq(uchar,uchar);
uchar condtrue(ushort);
uchar autobj(uchar);
uchar runact(ushort,uchar);
uchar cplscmp(ushort, char *);
uchar matchword(char **);
void usage(char *);
void dec32(ushort);
char yesno(void);
void savegame(void);
void loadgame(void);

void inform_src(ushort);
void zcode_binary(void);

/* Supported architectures */
#define ARCH_SPECTRUM 0
#define ARCH_CPC      1
#define ARCH_C64      2


#ifdef MAINMODULE

winid_t mainwin = 0;

uchar zxmemory[0xA500];    /* Spectrum memory from 0x5C00 up 
			    * In CPC mode, CPC memory from 0x1B00 to 0xBFFF */

char arch = ARCH_SPECTRUM;		  /* Architecture */
char copt=0;		  /* -C option */
char gopt=0;		  /* -G option */
char mopt=0;		  /* -M option */
char oopt='T';            /* Output option */
char xpos=0;              /* Used in msg display */
char dbver=0;             /* Which format of database? 0 = early Spectrum
                           *                          10 = later Spectrum/CPC */
ushort ofarg=0;            /* Output to file or stdout? */
ushort vocab,dict;	  /* Spectrum address of vocabulary & dictionary */
char verbose=0;           /* Output tables verbosely? */
char nobeep=0;            /* Peace and quiet? */
char running=0;           /* Actually playing the game? */
ushort loctab;
ushort objtab;             /* Tables of locations, objects, messages */
ushort msgtab;
ushort sysbase;		  /* Base of system messages */
ushort conntab;            /* Connections table */
ushort postab;             /* Object start positions table */
ushort objmap;		  /* Word-to-object map */
ushort proctab, resptab;	  /* Process and Response tables */
char indent;              /* Listing in single or multiple mode? */
char *inname;             /* Input filename */

uchar fileid=0xFF;	  /* Save file identity byte */
uchar ramsave[0x101];	  /* RAM save buffer */
uchar flags[37];           /* The Quill flags, 0-36. */
#define TURNLO flags[31]  /* (31-36 are special */
#define TURNHI flags[32]  /* Meanings of the special flags */
#define VERB flags[33]
#define NOUN flags[34]
#define CURLOC flags[35]  /* flags[36] == 0 */
uchar objpos[255];         /* Positions of objects */
uchar maxcar;		  /* Max no. of portable objects */
uchar maxcar1;		  /* As maxcar - later games can change it on the fly */
#define NUMCAR flags[1]   /* Number of objects currently carried */
uchar nobj;                /* No. of objects */
uchar nsys;		  /* No. of system messages */
uchar alsosee=1;		  /* Message "You can also see" */
uchar estop;		  /* Emergency stop flag */

FILE *infile, *outfile;

uchar found = 0, skipc = 0, skipf = 0, skipo = 0, skipm = 0, skipn = 0,skipl = 0,
             skipv = 0, skips = 0, skipu = 0, skipg = 0, nloc, nmsg;
uchar comment_out = 0;

ushort mem_base;          /* Minimum address loaded from snapshot     */
ushort mem_size;          /* Size of memory loaded from snapshot      */
short mem_offset;        /* Load address of snapshot in physical RAM */
char snapid[20];


#else /* def MAINMODULE */

extern winid_t mainwin;

extern uchar zxmemory[0xA400];

extern char copt, gopt, mopt;
extern char oopt, xpos, dbver,verbose,nobeep,running,indent,*inname;
extern ushort ofarg,vocab,dict,loctab,objtab,msgtab,sysbase,conntab,objmap;
extern ushort postab, proctab, resptab;
extern uchar comment_out;

extern uchar fileid,ramsave[0x101],flags[37];
#define TURNLO flags[31] 
#define TURNHI flags[32] 
#define VERB flags[33]
#define NOUN flags[34]
#define CURLOC flags[35]
#define NUMCAR flags[1]
extern uchar objpos[255],maxcar,maxcar1,nobj,nsys,alsosee,estop;		  /* Emergency stop flag */
extern FILE *infile, *outfile;

extern uchar skipc, skipo, skipm, skipn, skipl, skipv, skips, skipu, skipg;
extern uchar found, nloc, nmsg;
extern char arch;

extern ushort mem_base;          /* Minimum address loaded from snapshot     */
extern ushort mem_size;          /* Size of memory loaded from snapshot      */
extern short mem_offset;        /* Load address of snapshot in physical RAM */
extern char snapid[20];

#endif /* def MAINMODULE */

