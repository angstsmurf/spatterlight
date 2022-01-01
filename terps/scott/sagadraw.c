/* Sagadraw v2.4
 Originally by David Lodge

 With help from Paul David Doherty (including the colour code)
 */

//  16357 diff

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"

#include "sagadraw.h"

extern void *MemAlloc(int size);
extern winid_t Graphics;

uint8_t sprite[255][8];
uint8_t screenchars[768][8];


typedef struct
{
   uint8_t *imagedata;
   uint8_t xoff;
   uint8_t yoff;
   uint8_t width;
   uint8_t height;
} Image;

Image *images = NULL;

uint8_t *forest_images = NULL;

int pixel_size;
int x_offset;
int32_t version;

/* palette handler stuff starts here */

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

PALETTE pal;

int32_t errorcount = 0;

#define NONE 0
#define ZX 1
#define ZXOPT 2
#define C64A 3
#define C64B 4
#define VGA 5
int32_t palchosen = NONE;

#define STRIDE 765 /* 255 pixels * 3 (r, g, b) */

#define INVALIDCOLOR 16

const char *flipdescription[] =
{
   "none",
   "90°",
   "180°",
   "270°",
   "ERROR"
};

void
colrange (int32_t c)
{
   if ((c < 0) || (c > 15))
   {
      fprintf (stderr, "# col out of range: %d\n", c);
#ifdef DRAWDEBUG
      fprintf (stderr, "# col out of range: %d\n", c);
#endif
      errorcount++;
   }
}

void
checkrange (int32_t x, int32_t y)
{
   if ((x < 0) || (x > 254))
   {
      fprintf (stderr, "# x out of range: %d\n", x);
#ifdef DRAWDEBUG
      fprintf (stderr, "# x out of range: %d\n", x);
#endif
      errorcount++;
   }
   if ((y < 96) || (y > 191))
   {
      fprintf (stderr, "# y out of range: %d\n", y);
#ifdef DRAWDEBUG
      fprintf (stderr, "# y out of range: %d\n", y);
#endif
      errorcount++;
   }
}

void
do_palette (const char *palname)
{
   if (strcmp ("zx", palname) == 0)
      palchosen = ZX;
   else if (strcmp ("zxopt", palname) == 0)
      palchosen = ZXOPT;
   else if (strcmp ("c64a", palname) == 0)
      palchosen = C64A;
   else if (strcmp ("c64b", palname) == 0)
      palchosen = C64B;
   else if (strcmp ("vga", palname) == 0)
      palchosen = VGA;
}

void
set_color(int32_t index, RGB *colour)
{
   for (int32_t i = 0; i < 3; i++) {
      pal[index][i] = (*colour)[i];
   }
}

void
define_palette (void)
{
   /* set up the palette */
   if (palchosen == VGA)
   {
      RGB black = { 0, 0, 0 };
      RGB blue = { 0, 0, 63 };
      RGB red = { 63, 0, 0 };
      RGB magenta = { 63, 0, 63 };
      RGB green = { 0, 63, 0 };
      RGB cyan = { 0, 63, 63 };
      RGB yellow = { 63, 63, 0 };
      RGB white = { 63, 63, 63 };
      RGB brblack = { 0, 0, 0 };
      RGB brblue = { 0, 0, 63 };
      RGB brred = { 63, 0, 0 };
      RGB brmagenta = { 63, 0, 63 };
      RGB brgreen = { 0, 63, 0 };
      RGB brcyan = { 0, 63, 63 };
      RGB bryellow = { 63, 63, 0 };
      RGB brwhite = { 63, 63, 63 };

      set_color (0, &black);
      set_color (1, &blue);
      set_color (2, &red);
      set_color (3, &magenta);
      set_color (4, &green);
      set_color (5, &cyan);
      set_color (6, &yellow);
      set_color (7, &white);
      set_color (8, &brblack);
      set_color (9, &brblue);
      set_color (10, &brred);
      set_color (11, &brmagenta);
      set_color (12, &brgreen);
      set_color (13, &brcyan);
      set_color (14, &bryellow);
      set_color (15, &brwhite);
   }
   else if (palchosen == ZX)
   {
      /* corrected Sinclair ZX palette (pretty dull though) */
      RGB black = { 0, 0, 0 };
      RGB blue = { 0, 0, 38 };
      RGB red = { 38, 0, 0 };
      RGB magenta = { 38, 0, 38 };
      RGB green = { 0, 38, 0 };
      RGB cyan = { 0, 38, 38 };
      RGB yellow = { 38, 38, 0 };
      RGB white = { 38, 38, 38 };
      RGB brblack = { 0, 0, 0 };
      RGB brblue = { 0, 0, 42 };
      RGB brred = { 46, 0, 0 };
      RGB brmagenta = { 51, 0, 51 };
      RGB brgreen = { 0, 51, 0 };
      RGB brcyan = { 0, 55, 55 };
      RGB bryellow = { 59, 59, 0 };
      RGB brwhite = { 63, 63, 63 };

      set_color (0, &black);
      set_color (1, &blue);
      set_color (2, &red);
      set_color (3, &magenta);
      set_color (4, &green);
      set_color (5, &cyan);
      set_color (6, &yellow);
      set_color (7, &white);
      set_color (8, &brblack);
      set_color (9, &brblue);
      set_color (10, &brred);
      set_color (11, &brmagenta);
      set_color (12, &brgreen);
      set_color (13, &brcyan);
      set_color (14, &bryellow);
      set_color (15, &brwhite);
   }
   else if (palchosen == ZXOPT)
   {
      /* optimized but not realistic Sinclair ZX palette (SPIN emu) */
      RGB black = { 0, 0, 0 };
      RGB blue = { 0, 0, 50 };
      RGB red = { 50, 0, 0 };
      RGB magenta = { 50, 0, 50 };
      RGB green = { 0, 50, 0 };
      RGB cyan = { 0, 50, 50 };
      RGB yellow = { 50, 50, 0 };
      RGB white = { 50, 50, 50 };
      /*
       old David Lodge palette:

       RGB black = { 0, 0, 0 };
       RGB blue = { 0, 0, 53 };
       RGB red = { 53, 0, 0 };
       RGB magenta = { 53, 0, 53 };
       RGB green = { 0, 53, 0 };
       RGB cyan = { 0, 53, 53 };
       RGB yellow = { 53, 53, 0 };
       RGB white = { 53, 53, 53 };
       */
      RGB brblack = { 0, 0, 0 };
      RGB brblue = { 0, 0, 63 };
      RGB brred = { 63, 0, 0 };
      RGB brmagenta = { 63, 0, 63 };
      RGB brgreen = { 0, 63, 0 };
      RGB brcyan = { 0, 63, 63 };
      RGB bryellow = { 63, 63, 0 };
      RGB brwhite = { 63, 63, 63 };

      set_color (0, &black);
      set_color (1, &blue);
      set_color (2, &red);
      set_color (3, &magenta);
      set_color (4, &green);
      set_color (5, &cyan);
      set_color (6, &yellow);
      set_color (7, &white);
      set_color (8, &brblack);
      set_color (9, &brblue);
      set_color (10, &brred);
      set_color (11, &brmagenta);
      set_color (12, &brgreen);
      set_color (13, &brcyan);
      set_color (14, &bryellow);
      set_color (15, &brwhite);
   }
   else if ((palchosen == C64A) || (palchosen == C64B))
   {
      /* and now: C64 palette (pepto/VICE) */
      RGB black = { 0, 0, 0 };
      RGB white = { 63, 63, 63 };
      RGB red = { 26, 13, 10 };
      RGB cyan = { 28, 41, 44 };
      RGB purple = { 27, 15, 33 };
      RGB green = { 22, 35, 16 };
      RGB blue = { 13, 10, 30 };
      RGB yellow = { 46, 49, 27 };
      RGB orange = { 27, 19, 9 };
      RGB brown = { 16, 14, 0 };
      RGB lred = { 38, 25, 22 };
      RGB dgrey = { 17, 17, 17 };
      RGB grey = { 27, 27, 27 };
      RGB lgreen = { 38, 52, 33 };
      RGB lblue = { 27, 23, 45 };
      RGB lgrey = { 37, 37, 37 };

      set_color (0, &black);
      set_color (1, &white);
      set_color (2, &red);
      set_color (3, &cyan);
      set_color (4, &purple);
      set_color (5, &green);
      set_color (6, &blue);
      set_color (7, &yellow);
      set_color (8, &orange);
      set_color (9, &brown);
      set_color (10, &lred);
      set_color (11, &dgrey);
      set_color (12, &grey);
      set_color (13, &lgreen);
      set_color (14, &lblue);
      set_color (15, &lgrey);
   }
}

