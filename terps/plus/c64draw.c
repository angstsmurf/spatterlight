/* Routine to draw the C64 RLE graphics

 Code by David Lodge 29/04/2005
 */
#include <stdio.h>
#include <stdlib.h>

#include "glk.h"

extern int x, y, count;
extern int xlen, ylen;
extern int xoff, yoff;
extern int size;

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

extern PALETTE pal;

RGB colours[255];
RGB wibble;
int countflag;
extern FILE *infile,*outfile;


extern winid_t Graphics;

extern int pixel_size;
extern int x_offset, y_offset, right_margin;

static void PutPixel(glsi32 x, glsi32 y, int32_t color)
{
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glsi32 xpos = x * pixel_size + x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    glk_window_fill_rect(Graphics, glk_color, xpos,
                         y * pixel_size + y_offset, pixel_size, pixel_size);
}

//void setuppalette(void)
//{
//    int i;
//    FILE *palfile;
//    palfile=fopen("default.act","rb");
//
//    if (palfile == NULL)
//    {
//        fprintf(stderr, "Could not open palette file\n");
//        exit(1);
//    }
//
//    for (i=0;i<256;i++)
//    {
//        colours[i].r=fgetc(palfile)/4;
//        colours[i].g=fgetc(palfile)/4;
//        colours[i].b=fgetc(palfile)/4;
//    }
//    fclose(palfile);
//}



void drawpixels(int pattern, int pattern2)
{
    int pix1,pix2,pix3,pix4;

    //fprintf(stderr, "Plotting at %d %d: %x %x\n",x,y,pattern,pattern2);
    // Now get colours
    //if (x>(xlen)*8)
    //  return;
    pix1=(pattern & 0xc0)>>6;
    pix2=(pattern & 0x30)>>4;
    pix3=(pattern & 0x0c)>>2;
    pix4=(pattern & 0x03);

    PutPixel(x,y, pix1); x++;
    PutPixel(x,y, pix1); x++;
    PutPixel(x,y, pix2); x++;
    PutPixel(x,y, pix2); x++;
    PutPixel(x,y, pix3); x++;
    PutPixel(x,y, pix3); x++;
    PutPixel(x,y, pix4); x++;
    PutPixel(x,y, pix4); x++;
    y++;
    x-=8;

    pix1=(pattern2 & 0xc0)>>6;
    pix2=(pattern2 & 0x30)>>4;
    pix3=(pattern2 & 0x0c)>>2;
    pix4=(pattern2 & 0x03);

    PutPixel(x,y, pix1); x++;
    PutPixel(x,y, pix1); x++;
    PutPixel(x,y, pix2); x++;
    PutPixel(x,y, pix2); x++;
    PutPixel(x,y, pix3); x++;
    PutPixel(x,y, pix3); x++;
    PutPixel(x,y, pix4); x++;
    PutPixel(x,y, pix4); x++;
    y++;
    x-=8;

    if (y>ylen)
    {
        x+=8;
        y=yoff;
    }
}

static void set_color(int32_t index, RGB *colour)
{
    pal[index][0] = (*colour)[0];
    pal[index][1] = (*colour)[1];
    pal[index][2] = (*colour)[2];
}



int DrawImageFromData(uint8_t *data)
{
    int work,work2, outpic=0;
    int c,count;
    int i,j;
    char filename[256];
    char *ptr;
    int readcount=0;
    int linelen;
    int rawoffset;
    int offset;
    int curpos;
    FILE *tabptr;
    PALETTE pal;
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

    /* set up the palette */
    set_color(0, &black);
    set_color(1, &white);
    set_color(2, &red);
    set_color(3, &cyan);
    set_color(4, &purple);
    set_color(5, &green);
    set_color(6, &blue);
    set_color(7, &yellow);
    set_color(8, &orange);
    set_color(9, &brown);
    set_color(10, &lred);
    set_color(11, &dgrey);
    set_color(12, &grey);
    set_color(13, &lgreen);
    set_color(14, &lblue);
    set_color(15, &lgrey);

//    allegro_init();
//    install_keyboard();
//    savescr=create_bitmap(280,78);
    /* set up the palette */
    //for (i=0;i<256;i++)
    //{
    //   colours[i].r=0; colours[i].g=0;colours[i].b=0;
    //}
    //setuppalette();

//    infile=fopen(argv[1],"rb");
//    tabptr=fopen(argv[1],"rb");
//    offset=strtol(argv[2],NULL,0);
//    count=strtol(argv[3],NULL,0);
    //if (strcmp(argv[4],"y")==0) countflag=1;
    //fprintf(stderr, "%d\n",countflag);

    // Get the size of the graphics chuck
    fseek(tabptr, offset, SEEK_SET);
    work=fgetc(tabptr);
    size=work+(fgetc(tabptr)*256);
    size+=0x80;
    fprintf(stderr, "Size: %4x\n",size);

    // Now loop round for each image


//    int DrawImageFromFile(char *filename) {

        fprintf(stderr, "\nRendering Image %d\n",outpic);
        x=0;y=0;
        fseek(infile, size, SEEK_SET);
        work=fgetc(tabptr);
        size=work+(fgetc(tabptr)*256);
        size+=0x80;
        fprintf(stderr, "Next size: %4x\n",size);

        // Get the offset
        xoff=fgetc(infile)-3;
        if (xoff<0) xoff=8;
        yoff=fgetc(infile);
        x=xoff*8;
        y=yoff;
        fprintf(stderr, "xoff: %d yoff: %d\n",xoff,yoff);

        // Get the x length
        xlen=fgetc(infile);
        ylen=fgetc(infile);
        if (countflag) ylen-=2;
        fprintf(stderr, "xlen: %x ylen: %x\n",xlen, ylen);

        // Get the palette
        fprintf(stderr, "Colours: ");
        for (i=0;i<4;i++)
        {
            work=fgetc(infile);
            set_color(i,&colours[work]);
            set_color(i,&colours[work]);
            fprintf(stderr, "%d ",work);
        }
        //work=fgetc(infile);
        //set_color(0,&colours[work]);
        fprintf(stderr, "\n");

        //fseek(infile,offset+10,SEEK_SET);
        j=0;
        while (!feof(infile) && ftell(infile)<size)
        {
            // First get count
            c=fgetc(infile);

            if (((c & 0x80) == 0x80) || countflag)
            { // is a counter
                if (!countflag) c &= 0x7f;
                if (countflag) c-=1;
                work=fgetc(infile);
                work2=fgetc(infile);
                for (i=0;i<c+1;i++)
                {
                    drawpixels(work,work2);
                }
            }
            else
            {
                // Don't count on the next j characters

                for (i=0;i<c+1;i++)
                {
                    work=fgetc(infile);
                    work2=fgetc(infile);
                    drawpixels(work,work2);
                }
            }
        }
        sprintf(filename,"Output\\Output%2i.bmp",outpic);
        outpic++;

        // Now go to the next image
        fprintf(stderr, "Current Offset: %4lx\n",ftell(infile));
        //fseek(infile,size,SEEK_SET);
    fclose(infile);
}
