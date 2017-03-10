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


void oneitem(ushort table, uchar item)
{
    ushort n;
    uchar  cch;
	uchar  term;

	if (arch == ARCH_SPECTRUM) term = 0x1F; else term = 0;
	cch = ~term;
    
    n=zword(table+2*item);
    xpos=0;

    while (1)
        {
        cch=(0xFF-(zmem(n++)));
		if (cch == term) break;
        expch(cch,&n);
        }
}


void clrscr(void)
{
    //glk_put_string("\n\n-- clear --\n\n");
    glk_window_clear(mainwin);
}


void expdict(uchar cch, ushort *n)
{
	ushort d=dict;

 	if (dbver > 0) /* Early games aren't compressed & have no dictionary */
	{
		cch -= 164;
		while (cch)
       		if (zmem(d++) > 0x7f) cch--;

      /* d=address of expansion text */

		while (zmem(d) < 0x80) expch (zmem(d++) & 0x7F,n);
	        expch (zmem(d) & 0x7F,n);
	}
}



void expch_c64(uchar cch, ushort *n)
{
    if (cch >= 'A' && cch <= 'Z') cch = tolower(cch);
    cch &= 0x7F;
    
    if ((cch > 31) && (cch < 127)) opch32(cch);
    else if (cch > 126)  opch32('?');
    else if (cch == 8)   opch32(8);
    else if (cch == 0x0D) opch32('\n');
    else if (cch == 127) { glk_put_string("(c)"); ++xpos; }
}



void expch_cpc(uchar cch, ushort *n)
{
    if      ((cch > 31) && (cch < 127)) opch32(cch);
    /*	else if (cch > 164)  expdict(cch, n); */
    else if (cch == 127) { glk_put_string("(c)"); ++xpos; }
    else if (cch > 126)  opch32('?');
    else if (cch == 9)   return;
    else if (cch == 0x0D) opch32('\n');
    else if (cch == 0x14) opch32('\n');
    else if (cch < 31)    opch32('?');
}



void expch(uchar cch, ushort *n)
{
    ushort tbsp;
    
    if (arch == ARCH_CPC)
    {
	expch_cpc(cch, n);
	return;
    }
    if (arch == ARCH_C64)
    {
	expch_c64(cch, n);
	return;
    }
    
    if      ((cch > 31) && (cch < 127)) opch32(cch);
    else if (cch > 164)  expdict(cch, n);
    else if (cch > 126)  opch32('?');
    else if (cch == 6)   for (opch32(' ');(xpos%16);opch32(' '));
    else if (cch == 8)   opch32(8);
    else if (cch ==0x0D) opch32('\n');
    else if (cch == 0x17)
    {
	tbsp=(255-zmem((*n)++)) & 0x1F;
	++(*n);
	if (xpos > tbsp) opch32('\n');
	for (;tbsp>0;tbsp--) opch32(' ');
    }
    else if ((cch > 0x0F) && (cch<0x16)) (*n)++;

}

#if 0

static const char *p1_moves[] = {" 001  000",
                                 " 001  001",
				 " 000  001",
				 "-001  001",
				 "-001  000",
				 "-001 -001",
				 " 000 -001",
				 " 001 -001" };

/* 2 is a lousy sample size, but all the Spectrum Quill games I've examined 
 * have their graphics tables at 0xFAB8. */

