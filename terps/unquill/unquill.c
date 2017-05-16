/* Pull Quill data from a .SNA file.  *
* Output to stdout.                  */

/* Format of the Quill database:

Somewhere in memory (6D04h for vanilla Quill with no Illustrator/Patch/Press)
there is a colour definition table:

DEFB	10h,ink,11h,paper,12h,flash,13h,bright,14h,inverse,15h,over
DEFB	border

We use this as a magic number to find the Quill database in memory. After
the colour table is a table of values:

DEFB	no. of objects player can carry at once
DEFB	no. of objects
DEFB	no. of locations
DEFB	no. of messages

then a table of pointers:

DEFW	rsptab	;Response table
DEFW	protab	;Process table
DEFW	objtop	;Top of object descriptions
DEFW	loctop	;Top of locations.
DEFW	msgtop	;Top of messages.
**
DEFW	conntab	;Table of connections.
DEFW	voctab	;Table of words.
DEFW	oloctab	;Table of object inital locations.
DEFW	free	;[Early] Start of free memory
;[Late]  Word/object mapping
DEFW	freeend	;End of free memory
DEFS	3	;?
DEFB	80h,dict ;Expansion dictionary (for compressed games).
;Dictionary is stored in ASCII, high bit set on last
;letter of each word.
rsptab:	...		     ;response	- both stored as: DB verb, noun
protab: ...          ;process                     DW address-of-handler
;                               terminated by verb 0.
Handler format is:
DB conditions...0FFh
DB actions......0FFh
objtab:	...		;objects	- all are stored with all bytes
objtop:	DEFW	objtab        complemented to deter casual hackers.
DEFW    object1
DEFW    object2 etc.
loctab:	...		;locations       Texts are terminated with 0E0h
loctop:	DEFW    loctab           (ie 1Fh after decoding).
DEFW    locno1
DEFW    locno2 etc.
msgtab:	...		;messages
msgtop:	DEFW    msgtab
DEFW    mess1
DEFW    mess2 etc.
conntab: ...    ;connections    - stored as DB WORD,ROOM,WORD,ROOM...
each entry is terminated with 0FFh.

voctab: ...		;vocabulary     - stored as DB 'word',number - the
word is complemented to deter hackers.
Table terminated with a word entry all
five bytes of which are 0 before
decoding.
oloctab: ...    ;initial locations of objects. 0FCh => not created
0FDh => worn
0FEh => carried
0FFh => end of table

In later games (those made with Illustrator?), there is an extra byte 
(the number of system messages) after the number of messages, and at 
the label ** the following word is inserted:

DEFW    systab  ;Points to word pointing to table of system
;messages.
systab: DEFW    sysm0
DEFW    sysm1 etc.

The address of the user-defined graphics is stored at 23675. In "Early" games,
the system messages are at UDGs+168.

CPC differences:

* I don't know where the UDGs are.
* Strings are terminated with 0xFF (0 after decoding) rather than 0xE0 
(0x1F).
* No Spectrum colour codes in strings. Instead, the 
code 0x14 means "newline" and 0x09 means "toggle reverse video".
There are other CPC colour codes in the range 0x01-0x0A, but I don't
know their meaning.
* I have assumed that the database always starts at 0x1BD1, which it does 
in the snapshots I have examined.

Commodore 64 differences:

* I don't know where the UDGs are.
* Strings are terminated with 0xFF (0 after decoding) rather than 0xE0 
(0x1F).
* No Spectrum colour codes in strings. 
* I have assumed that the database always starts at 0x0804, which it does 
in the snapshots I have examined.


*/


#define MAINMODULE
#include "unquill.h"    /* Function prototypes */

#include <setjmp.h>
#include <getopt.h>

static ushort ucptr;

void die(char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    glk_put_string("\n\n*** Fatal Error ***\n\n");
    glk_put_string(buf);
    glk_exit();
}

void glk_printf(char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    glk_put_string(buf);    
}

/* The only command-line argument is the filename. */
glkunix_argumentlist_t glkunix_arguments[] = {
    { "-r", glkunix_arg_ValueFollows, "-r: Restore a save file." },
    { "-l", glkunix_arg_NoValue, "-l: Attempt to interpret as a 'later' type Quill file." },
    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
    { NULL, glkunix_arg_End, NULL }
};

void usage(char *title)
{
    fprintf(stderr,"UnQuill v" VERSION ". Play .SNA snapshots of Quill adventure games.\n");
    fprintf(stderr,"    -l           -- Attempt to interpret as a 'later' type Quill file.\n");
    fprintf(stderr,"    -r file.sav  -- restore a saved game\n");
    exit(1);
}

