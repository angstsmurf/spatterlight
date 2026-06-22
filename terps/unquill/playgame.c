/* UnQuill: Disassemble games written with the "Quill" adventure game system
    Copyright (C) 1996-2000  John Elliott

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


#include "unquill.h"


/* --- Interpreter-level UNDO ------------------------------------------------
 *
 * The Quill database itself has no notion of undo, so this is provided by the
 * interpreter as a convenience, in the same spirit as the Scott and Taylor
 * interpreters in this tree. The complete mutable game state is small (the 37
 * flags and the object-position table, plus a few runtime variables that
 * later games can change on the fly), so we keep a bounded stack of snapshots
 * and let "#undo" walk back through it one move at a time.
 *
 * A snapshot is captured once per real command prompt, immediately before the
 * player's line is read, mirroring the state shown to the player. */

#define MAX_UNDOS 64

typedef struct UndoState {
	uchar flags[37];
	uchar objpos[255];
	uchar maxcar1;
	uchar alsosee;
	uchar fileid;
	struct UndoState *prev;
} UndoState;

static UndoState *undo_top = NULL;	/* most recent snapshot */
static int undo_count = 0;
static int just_undid = 0;		/* suppress the snapshot after an undo */

static void capture_undo(UndoState *s)
{
	memcpy(s->flags,  flags,  37);
	memcpy(s->objpos, objpos, 255);
	s->maxcar1 = maxcar1;
	s->alsosee = alsosee;
	s->fileid  = fileid;
}

static void apply_undo(const UndoState *s)
{
	memcpy(flags,  s->flags,  37);
	memcpy(objpos, s->objpos, 255);
	maxcar1 = s->maxcar1;
	alsosee = s->alsosee;
	fileid  = s->fileid;
}

/* Push the current state onto the undo stack, capping its depth. Called once
 * per prompt; skipped on the turn immediately following an undo so that a
 * second "#undo" steps back a further move rather than re-saving. */
static void save_undo(void)
{
	UndoState *s;

	if (just_undid)
	{
		just_undid = 0;
		return;
	}

	s = malloc(sizeof(UndoState));
	if (!s) return;			/* out of memory: silently skip */
	capture_undo(s);
	s->prev = undo_top;
	undo_top = s;
	undo_count++;

	/* Drop the oldest snapshot once we exceed the cap. */
	if (undo_count > MAX_UNDOS)
	{
		UndoState *p = undo_top;
		while (p->prev && p->prev->prev) p = p->prev;
		free(p->prev);
		p->prev = NULL;
		undo_count--;
	}
}

/* Restore the state from before the previous command. Returns 1 if a move was
 * undone (caller should redescribe), 0 if there was nothing to undo. */
static uchar restore_undo(void)
{
	UndoState *current;

	/* undo_top holds the snapshot taken at this prompt (the live state);
	 * its predecessor is the state to roll back to. */
	if (!undo_top || !undo_top->prev)
	{
		glk_printf("\nYou can't undo any further.\n");
		return 0;
	}

	current = undo_top;
	undo_top = current->prev;
	free(current);
	undo_count--;

	apply_undo(undo_top);
	just_undid = 1;
	glk_printf("\nMove undone.\n");
	return 1;
}

/* True if the player's line is the interpreter UNDO meta-command. Leading and
 * trailing whitespace is ignored and the match is case-insensitive. During
 * normal play we require the '#' prefix so it can't clash with any UNDO word
 * in a game's own vocabulary; at the end-of-game prompt (allow_bare) a plain
 * "undo" is accepted too, as the game's parser is not involved there. */
static uchar is_undo_command(const char *line, uchar allow_bare)
{
	while (*line == ' ' || *line == '\t') line++;
	if (*line == '#') line++;
	else if (!allow_bare) return 0;
	if (tolower((uchar)line[0]) != 'u'
	||  tolower((uchar)line[1]) != 'n'
	||  tolower((uchar)line[2]) != 'd'
	||  tolower((uchar)line[3]) != 'o')
		return 0;
	line += 4;
	while (*line == ' ' || *line == '\t'
	|| *line == '\r' || *line == '\n') line++;
	return (*line == '\0');
}

