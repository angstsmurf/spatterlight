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

#include "detectgame.h"
#include "restorestate.h"
#include "sagadraw.h"
#include "layouttext.h"

#include "parser.h"

#include "robinofsherwood.h"
#include "seasofblood.h"
#include "gremlins.h"
#include "hulk.h"
#include "game_specific.h"

#include "bsd.h"
#include "scott.h"

//#include "gameinfo.h"


#ifdef SPATTERLIGHT
extern glui32 gli_determinism;
#endif

static const char *game_file;

//#ifdef __GNUC__
//__attribute__((__format__(__printf__, 2, 3)))
//#endif

extern int pixel_size;
extern int x_offset;

Header GameHeader;
Item *Items;
Room *Rooms;
const char **Verbs;
const char **Nouns;
const char **Messages;
Action *Actions;
int LightRefill;
//static char NounText[16];
int Counters[16];    /* Range unknown */
int CurrentCounter;
int SavedRoom;
int RoomSaved[16];    /* Range unknown */
static int Options;        /* Option flags set */
static glui32 Width;        /* Terminal width */
static glui32 TopHeight;        /* Height of top window */
int file_baseline_offset = 0;

struct Command *CurrentCommand = NULL;

struct GameInfo *GameInfo;

extern char *sysdict[MAX_SYSMESS];
extern char *sysdict_i_am[MAX_SYSMESS];

char *sys[MAX_SYSMESS];
char *system_messages[60];

const char *battle_messages[33];
uint8_t enemy_table[126];

uint8_t *entire_file;
size_t file_length;

int AnimationFlag = 0;

extern struct SavedState *initial_state;

int just_started = 1;
static int before_first_turn = 1;
static int moves_this_turn = 0;
int stop_time = 0;
/* The dead flag is used to prevent undo states */
/* from being created *after* victory or death */
int dead = 0;

int pause_next_room_description = 0;

int split_screen = 1;
winid_t Bottom, Top;
winid_t Graphics;

strid_t Transcript = NULL;

#define GLK_BUFFER_ROCK 1
#define GLK_STATUS_ROCK 1010
#define GLK_GRAPHICS_ROCK 1020

#define TRS80_LINE    "\n<------------------------------------------------------------>\n"

static void RestartGame(void);
static int YesOrNo(void);

static int PerformActions(int vb,int no);


void Display(winid_t w, const char *fmt, ...) {
	va_list ap;
	char msg[2048];

	int size = sizeof msg;

	va_start(ap, fmt);
	vsnprintf(msg, size, fmt, ap);
	va_end(ap);

	glui32 *unistring = ToUnicode(msg);
	glk_put_string_stream_uni(glk_window_get_stream(w), unistring);
	if (Transcript)
		glk_put_string_stream_uni(Transcript, unistring);
	free(unistring);
}

void Updates(event_t ev) {
	if(ev.type == evtype_Arrange) {
		if (Graphics) {
			CloseGraphicsWindow();
			OpenGraphicsWindow();
		}
		if (split_screen) {
			Look();
		}
	} else if(ev.type == evtype_Timer) {
		if (CurrentGame == ROBIN_OF_SHERWOOD)
			update_robin_of_sherwood_animations();
		else if (GameInfo->type == GREMLINS_VARIANT)
			update_gremlins_animations();
	}
}

void Delay(float seconds) {
	event_t ev;

	if(!glk_gestalt(gestalt_Timer, 0))
		return;

	glk_request_char_event(Bottom);
	glk_cancel_char_event(Bottom);

	glk_request_timer_events(1000 * seconds);

	do
	{
		glk_select(&ev);
		Updates(ev);
	} while(ev.type != evtype_Timer);

	glk_request_timer_events(0);
}


winid_t FindGlkWindowWithRock(glui32 rock)
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

const glui32 OptimalPictureSize(glui32 *width, glui32 *height)
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

void OpenTopWindow(void) {
	Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
	if (Top == NULL) {
		if(split_screen) {
			Top = glk_window_open(Bottom,
								  winmethod_Above | winmethod_Fixed,
								  TopHeight,
								  wintype_TextGrid, GLK_STATUS_ROCK);
			if(Top == NULL) {
				split_screen = 0;
				Top = Bottom;
			} else {
				glk_window_get_size(Top, &Width, NULL);
			}
		}
		else {
			Top = Bottom;
		}
	}
}

void OpenGraphicsWindow(void)
{
	glui32 graphwidth, graphheight, optimal_width, optimal_height;

	if (Top == NULL)
		Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
	if (Graphics == NULL)
		Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
	if (Graphics == NULL && Top != NULL) {
		glk_window_get_size(Top, &Width, &TopHeight);
		glk_window_close(Top, NULL);
		Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional, 60, wintype_Graphics, GLK_GRAPHICS_ROCK);
		glk_window_get_size(Graphics, &graphwidth, &graphheight);
		pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
		x_offset = ((int)graphwidth - (int)optimal_width) / 2;

		if (graphheight > optimal_height) {
			winid_t parent = glk_window_get_parent(Graphics);
			glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed, optimal_height, NULL);
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
		if (!Graphics)
			Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional, 60, wintype_Graphics, GLK_GRAPHICS_ROCK);
		glk_window_get_size(Graphics, &graphwidth, &graphheight);
		pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
		x_offset = (graphwidth - optimal_width) / 2;
		winid_t parent = glk_window_get_parent(Graphics);
		glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed, optimal_height, NULL);
	}
}

void CloseGraphicsWindow(void) {
	if (Graphics == NULL)
		Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
	if (Graphics) {
		glk_window_close(Graphics, NULL);
		Graphics = NULL;
		glk_window_get_size(Top, &Width, &TopHeight);
	}
}

long BitFlags=0;    /* Might be >32 flags - I haven't seen >32 yet */

void Fatal(const char *x)
{
	Display(Bottom, "%s\n", x);
	if (Transcript)
		glk_stream_close(Transcript, NULL);
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
	rv %=100;
//	uint rv = rand() % 100;
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

const char *MapSynonym(int noun)
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
		if (n == noun)
			return(lastword);
		n++;
	}
	return(NULL);
}

