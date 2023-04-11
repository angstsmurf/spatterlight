/***********************************************************************\
*
* Level 9 interpreter
* Version 5.1
* Copyright (c) 1996-2011 Glen Summers and contributors.
* Contributions from David Kinder, Alan Staniforth, Simon Baldwin,
* Dieter Baron and Andreas Scherrer.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The input routine will repond to the following 'hash' commands
*  #save         saves position file directly (bypasses any
*                disk change prompts)
*  #restore      restores position file directly (bypasses any
*                protection code)
*  #quit         terminates current game, RunGame() will return FALSE
*  #cheat        tries to bypass restore protection on v3,4 games
*                (can be slow)
*  #dictionary   lists game dictionary (press a key to interrupt)
*  #picture <n>  show picture <n>
*  #seed <n>     set the random number seed to the value <n>
*  #play         plays back a file as the input to the game
*
\***********************************************************************/

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "level9.h"

extern int gli_determinism;

extern L9UINT16 random_array[];
int random_counter = 0;

/* #define L9DEBUG */
/* #define CODEFOLLOW */
/* #define FULLSCAN */

/* "L901" */
#define L9_ID 0x4c393031

#define IBUFFSIZE 500
#define RAMSAVESLOTS 10
#define GFXSTACKSIZE 100
#define FIRSTLINESIZE 96

/* Typedefs */
typedef struct
{
	L9UINT16 vartable[256];
	L9BYTE listarea[LISTAREASIZE];
} SaveStruct;

/* Enumerations */
enum L9GameTypes { L9_V1, L9_V2, L9_V3, L9_V4 };
enum L9MsgTypes { MSGT_V1, MSGT_V2 };
/*
	Graphics type    Resolution     Scale stack reset
	-------------------------------------------------    
	GFX_V2           160 x 128            yes
	GFX_V3A          160 x 96             yes
	GFX_V3B          160 x 96             no
	GFX_V3C          320 x 96             no
*/
enum L9GfxTypes { GFX_V2, GFX_V3A, GFX_V3B, GFX_V3C };

/* Global Variables */
L9BYTE* startfile=NULL,*pictureaddress=NULL,*picturedata=NULL;
L9BYTE* startdata;
L9UINT32 FileSize,picturesize;

L9BYTE *L9Pointers[12];
L9BYTE *absdatablock,*list2ptr,*list3ptr,*list9startptr,*acodeptr;
L9BYTE *startmd,*endmd,*endwdp5,*wordtable,*dictdata,*defdict;
L9UINT16 dictdatalen;
L9BYTE *startmdV2;

int wordcase;
int unpackcount;
char unpackbuf[8];
L9BYTE* dictptr;
char threechars[34];
int L9GameType;
int L9MsgType;
char LastGame[MAX_PATH];
char FirstLine[FIRSTLINESIZE];
int FirstLinePos=0;
int FirstPicture=-1;

#if defined(AMIGA) && defined(_DCC)
__far SaveStruct ramsavearea[RAMSAVESLOTS];
#else
SaveStruct ramsavearea[RAMSAVESLOTS];
#endif

GameState workspace;

L9UINT16 randomseed;
L9UINT16 constseed=0;
L9BOOL Running;

char ibuff[IBUFFSIZE];
L9BYTE* ibuffptr;
char obuff[34];
FILE* scriptfile=NULL;

L9BOOL Cheating=FALSE;
int CheatWord;
GameState CheatWorkspace;

int reflectflag,scale,gintcolour,option;
int l9textmode=0,drawx=0,drawy=0,screencalled=0,showtitle=1;
L9BYTE *gfxa5=NULL;
Bitmap* bitmap=NULL;
int gfx_mode=GFX_V2;

L9BYTE* GfxA5Stack[GFXSTACKSIZE];
int GfxA5StackPos=0;
int GfxScaleStack[GFXSTACKSIZE];
int GfxScaleStackPos=0;

char lastchar='.';
char lastactualchar=0;
int d5;

L9BYTE* codeptr; /* instruction codes */
L9BYTE code;

L9BYTE* list9ptr;

int unpackd3;

L9BYTE exitreversaltable[20] = { 0x00,0x04,0x06,0x07,0x01,0x08,0x02,0x03,0x05,0x0a,0x09,0x0c,0x0b,0xff,0xff,0x0f,0xff,0xff,0xff,0xff };

L9UINT16 gnostack[128];
L9BYTE gnoscratch[32];
int object,gnosp,numobjectfound,searchdepth,inithisearchpos;


struct L9V1GameInfo
{
	L9BYTE dictVal1, dictVal2;
	int dictStart, L9Ptrs[5], absData, msgStart, msgLen;
};
struct L9V1GameInfo L9V1Games[] =
{
	0x1a,0x24,301, 0x0000,-0x004b, 0x0080,-0x002b, 0x00d0,0x03b0, 0x0f80,0x4857, /* Colossal Adventure */
	0x20,0x3b,283,-0x0583, 0x0000,-0x0508,-0x04e0, 0x0000,0x0800, 0x1000,0x39d1, /* Adventure Quest */
	0x14,0xff,153,-0x00d6, 0x0000, 0x0000, 0x0000, 0x0000,0x0a20, 0x16bf,0x420d, /* Dungeon Adventure */
	0x15,0x5d,252,-0x3e70, 0x0000,-0x3d30,-0x3ca0, 0x0100,0x4120,-0x3b9d,0x3988, /* Lords of Time */
	0x15,0x6c,284,-0x00f0, 0x0000,-0x0050,-0x0050,-0x0050,0x0300, 0x1930,0x3c17, /* Snowball */
};
int L9V1Game = -1;


/* Prototypes */
L9BOOL LoadGame2(char *filename,char *picname);
int getlongcode(void);
L9BOOL GetWordV2(char *buff,int Word);
L9BOOL GetWordV3(char *buff,int Word);
void show_picture(int pic);


#ifdef CODEFOLLOW
#define CODEFOLLOWFILE "c:\\temp\\level9.txt"
FILE *f;
L9UINT16 *cfvar,*cfvar2;
char *codes[]=
{
	"Goto",
	"intgosub",
	"intreturn",
	"printnumber",
	"messagev",
	"messagec",
	"function",
	"input",
	"varcon",
	"varvar",
	"_add",
	"_sub",
	"ilins",
	"ilins",
	"jump",
	"Exit",
	"ifeqvt",
	"ifnevt",
	"ifltvt",
	"ifgtvt",
	"screen",
	"cleartg",
	"picture",
	"getnextobject",
	"ifeqct",
	"ifnect",
	"ifltct",
	"ifgtct",
	"printinput",
	"ilins",
	"ilins",
	"ilins",
};
char *functions[]=
{
	"calldriver",
	"L9Random",
	"save",
	"restore",
	"clearworkspace",
	"clearstack"
};
char *drivercalls[]=
{
	"init",
	"drivercalcchecksum",
	"driveroswrch",
	"driverosrdch",
	"driverinputline",
	"driversavefile",
	"driverloadfile",
	"settext",
	"resettask",
	"returntogem",
	"10 *",
	"loadgamedatafile",
	"randomnumber",
	"13 *",
	"driver14",
	"15 *",
	"driverclg",
	"line",
	"fill",
	"driverchgcol",
	"20 *",
	"21 *",
	"ramsave",
	"ramload",
	"24 *",
	"lensdisplay",
	"26 *",
	"27 *",
	"28 *",
	"29 *",
	"allocspace",
	"31 *",
	"showbitmap",
	"33 *",
	"checkfordisc"
};
#endif

void initdict(L9BYTE *ptr)
{
	dictptr=ptr;
	unpackcount=8;
}

char getdictionarycode(void)
{
	if (unpackcount!=8) return unpackbuf[unpackcount++];
	else
	{
		/* unpackbytes */
		L9BYTE d1=*dictptr++,d2;
		unpackbuf[0]=d1>>3;
		d2=*dictptr++;
		unpackbuf[1]=((d2>>6) + (d1<<2)) & 0x1f;
		d1=*dictptr++;
		unpackbuf[2]=(d2>>1) & 0x1f;
		unpackbuf[3]=((d1>>4) + (d2<<4)) & 0x1f;
		d2=*dictptr++;
		unpackbuf[4]=((d1<<1) + (d2>>7)) & 0x1f;
		d1=*dictptr++;
		unpackbuf[5]=(d2>>2) & 0x1f;
		unpackbuf[6]=((d2<<3) + (d1>>5)) & 0x1f;
		unpackbuf[7]=d1 & 0x1f;
		unpackcount=1;
		return unpackbuf[0];
	}
}

int getdictionary(int d0)
{
	if (d0>=0x1a) return getlongcode();
	else return d0+0x61;
}

int getlongcode(void)
{
	int d0,d1;
	d0=getdictionarycode();
	if (d0==0x10)
	{
		wordcase=1;
		d0=getdictionarycode();
		return getdictionary(d0); /* reentrant? */
	}
	d1=getdictionarycode();
	return 0x80 | ((d0<<5) & 0xe0) | (d1 & 0x1f);
}

void printchar(char c)
{
	if (Cheating) return;

	if (c&128)
		lastchar=(c&=0x7f);
	else if (c!=0x20 && c!=0x0d && (c<'\"' || c>='.'))
	{
		if (lastchar=='!' || lastchar=='?' || lastchar=='.') c=toupper(c);
		lastchar=c;
	}
	/* eat multiple CRs */
	if (c!=0x0d || lastactualchar!=0x0d)
	{
		os_printchar(c);
		if (FirstLinePos < FIRSTLINESIZE-1)
			FirstLine[FirstLinePos++]=tolower(c);
	}
	lastactualchar=c;
}

void printstring(char*buf)
{
	int i;
	for (i=0;i< (int) strlen(buf);i++) printchar(buf[i]);
}

void printdecimald0(int d0)
{
	char temp[12];
	sprintf(temp,"%d",d0);
	printstring(temp);
}

void error(char *fmt,...)
{
	char buf[256];
	int i;
	va_list ap;
	va_start(ap,fmt);
	vsprintf(buf,fmt,ap);
	va_end(ap);
	for (i=0;i< (int) strlen(buf);i++)
		os_printchar(buf[i]);
}

void printautocase(int d0)
{
	if (d0 & 128) printchar((char) d0);
	else
	{
		if (wordcase) printchar((char) toupper(d0));
		else if (d5<6) printchar((char) d0);
		else
		{
			wordcase=0;
			printchar((char) toupper(d0));
		}
	}
}

void displaywordref(L9UINT16 Off)
{
	static int mdtmode=0;

	wordcase=0;
	d5=(Off>>12)&7;
	Off&=0xfff;
	if (Off<0xf80)
	{
	/* dwr01 */
		L9BYTE *a0,*oPtr,*a3;
		int d0,d2,i;

		if (mdtmode==1) printchar(0x20);
		mdtmode=1;

		/* setindex */
		a0=dictdata;
		d2=dictdatalen;

	/* dwr02 */
		oPtr=a0;
		while (d2 && Off >= L9WORD(a0+2))
		{
			a0+=4;
			d2--;
		}
	/* dwr04 */
		if (a0==oPtr)
		{
			a0=defdict;
		}
		else
		{
			a0-=4;
			Off-=L9WORD(a0+2);
			a0=startdata+L9WORD(a0);
		}
	/* dwr04b */
		Off++;
		initdict(a0);
		a3=(L9BYTE*) threechars; /* a3 not set in original, prevent possible spam */

		/* dwr05 */
		while (TRUE)
		{
			d0=getdictionarycode();
			if (d0<0x1c)
			{
				/* dwr06 */
				if (d0>=0x1a) d0=getlongcode();
				else d0+=0x61;
				*a3++=d0;
			}
			else
			{
				d0&=3;
				a3=(L9BYTE*) threechars+d0;
				if (--Off==0) break;
			}
		}
		for (i=0;i<d0;i++) printautocase(threechars[i]);

		/* dwr10 */
		while (TRUE)
		{
			d0=getdictionarycode();
			if (d0>=0x1b) return;
			printautocase(getdictionary(d0));
		}
	}

	else
	{
		if (d5&2) printchar(0x20); /* prespace */
		mdtmode=2;
		Off&=0x7f;
		if (Off!=0x7e) printchar((char)Off);
		if (d5&1) printchar(0x20); /* postspace */
	}
}

