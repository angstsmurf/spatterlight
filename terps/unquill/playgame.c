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
	strcpy(filename, glkunix_fileref_get_filename(fref));
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
	strcpy(filename, glkunix_fileref_get_filename(fref));
	glk_fileref_destroy(fref);
#endif
	
	loadfp = fopen(filename,"rb");
	if (!loadfp)
	{
		glk_printf("Could not open %s\n",filename);
		return;
	}
	if ((fread(savefile,1,0x104,loadfp) < 0x104)
	|| (fclose(loadfp)))
	{
		fclose(loadfp);
		glk_printf("Read error on %s\n",filename);
		return;
	}
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

	lbstart = linebuf;
	while(1) /* Main loop */
	{
		if (desc) 
		{
			clrscr();
			/* 0.7.5: Darkness */
			if (flags[0] && (!present(0))) 
			{
				sysmess(0);
				opch32('\n');
			}
			else
			{
			    /* draw graphics for location ... ? */
			    
				oneitem(loctab,CURLOC); 
				opch32('\n');
				listat(CURLOC);  /* List objects present */
				glk_put_char('\n');
			}
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
 
			/* Print the prompt */ 

			pn = (rand() & 3);
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
				glk_put_char('\n');
			} 
		}
		glk_put_string("    ");
		oneitem(objtab,n);
		glk_put_char('\n');
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