static int MatchUpItem(int noun, int loc)
{
	const char *word=MapSynonym(noun);
	int ct=0;

	if(word==NULL)
		word=Nouns[noun];

	while(ct<=GameHeader.NumItems)
	{
		if(Items[ct].AutoGet && (loc == 0 || Items[ct].Location==loc) &&
		   xstrncasecmp(Items[ct].AutoGet,word,GameHeader.WordLength)==0)
			return(ct);
		ct++;
	}
	return(-1);
}

static char *ReadString(FILE *f) {
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

size_t GetFileLength(FILE *in) {
	if (fseek(in, 0, SEEK_END) == -1) {
		return 0;
	}
	size_t length = ftell(in);
	if (length == -1) {
		return 0;
	}
	return length;
}

int header[24];

int SanityCheckScottFreeHeader(int ni, int na, int nw, int nr, int mc) {

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
	v = header[5]; // Number of Messages
	if (v < 10 || v > 255)
		return 0;

	return 1;
}

void FreeDatabase(void) {
	free(Items);
	free(Actions);
	free(Verbs);
	free(Nouns);
	free(Rooms);
	free(Messages);
}

int LoadDatabase(FILE *f, int loud)
{
	int ni,na,nw,nr,mc,pr,tr,wl,lt,mn,trm;
	int ct;
	short lo;
	Action *ap;
	Room *rp;
	Item *ip;
	/* Load the header */

	if(fscanf(f,"%*d %d %d %d %d %d %d %d %d %d %d %d",
			  &ni,&na,&nw,&nr,&mc,&pr,&tr,&wl,&lt,&mn,&trm)<10) {
		fprintf(stderr, "Invalid database(bad header)\n");
		return 0;
	}
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

	if (loud) {
		fprintf(stderr, "Number of items: %d\n", GameHeader.NumItems);
		fprintf(stderr, "Number of actions: %d\n", GameHeader.NumActions);
		fprintf(stderr, "Number of words: %d\n", GameHeader.NumWords);
		fprintf(stderr, "Word length: %d\n", GameHeader.WordLength);
		fprintf(stderr, "Number of rooms: %d\n", GameHeader.NumRooms);
		fprintf(stderr, "Number of messages: %d\n", GameHeader.NumMessages);
		fprintf(stderr, "Max carried: %d\n", GameHeader.MaxCarry);
		fprintf(stderr, "Starting location: %d\n", GameHeader.PlayerRoom);
		fprintf(stderr, "Light time: %d\n", GameHeader.LightTime);
		fprintf(stderr, "Number of treasures: %d\n", GameHeader.Treasures);
		fprintf(stderr, "Treasure room: %d\n", GameHeader.TreasureRoom);
	}

	/* Load the actions */

	ct=0;
	ap=Actions;
	if(loud)
		fprintf(stderr, "Reading %d actions.\n",na);
	while(ct<na+1)
	{
		if(fscanf(f,"%hu %hu %hu %hu %hu %hu %hu %hu",
				  &ap->Vocab,
				  &ap->Condition[0],
				  &ap->Condition[1],
				  &ap->Condition[2],
				  &ap->Condition[3],
				  &ap->Condition[4],
				  &ap->Action[0],
				  &ap->Action[1])!=8)
		{
			fprintf(stderr, "Bad action line (%d)\n",ct);
			FreeDatabase();
			return 0;
		}

		if (loud) {
			fprintf(stderr, "Action %d Vocab: %d\n",ct, ap->Vocab);
			fprintf(stderr, "Action %d Condition[0]: %d\n",ct, ap->Condition[0]);
			fprintf(stderr, "Action %d Action[0]: %d\n",ct, ap->Action[0]);
			fprintf(stderr, "Action %d Action[1]: %d\n",ct, ap->Action[1]);
		}

		ap++;
		ct++;
	}

	ct=0;
	if(loud)
		fprintf(stderr, "Reading %d word pairs.\n",nw);
	while(ct<nw+1)
	{
		Verbs[ct]=ReadString(f);
		Nouns[ct]=ReadString(f);
		ct++;
	}
	ct=0;
	rp=Rooms;
	if(loud)
		fprintf(stderr, "Reading %d rooms.\n",nr);
	while(ct<nr+1)
	{
		if(fscanf(f,"%hd %hd %hd %hd %hd %hd",
				  &rp->Exits[0],&rp->Exits[1],&rp->Exits[2],
				  &rp->Exits[3],&rp->Exits[4],&rp->Exits[5])!=6)
		{
			fprintf(stderr, "Bad room line (%d)\n",ct);
			FreeDatabase();
			return 0;
		}

		rp->Text=ReadString(f);
		if(loud)
			fprintf(stderr, "Room %d: \"%s\"\n",ct, rp->Text);
		if(loud){
			fprintf(stderr, "Room connections for room %d:\n",ct);
			for (int i = 0; i < 6; i++)
				fprintf(stderr, "Exit %d: %d\n", i, rp->Exits[i]);
		}
		rp->Image=255;
		ct++;
		rp++;
	}

	ct=0;
	if(loud)
		fprintf(stderr, "Reading %d messages.\n",mn);
	while(ct<mn+1)
	{
		Messages[ct]=ReadString(f);
		if(loud)
			fprintf(stderr, "Message %d: \"%s\"\n",ct, Messages[ct]);
		ct++;
	}
	ct=0;
	if(loud)
		fprintf(stderr, "Reading %d items.\n",ni);
	ip=Items;
	while(ct<ni+1)
	{
		ip->Text=ReadString(f);
		if(loud)
			fprintf(stderr, "Item %d: \"%s\"\n",ct, ip->Text);
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
		if(fscanf(f,"%hd",&lo)!=1)
		{
			fprintf(stderr, "Bad item line (%d)\n",ct);
			FreeDatabase();
			return 0;
		}
		ip->Location=(unsigned char)lo;
		if(loud)
			fprintf(stderr, "Location of item %d: %d, \"%s\"\n",ct, ip->Location, Rooms[ip->Location].Text);
		ip->InitialLoc=ip->Location;
		ip++;
		ct++;
	}
	ct=0;
	/* Discard Comment Strings */
	while(ct<na+1)
	{
		free(ReadString(f));
		ct++;
	}
	if(fscanf(f,"%d",&ct)!=1)
	{
		fprintf(stderr, "Cannot read version\n");
		FreeDatabase();
		return 0;
	}
	if(loud)
		fprintf(stderr, "Version %d.%02d of Adventure \n",
			   ct/100,ct%100);
	if(fscanf(f,"%d",&ct)!=1)
	{
		fprintf(stderr, "Cannot read adventure number\n");
		FreeDatabase();
		return 0;
	}
	if(loud)
		fprintf(stderr, "%d.\nLoad Complete.\n\n",ct);
	return 1;
}


void Output(const char *a)
{
	Display(Bottom, "%s", a);
}

void OutputNumber(int a)
{
	Display(Bottom, "%d", a);
}

void DrawBlack(void) {
	glk_window_fill_rect(Graphics, 0, x_offset, 0, 32 * 8 * pixel_size, 12 * 8 * pixel_size);
}

void DrawImage(int image) {
	OpenGraphicsWindow();
	if (Graphics == NULL) {
		fprintf(stderr, "DrawImage: Graphic window NULL?\n");
		return;
	}
	draw_saga_picture_number(image);
}

void DrawRoomImage(void) {

	if (CurrentGame == ADVENTURELAND) {
		adventureland_darkness();
	}

	int dark = ((BitFlags&(1<<DARKBIT)) && Items[LIGHT_SOURCE].Location!= CARRIED
	&& Items[LIGHT_SOURCE].Location!= MyLoc);

	if ((dark || CurrentGame == SPIDERMAN) && Graphics != NULL && Rooms[MyLoc].Image != 255) {
		DrawBlack();
		if (dark) {
			return;
		}
	}

	if (CurrentGame == SEAS_OF_BLOOD) {
		seas_of_blood_room_image();
		return;
	}
	if (CurrentGame == ROBIN_OF_SHERWOOD) {
		robin_of_sherwood_look();
		return;
	}

	if (CurrentGame == HULK) {
		hulk_look();
		return;
	}

	if (Rooms[MyLoc].Image == 255) {
		CloseGraphicsWindow();
		return;
	}

	if (dark)
		return;

	if (GameInfo->type == GREMLINS_VARIANT) {
		gremlins_look();
	} else {
		DrawImage(Rooms[MyLoc].Image & 127);
	}
	for (int ct = 0; ct <= GameHeader.NumItems; ct++)
		if (Items[ct].Image && Items[ct].Location == MyLoc) {
			if ((Items[ct].Flag & 127) == MyLoc) {
				DrawImage(Items[ct].Image);
				/* Draw the correct image of the bear on the beach */
			} else if (CurrentGame == SAVAGE_ISLAND
					   && ct == 20 && MyLoc == 8) {
				DrawImage(9);
			}
		}
}

#pragma mark Room description

strid_t room_description_stream = NULL;

void WriteToRoomDescriptionStream(const char *fmt, ...)
{
	if (room_description_stream == NULL)
		return;
	va_list ap;
	char msg[2048];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);

	glk_put_string_stream(room_description_stream, msg);
}

