/*
 *	ScottFree Revision 1.14
 *
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *
 *	You must have an ANSI C compiler to build this program.
 */

/*
 * Parts of this source file (mainly the Glk parts) are copyright 2011
 * Chris Spiegel.
 *
 * Some notes about the Glk version:
 *
 * o Room descriptions, exits, and visible items can, as in the
 *   original, be placed in a window at the top of the screen, or can be
 *   inline with user input in the main window.  The former is default,
 *   and the latter can be selected with the -w flag.
 *
 * o Game saving and loading uses Glk, which means that providing a
 *   save game file on the command-line will no longer work.  Instead,
 *   the verb "restore" has been special-cased to call GameLoad(), which
 *   now prompts for a filename via Glk.
 *
 * o The local character set is expected to be compatible with ASCII, at
 *   least in the printable character range.  Newlines are specially
 *   handled, however, and converted to Glk's expected newline
 *   character.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>

#include "glk.h"
#include "glkstart.h"

#include "sagadraw.h"

#include "bsd.h"
#include "scott.h"

#ifdef SPATTERLIGHT
extern glui32 gli_determinism;
#endif

static const char *game_file;

#ifdef __GNUC__
__attribute__((__format__(__printf__, 2, 3)))
#endif


static Header GameHeader;
static Item *Items;
static Room *Rooms;
static const char **Verbs;
static const char **Nouns;
static const char **Messages;
static Action *Actions;
static int LightRefill;
static char NounText[16];
static int Counters[16];    /* Range unknown */
static int CurrentCounter;
static int SavedRoom;
static int RoomSaved[16];    /* Range unknown */
static int Options;        /* Option flags set */
static glui32 Width;        /* Terminal width */
static glui32 TopHeight;        /* Height of top window */
static int pixel_size = 2;
static int x_offset = 0;
static int file_baseline_offset = 0;


static int split_screen = 1;
static winid_t Bottom, Top, Graphics;

static int AnimationFlag = 0;
#define ANIMATION_RATE 670

#define GLK_BUFFER_ROCK 1
#define GLK_STATUS_ROCK 1010
#define GLK_GRAPHICS_ROCK 1020

//#define TRS80_LINE    "\n<------------------------------------------------------------>\n"

#define MyLoc    (GameHeader.PlayerRoom)


static void Display(winid_t w, const char *fmt, ...)
{
	va_list ap;
	char msg[2048];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);

	/* Map newline to a Glk-compatible newline. */
	for(size_t i = 0; msg[i] != 0; i++)
	{
		if(msg[i] == '\n') msg[i] = 10;
	}

	glk_put_string_stream(glk_window_get_stream(w), msg);
}

static void Delay(int seconds)
{
	event_t ev;

	if(!glk_gestalt(gestalt_Timer, 0))
		return;

	glk_request_timer_events(1000 * seconds);

	char buf[1];
	glk_request_line_event(Bottom, buf, 1, 0);
	glk_cancel_line_event(Bottom, NULL);

	do
	{
		glk_select(&ev);
	} while(ev.type != evtype_Timer);

	glk_request_timer_events(0);
}


winid_t find_glk_window_with_rock(glui32 rock)
{
	winid_t win;
	glui32 rockptr;
	for (win = glk_window_iterate(NULL, &rockptr); win; win = glk_window_iterate(win, &rockptr))
	{
		if (rockptr == rock)
			return win;
	}
	return 0;
}

const glui32 optimal_picture_size(glui32 *width, glui32 *height)
{
	*width = 255; *height = 95;
	int multiplier = 1;
	glui32 graphwidth, graphheight;
	glk_window_get_size(Graphics, &graphwidth, &graphheight);
	multiplier = graphheight / 95;
	if (255 * multiplier > graphwidth)
		multiplier = graphwidth / 255;

	*width = 255 * multiplier;
	*height = 95 * multiplier;

	return multiplier;
}


void open_graphics_window(void)
{
	glui32 graphwidth, graphheight, optimal_width, optimal_height;

	if (Top == NULL)
		Top = find_glk_window_with_rock(GLK_STATUS_ROCK);
	if (Top == NULL)
		return;
	if (Graphics == NULL)
		Graphics = find_glk_window_with_rock(GLK_GRAPHICS_ROCK);
	if (Graphics == NULL) {
		glk_window_get_size(Top, &Width, &TopHeight);
		glk_window_close(Top, NULL);
		Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional, 60, wintype_Graphics, GLK_GRAPHICS_ROCK);
		glk_window_get_size(Graphics, &graphwidth, &graphheight);
		pixel_size = optimal_picture_size(&optimal_width, &optimal_height);
		x_offset = ((int)graphwidth - (int)optimal_width) / 2;

		if (graphheight > optimal_height)
		{
			glk_window_close(Graphics, NULL);
			Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, optimal_height, wintype_Graphics, GLK_GRAPHICS_ROCK);
		}

		/* Set the graphics window background to match
		 * the main window background, best as we can,
		 * and clear the window.
		 */
		glui32 background_color;
		if (glk_style_measure (Bottom,
							   style_Normal, stylehint_BackColor, &background_color))
		{
			glk_window_set_background_color (Graphics, background_color);
			glk_window_clear(Graphics);
		}

		Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, TopHeight, wintype_TextGrid, GLK_STATUS_ROCK);
		glk_window_get_size(Top, &Width, &TopHeight);
	} else {
		glk_window_get_size(Graphics, &graphwidth, &graphheight);
		pixel_size = optimal_picture_size(&optimal_width, &optimal_height);
		x_offset = ((int)graphwidth - (int)optimal_width) / 2;
	}
}

void close_graphics_window(void)
{
	if (Graphics == NULL)
		Graphics = find_glk_window_with_rock(GLK_GRAPHICS_ROCK);
	if (Graphics) {
		glk_window_close(Graphics, NULL);
		Graphics = NULL;
		glk_window_get_size(Top, &Width, &TopHeight);
	}
}

static long BitFlags=0;	/* Might be >32 flags - I haven't seen >32 yet */

static void Fatal(const char *x)
{
	Display(Bottom, "%s\n", x);
	glk_exit();
}

static void ClearScreen(void)
{
	glk_window_clear(Bottom);
}

static void *MemAlloc(int size)
{
	void *t=(void *)malloc(size);
	if(t==NULL)
		Fatal("Out of memory");
	return(t);
}

static int RandomPercent(int n)
{
	unsigned int rv=rand()<<6;
	rv%=100;
	if(rv<n)
		return(1);
	return(0);
}

static int CountCarried(void)
{
	int ct=0;
	int n=0;
	while(ct<=GameHeader.NumItems)
	{
		if(Items[ct].Location==CARRIED)
			n++;
		ct++;
	}
	return(n);
}