int getmdlength(L9BYTE **Ptr)
{
	int tot=0,len;
	do
	{
		len=(*(*Ptr)++ -1) & 0x3f;
		tot+=len;
	} while (len==0x3f);
	return tot;
}

void printmessage(int Msg)
{
	L9BYTE* Msgptr=startmd;
	L9BYTE Data;

	int len;
	L9UINT16 Off;

	while (Msg>0 && Msgptr-endmd<=0)
	{
		Data=*Msgptr;
		if (Data&128)
		{
			Msgptr++;
			Msg-=Data&0x7f;
		}
		else {
			len=getmdlength(&Msgptr);
			Msgptr+=len;
		}
		Msg--;
	}
	if (Msg<0 || *Msgptr & 128) return;

	len=getmdlength(&Msgptr);
	if (len==0) return;

	while (len)
	{
		Data=*Msgptr++;
		len--;
		if (Data&128)
		{
		/* long form (reverse word) */
			Off=(Data<<8) + *Msgptr++;
			len--;
		}
		else
		{
			Off=(wordtable[Data*2]<<8) + wordtable[Data*2+1];
		}
		if (Off==0x8f80) break;
		displaywordref(Off);
	}
}

/* v2 message stuff */

int msglenV2(L9BYTE **ptr)
{
	int i=0;
	L9BYTE a;

	/* catch berzerking code */
	if (*ptr >= startdata+FileSize) return 0;

	while ((a=**ptr)==0)
	{
	 (*ptr)++;
	 
	 if (*ptr >= startdata+FileSize) return 0;

	 i+=255;
	}
	i+=a;
	return i;
}

void printcharV2(char c)
{
	if (c==0x25) c=0xd;
	else if (c==0x5f) c=0x20;
	printautocase(c);
}

void displaywordV2(L9BYTE *ptr,int msg)
{
	int n;
	L9BYTE a;
	if (msg==0) return;
	while (--msg)
	{
		ptr+=msglenV2(&ptr);
	}
	n=msglenV2(&ptr);

	while (--n>0)
	{
		a=*++ptr;
		if (a<3) return;

		if (a>=0x5e) displaywordV2(startmdV2-1,a-0x5d);
		else printcharV2((char)(a+0x1d));
	}
}

int msglenV1(L9BYTE **ptr)
{
	L9BYTE *ptr2=*ptr;
	while (ptr2<startdata+FileSize && *ptr2++!=1);
	return ptr2-*ptr;
}

void displaywordV1(L9BYTE *ptr,int msg)
{
	int n;
	L9BYTE a;
	while (msg--)
	{
		ptr+=msglenV1(&ptr);
	}
	n=msglenV1(&ptr);

	while (--n>0)
	{
		a=*ptr++;
		if (a<3) return;

		if (a>=0x5e) displaywordV1(startmdV2,a-0x5e);
		else printcharV2((char)(a+0x1d));
	}
}

L9BOOL amessageV2(L9BYTE *ptr,int msg,long *w,long *c)
{
	int n;
	L9BYTE a;
	static int depth=0;
	if (msg==0) return FALSE;
	while (--msg)
	{
		ptr+=msglenV2(&ptr);
	}
	if (ptr >= startdata+FileSize) return FALSE;
	n=msglenV2(&ptr);

	while (--n>0)
	{
		a=*++ptr;
		if (a<3) return TRUE;

		if (a>=0x5e)
		{
			if (++depth>10 || !amessageV2(startmdV2-1,a-0x5d,w,c))
			{
				depth--;
				return FALSE;
			}
			depth--;
		}
		else
		{
			char ch=a+0x1d;
			if (ch==0x5f || ch==' ') (*w)++;
			else (*c)++;
		}
	}
	return TRUE;
}

L9BOOL amessageV1(L9BYTE *ptr,int msg,long *w,long *c)
{
	int n;
	L9BYTE a;
	static int depth=0;

	while (msg--)
	{
		ptr+=msglenV1(&ptr);
	}
	if (ptr >= startdata+FileSize) return FALSE;
	n=msglenV1(&ptr);

	while (--n>0)
	{
		a=*ptr++;
		if (a<3) return TRUE;

		if (a>=0x5e)
		{
			if (++depth>10 || !amessageV1(startmdV2,a-0x5e,w,c))
			{
				depth--;
				return FALSE;
			}
			depth--;
		}
		else
		{
			char ch=a+0x1d;
			if (ch==0x5f || ch==' ') (*w)++;
			else (*c)++;
		}
	}
	return TRUE;
}

L9BOOL analyseV2(double *wl)
{
	long words=0,chars=0;
	int i;
	for (i=1;i<256;i++)
	{
		long w=0,c=0;
		if (amessageV2(startmd,i,&w,&c))
		{
			words+=w;
			chars+=c;
		}
		else return FALSE;
	}
	*wl=words ? (double) chars/words : 0.0;
	return TRUE;
}

L9BOOL analyseV1(double *wl)
{
	long words=0,chars=0;
	int i;
	for (i=0;i<256;i++)
	{
		long w=0,c=0;
		if (amessageV1(startmd,i,&w,&c))
		{
			words+=w;
			chars+=c;
		}
		else return FALSE;
	}

	*wl=words ? (double) chars/words : 0.0;
	return TRUE;
}

void printmessageV2(int Msg)
{
	if (L9MsgType==MSGT_V2) displaywordV2(startmd,Msg);
	else displaywordV1(startmd,Msg);
}

L9UINT32 filelength(FILE *f)
{
	L9UINT32 pos,FileSize;

	pos=ftell(f);
	fseek(f,0,SEEK_END);
	FileSize=ftell(f);
	fseek(f,pos,SEEK_SET);
	return FileSize;
}

void L9Allocate(L9BYTE **ptr,L9UINT32 Size)
{
	if (*ptr) free(*ptr);
	*ptr=malloc(Size);
	if (*ptr==NULL) 
	{
		fprintf(stderr,"Unable to allocate memory for the game! Exiting...\n");
		exit(0);
	}
}

void FreeMemory(void)
{
	if (startfile)
	{
		free(startfile);
		startfile=NULL;
	}
	if (pictureaddress)
	{
		free(pictureaddress);
		pictureaddress=NULL;
	}
	if (bitmap)
	{
		free(bitmap);
		bitmap=NULL;
	}
	if (scriptfile)
	{
		fclose(scriptfile);
		scriptfile=NULL;
	}
	picturedata=NULL;
	picturesize=0;
	gfxa5=NULL;
}

L9BOOL load(char *filename)
{
	FILE *f=fopen(filename,"rb");
	if (!f) return FALSE;

	if ((FileSize=filelength(f)) < 256)
	{
		fclose(f);
		error("\rFile is too small to contain a Level 9 game\r");
		return FALSE;
	}

	L9Allocate(&startfile,FileSize);
	if (fread(startfile,1,FileSize,f)!=FileSize)
	{
		fclose(f);
		return FALSE;
	}
 	fclose(f);
	return TRUE;
}

L9UINT16 scanmovewa5d0(L9BYTE* Base,L9UINT32 *Pos)
{
	L9UINT16 ret=L9WORD(Base+*Pos);
	(*Pos)+=2;
	return ret;
}

L9UINT32 scangetaddr(int Code,L9BYTE *Base,L9UINT32 *Pos,L9UINT32 acode,int *Mask)
{
	(*Mask)|=0x20;
	if (Code&0x20)
	{
		/* getaddrshort */
		signed char diff=Base[*Pos];
		(*Pos)++;
		return (*Pos)+diff-1;
	}
	else
	{
		return acode+scanmovewa5d0(Base,Pos);
	}
}

void scangetcon(int Code,L9UINT32 *Pos,int *Mask)
{
	(*Pos)++;
	if (!(Code & 64)) (*Pos)++;
	(*Mask)|=0x40;
}

L9BOOL CheckCallDriverV4(L9BYTE* Base,L9UINT32 Pos)
{
	int i,j;

	/* Look back for an assignment from a variable
	 * to list9[0], which is used to specify the
	 * driver call.
	 */
	for (i = 0; i < 2; i++)
	{
		int x = Pos - ((i+1)*3);
		if ((Base[x] == 0x89) && (Base[x+1] == 0x00))
		{
			/* Get the variable being copied to list9[0] */
			int var = Base[x+2];

			/* Look back for an assignment to the variable. */
			for (j = 0; j < 2; j++)
			{
				int y = x - ((j+1)*3);
				if ((Base[y] == 0x48) && (Base[y+2] == var))
				{
					/* If this a V4 driver call? */
					switch (Base[y+1])
					{
					case 0x0E:
					case 0x20:
					case 0x22:
						return TRUE;
					}
					return FALSE;
				}
			}
		}
	}
	return FALSE;
}

L9BOOL ValidateSequence(L9BYTE* Base,L9BYTE* Image,L9UINT32 iPos,L9UINT32 acode,L9UINT32 *Size,L9UINT32 FileSize,L9UINT32 *Min,L9UINT32 *Max,L9BOOL Rts,L9BOOL *JumpKill, L9BOOL *DriverV4)
{
	L9UINT32 Pos;
	L9BOOL Finished=FALSE,Valid;
	L9UINT32 Strange=0;
	int ScanCodeMask;
	int Code;
	*JumpKill=FALSE;

	if (iPos>=FileSize)
		return FALSE;
	Pos=iPos;
	if (Pos<*Min) *Min=Pos;

	if (Image[Pos]) return TRUE; /* hit valid code */

	do
	{
		Code=Base[Pos];
		Valid=TRUE;
		if (Image[Pos]) break; /* converged to found code */
		Image[Pos++]=2;
		if (Pos>*Max) *Max=Pos;

		ScanCodeMask=0x9f;
		if (Code&0x80)
		{
			ScanCodeMask=0xff;
			if ((Code&0x1f)>0xa)
				Valid=FALSE;
			Pos+=2;
		}
		else switch (Code & 0x1f)
		{
			case 0: /* goto */
			{
				L9UINT32 Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,TRUE/*Rts*/,JumpKill,DriverV4);
				Finished=TRUE;
				break;
			}
			case 1: /* intgosub */
			{
				L9UINT32 Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,TRUE,JumpKill,DriverV4);
				break;
			}
			case 2: /* intreturn */
				Valid=Rts;
				Finished=TRUE;
				break;
			case 3: /* printnumber */
				Pos++;
				break;
			case 4: /* messagev */
				Pos++;
				break;
			case 5: /* messagec */
				scangetcon(Code,&Pos,&ScanCodeMask);
				break;
			case 6: /* function */
				switch (Base[Pos++])
				{
					case 2:/* random */
						Pos++;
						break;
					case 1:/* calldriver */
						if (DriverV4)
						{
							if (CheckCallDriverV4(Base,Pos-2))
								*DriverV4 = TRUE;
						}
						break;
					case 3:/* save */
					case 4:/* restore */
					case 5:/* clearworkspace */
					case 6:/* clear stack */
						break;
					case 250: /* printstr */
						while (Base[Pos++]);
						break;

					default:
#ifdef L9DEBUG
						/* printf("scan: illegal function call: %d",Base[Pos-1]); */
#endif
						Valid=FALSE;
						break;
				}
				break;
			case 7: /* input */
				Pos+=4;
				break;
			case 8: /* varcon */
				scangetcon(Code,&Pos,&ScanCodeMask);
				Pos++;
				break;
			case 9: /* varvar */
				Pos+=2;
				break;
			case 10: /* _add */
				Pos+=2;
				break;
			case 11: /* _sub */
				Pos+=2;
				break;
			case 14: /* jump */
#ifdef L9DEBUG
				/* printf("jmp at codestart: %ld",acode); */
#endif
				*JumpKill=TRUE;
				Finished=TRUE;
				break;
			case 15: /* exit */
				Pos+=4;
				break;
			case 16: /* ifeqvt */
			case 17: /* ifnevt */
			case 18: /* ifltvt */
			case 19: /* ifgtvt */
			{
				L9UINT32 Val;
				Pos+=2;
				Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,Rts,JumpKill,DriverV4);
				break;
			}
			case 20: /* screen */
				if (Base[Pos++]) Pos++;
				break;
			case 21: /* cleartg */
				Pos++;
				break;
			case 22: /* picture */
				Pos++;
				break;
			case 23: /* getnextobject */
				Pos+=6;
				break;
			case 24: /* ifeqct */
			case 25: /* ifnect */
			case 26: /* ifltct */
			case 27: /* ifgtct */
			{
				L9UINT32 Val;
				Pos++;
				scangetcon(Code,&Pos,&ScanCodeMask);
				Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,Rts,JumpKill,DriverV4);
				break;
			}
			case 28: /* printinput */
				break;
			case 12: /* ilins */
			case 13: /* ilins */
			case 29: /* ilins */
			case 30: /* ilins */
			case 31: /* ilins */