static void PrintWindowDelimiter(void) {
	glk_window_get_size(Top, &Width, &TopHeight);
	glk_window_move_cursor(Top, 0, TopHeight - 1);
	glk_stream_set_current(glk_window_get_stream(Top));
	if (Options & SPECTRUM_STYLE)
		for (int i = 0; i < Width; i++)
			glk_put_char('*');
	else {
		glk_put_char('<');
		for (int i = 0; i < Width - 2; i++)
			glk_put_char('-');
		glk_put_char('>');
	}
}

static void ListExitsSpectrumStyle(void) {
	int ct=0;
	int f=0;

	while(ct<6) {
		if((&Rooms[MyLoc])->Exits[ct]!=0) {
			if(f==0) {
				WriteToRoomDescriptionStream("\n\n%s", sys[EXITS]);
			}
			else {
				WriteToRoomDescriptionStream(sys[EXITS_DELIMITER]);
			}
			/* sys[] begins with the exit names */
			WriteToRoomDescriptionStream("%s",sys[ct]);
			f=1;
		}
		ct++;
	}
	WriteToRoomDescriptionStream("\n");
	return;
}


static void ListExits(void)
{
	int ct=0;
	int f=0;

	WriteToRoomDescriptionStream("\n\n%s", sys[EXITS]);

	while(ct<6)
	{
		if((&Rooms[MyLoc])->Exits[ct]!=0) {
			if(f) {
				WriteToRoomDescriptionStream(sys[EXITS_DELIMITER]);
			}
			/* sys[] begins with the exit names */
			WriteToRoomDescriptionStream(sys[ct]);
			f=1;
		}
		ct++;
	}
	if (f == 0)
		WriteToRoomDescriptionStream(sys[NONE]);
	return;
}

static void FlushRoomDescription(char *buf) {
	glk_stream_close(room_description_stream, 0);

	Display(Bottom, buf);
	free(buf);

	if (Options & (TRS80_STYLE | SPECTRUM_STYLE)) {
		PrintWindowDelimiter();
	}
}

static void FlushRoomDescriptionSplitScreen(char *buf)
{
	glk_stream_close(room_description_stream, 0);

	int print_delimiter = (Options & (TRS80_STYLE | SPECTRUM_STYLE));

	glk_window_clear(Top);
	glk_window_get_size(Top, &Width, &TopHeight);
	int rows, length;
	char *text_with_breaks = LineBreakText(buf, Width, &rows, &length);

	glui32 bottomheight;
	glk_window_get_size(Bottom, NULL, &bottomheight);
	winid_t o2 = glk_window_get_parent(Top);
	if (!(bottomheight < 3 && TopHeight < rows)) {
		glk_window_get_size(Top, &Width, &TopHeight);
		glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed, rows, Top);
	} else {
		print_delimiter = 0;
	}

	int line = 0;
	int index = 0;
	int i;
	char string[Width + 1];
	for (line = 0; line < rows && index < length; line++) {
		for (i = 0; i < Width; i++) {
			string[i] = text_with_breaks[index++];
			if (string[i] == 10 || string[i] == 13 || index >= length)
				break;
		}
		if (i < Width + 1) {
			string[i++] = '\n';
		}
		string[i] = 0;
		if (strlen(string) == 0)
			break;
		glk_window_move_cursor(Top, 0, line);
		Display(Top, string);
	}

	if (line < rows - 1) {
		glk_window_get_size(Top, &Width, &TopHeight);
		glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed, MIN(rows - 1, TopHeight - 1), Top);
	}

	free(buf);
	free(text_with_breaks);
	if (print_delimiter) {
		PrintWindowDelimiter();
	}
	
	if (pause_next_room_description) {
		Delay(0.8);
		pause_next_room_description = 0;
	}
}