/* Case-insensitively match word (which must be lower-case) at the front of
 * line; return the position just past it on success, or NULL. */
static const char *match_word(const char *line, const char *word)
{
	while (*word)
	{
		if (tolower((uchar)*line) != *word) return NULL;
		line++;
		word++;
	}
	return line;
}

/* The interpreter-level meta-commands. These are recognised during play by
 * meta_command() below and acted on before the turn counter advances and before
 * the game's own parser runs, so they never count as a turn or clash with a
 * game's vocabulary. */
enum { META_NONE, META_UNDO, META_RESTORE, META_SAVE,
       META_RESTART, META_QUIT, META_TRANSCRIPT, META_HELP };

/* Each meta-command word and the action it maps to (several words may share an
 * action, e.g. "transcript"/"script"). This table also drives the #help
 * listing, so the two can never drift apart. */
static const struct { const char *name; int id; } meta_words[] = {
	{ "undo",       META_UNDO },
	{ "restore",    META_RESTORE },
	{ "save",       META_SAVE },
	{ "restart",    META_RESTART },
	{ "quit",       META_QUIT },
	{ "transcript", META_TRANSCRIPT },
	{ "script",     META_TRANSCRIPT },
	{ "help",       META_HELP },
	{ "commands",   META_HELP },
};

/* If line is an interpreter meta-command, return its META_* id, else META_NONE.
 * Same surface syntax as is_undo_command: leading/trailing whitespace ignored,
 * case-insensitive, and the '#' prefix is required during play so a command can
 * never clash with a word in the game's own vocabulary. */
static int meta_command(const char *line)
{
	size_t k;

	while (*line == ' ' || *line == '\t') line++;
	if (*line != '#') return META_NONE;
	line++;

	for (k = 0; k < sizeof meta_words / sizeof meta_words[0]; k++)
	{
		const char *rest = match_word(line, meta_words[k].name);
		if (!rest) continue;
		while (*rest == ' ' || *rest == '\t'
		|| *rest == '\r' || *rest == '\n') rest++;
		if (*rest == '\0') return meta_words[k].id;
	}
	return META_NONE;
}

/* Print the list of available meta-commands (the #help command). */
static void print_meta_help(void)
{
	glk_printf("\nInterpreter commands (type with the '#'):\n");
	glk_printf("  #undo       undo the last move\n");
	glk_printf("  #save       save your position to a file\n");
	glk_printf("  #restore    restore a saved position\n");
	glk_printf("  #restart    start the game again from the beginning\n");
	glk_printf("  #quit       stop playing\n");
	glk_printf("  #transcript start/stop recording a transcript (#script)\n");
	glk_printf("  #help       show this list (#commands)\n");
}

/* Discard the entire undo history. Called when a genuinely new game begins (a
 * first launch, a "play again" after the end, or a game-driven RESTART) so
 * that undo never reaches back into a previous life. The undo-after-death
 * resume path deliberately does *not* call this. */
void undo_reset(void)
{
	while (undo_top)
	{
		UndoState *p = undo_top;
		undo_top = p->prev;
		free(p);
	}
	undo_count = 0;
	just_undid = 0;
}

/* Handle the prompt shown after the game ends (death or victory), once the
 * game's own "would you like to play again?" message has been printed. Reads
 * a line and recognises three answers:
 *   - "#undo" / "undo": roll back to the move before the game ended and resume
 *     the same game (returns END_UNDO; the caller re-enters playgame without
 *     re-initialising). No snapshot was taken for the fatal move, so the top
 *     of the stack already holds the pre-fatal-move state: restore it in place.
 *   - yes: start a new game            (returns END_AGAIN)
 *   - no:  stop playing                (returns END_QUIT)
 * The game's *F / *O diagnostics work here too. */