#ifdef L9DEBUG 
				/* printf("scan: illegal instruction"); */
#endif
				Valid=FALSE;
				break;
		}
	if (Valid && (Code & ~ScanCodeMask))
		Strange++;
	} while (Valid && !Finished && Pos<FileSize); /* && Strange==0); */
	(*Size)+=Pos-iPos;
	return Valid; /* && Strange==0; */
}

L9BYTE calcchecksum(L9BYTE* ptr,L9UINT32 num)
{
	L9BYTE d1=0;
	while (num--!=0) d1+=*ptr++;
	return d1;
}

/*
L9BOOL Check(L9BYTE* StartFile,L9UINT32 FileSize,L9UINT32 Offset)
{
	L9UINT16 d0,num;
	int i;
	L9BYTE* Image;
	L9UINT32 Size=0,Min,Max;
	L9BOOL ret,JumpKill;

	for (i=0;i<12;i++)
	{
		d0=L9WORD (StartFile+Offset+0x12 + i*2);
		if (d0>=0x8000+LISTAREASIZE) return FALSE;
	}

	num=L9WORD(StartFile+Offset)+1;
	if (Offset+num>FileSize) return FALSE;
	if (calcchecksum(StartFile+Offset,num)) return FALSE; 

	Image=calloc(FileSize,1);

	Min=Max=Offset+d0;
	ret=ValidateSequence(StartFile,Image,Offset+d0,Offset+d0,&Size,FileSize,&Min,&Max,FALSE,&JumpKill,NULL);
	free(Image);
	return ret;
}
*/

long Scan(L9BYTE* StartFile,L9UINT32 FileSize)
{
	L9BYTE *Chk = calloc(FileSize+1, 1);
	L9BYTE *Image=calloc(FileSize,1);
	L9UINT32 i,num,Size,MaxSize=0;
	int j;
	L9UINT16 d0=0,l9,md,ml,dd,dl;
	L9UINT32 Min,Max;
	long Offset=-1;
	L9BOOL JumpKill, DriverV4;

	if ((Chk==NULL)||(Image==NULL))
	{
		fprintf(stderr,"Unable to allocate memory for game scan! Exiting...\n");
		exit(0);
	}

	Chk[0]=0;
	for (i=1;i<=FileSize;i++)
		Chk[i]=Chk[i-1]+StartFile[i-1];

	for (i=0;i<FileSize-33;i++)
	{
		num=L9WORD(StartFile+i)+1;
/*
		Chk[i] = 0 +...+ i-1
		Chk[i+n] = 0 +...+ i+n-1
		Chk[i+n] - Chk[i] = i + ... + i+n
*/
		if (num>0x2000 && i+num<=FileSize && Chk[i+num]==Chk[i])
		{
			md=L9WORD(StartFile+i+0x2);
			ml=L9WORD(StartFile+i+0x4);
			dd=L9WORD(StartFile+i+0xa);
			dl=L9WORD(StartFile+i+0xc);

			if (ml>0 && md>0 && i+md+ml<=FileSize && dd>0 && dl>0 && i+dd+dl*4<=FileSize)
			{
				/* v4 files may have acodeptr in 8000-9000, need to fix */
				for (j=0;j<12;j++)
				{
					d0=L9WORD (StartFile+i+0x12 + j*2);
					if (j!=11 && d0>=0x8000 && d0<0x9000)
					{
						if (d0>=0x8000+LISTAREASIZE) break;
					}
					else if (i+d0>FileSize) break;
				}
				/* list9 ptr must be in listarea, acode ptr in data */
				if (j<12 /*|| (d0>=0x8000 && d0<0x9000)*/) continue;

				l9=L9WORD(StartFile+i+0x12 + 10*2);
				if (l9<0x8000 || l9>=0x8000+LISTAREASIZE) continue;

				Size=0;
				Min=Max=i+d0;
				DriverV4=0;
				if (ValidateSequence(StartFile,Image,i+d0,i+d0,&Size,FileSize,&Min,&Max,FALSE,&JumpKill,&DriverV4))
				{
#ifdef L9DEBUG
					printf("Found valid header at %ld, code size %ld",i,Size);
#endif
					if (Size>MaxSize && Size>100)
					{
						Offset=i;
						MaxSize=Size;
						L9GameType=DriverV4?L9_V4:L9_V3;
					}
				}
			}
		}
	}
	free(Chk);
	free(Image);
	return Offset;
}

long ScanV2(L9BYTE* StartFile,L9UINT32 FileSize)
{
	L9BYTE *Chk=calloc(FileSize+1, 1);
	L9BYTE *Image=calloc(FileSize,1);
	L9UINT32 i,Size,MaxSize=0,num;
	int j;
	L9UINT16 d0=0,l9;
	L9UINT32 Min,Max;
	long Offset=-1;
	L9BOOL JumpKill;

	if ((Chk==NULL)||(Image==NULL))
	{
		fprintf(stderr,"Unable to allocate memory for game scan! Exiting...\n");
		exit(0);
	}

	Chk[0]=0;
	for (i=1;i<=FileSize;i++)
		Chk[i]=Chk[i-1]+StartFile[i-1];

	for (i=0;i<FileSize-28;i++)
	{
		num=L9WORD(StartFile+i+28)+1;
        if ((i + num) <= FileSize && i < (FileSize - 32) && ((Chk[i + num] - Chk[i + 32]) & 0xff) == StartFile[i + 0x1e])
		{
			for (j=0;j<14;j++)
			{
				 d0=L9WORD (StartFile+i+ j*2);
				 if (j!=13 && d0>=0x8000 && d0<0x9000)
				 {
					if (d0>=0x8000+LISTAREASIZE) break;
				 }
				 else if (i+d0>FileSize) break;
			}
			/* list9 ptr must be in listarea, acode ptr in data */
			if (j<14 /*|| (d0>=0x8000 && d0<0x9000)*/) continue;

			l9=L9WORD(StartFile+i+6 + 9*2);
			if (l9<0x8000 || l9>=0x8000+LISTAREASIZE) continue;

			Size=0;
			Min=Max=i+d0;
			if (ValidateSequence(StartFile,Image,i+d0,i+d0,&Size,FileSize,&Min,&Max,FALSE,&JumpKill,NULL))
			{
#ifdef L9DEBUG 
				printf("Found valid V2 header at %ld, code size %ld",i,Size);
#endif
				if (Size>MaxSize && Size>100)
				{
					Offset=i;
					MaxSize=Size;
				}
			}
		}
	}
	free(Chk);
	free(Image);
	return Offset;
}

long ScanV1(L9BYTE* StartFile,L9UINT32 FileSize)
{
	L9BYTE *Image=calloc(FileSize,1);
	L9UINT32 i,Size;
	int Replace;
	L9BYTE* ImagePtr;
	long MaxPos=-1;
	L9UINT32 MaxCount=0;
	L9UINT32 Min,Max,MaxMin,MaxMax;
	L9BOOL JumpKill,MaxJK;

	int dictOff1, dictOff2;
	L9BYTE dictVal1 = 0xff, dictVal2 = 0xff;

	if (Image==NULL)
	{
		fprintf(stderr,"Unable to allocate memory for game scan! Exiting...\n");
		exit(0);
	}

	for (i=0;i<FileSize;i++)
	{
		if ((StartFile[i]==0 && StartFile[i+1]==6) || (StartFile[i]==32 && StartFile[i+1]==4))
		{
			Size=0;
			Min=Max=i;
			Replace=0;
			if (ValidateSequence(StartFile,Image,i,i,&Size,FileSize,&Min,&Max,FALSE,&JumpKill,NULL))
			{
				if (Size>MaxCount && Size>100 && Size<10000)
				{
					MaxCount=Size;
					MaxMin=Min;
					MaxMax=Max;

					MaxPos=i;
					MaxJK=JumpKill;
				}
				Replace=0;
			}
			for (ImagePtr=Image+Min;ImagePtr<=Image+Max;ImagePtr++)
			{
				if (*ImagePtr==2)
					*ImagePtr=Replace;
			}
		}
	}
#ifdef L9DEBUG
	printf("V1scan found code at %ld size %ld",MaxPos,MaxCount);
#endif

    dictOff1 = 0;

	/* V1 dictionary detection from L9Cut by Paul David Doherty */
	for (i=0;i<FileSize-20;i++)
	{
		if (StartFile[i]=='A')
		{
			if (StartFile[i+1]=='T' && StartFile[i+2]=='T' && StartFile[i+3]=='A' && StartFile[i+4]=='C' && StartFile[i+5]==0xcb)
			{
				dictOff1 = i;
				dictVal1 = StartFile[dictOff1+6];
				break;
			}
		}
	}
	for (i=dictOff1;i<FileSize-20;i++)
	{
		if (StartFile[i]=='B')
		{
			if (StartFile[i+1]=='U' && StartFile[i+2]=='N' && StartFile[i+3]=='C' && StartFile[i+4] == 0xc8)
			{
				dictOff2 = i;
				dictVal2 = StartFile[dictOff2+5];
				break;
			}
		}
	}
	L9V1Game = -1;
	if (dictVal1 != 0xff || dictVal2 != 0xff)
	{
		for (i = 0; i < sizeof L9V1Games / sizeof L9V1Games[0]; i++)
		{
			if ((L9V1Games[i].dictVal1 == dictVal1) && (L9V1Games[i].dictVal2 == dictVal2))
			{
				L9V1Game = i;
				dictdata = StartFile+dictOff1-L9V1Games[i].dictStart;
			}
		}
	}

#ifdef L9DEBUG
	if (L9V1Game >= 0)
		printf("V1scan found known dictionary: %d",L9V1Game);
#endif

	free(Image);

	if (MaxPos>0)
	{
		acodeptr=StartFile+MaxPos;
		return 0;
	}
	return -1;
}

#ifdef FULLSCAN
void FullScan(L9BYTE* StartFile,L9UINT32 FileSize)
{
	L9BYTE *Image=calloc(FileSize,1);
	L9UINT32 i,Size;
	int Replace;
	L9BYTE* ImagePtr;
	L9UINT32 MaxPos=0;
	L9UINT32 MaxCount=0;
	L9UINT32 Min,Max,MaxMin,MaxMax;
	int Offset;
	L9BOOL JumpKill,MaxJK;
	for (i=0;i<FileSize;i++)
	{
		Size=0;
		Min=Max=i;
		Replace=0;
		if (ValidateSequence(StartFile,Image,i,i,&Size,FileSize,&Min,&Max,FALSE,&JumpKill,NULL))
		{
			if (Size>MaxCount)
			{
				MaxCount=Size;
				MaxMin=Min;
				MaxMax=Max;

				MaxPos=i;
				MaxJK=JumpKill;
			}
			Replace=0;
		}
		for (ImagePtr=Image+Min;ImagePtr<=Image+Max;ImagePtr++)
		{
			if (*ImagePtr==2)
				*ImagePtr=Replace;
		}
	}
	printf("%ld %ld %ld %ld %s",MaxPos,MaxCount,MaxMin,MaxMax,MaxJK ? "jmp killed" : "");
	/* search for reference to MaxPos */
	Offset=0x12 + 11*2;
	for (i=0;i<FileSize-Offset-1;i++)
	{
		if ((L9WORD(StartFile+i+Offset)) +i==MaxPos)
		{
			printf("possible v3,4 Code reference at : %ld",i);
			/* startdata=StartFile+i; */
		}
	}
	Offset=13*2;
	for (i=0;i<FileSize-Offset-1;i++)
	{
		if ((L9WORD(StartFile+i+Offset)) +i==MaxPos)
			printf("possible v2 Code reference at : %ld",i);
	}
	free(Image);
}
#endif