int glkunix_startup_code(glkunix_startup_t *data)
{
    char *restorefile;
    int c;
    
    while ((c = getopt(data->argc, data->argv, "lr:")) != -1)
    {
	switch (c)
	{
	    case 'l': dbver = 10; break;
	    case 'r': restorefile = optarg; break;
	    default: usage(data->argv[0]); break;
	}
    }
    
    if (data->argc == optind)
	usage(data->argv[0]);
    
    inname = data->argv[optind];
    
    if ((infile = fopen(inname,"rb")) == NULL)
    {
	fprintf(stderr, "Cannot open file '%s'\n", inname);
	return FALSE;
    }
    
    return TRUE;
}

static jmp_buf myenv;

void glk_main(void)
{
    ushort n, seekpos;
    ushort zxptr = 0;
    
    /* Set up GLK window. */
    
    mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
    if (!mainwin)
    {
	fprintf(stderr, "could not create glk window!\n");
	exit(1);
    }
    glk_set_window(mainwin);
    
    /* Load the snapshot. To save space, we ignore the printer
    buffer and the screen, which can contain nothing of value. */
    
    /* << v0.7: Check for CPC6128 format */
    
    if (fread(snapid, sizeof(snapid), 1, infile) != 1)
    {
	die("Not in Spectrum, CPC or C64 snapshot format.");
    }
    
    arch       = ARCH_SPECTRUM;
    seekpos    = 0x1C1B;	/* Number of bytes to skip in the file */
    mem_base   = 0x5C00;	/* First address loaded */
    mem_size   = 0xA400;	/* Number of bytes to load from it */
    mem_offset = 0x3FE5;	/* Load address of snapshot in host system memory */
    
    /* CPCEMU snapshot */
    if (!memcmp(snapid, "MV - SNA", 9))
    {
	arch = ARCH_CPC;
	
	seekpos    = 0x1C00;
	mem_base   = 0x1B00;
	mem_size   = 0xA500;
	mem_offset = -0x100;
	dbver = 10;	/* CPC engine is equivalent to the "later" Spectrum one. */
	
	glk_put_string("CPC snapshot signature found.\n");
    }
    
    /* VICE snapshot */
    if (!memcmp(snapid, "VICE Snapshot File\032", 19))
    {
	arch = ARCH_C64;
	
	seekpos    =  0x874;
	mem_base   =  0x800;
	mem_size   = 0xA500;
	mem_offset =  -0x74;
	dbver = 5;	/* C64 engine is between the two Spectrum ones. */
	
	glk_put_string("C64 snapshot signature found.\n");
    }
    
    /* >> v0.7 */
    
    /* Skip screen/printer buffer/registers and load the rest */
    
    if (fseek(infile,seekpos,SEEK_SET))
    {
	perror(inname);
	exit(1);
    }
    
    /* Load file */
    
    if (fread(zxmemory, mem_size, 1, infile) != 1)
    {
	perror (inname);
	exit(1);
    }
    
    /* .SNA read ok. Find a Quill signature */
    
    switch(arch)
    {
	case ARCH_SPECTRUM:
	    
	    /* I could _probably_ assume that the Spectrum database is 
	    * always at one location for "early" and another for "late"
	    * games. But this method is safer. */
	    
	    for (n = 0x5C00; n < 0xFFF5; n++)
	    {
		if ((zmem(n  ) == 0x10) && (zmem(n+2) == 0x11) && 
		    (zmem(n+4) == 0x12) && (zmem(n+6) == 0x13) && 
		    (zmem(n+8) == 0x14) && (zmem(n+10) == 0x15))
		{
		    glk_put_string("Quill signature found.\n");
		    found = 1;
		    zxptr = n + 13;
		    break;
		}
	    }
	    break;
	    
	case ARCH_CPC: 
	    found = 1;
	    zxptr = 0x1BD1;	/* From guesswork: CPC Quill files always seem to start at 0x1BD1 */
	    break;
	    
	case ARCH_C64: 
	    found = 1;
	    zxptr = 0x804;	/* From guesswork: C64 Quill files always seem to start at 0x804 */
	    break;
    }
    
    if (!found)
    {
	die("%s does not seem to be a valid Quill .SNA file."
	    "If you know it is, you have found a bug.", inname);
    }
  
    
    /* If we later bomb on an invalid address, come back here and try with the -l option */
    if (setjmp(myenv))
    {
	if (dbver == 10)
	{
	    die("Invalid address requested.");
	}
	else
	{
	    glk_put_string("Retrying as a later version game.\n");
	    dbver = 10;
	}
    }
        
    /* Setup memory and tables */
    
    ucptr   = zxptr;
    maxcar1 = maxcar = zmem (zxptr);	/* Player's carrying capacity */
    nobj             = zmem (zxptr +  1);	/* No. of objects */
    nloc             = zmem (zxptr +  2);	/* No. of locations */
    nmsg             = zmem (zxptr +  3);	/* No. of messages */
    if (dbver)
    {
	++zxptr;
	nsys     = zmem (zxptr +  3);	/* No. of system messages */
	vocab    = zword(zxptr + 18);	/* Words list */
	dict     = zxptr + 29;		/* Expansion dictionary */
    }
    else vocab = zword(zxptr + 16);
    
    resptab            = zword(zxptr +  4);
    proctab            = zword(zxptr +  6);
    objtab             = zword(zxptr +  8);
    loctab             = zword(zxptr + 10);
    msgtab             = zword(zxptr + 12);
    if (dbver) sysbase = zword(zxptr + 14);
    else       sysbase = zword(23675) + 168; /* Just after the UDGs */ 
    conntab            = zword(zxptr + 14 + ( dbver ? 2 : 0));
    if(dbver) objmap   = zword(zxptr + 22);
    postab             = zword(zxptr + 18 + ( dbver ? 2 : 0 ));
    
    /* Run the game */
    
    glk_put_string("\n");
    
    outfile = stderr;
    running = 1;
   
    ramsave[0x100] = 0; /* No position RAMsaved */
    
    while (running)
    {
	estop = 0;   
	srand(1);
	oopt  = 'T';        /* Outputs in plain text */
	initgame(zxptr); /* Initialise the game */
	playgame(zxptr); /* Play it */ 
	if (estop)
	{
	    estop=0;	/* Emergency stop operation, game restarts */
	    continue;   /* automatically */
	}
	sysmess(13);
	opch32('\n');
	if (yesno()=='N')
	{
	    running=0;
	    sysmess(14);
	}
    }
    
    glk_put_string("\n");
}