void listgfx(void)
{
	ushort n, m;
	ushort gfx_base  = 0xFAB8;	/* Arbitrary */
	ushort gfx_ptrs  = zword(gfx_base);
	ushort gfx_flags = zword(gfx_base + 2);
	ushort gfx_count = zmem (gfx_base + 6);
	uchar gflag, nargs = 0, value;
	ushort gptr;
	char inv, ovr;
	char *opcode = NULL;
	ushort neg[8];
	char tempop[30];

	fprintf(outfile, "%04x: There are %d graphics record%s.\n",
			gfx_base + 6, gfx_count, (gfx_count == 1) ? "" : "s");
	for (n = 0; n < gfx_count; n++)
	{
		fprintf(outfile, "%04x: Location %d graphics flag: ", 	
				gfx_flags + n, n);
		gflag = zmem (gfx_flags + n);
		gptr  = zword(gfx_ptrs  + 2*n);

		fprintf(outfile, "      ");
		if (gflag & 0x80) fprintf(outfile, "Location graphic.\n");
		else		  fprintf(outfile, "Not location graphic.\n");
		fprintf(outfile, "      Ink=%d Paper=%d Bit6=%d\n", 
			(gflag & 7), (gflag >> 3) & 7, (gflag >> 6) & 1);

		do
		{
			memset(neg, 0, sizeof(neg));
			gflag = zmem(gptr);
			fprintf(outfile, "%04x: ", gptr);
			inv = ' '; ovr = ' ';
			if (gflag & 0x08) ovr = 'o';
			if (gflag & 0x10) inv = 'i';
			value = (gflag >> 3);
			switch(gflag & 7)
			{
				case 0:  nargs = 2;  
                                         opcode = tempop;
                                         sprintf(opcode, "PLOT  %c%c",
							ovr, inv);
					 if (ovr == 'o' && inv == 'i')
						strcpy(opcode, "AMOVE");
					 break;
				case 1:  nargs = 2; 
                                         if (gflag & 0x40) neg[0] = 1;
                                         if (gflag & 0x80) neg[1] = 1;

					 if (ovr == 'o' && inv == 'i')
						opcode = "MOVE";	
					 else
					 {
						opcode = tempop;
           	                                sprintf(opcode, "LINE  %c%c",
                                                        ovr, inv);
					 }
					 break;
				case 2:  switch(gflag & 0x30)
					{
						case 0x00:
                                                	if (gflag & 0x40) neg[0] = 1;
                                                	if (gflag & 0x80) neg[1] = 1;
							nargs = 2; 
							opcode = "FILL";
							break; 
						case 0x10:
							nargs = 4; 
							opcode = "BLOCK";
							break;
						case 0x20:
                                                	if (gflag & 0x40) neg[0] = 1;
                                                	if (gflag & 0x80) neg[1] = 1;
							nargs = 3; 
							opcode = "SHADE";
							break;
						case 0x30:
                                                	if (gflag & 0x40) neg[0] = 1;
                                                	if (gflag & 0x80) neg[1] = 1;
							nargs = 3; 
							opcode = "BSHADE";
							break;
						}
					 break;
				case 3:  nargs = 1; 
					 sprintf(tempop, "GOSUB  sc=%03d",
					 		value & 7);
					 opcode = tempop; 
					break;
				case 4:  nargs = 0; 
					 sprintf(tempop, "RPLOT %c%c %s",
							ovr, inv, p1_moves[(value / 4)]);
					 opcode = tempop;
					 break;
				case 5:  nargs = 0; 
					 opcode = tempop;
					 if (gflag & 0x80) sprintf(tempop,
					  "BRIGHT    %03d ", value & 15);
					 else sprintf(tempop,
					  "PAPER     %03d ", value & 15);
					 break;
                                case 6:  nargs = 0; 
					 opcode = tempop;
                                         if (gflag & 0x80) sprintf(tempop,
                                          "FLASH     %03d ", value & 15);
                                         else sprintf(tempop,
                                          "INK       %03d ", value & 15);
					 break;
				case 7:  nargs = 0; opcode = "END";   break;
			}
			fprintf(outfile, "%-8s ", opcode); 
			for (m = 0; m < nargs; m++)
			{
				fprintf(outfile, "%c%03d ", 
					neg[m] ? '-' : ' ',
					zmem(gptr + 1 + m));
			}
			fprintf(outfile, "\n");

			gptr += (nargs + 1);

		}
		while((gflag & 7) != 7);

	}
}

#endif