int end_game_prompt(void)
{
	char line[255];

	while (1)
	{
		myreadline(line, 254);
		while (line[0] == '*')		/* diagnostics */
			myreadline(line, 254);

		if (is_undo_command(line, 1))
		{
			if (!undo_top)
			{
				glk_printf("There is nothing to undo.\n");
				continue;
			}
			apply_undo(undo_top);
			just_undid = 1;		/* keep this snapshot for next turn */
			glk_printf("Move undone.\n");
			return END_UNDO;
		}

		{
			char *p = line;
			while (*p == ' ' || *p == '\t') p++;
			if (*p == 'y' || *p == 'Y') return END_AGAIN;
			if (*p == 'n' || *p == 'N') return END_QUIT;
		}
		/* Anything else: ask again. */
	}
}

void initgame(ushort zxptr)
{
	uchar  n; 
	ushort obase;

	alsosee = 1;	/* Options possibly set by later games */
	fileid  = 255;
	maxcar1 = maxcar;

	obase = postab; /* Object initial positions table */
	for (n = 0; n < 36; n++) flags[n] = 0;
	
	NUMCAR = 0;
	for (n = 0; n < 255; n++)
	{
		objpos[n] = zmem(obase + n);
		if (objpos[n] == 254) NUMCAR++;
	}
}


void savegame(void)
{
	uchar xor = fileid, n;
	FILE *savefp;
	char filename[255];
	
	for (n = 0; n < 37;   n++) xor ^= flags[n];
	for (n = 0; n < 0xDB; n++) xor ^= objpos[n];
	opch32('\n');
	
#if 0
	printf("Save game to file>");
	fflush(stdout);
	fgets(filename,255,stdin);
	l = strlen(filename)-1;
	if ((filename[l] == '\r') || (filename[l] == '\n')) filename[l]=0;
#else
	frefid_t fref;
	fref = glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Write, 0);
	if (fref == NULL)
	    return;
	strncpy(filename, glkunix_fileref_get_filename(fref), sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = '\0';
	glk_fileref_destroy(fref);
#endif

	savefp = fopen(filename,"wb");
	if (!savefp)
	{
		glk_printf("Could not create %s\n",filename);
		return;
	}
	if ((fputc(2,      savefp) == EOF)
	||  (fputc(1,      savefp) == EOF)
	||  (fputc(fileid, savefp) == EOF)
	||  (fwrite(flags, 1,37,  savefp) < 37)
	||  (fwrite(objpos,1,0xDB,savefp) < 0xDB)
	||  (fputc(xor,savefp)     == EOF)
	||  (fclose(savefp)))
	{
		fclose (savefp);
		glk_printf("Write error on %s\n",filename);
		return;
	}
}

/* Format of Quill save file (based on a Spectrum .TAP file):

DW 0102h	;Length / magic no.
DB 0FFh		;Block type ("fileid") - usually 0FFh, but can be changed in
		;later games by PAUSE, subfunction 22.
DS 31		;Flags 0-30
DW xx		;Flags 31-32 = no. of turns
DB xx		;Flag  33    = verb
DB xx		;Flag  34    = noun
DW xx		;Flags 35-36 = location (flag 36 is always 0)
DS 0DBh		;Object locations table, terminated with 0FFh.
DB xsum		;XOR of all bytes except the 0102h.
*/

void loadgame(void)
{
	uchar xor=0;
	ushort n;
	FILE *loadfp;
	char filename[255];
	uchar savefile[0x104];
	
	opch32('\n');
#if 0
	printf("Load game from file>");
	fflush(stdout);
	fgets(filename,255,stdin);
	l = strlen(filename)-1;
	if ((filename[l] == '\r') || (filename[l] == '\n'))
		filename[l] = 0;
#else
	frefid_t fref;
	fref = glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Read, 0);
	if (fref == NULL)
	    return;
	strncpy(filename, glkunix_fileref_get_filename(fref), sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = '\0';
	glk_fileref_destroy(fref);
#endif

	loadfp = fopen(filename,"rb");
	if (!loadfp)
	{
		glk_printf("Could not open %s\n",filename);
		return;
	}
	if (fread(savefile,1,0x104,loadfp) < 0x104)
	{
		fclose(loadfp);
		glk_printf("Read error on %s\n",filename);
		return;
	}
	fclose(loadfp);
	for (n = 2; n < 0x103; n++) xor ^= savefile[n];
	if ((xor !=savefile[0x103])
	||  (savefile[0] != 2)
	||  (savefile[1] != 1)
	||  (savefile[2] != fileid))
	{
		glk_printf("%s is not a suitable save file\nPress RETURN...",filename);
		getch();
		return;
	}	
	memcpy(flags,  savefile + 3,  37);
	memcpy(objpos, savefile + 40, 0xDB);
}