static const char *MapSynonym(const char *word)
{
	int n=1;
	const char *tp;
	static char lastword[16];	/* Last non synonym */
	while(n<=GameHeader.NumWords)
	{
		tp=Nouns[n];
		if(*tp=='*')
			tp++;
		else
			strcpy(lastword,tp);
		if(xstrncasecmp(word,tp,GameHeader.WordLength)==0)
			return(lastword);
		n++;
	}
	return(NULL);
}

static int MatchUpItem(const char *text, int loc)
{
	const char *word=MapSynonym(text);
	int ct=0;

	if(word==NULL)
		word=text;

	while(ct<=GameHeader.NumItems)
	{
		if(Items[ct].AutoGet && Items[ct].Location==loc &&
		   xstrncasecmp(Items[ct].AutoGet,word,GameHeader.WordLength)==0)
			return(ct);
		ct++;
	}
	return(-1);
}

static char *ReadString(FILE *f)
{
	char tmp[1024];
	char *t;
	int c,nc;
	int ct=0;
	do
	{
		c=fgetc(f);
	}
	while(c!=EOF && isspace(c));
	if(c!='"')
	{
		Fatal("Initial quote expected");
	}
	do
	{
		c=fgetc(f);
		if(c==EOF)
			Fatal("EOF in string");
		if(c=='"')
		{
			nc=fgetc(f);
			if(nc!='"')
			{
				ungetc(nc,f);
				break;
			}
		}
		if(c=='`')
			c='"'; /* pdd */

		/* Ensure a valid Glk newline is sent. */
		if(c=='\n')
			tmp[ct++]=10;
		/* Special case: assume CR is part of CRLF in a
		 * DOS-formatted file, and ignore it.
		 */
		else if(c==13)
			;
		/* Pass only ASCII to Glk; the other reasonable option
		 * would be to pass Latin-1, but it's probably safe to
		 * assume that Scott Adams games are ASCII only.
		 */
		else if((c >= 32 && c <= 126))
			tmp[ct++]=c;
		else
			tmp[ct++]='?';
	}
	while(1);
	tmp[ct]=0;
	t=MemAlloc(ct+1);
	memcpy(t,tmp,ct+1);
	return(t);
}

int header[36];

void readheader(FILE *in) {
	int i,value;
	for (i = 0; i < 36; i++)
	{
		value = fgetc(in) + 256 * fgetc(in);
		header[i] = value;
	}
}

void read_dictionary(FILE *ptr)
{
	char dictword[6];
	char c = 0;
	int wordnum = 0;
	int charindex = 0;

	do {
		for (int i = 0; i < 4; i++)
		{
			c=fgetc(ptr);
			if (c == 0)
			{
				if (charindex == 0)
				{
					c=fgetc(ptr);
				}
			}
			dictword[charindex] = c;
			if (c == '*')
				i--;
			charindex++;
		}
		dictword[charindex] = 0;
		//        if (!strcmp(".   ", dictword))
		//            periodonly++;

		if (wordnum < 115)
		{
			Verbs[wordnum] = malloc(charindex+1);
			memcpy((char *)Verbs[wordnum], dictword, charindex+1);
			//            fprintf(stderr, "Verb %d: \"%s\"\n", wordnum, Verbs[wordnum]);
		} else {
			Nouns[wordnum-115] = malloc(charindex+1);
			memcpy((char *)Nouns[wordnum-115], dictword, charindex+1);
			//            fprintf(stderr, "Noun %d: \"%s\"\n", wordnum-115, Nouns[wordnum-115]);
		}
		wordnum++;
		//      }
		if (c != 0 && !isascii(c))
			return;
		charindex = 0;
	} while (wordnum < header[3] * 2 - 10);

	for (int i = 0; i < 12; i++)
		Verbs[115 + i] = ".\0";
}

size_t get_file_length(FILE *in) {
	if (fseek(in, 0, SEEK_END) == -1)
	{
		return 0;
	}
	size_t file_length = ftell(in);
	if (file_length == -1)
	{
		return 0;
	}
	return file_length;
}

int sanity_check_header(void) {

	int16_t v = header[1]; // items
	if (v < 10 || v > 500)
		return 0;
	v = header[2]; // actions
	if (v < 100 || v > 500)
		return 0;
	v = header[3]; // word pairs
	if (v < 50 || v > 200)
		return 0;
	v = header[4]; // Number of rooms
	if (v < 10 || v > 100)
		return 0;

	return 1;
}

static void LoadDatabase(FILE *f, int loud)
{
	int ni,na,nw,nr,mc,pr,tr,wl,lt,mn,trm;
	int ct;
	//    short lo;
	Action *ap;
	Room *rp;
	Item *ip;
	loud = 0;
	/* Load the header */


	readheader(f);

	size_t file_length = get_file_length(f);

	size_t pos = 0;

	do {
		fseek(f, pos, SEEK_SET);
		readheader(f);
		pos++;
		if (pos >= file_length - 24) {
			fprintf(stderr, "found no valid header in file\n");
			return;
		}
	} while (sanity_check_header() == 0);

	fprintf(stderr, "Found a valid header at file offset %zx\n", pos);
	file_baseline_offset = pos - 0x2371;

	fprintf(stderr, "Difference from baseline .sna file: %x\n", file_baseline_offset);

	ni = header[1];
	na = header[2];
	nw = header[3];
	nr = header[4];
	mc = header[5];
	wl = header[6];
	mn = header[7];
	pr = 1;
	tr = 0;
	lt = -1;
	trm = 0;
	//    if(fscanf(f,"%*d %d %d %d %d %d %d %d %d %d %d %d",
	//        &ni,&na,&nw,&nr,&mc,&pr,&tr,&wl,&lt,&mn,&trm)<10)
	//        Fatal("Invalid database(bad header)");
	GameHeader.NumItems=ni;
	Items=(Item *)MemAlloc(sizeof(Item)*(ni+1));
	GameHeader.NumActions=na;
	Actions=(Action *)MemAlloc(sizeof(Action)*(na+1));
	GameHeader.NumWords=nw;
	GameHeader.WordLength=wl;
	Verbs=MemAlloc(sizeof(char *)*(nw+1));
	Nouns=MemAlloc(sizeof(char *)*(nw+1));
	GameHeader.NumRooms=nr;
	Rooms=(Room *)MemAlloc(sizeof(Room)*(nr+1));
	GameHeader.MaxCarry=mc;
	GameHeader.PlayerRoom=pr;
	GameHeader.Treasures=tr;
	GameHeader.LightTime=lt;
	LightRefill=lt;
	GameHeader.NumMessages=mn;
	Messages=MemAlloc(sizeof(char *)*(mn+1));
	GameHeader.TreasureRoom=trm;

#pragma mark room images

	/* Load the room images */

	fseek(f,0x3A09 + file_baseline_offset,SEEK_SET);

	rp=Rooms;

	for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
		rp->Image = fgetc(f);
		rp++;
	}

#pragma mark Item flags

	/* Load the item flags */

	ip=Items;

	for (ct = 0; ct <= GameHeader.NumItems; ct++) {
		ip->Flag = fgetc(f);
		ip++;
	}