/* All Spectrum memory accesses are routed through this procedure.
* If TINY is defined, this accesses the .sna file directly.
*/
uchar zmem(ushort addr)
{
    if (addr < mem_base || (arch != ARCH_SPECTRUM && 
			    (addr >= (mem_base + mem_size))))
    {
	glk_printf("\n\n*** Invalid address %4.4x requested ***\n\n", addr);
	if (arch != ARCH_SPECTRUM)
	    glk_put_string("Probably not a Quilled game.\n");
	else
	{
	    longjmp(myenv, 1);
	}
	exit(1);
    }
    
    return zxmemory[addr - mem_base];
}



ushort zword(ushort addr)
{
    return (ushort)(zmem(addr) + (256 * zmem(addr + 1)));
}



void dec32(ushort v)
{
    char decbuf[6];
    int i;
    
    sprintf(decbuf,"%d",v);
    i=0;
    while (decbuf[i]) opch32(decbuf[i++]);
}


/* Output a character, assuming 32-column screen */
void opch32(char ch)
{
#if 0 // original 32/40 column screen exact replica output
    glk_put_char(ch);
    if (ch == '\n') xpos = 0;
    else if (ch == 8) xpos--;
    else if (arch == ARCH_SPECTRUM && xpos == 31) opch32('\n');
    else if (arch != ARCH_SPECTRUM && xpos == 39) opch32('\n');
    else xpos++;
    
#else // reformatted ...
    
    static int space = 0;
    static int linefeed = 0;
    
    /* inhibit multiple spaces and linefeeds and spaces at the beginning of a line */
    if (!(space && ch == ' ') && !(linefeed && ch == '\n') && !(xpos == 0 && ch == ' '))
	glk_put_char(ch);
    
    /* count spaces */
    if (ch == ' ')
	space = 1;
    else
	space = 0;
    
    if (ch == '\n')
    {
	if (linefeed == 0)
	    glk_put_char('\n'); // an extra paragraph breaking space
	if (linefeed == 5)
	    glk_put_char('\n'); // a clear screen really ... just add some more space
	linefeed ++;
	xpos = 0;
    }
     
    else if (ch == 8)
    {
	xpos--;
    }
    
    /* autowrap ... in the original output  */
    else if ((arch == ARCH_SPECTRUM && xpos == 31) ||
	     (arch != ARCH_SPECTRUM && xpos == 39))
    {
	/* ended line directly after a word, so we need an extra space */
	if (!space)
	    glk_put_char(' ');
	xpos = 0;
    }
    
    else
    {
	linefeed = 0;
	xpos++;
    }
    
#endif
}

int getch()
{
    event_t event;
    
    glk_request_char_event(mainwin);

    while (1)
    {
	glk_select(&event);
	if (event.type == evtype_CharInput)
	    break;
    }
    
    return event.val1;
}

void myreadline(char *buf, int cap)
{
    event_t ev;
    
    if (xpos == 0)
	glk_put_string("> ");
    
    glk_request_line_event(mainwin, buf, cap, 0);

    while (1)
    {
	glk_select(&ev);
	if (ev.type == evtype_LineInput)
	    break;
    }
    
    /* The line we have received in commandbuf is not null-terminated */
    buf[ev.val1] = '\0';	/* i.e., the length */
}