char present(uchar obj)
{
	if ((objpos[obj]==254) || (objpos[obj]==253) || (objpos[obj]==CURLOC))
    		return 1;
  	return 0;
}



void playgame(ushort zxptr)
{
	uchar desc = 1, verb, noun, pn, r, n; 
	char *lbstart;
	ushort connbase;
	char linebuf[255];

	/* The undo stack is *not* cleared here. playgame() is re-entered to
	 * resume the same game after an undo-after-death (see end_game_prompt),
	 * and that path must keep its history. A genuinely new game clears it via
	 * undo_reset() in the caller's play loop. */

	lbstart = linebuf;
	while(1) /* Main loop */
	{
		/* `desc` marks a genuine re-describe (movement, a DESC condact, or
		 * undo/restore): only then do we (re)draw the Illustrator picture and
		 * run the once-per-description flag decrements. draw_location_graphic
		 * redraws the whole bitmap (and replays its reveal), so it must not
		 * fire on turns where the location is unchanged. */
		if (desc && !(flags[0] && (!present(0))))
			draw_location_graphic(CURLOC);

		/* The location description and the objects present are drawn into the
		 * text-grid status window above the main buffer, Scott-style; command
		 * input and responses keep scrolling below. This is re-rendered every
		 * turn so the object list stays current after GET/DROP even when the
		 * game doesn't issue a DESC. status_begin()/status_end() capture and
		 * lay out the text produced in between. */
		status_begin();
		/* 0.7.5: Darkness */
		if (flags[0] && (!present(0)))
		{
			sysmess(0);
			opch32('\n');
		}
		else
		{
			oneitem(loctab,CURLOC);
			opch32('\n');
			listat(CURLOC);  /* List objects present */
			put_ch('\n');
		}
		status_end();

		if (desc)
		{
			desc = 0;

		/* Decrement flags depending on location descriptions */

			if (flags[2]) flags[2]--;
			if (flags[0] && flags[3]) flags[3]--;
			if (flags[0] && (!present(0)) && flags[4]) flags[4]--;
		}

		/* [new in 0.7.0: Flag decrements moved to *after* the
                 * process table; this makes Bugsy work properly */

		/* Process "process" conditions */

		verb = 0xFF; noun = 0xFF;
		
		r = doproc(zword(zxptr + 6), verb, noun);  /* The Process Table */
		if      (r == 2) desc = 1;      /* DESC */
		else if (r == 3) break;  	/* END  */
		else
		{ 
                   /* Decrement flags not depending on location descriptions */

                	if (flags[5]) flags[5]--;
                	if (flags[6]) flags[6]--;
                	if (flags[7]) flags[7]--;
                	if (flags[8]) flags[8]--;
       	        	if (flags[0] && flags[9]) flags[9]--;
	                if (flags[0] && (!present(0)) && flags[10]) flags[10]--;
 
			/* Snapshot the state shown to the player so that a later
			 * "#undo" can roll this move back. */
			save_undo();

			/* Print the prompt */

			pn = (erkyrath_random() & 3);
			sysmess(pn + 2);
			opch32('\n');
			if (dbver == 0) sysmess(28);
			myreadline(linebuf, 254);

			while (linebuf[0] == '*')  /* Diagnostics: */
			{
				if (linebuf[1]=='F')  /* *F: dump flags */
					for (n=0; n<36; n++) glk_printf(" F%3.3d:%3.3d ",n,flags[n]);
				if (linebuf[1]=='O')  /* *O: dump object locations */
					for (n=0; n<255; n++) glk_printf(" O%3.3d:%3.3d ",n,objpos[n]);
				myreadline(linebuf,254);
			}

			/* Interpreter-level meta-commands. None of these count as
			 * a turn (handled before TURNLO++). #undo/#restore/#save
			 * redescribe the location like the game's own verbs; the
			 * destructive #restart/#quit confirm first. A 'continue'
			 * inside the switch re-loops; to leave the play loop we set
			 * `leave` and break the loop after the switch (a plain
			 * `break` would only leave the switch). */
			{
			int leave = 0;
			switch (meta_command(linebuf))
			{
			case META_UNDO:
				if (restore_undo()) desc = 1;
				continue;
			case META_TRANSCRIPT:
				script_toggle();
				continue;
			case META_RESTORE:	/* synonym for the LOAD verb */
				loadgame();
				desc = 1;
				continue;
			case META_SAVE:		/* synonym for the SAVE verb */
				savegame();
				desc = 1;
				continue;
			case META_HELP:
				print_meta_help();
				continue;
			case META_RESTART:
				glk_printf("\nRestart from the beginning? ");
				if (yesno() != 'Y') continue;
				estop = 1;	/* glk_main restarts the game */
				leave = 1;
				break;
			case META_QUIT:
				glk_printf("\nStop playing this game? ");
				if (yesno() != 'Y') continue;
				leave = 1;	/* -> "play again?" prompt */
				break;
			default:
				break;		/* not a meta-command: parse it */
			}
			if (leave) break;	/* exit the while(1) play loop */
			}

    			TURNLO++;
			if (TURNLO == 0) TURNHI++;

			/* Parse the input */
	
			lbstart = linebuf;
			verb    = matchword(&lbstart);
			if (verb != 0xFF)
 			{
 				VERB = verb;
				noun = matchword(&lbstart);
				NOUN = noun;
				if (noun == 0xFF) noun = 0xFE;

	/* v0.7.5: Moved "response" conditions to after the attempt to 
	 *         move the player
	 */

    	/* Attempt to move player */
				r = 0;
/*				if (verb < 20)*/   /* A movement word       */
/*				{  * This test is incorrect; Quill does not *
 *				   * insist on the word number being < 20   */
					connbase = zword(2*CURLOC+conntab);
					while ((!r) && (zmem(connbase) != 0xFF)) if (verb == zmem(connbase))
					{
						CURLOC=zmem(++connbase);
						desc = 1; r = 1;
					}
				   	else connbase += 2; 
	    			/* } */
    	 			if (r == 0)
				{
			        /* Process "response" conditions */
                                	r = doproc(zword(zxptr+4),verb,noun);
                                	if      (r == 2) desc=1;        /* DESC */
                                	else if (r == 3) break;         /* END  */
				}
				/* Print "I can't do that/go that way" */
				if (r == 0)
				{
					if (verb < 20) sysmess(7); 
					else           sysmess(8);
					opch32('\n');
				}	
        		}
        		else sysmess(6); /* Unknown verb */
      		}
	}
}