void Look(void) {

	DrawRoomImage();

	if (split_screen && Top == NULL)
		return;
	
	char *buf = MemAlloc(1000);
	bzero(buf, 1000);
	room_description_stream = glk_stream_open_memory(buf, 1000, filemode_Write, 0);

	Room *r;
	int ct,f;

	if (!split_screen) {
		WriteToRoomDescriptionStream("\n");
	} else if (Transcript) {
		glk_put_char_stream_uni(Transcript, 10);
	}

	if((BitFlags&(1<<DARKBIT)) && Items[LIGHT_SOURCE].Location!= CARRIED
	   && Items[LIGHT_SOURCE].Location!= MyLoc)
	{
		WriteToRoomDescriptionStream(sys[TOO_DARK_TO_SEE]);
		if (split_screen)
			FlushRoomDescriptionSplitScreen(buf);
		else
			FlushRoomDescription(buf);
		return;
	}

	r=&Rooms[MyLoc];
	/* An initial asterisk means the room description should not */
	/* start with "You are" or equivalent */
	if(*r->Text=='*') {
		WriteToRoomDescriptionStream(r->Text+1);
	} else {
		WriteToRoomDescriptionStream("%s%s", sys[YOU_ARE], r->Text);
	}

	if (!(Options & SPECTRUM_STYLE)) {
		ListExits();
		WriteToRoomDescriptionStream(".\n");
	}

	ct=0;
	f=0;
	while(ct<=GameHeader.NumItems) {
		if(Items[ct].Location==MyLoc) {
			if (Items[ct].Text[0] == 0) {
				fprintf(stderr, "Invisible item in room: %d\n", ct);
				ct++;
				continue;
			}
			if(f==0) {
				WriteToRoomDescriptionStream(sys[YOU_SEE]);
				f++;
				if (Options & SPECTRUM_STYLE)
					WriteToRoomDescriptionStream("\n");
			} else if (!(Options & (TRS80_STYLE | SPECTRUM_STYLE))) {
				WriteToRoomDescriptionStream(sys[ITEM_DELIMITER]);
			}
			WriteToRoomDescriptionStream(Items[ct].Text);
			if (Options & (TRS80_STYLE | SPECTRUM_STYLE)) {
				WriteToRoomDescriptionStream(sys[ITEM_DELIMITER]);
			}
		}
		ct++;
	}

	if (Options & SPECTRUM_STYLE) {
		ListExitsSpectrumStyle();
	} else if (f) {
		WriteToRoomDescriptionStream("\n");
	}
	if (split_screen)
		FlushRoomDescriptionSplitScreen(buf);
	else
		FlushRoomDescription(buf);
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
	Output(sys[SAVED]);
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

	struct SavedState *state = SaveCurrentState();

	int result;

	for(ct=0;ct<16;ct++) {
		glk_get_line_stream(file, buf, sizeof buf);
		result = sscanf(buf, "%d %d",&Counters[ct],&RoomSaved[ct]);
		if (result != 2 || RoomSaved[ct] > GameHeader.NumRooms) {
			RecoverFromBadRestore(state);
			return;
		}
	}
	glk_get_line_stream(file, buf, sizeof buf);
	result = sscanf(buf, "%ld %hd %hd %d %d %hd\n",
		   &BitFlags,&DarkFlag,&MyLoc,&CurrentCounter,&SavedRoom,
		   &GameHeader.LightTime);
	if (result != 6 || MyLoc > GameHeader.NumRooms ||
		MyLoc < 1 || SavedRoom > GameHeader.NumRooms) {
		RecoverFromBadRestore(state);
		return;
	}

	/* Backward compatibility */
	if(DarkFlag)
		BitFlags|=(1<<15);
	for(ct=0;ct<=GameHeader.NumItems;ct++) {
		glk_get_line_stream(file, buf, sizeof buf);
		result = sscanf(buf, "%hd\n",&lo);
		Items[ct].Location=(unsigned char)lo;
		if (result != 1 || (Items[ct].Location > GameHeader.NumRooms &&
			Items[ct].Location != CARRIED)) {
			RecoverFromBadRestore(state);
			return;
		}
	}

	glui32 position = glk_stream_get_position(file);
	glk_stream_set_position(file, 0, seekmode_End);
	glui32 end = glk_stream_get_position(file);
	if (end != position) {
		RecoverFromBadRestore(state);
		return;
	}

	SaveUndo();
	just_started = 0;
	stop_time = 1;
}


static void RestartGame(void) {
	before_first_turn = 1;
	if (CurrentCommand)
		FreeCommands();
	RestoreState(initial_state);
	just_started = 0;
	stop_time = 0;
	glk_window_clear(Bottom);
	OpenTopWindow();
	PerformActions(0, 0);
}

static void TranscriptOn(void) {
	frefid_t ref;

	if (Transcript) {
		Output(sys[TRANSCRIPT_ALREADY]);
		return;
	}

	ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_Transcript, filemode_Write, 0);
	if(ref == NULL) return;

	Transcript = glk_stream_open_file_uni(ref, filemode_Write, 0);
	glk_fileref_destroy(ref);

	if(Transcript == NULL) {
		Output(sys[FAILED_TRANSCRIPT]);
		return;
	}

	glui32 *start_of_transcript = ToUnicode(sys[TRANSCRIPT_START]);
	glk_put_string_stream_uni(Transcript, start_of_transcript);
	free(start_of_transcript);
	glk_put_string_stream(glk_window_get_stream(Bottom), sys[TRANSCRIPT_ON]);
}

static void TranscriptOff(void)
{
	if (Transcript == NULL) {
		Output(sys[NO_TRANSCRIPT]);
		return;
	}

	glui32 *end_of_transcript = ToUnicode(sys[TRANSCRIPT_END]);
	glk_put_string_stream_uni(Transcript, end_of_transcript);
	free(end_of_transcript);

	glk_stream_close(Transcript, NULL);
	Transcript = NULL;
	Output(sys[TRANSCRIPT_OFF]);
}