L9BOOL findsubs(L9BYTE* testptr, L9UINT32 testsize, L9BYTE** picdata, L9UINT32 *picsize)
{
	int i, j, length, count;
	L9BYTE *picptr, *startptr, *tmpptr;

	if (testsize < 16) return FALSE;
	
	/*
		Try to traverse the graphics subroutines.
		
		Each subroutine starts with a header: nn | nl | ll
		nnn : the subroutine number ( 0x000 - 0x7ff ) 
		lll : the subroutine length ( 0x004 - 0x3ff )
		
		The first subroutine usually has the number 0x000.
		Each subroutine ends with 0xff.
		
		findsubs() searches for the header of the second subroutine
		(pattern: 0xff | nn | nl | ll) and then tries to find the
		first and next subroutines by evaluating the length fields
		of the subroutine headers.
	*/
	for (i = 4; i < (int)(testsize - 4); i++)
	{
		picptr = testptr + i;
		if (*(picptr - 1) != 0xff || (*picptr & 0x80) || (*(picptr + 1) & 0x0c) || (*(picptr + 2) < 4))
			continue;

		count = 0;
		startptr = picptr;

		while (TRUE)
		{			
			length = ((*(picptr + 1) & 0x0f) << 8) + *(picptr + 2);
			if (length > 0x3ff || picptr + length + 4 > testptr + testsize)
				break;
			
			picptr += length;
			if (*(picptr - 1) != 0xff)
			{
				picptr -= length;
				break;
			}
			if ((*picptr & 0x80) || (*(picptr + 1) & 0x0c) || (*(picptr + 2) < 4))
				break;
			
			count++;
		}

		if (count > 10)
		{
			/* Search for the start of the first subroutine */
			for (j = 4; j < 0x3ff; j++)
			{
				tmpptr = startptr - j;				
				if (*tmpptr == 0xff || tmpptr < testptr)
					break;
					
				length = ((*(tmpptr + 1) & 0x0f) << 8) + *(tmpptr + 2);
				if (tmpptr + length == startptr)
				{
					startptr = tmpptr;					
					break;
				}
			}
			
			if (*tmpptr != 0xff)
			{ 		
				*picdata = startptr;
				*picsize = picptr - startptr;
				return TRUE;
			}		
		}
	}
	return FALSE;
}

L9BOOL intinitialise(char*filename,char*picname)
{
/* init */
/* driverclg */

	int i;
	int hdoffset;
	long Offset;
	FILE *f;

	if (pictureaddress)
	{
		free(pictureaddress);
		pictureaddress=NULL;
	}
	picturedata=NULL;
	picturesize=0;
	gfxa5=NULL;

	if (!load(filename))
	{
		error("\rUnable to load: %s\r",filename);
		return FALSE;
	}

	/* try to load graphics */
	if (picname)
	{
		f=fopen(picname,"rb");
		if (f)
		{
			picturesize=filelength(f);
			L9Allocate(&pictureaddress,picturesize);
			if (fread(pictureaddress,1,picturesize,f)!=picturesize)
			{
				free(pictureaddress);
				pictureaddress=NULL;
				picturesize=0;
			}
			fclose(f);
		}
	}
	screencalled=0;
	l9textmode=0;

#ifdef FULLSCAN
	FullScan(startfile,FileSize);
#endif

	Offset=Scan(startfile,FileSize);
	if (Offset<0)
	{
		Offset=ScanV2(startfile,FileSize);
		L9GameType=L9_V2;
		if (Offset<0)
		{
			Offset=ScanV1(startfile,FileSize);
			L9GameType=L9_V1;
			if (Offset<0)
			{
				error("\rUnable to locate valid Level 9 game in file: %s\r",filename);
			 	return FALSE;
			}
		}
	}

	startdata=startfile+Offset;
	FileSize-=Offset;

/* setup pointers */
	if (L9GameType==L9_V1)
	{
		if (L9V1Game < 0)
		{
			error("\rWhat appears to be V1 game data was found, but the game was not recognised.\rEither this is an unknown V1 game file or, more likely, it is corrupted.\r");
			return FALSE;
		}
		for (i=0;i<5;i++)
		{
			int off=L9V1Games[L9V1Game].L9Ptrs[i];
			if (off<0)
				L9Pointers[i+2]=acodeptr+off;
			else
				L9Pointers[i+2]=workspace.listarea+off;
		}
		absdatablock=acodeptr-L9V1Games[L9V1Game].absData;
	}
	else
	{
		/* V2,V3,V4 */
		hdoffset=L9GameType==L9_V2 ? 4 : 0x12;
		for (i=0;i<12;i++)
		{
			L9UINT16 d0=L9WORD(startdata+hdoffset+i*2);
			L9Pointers[i]= (i!=11 && d0>=0x8000 && d0<=0x9000) ? workspace.listarea+(d0-0x8000) : startdata+d0;
		}
		absdatablock=L9Pointers[0];
		dictdata=L9Pointers[1];
		list2ptr=L9Pointers[3];
		list3ptr=L9Pointers[4];
		/*list9startptr */
		list9startptr=L9Pointers[10];
		acodeptr=L9Pointers[11];
	}

	switch (L9GameType)
	{
		case L9_V1:
		{
			double a1;
			startmd=acodeptr+L9V1Games[L9V1Game].msgStart;
			startmdV2=startmd+L9V1Games[L9V1Game].msgLen;

			if (analyseV1(&a1) && a1>2 && a1<10)
			{
				L9MsgType=MSGT_V1;
				#ifdef L9DEBUG
				printf("V1 msg table: wordlen=%.2lf",a1);
				#endif
			}
			else
			{
				error("\rUnable to identify V1 message table in file: %s\r",filename);
				return FALSE;
			}
			break;
		}
		case L9_V2:
		{
			double a2,a1;
			startmd=startdata + L9WORD(startdata+0x0);
			startmdV2=startdata + L9WORD(startdata+0x2);

			/* determine message type */
			if (analyseV2(&a2) && a2>2 && a2<10)
			{
				L9MsgType=MSGT_V2;
				#ifdef L9DEBUG
				printf("V2 msg table: wordlen=%.2lf",a2);
				#endif
			}
			else if (analyseV1(&a1) && a1>2 && a1<10)
			{
				L9MsgType=MSGT_V1;
				#ifdef L9DEBUG
				printf("V1 msg table: wordlen=%.2lf",a1);
				#endif
			}
			else
			{
				error("\rUnable to identify V2 message table in file: %s\r",filename);
				return FALSE;
			}
			break;
		}
		case L9_V3:
		case L9_V4:
			startmd=startdata + L9WORD(startdata+0x2);
			endmd=startmd + L9WORD(startdata+0x4);
			defdict=startdata + L9WORD(startdata+6);
			endwdp5=defdict + 5 + L9WORD(startdata+0x8);
			dictdata=startdata + L9WORD(startdata+0x0a);
			dictdatalen=L9WORD(startdata+0x0c);
			wordtable=startdata + L9WORD(startdata+0xe);
			break;
	}

#ifndef NO_SCAN_GRAPHICS
	/* If there was no graphics file, look in the game data */
	if (pictureaddress)
	{
		if (!findsubs(pictureaddress, picturesize, &picturedata, &picturesize))
		{
			picturedata = NULL;
			picturesize = 0;
		}
	}
	else
	{
		if (!findsubs(startdata, FileSize, &picturedata, &picturesize)
			&& !findsubs(startfile, startdata - startfile, &picturedata, &picturesize))
		{
			picturedata = NULL;
			picturesize = 0;
		}
	}
#endif

	memset(FirstLine,0,FIRSTLINESIZE);
	FirstLinePos=0;

	return TRUE;
}

L9BOOL checksumgamedata(void)
{
	return calcchecksum(startdata,L9WORD(startdata)+1)==0;
}

L9UINT16 movewa5d0(void)
{
	L9UINT16 ret=L9WORD(codeptr);
	codeptr+=2;
	return ret;
}

L9UINT16 getcon(void)
{
	if (code & 64)
	{
		/* getconsmall */
		return *codeptr++;
	}
	else return movewa5d0();
}

L9BYTE* getaddr(void)
{
	if (code&0x20)
	{
		/* getaddrshort */
		signed char diff=*codeptr++;
		return codeptr+ diff-1;
	}
	else
	{
		return acodeptr+movewa5d0();
	}
}

L9UINT16 *getvar(void)
{
#ifndef CODEFOLLOW
	return workspace.vartable + *codeptr++;
#else
	cfvar2=cfvar;
	return cfvar=workspace.vartable + *codeptr++;
#endif
}

void Goto(void)
{
	L9BYTE* target = getaddr();
	if (target == codeptr - 2)
		Running = FALSE; /* Endless loop! */
	else
		codeptr = target;
}

void intgosub(void)
{
	L9BYTE* newcodeptr=getaddr();
	if (workspace.stackptr==STACKSIZE)
	{
		error("\rStack overflow error\r");
		Running=FALSE;
		return;
	}
	workspace.stack[workspace.stackptr++]=(L9UINT16) (codeptr-acodeptr);
	codeptr=newcodeptr;
}

void intreturn(void)
{
	if (workspace.stackptr==0)
	{
		error("\rStack underflow error\r");
		Running=FALSE;
		return;
	}
	codeptr=acodeptr+workspace.stack[--workspace.stackptr];
}

void printnumber(void)
{
	printdecimald0(*getvar());
}

void messagec(void)
{
	if (L9GameType<=L9_V2)
		printmessageV2(getcon());
	else
		printmessage(getcon());
}

void messagev(void)
{
	if (L9GameType<=L9_V2)
		printmessageV2(*getvar());
	else
		printmessage(*getvar());
}

void init(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - init");
#endif
}

void randomnumber(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - randomnumber");
#endif
	L9SETWORD(a6,rand());
}

void driverclg(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driverclg");
#endif
}

void _line(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - line");
#endif
}

void fill(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - fill");
#endif
}

void driverchgcol(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driverchgcol");
#endif
}

void drivercalcchecksum(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - calcchecksum");
#endif
}

void driveroswrch(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driveroswrch");
#endif
}

void driverosrdch(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driverosrdch");
#endif

	os_flush();
	if (Cheating) {
		*a6 = '\r';
	} else {
		/* max delay of 1/50 sec */
		*a6=os_readchar(20);
	}
}

void driversavefile(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driversavefile");
#endif
}

void driverloadfile(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driverloadfile");
#endif
}

void settext(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - settext");
#endif
}

void resettask(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - resettask");
#endif
}

void driverinputline(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driverinputline");
#endif
}

void returntogem(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - returntogem");
#endif
}

void lensdisplay(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - lensdisplay");
#endif

	printstring("\rLenslok code is ");
	printchar(*a6);
	printchar(*(a6+1));
	printchar('\r');
}

void allocspace(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - allocspace");
#endif
}

void driver14(L9BYTE *a6)
{
#ifdef L9DEBUG
	printf("driver - call 14");
#endif

	*a6 = 0;
}

void showbitmap(L9BYTE *a6)
{
#ifdef L9DEBUG
	printf("driver - showbitmap");
#endif

	os_show_bitmap(a6[1],a6[3],a6[5]);
}

void checkfordisc(L9BYTE *a6)
{
#ifdef L9DEBUG
	printf("driver - checkfordisc");
#endif

	*a6 = 0;
	list9startptr[2] = 0;
}

void driver(int d0,L9BYTE* a6)
{
	switch (d0)
	{
		case 0: init(a6); break;
		case 0x0c: randomnumber(a6); break;
		case 0x10: driverclg(a6); break;
		case 0x11: _line(a6); break;
		case 0x12: fill(a6); break;
		case 0x13: driverchgcol(a6); break;
		case 0x01: drivercalcchecksum(a6); break;
		case 0x02: driveroswrch(a6); break;
		case 0x03: driverosrdch(a6); break;
		case 0x05: driversavefile(a6); break;
		case 0x06: driverloadfile(a6); break;
		case 0x07: settext(a6); break;
		case 0x08: resettask(a6); break;
		case 0x04: driverinputline(a6); break;
		case 0x09: returntogem(a6); break;
/*
		case 0x16: ramsave(a6); break;
		case 0x17: ramload(a6); break;
*/
		case 0x19: lensdisplay(a6); break;
		case 0x1e: allocspace(a6); break;
/* v4 */
		case 0x0e: driver14(a6); break;
		case 0x20: showbitmap(a6); break;
		case 0x22: checkfordisc(a6); break;
	}
}

void ramsave(int i)
{
#ifdef L9DEBUG
	printf("driver - ramsave %d",i);
#endif

	memmove(ramsavearea+i,workspace.vartable,sizeof(SaveStruct));
}