uchar cplscmp(ushort first, char *snd)
{
	if (((255-(zmem(first++))) & 0x7F) != (snd[0])) return 0;
	if (((255-(zmem(first++))) & 0x7F) != (snd[1])) return 0;
	if (((255-(zmem(first++))) & 0x7F) != (snd[2])) return 0;
	if (((255-(zmem(first++))) & 0x7F) != (snd[3])) return 0;
	return 1;
}




uchar matchword(char **wordbeg)
{
/* Match a word of player input with the vocabulary table */

	ushort matchp=vocab;
	char wordbuf[5];
	int i;

	wordbuf[4]=0;
	while(1)
	{
		for (i=0; i<4; i++)
		{
			wordbuf[i]=(**wordbeg);
			if (islower(wordbuf[i])) wordbuf[i] = toupper(wordbuf[i]);	/* (v0.4.1), was munging numbers */
			if (wordbuf[i]==0)    wordbuf[i]=' ';
			if (wordbuf[i]=='\n') wordbuf[i]=' ';
			if (wordbuf[i]=='\r') wordbuf[i]=' ';
			if (wordbuf[i]==' ')  (*wordbeg)--;
			(*wordbeg)++;
		}
		while ((**wordbeg) 
		&&     (**wordbeg!='\n')
		&&     (**wordbeg!='\r')
		&&     (**wordbeg!=' '))
			(*wordbeg)++; 
		while (zmem(matchp))
		{
			if (cplscmp(matchp,wordbuf))
			{
				return zmem(matchp+4);
			}
			matchp+=5;
		}
		matchp=vocab;
		
		if (((**wordbeg)==0)
		||  ((**wordbeg)=='\r')
		||  ((**wordbeg)=='\n'))
		return 0xFF;

		while ((**wordbeg)==0x20)
		{
			if ((**wordbeg)==0) return 0xFF;
			(*wordbeg)++;
		}
	}
	return 0xFF;
		
}


