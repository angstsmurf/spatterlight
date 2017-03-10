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
	
uchar runact(ushort ccond,uchar noun)
{
/* Conditions have been met; execute the actions */
/* WARNING: This procedure contains goto statements - take care! */

    uchar cact, cdone=0, cobj, nout,n;
    time_t wait1;

    cact=zmem(ccond);  /* Action byte */

    while ((cact != 0xFF) && !cdone)   
      {
	/* Translate condition numbers for old Spectrum games */
      if ((!dbver) && (cact  > 11)) cact+=9;  /* Translate condition numbers */
      if ((!dbver) && (cact == 11)) cact=17;  /* for old-style games */
      if ((!dbver) && (cact  > 29)) cact++; 

	/* Translate numbers for C64 games */
      if (dbver > 0 && dbver < 10 && cact >= 13) cact += 4;

      switch(cact)
        {
        case 0:   /* INVEN */
          nout=0;
	  opch32('\n');
          sysmess(9);
          opch32('\n');
          for (n=0;n<nobj;n++)
            {
            if (objpos[n]==254)
              {
              oneitem(objtab,n);
              opch32('\n');
              nout++;
              }
            if (objpos[n]==253)
              {
              oneitem(objtab,n);
              sysmess(10);
              opch32('\n');
              nout++;
              }
            }
            if (nout==0) sysmess(11);
            opch32('\n');
          break;
        case 1:  /* DESC */
          cdone=2;
          break;
        case 2:  /* QUIT */
          sysmess(12);
          if (yesno()=='N') cdone=1;
	  break;
	case 3:  /* END */
	  cdone=3;
	  break;
	case 4:  /* DONE */
	  cdone=1;
	  break;
	case 5: /* OK */
	  cdone=1;
	  sysmess(15);
	  opch32('\n');
	  break;
        case 6:  /* ANYKEY */
          sysmess(16);
          fflush(stdout);
          getch();
          opch32('\n');
          break;
        case 7: /* SAVE */
          savegame();
          cdone=2;
          break;
        case 8: /* LOAD */
          loadgame();
          cdone=2;
          break;
        case 9: /* TURNS */
	  sysmess(17);
	  dec32(TURNLO+(256*TURNHI));
	  sysmess(18);
	  if (TURNLO+(256*TURNHI) !=1)
	  {
	  	if(dbver > 0)
	  		sysmess(19);
	  	else	opch32('s');
	  }
	  if (dbver > 0)
	  	sysmess(20);
	  else	opch32('.');
	  opch32('\n');
	  break;
	case 10: /* SCORE */
  	  sysmess(19+ (dbver ? 2 : 0));
  	  dec32(flags[30]);
  	  if(dbver > 0)
  	        sysmess(22);
  	  else	opch32('.');
  	  opch32('\n');
	  break;
	case 11: /* CLS */
	  clrscr();
	  break;
	case 12: /* DROPALL (note: not a true DROP ALL) */
	  for (cobj=0;cobj<nobj;cobj++)
	  	if ((objpos[cobj]==254) || (objpos[cobj]==253))
                	objpos[cobj]=CURLOC; 
          NUMCAR=0;
          opch32('\n'); 
	  break;
	case 13: /* AUTOG  Warning - these four cases contain gotos */
	  cobj=autobj(noun);
/*	  if (cobj==0xFF) cdone=1; */
	  goto get0001;
	case 14: /* AUTOD */
	  cobj=autobj(noun);
/*	  if (cobj==0xFF) cdone=1; */
	  goto drop0001;
	case 15: /* AUTOW */
          if (noun < 200) { sysmess(8); cdone = 1; break; } /* v0.5 */
	  cobj=autobj(noun);
/*	  if (cobj==0xFF) cdone=1; */
	  goto wear0001;
	case 16: /* AUTOR */
          if (noun < 200) { sysmess(8); cdone = 1; break; } /* v0.5 */
	  cobj=autobj(noun);
/*	  if (cobj==0xFF) cdone=1; */
	  goto rem0001;
        case 17: /* PAUSE - and extra functions */
          n=zmem(++ccond);
          if ((dbver > 0) && (flags[28]<0x17))
          {
	        uchar ybrk=1;
	        switch(flags[28])
          	{
/* The following subfunctions of PAUSE cannot be implemented on this style
  of text-based terminal, or deliberately are no-ops. */

  			case 1: case 2: case 3: case 5: case 6:
  			        /* Sound effects... */
  			case 16: case 17: case 18:
  				/* Set the keyboard click */
  			case 4: /* Make the screen flicker */
			case 7: /* Select font 1 */
			case 8: /* Select font 2 */
			case 19: /* Graphics on/off */
			case 20: /* No-op */
			break;
			
/* Subfunctions which can be implemented */

			case 9:
			clrscr();  /* Clear screen impressively */
			break;
			case 10: /* Set the "you can also see" message */
			if (n<nsys) alsosee=n;
			break;
			case 11: /* Set maxcar1 */
			maxcar1=n;
			break;
			case 12: /* Restart the game */
			alsosee=1;
			fileid=255;
			maxcar1=maxcar;
			cdone=3;	/* End game */
			estop=1;	/* Emergency stop */
			break;
			case 13: /* Reboot the Spectrum */
			printf("\n\nGame terminated.\n");
			exit(2);
			case 14:  /* Increase number of portable objects */
			if ((255-n) >= maxcar1) maxcar1+=n;
			else maxcar1=255;
			break;
			case 15:  /* Decrease number of portable objects */
			if (maxcar1<n) maxcar1=0;
			else maxcar1-=n;
			break;
			case 21: /* RAMsave & RAMload */
			if (n==50) /* RAMload */
			{
			  if(!ramsave[0x100]) break;
			  memcpy(flags,ramsave,37);
			  memcpy(objpos,ramsave+37,0xDB);
			}
			else	/* RAMsave */
			{
			  ramsave[0x100]=0xFF;
			  memcpy(ramsave,flags,37);
			  memcpy(ramsave+37,objpos,0xDB);
			}
			break;

			case 22: /* Change identity byte in save file */
			fileid=n;
			break;
			
			default: /* Cases 0, 23, 24 */
			ybrk = 0;
			break;	 /* Pretend it's a normal pause */
	        }
	        flags[28]=0;
	        if(ybrk) break;
	  }
          if (n>0) n--;
          n=n/50;
          n++;
          wait1=time(NULL);
          while (time(NULL)<(wait1+n))
#ifdef YES_GETCH		  /* In case the clock is nonexistent */
          	if(kbhit()) break  /* (as on some CP/M boxes) */
#endif
          	;
          break;
	case 19:	/* INK */
	  if (arch == ARCH_CPC) ++ccond;	/* On CPCs, INK has 2 parameters */
        case 18:
        case 20: 	/* PAPER, and BORDER. Ignore. */       
	  ++ccond;	/* (v0.5 Skip parameter byte!) */ 
          break;    
        case 21: /* GOTO */
          CURLOC=zmem(++ccond);
          break;
        case 22: /* MESSAGE */
          oneitem(msgtab,zmem(++ccond));
          opch32('\n');
          break;
        case 26: /* WEAR */
          cobj=zmem(++ccond);
wear0001: cdone=1;         
          if      (objpos[cobj] != 254)       sysmess(25+ (dbver ? 3 : 0));
          else  {
                objpos[cobj]=253;
                NUMCAR--;
                cdone=0;
                }
          opch32('\n');
          break;
        case 24: /* GET */
          cobj=zmem(++ccond);
get0001:  cdone=1;
          if      (cobj == 0xFF)              sysmess(8);	
          else if (objpos[cobj] == 254)       sysmess(22+ (dbver ? 3 : 0));
          else if (objpos[cobj] == 253)       sysmess(26+ (dbver ? 3 : 0));
          else if (objpos[cobj] != CURLOC)    sysmess(23+ (dbver ? 3 : 0));
          else if (maxcar1==NUMCAR)           sysmess(21+ (dbver ? 3 : 0));
          else {
	        cdone=0;
		objpos[cobj]=254;
                NUMCAR++;
                }
          opch32('\n'); 
          break;
        case 25: /* DROP */
          cobj=zmem(++ccond);
drop0001: cdone=1;
          if      (cobj == 0xFF) sysmess(8);
          else if ((objpos[cobj] != 254) && (objpos[cobj] != 253))
          	sysmess(25+ (dbver ? 3 : 0)); 
          else  {
                objpos[cobj]=CURLOC; 
                NUMCAR--;
                cdone=0;
                }
          opch32('\n'); 
          break;
        case 23: /* REMOVE */
          cobj=zmem(++ccond);
rem0001:  cdone=1;
          if      (objpos[cobj] != 253) sysmess(20+ (dbver ? 3 : 0));
          else if (maxcar1 == NUMCAR)   sysmess(21+ (dbver ? 3 : 0));
          else  {
                objpos[cobj]=254;
                NUMCAR++;
                cdone=0;
                }
          opch32('\n');
          break;
        case 27: /* DESTROY */
          cobj=zmem(++ccond);
          if (objpos[cobj]==254) NUMCAR--;
          objpos[cobj]=252;
          break;
        case 28: /* CREATE */
          cobj=zmem(++ccond);
          objpos[cobj]=CURLOC;
          break;
        case 29: /* SWAP */
          cobj=zmem(++ccond);
          nout=zmem(++ccond);
          n=objpos[cobj];
          objpos[cobj]=objpos[nout];
          objpos[nout]=n;
          break;
        case 30: /* PLACE */
          cobj=zmem(++ccond);
          nout=zmem(++ccond);
          if (objpos[cobj]==254) NUMCAR--;
          objpos[cobj]=nout;
          break;
        case 31: /* SET */
          flags[zmem(++ccond)]=0xFF;
          break;
        case 32: /* CLEAR */
          flags[zmem(++ccond)]=0;
          break;
        case 33: /* PLUS */
          n=zmem(++ccond);
          flags[n]+=zmem(++ccond);
          break;
        case 34: /* MINUS */
          n=zmem(++ccond);
          flags[n]-=zmem(++ccond);
          break;
        case 35: /* LET */
          n=zmem(++ccond);
          flags[n]=zmem(++ccond);
          break;
	case 36: /* BEEP */
          if (!nobeep) fputc(7,stderr);
          n=zmem(++ccond);
	  ccond++;
          if (n>0) n--;
          n=n/100;
          n++;
          wait1=time(NULL);
          while (time(NULL)<(wait1+n))
#ifdef YES_GETCH			/* In case the clock is non- */
          	if(kbhit()) break       /* existent, as it might be on */
#endif					/* some CP/M boxes */
          	;
          fflush(stderr);
          break;
        default:
          fprintf(stderr,"Invalid action %d\n",cact);
        }
      cact=zmem(++ccond);
      }
      return(cdone);
}


