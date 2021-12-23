/*
 *    ScottFree Revision 1.14
 *
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version
 *    2 of the License, or (at your option) any later version.
 *
 *
 *    You must have an ANSI C compiler to build this program.
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

//#ifdef __GNUC__
//__attribute__((__format__(__printf__, 2, 3)))
//#endif

extern int pixel_size;
extern int x_offset;

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
static int file_baseline_offset = 0;


static int split_screen = 1;
static winid_t Bottom, Top;
winid_t Graphics;

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

static void Delay(float seconds)
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
	*width = 255; *height = 96;
	int multiplier = 1;
	glui32 graphwidth, graphheight;
	glk_window_get_size(Graphics, &graphwidth, &graphheight);
	multiplier = graphheight / 96;
	if (255 * multiplier > graphwidth)
		multiplier = graphwidth / 255;

	if (multiplier == 0)
		multiplier = 1;

	*width = 255 * multiplier;
	*height = 96 * multiplier;

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
		x_offset = (graphwidth - optimal_width) / 2;
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

static long BitFlags=0;    /* Might be >32 flags - I haven't seen >32 yet */

static void Fatal(const char *x)
{
	Display(Bottom, "%s\n", x);
	glk_exit();
}

static void ClearScreen(void)
{
	glk_window_clear(Bottom);
}

void *MemAlloc(int size)
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
	static char lastword[16];    /* Last non synonym */
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
			if (charindex == 0) {
				if (c >= 'a')
				{
					c = toupper(c);
				} else {
					dictword[charindex++] = '*';
				}
			}


			dictword[charindex++] = c;

		}
		dictword[charindex] = 0;


		if (wordnum < 115)
		{
			Verbs[wordnum] = malloc(charindex+1);
			memcpy((char *)Verbs[wordnum], dictword, charindex+1);
//			fprintf(stderr, "Verb %d: \"%s\"\n", wordnum, Verbs[wordnum]);
		} else {
			Nouns[wordnum-115] = malloc(charindex+1);
			memcpy((char *)Nouns[wordnum-115], dictword, charindex+1);
//			fprintf(stderr, "Noun %d: \"%s\"\n", wordnum-115, Nouns[wordnum-115]);
		}
		wordnum++;
		//      }
		//        if (c != 0 && !isascii(c))
		//            return;
		charindex = 0;
	} while (wordnum < header[3] * 2 - 3);

	for (int i = 0; i < 5; i++) {
		Nouns[110 + i] = "\0";
//		fprintf(stderr, "Noun %d: \"%s\"\n", 110 + i, Nouns[110 + i]);
	}
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
	v = header[5]; // Number of rooms
	if (v < 10 || v > 255)
		return 0;

	return 1;
}

void print_header_info(void)
{
	uint16_t value;
	for (int i = 0; i < 12; i++)
	{
		value=header[i];
		fprintf(stderr, "b $%X %d: ", 0x3b5a + 0x3FE5 + i * 2, i);
		switch(i)
		{
			case 0 :
				fprintf(stderr, "???? =\t%x\n",value);
				break;

			case 1 :
				fprintf(stderr, "Number of items =\t%d\n",value);
				break;

			case 2 :
				fprintf(stderr, "Number of actions =\t%d\n",value);
				break;

			case 3 :
				fprintf(stderr, "Number of words =\t%d\n",value);
				break;

			case 4 :
				fprintf(stderr, "Number of rooms =\t%d\n",value);
				break;

			case 5 :
				fprintf(stderr, "Max carried items =\t%d\n",value);
				break;

			case 6 :
				fprintf(stderr, "Word length =\t%d\n",value);
				break;

			case 7 :
				fprintf(stderr, "Number of messages =\t%d\n",value);
				break;

			case 8 :
				fprintf(stderr, "???? =\t%x\n",value);
				break;

			case 9 :
				fprintf(stderr, "???? =\t%x\n",value);
				break;

			case 10 :
				fprintf(stderr, "???? =\t%x\n",value);
				break;

			case 11 :
				fprintf(stderr, "???? =\t%x\n",value);
				break;

			default :
				break;
		}
	}
	for (int i = 12; i < 36; i++)
	{
		value=header[i];
		fprintf(stderr, "; %d %d: ???? =\t%d\n", 0x3b5b + i * 2, i, value);
	}
}