int PerformExtraCommand(void) {
	struct Command command = *CurrentCommand;
	int verb = command.verb;
	if (verb > GameHeader.NumWords)
		verb -= GameHeader.NumWords;
	int noun = command.noun;
	if (noun > GameHeader.NumWords)
		noun -= GameHeader.NumWords;
	else if (noun) {
		const char *NounWord = CharWords[CurrentCommand->nounwordindex];
		noun = WhichWord(NounWord, ExtraNouns, strlen(NounWord), NUMBER_OF_EXTRA_NOUNS);
		noun = ExtraNounsKey[noun];
	}

	stop_time = 1;

	switch (verb) {
		case RESTORE:
			if (noun == 0 || noun == GAME) {
				LoadGame();
				return 1;
			}
			break;
		case RESTART:
			if (noun == 0 || noun == GAME) {
				Output(sys[ARE_YOU_SURE]);
				if (YesOrNo()) {
					RestartGame();
				}
				return 1;
			}
			break;
		case SAVE:
			if (noun == 0 || noun == GAME) {
				SaveGame();
				return 1;
			}
			break;
		case UNDO:
			if (noun == 0 || noun == COMMAND) {
				RestoreUndo();
				return 1;
			}
			break;
		case RAM:
			if (noun == RAMLOAD) {
				RamRestore();
				return 1;
			} else if (noun == RAMSAVE) {
				RamSave();
				return 1;
			}
			break;
		case RAMSAVE:
			if (noun == 0) {
				RamSave();
				return 1;
			}
			break;
		case RAMLOAD:
			if (noun == 0) {
				RamRestore();
				return 1;
			}
			break;
		case SCRIPT:
			if (noun == ON || noun == 0) {
				TranscriptOn();
				return 1;
			} else if (noun == OFF) {
				TranscriptOff();
				return 1;
			}
			break;
	}

	stop_time = 0;
	return 0;
}

static int YesOrNo(void) {

	glk_request_char_event(Bottom);

	event_t ev;
	int result = 0;
	const char y = tolower(sys[YES][0]);
	const char n = tolower(sys[NO][0]);

	do
	{
		glk_select(&ev);
		if (ev.type == evtype_CharInput) {
			const char reply = tolower(ev.val1);
			if (reply == y) {
				result = 1;
			} else if (reply == n) {
				result = 2;
			} else {
				Output(sys[ANSWER_YES_OR_NO]);
				glk_request_char_event(Bottom);
			}
		}
		else Updates(ev);
	} while(result == 0);

	return (result == 1);
}

void HitEnter(void) {
	glk_request_char_event(Bottom);

	event_t ev;
	int result = 0;
	do {
		glk_select(&ev);
		if (ev.type == evtype_CharInput) {
			if (ev.val1 == '\n' || ev.val1 == '\372') {
				result = 1;
			} else {
				fprintf(stderr, "%c\n", ev.val1);
				glk_request_char_event(Bottom);
			}
		}
		else Updates(ev);
	} while(result == 0);

	return;
}

void ListInventory(void) {
	int i = 0;
	int anything = 0;
	Output(sys[INVENTORY]);
	while (i <= GameHeader.NumItems) {
		if (Items[i].Location==CARRIED) {
			if (Items[i].Text[0] == 0) {
				fprintf(stderr, "Invisible item in inventory: %d\n", i);
				i++;
				continue;
			}
			if (anything == 1 && (Options & (TRS80_STYLE | SPECTRUM_STYLE)) == 0) {
				Output(sys[ITEM_DELIMITER]);
			}
			anything=1;
			Output(Items[i].Text);
			if (Options & (TRS80_STYLE | SPECTRUM_STYLE)) {
				Output(sys[ITEM_DELIMITER]);
			}
		}
		i++;
	}
	if (anything == 0)
		Output(sys[NOTHING]);
	if (Transcript) {
		glk_put_char_stream_uni(Transcript, 10);
	}
}


void LookWithPause(void) {
	if (before_first_turn || Rooms[MyLoc].Text == NULL || Rooms[MyLoc].Text[0] == 0)
		return;
	pause_next_room_description = 1;
	Look();
}

//#define DEBUG_ACTIONS