#pragma mark item images

	/* Load the item images */

	ip=Items;

	for (ct = 0; ct <= GameHeader.NumItems; ct++) {
		ip->Image = fgetc(f);
		ip++;
	}


#pragma mark actions

	/* Load the actions */

//	fseek(f,15102 + file_baseline_offset,SEEK_SET);

	fgetc(f); fgetc(f); // Skip two bytes

	ct=0;

	Actions=(Action *)malloc(sizeof(Action)*(na+1));
	ap=Actions;

	int value, cond, comm;
	while(ct<na+1)
	{
		value = fgetc(f) + 256 * fgetc(f); /* verb/noun */
		ap->Vocab = value;
		value = fgetc(f); /* count of actions/conditions */
		cond = value & 0x1f;
		comm = (value & 0xe0) >> 5;
		for (int j = 0; j < 5; j++)
		{
			if (j < cond) value = fgetc(f) + 256 * fgetc(f); else value = 0;
			ap->Condition[j] = value;
		}
		for (int j = 0; j < 2; j++)
		{
			if (j < comm) value = fgetc(f) + 256 * fgetc(f); else value = 0;
			ap->Action[j] = value;
		}

		ap++;
		ct++;
	}

#pragma mark dictionary

	read_dictionary(f);

#pragma mark rooms

	ct=0;
	rp=Rooms;
	if(loud)
		fprintf(stderr, "Reading %d rooms.\n",nr);

	char text[256];
	char c=0;
	int charindex = 0;

	do {
		c=fgetc(f);
		text[charindex] = c;
		if (c == 0) {
			rp->Text = malloc(charindex + 1);
			strcpy(rp->Text, text);
			ct++;
			rp++;
			charindex = 0;
		} else {
			charindex++;
		}

		if (c != 0 && !isascii(c))
			return;
	} while (ct<nr+1);


#pragma mark room connections

	ct=0;
	rp=Rooms;
	if(loud)
		fprintf(stderr, "Reading %d X 6 room connections.\n",nr);
	while(ct<nr+1)
	{
		for (int j= 0; j < 6; j++) {
			rp->Exits[j] = fgetc(f);
		}
		ct++;
		rp++;
	}

#pragma mark room messages

	ct=0;
	if(loud)
		fprintf(stderr, "Reading %d messages.\n",mn);
	while(ct<mn+1)
	{
		c=fgetc(f);
		text[charindex] = c;
		if (c == 0) {
			Messages[ct] = malloc(charindex + 1);
			strcpy((char *)Messages[ct], text);
			ct++;
			charindex = 0;
		} else {
			charindex++;
		}
	}

#pragma mark items

	ct=0;
	charindex = 0;
	if(loud)
		fprintf(stderr, "Reading %d items.\n",ni);
	ip=Items;


	do {

		c=fgetc(f);
		text[charindex] = c;
		if (c == 0) {
			ip->Text = malloc(charindex + 1);

			strcpy(ip->Text, text);
			//            fprintf(stderr, "item %d: \"%s\"\n",ct, ip->Text);



			ip->AutoGet=strchr(ip->Text,'/');
			/* Some games use // to mean no auto get/drop word! */
			if(ip->AutoGet && strcmp(ip->AutoGet,"//") && strcmp(ip->AutoGet,"/*"))
			{
				char *t;
				*ip->AutoGet++=0;
				t=strchr(ip->AutoGet,'/');
				if(t!=NULL)
					*t=0;
			}



			ct++;
			ip++;
			charindex = 0;
		} else {
			charindex++;
		}
	} while(ct<ni+1);

#pragma mark item locations

	ct=0;
	ip=Items;
	while(ct<ni+1)
	{
		ip->Location=fgetc(f);
		ip->InitialLoc=ip->Location;
		ip++;
		ct++;
	}
	/* Discard Comment Strings */
	//    while(ct<na+1)
	//    {
	//        free(ReadString(f));
	//        ct++;
	//    }
	//    if(fscanf(f,"%d",&ct)!=1)
	//    {
	//        puts("Cannot read version");
	//        exit(1);
	//    }
	//    if(loud)
	//        printf("Version %d.%02d of Adventure ",
	//        ct/100,ct%100);
	//    if(fscanf(f,"%d",&ct)!=1)
	//    {
	//        puts("Cannot read adventure number");
	//        exit(1);
	//    }
	if(loud)
		fprintf(stderr, "%d.\nLoad Complete.\n\n",ct);
}

static void Output(const char *a)
{
	Display(Bottom, "%s", a);
}

static void OutputNumber(int a)
{
	Display(Bottom, "%d", a);
}

static void DrawImage(int image) {
	open_graphics_window();
	if (Graphics == NULL)
		return;
	draw_saga_picture(image, game_file, "o2", "zxopt", Graphics, pixel_size, x_offset, file_baseline_offset);
}


static void DrawAnimations(void) {
	if (Rooms[MyLoc].Image == 255) {
		glk_request_timer_events(0);
		return;
	}
	open_graphics_window();
	if (Graphics == NULL) {
		glk_request_timer_events(0);
		return;
	}

	int timer_delay = ANIMATION_RATE;
	switch(MyLoc) {
		case 1: /* Bedroom */
			if (Items[50].Location == 1)  /* Gremlin throwing darts */
			{
				if (AnimationFlag)
				{
					DrawImage(60); /* Gremlin throwing dart frame 1 */
				} else {
					DrawImage(59); /* Gremlin throwing dart frame 2 */
				}
			}
			break;
		case 17: /* Dotty's Tavern */
			if (Items[82].Location == 17)  /* Gang of GREMLINS */
			{
				if (AnimationFlag)
				{
					DrawImage(49); /* Gremlin hanging from curtains frame 1 */
					DrawImage(51); /* Gremlin ear frame 1 */
					DrawImage(54); /* Stripe's mouth frame 1 */
				} else {
					DrawImage(50); /* Gremlin hanging from curtains frame 2 */
					DrawImage(52); /* Gremlin ear frame 2 */
					DrawImage(53); /* Stripe's mouth frame 2 */
				}
			}
			break;
		case 16: /* Behind a Bar */
			if (Items[82].Location == 16)  /* Gang of GREMLINS */
			{
				if (AnimationFlag)
				{
					DrawImage(57); /* Flasher gremlin frame 1 */
					DrawImage(24); /* Gremlin tongue frame 1 */
					DrawImage(73); /* Gremlin ear frame 1 */
				} else {
					DrawImage(58); /* Flasher gremlin frame 2 */
					DrawImage(72); /* Gremlin tongue frame 2 */
					DrawImage(74); /* Gremlin ear frame 2 */
				}
			}
			break;
		case 19: /* Square */
			if (Items[82].Location == 19)  /* Gang of GREMLINS */
			{
				if (AnimationFlag)
				{
					DrawImage(55); /* Silhouette of Gremlins frame 1 */
				} else {
					DrawImage(71); /* Silhouette of Gremlins frame 1 */
				}
			}
			break;
		case 6: /* on a road */
			if (Items[82].Location == 6)  /* Gang of GREMLINS */
			{
				if (AnimationFlag)
				{
					DrawImage(75); /* Silhouette 2 of Gremlins  */
				} else {
					DrawImage(48); /* Silhouette 2 of Gremlins flipped */
				}
			}
			break;
		case 3:  /* Kitchen */
			if (Counters[2] == 2)  /* Blender is on */
			{
				if (AnimationFlag)
				{
					DrawImage(56); /* Blended Gremlin */
				} else {
					DrawImage(12); /* Blended Gremlin flipped */
				}
			}
			break;
		default:
			timer_delay = 0;
			break;
	}
	AnimationFlag = (AnimationFlag == 0);
	glk_request_timer_events(timer_delay);
}