void ramload(int i)
{
#ifdef L9DEBUG
	printf("driver - ramload %d",i);
#endif

	memmove(workspace.vartable,ramsavearea+i,sizeof(SaveStruct));
}

void calldriver(void)
{
	L9BYTE* a6=list9startptr;
	int d0=*a6++;
#ifdef CODEFOLLOW
	fprintf(f," %s",drivercalls[d0]);
#endif

	if (d0==0x16 || d0==0x17)
	{
		int d1=*a6;
		if (d1>0xfa) *a6=1;
		else if (d1+1>=RAMSAVESLOTS) *a6=0xff;
		else
		{
			*a6=0;
			if (d0==0x16) ramsave(d1+1); else ramload(d1+1);
		}
		*list9startptr=*a6;
	}
	else if (d0==0x0b)
	{
		char NewName[MAX_PATH];
		strcpy(NewName,LastGame);
		if (*a6==0)
		{
			printstring("\rSearching for next sub-game file.\r");
			if (!os_get_game_file(NewName,MAX_PATH))
			{
				printstring("\rFailed to load game.\r");
				return;
			}
		}
		else
		{
			os_set_filenumber(NewName,MAX_PATH,*a6);
		}
		LoadGame2(NewName,NULL);
	}
	else driver(d0,a6);
}

void L9Random(void)
{
#ifdef CODEFOLLOW
	fprintf(f," %d",randomseed);
#endif
    if (gli_determinism) {
        randomseed = random_array[random_counter];
        random_counter++;
        if (random_counter > 99)
            random_counter = 0;
    } else {
        randomseed=(((randomseed<<8) + 0x0a - randomseed) <<2) + randomseed + 1;
    }

	*getvar()=randomseed & 0xff;
#ifdef CODEFOLLOW
	fprintf(f," %d",randomseed);
#endif
}

void save(void)
{
	L9UINT16 checksum;
	int i;
#ifdef L9DEBUG
	printf("function - save");
#endif
/* does a full save, workpace, stack, codeptr, stackptr, game name, checksum */

	workspace.Id=L9_ID;
	workspace.codeptr=codeptr-acodeptr;
	workspace.listsize=LISTAREASIZE;
	workspace.stacksize=STACKSIZE;
	workspace.filenamesize=MAX_PATH;
	workspace.checksum=0;
	strcpy(workspace.filename,LastGame);

	checksum=0;
	for (i=0;i<sizeof(GameState);i++) checksum+=((L9BYTE*) &workspace)[i];
	workspace.checksum=checksum;

	if (os_save_file((L9BYTE*) &workspace,sizeof(workspace))) printstring("\rGame saved.\r");
	else printstring("\rUnable to save game.\r");
}

L9BOOL CheckFile(GameState *gs)
{
	L9UINT16 checksum;
	int i;
	char c = 'Y';

	if (gs->Id!=L9_ID) return FALSE;
	checksum=gs->checksum;
	gs->checksum=0;
	for (i=0;i<sizeof(GameState);i++) checksum-=*((L9BYTE*) gs+i);
	if (checksum) return FALSE;
	if (stricmp(gs->filename,LastGame))
	{
		printstring("\rWarning: game path name does not match, you may be about to load this position file into the wrong story file.\r");
		printstring("Are you sure you want to restore? (Y/N)");
		os_flush();

		c = '\0';
		while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N')) 
			c = os_readchar(20);
	}
	if ((c == 'y') || (c == 'Y'))
		return TRUE;
	return FALSE;
}

void NormalRestore(void)
{
	GameState temp;
	int Bytes;
#ifdef L9DEBUG
	printf("function - restore");
#endif
	if (Cheating)
	{
		/* not really an error */
		Cheating=FALSE;
		error("\rWord is: %s\r",ibuff);
	}

	if (os_load_file((L9BYTE*) &temp,&Bytes,sizeof(GameState)))
	{
		if (Bytes==V1FILESIZE)
		{
			printstring("\rGame restored.\r");
			memset(workspace.listarea,0,LISTAREASIZE);
			memmove(workspace.vartable,&temp,V1FILESIZE);
		}
		else if (CheckFile(&temp))
		{
			printstring("\rGame restored.\r");
			/* only copy in workspace */
			memmove(workspace.vartable,temp.vartable,sizeof(SaveStruct));
		}
		else
		{
			printstring("\rSorry, unrecognised format. Unable to restore\r");
		}
	}
	else printstring("\rUnable to restore game.\r");
}

void restore(void)
{
	int Bytes;
	GameState temp;
	if (os_load_file((L9BYTE*) &temp,&Bytes,sizeof(GameState)))
	{
		if (Bytes==V1FILESIZE)
		{
			printstring("\rGame restored.\r");
			/* only copy in workspace */
			memset(workspace.listarea,0,LISTAREASIZE);
			memmove(workspace.vartable,&temp,V1FILESIZE);
		}
		else if (CheckFile(&temp))
		{
			printstring("\rGame restored.\r");
			/* full restore */
			memmove(&workspace,&temp,sizeof(GameState));
			codeptr=acodeptr+workspace.codeptr;
		}
		else
		{
			printstring("\rSorry, unrecognised format. Unable to restore\r");
		}
	}
	else printstring("\rUnable to restore game.\r");
}

void playback(void)
{
	if (scriptfile) fclose(scriptfile);
	scriptfile = os_open_script_file();
	if (scriptfile)
		printstring("\rPlaying back input from script file.\r");
	else
		printstring("\rUnable to play back script file.\r");
}

void l9_fgets(char* s, int n, FILE* f)
{
	int c = '\0';
	int count = 0;

	while ((c != '\n') && (c != '\r') && (c != EOF) && (count < n-1))
	{
		c = fgetc(f);
		*s++ = c;
		count++;
	}
	*s = '\0';

	if (c == EOF)
	{
		s--;
		*s = '\n';
	}
	else if (c == '\r')
	{
		s--;
		*s = '\n';

		c = fgetc(f);
		if ((c != '\r') && (c != EOF))
			fseek(f,-1,SEEK_CUR);
	}
}

L9BOOL scriptinput(char* ibuff, int size)
{
	while (scriptfile != NULL)
	{
		if (feof(scriptfile))
		{
			fclose(scriptfile);
			scriptfile=NULL;
		}
		else
		{
			char* p = ibuff;
			*p = '\0';
			l9_fgets(ibuff,size,scriptfile);
			while (*p != '\0')
			{
				switch (*p)
				{
				case '\n':
				case '\r':
				case '[':
				case ';':
					*p = '\0';
					break;
				case '#':
					if ((p==ibuff) && (strnicmp(p,"#seed ",6)==0))
						p++;
					else
						*p = '\0';
					break;
				default:
					p++;
					break;
				}
			}
			if (*ibuff != '\0')
			{
				printstring(ibuff);
				lastchar=lastactualchar='.';
				return TRUE;
			}
		}
	}
	return FALSE;
}

void clearworkspace(void)
{
	memset(workspace.vartable,0,sizeof(workspace.vartable));
}

void ilins(int d0)
{
	error("\rIllegal instruction: %d\r",d0);
	Running=FALSE;
}

void function(void)
{
	int d0=*codeptr++;
#ifdef CODEFOLLOW
	fprintf(f," %s",d0==250 ? "printstr" : functions[d0-1]);
#endif

	switch (d0)
	{
		case 1:
			if (L9GameType==L9_V1)
				StopGame();
			else
				calldriver();
			break;
		case 2: L9Random(); break;
		case 3: save(); break;
		case 4: NormalRestore(); break;
		case 5: clearworkspace(); break;
		case 6: workspace.stackptr=0; break;
		case 250:
			printstring((char*) codeptr);
			while (*codeptr++);
			break;

		default: ilins(d0);
	}
}

void findmsgequiv(int d7)
{
	int d4=-1,d0;
	L9BYTE* a2=startmd;

	do
	{
		d4++;
		if (a2>endmd) return;
		d0=*a2;
		if (d0&0x80)
		{
			a2++;
			d4+=d0&0x7f;
		}
		else if (d0&0x40)
		{
			int d6=getmdlength(&a2);
			do
			{
				int d1;
				if (d6==0) break;

				d1=*a2++;
				d6--;
				if (d1 & 0x80)
				{
					if (d1<0x90)
					{
						a2++;
						d6--;
					}
					else
					{
						d0=(d1<<8) + *a2++;
						d6--;
						if (d7==(d0 & 0xfff))
						{
							d0=((d0<<1)&0xe000) | d4;
							list9ptr[1]=d0;
							list9ptr[0]=d0>>8;
							list9ptr+=2;
							if (list9ptr>=list9startptr+0x20) return;
						}
					}
				}
			} while (TRUE);
		}
		else {
			int len=getmdlength(&a2);
			a2+=len;
		}
	} while (TRUE);
}

L9BOOL unpackword(void)
{
	L9BYTE *a3;

	if (unpackd3==0x1b) return TRUE;

	a3=(L9BYTE*) threechars + (unpackd3&3);

/*uw01 */
	while (TRUE)
	{
		L9BYTE d0=getdictionarycode();
		if (dictptr>=endwdp5) return TRUE;
		if (d0>=0x1b)
		{
			*a3=0;
			unpackd3=d0;
			return FALSE;
		}
		*a3++=getdictionary(d0);
	}
}

L9BOOL initunpack(L9BYTE* ptr)
{
	initdict(ptr);
	unpackd3=0x1c;
	return unpackword();
}

int partword(char c)
{
	c=tolower(c);

	if (c==0x27 || c==0x2d) return 0;
	if (c<0x30) return 1;
	if (c<0x3a) return 0;
	if (c<0x61) return 1;
	if (c<0x7b) return 0;
	return 1;
}

L9UINT32 readdecimal(char *buff)
{
	return atol(buff);
}

void checknumber(void)
{
	if (*obuff>=0x30 && *obuff<0x3a)
	{
		if (L9GameType==L9_V4)
		{
			*list9ptr=1;
			L9SETWORD(list9ptr+1,readdecimal(obuff));
			L9SETWORD(list9ptr+3,0);
		}
		else
		{
			L9SETDWORD(list9ptr,readdecimal(obuff));
			L9SETWORD(list9ptr+4,0);
		}
	}
	else
	{
		L9SETWORD(list9ptr,0x8000);
		L9SETWORD(list9ptr+2,0);
	}
}

void NextCheat(void)
{
	/* restore game status */
	memmove(&workspace,&CheatWorkspace,sizeof(GameState));
	codeptr=acodeptr+workspace.codeptr;

	if (!((L9GameType<=L9_V2) ? GetWordV2(ibuff,CheatWord++) : GetWordV3(ibuff,CheatWord++)))
	{
		Cheating=FALSE;
		printstring("\rCheat failed.\r");
		*ibuff=0;
	}
}

void StartCheat(void)
{
	Cheating=TRUE;
	CheatWord=0;

	/* save current game status */
	memmove(&CheatWorkspace,&workspace,sizeof(GameState));
	CheatWorkspace.codeptr=codeptr-acodeptr;

	NextCheat();
}

/* v3,4 input routine */

L9BOOL GetWordV3(char *buff,int Word)
{
	int i;
	int subdict=0;
	/* 26*4-1=103 */

	initunpack(startdata+L9WORD(dictdata));
	unpackword();

	while (Word--)
	{
		if (unpackword())
		{
			if (++subdict==dictdatalen) return FALSE;
			initunpack(startdata+L9WORD(dictdata+(subdict<<2)));
			Word++; /* force unpack again */
		}
	}
	strcpy(buff,threechars);
	for (i=0;i<(int)strlen(buff);i++) buff[i]&=0x7f;
	return TRUE;
}