const char *
colortext (int32_t col)
{
   const char *zxcolorname[] =
   { "black", "blue", "red", "magenta", "green", "cyan", "yellow", "white",
      "bright black", "bright blue", "bright red", "bright magenta",
      "bright green", "bright cyan", "bright yellow", "bright white", "INVALID",
   };

   const char *c64colorname[] =
   { "black", "white", "red", "cyan", "purple", "green", "blue", "yellow",
      "orange", "brown", "light red", "dark grey",
      "grey", "light green", "light blue", "light grey", "INVALID",
   };

   if ((palchosen == C64A) || (palchosen == C64B))
      return (c64colorname[col]);
   else
      return (zxcolorname[col]);
}

int32_t
remap (int32_t color)
{
   int32_t mapcol;

   if ((palchosen == ZX) || (palchosen == ZXOPT))
   {
      /* nothing to remap here; shows that the gfx were created on a ZX */
      mapcol = (((color >= 0) && (color <= 15)) ? color : INVALIDCOLOR);
   }
   else if (palchosen == C64A)
   {
      /* remap A determined from Golden Baton, applies to S1/S3/S13 too (8col) */
      int32_t c64remap[] = { 0, 6, 2, 4, 5, 3, 7, 1, 8, 1, 1, 1, 7, 12, 8, 7, };
      mapcol = (((color >= 0)
                 && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
   }
   else if (palchosen == C64B)
   {
      /* remap B determined from Spiderman (16col) */
      int32_t c64remap[] = { 0, 6, 2, 4, 5, 14, 8, 12, 0, 6, 2, 4, 5, 3, 7, 1, };
      mapcol = (((color >= 0)
                 && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
   }
   else
      mapcol = (((color >= 0) && (color <= 15)) ? color : INVALIDCOLOR);

   return (mapcol);
}

/* real code starts here */

void
flip (uint8_t character[])
{
   int32_t i, j;
   uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

   for (i = 0; i < 8; i++)
      for (j = 0; j < 8; j++)
         if ((character[i] & (1 << j)) != 0)
            work2[i] += 1 << (7 - j);
   for (i = 0; i < 8; i++)
      character[i] = work2[i];
}

void
rot90 (uint8_t character[])
{
   int32_t i, j;
   uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

   for (i = 0; i < 8; i++)
      for (j = 0; j < 8; j++)
         if ((character[j] & (1 << i)) != 0)
            work2[7 - i] += 1 << j;

   for (i = 0; i < 8; i++)
      character[i] = work2[i];
}

void
rot270 (uint8_t character[])
{
   int32_t i, j;
   uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

   for (i = 0; i < 8; i++)
      for (j = 0; j < 8; j++)
         if ((character[j] & (1 << i)) != 0)
            work2[i] += 1 << (7 - j);

   for (i = 0; i < 8; i++)
      character[i] = work2[i];
}

void
rot180 (uint8_t character[])
{
   int32_t i, j;
   uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

   for (i = 0; i < 8; i++)
      for (j = 0; j < 8; j++)
         if ((character[i] & (1 << j)) != 0)
            work2[7 - i] += 1 << (7 - j);

   for (i = 0; i < 8; i++)
      character[i] = work2[i];
}

void
transform (int32_t character, int32_t flip_mode, int32_t ptr)
{
   uint8_t work[8];
   int32_t i;

#ifdef DRAWDEBUG
   fprintf (stderr, "Plotting char: %d with flip: %02x (%s) at %d: %d,%d\n",
            character, flip_mode, flipdescription[(flip_mode & 48) >> 4], ptr, ptr % 0x20, ptr / 0x20);
#endif

   // first copy the character into work
   for (i = 0; i < 8; i++)
      work[i] = sprite[character][i];

   // Now flip it
   if ((flip_mode & 0x30) == 0x10)
   {
      rot90 (work);
      //      fprintf(stderr, "rot 90 character %d\n",character);
   }
   if ((flip_mode & 0x30) == 0x20)
   {
      rot180 (work);
      //       fprintf(stderr, "rot 180 character %d\n",character);
   }
   if ((flip_mode & 0x30) == 0x30)
   {
      rot270 (work);
      //       fprintf(stderr, "rot 270 character %d\n",character);
   }
   if ((flip_mode & 0x40) != 0)
   {
      flip (work);
      /* printf("flipping character %d\n",character); */
   }
   flip (work);

   // Now mask it onto the previous character
   for (i = 0; i < 8; i++)
   {
      if ((flip_mode & 0x0c) == 12)
         screenchars[ptr][i] ^= work[i];
      else if ((flip_mode & 0x0c) == 8)
         screenchars[ptr][i] &= work[i];
      else if ((flip_mode & 0x0c) == 4)
         screenchars[ptr][i] |= work[i];
      else
         screenchars[ptr][i] = work[i];
   }
}

void
putpixel(glsi32 x, glsi32 y, int32_t color)
{
   int y_offset = 0;

   glui32 glk_color = (4*(pal[color][0] << 16)) | (4*(pal[color][1] << 8)) | (4*pal[color][2]) ;

   glk_window_fill_rect(Graphics, glk_color,
                        x * pixel_size + x_offset,
                        y * pixel_size + y_offset,
                        pixel_size,
                        pixel_size);
}

void
rectfill (int32_t x, int32_t y, int32_t width, int32_t height, int32_t color)
{
   int y_offset = 0;


   glui32 glk_color = (4*(pal[color][0] << 16)) | (4*(pal[color][1] << 8)) | (4*pal[color][2]) ;

   glk_window_fill_rect(Graphics, glk_color,
                        x * pixel_size + x_offset,
                        y * pixel_size + y_offset,
                        width * pixel_size,
                        height * pixel_size);
}



void
background (int32_t x, int32_t y, int32_t color)
{
   /* Draw the background */
   rectfill(x * 8, y * 8, 8, 8, color);
}

void
plotsprite (int32_t character, int32_t x, int32_t y, int32_t fg, int32_t bg)
{
   int32_t i, j;
   background (x, y, bg);
   for (i = 0; i < 8; i++)
      for (j = 0; j < 8; j++)
         if ((screenchars[character][i] & (1 << j)) != 0)
            putpixel (x * 8 + j, y * 8 + i, fg);
}

//int isNthBitSet (unsigned char c, int n) {
//    static unsigned char mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
//    return ((c & mask[n]) != 0);
//}

//void draw_saga_picture(int picture_number, const char *saga_filename, const char *saga_game, const char *saga_palette, winid_t window, int pix_size, int x_off, int file_offset)
//{
//
////#define DRAWDEBUG
//
////   graphics_window = window;
//   pixel_size = pix_size;
//   x_offset = x_off;
//
//   FILE *infile;
//   //   const char filename[256];
//   int32_t ptr, cont = 0;
//   int32_t i, x, y, mask_mode;
//   int32_t CHAR_START = 0, OFFSET_TAB = 0, numgraphics;
//   int32_t TAB_OFFSET;
//   int32_t xsize, ysize, xoff, yoff;
//   int32_t starttab = 0, version;
//   uint8_t data, data2, old = 0;
//   int32_t ink[0x22][14], paper[0x22][14];
//
//   //   if (argc != 4)
//   //   {
//   //      fprintf (stderr, "usage: sagadraw filename game palette\n");
//   //      fprintf (stderr, "  e.g. sagadraw adventureland.sna s1 zxopt\n");
//   //      fprintf (stderr,
//   //          "  Accepted games: s1, s3, s10, s11, s13, s13c64, q2, q2c64, o1, o2\n");
//   //      fprintf (stderr, "  Accepted palettes: zx, zxopt, c64a, c64b, vga\n");
//   //      exit (EXIT_FAILURE);
//   //   }
//
//   do_palette (saga_palette);
//
//   if (palchosen == NONE)
//   {
//      fprintf (stderr, "unknown palette\n");
//      exit (EXIT_FAILURE);
//   }
//
////   if (!infile)
//      infile = fopen (saga_filename, "rb");
////   fprintf (stderr, "opened file %s\n", saga_filename);
//
//   TAB_OFFSET = 0x3fe5;
//   if (strcmp (saga_game, "s1") == 0)
//   {
//      CHAR_START = 0x631e; // Adventureland
//      numgraphics = 41;
//      version = 1;
//   }
//   else if (strcmp (saga_game, "s3") == 0)
//   {
//      CHAR_START = 0x625d; // Secret Mission
//      numgraphics = 44;
//      version = 1;
//   }
//   else if (strcmp (saga_game, "s13") == 0)
//   {
//      CHAR_START = 0x6007; // Sorceror of Claymorgue Castle
//      numgraphics = 38;
//      version = 2;
//   }
//   else if (strcmp (saga_game, "s13c64") == 0)
//   {
//      CHAR_START = 0x7398; // Sorceror of Claymorgue Castle (C64)
//      numgraphics = 44;
//      TAB_OFFSET = (-128);
//      version = 2;
//   }
//   else if (strcmp (saga_game, "q2") == 0)
//   {
//      CHAR_START = 0x6296; // Spiderman
//      numgraphics = 41;
//      version = 2;
//   }
//   else if (strcmp (saga_game, "q2c64") == 0)
//   {
//      CHAR_START = 0x79eb; // Spiderman (C64)
//      numgraphics = 41;
//      TAB_OFFSET = (-128);
//      version = 2;
//   }
//   else if (strcmp (saga_game, "s10") == 0)
//   {
//      CHAR_START = 0x570f; // Savage Island 1
//      numgraphics = 37;
//      TAB_OFFSET = 0;
//      version = 3;
//   }
//   else if (strcmp (saga_game, "s11") == 0)
//   {
//      CHAR_START = 0x57d0; // Savage Island 2
//      numgraphics = 21;
//      TAB_OFFSET = 0;
//      version = 3;
//   }
//   else if (strcmp (saga_game, "o1") == 0)
//   {
//      CHAR_START = 0x5a4e; // Supergran
//      numgraphics = 47;
//      TAB_OFFSET = 0;
//      version = 3;
//   }
//   else if (strcmp (saga_game, "o2") == 0)
//   {
//      CHAR_START = 0x5be1; // Gremlins
//      numgraphics = 78;
//      TAB_OFFSET = 0;
//      version = 3;
//   }
//   else if (strcmp (saga_game, "o3") == 0)
//   {
//      CHAR_START = 0x614f; // Robin of Sherwood
//      TAB_OFFSET = 0x6765;
//      numgraphics = 82;
//      version = 4;
//   }
//   else if (strcmp( saga_game, "q1" ) == 0)
//   {
//      CHAR_START = 0x281b; // Hulk
//      numgraphics = 39;
//      TAB_OFFSET = 0x741b;
//      version = 0;
//   }
//   else
//   {
//      printf ("Unknown game: %s\n", saga_game);
//      exit (EXIT_FAILURE);
//   }
//
//   CHAR_START += file_offset;
//
//   if (version < 4) OFFSET_TAB = CHAR_START + 0x800;
//
//   define_palette ();
//
//   fseek (infile, CHAR_START, SEEK_SET);
//
//#ifdef DRAWDEBUG
//   fprintf (stderr, "Grabbing Character details\n");
//   fprintf (stderr, "Character Offset: %04x\n", CHAR_START);
//#endif
//   for (i = 0; i < 256; i++)
//   {
//      for (y = 0; y < 8; y++)
//      {
//         sprite[i][y] = fgetc (infile);
//         //          fprintf (stderr, "\n%s%d DEFS ", (y == 0) ? "s" : " ", CHAR_START + i * 8 + y + 16357);
//         //          for (int n = 0; n < 8; n++) {
//         //              if (isNthBitSet(sprite[i][y], n))
//         //                  fprintf (stderr, "1");
//         //              else
//         //                  fprintf (stderr, "0");
//         //          }
//      }
//   }
//
//
//
//   // first build up the screen graphics
//#ifdef DRAWDEBUG
//   fprintf (stderr, "Working out colours details\n");
//#endif
//
//#ifdef DRAWDEBUG
//   fprintf (stderr, "Rendering picture number: %d\n", picture_number);
//#endif
//
//   // Robin version: 4
//   if (version == 0 || version == 4)
//   {
//
//      fseek(infile, 0x66BF + (picture_number * 2), SEEK_SET);
//      TAB_OFFSET += (fgetc (infile) + (fgetc (infile) * 256));
//
//      fseek(infile, TAB_OFFSET, SEEK_SET);
//      fprintf(stderr, "image offset %d (memory address %x)\n", TAB_OFFSET, TAB_OFFSET+16357);
//   }
//   else
//   {
//      fseek (infile, OFFSET_TAB + (picture_number * 2), SEEK_SET);
//      starttab = (fgetc (infile) + (fgetc (infile) * 256));
//      //         fprintf (stderr, "actual offset in file: %04x\n", starttab);
//
//   }
//#ifdef DRAWDEBUG
//   fprintf (stderr, "TAB_OFFSET: %04x\n", TAB_OFFSET);
//#endif
//   if (TAB_OFFSET)
//   {
//      starttab -= TAB_OFFSET;
//   }
//   else
//   {
//      starttab += OFFSET_TAB + 0x100;
//   }
//#ifdef DRAWDEBUG
//   fprintf (stderr, "starttab: %04x\n", starttab);
//#endif
//   if (version > 0) fseek (infile, starttab, SEEK_SET);
//
//#ifdef DRAWDEBUG
//   size_t pos = ftell(infile);
//   fprintf (stderr, "File offset %lx\n", pos);
//   fprintf (stderr, "Memory address %ld (%lx)\n", pos + 16357,  pos + 16357);
//
//#endif
//   xsize = fgetc (infile);
//   fprintf (stderr, "xsize for image %d: %d\n", picture_number, xsize);
//   ysize = fgetc (infile);
//   fprintf (stderr, "xsize for image %d: %d\n", picture_number, ysize);
//
//   if (version > 0)
//   {
//      xoff = fgetc (infile);
//      fprintf (stderr, "xoff for image %d: %d\n", picture_number, xoff);
//      yoff = fgetc (infile);
//      fprintf (stderr, "yoff for image %d: %d\n", picture_number, yoff);
//
//   }
//   else
//   {
//      xoff = yoff = 0;
//   }
//#ifdef DRAWDEBUG
//   fprintf (stderr, "Header: width: %d height: %d y: %d x: %d\n", xsize, ysize, xoff, yoff);
//#endif
//
//   ptr = 0;
//   int32_t character = 0;
//   int32_t count;
//   do
//   {
//      count = 1;
//
//      /* first handle mode */
//      data = fgetc (infile);
//      if (data < 0x80)
//      {
//         if (character > 127)
//         {
//            data+=128;
//         }
//         character = data;
//#ifdef DRAWDEBUG
//         fprintf (stderr, "******* SOLO CHARACTER: %04x\n",
//                  character);
//         fprintf (stderr, "Location: %lx\n", ftell (infile));
//#endif
//         transform (character, 0, ptr);
//         ptr++;
//         if (ptr > 767)
//            break;
//      }
//      else
//      {
//         // first check for a count
//         if ((data & 2) == 2)
//         {
//            count = fgetc (infile) + 1;
//         }
//
//         // Next get character and plot (count) times
//         character = fgetc (infile);
//
//         // Plot the initial character
//         if ((data & 1) == 1 && character < 128)
//            character += 128;
//
//#ifdef DRAWDEBUG
//         fprintf (stderr, "Location: %lx\n", ftell (infile));
//#endif
//         for (i = 0; i < count; i++)
//            transform (character, (data & 0x0c) ? (data & 0xf3) : data,
//                       ptr + i);
//
//         // Now check for overlays
//         if ((data & 0xc) != 0)
//         {
//            // We have overlays so grab each member of the stream and work out what to
//            // do with it
//
//            mask_mode = (data & 0xc);
//            data2 = fgetc (infile);
//            old = data;
//            do
//            {
//               cont = 0;
//               if (data2 < 0x80)
//               {
//                  //if ((old & 1) == 1)
//                  //data2 += 128;
//#ifdef DRAWDEBUG
//                  fprintf (stderr,
//                           "Plotting %d directly (overlay) at %d\n",
//                           data2, ptr);
//                  fprintf (stderr, "Location: %lx\n",
//                           ftell (infile));
//#endif
//                  for (i = 0; i < count; i++)
//                     transform (data2, old & 0x0c, ptr + i);
//               }
//               else
//               {
//                  character = fgetc (infile);
//                  if ((data2 & 1) == 1)
//                     character += 128;
//#ifdef DRAWDEBUG
//                  fprintf (stderr,
//                           "Plotting %d with flip %02x (%s) at %d %d\n",
//                           character, (data2 | mask_mode), flipdescription[((data2 | mask_mode) & 48) >> 4], ptr, count);
//                  fprintf (stderr, "Location: %lx\n",
//                           ftell (infile));
//#endif
//                  for (i = 0; i < count; i++)
//                     transform (character, (data2 & 0xf3) | mask_mode,
//                                ptr + i);
//                  if ((data2 & 0x0c) != 0)
//                  {
//                     mask_mode = (data2 & 0x0c);
//                     old = data2;
//                     cont = 1;
//                     data2 = fgetc (infile);
//                  }
//               }
//            }
//            while (cont != 0);
//         }
//         ptr += count;
//      }
//   }
//   while (ptr < xsize * ysize);
//
//#ifdef DRAWDEBUG
//   fprintf (stderr, "Start of colours: %04lx\n", ftell (infile));
//#endif
//   y = 0;
//   x = 0;
//
//   int32_t colour = 0;
////   int32_t count;
//   // Note version 3 count is inverse it is repeat previous colour
//   // Whilst version0-2 count is repeat next character
//   while (y < ysize)
//   {
//      data = fgetc (infile);
//      if ((data & 0x80))
//      {
//         count = (data & 0x7f) + 1;
//         if (version >= 3)
//         {
//            count--;
//         }
//         else
//         {
//            colour = fgetc (infile);
//         }
//      }
//      else
//      {
//         count = 1;
//         colour = data;
//      }
//      while (count > 0)
//      {
//
//         // Now split up depending on which version we're using
//
//         // For version 3+
//
//         if (version > 2)
//         {
//            // ink is 0-2, screen is 3-5, 6 in bright flag
//            ink[x][y] = colour & 0x07;
//            paper[x][y] = (colour & 0x38) >> 3;
//
//            if ((colour & 0x40) == 0x40)
//            {
//               paper[x][y] += 8;
//               ink[x][y] += 8;
//            }
//         }
//         else
//         {
//            paper[x][y] = colour & 0x07;
//            ink[x][y] = ((colour & 0x70) >> 4);
//
//            if ((colour & 0x08) == 0x08 || version < 2)
//            {
//               paper[x][y] += 8;
//               ink[x][y] += 8;
//            }
//         }
//
//         x++;
//         if (x == xsize)
//         {
//            x = 0;
//            y++;
//         }
//         count--;
//      }
//   }
//   ptr = 0;
//   int32_t xoff2;
//   for (y = 0; y < ysize; y++)
//      for (x = 0; x < xsize; x++)
//      {
//         xoff2 = xoff;
//         if (version>0 && version < 3)
//            xoff2 = xoff - 4;
//
//         plotsprite(ptr, x + xoff2, y + yoff, remap (ink[x][y]),
//                     remap (paper[x][y]));
//
//#ifdef DRAWDEBUG
//         fprintf (stderr, "(gfx#%02d:plotting %d,%d:paper=%s,ink=%s)\n",
//                  picture_number, x + xoff2, y + yoff,
//                  colortext (remap (paper[x][y])),
//                  colortext (remap (ink[x][y])));
//#endif
//         ptr++;
//         if (ptr > 766)
//            break;
////         {
////            fprintf (stderr, "SagaDraw: Too far gone. Bailing\n");
////            fclose (infile);
////            return;
////         }
//
//#ifdef DRAWDEBUG
//         long real_offset = ftell(infile) + 16357;
//         fprintf (stderr, "Real End Offset: %ld (%lx)\n", real_offset, real_offset);
//
//         fprintf (stderr, "End Offset: %ld (%lx)\n", real_offset - 16357, real_offset - 16357);
//         //       int real_offset = OFFSET_TAB + (fgetc (infile) + (fgetc (infile) * 256)) + 16357;
//#endif
//
////               rectfill (0, 0, 255, 95, 0);
//      }
//   fclose (infile);
//#ifdef DRAWDEBUG
////   fclose (stderr);
//#endif
//   return;
//}

void saga_setup(const char *saga_filename, const char *saga_game, int file_offset, const char *saga_palette) {
   int32_t i, y;

   do_palette (saga_palette);

   if (palchosen == NONE)
   {
      fprintf (stderr, "unknown palette\n");
      exit (EXIT_FAILURE);
   }

   define_palette ();

   int32_t CHAR_START = 0, OFFSET_TAB = 0, numgraphics;
   int32_t TAB_OFFSET;

   //   if (argc != 4)
   //   {
   //      fprintf (stderr, "usage: sagadraw filename game palette\n");
   //      fprintf (stderr, "  e.g. sagadraw adventureland.sna s1 zxopt\n");
   //      fprintf (stderr,
   //          "  Accepted games: s1, s3, s10, s11, s13, s13c64, q2, q2c64, o1, o2\n");
   //      fprintf (stderr, "  Accepted palettes: zx, zxopt, c64a, c64b, vga\n");
   //      exit (EXIT_FAILURE);
   //   }

   FILE *infile = fopen (saga_filename, "rb");
   //   fprintf (stderr, "opened file %s\n", saga_filename);

   TAB_OFFSET = 0x3fe5;
   if (strcmp (saga_game, "s1") == 0)
   {
      CHAR_START = 0x631e; // Adventureland
      numgraphics = 41;
      version = 1;
   }
   else if (strcmp (saga_game, "s3") == 0)
   {
      CHAR_START = 0x625d; // Secret Mission
      numgraphics = 44;
      version = 1;
   }
   else if (strcmp (saga_game, "s13") == 0)
   {
      CHAR_START = 0x6007; // Sorceror of Claymorgue Castle
      numgraphics = 38;
      version = 2;
   }
   else if (strcmp (saga_game, "s13c64") == 0)
   {
      CHAR_START = 0x7398; // Sorceror of Claymorgue Castle (C64)
      numgraphics = 44;
      TAB_OFFSET = (-128);
      version = 2;
   }
   else if (strcmp (saga_game, "q2") == 0)
   {
      CHAR_START = 0x6296; // Spiderman
      numgraphics = 41;
      version = 2;
   }
   else if (strcmp (saga_game, "q2c64") == 0)
   {
      CHAR_START = 0x79eb; // Spiderman (C64)
      numgraphics = 41;
      TAB_OFFSET = (-128);
      version = 2;
   }
   else if (strcmp (saga_game, "s10") == 0)
   {
      CHAR_START = 0x570f; // Savage Island 1
      numgraphics = 37;
      TAB_OFFSET = 0;
      version = 3;
   }
   else if (strcmp (saga_game, "s11") == 0)
   {
      CHAR_START = 0x57d0; // Savage Island 2
      numgraphics = 21;
      TAB_OFFSET = 0;
      version = 3;
   }
   else if (strcmp (saga_game, "o1") == 0)
   {
      CHAR_START = 0x5a4e; // Supergran
      numgraphics = 47;
      TAB_OFFSET = 0;
      version = 3;
   }
   else if (strcmp (saga_game, "o2") == 0)
   {
      CHAR_START = 0x5be1; // Gremlins
      numgraphics = 78;
      TAB_OFFSET = 0;
      version = 3;
   }
   else if (strcmp (saga_game, "o3") == 0)
   {
      CHAR_START = 0x614f; // Robin of Sherwood
      TAB_OFFSET = 0x6765;
      numgraphics = 83;
      version = 4;
   }
   else if (strcmp( saga_game, "q1" ) == 0)
   {
      CHAR_START = 0x281b; // Hulk
      numgraphics = 39;
      TAB_OFFSET = 0x741b;
      version = 0;
   }
   else
   {
      printf ("Unknown game: %s\n", saga_game);
      exit (EXIT_FAILURE);
   }

   CHAR_START += file_offset;

   if (version < 4) OFFSET_TAB = CHAR_START + 0x800;

   images = (Image *)MemAlloc(sizeof(Image)*numgraphics);
   Image *img = images;

   uint16_t *image_offsets = MemAlloc(sizeof(uint16_t)*numgraphics);

   int32_t starttab = 0;
   uint16_t data_length = 0;

   fseek(infile, 0, SEEK_END);
   uint16_t file_length = ftell(infile);

   fseek (infile, 0x66bf + file_offset, SEEK_SET);

   size_t result = fread(image_offsets, sizeof(uint16_t), numgraphics, infile);
//   fprintf (stderr, "Read %zu bytes of image offset data\n", result);
//   if (result < numgraphics) {
//      fprintf (stderr, "This is %zu bytes less than the expected %d\n", numgraphics - result, numgraphics);
//   }

   for (int picture_number = 0; picture_number < numgraphics; picture_number++) {
//      fprintf(stderr, "Offset of image %d: %d\n", picture_number, image_offsets[picture_number]);
   }

   starttab = ftell(infile);
//   fprintf(stderr, "File offset after reading image offset data: %d (%x)\n", starttab, starttab);


   fseek (infile, CHAR_START, SEEK_SET);

   #ifdef DRAWDEBUG
   fprintf (stderr, "Grabbing Character details\n");
   fprintf (stderr, "Character Offset: %04x\n", CHAR_START);
   #endif
   for (i = 0; i < 256; i++)
   {
      for (y = 0; y < 8; y++)
      {
         sprite[i][y] = fgetc (infile);
      }
   }

   starttab = ftell(infile);
//   fprintf(stderr, "File offset after reading char data: %d (%x)\n", starttab, starttab);

   int w = fgetc(infile);
   int h = fgetc(infile);

   while (w != 32 || h != 12) {
      starttab--;
      fseek (infile, starttab, SEEK_SET);
      w = fgetc(infile);
      h = fgetc(infile);
   }

//   fprintf(stderr, "Final starttab %d (%x)\n", starttab, starttab);

   for (int picture_number = 0; picture_number < numgraphics; picture_number++) {

      // Robin version: 4



      //         TAB_OFFSET += image_offsets[picture_number];
      //         fseek(infile, TAB_OFFSET, SEEK_SET);


      //         fprintf(stderr, "image offset %d (memory address %x)\n", TAB_OFFSET, TAB_OFFSET+16357);


      if (picture_number == numgraphics - 1)
      {
         int currentpos = ftell(infile);

         data_length = file_length - currentpos;
//         fprintf (stderr, "This is the last image, so we calculate length as total file length %hu (%hx) - current file position %d(%x) = %hu (%hx)\n", file_length, file_length, currentpos, currentpos, data_length, data_length);
      } else {
         data_length = image_offsets[picture_number + 1] - image_offsets[picture_number];
      }

      //      if (TAB_OFFSET)
      //      {
      //         starttab += image_offsets[picture_number];
      //      }
      //      else
      //      {
      //         starttab += OFFSET_TAB + 0x100;
      //      }
      //#ifdef DRAWDEBUG
      //#endif
      //      if (version > 0)
      fseek (infile, TAB_OFFSET + image_offsets[picture_number] + file_offset, SEEK_SET);



      //      fseek (infile, CHAR_START + , SEEK_SET);

      img->width = fgetc (infile);
//      fprintf (stderr, "width of image %d: %d\n", picture_number, img->width);

      img->height = fgetc (infile);
//      fprintf (stderr, "height of image %d: %d\n", picture_number, img->height);


      data_length -=2;

      if (version > 0)
      {
         img->xoff = fgetc (infile);
//         fprintf (stderr, "xoff of image %d: %d\n", picture_number, img->xoff);

         img->yoff = fgetc (infile);
//         fprintf (stderr, "yoff of image %d: %d\n", picture_number, img->yoff);
         data_length -=2;
      }
      else
      {
         img->xoff = img->yoff = 0;
      }

      img->imagedata = MemAlloc(data_length);
      size_t result = fread(img->imagedata, sizeof(uint8_t), data_length, infile);
//      fprintf (stderr, "Read %zu bytes of image data for image %d\n", result, picture_number);
      if (result < data_length) {
         fprintf (stderr, "This is %zu bytes less than the expected %d\n", data_length - result, data_length);
      }

      img++;
   }

   if (strcmp (saga_game, "o3") == 0) {
      int cells = 555;
      forest_images = MemAlloc(cells);
      fseek(infile, 0x7b53 - 16357 + file_offset, SEEK_SET);
      result = fread(forest_images, 1, cells, infile);
//      fprintf (stderr, "Read %zu bytes of forest image special data\n", result);
   }
}

void draw_saga_picture_from_data(uint8_t *dataptr, int xsize, int ysize, int xoff, int yoff) {
   int32_t offset = 0, cont = 0;
   int32_t i, x, y, mask_mode;
   uint8_t data, data2, old = 0;
   int32_t ink[0x22][14], paper[0x22][14];

   offset = 0;
   int32_t character = 0;
   int32_t count;
   do
   {
      count = 1;

      /* first handle mode */
      uint8_t data = *dataptr++;
      if (data < 0x80)
      {
         if (character > 127)
         {
            data+=128;
         }
         character = data;
#ifdef DRAWDEBUG
         fprintf (stderr, "******* SOLO CHARACTER: %04x\n",
                  character);
#endif
         transform (character, 0, offset);
         offset++;
         if (offset > 767)
            break;
      }
      else
      {
         // first check for a count
         if ((data & 2) == 2)
         {
            count = (*dataptr++) + 1;
         }

         // Next get character and plot (count) times
         character = *dataptr++;

         // Plot the initial character
         if ((data & 1) == 1 && character < 128)
            character += 128;

         for (i = 0; i < count; i++)
            transform (character, (data & 0x0c) ? (data & 0xf3) : data,
                       offset + i);

         // Now check for overlays
         if ((data & 0xc) != 0)
         {
            // We have overlays so grab each member of the stream and work out what to
            // do with it

            mask_mode = (data & 0xc);
            data2 = *dataptr++;
            old = data;
            do
            {
               cont = 0;
               if (data2 < 0x80)
               {
                  if (version == 4 && (old & 1) == 1)
                     data2 += 128;
#ifdef DRAWDEBUG
                  fprintf (stderr,
                           "Plotting %d directly (overlay) at %d\n",
                           data2, offset);
                  //                  fprintf (stderr, "Location: %lx\n",
                  //                           ftell (infile));
#endif
                  for (i = 0; i < count; i++)
                     transform (data2, old & 0x0c, offset + i);
               }
               else
               {
                  character = *dataptr++;
                  if ((data2 & 1) == 1)
                     character += 128;
#ifdef DRAWDEBUG
                  fprintf (stderr,
                           "Plotting %d with flip %02x (%s) at %d %d\n",
                           character, (data2 | mask_mode), flipdescription[((data2 | mask_mode) & 48) >> 4], offset, count);
                  //                  fprintf (stderr, "Location: %lx\n",
                  //                           ftell (infile));
#endif
                  for (i = 0; i < count; i++)
                     transform (character, (data2 & 0xf3) | mask_mode,
                                offset + i);
                  if ((data2 & 0x0c) != 0)
                  {
                     mask_mode = (data2 & 0x0c);
                     old = data2;
                     cont = 1;
                     data2 = *dataptr++;
                  }
               }
            }
            while (cont != 0);
         }
         offset += count;
      }
   }
   while (offset < xsize * ysize);

#ifdef DRAWDEBUG
   //   fprintf (stderr, "Start of colours: %04lx\n", ftell (infile));
#endif
   y = 0;
   x = 0;

   uint8_t colour = 0;
   //   int32_t count;
   // Note version 3 count is inverse it is repeat previous colour
   // Whilst version0-2 count is repeat next character
   while (y < ysize)
   {
      data = *dataptr++;
      if ((data & 0x80))
      {
         count = (data & 0x7f) + 1;
         if (version >= 3)
         {
            count--;
         }
         else
         {
            colour = *dataptr++;
         }
      }
      else
      {
         count = 1;
         colour = data;
      }
      while (count > 0)
      {

         // Now split up depending on which version we're using

         // For version 3+

         if (version > 2)
         {
            // ink is 0-2, screen is 3-5, 6 in bright flag
            ink[x][y] = colour & 0x07;
            paper[x][y] = (colour & 0x38) >> 3;

            if ((colour & 0x40) == 0x40)
            {
               paper[x][y] += 8;
               ink[x][y] += 8;
            }
         }
         else
         {
            paper[x][y] = colour & 0x07;
            ink[x][y] = ((colour & 0x70) >> 4);

            if ((colour & 0x08) == 0x08 || version < 2)
            {
               paper[x][y] += 8;
               ink[x][y] += 8;
            }
         }

         x++;
         if (x == xsize)
         {
            x = 0;
            y++;
         }
         count--;
      }
   }
   offset = 0;
   int32_t xoff2;
   for (y = 0; y < ysize; y++)
      for (x = 0; x < xsize; x++)
      {
         xoff2 = xoff;
         if (version>0 && version < 3)
            xoff2 = xoff - 4;

         plotsprite(offset, x + xoff2, y + yoff, remap (ink[x][y]),
                    remap (paper[x][y]));

#ifdef DRAWDEBUG
         fprintf (stderr, "(gfx#:plotting %d,%d:paper=%s,ink=%s)\n",
                  x + xoff2, y + yoff,
                  colortext (remap (paper[x][y])),
                  colortext (remap (ink[x][y])));
#endif
         offset++;
         if (offset > 766)
            break;
      }
}

void draw_saga_picture_number(int picture_number) {

   Image img = images[picture_number];

   draw_saga_picture_from_data(img.imagedata, img.width, img.height, img.xoff, img.yoff);
}

void draw_saga_picture_at_pos(int picture_number, int x, int y) {
   Image img = images[picture_number];

   draw_saga_picture_from_data(img.imagedata, img.width, img.height, x, y);
}


#define TREES 0
#define BUSHES 1

void draw_sherwood(int loc) {
   int subimage_index = 0;

   for (int i = 0; i < loc - 11; i++)
   {
      // BUSHES type images are made up of 5 smaller images
      int skip = 5;
      if (forest_images[subimage_index] < 128)
      // Those with bit 7 unset have 10 (trees only) or 11 (trees with path)
         skip = 11;
      subimage_index += skip;
   }

   int forest_type = TREES;
   int subimages;

   // if bit 7 of the image index is set then this image is BUSHES
   if (forest_images[subimage_index] >= 128) {
      forest_type = BUSHES;
      subimages = 5;
      // if the last subimage value is 255, there is no path
   } else if (forest_images[subimage_index + 10] == 0xff) {
      // Trees only
      subimages = 10;
   } else {
      // Trees with path
      subimages = 11;
   }

   int xpos = 0, ypos = 0, image_number;

   for (int i = 0; i < subimages; i++) {
      if (forest_type == TREES) {
         if (i >= 8) {
            if (i == 8) { // Undergrowth
               ypos = 7;
               xpos = 0;
            } else if (i == 9) { // Bottom path
               ypos = 10;
               xpos = 0;
            } else { // Forward path
               ypos = 7;
               xpos = 12;
            }
         } else { // Trees (every tree image is 4 characters wide)
            ypos = 0;
            xpos = i * 4;
         }
      }

      image_number = forest_images[subimage_index++] & 127;

      draw_saga_picture_at_pos(image_number, xpos, ypos);

      if (forest_type == BUSHES) {
         xpos += images[image_number].width;
      }
   }
}

void animate_waterfall(int stage) {
   glk_request_timer_events(14);
   rectfill (88, 16, 48, 64, 15);
   for (int line = 2; line < 10; line++) {
      for (int col = 11; col < 17; col++) {
         for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
               if ((screenchars[col + line * 32][i] & (1 << j)) != 0) {
                  int ypos = line * 8 + i + stage;
                  if (ypos > 79)
                     ypos = ypos - 64;
                  putpixel (col * 8 + j, ypos, 9);
               }
      }
   }
}

void animate_waterfall_cave(int stage) {
   glk_request_timer_events(10);
   rectfill (248, 24, 8, 64, 15);
   for (int line = 3; line < 11; line++) {
         for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
               if ((screenchars[31 + line * 32][i] & (1 << j)) != 0) {
                  int ypos = line * 8 + i + stage;
                  if (ypos > 87)
                     ypos = ypos - 64;
                  putpixel (248 + j, ypos, 9);
               }
   }
}

void switch_palettes(int pal1, int pal2) {
   uint8_t temp[3];

   temp[0] = pal[pal1][0];
   temp[1] = pal[pal1][1];
   temp[2] = pal[pal1][2];

   pal[pal1][0] = pal[pal2][0];
   pal[pal1][1] = pal[pal2][1];
   pal[pal1][2] = pal[pal2][2];

   pal[pal2][0] = temp[0];
   pal[pal2][1] = temp[1];
   pal[pal2][2] = temp[2];
}

void animate_lightning(int stage) {
   // swich blue and bright yellow
   switch_palettes(1,14);
   switch_palettes(9,6);
   draw_saga_picture_number(77);
   if (stage == 11) {
      glk_request_timer_events(0);
   } else if (stage == 3) {
      glk_request_timer_events(700);
   } else {
      glk_request_timer_events(20);
   }
}