static void adjust_top_height(int ypos);


static void print_top_window_delimiter(int ypos) {
	glk_window_move_cursor(Top, 0, ypos);
	glk_window_get_size(Top, &Width, NULL);
	for (int i = 0; i < Width; i++)
		Display(Top,"*");
}

static int list_exits(int ypos)
{
	static char *ExitNames[6]=
	{
//		"North","South","East","West","Up","Down"
		"NORTH","SOUTH","EAST","WEST","UP","DOWN"

	};

	int ct=0;
	int f=0;
	while(ct<6)
	{
		if((&Rooms[MyLoc])
->Exits[ct]!=0)
		{
			if(f==0)
			{
				glk_window_move_cursor(Top, 0, ypos+1);
				Display(Top,"Exits: ");
				f=1;
			}
			else
				Display(Top,", ");
			Display(Top,"%s",ExitNames[ct]);
		}
		ct++;
	}
	//	if(f==0)
	//		Display(Top,"none");
	if(f!=0) {
//		Display(Top,".\n");
//		Display(Top,"\n");
		return 2;
	}
	return 0;
}


static void Look(void)
{
	glk_window_get_size(Top, &Width, &TopHeight);

	Room *r;
	int ct,f;
	int xpos, ypos = 1;

	if(split_screen) {
		glk_window_clear(Top);
		if (Rooms[MyLoc].Image != 255) {
			if (MyLoc == 17 && Items[82].Location == 17)
				DrawImage(45); /* Bar full of Gremlins */
			else
				DrawImage(Rooms[MyLoc].Image);
			AnimationFlag = 0;
			DrawAnimations();
		} else {
			close_graphics_window();
		}
	}

	if((BitFlags&(1<<DARKBIT)) && Items[LIGHT_SOURCE].Location!= CARRIED
	   && Items[LIGHT_SOURCE].Location!= MyLoc)
	{
		if(Options&YOUARE)
			Display(Top,"You can't see. It is too dark!\n");
		else
			Display(Top,"I can't see. It is too dark!\n");
		if (Options & TRS80_STYLE)
			print_top_window_delimiter(++ypos);
		return;
	}
	r=&Rooms[MyLoc];
	if(*r->Text=='*')
//		Display(Top,"%s\n",r->Text+1);
		Display(Top,"%s",r->Text+1);
	else
	{
		if(Options&YOUARE)
			Display(Top,"You are in a %s\n",r->Text);
		else
			Display(Top,"I'm in a %s",r->Text);
	}
//	ct=0;
//	f=0;
//	while(ct<6)
//	{
//		if(r->Exits[ct]!=0)
//		{
//			if(f==0)
//			{
//				Display(Top,"\nObvious exits: ");
//				f=1;
//			}
//			else
//				Display(Top,", ");
//			Display(Top,"%s",ExitNames[ct]);
//		}
//		ct++;
//	}
////	if(f==0)
////		Display(Top,"none");
//	if(f!=0) {
//	Display(Top,".\n");
//	ypos++;
//	}
	ct=0;
	f=0;
	xpos=0;
	while(ct<=GameHeader.NumItems)
	{
		if(Items[ct].Location==MyLoc)
		{
			if(f==0)
			{
				if(Options&YOUARE)
				{
					Display(Top,"\nYou can also see: ");
					xpos=18;
				}
				else
				{
//					Display(Top,"\nI can also see: ");
					Display(Top,". Things I see:\n\n");
					xpos=0;
					ypos=2;
				}
				f++;
			}
			else if (!(Options & TRS80_STYLE))
			{
				Display(Top," - ");
				xpos+=3;
			}
			if(xpos+strlen(Items[ct].Text)>=(Width - 1))
			{
				xpos=0;
				ypos++;
//				Display(Top,"\n");
				glk_window_move_cursor(Top, xpos, ypos);
			}
			Display(Top,"%s",Items[ct].Text);
			xpos += strlen(Items[ct].Text);
			if (Options & TRS80_STYLE)
			{
				Display(Top,". ");
				xpos+=2;
			}
			if (split_screen && Items[ct].Image) {
				if ((Items[ct].Flag & 127) == MyLoc) {
					DrawImage(Items[ct].Image);
				}
				if (MyLoc == 34 && ct == 53) {
					DrawImage(42);
				}
			}
		}
		ct++;
	}
	if (f)
		ypos++;
	ypos += list_exits(ypos);
	Display(Top,"\n");
	if (Options & TRS80_STYLE) {
		print_top_window_delimiter(ypos);
		ypos++;
	}
	adjust_top_height(ypos);
}

static void adjust_top_height(int ypos)
{
	if (TopHeight != ypos) {
		glui32 bottomheight;
		glk_window_get_size(Bottom, NULL, &bottomheight);
		if (bottomheight < 3 && TopHeight < ypos)
			return;
		winid_t o2;
		o2 = glk_window_get_parent(Top);
		glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed, ypos, Top);
		if (TopHeight < ypos) {
			glk_window_get_size(Top, &Width, &TopHeight);
			Look();
		}
		glk_window_get_size(Top, &Width, &TopHeight);
	}
}

static int WhichWord(const char *word, const char **list)
{
	int n=1;
	int ne=1;
	const char *tp;
	while(ne<=GameHeader.NumWords)
	{
		tp=list[ne];
		if(*tp=='*')
			tp++;
		else
			n=ne;
		if(xstrncasecmp(word,tp,GameHeader.WordLength)==0)
			return(n);
		ne++;
	}
	return(-1);
}

static void LineInput(char *buf, size_t n)
{
	event_t ev;

	glk_request_line_event(Bottom, buf, (glui32)(n - 1), 0);

	while(1)
	{
		glk_select(&ev);

		if(ev.type == evtype_LineInput)
			break;
		else if(ev.type == evtype_Arrange && split_screen) {
			close_graphics_window();
			Look();
		}
		else if(ev.type == evtype_Timer && split_screen)
			DrawAnimations();
	}

	buf[ev.val1] = 0;
}