void list_action_addresses(FILE *ptr) {
	int offset = 0x758B - 16357;
	int low_byte, high_byte;
	fseek(ptr,offset,SEEK_SET);
	for (int i = 52; i<102; i++) {
		low_byte = fgetc(ptr);
		high_byte = fgetc(ptr);
		fprintf(stderr, "Address of action %d: %x\n", i, low_byte + 256 * high_byte );
	}
}

void list_condition_addresses(FILE *ptr) {
	int offset = 0x73C2 - 16357;
	int low_byte, high_byte;
	fseek(ptr,offset,SEEK_SET);
	for (int i = 0; i<20; i++) {
		low_byte = fgetc(ptr);
		high_byte = fgetc(ptr);
		fprintf(stderr, "Address of condition %d: %x\n", i, low_byte + 256 * high_byte );
	}

}

static char *decompress_text(FILE *p, int offset, int index);


static void LoadDatabase(FILE *f, int loud)
{
	int ni,na,nw,nr,mc,pr,tr,wl,lt,mn,trm;
	int ct;
	//    short lo;
	Action *ap;
	Room *rp;
	Item *ip;
	loud = 1;
	/* Load the header */


	readheader(f);

	size_t file_length = get_file_length(f);

	size_t pos = 0x3b5a;
	fseek(f, pos, SEEK_SET);
	readheader(f);
	if (sanity_check_header() == 0) {
		pos = 0;

		while (sanity_check_header() == 0) {
			pos++;
			fseek(f, pos, SEEK_SET);
			readheader(f);
			if (pos >= file_length - 24) {
				fprintf(stderr, "found no valid header in file\n");
				exit(1);
			}
		}
	}

	fprintf(stderr, "Found a valid header at file offset %zx\n", pos);
	file_baseline_offset = pos - 0x3b5a;

	//    list_action_addresses(f);
	//    list_condition_addresses(f);

	fprintf(stderr, "Difference from baseline .sna file: %d\n", file_baseline_offset);

	print_header_info();

	ni = header[1];
	na = header[2];
	nw = header[3];
	nr = header[4];
	mc = header[5];
	wl = header[6];
	mn = header[7];
//	pr = 1;
//	pr = 46;
	pr = 86;
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

	int offset;

#pragma mark room images

//	offset = 0x3A09 + file_baseline_offset;
//	offset = 15462;
//	offset = 6181;
	offset = 15769 + file_baseline_offset;
	/* Load the room images */

jumpRoomImages:
	fseek(f,offset,SEEK_SET);

	rp=Rooms;

	for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
		rp->Image = fgetc(f);
		if ((ct == 1 && rp->Image != 2)  || (ct == 5 && rp->Image != 9) || (ct == 2 && rp->Image != 2)) {
			offset++;
			goto jumpRoomImages;
		}
		rp++;
		if (ct == 10) {
			for (int i = 0; i<63; i++) {
				rp++;
				ct++;
			}
		}
	}

	fprintf(stderr, "Found room images at offset %d\n", offset);
	fprintf(stderr, "Difference from expected 15769: %d\n", offset - 15769);
	fprintf(stderr, "Difference from expected 15769 + file_baseline_offset(%d) : %d\n", file_baseline_offset, offset - (15769 + file_baseline_offset));

	file_baseline_offset += offset - (15769 + file_baseline_offset);

#pragma mark Item flags

offset = 15800 + file_baseline_offset;

jumpItemFlags:
	fseek(f,offset,SEEK_SET);


