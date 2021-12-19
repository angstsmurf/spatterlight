/* Sagadraw v2.5
 Originally by David Lodge

 With help from Paul David Doherty (including the colour code)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"

#include "definitions.h"
#include "scott.h"

#include "sagadraw.h"

extern uint8_t *seek_to_pos(uint8_t *buf, int offset);

int draw_to_buffer = 0;

uint8_t sprite[256][8];
uint8_t screenchars[768][8];
uint8_t buffer[384][9];



Image *images = NULL;

uint8_t *blood_image_data = NULL;

int pixel_size;
int x_offset;


/* palette handler stuff starts here */

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

PALETTE pal;

int32_t errorcount = 0;

palette_type palchosen = NO_PALETTE;

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
#ifdef DRAWDEBUG
      fprintf (stderr, "# x out of range: %d\n", x);
#endif
      errorcount++;
   }
   if ((y < 96) || (y > 191))
   {
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
      /* fprintf("flipping character %d\n",character); */
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

   int bufferpos = (y / 8) * 32 + (x / 8);
   buffer[bufferpos][8] = buffer[bufferpos][8] | (color << 3);

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
   for (i = 0; i < 8; i++) {
      for (j = 0; j < 8; j++)
         if ((screenchars[character][i] & (1 << j)) != 0)
            putpixel (x * 8 + j, y * 8 + i, fg);
   }
}


int isNthBitSet (unsigned const char c, int n) {
    static unsigned const char mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
    return ((c & mask[n]) != 0);
}
uint8_t *draw_saga_picture_from_data(uint8_t *dataptr, int xsize, int ysize, int xoff, int yoff);

void HitEnter(void);

void saga_setup() {
   int32_t i, y;

   uint16_t image_offsets[GameInfo->number_of_pictures];

   if (palchosen == NO_PALETTE) {
      palchosen = GameInfo->palette;
   }

   if (palchosen == NO_PALETTE)
   {
      fprintf (stderr, "unknown palette\n");
      exit (EXIT_FAILURE);
   }

   define_palette();

   int version = GameInfo->picture_format_version;

   int32_t CHAR_START = GameInfo->start_of_characters + file_baseline_offset;
   int32_t OFFSET_TABLE_START;

   OFFSET_TABLE_START = GameInfo->start_of_image_data + file_baseline_offset;

   if (GameInfo->start_of_image_data == FOLLOWS) {
      OFFSET_TABLE_START = CHAR_START + 0x800;
   }

   int32_t DATA_OFFSET = GameInfo->image_address_offset + file_baseline_offset;

   int numgraphics = GameInfo->number_of_pictures;

   uint8_t *pos = seek_to_pos(entire_file, CHAR_START);

   #ifdef DRAWDEBUG
   fprintf (stderr, "Grabbing Character details\n");
   fprintf (stderr, "Character Offset: %04x\n", CHAR_START);
   #endif
   for (i = 0; i < 256; i++)
   {
//      fprintf (stderr, "Character %d:\n", i);
      for (y = 0; y < 8; y++)
      {
         sprite[i][y] = *(pos++);
//         fprintf (stderr, "\n%s$%04X DEFS ", (y == 0) ? "s" : " ", CHAR_START + i * 8 + y + 16357);
//         for (int n = 0; n < 8; n++) {
//            if (isNthBitSet(sprite[i][y], n))
//               fprintf (stderr, "■");
//            else
//               fprintf (stderr, "0");
//         }
      }
   }


//   fprintf(stderr, "File offset after reading char data: %d (%x)\n", starttab, starttab);

   /* Now we have hopefully read the character data */
   /* Time for the image offsets */

   images = (Image *)MemAlloc(sizeof(Image)*numgraphics);
   Image *img = images;

   pos = seek_to_pos(entire_file, OFFSET_TABLE_START);

   for (int i = 0; i < numgraphics; i++) {
      if (GameInfo->picture_format_version == 0) {
         uint16_t address;

         if (i < 11) {
            address = 0x2782 + (i * 2);
         } else if (i < 28) {
            address = 0x2798 + (i - 10) * 2;
         } else if (i < 34) {
            address = 0x27bc + (i - 28) * 2;
         } else {
            address = 0x276e + (i - 34) * 2;
         }

         address += file_baseline_offset;
         address = entire_file[address] + entire_file[address + 1] * 0x100;

         image_offsets[i] = address + 0x441b;
      } else {
         image_offsets[i] = *(pos++);
         image_offsets[i] += *(pos++) * 0x100;
      }
//      fprintf(stderr, "image_offsets[%d]: %d (%x) (%x)\n", i, image_offsets[i], image_offsets[i], image_offsets[i] + 0x3fe5);
   }

//   fprintf(stderr, "File offset after reading image offset data: %d (%x)\n", starttab, starttab);

   for (int picture_number = 0; picture_number < numgraphics; picture_number++) {

      pos = seek_to_pos(entire_file, image_offsets[picture_number] + DATA_OFFSET);

      img->width = *(pos++);
//         fprintf (stderr, "width of image %d: %d\n", picture_number, img->width);

      img->height = *(pos++);
//         fprintf (stderr, "height of image %d: %d\n", picture_number, img->height);

      if (version > 0)
      {
         img->xoff = *(pos++);
         img->yoff = *(pos++);
      }
      else
      {
         if (picture_number > 9 && picture_number < 28) {
            img->xoff = entire_file[0x66C0 - 0x3fe5 + picture_number - 10 + file_baseline_offset];
            img->yoff = entire_file[0x66D2 - 0x3fe5 + picture_number - 10 + file_baseline_offset];
         } else {
            img->xoff = img->yoff = 0;
         }
      }

      img->imagedata = pos;
      img++;
   }
}