static void SaveGame(void)
{
	strid_t file;
	frefid_t ref;
	int ct;
	char buf[128];

	ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame, filemode_Write, 0);
	if(ref == NULL) return;

	file = glk_stream_open_file(ref, filemode_Write, 0);
	glk_fileref_destroy(ref);
	if(file == NULL) return;

	for(ct=0;ct<16;ct++)
	{
		snprintf(buf, sizeof buf, "%d %d\n",Counters[ct],RoomSaved[ct]);
		glk_put_string_stream(file, buf);
	}
	snprintf(buf, sizeof buf, "%ld %d %hd %d %d %hd\n",BitFlags, (BitFlags&(1<<DARKBIT))?1:0,
			 MyLoc,CurrentCounter,SavedRoom,GameHeader.LightTime);
	glk_put_string_stream(file, buf);
	for(ct=0;ct<=GameHeader.NumItems;ct++)
	{
		snprintf(buf, sizeof buf, "%hd\n",(short)Items[ct].Location);
		glk_put_string_stream(file, buf);
	}

	glk_stream_close(file, NULL);
	Output("Saved.\n");
}

static void LoadGame(void)
{
	strid_t file;
	frefid_t ref;
	char buf[128];
	int ct=0;
	short lo;
	short DarkFlag;

	ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame, filemode_Read, 0);
	if(ref == NULL) return;

	file = glk_stream_open_file(ref, filemode_Read, 0);
	glk_fileref_destroy(ref);
	if(file == NULL) return;

	for(ct=0;ct<16;ct++)
	{
		glk_get_line_stream(file, buf, sizeof buf);
		sscanf(buf, "%d %d",&Counters[ct],&RoomSaved[ct]);
	}
	glk_get_line_stream(file, buf, sizeof buf);
	sscanf(buf, "%ld %hd %hd %d %d %hd\n",
		   &BitFlags,&DarkFlag,&MyLoc,&CurrentCounter,&SavedRoom,
		   &GameHeader.LightTime);
	/* Backward compatibility */
	if(DarkFlag)
		BitFlags|=(1<<15);
	for(ct=0;ct<=GameHeader.NumItems;ct++)
	{
		glk_get_line_stream(file, buf, sizeof buf);
		sscanf(buf, "%hd\n",&lo);
		Items[ct].Location=(unsigned char)lo;
	}
}

static int GetInput(int *vb, int *no)
{
	char buf[256];
	char verb[10],noun[10];
	int vc,nc;
	int num;

	do
	{
		do
		{
//			Output("\nTell me what to do ? ");
			Output("\n--WHAT NOW ? ");
			LineInput(buf, sizeof buf);
			num=sscanf(buf,"%9s %9s",verb,noun);
		}
		while(num==0||*buf=='\n');
		if(xstrcasecmp(verb, "restore") == 0)
		{
			LoadGame();
			return -1;
		}
		if(num==1)
			*noun=0;
		if(*noun==0 && strlen(verb)==1)
		{
			switch(isupper((unsigned char)*verb)?tolower((unsigned char)*verb):*verb)
			{
				case 'n':strcpy(verb,"NORTH");break;
				case 'e':strcpy(verb,"EAST");break;
				case 's':strcpy(verb,"SOUTH");break;
				case 'w':strcpy(verb,"WEST");break;
				case 'u':strcpy(verb,"UP");break;
				case 'd':strcpy(verb,"DOWN");break;
				/* Brian Howarth interpreter also supports this */
				case 'i':strcpy(verb,"INVENTORY");break;
			}
		}
		nc=WhichWord(verb,Nouns);
		/* The Scott Adams system has a hack to avoid typing 'go' */
		if(nc>=1 && nc <=6)
		{
			vc=1;
		}
		else
		{
			vc=WhichWord(verb,Verbs);
			nc=WhichWord(noun,Nouns);
		}
		*vb=vc;
		*no=nc;
		if(vc==-1)
		{
//			Output("You use word(s) I don't know! ");
			Output("\"");
			Output(verb);
			Output("\" is not a word I know. ");
		}
	}
	while(vc==-1);
	strcpy(NounText,noun);	/* Needed by GET/DROP hack */

	return 0;
}