//	(Items[ct].Flag & 127) == MyLoc)

	/* Load the item flags */

	ip=Items;

	for (ct = 0; ct <= GameHeader.NumItems; ct++) {
		ip->Flag = fgetc(f);
		if((ct == 1 && (ip->Flag & 127) != 8) || (ct == 23 && (ip->Flag & 127) != 7) || (ct == 18 && (ip->Flag & 127) != 4)) {
			offset++;
			goto jumpItemFlags;
		}
		ip++;
	}


	fprintf(stderr, "Found item flags at %d\n", offset);
	fprintf(stderr, "Difference from expected 15800: %d\n", offset - 15800);
	fprintf(stderr, "Difference from expected 15800 + file_baseline_offset(%d) : %d\n", file_baseline_offset, offset - (15800 + file_baseline_offset));



offset = 15888 + file_baseline_offset;

jumpItemImages:
	fseek(f,offset,SEEK_SET);

#pragma mark item images

	/* Load the item images */

	ip=Items;

	for (ct = 0; ct <= GameHeader.NumItems; ct++) {
		ip->Image = fgetc(f);
		if ((ct == 1 && ip->Image != 73) || (ct == 23 && ip->Image != 70) || (ct == 18 && ip->Image != 5)) {
			offset++;
			goto jumpItemImages;
		}
		ip++;
	}
	fprintf(stderr, "Found item images at %d\n", offset);



	offset = 16539 + file_baseline_offset;
jumpHere:

#pragma mark actions

	/* Load the actions */

	fseek(f,offset,SEEK_SET);
	ct=0;

	Actions=(Action *)malloc(sizeof(Action)*(na+1));
	ap=Actions;

	int value, cond, comm;
	while(ct<na+1)
	{
		value = fgetc(f) + 256 * fgetc(f); /* verb/noun */
		ap->Vocab = value;

		int verb = value;

		int noun=verb%150;
		//        fprintf(stderr, "Action %d noun: %s\n", ct, Nouns[nv]);
		verb/=150;

		if (noun < 0 || noun > nw || verb < 0 || verb > nw) {
			offset++;
			goto jumpHere;
		}

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

//	fprintf(stderr, "Found valid actions at offset %d\n", offset);

#pragma mark dictionary

	fseek(f,0x4dc3 + file_baseline_offset,SEEK_SET);

	read_dictionary(f);

//    for (int i = 0; i <= na; i++)
//    {
//        int vocab = Actions[i].Vocab;
//
//        int verb = vocab;
//
//        int noun=verb%150;
//
//        verb/=150;
//
//        fprintf(stderr, "Action %d verb: %s (%d) noun: %s (%d)\n", i,  Verbs[verb], verb, Nouns[noun], noun);
//    }

#pragma mark rooms


	ct=1;
	rp=Rooms;
	if(loud)
		fprintf(stderr, "Reading %d rooms.\n",nr);

	//    char c=0;

	rp->Text = malloc(3);
	strcpy(rp->Text, ". \0");
    fprintf(stderr, "Room 0 : \"%s\"\n", rp->Text);
	rp++;

	int actual_room_number = 1;

	do {
		rp->Text = decompress_text(f, 0x9B53 + file_baseline_offset, ct);
		fprintf(stderr, "Room %d : \"%s\"\n", actual_room_number, Rooms[actual_room_number].Text);
		*(rp->Text) = tolower(*(rp->Text));
		ct++;
		actual_room_number++;
		if (ct == 11) {
			for (int i = 0; i<61; i++) {
				rp++;
				rp->Text = "in Sherwood Forest";
//                fprintf(stderr, "Room %d : \"%s\"\n", actual_room_number, Rooms[actual_room_number].Text);
				actual_room_number++;
			}
		}
		rp++;
//	} while (ct<nr+1);
	} while (ct<33);

	offset = 0x3e67 + file_baseline_offset;

try_again:

#pragma mark room connections
	fseek(f,offset,SEEK_SET);

	ct=0;
	rp=Rooms;
	//    if(loud)

	while(ct<nr+1)
	{
		for (int j= 0; j < 6; j++) {
			rp->Exits[j] = fgetc(f);
			if (rp->Exits[j] < 0 || rp->Exits[j] > nr || (ct == 2 && j == 5 && rp->Exits[j] != 1) || (ct == 1 && j == 3 && rp->Exits[j] != 0)) {
				offset++;
				goto try_again;
			}

		}
		ct++;
		rp++;
	}

	//    fprintf(stderr, "Reading %d X 6 room connections, total of %d bytes.\n",nr, nr * 6);
//    fprintf(stderr, "Found room connections at offset %d.\n", offset);
//
//	static char *ExitNames[6]=
//	{
//		//        "North","South","East","West","Up","Down"
//		"NORTH","SOUTH","EAST","WEST","UP","DOWN"
//
//	};
//
//        for (int i = 0; i<= nr; i++) {
//
//		fprintf(stderr, "Exits from room %d, %s:\n",i, Rooms[i].Text);
//
//
//		for (int j = 0; j< 6; j++) {
//
//			if((&Rooms[i])
//			   ->Exits[j]!=0)
//			{
//				fprintf(stderr, " %s to %s,",ExitNames[j], Rooms[(&Rooms[i])
//																 ->Exits[j]].Text);
//			}
//		}
//			fprintf(stderr, "\n");
//	}
//
//


#pragma mark messages

	ct=1;
	if(loud)
		fprintf(stderr, "Reading %d messages.\n",mn);

	Messages[0] = malloc(3);
	strcpy((char *)Messages[0], ". \0");
//    fprintf(stderr, "Message 0 : \"%s\"\n", Messages[0] );
	while(ct<mn+1)
	{
		Messages[ct] = decompress_text(f, 0x912c + file_baseline_offset, ct);
        fprintf(stderr, "Message %d : \"%s\"\n", ct, Messages[ct]);
		ct++;
	}

#pragma mark items

	ct=0;
	if(loud)
		fprintf(stderr, "Reading %d items.\n",ni);
	ip=Items;

	do {
		ip->Text = decompress_text(f, 0x9B66 + file_baseline_offset, ct+32);

		ip->AutoGet=strchr(ip->Text,'.');
		//            /* Some games use // to mean no auto get/drop word! */
		if(ip->AutoGet)
		{
			char *t;
			*ip->AutoGet++=0;
			ip->AutoGet++;
			t=strchr(ip->AutoGet,'.');
			if(t!=NULL)
				*t=0;
			for (int i = 1; i < 4; i++)
				*(ip->AutoGet+i) = toupper(*(ip->AutoGet+i));
//            fprintf(stderr, "Autoget: \"%s\"\n", ip->AutoGet);
		}

		ct++;
		ip++;
	} while(ct<ni+1);

#pragma mark item locations

	offset = 0x4d6b + file_baseline_offset;
jumpItemLoc:

    fseek(f,offset,SEEK_SET);

	//    0x6493
	ct=0;
	ip=Items;
	while(ct<ni+1)
	{
		ip->Location=fgetc(f);
		if ((ct == 11 && ip->Location != 1) || (ct == 40 && ip->Location != 79)) {
			offset++;
			goto jumpItemLoc;
		}

		ip->InitialLoc=ip->Location;
		ip++;
		ct++;
	}

	fprintf(stderr, "Found item locations at offset: %d\n", offset);
	fprintf(stderr, "Difference from expected 0x4d6b: %d\n", offset - 0x4d6b);
	fprintf(stderr, "Difference from expected 0x4d6b + file_baseline_offset(%d) : %d\n", file_baseline_offset, offset - (0x4d6b + file_baseline_offset));


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
//	fprintf(stderr, "DrawImage %d\n", image);
	draw_saga_picture_number(image);
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
		//        "North","South","East","West","Up","Down"
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
	//    if(f==0)
	//        Display(Top,"none");
	if(f!=0) {
		//        Display(Top,".\n");
		//        Display(Top,"\n");
		return 2;
	}
	return 0;
}

int rotate_left_with_carry(uint8_t *byte, int last_carry)
{
	int carry = ((*byte & 0x80) > 0);
	*byte = *byte << 1;
	if (last_carry)
		*byte = *byte | 0x01;
	return carry;
}

int rotate_right_with_carry(uint8_t *byte, int last_carry)
{
	int carry = ((*byte & 0x01) > 0);
	*byte = *byte >> 1;
	if (last_carry)
		*byte = *byte | 0x80;
	return carry;
}