uchar condtrue(ushort ccond) 
{
    uchar cond,ctrue=1,cobj;

    cond=zmem(ccond);  /* Condition byte */

    while ((cond != 0xFF) && ctrue)  /* If ctrue = 0, condition invalid */
      {
      switch(cond)
        {
        case 0:   /* AT */
          ctrue=(zmem(++ccond)==CURLOC);
          break; 
        case 1:   /* NOTAT */
          ctrue=(zmem(++ccond)!=CURLOC);
          break; 
        case 2:   /* ATGT */
          ctrue=(zmem(++ccond) <CURLOC); 
          break;                      
        case 3:   /* ATLT */
          ctrue=(zmem(++ccond)> CURLOC);
          break;                      
        case 4:   /* PRESENT */
          cobj=zmem(++ccond);
          ctrue=present(cobj);
          break;                      
        case 5:   /* ABSENT */
          cobj=zmem(++ccond);
          ctrue=(!present(cobj));
          break;                      
        case 6:   /* WORN */
          ctrue=(objpos[zmem(++ccond)]==253);
          break;                      
        case 7:   /* NOTWORN */
          ctrue=(objpos[zmem(++ccond)]!=253); 
          break;                      
        case 8:   /* CARRIED */
          ctrue=(objpos[zmem(++ccond)]==254); 
          break;                      
        case 9:   /* NOTCARR */
          ctrue=(objpos[zmem(++ccond)]!=254);
          break;                      
        case 10:  /* CHANCE */
          ctrue=((rand () % 100) < (zmem(++ccond)));
          break;                      
        case 11:  /* ZERO */
          ctrue=(flags[zmem(++ccond)]==0);
          break;                      
        case 12:  /* NOTZERO */
	  ctrue=(flags[zmem(++ccond)]);
          break;                      
        case 13:  /* EQ */
          ctrue=((flags[zmem(ccond+1)]) == zmem(ccond+2));
	  ccond += 2;
          break;                      
        case 14:  /* GT */
          ctrue=((flags[zmem(ccond+1)]) > zmem(ccond+2));
	  ccond += 2;
          break;                      
        case 15:  /* LT */
          ctrue=((flags[zmem(ccond+1)]) < zmem(ccond+2));
	  ccond += 2;
          break;                      
        default:
          fprintf(stderr,"Unknown condition %d in table\n",zmem(ccond));
          ctrue=0;
        }
      cond=zmem(++ccond);
      } 
    return(ctrue);
}