L9BOOL CheckHash(void)
{
	if (stricmp(ibuff,"#cheat")==0) StartCheat();
	else if (stricmp(ibuff,"#save")==0)
	{
		save();
		return TRUE;
	}
	else if (stricmp(ibuff,"#restore")==0)
	{
		restore();
		return TRUE;
	}
	else if (stricmp(ibuff,"#quit")==0)
	{
		StopGame();
		printstring("\rGame Terminated\r");
		return TRUE;
	}
	else if (stricmp(ibuff,"#dictionary")==0)
	{
		CheatWord=0;
		printstring("\r");
		while ((L9GameType<=L9_V2) ? GetWordV2(ibuff,CheatWord++) : GetWordV3(ibuff,CheatWord++))
		{
			error("%s ",ibuff);
			if (os_stoplist() || !Running) break;
		}
		printstring("\r");
		return TRUE;
	}
	else if (strnicmp(ibuff,"#picture ",9)==0)
	{
		int pic = 0;
		if (sscanf(ibuff+9,"%d",&pic) == 1)
		{
			if (L9GameType==L9_V4)
				os_show_bitmap(pic,0,0);
			else
				show_picture(pic);
		}

		lastactualchar = 0;
		printchar('\r');
		return TRUE;
	}
	else if (strnicmp(ibuff,"#seed ",6)==0)
	{
		int seed = 0;
		if (sscanf(ibuff+6,"%d",&seed) == 1)
			randomseed = constseed = seed;
		lastactualchar = 0;
		printchar('\r');
		return TRUE;
	}
	else if (stricmp(ibuff,"#play")==0)
	{
		playback();
		return TRUE;
	}
	return FALSE;
}

L9BOOL IsInputChar(char c)
{
	if (c=='-' || c=='\'')
		return TRUE;
	if ((L9GameType>=L9_V3) && (c=='.' || c==','))
		return TRUE;
	return isalnum(c);
}

L9BOOL corruptinginput(void)
{
	L9BYTE *a0,*a2,*a6;
	int d0,d1,d2,keywordnumber,abrevword;
	char *iptr;

	list9ptr=list9startptr;

	if (ibuffptr==NULL)
	{
		if (Cheating) NextCheat();
		else
		{
			/* flush */
			os_flush();
			lastchar=lastactualchar='.';
			/* get input */
			if (!scriptinput(ibuff,IBUFFSIZE))
			{
				if (!os_input(ibuff,IBUFFSIZE))
					return FALSE; /* fall through */
			}
			if (CheckHash())
				return FALSE;

			/* check for invalid chars */
			for (iptr=ibuff;*iptr!=0;iptr++)
			{
				if (!IsInputChar(*iptr))
					*iptr=' ';
			}

			/* force CR but prevent others */
			os_printchar(lastactualchar='\r');
		}
		ibuffptr=(L9BYTE*) ibuff;
	}

	a2=(L9BYTE*) obuff;
	a6=ibuffptr;

/*ip05 */
	while (TRUE)
	{
		d0=*a6++;
		if (d0==0)
		{
			ibuffptr=NULL;
			L9SETWORD(list9ptr,0);
			return TRUE;
		}
		if (partword((char)d0)==0) break;
		if (d0!=0x20)
		{
			ibuffptr=a6;
			L9SETWORD(list9ptr,0);
			L9SETWORD(list9ptr+2,0);
			list9ptr[1]=d0;
			*a2=0x20;
			keywordnumber=-1;
			return TRUE;
		}
	}

	a6--;
/*ip06loop */
	do
	{
		d0=*a6++;
		if (partword((char)d0)==1) break;
		d0=tolower(d0);
		*a2++=d0;
	} while (a2<(L9BYTE*) obuff+0x1f);
/*ip06a */
	*a2=0x20;
	a6--;
	ibuffptr=a6;
	abrevword=-1;
	keywordnumber=-1;
	list9ptr=list9startptr;
/* setindex */
	a0=dictdata;
	d2=dictdatalen;
	d0=*obuff-0x61;
	if (d0<0)
	{
		a6=defdict;
		d1=0;
	}
	else
	{
	/*ip10 */
		d1=0x67;
		if (d0<0x1a)
		{
			d1=d0<<2;
			d0=obuff[1];
			if (d0!=0x20) d1+=((d0-0x61)>>3)&3;
		}
	/*ip13 */
		if (d1>=d2)
		{
			checknumber();
			return TRUE;
		}
		a0+=d1<<2;
		a6=startdata+L9WORD(a0);
		d1=L9WORD(a0+2);
	}
/*ip13gotwordnumber */

	initunpack(a6);
/*ip14 */
	d1--;
	do
	{
		d1++;
		if (unpackword())
		{/* ip21b */
			if (abrevword==-1) break; /* goto ip22 */
			else d0=abrevword; /* goto ip18b */
		}
		else
		{
			L9BYTE* a1=(L9BYTE*) threechars;
			int d6=-1;

			a0=(L9BYTE*) obuff;
		/*ip15 */
			do
			{
				d6++;
				d0=tolower(*a1++ & 0x7f);
				d2=*a0++;
			} while (d0==d2);

			if (d2!=0x20)
			{/* ip17 */
				if (abrevword==-1) continue;
				else d0=-1;
			}
			else if (d0==0) d0=d1;
			else if (abrevword!=-1) break;
			else if (d6>=4) d0=d1;
			else
			{
				abrevword=d1;
				continue;
			}
		}
		/*ip18b */
		findmsgequiv(d1);

		abrevword=-1;
		if (list9ptr!=list9startptr)
		{
			L9SETWORD(list9ptr,0);
			return TRUE;
		}
	} while (TRUE);
/* ip22 */
	checknumber();
	return TRUE;
}

/* version 2 stuff hacked from bbc v2 files */

L9BOOL IsDictionaryChar(char c)
{
	switch (c)
	{
	case '?': case '-': case '\'': case '/': return TRUE;
	case '!': case '.': case ',': return TRUE;
	}
	return isupper(c) || isdigit(c);
}

L9BOOL GetWordV2(char *buff,int Word)
{
	L9BYTE *ptr=dictdata,x;

	while (Word--)
	{
		do
		{
			x=*ptr++;
		} while (x>0 && x<0x7f);
		if (x==0) return FALSE; /* no more words */
		ptr++;
	}
	do
	{
		x=*ptr++;
		if (!IsDictionaryChar(x&0x7f)) return FALSE;
		*buff++=x&0x7f;
	} while (x>0 && x<0x7f);
	*buff=0;
	return TRUE;
}

L9BOOL inputV2(int *wordcount)
{
	L9BYTE a,x;
	L9BYTE *ibuffptr,*obuffptr,*ptr,*list0ptr;
	char *iptr;

	if (Cheating) NextCheat();
	else
	{
		os_flush();
		lastchar=lastactualchar='.';
		/* get input */
		if (!scriptinput(ibuff,IBUFFSIZE))
		{
			if (!os_input(ibuff,IBUFFSIZE))
				return FALSE; /* fall through */
		}
		if (CheckHash())
			return FALSE;

		/* check for invalid chars */
		for (iptr=ibuff;*iptr!=0;iptr++)
		{
			if (!IsInputChar(*iptr))
				*iptr=' ';
		}

		/* force CR but prevent others */
		os_printchar(lastactualchar='\r');
	}
	/* add space onto end */
	ibuffptr=(L9BYTE*) strchr(ibuff,0);
	*ibuffptr++=32;
	*ibuffptr=0;

	*wordcount=0;
	ibuffptr=(L9BYTE*) ibuff;
	obuffptr=(L9BYTE*) obuff;
	/* ibuffptr=76,77 */
	/* obuffptr=84,85 */
	/* list0ptr=7c,7d */
	list0ptr=dictdata;

	while (*ibuffptr==32) ++ibuffptr;

	ptr=ibuffptr;
	do
	{
		while (*ptr==32) ++ptr;
		if (*ptr==0) break;
		(*wordcount)++;
		do
		{
			a=*++ptr;
		} while (a!=32 && a!=0);
	} while (*ptr>0);

	while (TRUE)
	{
		ptr=ibuffptr; /* 7a,7b */
		while (*ibuffptr==32) ++ibuffptr;

		while (TRUE)
		{
			a=*ibuffptr;
			x=*list0ptr++;

			if (a==32) break;
			if (a==0)
			{
				*obuffptr++=0;
				return TRUE;
			}

			++ibuffptr;
			if (!IsDictionaryChar(x&0x7f)) x = 0;
			if (tolower(x&0x7f) != tolower(a))
			{
				while (x>0 && x<0x7f) x=*list0ptr++;
				if (x==0)
				{
					do
					{
						a=*ibuffptr++;
						if (a==0)
						{
							*obuffptr=0;
							return TRUE;
						}
					} while (a!=32);
					while (*ibuffptr==32) ++ibuffptr;
					list0ptr=dictdata;
					ptr=ibuffptr;
				}
				else
				{
					list0ptr++;
					ibuffptr=ptr;
				}
			}
			else if (x>=0x7f) break;
		}

		a=*ibuffptr;
		if (a!=32)
		{
			ibuffptr=ptr;
			list0ptr+=2;
			continue;
		}
		--list0ptr;
		while (*list0ptr++<0x7e);
		*obuffptr++=*list0ptr;
		while (*ibuffptr==32) ++ibuffptr;
		list0ptr=dictdata;
	}
}

void input(void)
{
	if (L9GameType == L9_V3 && FirstPicture >= 0)
	{
		show_picture(FirstPicture);
		FirstPicture = -1;
	}

	/* if corruptinginput() returns false then, input will be called again
	   next time around instructionloop, this is used when save() and restore()
	   are called out of line */

	codeptr--;
	if (L9GameType<=L9_V2)
	{
		int wordcount;
		if (inputV2(&wordcount))
		{
			L9BYTE *obuffptr=(L9BYTE*) obuff;
			codeptr++;
			*getvar()=*obuffptr++;
			*getvar()=*obuffptr++;
			*getvar()=*obuffptr;
			*getvar()=wordcount;
		}
	}
	else
		if (corruptinginput()) codeptr+=5;
}

void varcon(void)
{
	L9UINT16 d6=getcon();
	*getvar()=d6;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]=%d)",cfvar-workspace.vartable,*cfvar);
#endif
}

void varvar(void)
{
	L9UINT16 d6=*getvar();
	*getvar()=d6;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]=Var[%d] (=%d)",cfvar-workspace.vartable,cfvar2-workspace.vartable,d6);
#endif
}

void _add(void)
{
	L9UINT16 d0=*getvar();
	*getvar()+=d0;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]+=Var[%d] (+=%d)",cfvar-workspace.vartable,cfvar2-workspace.vartable,d0);
#endif
}

void _sub(void)
{
	L9UINT16 d0=*getvar();
	*getvar()-=d0;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]-=Var[%d] (-=%d)",cfvar-workspace.vartable,cfvar2-workspace.vartable,d0);
#endif
}

void jump(void)
{
	L9UINT16 d0=L9WORD(codeptr);
	L9BYTE* a0;
	codeptr+=2;

	a0=acodeptr+((d0+((*getvar())<<1))&0xffff);
	codeptr=acodeptr+L9WORD(a0);
}

/* bug */
void exit1(L9BYTE *d4,L9BYTE *d5,L9BYTE d6,L9BYTE d7)
{
	L9BYTE* a0=absdatablock;
	L9BYTE d1=d7,d0;
	if (--d1)
	{
		do
		{
			d0=*a0;
			if (L9GameType==L9_V4)
			{
				if ((d0==0) && (*(a0+1)==0))
					goto notfn4;
			}
			a0+=2;
		}
		while ((d0&0x80)==0 || --d1);
	}

	do
	{
		*d4=*a0++;
		if (((*d4)&0xf)==d6)
		{
			*d5=*a0;
			return;
		}
		a0++;
	}
	while (((*d4)&0x80)==0);

	/* notfn4 */
notfn4:
	d6=exitreversaltable[d6];
	a0=absdatablock;
	*d5=1;

	do
	{
		*d4=*a0++;
		if (((*d4)&0x10)==0 || ((*d4)&0xf)!=d6) a0++;
		else if (*a0++==d7) return;
		/* exit6noinc */
		if ((*d4)&0x80) (*d5)++;
	} while (*d4);
	*d5=0;
}

void Exit(void)
{
	L9BYTE d4,d5;
	L9BYTE d7=(L9BYTE) *getvar();
	L9BYTE d6=(L9BYTE) *getvar();
#ifdef CODEFOLLOW
	fprintf(f," d7=%d d6=%d",d7,d6);
#endif
	exit1(&d4,&d5,d6,d7);

	*getvar()=(d4&0x70)>>4;
	*getvar()=d5;
#ifdef CODEFOLLOW
	fprintf(f," Var[%d]=%d(d4=%d) Var[%d]=%d",
		cfvar2-workspace.vartable,(d4&0x70)>>4,d4,cfvar-workspace.vartable,d5);
#endif
}