static int PerformLine(int ct)
{
#ifdef DEBUG_ACTIONS
	fprintf(stderr, "Performing line %d: ", ct);
#endif
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
#ifdef DEBUG_ACTIONS
			fprintf(stderr, "Testing condition %d: ", cv);
#endif
		switch(cv) {
			case 0:
				param[pptr++]=dv;
				break;
			case 1:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Does the player carry %s?\n", Items[dv].Text);
#endif
				if(Items[dv].Location!=CARRIED)
					return(0);
				break;
			case 2:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Is %s in location?\n", Items[dv].Text);
#endif
				if(Items[dv].Location!=MyLoc)
					return(0);
				break;
			case 3:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Is %s held or in location?\n", Items[dv].Text);
#endif
				if(Items[dv].Location!=CARRIED &&
				   Items[dv].Location!=MyLoc)
					return(0);
				break;
			case 4:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Is location %s?\n", Rooms[dv].Text);
#endif
				if(MyLoc!=dv)
					return(0);
				break;
			case 5:
#ifdef DEBUG_ACTIONS
               fprintf(stderr, "Is %s NOT in location?\n", Items[dv].Text);
#endif
				if(Items[dv].Location==MyLoc)
					return(0);
				break;
			case 6:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Does the player NOT carry %s?\n", Items[dv].Text);
#endif
				if(Items[dv].Location==CARRIED)
					return(0);
				break;
			case 7:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Is location NOT %s?\n", Rooms[dv].Text);
#endif
				if(MyLoc==dv)
					return(0);
				break;
			case 8:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Is bitflag %d set?\n", dv);
#endif
				if((BitFlags&(1<<dv))==0)
					return(0);
				break;
			case 9:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Is bitflag %d NOT set?\n", dv);
#endif
				if(BitFlags&(1<<dv))
					return(0);
				break;
			case 10:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Does the player carry anything?\n");
#endif
				if(CountCarried()==0)
					return(0);
				break;
			case 11:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Does the player carry nothing?\n");
#endif
				if(CountCarried())
					return(0);
				break;
			case 12:
#ifdef DEBUG_ACTIONS
                fprintf(stderr, "Is %s neither carried nor in room?\n", Items[dv].Text);
#endif
				if(Items[dv].Location==CARRIED||Items[dv].Location==MyLoc)
					return(0);
				break;
			case 13:
#ifdef DEBUG_ACTIONS
				                fprintf(stderr, "Is %s (%d) in play?\n", Items[dv].Text, dv);
#endif
				if(Items[dv].Location==0)
					return(0);
				break;
			case 14:
#ifdef DEBUG_ACTIONS
				                fprintf(stderr, "Is %s NOT in play?\n", Items[dv].Text);
#endif
				if(Items[dv].Location)
					return(0);
				break;
			case 15:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Is CurrentCounter <= %d?\n", dv);
#endif
				if(CurrentCounter>dv)
					return(0);
				break;
			case 16:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Is CurrentCounter > %d?\n", dv);
#endif
				if(CurrentCounter<=dv)
					return(0);
				break;
			case 17:
#ifdef DEBUG_ACTIONS
                fprintf(stderr, "Is %s still in initial room?\n", Items[dv].Text);
#endif
				if(Items[dv].Location!=Items[dv].InitialLoc)
					return(0);
				break;
			case 18:
#ifdef DEBUG_ACTIONS
                fprintf(stderr, "Has %s been moved?\n", Items[dv].Text);
#endif
				if(Items[dv].Location==Items[dv].InitialLoc)
					return(0);
				break;
			case 19:/* Only seen in Brian Howarth games so far */
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Is current counter == %d?\n", dv);
				if(CurrentCounter!=dv)
					fprintf(stderr, "Nope, current counter is %d\n", CurrentCounter);
#endif
				if(CurrentCounter!=dv)
					return(0);
				break;
		}
#ifdef DEBUG_ACTIONS
		fprintf(stderr, "YES\n");
#endif
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
#ifdef DEBUG_ACTIONS
		fprintf(stderr, "Performing action %d: ", act[cc]);
#endif
		if(act[cc]>=1 && act[cc]<52)
		{
#ifdef DEBUG_ACTIONS
			fprintf(stderr, "Action: print message %d: \"%s\"\n", act[cc], Messages[act[cc]]);
#endif
			Output(Messages[act[cc]]);
			const char lastchar = Messages[act[cc]][strlen(Messages[act[cc]]) - 1];
			if (lastchar != 13 && lastchar != 10)
				Output(sys[MESSAGE_DELIMITER]);
		}
		else if(act[cc]>101)
		{
#ifdef DEBUG_ACTIONS
			fprintf(stderr, "Action: print message %d: \"%s\"\n", act[cc]-50, Messages[act[cc]-50]);
#endif
			Output(Messages[act[cc]-50]);
			const char lastchar = Messages[act[cc]-50][strlen(Messages[act[cc]-50]) - 1];
			if (lastchar != 13 && lastchar != 10)
				Output(sys[MESSAGE_DELIMITER]);
		}
		else switch(act[cc])
		{
			case 0:/* NOP */
				break;
			case 52:
				if(CountCarried()==GameHeader.MaxCarry)
				{
					Output(sys[YOURE_CARRYING_TOO_MUCH]);
					break;
				}
				Items[param[pptr++]].Location= CARRIED;
				break;
			case 53:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "item %d (\"%s\") is now in location.\n", param[pptr], Items[param[pptr]].Text);
#endif
				Items[param[pptr++]].Location=MyLoc;
				break;
			case 54:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "player location is now room %d (%s).\n", param[pptr], Rooms[param[pptr]].Text);
#endif
				MyLoc=param[pptr++];
				if (moves_this_turn++ > 0) {
					LookWithPause();
				}
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
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Bitflag %d is set\n", param[pptr]);
#endif
				BitFlags|=(1<<param[pptr++]);
				break;
			case 59:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Item %d (%s) is removed from play.\n", param[pptr], Items[param[pptr]].Text);
#endif
				Items[param[pptr++]].Location=0;
				break;
			case 60:
#ifdef DEBUG_ACTIONS
							fprintf(stderr, "BitFlag %d is cleared\n", param[pptr]);
#endif
				BitFlags&=~(1<<param[pptr++]);
				break;
			case 61:
				Output(sys[IM_DEAD]);
				dead = 1;
				LookWithPause();
				BitFlags&=~(1<<DARKBIT);
				MyLoc=GameHeader.NumRooms;/* It seems to be what the code says! */
				break;
			case 62:
			{
				/* Bug fix for some systems - before it could get parameters wrong */
				int i=param[pptr++];
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Item %d (%s) is put in room %d (%s). MyLoc: %d (%s)\n", i, Items[i].Text, param[pptr], Rooms[param[pptr]].Text, MyLoc, Rooms[MyLoc].Text);
#endif
				if (Items[i].Location == MyLoc || param[pptr] == MyLoc) {
					LookWithPause();
				}
				Items[i].Location=param[pptr++];
				break;
			}
			case 63:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Game over.\n");