static int PerformLine(int ct)
{
//	fprintf(stderr, "Performing line %d: ", ct);
//
	int continuation=0;
	int param[5],pptr=0;
	int act[4];
	int cc=0;
	while(cc<5)
	{
		int cv,dv;
		cv=Actions[ct].Condition[cc];
		dv=cv/20;
		cv%=20;
//		fprintf(stderr, "Testing condition %d: ", cv);
		switch(cv)
		{
			case 0:
				param[pptr++]=dv;
				break;
			case 1:
//				fprintf(stderr, "Does the player carry %s?\n", Items[dv].Text);
				if(Items[dv].Location!=CARRIED)
					return(0);
				break;
			case 2:
//				fprintf(stderr, "Is %s in location?\n", Items[dv].Text);
				if(Items[dv].Location!=MyLoc)
					return(0);
				break;
			case 3:
//				fprintf(stderr, "Is %s held or in location?\n", Items[dv].Text);
				if(Items[dv].Location!=CARRIED&&
				   Items[dv].Location!=MyLoc)
					return(0);
				break;
			case 4:
//				fprintf(stderr, "Is location %s?\n", Rooms[dv].Text);
				if(MyLoc!=dv)
					return(0);
				break;
			case 5:
//				fprintf(stderr, "Is %s NOT in location?\n", Items[dv].Text);
				if(Items[dv].Location==MyLoc)
					return(0);
				break;
			case 6:
//				fprintf(stderr, "Does the player NOT carry %s?\n", Items[dv].Text);
				if(Items[dv].Location==CARRIED)
					return(0);
				break;
			case 7:
//				fprintf(stderr, "Is location NOT %s?\n", Rooms[dv].Text);
				if(MyLoc==dv)
					return(0);
				break;
			case 8:
//				fprintf(stderr, "Is bitflag %d set?\n", dv);
				if((BitFlags&(1<<dv))==0)
					return(0);
				break;
			case 9:
//				fprintf(stderr, "Is bitflag %d NOT set?\n", dv);
				if(BitFlags&(1<<dv))
					return(0);
				break;
			case 10:
//				fprintf(stderr, "Does the player carry anything?\n");
				if(CountCarried()==0)
					return(0);
				break;
			case 11:
//				fprintf(stderr, "Does the player carry nothing?\n");
				if(CountCarried())
					return(0);
				break;
			case 12:
//				fprintf(stderr, "Is %s neither carried nor in room?\n", Items[dv].Text);
				if(Items[dv].Location==CARRIED||Items[dv].Location==MyLoc)
					return(0);
				break;
			case 13:
//				fprintf(stderr, "Is %s (%d) in play?\n", Items[dv].Text, dv);
				if(Items[dv].Location==0)
					return(0);
				break;
			case 14:
//				fprintf(stderr, "Is %s NOT in play?\n", Items[dv].Text);
				if(Items[dv].Location)
					return(0);
				break;
			case 15:
//				fprintf(stderr, "Is CurrentCounter <= %d?\n", dv);
				if(CurrentCounter>dv)
					return(0);
				break;
			case 16:
//				fprintf(stderr, "Is CurrentCounter > %d?\n", dv);
				if(CurrentCounter<=dv)
					return(0);
				break;
			case 17:
//				fprintf(stderr, "Is %s still in initial room?\n", Items[dv].Text);
				if(Items[dv].Location!=Items[dv].InitialLoc)
					return(0);
				break;
			case 18:
//				fprintf(stderr, "Has %s been moved?\n", Items[dv].Text);
				if(Items[dv].Location==Items[dv].InitialLoc)
					return(0);
				break;
			case 19:/* Only seen in Brian Howarth games so far */
//				fprintf(stderr, "Is current counter == %d?\n", dv);
				if(CurrentCounter!=dv)
					return(0);
				break;
		}
//		fprintf(stderr, "YES\n");
		cc++;
	}

#pragma mark Actions

	/* Actions */
	act[0]=Actions[ct].Action[0];
	act[2]=Actions[ct].Action[1];
	act[1]=act[0]%150;
	act[3]=act[2]%150;
	act[0]/=150;
	act[2]/=150;
	cc=0;
	pptr=0;
	while(cc<4)
	{
//		fprintf(stderr, "Performing action %d:\n", act[cc]);

		if(act[cc]>=1 && act[cc]<52)
		{
//			fprintf(stderr, "Action: print message %d: \"%s\"\n", act[cc], Messages[act[cc]]);
			Output(Messages[act[cc]]);
			//            Output("\n");
			Output(" ");

		}
		else if(act[cc]>101)
		{
//			fprintf(stderr, "Action: print message %d: \"%s\"\n", act[cc]-50, Messages[act[cc]-50]);
			Output(Messages[act[cc]-50]);
			//            Output("\n");
			Output(" ");
		}
		else switch(act[cc])
		{
			case 0:/* NOP */
				break;
			case 52:
				if(CountCarried()==GameHeader.MaxCarry)
				{
					if(Options&YOUARE)
						Output("You are carrying too much. ");
					else
//						Output("I've too much to carry! ");
						Output("I'm carrying too much! ");
					break;
				}
				Items[param[pptr++]].Location= CARRIED;
				break;
			case 53:
				Items[param[pptr++]].Location=MyLoc;
				break;
			case 54:
				MyLoc=param[pptr++];
				break;
			case 55:
				Items[param[pptr++]].Location=0;
				break;
			case 56:
				BitFlags|=1<<DARKBIT;
				break;
			case 57:
				BitFlags&=~(1<<DARKBIT);
				break;
			case 58:
//				fprintf(stderr, "Action 58: Bitflag %d is set\n", param[pptr]);
				BitFlags|=(1<<param[pptr++]);
				break;
			case 59:
				Items[param[pptr++]].Location=0;
				break;
			case 60:
				BitFlags&=~(1<<param[pptr++]);
				break;
			case 61:
				if(Options&YOUARE)
					Output("You are dead.\n");
				else
//					Output("I am dead.\n");
					Output("I'm DEAD!! \n");
				BitFlags&=~(1<<DARKBIT);
				MyLoc=GameHeader.NumRooms;/* It seems to be what the code says! */
				Look();
				break;
			case 62:
			{
				/* Bug fix for some systems - before it could get parameters wrong */
				int i=param[pptr++];
				Items[i].Location=param[pptr++];
				break;
			}
			case 63:
			doneit:                Output("The game is now over.\n\nResume a saved game ?");
				glk_request_char_event(Bottom);

				event_t ev;
				do
				{
					glk_select(&ev);
				} while(ev.type != evtype_CharInput);

				if (ev.val1 == 'y' || ev.val1 == 'Y') {
					LoadGame();
					break;
				} else glk_exit();
			case 64:
				break;
			case 65:
			{
				int i=0;
				int n=0;
				while(i<=GameHeader.NumItems)
				{
					if(Items[i].Location==GameHeader.TreasureRoom &&
					   *Items[i].Text=='*')
						n++;
					i++;
				}
				if(Options&YOUARE)
					Output("You have stored ");
				else
					Output("I've stored ");
				OutputNumber(n);
				Output(" treasures.  On a scale of 0 to 100, that rates ");
				OutputNumber((n*100)/GameHeader.Treasures);
				Output(".\n");
				if(n==GameHeader.Treasures)
				{
					Output("Well done.\n");
					goto doneit;
				}
				break;
			}
			case 66:
			{
				int i=0;
				int f=0;
				if(Options&YOUARE)
					Output("You are carrying:\n");
				else
					Output("I'm carrying:\n");
				while(i<=GameHeader.NumItems)
				{
					if(Items[i].Location==CARRIED)
					{
						if(f==1)
						{
							if (Options & TRS80_STYLE)
								Output(". ");
							else
								Output(" - ");
						}
						f=1;
						Output(Items[i].Text);
					}
					i++;
				}
				if(f==0)
//					Output("Nothing");
					Output("Not a thing!");
				Output(".\n");
				break;
			}
			case 67:
				BitFlags|=(1<<0);
				break;
			case 68:
				BitFlags&=~(1<<0);
				break;
			case 69:
				GameHeader.LightTime=LightRefill;
				Items[LIGHT_SOURCE].Location=CARRIED;
				BitFlags&=~(1<<LIGHTOUTBIT);
				break;
			case 70:
				ClearScreen(); /* pdd. */
				break;
			case 71:
				SaveGame();
				break;
			case 72:
			{
				int i1=param[pptr++];
				int i2=param[pptr++];
				int t=Items[i1].Location;
				Items[i1].Location=Items[i2].Location;
				Items[i2].Location=t;
				break;
			}
			case 73:
//				fprintf(stderr, "Action 73: Continue with next line\n");
				continuation=1;
				break;
			case 74:
				Items[param[pptr++]].Location= CARRIED;
				break;
			case 75:
			{
				int i1,i2;
				i1=param[pptr++];
				i2=param[pptr++];
				Items[i1].Location=Items[i2].Location;
				break;
			}
			case 76:    /* Looking at adventure .. */
				break;
			case 77:
//				fprintf(stderr, "Performing action 77: decrementing current counter.\n");
				if(CurrentCounter>=0)
					CurrentCounter--;
//				fprintf(stderr, "Current counter is now %d.\n", CurrentCounter);
				break;
			case 78:
				OutputNumber(CurrentCounter);
				break;
			case 79:
				CurrentCounter=param[pptr++];
				break;
			case 80:
			{
				int t=MyLoc;
				MyLoc=SavedRoom;
				SavedRoom=t;
				break;
			}
			case 81:
			{
				/* This is somewhat guessed. Claymorgue always
				 seems to do select counter n, thing, select counter n,
				 but uses one value that always seems to exist. Trying
				 a few options I found this gave sane results on ageing */

//				fprintf(stderr, "Select a counter. Current counter is swapped with backup counter %d\n", param[pptr]);

				int t=param[pptr++];
				int c1=CurrentCounter;
				CurrentCounter=Counters[t];
				Counters[t]=c1;
//				fprintf(stderr, "Value of new selected counter is %d\n", CurrentCounter);
				break;
			}
			case 82:
				CurrentCounter+=param[pptr++];
				break;
			case 83:
				CurrentCounter-=param[pptr++];
				if(CurrentCounter< -1)
					CurrentCounter= -1;
				/* Note: This seems to be needed. I don't yet
				 know if there is a maximum value to limit too */
				break;
			case 84:
				Output(NounText);
				break;
			case 85:
				Output(NounText);
				Output("\n");
				break;
			case 86:
				Output("\n");
				break;
			case 87:
			{
				/* Changed this to swap location<->roomflag[x]
				 not roomflag 0 and x */
				int p=param[pptr++];
				int sr=MyLoc;
				MyLoc=RoomSaved[p];
				RoomSaved[p]=sr;
				break;
			}
			case 88:
				Delay(2);
				break;
			case 89:
			{
				/* SAGA draw picture n */
				/* Spectrum Seas of Blood - start combat ? */
				/* Poking this into older spectrum games causes a crash */

				DrawImage(68); /* Mogwai */

				Output("\n<HIT ENTER> ");

				glk_request_char_event(Bottom);

				event_t ev;
				do
				{
					glk_select(&ev);
				} while(ev.type != evtype_CharInput);

				Output("\n");
				break;
			}
			default:
				fprintf(stderr,"Unknown action %d [Param begins %d %d]\n",
						act[cc],param[pptr],param[pptr+1]);
				break;
		}
		cc++;
	}
	return(1+continuation);
}