void ifeqvt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
	L9BYTE* a0=getaddr();
	if (d0==d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]=Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0==d1 ? "Yes":"No");
#endif
}

void ifnevt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
	L9BYTE* a0=getaddr();
	if (d0!=d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]!=Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0!=d1 ? "Yes":"No");
#endif
}

void ifltvt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
	L9BYTE* a0=getaddr();
	if (d0<d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]<Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0<d1 ? "Yes":"No");
#endif
}

void ifgtvt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
	L9BYTE* a0=getaddr();
	if (d0>d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]>Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0>d1 ? "Yes":"No");
#endif
}

int scalex(int x)
{
	return (gfx_mode != GFX_V3C) ? (x>>6) : (x>>5);
}

int scaley(int y)
{
	return (gfx_mode == GFX_V2) ? 127 - (y>>7) : 95 - (((y>>5)+(y>>6))>>3);
}

void detect_gfx_mode(void)
{
	if (L9GameType == L9_V3)
	{
		/* These V3 games use graphics logic similar to the V2 games */
		if (strstr(FirstLine,"price of magik") != 0)
			gfx_mode = GFX_V3A;
		else if (strstr(FirstLine,"the archers") != 0)
			gfx_mode = GFX_V3A;
		else if (strstr(FirstLine,"secret diary of adrian mole") != 0)
			gfx_mode = GFX_V3A;
		else if ((strstr(FirstLine,"worm in paradise") != 0)
			&& (strstr(FirstLine,"silicon dreams") == 0))
			gfx_mode = GFX_V3A;
		else if (strstr(FirstLine,"growing pains of adrian mole") != 0)
			gfx_mode = GFX_V3B;
		else if (strstr(FirstLine,"jewels of darkness") != 0 && picturesize < 11000)
			gfx_mode = GFX_V3B;
		else if (strstr(FirstLine,"silicon dreams") != 0)
		{
			if (picturesize > 11000
				|| (startdata[0] == 0x14 && startdata[1] == 0x7d)  /* Return to Eden /SD (PC) */
				|| (startdata[0] == 0xd7 && startdata[1] == 0x7c)) /* Worm in Paradise /SD (PC) */
				gfx_mode = GFX_V3C;
			else
				gfx_mode = GFX_V3B;
		} 
		else
			gfx_mode = GFX_V3C;
	}
	else
		gfx_mode = GFX_V2;
}

void _screen(void)
{
	int mode = 0;

	if (L9GameType == L9_V3 && strlen(FirstLine) == 0)
	{
		if (*codeptr++)
			codeptr++;
		return;
	}

	detect_gfx_mode();
	l9textmode = *codeptr++;
	if (l9textmode)
	{
		if (L9GameType==L9_V4)
			mode = 2;
		else if (picturedata)
			mode = 1;
	}
	os_graphics(mode);

	screencalled = 1;

#ifdef L9DEBUG
	printf("screen %s",l9textmode ? "graphics" : "text");
#endif

	if (l9textmode)
	{
		codeptr++;
/* clearg */
/* gintclearg */
		os_cleargraphics();

		/* title pic */
		if (showtitle==1 && mode==2)
		{
			showtitle = 0;
			os_show_bitmap(0,0,0);
		}
	}
/* screent */
}

void cleartg(void)
{
	int d0 = *codeptr++;
#ifdef L9DEBUG
	printf("cleartg %s",d0 ? "graphics" : "text");
#endif

	if (d0)
	{
/* clearg */
		if (l9textmode)
/* gintclearg */
			os_cleargraphics();
	}
/* cleart */
/* oswrch(0x0c) */
}

L9BOOL validgfxptr(L9BYTE* a5)
{
	return ((a5 >= picturedata) && (a5 < picturedata+picturesize));
}

L9BOOL findsub(int d0,L9BYTE** a5)
{
	int d1,d2,d3,d4;

	d1=d0 << 4;
	d2=d1 >> 8;
	*a5=picturedata;
/* findsubloop */
	while (TRUE)
	{
		d3=*(*a5)++;
		if (!validgfxptr(*a5))
			return FALSE;
		if (d3&0x80) 
			return FALSE;
		if (d2==d3)
		{
			if ((d1&0xff)==(*(*a5) & 0xf0))
			{
				(*a5)+=2;
				return TRUE;
			}
		}

		d3=*(*a5)++ & 0x0f;
		if (!validgfxptr(*a5))
			return FALSE;

		d4=**a5;
		if ((d3|d4)==0)
			return FALSE;

		(*a5)+=(d3<<8) + d4 - 2;
		if (!validgfxptr(*a5))
			return FALSE;
	}
}

void gosubd0(int d0, L9BYTE** a5)
{
	if (GfxA5StackPos < GFXSTACKSIZE)
	{
		GfxA5Stack[GfxA5StackPos] = *a5;
		GfxA5StackPos++;
		GfxScaleStack[GfxScaleStackPos] = scale;
		GfxScaleStackPos++;

		if (findsub(d0,a5) == FALSE)
		{
			GfxA5StackPos--;
			*a5 = GfxA5Stack[GfxA5StackPos];
			GfxScaleStackPos--;
			scale = GfxScaleStack[GfxScaleStackPos];
		}
	}
}

void newxy(int x, int y)
{
	drawx += (x*scale)&~7;
	drawy += (y*scale)&~7;
}

/* sdraw instruction plus arguments are stored in an 8 bit word.
       76543210
       iixxxyyy
   where i is instruction code
         x is x argument, high bit is sign
         y is y argument, high bit is sign
*/
void sdraw(int d7)
{
	int x,y,x1,y1;

/* getxy1 */
	x = (d7&0x18)>>3;
	if (d7&0x20)
		x = (x|0xfc) - 0x100;
	y = (d7&0x3)<<2;
	if (d7&0x4)
		y = (y|0xf0) - 0x100;

	if (reflectflag&2)
		x = -x;
	if (reflectflag&1)
		y = -y;

/* gintline */
	x1 = drawx;
	y1 = drawy;
	newxy(x,y);

#ifdef L9DEBUG
	printf("gfx - sdraw (%d,%d) (%d,%d) colours %d,%d",
		x1,y1,drawx,drawy,gintcolour&3,option&3);
#endif

	os_drawline(scalex(x1),scaley(y1),scalex(drawx),scaley(drawy),
		gintcolour&3,option&3);
}

/* smove instruction plus arguments are stored in an 8 bit word.
       76543210
       iixxxyyy
   where i is instruction code
         x is x argument, high bit is sign
         y is y argument, high bit is sign
*/
void smove(int d7)
{
	int x,y;

/* getxy1 */
	x = (d7&0x18)>>3;
	if (d7&0x20)
		x = (x|0xfc) - 0x100;
	y = (d7&0x3)<<2;
	if (d7&0x4)
		y = (y|0xf0) - 0x100;

	if (reflectflag&2)
		x = -x;
	if (reflectflag&1)
		y = -y;
	newxy(x,y);
}

void sgosub(int d7, L9BYTE** a5)
{
	int d0 = d7&0x3f;
#ifdef L9DEBUG
	printf("gfx - sgosub 0x%.2x",d0);
#endif
	gosubd0(d0,a5);
}

/* draw instruction plus arguments are stored in a 16 bit word.
       FEDCBA9876543210
       iiiiixxxxxxyyyyy
   where i is instruction code
         x is x argument, high bit is sign
         y is y argument, high bit is sign
*/
void draw(int d7, L9BYTE** a5)
{
	int xy,x,y,x1,y1;

/* getxy2 */
	xy = (d7<<8)+(*(*a5)++);
	x = (xy&0x3e0)>>5;
	if (xy&0x400)
		x = (x|0xe0) - 0x100;
	y = (xy&0xf)<<2;
	if (xy&0x10)
		y = (y|0xc0) - 0x100;

	if (reflectflag&2)
		x = -x;
	if (reflectflag&1)
		y = -y;

/* gintline */
	x1 = drawx;
	y1 = drawy;
	newxy(x,y);

#ifdef L9DEBUG
	printf("gfx - draw (%d,%d) (%d,%d) colours %d,%d",
		x1,y1,drawx,drawy,gintcolour&3,option&3);
#endif

	os_drawline(scalex(x1),scaley(y1),scalex(drawx),scaley(drawy),
		gintcolour&3,option&3);
}

/* move instruction plus arguments are stored in a 16 bit word.
       FEDCBA9876543210
       iiiiixxxxxxyyyyy
   where i is instruction code
         x is x argument, high bit is sign
         y is y argument, high bit is sign
*/
void _move(int d7, L9BYTE** a5)
{
	int xy,x,y;

/* getxy2 */
	xy = (d7<<8)+(*(*a5)++);
	x = (xy&0x3e0)>>5;
	if (xy&0x400)
		x = (x|0xe0) - 0x100;
	y = (xy&0xf)<<2;
	if (xy&0x10)
		y = (y|0xc0) - 0x100;

	if (reflectflag&2)
		x = -x;
	if (reflectflag&1)
		y = -y;
	newxy(x,y);
}

void icolour(int d7)
{
	gintcolour = d7&3;
#ifdef L9DEBUG
	printf("gfx - icolour 0x%.2x",gintcolour);
#endif
}

void size(int d7)
{
	static int sizetable[7] = { 0x02,0x04,0x06,0x07,0x09,0x0c,0x10 };

	d7 &= 7;
	if (d7)
	{
		int d0 = (scale*sizetable[d7-1])>>3;
		scale = (d0 < 0x100) ? d0 : 0xff;
	}
	else
	{
		/* sizereset */
		scale = 0x80;
		if (gfx_mode == GFX_V2 || gfx_mode == GFX_V3A)
			GfxScaleStackPos = 0;	
	}

#ifdef L9DEBUG
	printf("gfx - size 0x%.2x",scale);
#endif
}

void gintfill(int d7)
{
	if ((d7&7) == 0)
/* filla */
		d7 = gintcolour;
	else
		d7 &= 3;
/* fillb */

#ifdef L9DEBUG
	printf("gfx - gintfill (%d,%d) colours %d,%d",drawx,drawy,d7&3,option&3);
#endif

	os_fill(scalex(drawx),scaley(drawy),d7&3,option&3);
}

void gosub(int d7, L9BYTE** a5)
{
	int d0 = ((d7&7)<<8)+(*(*a5)++);
#ifdef L9DEBUG
	printf("gfx - gosub 0x%.2x",d0);
#endif
	gosubd0(d0,a5);
}

void reflect(int d7)
{
#ifdef L9DEBUG
	printf("gfx - reflect 0x%.2x",d7);
#endif

	if (d7&4)
	{
		d7 &= 3;
		d7 ^= reflectflag;
	}
/* reflect1 */
	reflectflag = d7;
}

void notimp(void)
{
#ifdef L9DEBUG
	printf("gfx - notimp");
#endif
}

void gintchgcol(L9BYTE** a5)
{
	int d0 = *(*a5)++;

#ifdef L9DEBUG
	printf("gfx - gintchgcol %d %d",(d0>>3)&3,d0&7);
#endif

	os_setcolour((d0>>3)&3,d0&7);
}

void amove(L9BYTE** a5)
{
	drawx = 0x40*(*(*a5)++);
	drawy = 0x40*(*(*a5)++);
#ifdef L9DEBUG
	printf("gfx - amove (%d,%d)",drawx,drawy);
#endif
}

void opt(L9BYTE** a5)
{
	int d0 = *(*a5)++;
#ifdef L9DEBUG
	printf("gfx - opt 0x%.2x",d0);
#endif

	if (d0)
		d0 = (d0&3)|0x80;
/* optend */
	option = d0;
}

void restorescale(void)
{
#ifdef L9DEBUG
	printf("gfx - restorescale");
#endif
	if (GfxScaleStackPos > 0)
		scale = GfxScaleStack[GfxScaleStackPos-1];
}

L9BOOL rts(L9BYTE** a5)
{
	if (GfxA5StackPos > 0)
	{
		GfxA5StackPos--;
		*a5 = GfxA5Stack[GfxA5StackPos];
		if (GfxScaleStackPos > 0)
		{
			GfxScaleStackPos--;
			scale = GfxScaleStack[GfxScaleStackPos];
		}
		return TRUE;
	}
	return FALSE;
}