#endif
			doneit:
				if (split_screen && Top)
					Look();
				dead = 1;
				if (!before_first_turn) {
					Output("\n\n");
					Output(sys[PLAY_AGAIN]);
					Output("\n");
					if (YesOrNo()) {
						RestartGame();
						break;
					} else {
						if (Transcript)
							glk_stream_close(Transcript, NULL);
						glk_exit();
					}
				}
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
				Display(Bottom, "%s %d %s%s %d.\n", sys[IVE_STORED], n, sys[TREASURES],
						sys[ON_A_SCALE_THAT_RATES], (n*100)/GameHeader.Treasures);
				if(n==GameHeader.Treasures)
				{
					Output(sys[YOUVE_SOLVED_IT]);
					goto doneit;
				}
				break;
			}
			case 66:
			{
				if (CurrentGame == SEAS_OF_BLOOD) {
					adventure_sheet();
					break;
				}
				ListInventory();
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
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Continue with next line\n");
#endif
				continuation=1;
				break;
			case 74:
				Items[param[pptr++]].Location = CARRIED;
				break;
			case 75:
			{
				int i1,i2;
				i1=param[pptr++];
				i2=param[pptr++];
				Items[i1].Location=Items[i2].Location;
				break;
			}
			case 76:	/* Looking at adventure .. */
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "LOOK\n");
#endif
				if (split_screen)
					Look();
				break;
			case 77:
				if(CurrentCounter >= 1)
					CurrentCounter--;
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "decrementing current counter. Current counter is now %d.\n", CurrentCounter);
#endif
				break;
			case 78:
				OutputNumber(CurrentCounter);
				Output(" ");
				break;
			case 79:
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "CurrentCounter is set to %d.\n", param[pptr]);
#endif
				CurrentCounter=param[pptr++];
				break;
			case 80:
			{
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "switch location to stored location (%d) (%s).\n", SavedRoom, Rooms[SavedRoom].Text);
#endif
				moves_this_turn++;
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
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Select a counter. Current counter is swapped with backup counter %d\n", param[pptr]);
#endif
				int t=param[pptr++];
				int c1=CurrentCounter;
				CurrentCounter=Counters[t];
				Counters[t]=c1;
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "Value of new selected counter is %d\n", CurrentCounter);
#endif
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
				if (CurrentCommand)
					glk_put_string_stream_uni(glk_window_get_stream(Bottom), UnicodeWords[CurrentCommand->nounwordindex]);
				break;
			case 85:
				if (CurrentCommand)
					glk_put_string_stream_uni(glk_window_get_stream(Bottom), UnicodeWords[CurrentCommand->nounwordindex]);
				Output("\n");
				break;
			case 86:
				if(!(Options & SPECTRUM_STYLE))
					Output("\n");
//				else Output(" ");
				break;
			case 87:
			{
#ifdef DEBUG_ACTIONS
				fprintf(stderr, "swap location<->roomflag[%d]\n", param[pptr]);
#endif
				/* Changed this to swap location<->roomflag[x]
				 not roomflag 0 and x */
				int p=param[pptr++];
				int sr=MyLoc;
				moves_this_turn++;
				MyLoc=RoomSaved[p];
				RoomSaved[p]=sr;
				LookWithPause();
				break;
			}
			case 88:
#ifdef DEBUG_ACTIONS
				fprintf(stderr,"Delay\n");
#endif
				Delay(1);
				break;
			case 89: {
				fprintf(stderr,"Action 89, parameter %d\n",param[pptr]);
				int p=param[pptr++];
				switch(CurrentGame) {
					case SECRET_MISSION:
						secret_action(p);
						break;
					case ADVENTURELAND:
						adventureland_action(p);
						break;
					case SEAS_OF_BLOOD:
						blood_action(p);
						break;
					case ROBIN_OF_SHERWOOD:
						sherwood_action(p);
						break;
					case GREMLINS:
					case GREMLINS_SPANISH:
					case GREMLINS_GERMAN:
						gremlins_action(p);
						break;
					default:
						fprintf(stderr,"Action 89 in unsupported game!\n");
						break;
				}
				break;
			}
			case 90:
			{
#ifdef DEBUG_ACTIONS
				fprintf(stderr,"Draw Hulk image, parameter %d\n",param[pptr]);
#endif
				if (CurrentGame != HULK)
					fprintf(stderr,"Draw Hulk image in non-Hulk game!\n");
				if (!(BitFlags&(1<<DARKBIT)))
					draw_hulk_image(param[pptr++]);
				break;
			}
			default:
				fprintf(stderr,"Unknown action %d [Param begins %d %d]\n",
						act[cc],param[pptr],param[pptr+1]);
				break;
		}
		cc++;
	}

//	if (just_restored)
//		return 0;

	return(1+continuation);
}

void PrintTakenOrDropped(int index) {
	Output(sys[index]);
	int length = strlen(sys[index]);
	char last = sys[index][length-1];
	if (last == 10 || last == 13)
		return;
	Output(" ");
	if ((CurrentCommand->allflag && (CurrentCommand->allflag & LASTALL) != LASTALL) || split_screen == 0) {
		Output("\n");
	}
}