void debugdrawcharacter(int character) {
   fprintf(stderr, "Contents of character %d of 256:\n", character);
   for (int row = 0; row < 8; row++) {
      for (int n = 0; n < 8; n++) {
         if (isNthBitSet(sprite[character][row], n))
            fprintf (stderr, "■");
         else
            fprintf (stderr, "0");
      }
      fprintf (stderr, "\n");
   }
   if (character != 255)
      debugdrawcharacter(255);
}


void debugdraw(int on, int character, int xoff, int yoff, int width) {
   if (on) {
      int x = character % width;
      int y = character / width;
      plotsprite(character, x + xoff, y + yoff, 0,
                 15);
      fprintf(stderr, "Contents of character position %d:\n", character);
      for (int row = 0; row < 8; row++) {
            for (int n = 0; n < 8; n++) {
               if (isNthBitSet(screenchars[character][row], n))
                  fprintf (stderr, "■");
               else
                  fprintf (stderr, "0");
            }
         fprintf (stderr, "\n");

      }
   }
}

uint8_t *draw_saga_picture_from_data(uint8_t *dataptr, int xsize, int ysize, int xoff, int yoff) {
   int32_t offset = 0, cont = 0;
   int32_t i, x, y, mask_mode;
   uint8_t data, data2, old = 0;
   int32_t ink[0x22][14], paper[0x22][14];

   int version = GameInfo->picture_format_version;

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
         if (character > 127 && version > 0)
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

   y = 0;
   x = 0;

   uint8_t colour = 0;
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

         if (draw_to_buffer)
            buffer[(yoff + y) * 32 + (xoff + x)][8] = colour;
         else {
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

         if (draw_to_buffer) {
            for (int i = 0; i < 8; i++)
               buffer[(y + yoff) * 32 + x + xoff2][i] = screenchars[offset][i];
         } else {
            plotsprite(offset, x + xoff2, y + yoff, remap (ink[x][y]),
                       remap (paper[x][y]));
         }

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
   return dataptr;
}



void draw_saga_picture_number(int picture_number) {
//   fprintf(stderr, "draw_saga_picture_number:%d\n", picture_number);

   int numgraphics = GameInfo->number_of_pictures;
   if (picture_number >= numgraphics) {
      fprintf(stderr, "Invalid image number %d! Last image:%d\n", picture_number, numgraphics - 1);
      return;
   }
   Image img = images[picture_number];

   draw_saga_picture_from_data(img.imagedata, img.width, img.height, img.xoff, img.yoff);
}

void draw_saga_picture_at_pos(int picture_number, int x, int y) {
//   fprintf(stderr, "Drawing image %d at x: %d y: %d\n", picture_number, x, y);
   Image img = images[picture_number];

   draw_saga_picture_from_data(img.imagedata, img.width, img.height, x, y);
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

void draw_image_from_buffer(void) {
   for (int line = 0; line < 12; line++) {
      for (int col = 0; col < 32; col++) {

         uint8_t colour = buffer[col + line * 32][8];

         int paper = (colour >> 3) & 0x7;
         paper += 8 * ((colour & 0x40) == 0x40);
         int ink = (colour & 0x7);
         ink += 8 * ((colour & 0x40) == 0x40);

         background(col, line, paper);

         for (int i = 0; i < 8; i++) {
            if (buffer[col + line * 32][i] == 0)
               continue;
            if (buffer[col + line * 32][i] == 255) {

               glui32 glk_color = (4*(pal[ink][0] << 16)) | (4*(pal[ink][1] << 8)) | (4*pal[ink][2]) ;

               glk_window_fill_rect(Graphics, glk_color,
                                    col * 8 * pixel_size + x_offset,
                                    (line * 8 + i) * pixel_size,
                                    8 * pixel_size,
                                    pixel_size);
               continue;
            }
            for (int j = 0; j < 8; j++)
               if ((buffer[col + line * 32][i] & (1 << j)) != 0) {
                  int ypos = line * 8 + i;
                  putpixel (col * 8 + j, ypos, ink);
               }
         }
      }
   }
}