int decompress_one(uint8_t *bytes) {
    uint8_t result = 0;
	int carry;
	for (int i = 0; i < 5; i++)
	{
		carry = 0;
		for (int j = 0; j < 5; j++)
		{
			carry = rotate_left_with_carry(bytes+4-j, carry);
		}
        rotate_left_with_carry(&result, carry);
	}
    return result;
}


static char *decompressed(uint8_t *source, int stringindex)
{
	// Lookup table
	const char *alphabet = " abcdefghijklmnopqrstuvwxyz'\x01,.\x00";

	int pos, c, uppercase, i, j;
	uint8_t decompressed[256];
	uint8_t buffer[5];
	int idx = 0;

	// Find the start of the compressed message
	for(int i = 0; i < stringindex; i++) {
		pos = *source;
		pos = pos & 0x7F;
		source += pos;
	};

	uppercase = ((*source & 0x40) == 0); // Test bit 6

	source++;
	do {
		// Get five compressed bytes
		for (i = 0; i < 5; i++) {
			buffer[i] = *source++;
		}
		for (j = 0; j < 8; j++) {
			// Decompress one character:
			int next = decompress_one(buffer);

			c = alphabet[next];

			if (c == 0x01) {
				uppercase = 1;
				c = ' ';
			}

			if (c >= 'a' && uppercase) {
				c = toupper(c);
				uppercase = 0;
			}
			decompressed[idx++] = c;

			if (c == 0) {
				char *result = malloc(idx);
				memcpy(result, decompressed, idx);
				return result;
			} else if (c == '.' || c == ',') {
				if (c == '.')
					uppercase = 1;
				decompressed[idx++] = ' ';
			}
		}
	} while (idx < 0xff);
	return NULL;
}

static char *decompress_text(FILE *p, int offset, int index)
{
	size_t length = get_file_length(p);
	uint8_t *buf = (uint8_t *)malloc(length);
	fseek(p, 0, SEEK_SET);
	fread(buf, sizeof *buf, length, p);

	//    offset = 0x912c; // Messages
	//    offset = 0x9B53; // Room descriptions
	//    offset = 0x9B66; // Item descriptions

	uint8_t *mem = (uint8_t *)malloc(0xFFFF);
	memset(mem, 0, 0xffff);
	for (int i = 0; i<length; i++) {
		mem[i+16357] = buf[i];
	}

	char *text = decompressed(mem+offset, index);
	free(mem);
	return text;
}

int is_forest_location(void) {
	return (MyLoc >= 11 && MyLoc <=73);
}