void listat(uchar locno)
{
/* List items at location n */
	
	uchar any = 0, n;

	for (n=0; n<nobj; n++) if (objpos[n] == locno)
        {
		if (any==0)
		{
			any = 1;
			if (locno < 253)
                  	{
				sysmess(alsosee);
				put_ch('\n');
			}
		}
		put_str("    ");
		oneitem(objtab,n);
		put_ch('\n');
	}
}



uchar ffeq(uchar x,uchar y)  /* Match x with y, 0FFh matches any */
{
	return (uchar)((x == 0xFF) || (y == 0xFF) || (x == y));
}




uchar doproc (ushort table, uchar verb, uchar noun)
{
	ushort ccond;
	uchar done = 0; /* Done returns: 0 for fell off end of table
                                        1 for DONE
                                        2 for DESC
                                        3 for END (end game) */
	uchar t, tverb, tnoun, td1 = 0;

	while ((zmem(table)) && !done)
    	{
		tverb = zmem(table++);
		tnoun = zmem(table++);
		ccond = zword(table++);
		table++;

		if (ffeq(verb,tverb) && ffeq(noun,tnoun))
		{
			t = condtrue(ccond);
      			/* Skip over condition clauses */
      			while (zmem(ccond++) != 0xFF);	
			if (t) 
			{
        			done = runact(ccond,noun);    
        			/* Returned nonzero if should not scan */
				td1 = 1;  /* Something was run */
			}
		} 
	}
	if ((done==0) && (td1)) done=1;
	return(done);
} 



uchar autobj(uchar noun)  /* Find object number for AUTOx actions */
{
	uchar n;

	if (dbver == 0) return 0xFF;
	if (noun > 253) return 0xFF;
	for (n = 0; n < nobj; n++) if (noun == zmem(objmap + n)) return n;
	return 0xFF;
}


char yesno(void)
{
    char n;
    
    while (1)
    {
	glk_put_string("[yn] ");
	n = getch();
	glk_put_char(n);
	glk_put_char('\n');
	if ((n == 'Y') || (n == 'y')) return ('Y');
	if ((n == 'N') || (n == 'n')) return ('N');
    }
    return('N');
}



void sysmess(uchar sysno)
{
	uchar  cch = 0;
	ushort msgadd;

	if (dbver > 0) 
	{	
		oneitem(sysbase,sysno);
		return;
	}
	msgadd = sysbase;
	while (sysno)	/* Skip (sysno) messages */
	{
		while (cch != 0x1F) cch = 0xFF - (zmem(msgadd++));
		sysno--;
		cch = 0xFF - (zmem(msgadd++));
      	}
	msgadd--;
	while (cch != 0x1F)
	{
		cch = 0xFF - (zmem(msgadd++));
		expch (cch,&msgadd);
	}
}