static int PerformActions(int vb,int no)
{
	static int disable_sysfunc=0;    /* Recursion lock */
	int d=BitFlags&(1<<DARKBIT);

	int ct=0;
	int fl;
	int doagain=0;
#pragma mark GO
	if(vb==1 && no == -1 )
	{
//		Output("Give me a direction too.");
		Output("Direction ?");
		return(0);
	}
	if(vb==1 && no>=1 && no<=6)
	{
		int nl;
		if(Items[LIGHT_SOURCE].Location==MyLoc ||
		   Items[LIGHT_SOURCE].Location==CARRIED)
			d=0;
		if(d)
			Output("Dangerous to move in the dark! ");
		nl=Rooms[MyLoc].Exits[no-1];
		if(nl!=0)
		{
			MyLoc=nl;
			Output("O.K. ");
			return(0);
		}
		if(d)
		{
			if(Options&YOUARE)
				Output("You fell down and broke your neck. ");
			else
				Output("I fell down and broke my neck. ");
			glk_exit();
		}
		if(Options&YOUARE)
			Output("You can't go in that direction. ");
		else
//			Output("I can't go in that direction. ");
			Output("I can't go THAT way. ");
		return(0);
	}
	fl= -1;
	while(ct<=GameHeader.NumActions)
	{
		int vv,nv;
		vv=Actions[ct].Vocab;
		//        fprintf(stderr, "Action %d Vocab: %d\n", ct, vv);

		/* Think this is now right. If a line we run has an action73
		 run all following lines with vocab of 0,0 */
		if(vb!=0 && (doagain&&vv!=0))
			break;
		/* Oops.. added this minor cockup fix 1.11 */
		if(vb!=0 && !doagain && fl== 0)
			break;
		nv=vv%150;
		//        fprintf(stderr, "Action %d noun: %s\n", ct, Nouns[nv]);
		vv/=150;
		//        fprintf(stderr, "Action %d verb: %s\n", ct, Verbs[vv]);

		if((vv==vb)||(doagain&&Actions[ct].Vocab==0))
		{
			if((vv==0 && RandomPercent(nv))||doagain||
			   (vv!=0 && (nv==no||nv==0)))
			{
				int f2;
				if(fl== -1)
					fl= -2;
				if((f2=PerformLine(ct))>0)
				{
					/* ahah finally figured it out ! */
					fl=0;
					if(f2==2)
						doagain=1;
					if(vb!=0 && doagain==0)
						return(0);
				}
			}
		}
		ct++;

		/* Previously this did not check ct against
		 * GameHeader.NumActions and would read past the end of
		 * Actions.  I don't know what should happen on the last
		 * action, but doing nothing is better than reading one
		 * past the end.
		 * --Chris
		 */
		if(ct <= GameHeader.NumActions && Actions[ct].Vocab!=0)
			doagain=0;
	}
	if(fl!=0 && disable_sysfunc==0)
	{
		int item;
		if(Items[LIGHT_SOURCE].Location==MyLoc ||
		   Items[LIGHT_SOURCE].Location==CARRIED)
			d=0;

#pragma mark TAKE
		if(vb==10 || vb==18)
		{
			/* Yes they really _are_ hardcoded values */
			if(vb==10)
			{
				if(xstrcasecmp(NounText,"ALL")==0)
				{
					int i=0;
					int f=0;

					if(d)
					{
						Output("It is dark.\n");
						return 0;
					}
					while(i<=GameHeader.NumItems)
					{
						if(Items[i].Location==MyLoc && (Items[i].Flag & 128) == 0 && Items[i].AutoGet!=NULL && Items[i].AutoGet[0]!='*')
						{
							no=WhichWord(Items[i].AutoGet,Nouns);
							disable_sysfunc=1;    /* Don't recurse into auto get ! */
							PerformActions(vb,no);    /* Recursively check each items table code */
							disable_sysfunc=0;
							if(CountCarried()==GameHeader.MaxCarry)
							{
								if(Options&YOUARE)
									Output("You are carrying too much. ");
								else
//									Output("I've too much to carry. ");
									Output("I'm carrying too much! ");
								return(0);
							}
							Items[i].Location= CARRIED;
							Output(Items[i].Text);
//							Output(": O.K.\n");
							Output("....Taken.\n");
							f=1;
						}
						i++;
					}
					if(f==0)
						Output("Nothing taken.");
					return(0);
				}
				if(no==-1)
				{
					Output("What ? ");
					return(0);
				}
				if(CountCarried()==GameHeader.MaxCarry)
				{
					if(Options&YOUARE)
						Output("You are carrying too much. ");
					else
//						Output("I've too much to carry. ");
						Output("I'm carrying too much! ");

					return(0);
				}
				item=MatchUpItem(NounText,MyLoc);
				if(item==-1)
				{
					item=MatchUpItem(NounText,CARRIED);
					if(item==-1)
					{
						if(Options&YOUARE)
							Output("It is beyond your power to do that. ");
						else
							//Output("It's beyond my power to do that. ");
							Output("That's beyond my power. ");
					} else {
						Output("I have it. ");
					}
					return(0);

				}
				Items[item].Location= CARRIED;
//				Output("O.K. ");
				Output("Taken. ");
				return(0);
			}
#pragma mark DROP
			if(vb==18)
			{
				if(xstrcasecmp(NounText,"ALL")==0)
				{
					int i=0;
					int f=0;
					while(i<=GameHeader.NumItems)
					{
						if(Items[i].Location==CARRIED && Items[i].AutoGet && Items[i].AutoGet[0]!='*')
						{
							no=WhichWord(Items[i].AutoGet,Nouns);
							disable_sysfunc=1;
							PerformActions(vb,no);
							disable_sysfunc=0;
							Items[i].Location=MyLoc;
							Output(Items[i].Text);
//							Output(": O.K.\n");
							Output("....Dropped.\n");
							f=1;
						}
						i++;
					}
					if(f==0)
						Output("Nothing dropped.\n");
					return(0);
				}
				if(no==-1)
				{
					Output("What ? ");
					return(0);
				}
				item=MatchUpItem(NounText,CARRIED);
				if(item==-1)
				{
					if(Options&YOUARE)
						Output("It's beyond your power to do that.\n");
					else
						Output("It's beyond my power to do that.\n");

					return(0);
				}
				Items[item].Location=MyLoc;
//				Output("O.K. ");
				Output("Dropped. ");
				return(0);
			}
		}
	}
	return(fl);
}