static void robin_of_sherwood_look(void) {
	if (MyLoc == 82) // Dummy room where exit from Up a tree goes
		MyLoc = RoomSaved[0];
	if (MyLoc == 93) // Up a tree
		for (int i = 0; i < GameHeader.NumItems; i++)
			if (Items[i].Location == 93)
				Items[i].Location = RoomSaved[0];
	if (is_forest_location()) {
		open_graphics_window();
		DrawImage(0);
		draw_sherwood(MyLoc);

		if (Items[36].Location == MyLoc) {
			//"Gregory the tax collector with his horse and cart"
			DrawImage(15); // Horse and cart
			DrawImage(3); // Sacks of grain
		}
		if (Items[60].Location == MyLoc) {
			// "A serf driving a horse and cart"
			DrawImage(15); // Horse and cart
			DrawImage(12); // Hay
		}
		if (MyLoc == 73) {
			// Outlaws camp
			DrawImage(36); // Campfire
		}
	}

	if (MyLoc == 86 || MyLoc == 79) {
		glk_request_timer_events(15);
	}
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
				DrawImage(Rooms[MyLoc].Image);
		} else {
			close_graphics_window();
		}
	}

	robin_of_sherwood_look();

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
		//        Display(Top,"%s\n",r->Text+1);
		Display(Top,"%s",r->Text+1);
	else
	{
		if(Options&YOUARE) {
//			Display(Top,"You are in a %s\n",r->Text);
//			Display(Top,"You are %s\n",r->Text);
			Display(Top,"You are %s",r->Text);
		} else
			Display(Top,"I'm in a %s",r->Text);
	}
	//    ct=0;
	//    f=0;
	//    while(ct<6)
	//    {
	//        if(r->Exits[ct]!=0)
	//        {
	//            if(f==0)
	//            {
	//                Display(Top,"\nObvious exits: ");
	//                f=1;
	//            }
	//            else
	//                Display(Top,", ");
	//            Display(Top,"%s",ExitNames[ct]);
	//        }
	//        ct++;
	//    }
	////    if(f==0)
	////        Display(Top,"none");
	//    if(f!=0) {
	//    Display(Top,".\n");
	//    ypos++;
	//    }
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
//					Display(Top,"\nYou can also see: ");
//					xpos=18;
					Display(Top,". You see:\n\n");
					xpos=0;
					ypos=2;
				}
				else
				{
					//                    Display(Top,"\nI can also see: ");
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
				xpos++;
			}
			if (split_screen && Items[ct].Image) {
				if ((Items[ct].Flag & 127) == MyLoc) {
					DrawImage(Items[ct].Image);
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

void drawstuff(void);

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
		else if(ev.type == evtype_Timer) {
			AnimationFlag++;
			if (AnimationFlag > 63)
				AnimationFlag = 0;
			if (split_screen && (MyLoc == 86 || MyLoc == 79 || MyLoc == 84)) {
				if (MyLoc == 86) {
					animate_waterfall(AnimationFlag);
				} else if (MyLoc == 79) {
					animate_waterfall_cave(AnimationFlag);
				} else {
					animate_lightning(AnimationFlag);
				}
			} else {
				glk_request_timer_events(0);
			}
		}
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
			//            Output("\nTell me what to do ? ");
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
			//            Output("You use word(s) I don't know! ");
			Output("\"");
			Output(verb);
			Output("\" is not a word I know. ");
		}
	}
	while(vc==-1);
	strcpy(NounText,noun);    /* Needed by GET/DROP hack */

	return 0;
}

static void robin_of_sherwood_action(int p)
{
	switch (p) {
		case 1:
			Look();
			if (MyLoc == 14)
				Delay(0.4);
			Items[39].Location = MyLoc;
			Look();
			DrawImage(0); /* Herne */

			Output("<HIT ENTER> ");

			glk_request_char_event(Bottom);
			event_t ev;
			do
			{
				glk_select(&ev);
			} while(ev.type != evtype_CharInput);

			Items[39].Location = 79;
			Look();
			break;
		case 2:
			RoomSaved[0] = MyLoc;
			MyLoc = 93;
			Look();
			break;
		case 3:
			// Flash animation
			AnimationFlag = 1;
			glk_request_timer_events(15);
			break;
		default:
			fprintf(stderr, "Unhandled special action!\n");
			break;
	}
}