L9BOOL getinstruction(L9BYTE** a5)
{
	int d7 = *(*a5)++;
	if ((d7&0xc0) != 0xc0)
	{
		switch ((d7>>6)&3)
		{
		case 0: sdraw(d7); break;
		case 1: smove(d7); break;
		case 2: sgosub(d7,a5); break;
		}
	}
	else if ((d7&0x38) != 0x38)
	{
		switch ((d7>>3)&7)
		{
		case 0: draw(d7,a5); break;
		case 1: _move(d7,a5); break;
		case 2: icolour(d7); break;
		case 3: size(d7); break;
		case 4: gintfill(d7); break;
		case 5: gosub(d7,a5); break;
		case 6: reflect(d7); break;
		}
	}
	else
	{
		switch (d7&7)
		{
		case 0: notimp(); break;
		case 1: gintchgcol(a5); break;
		case 2: notimp(); break;
		case 3: amove(a5); break;
		case 4: opt(a5); break;
		case 5: restorescale(); break;
		case 6: notimp(); break;
		case 7: return rts(a5);
		}
	}
	return TRUE;
}

void absrunsub(int d0)
{
	L9BYTE* a5;
	if (!findsub(d0,&a5))
		return;
	while (getinstruction(&a5));
}

void show_picture(int pic)
{
	if (L9GameType == L9_V3 && strlen(FirstLine) == 0)
	{
		FirstPicture = pic;
		return;
	}

	if (picturedata)
	{
		/* Some games don't call the screen() opcode before drawing
		   graphics, so here graphics are enabled if necessary. */
		if ((screencalled == 0) && (l9textmode == 0))
		{
			detect_gfx_mode();
			l9textmode = 1;
			os_graphics(1);
		}

#ifdef L9DEBUG
		printf("picture %d",pic);
#endif

		os_cleargraphics();
/* gintinit */
		gintcolour = 3;
		option = 0x80;
		reflectflag = 0;
		drawx = 0x1400;
		drawy = 0x1400;
/* sizereset */
		scale = 0x80;

		GfxA5StackPos=0;
		GfxScaleStackPos=0;
		absrunsub(0);
		if (!findsub(pic,&gfxa5))
			gfxa5 = NULL;
	}
}

void picture(void)
{
	show_picture(*getvar());
}

void GetPictureSize(int* width, int* height)
{
	if (L9GameType == L9_V4)
	{
		if (width != NULL)
			*width = 0;
		if (height != NULL)
			*height = 0;
	}
	else
	{
		if (width != NULL)
			*width = (gfx_mode != GFX_V3C) ? 160 : 320;
		if (height != NULL)
			*height = (gfx_mode == GFX_V2) ? 128 : 96;			
	}
}

L9BOOL RunGraphics(void)
{
	if (gfxa5)
	{
		if (!getinstruction(&gfxa5))
			gfxa5 = NULL;
		return TRUE;
	}
	return FALSE;
}

void initgetobj(void)
{
	int i;
	numobjectfound=0;
	object=0;
	for (i=0;i<32;i++) gnoscratch[i]=0;
}

void getnextobject(void)
{
	int d2,d3,d4;
	L9UINT16 *hisearchposvar,*searchposvar;

#ifdef L9DEBUG
	printf("getnextobject");
#endif

	d2=*getvar();
	hisearchposvar=getvar();
	searchposvar=getvar();
	d3=*hisearchposvar;
	d4=*searchposvar;

/* gnoabs */
	do
	{
		if ((d3 | d4)==0)
		{
			/* initgetobjsp */
			gnosp=128;
			searchdepth=0;
			initgetobj();
			break;
		}

		if (numobjectfound==0) inithisearchpos=d3;

	/* gnonext */
		do
		{
			if (d4==list2ptr[++object])
			{
				/* gnomaybefound */
				int d6=list3ptr[object]&0x1f;
				if (d6!=d3)
				{
					if (d6==0 || d3==0) continue;
					if (d3!=0x1f)
					{
						gnoscratch[d6]=d6;
						continue;
					}
					d3=d6;
				}
				/* gnofound */
				numobjectfound++;
				gnostack[--gnosp]=object;
				gnostack[--gnosp]=0x1f;

				*hisearchposvar=d3;
				*searchposvar=d4;
				*getvar()=object;
				*getvar()=numobjectfound;
				*getvar()=searchdepth;
				return;
			}
		} while (object<=d2);

		if (inithisearchpos==0x1f)
		{
			gnoscratch[d3]=0;
			d3=0;

		/* gnoloop */
			do
			{
				if (gnoscratch[d3])
				{
					gnostack[--gnosp]=d4;
					gnostack[--gnosp]=d3;
				}
			} while (++d3<0x1f);
		}
	/* gnonewlevel */
		if (gnosp!=128)
		{
			d3=gnostack[gnosp++];
			d4=gnostack[gnosp++];
		}
		else d3=d4=0;

		numobjectfound=0;
		if (d3==0x1f) searchdepth++;

		initgetobj();
	} while (d4);

/* gnofinish */
/* gnoreturnargs */
	*hisearchposvar=0;
	*searchposvar=0;
	*getvar()=object=0;
	*getvar()=numobjectfound;
	*getvar()=searchdepth;
}

void ifeqct(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
	if (d0==d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]=%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0==d1 ? "Yes":"No");
#endif
}

void ifnect(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
	if (d0!=d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]!=%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0!=d1 ? "Yes":"No");
#endif
}

void ifltct(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
	if (d0<d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]<%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0<d1 ? "Yes":"No");
#endif
}

void ifgtct(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
	if (d0>d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]>%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0>d1 ? "Yes":"No");
#endif
}

void printinput(void)
{
	L9BYTE* ptr=(L9BYTE*) obuff;
	char c;
	while ((c=*ptr++)!=' ') printchar(c);

#ifdef L9DEBUG
	printf("printinput");
#endif
}

void listhandler(void)
{
	L9BYTE *a4,*MinAccess,*MaxAccess;
	L9UINT16 val;
	L9UINT16 *var;
#ifdef CODEFOLLOW
	int offset; 
#endif

	if ((code&0x1f)>0xa)
	{
		error("\rillegal list access %d\r",code&0x1f);
		Running=FALSE;
		return;
	}
	a4=L9Pointers[1+code&0x1f];

	if (a4>=workspace.listarea && a4<workspace.listarea+LISTAREASIZE)
	{
		MinAccess=workspace.listarea;
		MaxAccess=workspace.listarea+LISTAREASIZE;
	}
	else
	{
		MinAccess=startdata;
		MaxAccess=startdata+FileSize;
	}

	if (code>=0xe0)
	{
		/* listvv */
#ifndef CODEFOLLOW
		a4+=*getvar();
		val=*getvar();
#else
		offset=*getvar();
		a4+=offset;
		var=getvar();
		val=*var;
		fprintf(f," list %d [%d]=Var[%d] (=%d)",code&0x1f,offset,var-workspace.vartable,val);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *a4=(L9BYTE) val;
		#ifdef L9DEBUG
		else printf("Out of range list access");
		#endif
	}
	else if (code>=0xc0)
	{
		/* listv1c */
#ifndef CODEFOLLOW
		a4+=*codeptr++;
		var=getvar();
#else
		offset=*codeptr++;
		a4+=offset;
		var=getvar();
		fprintf(f," Var[%d]= list %d [%d])",var-workspace.vartable,code&0x1f,offset);
		if (a4>=MinAccess && a4<MaxAccess) fprintf(f," (=%d)",*a4);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *var=*a4;
		else
		{
			*var=0;
			#ifdef L9DEBUG
			printf("Out of range list access");
			#endif
		}
	}
	else if (code>=0xa0)
	{
		/* listv1v */
#ifndef CODEFOLLOW
		a4+=*getvar();
		var=getvar();
#else
		offset=*getvar();
		a4+=offset;
		var=getvar();

		fprintf(f," Var[%d] =list %d [%d]",var-workspace.vartable,code&0x1f,offset);
		if (a4>=MinAccess && a4<MaxAccess) fprintf(f," (=%d)",*a4);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *var=*a4;
		else
		{
			*var=0;
			#ifdef L9DEBUG
			printf("Out of range list access");
			#endif
		}
	}
	else
	{
#ifndef CODEFOLLOW
		a4+=*codeptr++;
		val=*getvar();
#else
		offset=*codeptr++;
		a4+=offset;
		var=getvar();
		val=*var;
		fprintf(f," list %d [%d]=Var[%d] (=%d)",code&0x1f,offset,var-workspace.vartable,val);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *a4=(L9BYTE) val;
		#ifdef L9DEBUG
		else printf("Out of range list access");
		#endif
	}
}

void executeinstruction(void)
{
#ifdef CODEFOLLOW
	f=fopen(CODEFOLLOWFILE,"a");
	fprintf(f,"%ld (s:%d) %x",(L9UINT32) (codeptr-acodeptr)-1,workspace.stackptr,code);
	if (!(code&0x80))
		fprintf(f," = %s",codes[code&0x1f]);
#endif

	if (code & 0x80)
		listhandler();
	else
	{
		switch (code & 0x1f)
		{
			case 0:		Goto();break;
			case 1: 	intgosub();break;
			case 2:		intreturn();break;
			case 3:		printnumber();break;
			case 4:		messagev();break;
			case 5:		messagec();break;
			case 6:		function();break;
			case 7:		input();break;
			case 8:		varcon();break;
			case 9:		varvar();break;
			case 10:	_add();break;
			case 11:	_sub();break;
			case 12:	ilins(code & 0x1f);break;
			case 13:	ilins(code & 0x1f);break;
			case 14:	jump();break;
			case 15:	Exit();break;
			case 16:	ifeqvt();break;
			case 17:	ifnevt();break;
			case 18:	ifltvt();break;
			case 19:	ifgtvt();break;
			case 20:	_screen();break;
			case 21:	cleartg();break;
			case 22:	picture();break;
			case 23:	getnextobject();break;
			case 24:	ifeqct();break;
			case 25:	ifnect();break;
			case 26:	ifltct();break;
			case 27:	ifgtct();break;
			case 28:	printinput();break;
			case 29:	ilins(code & 0x1f);break;
			case 30:	ilins(code & 0x1f);break;
			case 31:	ilins(code & 0x1f);break;
		}
	}
#ifdef CODEFOLLOW
	fprintf(f,"\n");
	fclose(f);
#endif
}

L9BOOL LoadGame2(char *filename,char *picname)
{
#ifdef CODEFOLLOW
	f=fopen(CODEFOLLOWFILE,"w");
	fprintf(f,"Code follow file...\n");
	fclose(f);
#endif

	/* may be already running a game, maybe in input routine */
	Running=FALSE;
	ibuffptr=NULL;

/* intstart */
	if (!intinitialise(filename,picname)) return FALSE;
/*	if (!checksumgamedata()) return FALSE; */

	codeptr=acodeptr;
	if (constseed > 0)
		randomseed=constseed;
	else
		randomseed=(L9UINT16)time(NULL);
	strcpy(LastGame,filename);
	return Running=TRUE;
}

L9BOOL LoadGame(char *filename,char *picname)
{
	L9BOOL ret=LoadGame2(filename,picname);
	showtitle=1;
	clearworkspace();
	workspace.stackptr=0;
	/* need to clear listarea as well */
	memset((L9BYTE*) workspace.listarea,0,LISTAREASIZE);
	return ret;
}

/* can be called from input to cause fall through for exit */
void StopGame(void)
{
	Running=FALSE;
}

L9BOOL RunGame(void)
{
	code=*codeptr++;
/*	printf("%d",code); */
	executeinstruction();
	return Running;
}

void RestoreGame(char* filename)
{
	int Bytes;
	GameState temp;
	FILE* f = NULL;

	if ((f = fopen(filename, "rb")) != NULL)
	{
		Bytes = fread(&temp, 1, sizeof(GameState), f);
		if (Bytes==V1FILESIZE)
		{
			printstring("\rGame restored.\r");
			/* only copy in workspace */
			memset(workspace.listarea,0,LISTAREASIZE);
			memmove(workspace.vartable,&temp,V1FILESIZE);
		}
		else if (CheckFile(&temp))
		{
			printstring("\rGame restored.\r");
			/* full restore */
			memmove(&workspace,&temp,sizeof(GameState));
			codeptr=acodeptr+workspace.codeptr;
		}
		else
			printstring("\rSorry, unrecognised format. Unable to restore\r");
	}
	else
		printstring("\rUnable to restore game.\r");
}