glkunix_argumentlist_t glkunix_arguments[] =
{
	{ "-y",		glkunix_arg_NoValue,		"-y		Generate 'You are', 'You are carrying' type messages for games that use these instead (eg Robin Of Sherwood)" },
	{ "-i",		glkunix_arg_NoValue,		"-i		Generate 'I am' type messages (default)" },
	{ "-d",		glkunix_arg_NoValue,		"-d		Debugging info on load " },
	{ "-s",		glkunix_arg_NoValue,		"-s		Generate authentic Scott Adams driver light messages rather than other driver style ones (Light goes out in n turns..)" },
	{ "-t",		glkunix_arg_NoValue,		"-t		Generate TRS80 style display (terminal width is 64 characters; a line <-----------------> is displayed after the top stuff; objects have periods after them instead of hyphens" },
	{ "-p",		glkunix_arg_NoValue,		"-p		Use for prehistoric databases which don't use bit 16" },
	{ "-w",		glkunix_arg_NoValue,		"-w		Disable upper window" },
	{ "",		glkunix_arg_ValueFollows,	"filename	file to load" },

	{ NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
	int argc = data->argc;
	char **argv = data->argv;

	if(argc < 1)
		return 0;

	while(argv[1])
	{
		if(*argv[1]!='-')
			break;
		switch(argv[1][1])
		{
			case 'y':
				Options|=YOUARE;
				break;
			case 'i':
				Options&=~YOUARE;
				break;
			case 'd':
				Options|=DEBUGGING;
				break;
			case 's':
				Options|=SCOTTLIGHT;
				break;
			case 't':
				Options|=TRS80_STYLE;
				break;
			case 'p':
				Options|=PREHISTORIC_LAMP;
				break;
			case 'w':
				split_screen = 0;
				break;
		}
		argv++;
		argc--;
	}

#ifdef GARGLK
	garglk_set_program_name("ScottFree 1.14");
	garglk_set_program_info(
							"ScottFree 1.14 by Alan Cox\n"
							"Glk port by Chris Spiegel\n");
#endif

	if(argc==2)
	{
		//        game_file = argv[1];
		game_file = "/Users/administrator/Desktop/gremlins.sna";
//		game_file = "/Users/administrator/Downloads/Gremlins - The Adventure.tzx";

#ifdef GARGLK
		const char *s;
		if((s = strrchr(game_file, '/')) != NULL || (s = strrchr(game_file, '\\')) != NULL)
		{
			garglk_set_story_name(s + 1);
		}
		else
		{
			garglk_set_story_name(game_file);
		}
#endif
	}

	return 1;
}

void glk_main(void)
{
	FILE *f;
	int vb,no;

	Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
	if(Bottom == NULL)
		glk_exit();
	glk_set_window(Bottom);

	if(game_file == NULL)
		Fatal("No game provided");

	f = fopen(game_file, "r");
	if(f==NULL)
		Fatal("Cannot open game");

	Options|=TRS80_STYLE;

	if (Options & TRS80_STYLE)
	{
		Width = 64;
		TopHeight = 11;
	}
	else
	{
		Width = 80;
		TopHeight = 10;
	}

	if(split_screen)
	{
		Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, TopHeight, wintype_TextGrid, GLK_STATUS_ROCK);
		if(Top == NULL)
		{
			split_screen = 0;
			Top = Bottom;
		} else {
			glk_window_get_size(Top, &Width, NULL);
		}
	}
	else
	{
		Top = Bottom;
	}

	Output("\
Scott Free, A Scott Adams game driver in C.\n\
Release 1.14, (c) 1993,1994,1995 Swansea University Computer Society.\n\
Distributed under the GNU software license\n\n");
	LoadDatabase(f,(Options&DEBUGGING)?1:0);
	fclose(f);
#ifdef SPATTERLIGHT
	if (gli_determinism)
		srand(1234);
	else
#endif
		srand((unsigned int)time(NULL));
	while(1)
	{
		glk_tick();

		PerformActions(0,0);

		Look();

		if(GetInput(&vb,&no) == -1)
			continue;
		switch(PerformActions(vb,no))
		{
			case -1:Output("I don't understand you");
//			case -1:Output("I don't understand your command. ");
				break;
//			case -2:Output("I can't do that just now! ");
			case -2:Output("I can't do that yet. ");
				break;
		}
		/* Brian Howarth games seem to use -1 for forever */
		if(Items[LIGHT_SOURCE].Location/*==-1*/!=DESTROYED && GameHeader.LightTime!= -1)
		{
			GameHeader.LightTime--;
			if(GameHeader.LightTime<1)
			{
				BitFlags|=(1<<LIGHTOUTBIT);
				if(Items[LIGHT_SOURCE].Location==CARRIED ||
				   Items[LIGHT_SOURCE].Location==MyLoc)
				{
					if(Options&SCOTTLIGHT)
						Output("Light has run out! ");
					else
						Output("Your light has run out. ");
				}
				if(Options&PREHISTORIC_LAMP)
					Items[LIGHT_SOURCE].Location=DESTROYED;
			}
			else if(GameHeader.LightTime<25)
			{
				if(Items[LIGHT_SOURCE].Location==CARRIED ||
				   Items[LIGHT_SOURCE].Location==MyLoc)
				{

					if(Options&SCOTTLIGHT)
					{
						Output("Light runs out in ");
						OutputNumber(GameHeader.LightTime);
						Output(" turns. ");
					}
					else
					{
						if(GameHeader.LightTime%5==0)
							Output("Your light is growing dim. ");
					}
				}
			}
		}
	}
}