static int PerformLine(int ct)
{
	//    fprintf(stderr, "Performing line %d: ", ct);
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
				//                fprintf(stderr, "Does the player carry %s?\n", Items[dv].Text);
				if(Items[dv].Location!=CARRIED)
					return(0);
				break;
			case 2:
				//                fprintf(stderr, "Is %s in location?\n", Items[dv].Text);
				if(Items[dv].Location!=MyLoc)
					return(0);
				break;
			case 3:
				//                fprintf(stderr, "Is %s held or in location?\n", Items[dv].Text);
				if(Items[dv].Location!=CARRIED&&
				   Items[dv].Location!=MyLoc)
					return(0);
				break;
			case 4:
				//                fprintf(stderr, "Is location %s?\n", Rooms[dv].Text);
				if(MyLoc!=dv)
					return(0);
				break;
			case 5:
				//                fprintf(stderr, "Is %s NOT in location?\n", Items[dv].Text);
				if(Items[dv].Location==MyLoc)
					return(0);
				break;
			case 6:
				//                fprintf(stderr, "Does the player NOT carry %s?\n", Items[dv].Text);
				if(Items[dv].Location==CARRIED)
					return(0);
				break;
			case 7:
				//                fprintf(stderr, "Is location NOT %s?\n", Rooms[dv].Text);
				if(MyLoc==dv)
					return(0);
				break;
			case 8:
				//                fprintf(stderr, "Is bitflag %d set?\n", dv);
				if((BitFlags&(1<<dv))==0)
					return(0);
				break;
			case 9:
				//                fprintf(stderr, "Is bitflag %d NOT set?\n", dv);
				if(BitFlags&(1<<dv))
					return(0);
				break;
			case 10:
				//                fprintf(stderr, "Does the player carry anything?\n");
				if(CountCarried()==0)
					return(0);
				break;
			case 11:
				//                fprintf(stderr, "Does the player carry nothing?\n");
				if(CountCarried())
					return(0);
				break;
			case 12:
				//                fprintf(stderr, "Is %s neither carried nor in room?\n", Items[dv].Text);
				if(Items[dv].Location==CARRIED||Items[dv].Location==MyLoc)
					return(0);
				break;
			case 13:
				//                fprintf(stderr, "Is %s (%d) in play?\n", Items[dv].Text, dv);
				if(Items[dv].Location==0)
					return(0);
				break;
			case 14:
				//                fprintf(stderr, "Is %s NOT in play?\n", Items[dv].Text);
				if(Items[dv].Location)
					return(0);
				break;
			case 15:
				//                fprintf(stderr, "Is CurrentCounter <= %d?\n", dv);
				if(CurrentCounter>dv)
					return(0);
				break;
			case 16:
				//                fprintf(stderr, "Is CurrentCounter > %d?\n", dv);
				if(CurrentCounter<=dv)
					return(0);
				break;
			case 17:
				//                fprintf(stderr, "Is %s still in initial room?\n", Items[dv].Text);
				if(Items[dv].Location!=Items[dv].InitialLoc)
					return(0);
				break;
			case 18:
				//                fprintf(stderr, "Has %s been moved?\n", Items[dv].Text);
				if(Items[dv].Location==Items[dv].InitialLoc)
					return(0);
				break;
			case 19:/* Only seen in Brian Howarth games so far */
				//                fprintf(stderr, "Is current counter == %d?\n", dv);
				if(CurrentCounter!=dv)
					return(0);
				break;
		}
		//        fprintf(stderr, "YES\n");
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
//			            fprintf(stderr, "Action: print message %d: \"%s\"\n", act[cc], Messages[act[cc]]);
			Output(Messages[act[cc]]);
			//            Output("\n");
			Output(". ");

		}
		else if(act[cc]>101)
		{
			//            fprintf(stderr, "Action: print message %d: \"%s\"\n", act[cc]-50, Messages[act[cc]-50]);
			Output(Messages[act[cc]-50]);
			//            Output("\n");
			Output(". ");
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
						//                        Output("I've too much to carry! ");
						Output("I'm carrying too much! ");
					break;
				}
				Items[param[pptr++]].Location= CARRIED;
				break;
			case 53:
				Items[param[pptr++]].Location=MyLoc;
				break;
			case 54:
//				fprintf(stderr, "Action 54: player location is now room %d (%s).\n", param[pptr], Rooms[param[pptr]].Text);
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
				//                fprintf(stderr, "Action 58: Bitflag %d is set\n", param[pptr]);
				BitFlags|=(1<<param[pptr++]);
				break;
			case 59:
//				fprintf(stderr, "Action 59: Item %d (%s) is removed from play.\n", param[pptr], Items[param[pptr]].Text);
				Items[param[pptr++]].Location=0;
				break;
			case 60:
//				fprintf(stderr, "Action 60: BitFlag %d is cleared\n", param[pptr]);
				BitFlags&=~(1<<param[pptr++]);
				break;
			case 61:
				if(Options&YOUARE)