static int PerformActions(int vb,int no)
{
	int dark=BitFlags&(1<<DARKBIT);

	int ct=0;
	int flag;
	int doagain=0;
	int found_match = 0;
	moves_this_turn = 0;
#pragma mark GO
	if(vb==GO && no == -1 )
	{
		Output(sys[DIRECTION]);
		return(0);
	}
	if(vb==1 && no>=1 && no<=6)
	{
		int nl;
		if(Items[LIGHT_SOURCE].Location==MyLoc ||
		   Items[LIGHT_SOURCE].Location==CARRIED)
			dark=0;
		if(dark)
			Output(sys[DANGEROUS_TO_MOVE_IN_DARK]);
		nl=Rooms[MyLoc].Exits[no-1];
		if(nl!=0)
		{
			/* Seas of Blood needs this to be able to flee back to the last room */
			if (CurrentGame == SEAS_OF_BLOOD)
				SavedRoom = MyLoc;
			if(Options & SPECTRUM_STYLE)
				Output(sys[OK]);
			MyLoc=nl;
			if (CurrentCommand && CurrentCommand->next) {
				LookWithPause();
			}
			return(0);
		}
		if(dark)
		{
			Output(sys[YOU_FELL_AND_BROKE_YOUR_NECK]);
			Output("\n\n");
			Output(sys[PLAY_AGAIN]);
			Output("\n");
			if (YesOrNo()) {
				RestartGame();
				return 0;
			} else {
				if (Transcript)
					glk_stream_close(Transcript, NULL);
				glk_exit();
			}
		}
		Output(sys[YOU_CANT_GO_THAT_WAY]);
		return(0);
	}

	if (CurrentGame == HULK && vb == 39 && !dark) {
		hulk_show_image_on_examine(no);
	}

	flag = -1;
	while(ct<=GameHeader.NumActions)
	{
		int verbvalue,nounvalue;
		verbvalue=Actions[ct].Vocab;
		/* Think this is now right. If a line we run has an action73
		 run all following lines with vocab of 0,0 */
		if(vb!=0 && (doagain&&verbvalue!=0))
			break;
		/* Oops.. added this minor cockup fix 1.11 */
		if(vb!=0 && !doagain && flag== 0)
			break;
		nounvalue=verbvalue%150;
		verbvalue/=150;
		if((verbvalue==vb)||(doagain&&Actions[ct].Vocab==0))
		{
			if((verbvalue==0 && RandomPercent(nounvalue))||doagain||
			   (verbvalue!=0 && (nounvalue==no||nounvalue==0)))
			{
				if (verbvalue == vb && vb !=0 && no != 0 && nounvalue == no)
					found_match = 1;
				int f2;
				if(flag == -1)
					flag = -2;
				if((f2=PerformLine(ct))>0)
				{
					/* ahah finally figured it out ! */
					flag = 0;
					if(f2 == 2)
						doagain=1;
					if(vb != 0 && doagain == 0)
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

	if (found_match)
		return flag;

	if(flag !=0)
	{
		int item = 0;
		if(Items[LIGHT_SOURCE].Location==MyLoc ||
		   Items[LIGHT_SOURCE].Location==CARRIED)
			dark=0;

#pragma mark TAKE
		if(vb==TAKE || vb==DROP)
		{
			if (CurrentCommand->allflag) {
				if(vb==TAKE && dark) {
					Output(sys[TOO_DARK_TO_SEE]);
					while((CurrentCommand->allflag & LASTALL) != LASTALL) {
						CurrentCommand = CurrentCommand->next;
					}
					return 0;
				}
				item = CurrentCommand->item;
				int location = CARRIED;
				if (vb == TAKE)
					location = MyLoc;
				while (Items[item].Location != location &&
					   (CurrentCommand->allflag & LASTALL) != LASTALL) {
					CurrentCommand = CurrentCommand->next;
				}
				if (Items[item].Location != location)
					return 0;
				Output(Items[CurrentCommand->item].Text);
				Output("....");
			}

			/* Yes they really _are_ hardcoded values */
			if(vb==TAKE)
			{
				if(no==-1)
				{
					Output(sys[WHAT]);
					return(0);
				}
				if(CountCarried()==GameHeader.MaxCarry)
				{
					Output(sys[YOURE_CARRYING_TOO_MUCH]);
					return(0);
				}
				if (!item)
					item=MatchUpItem(no,MyLoc);
				if(item==-1) {
					item=MatchUpItem(no,CARRIED);
					if(item==-1) {
						item=MatchUpItem(no,0);
						if(item==-1) {
							Output(sys[THATS_BEYOND_MY_POWER]);
						} else {
							Output(sys[YOU_DONT_SEE_IT]);
						}
					} else {
						Output(sys[YOU_HAVE_IT]);
					}
					return(0);
				}
				Items[item].Location = CARRIED;
				PrintTakenOrDropped(TAKEN);
				return(0);
			}
#pragma mark DROP
			if(vb==DROP) {
				if(no==-1) {
					Output(sys[WHAT]);
					return(0);
				}
				if (!item)
					item=MatchUpItem(no,CARRIED);
				if(item==-1) {
					item=MatchUpItem(no,0);
					if(item==-1) {
						Output(sys[THATS_BEYOND_MY_POWER]);
					} else {
						Output(sys[YOU_HAVENT_GOT_IT]);
					}
					return(0);
				}
				Items[item].Location=MyLoc;
				PrintTakenOrDropped(DROPPED);
				return(0);
			}
		}
	}
	return(flag);
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
		game_file = argv[1];

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


	for (int i = 0; i < MAX_SYSMESS; i++) {
		sys[i] = sysdict[i];
	}


	char **dictpointer;

	if (Options & YOUARE)
		dictpointer = (char **)sysdict;
	else
		dictpointer = (char **)sysdict_i_am;

	for (int i = 0; i < MAX_SYSMESS && dictpointer[i] != NULL; i++) {
		sys[i] = dictpointer[i];
	}

	GameIDType game_type = detect_game(f);

	if (!game_type)
		glk_exit();

	if (game_type != SCOTTFREE) {
		Options|=SPECTRUM_STYLE;
		split_screen = 1;
	} else {
		Options|=TRS80_STYLE;
		split_screen = 1;
	}

	/* For the Adventure Sheet */
	if (game_type == SEAS_OF_BLOOD)
		glk_stylehint_set(wintype_TextBuffer, style_Preformatted, stylehint_Justification, stylehint_just_Centered);

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

	OpenTopWindow();

	if (game_type == SCOTTFREE)
		Output("\
Scott Free, A Scott Adams game driver in C.\n\
Release 1.14, (c) 1993,1994,1995 Swansea University Computer Society.\n\
Distributed under the GNU software license\n\n");

#ifdef SPATTERLIGHT
	if (gli_determinism)
		srand(1234);
	else
#endif
		srand((unsigned int)time(NULL));

	if (initial_state == NULL) {
		initial_state = SaveCurrentState();
		SaveUndo();
	}

	while(1) {
		glk_tick();

		if (!stop_time)
			PerformActions(0,0);
		if (!(CurrentCommand && CurrentCommand->allflag && (CurrentCommand->allflag & LASTALL) != LASTALL))
			Look();

		if (!stop_time && !before_first_turn && !dead)
			SaveUndo();

		before_first_turn = 0;

		if (GetInput(&vb, &no) == 1)
			continue;

		switch(PerformActions(vb,no))
		{
			case -1:
				if (!RecheckForExtraCommand())
					Output(sys[I_DONT_UNDERSTAND]);
				break;
			case -2:
				Output(sys[YOU_CANT_DO_THAT_YET]);
				break;
			default:
				just_started = 0;
				stop_time = 0;
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
					Output(sys[LIGHT_HAS_RUN_OUT]);
				}
				if(Options&PREHISTORIC_LAMP)
					Items[LIGHT_SOURCE].Location=DESTROYED;
			} else if(GameHeader.LightTime<25) {
				if(Items[LIGHT_SOURCE].Location==CARRIED ||
				   Items[LIGHT_SOURCE].Location==MyLoc)
				{

					if(Options&SCOTTLIGHT)
					{
						Output(sys[LIGHT_RUNS_OUT_IN]);
						OutputNumber(GameHeader.LightTime);
						Output(sys[TURNS]);
					}
					else
					{
						if(GameHeader.LightTime%5==0)
							Output(sys[LIGHT_GROWING_DIM]);
					}
				}
			}
		}
	}
}