//					Output("You are dead.\n");
					Output("Your reign of the Sherwood Forest is over.\n");

				else
					//                    Output("I am dead.\n");
					Output("I'm DEAD!! \n");
				BitFlags&=~(1<<DARKBIT);
				MyLoc=GameHeader.NumRooms;/* It seems to be what the code says! */
				Look();
				break;
			case 62:
			{
				/* Bug fix for some systems - before it could get parameters wrong */
				int i=param[pptr++];
//				fprintf(stderr, "Action 62: Item %d (%s) is put in room %d (%s).\n", i, Items[i].Text, param[pptr], Rooms[param[pptr]].Text);
				Items[i].Location=param[pptr++];
				break;
			}
			case 63:
			doneit:
				Look();
				Output("\n\nResume a saved game ?");
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
					Output("You are carrying: ");
				else
					Output("I'm carrying:\n");
				while(i<=GameHeader.NumItems)
				{
					if(Items[i].Location==CARRIED)
					{
						if(f==1 && (Options & TRS80_STYLE) == 0)
						{
								Output(" - ");
						}
						f=1;
						Output(Items[i].Text);
						if(Options & TRS80_STYLE)
						{
							Output(". ");
						}
					}
					i++;
				}
				if(f==0)
					                   Output("Nothing!");
//					Output("Not a thing!");
//				Output("\n");
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
//				fprintf(stderr, "CurrentCounter is set to %d.\n", param[pptr]);
				CurrentCounter=param[pptr++];
				break;
			case 80:
			{
//				fprintf(stderr, "Performing action 80: switch location to saved location.\n");
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

				int p=param[pptr];
//				fprintf(stderr, "Drawing special image, action 89. Param %d\n", p);

				robin_of_sherwood_action(p);
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
		//        Output("Give me a direction too.");
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
			//            Output("I can't go in that direction. ");
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
									//                                    Output("I've too much to carry. ");
									Output("I'm carrying too much! ");
								return(0);
							}
							Items[i].Location= CARRIED;
							Output(Items[i].Text);
							//                            Output(": O.K.\n");
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
						//                        Output("I've too much to carry. ");
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
				//                Output("O.K. ");
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
							//                            Output(": O.K.\n");
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
				//                Output("O.K. ");
				Output("Dropped. ");
				return(0);
			}
		}
	}
	return(fl);
}

glkunix_argumentlist_t glkunix_arguments[] =
{
	{ "-y",        glkunix_arg_NoValue,        "-y        Generate 'You are', 'You are carrying' type messages for games that use these instead (eg Robin Of Sherwood)" },
	{ "-i",        glkunix_arg_NoValue,        "-i        Generate 'I am' type messages (default)" },
	{ "-d",        glkunix_arg_NoValue,        "-d        Debugging info on load " },
	{ "-s",        glkunix_arg_NoValue,        "-s        Generate authentic Scott Adams driver light messages rather than other driver style ones (Light goes out in n turns..)" },
	{ "-t",        glkunix_arg_NoValue,        "-t        Generate TRS80 style display (terminal width is 64 characters; a line <-----------------> is displayed after the top stuff; objects have periods after them instead of hyphens" },
	{ "-p",        glkunix_arg_NoValue,        "-p        Use for prehistoric databases which don't use bit 16" },
	{ "-w",        glkunix_arg_NoValue,        "-w        Disable upper window" },
	{ "",        glkunix_arg_ValueFollows,    "filename    file to load" },

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
		game_file = "/Users/administrator/Desktop/sherwood.sna";
		//        game_file = "/Users/administrator/Downloads/Gremlins - The Adventure.tzx";
//		game_file = "/Users/administrator/Downloads/Robin Of Sherwood - Alternate.tzx";
		Options|=YOUARE;
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

//	Output("\
//Scott Free, A Scott Adams game driver in C.\n\
//Release 1.14, (c) 1993,1994,1995 Swansea University Computer Society.\n\
//Distributed under the GNU software license\n\n");
	LoadDatabase(f,(Options&DEBUGGING)?1:0);
	fclose(f);
	if(split_screen) {
		open_graphics_window();
		saga_setup(game_file, "o3", file_baseline_offset, "zxopt");
		close_graphics_window();
	}
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
			case -1:Output("I don't understand");
				//            case -1:Output("I don't understand your command. ");
				break;
				//            case -2:Output("I can't do that just now! ");
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
