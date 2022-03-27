/*
 *	Simple curses UI implementation. You probably want to replace
 *	this with something more useful
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#include <string.h>
#include <strings.h>

#include "glk.h"
#include "glkimp.h"
#include "glkstart.h"

#include "taylor.h"
#include "utility.h"
#include "sagadraw.h"
#include "layouttext.h"

#define GLK_BUFFER_ROCK 1
#define GLK_STATUS_ROCK 1010
#define GLK_GRAPHICS_ROCK 1020

winid_t Bottom, Top, Graphics;
winid_t CurrentWindow;
static int OutC;
static char OutWord[128];

glui32 TopWidth; /* Terminal width */
glui32 TopHeight; /* Height of top window */

winid_t FindGlkWindowWithRock(glui32 rock)
{
    winid_t win;
    glui32 rockptr;
    for (win = glk_window_iterate(NULL, &rockptr); win;
         win = glk_window_iterate(win, &rockptr)) {
        if (rockptr == rock)
            return win;
    }
    return 0;
}

void Display(winid_t w, const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glk_put_string_stream(glk_window_get_stream(w), msg);
}

void HitEnter(void)
{
    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;
    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if (ev.val1 == keycode_Return) {
                result = 1;
            } else {
                glk_request_char_event(Bottom);
            }
        }
    } while (result == 0);

    return;
}

static void WordFlush(winid_t win);

void PrintCharacter(unsigned char c)
{
    if(OutC == 0 &&  c == '\0')
        return;

    if(CurrentWindow == Bottom) {
        if(isspace(c)) {
            WordFlush(Bottom);
            if(c == '\n') {
                Display(Bottom, "\n");
            } else {
                Display(Bottom, " ");
            }
            return;
        }
        OutWord[OutC] = c;
        OutC++;
        if(OutC > 79)
            WordFlush(Bottom);
        return;
    } else {
        if(isspace(c)) {
            WordFlush(Top);
            WriteToRoomDescriptionStream(" ");
            if(c == '\n') {
                WriteToRoomDescriptionStream("\n");
            }
            return;
        }
        OutWord[OutC] = c;
        OutC++;
        if(OutC == 78)
            WordFlush(Top);
    }
    return;
}

unsigned char WaitCharacter(void)
{
    WordFlush(Bottom);
    glk_request_char_event(Bottom);

    event_t ev;
    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_CharInput);
    return ev.val1;
}

static void WordFlush(winid_t win)
{
    int i;
    for(i = 0; i < OutC; i++) {
        if (win == Top)
            WriteToRoomDescriptionStream("%c", OutWord[i]);
        else
            Display(win, "%c", OutWord[i]);
    }
    OutC = 0;
}

extern strid_t room_description_stream;
extern char *roomdescbuf;


void TopWindow(void)
{
    WordFlush(Bottom);
    if (roomdescbuf != NULL) {
        fprintf(stderr, "roomdescbuf was not Null, so freeing it\n");
        free(roomdescbuf);
    }
    roomdescbuf = MemAlloc(1000);
    roomdescbuf = memset(roomdescbuf, 0, 1000);
    room_description_stream = glk_stream_open_memory(roomdescbuf, 1000, filemode_Write, 0);

    CurrentWindow = Top;
    glk_window_clear(Top);
}

static void PrintWindowDelimiter(void)
{
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    glk_window_move_cursor(Top, 0, TopHeight - 1);
    glk_stream_set_current(glk_window_get_stream(Top));
    for (int i = 0; i < TopWidth; i++)
        glk_put_char('_');
}

static void FlushRoomDescription(void)
{
    glk_stream_close(room_description_stream, 0);

    //    strid_t StoredTranscript = Transcript;
    //    if (!print_look_to_transcript)
    //        Transcript = NULL;

    int print_delimiter = 1;

    glk_window_clear(Top);
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    int rows, length;
    char *text_with_breaks = LineBreakText(roomdescbuf, TopWidth, &rows, &length);

    glui32 bottomheight;
    glk_window_get_size(Bottom, NULL, &bottomheight);
    winid_t o2 = glk_window_get_parent(Top);
    if (!(bottomheight < 3 && TopHeight < rows)) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed, rows,
                                   Top);
    } else {
        print_delimiter = 0;
    }

    int line = 0;
    int index = 0;
    int i;
    char string[TopWidth + 1];
    for (line = 0; line < rows && index < length; line++) {
        for (i = 0; i < TopWidth; i++) {
            string[i] = text_with_breaks[index++];
            if (string[i] == 10 || string[i] == 13 || index >= length)
                break;
        }
        if (i < TopWidth + 1) {
            string[i++] = '\n';
        }
        string[i] = 0;
        if (strlen(string) == 0)
            break;
        glk_window_move_cursor(Top, 0, line);
        Display(Top, "%s", string);
    }

    if (line < rows - 1) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed,
                                   MIN(rows - 1, TopHeight - 1), Top);
    }

    free(text_with_breaks);

    if (print_delimiter) {
        PrintWindowDelimiter();
    }

    //    if (pause_next_room_description) {
    //        Delay(0.8);
    //        pause_next_room_description = 0;
    //    }

    if (roomdescbuf != NULL) {
        free(roomdescbuf);
        roomdescbuf = NULL;
    }
}

char *roomdescbuf = NULL;

extern int PendSpace;

void BottomWindow(void)
{
    WordFlush(Top);
    FlushRoomDescription();
    WordFlush(Top);
    PendSpace = 0;
    CurrentWindow = Bottom;
}

void Look(void);

void OpenGraphicsWindow(void);
void CloseGraphicsWindow(void);


void Updates(event_t ev)
{
    if (ev.type == evtype_Arrange) {
        //        UpdateSettings();

        CloseGraphicsWindow();
        OpenGraphicsWindow();

        Look();
    }
}


void LineInput(char *buf, int len)
{
    event_t ev;

    if (PendSpace) {
        fprintf(stderr, "PendSpace before LineInput?\n");
        PendSpace = 0;
    }

    glk_request_line_event(Bottom, buf, len - 1, 0);

    while(1)
    {
        glk_select(&ev);

        if(ev.type == evtype_LineInput)
            break;
        else Updates(ev);
    }

    buf[ev.val1] = 0;
}


const glui32 OptimalPictureSize(glui32 *width, glui32 *height)
{
    *width = 255;
    *height = 96;
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

void CloseGraphicsWindow(void)
{
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics) {
        glk_window_close(Graphics, NULL);
        Graphics = NULL;
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    }
}

void OpenGraphicsWindow(void)
{
    if (!gli_enable_graphics)
        return;
    glui32 graphwidth, graphheight, optimal_width, optimal_height;

    if (Top == NULL)
        Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics == NULL && Top != NULL) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_close(Top, NULL);
        Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional,
                                   60, wintype_Graphics, GLK_GRAPHICS_ROCK);
        glk_window_get_size(Graphics, &graphwidth, &graphheight);
        pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
        x_offset = ((int)graphwidth - (int)optimal_width) / 2;

        if (graphheight > optimal_height) {
            winid_t parent = glk_window_get_parent(Graphics);
            glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                                       optimal_height, NULL);
        }

        /* Set the graphics window background to match
         * the main window background, best as we can,
         * and clear the window.
         */
        glui32 background_color;
        if (glk_style_measure(Bottom, style_Normal, stylehint_BackColor,
                              &background_color)) {
            glk_window_set_background_color(Graphics, background_color);
            glk_window_clear(Graphics);
        }

        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, TopHeight,
                              wintype_TextGrid, GLK_STATUS_ROCK);
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    } else {
        if (!Graphics)
            Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional, 60,
                                       wintype_Graphics, GLK_GRAPHICS_ROCK);
        glk_window_get_size(Graphics, &graphwidth, &graphheight);
        pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
        x_offset = (graphwidth - optimal_width) / 2;
        winid_t parent = glk_window_get_parent(Graphics);
        glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                                   optimal_height, NULL);
    }
}

void OpenTopWindow(void)
{
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top == NULL) {
        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed,
                              TopHeight, wintype_TextGrid, GLK_STATUS_ROCK);
        if (Top == NULL) {
            Top = Bottom;
        } else {
            glk_window_get_size(Top, &TopWidth, NULL);
        }
    }
}

void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, 0, x_offset, 0, 32 * 8 * pixel_size,
                         12 * 8 * pixel_size);
}

//7025


uint16_t HL, pushedHL, BC, pushedBC, DE, pushedDE, IX, pushedIX, current_screen_address, temp, previousPush, attributes_start, instructionaddress, stored_address, lastblockaddr;
uint8_t A, carry, pushedA, xoff, yoff, width, height, column, lastimgblock = 0xff, columnstodraw = 0, rowstodraw = 0, repeats;

uint8_t *mem = NULL;

void print_memory(int address, int length);

static void draw_spectrum_screen_from_mem(void);
void draw_attributes(void);
void draw_image(int img);
void print_screen_memory(void);
void print_screen_memory_2(void);

void LoadMemory(void) {
    mem = (uint8_t *)malloc(0xffff);
    memset(mem, 0, 0xffff);
    int i;
    for (i = 0; i<FileImageLen && i < 0xffff - 0x3fe5; i++) {
        mem[i+0x3fe5] = FileImage[i];
    }
    fprintf(stderr, "Read 0x%04x bytes into mem, starting at 0x3fe5. Final address: 0x%04x \n", i, i+0x3fe5);

    for (int i = 0x4000; i < 0x5800; i++) {
        mem[i] = 0;
    }

    for (int i = 0x5800; i < 0x5800 + 384; i++) {
        mem[i] = 0;
    }
    print_screen_memory();
}

void DrawRoomImage(void) {
    ClearGraphMem();
    draw_image(MyLoc);
    DrawSagaPictureFromBuffer();
}

uint16_t getImageAddress(uint8_t blocknum);

void DisplayInit(void)
{
    LoadMemory();
    SagaSetup();
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    OpenTopWindow();
    OpenGraphicsWindow();
    glk_window_clear(Graphics);
    for (int i = 1; i < 58; i++) {
        draw_image(i);
        fprintf(stderr, "Image: %d\n", i);
        draw_spectrum_screen_from_mem();
    }
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


void print_registers(void)
{
    fprintf(stderr, "A:%X HL:%X (HL):%X\n", A, HL, mem[HL]);
    fprintf(stderr, "BC:%X DE:%X IX:%X carry:%d\n", BC, DE, IX, carry);

    //    if (A == 0 && DE == 0 && IX == 0xA4BC && HL == 0xA74D && mem[HL] == 0)
    //        fprintf(stderr, "Seems to be stuck\n");

}

void print_memory(int address, int length)
{
    for (int i = address; i <= address + length ; i=i + 16){
        fprintf(stderr, "\n%04X:  ", i);
        for (int j = 0; j < 16; j++){
            fprintf(stderr, "%02X ", mem[i+j]);
        }
    }
    fprintf(stderr, "\n");
}

void print_screen_memory_2(void)
{
    for (int i = 0x5800; i < 0x5800 + 768 ; i=i + 8){
        fprintf(stderr, "\n    ");
        for (int j = 0; j < 8; j++){
            fprintf(stderr, "0x%02x, ", mem[i+j]);
        }
    }
}

void print6430(void) {
    print_memory(0x6430, 97);
}

void print_screen_memory(void) {
    fprintf(stderr, "Graphics memory:\n");
    print_memory(0x4000, 768);
    fprintf(stderr, "Attributes memory:\n");
    print_memory(0x5800, 768);
}


void draw_image(int img);

static void draw_spectrum_screen_from_mem(void) {
    OpenGraphicsWindow();

    for (int i = 0; i < 768; i++)
    {
        glui32 paper = (mem[0x5800+i] >> 3) & 0b111;
        paper += 8 * ((mem[0x5800+i] & 64) == 64);

        RectFill((i % 32) * 8, (i / 32) * 8, 8, 8, paper, 1);
        //        fprintf(stderr, "draw_spectrum_screen_from_mem: Filling (8 x 8 rect at x:%d y:%d (char pos %d, %d), color %d\n", (i % 32) * 8, (i / 24) * 8, (i % 32), (i / 32), paper);
    }

    for (int i = 0; i < 3972; i++)
    {
        uint16_t address = 0x4000+i;
        uint8_t x = (address & 0b11111) * 8;
        uint8_t low = (address >> 8) & 0b111;
        uint8_t mid = (address >> 2) & 0b111000;
        uint8_t high = (address >> 5) & 0b11000000;
        uint8_t y = low | mid | high;

        glui32 ink = (mem[0x5800+(y/8) * 32 + (x/8)] & 0b111);
        ink += 8 * ((mem[0x5800+(y/8) * 32 + (x/8)] & 64) == 64);

        for (int j = 7; j >= 0; j--) {
            if ((mem[address] >> j) & 1) {
                PutPixel(x+7-j,y, ink);
                //                fprintf(stderr, "plotting pixel at %d,%d, ink %d\n", x+7-j, y, ink);
            }
        }
    }

    event_t ev;
    glk_request_char_event(Bottom);

    do {
        glk_select(&ev);
    } while (ev.type != evtype_CharInput);
}

uint16_t currentinstruction;
uint16_t currentscreenaddress;

int calla050(void) // Get next adress?
{
    uint8_t lastAttribute = 0;
    currentscreenaddress++; // byte to the right
    columnstodraw--;
    if (columnstodraw != 0)
        return 0;
    rowstodraw--;
    if (rowstodraw != 0) {
        columnstodraw = width;
        column++;
        uint16_t address = 0x9d08 + column * 2;
        currentscreenaddress = xoff + mem[address] + mem[address + 1] * 256;
        return 0;
    }
    currentinstruction++;
    columnstodraw = width;
    uint16_t screenMemPos = 0x5800 + xoff + yoff * 32;
    IX = currentinstruction;
    do {
        A = mem[IX];
        if (A >= 128) { // Bit 7 is repeat flag
            repeats = (A & 0x7f) - 1;
            A = lastAttribute;
        }
    jumpa0b0:
        mem[screenMemPos] = A;
        fprintf(stderr, "Set attribute address 0x%04x to %x\n", DE, A);
        lastAttribute = A;
        screenMemPos++;
        columnstodraw--;
        if (columnstodraw == 0) {
            height--;
            if (height == 0) {
                IX = pushedIX;
                return 1;
            }
            columnstodraw = width;
            yoff++;
            screenMemPos = 0x5800 + xoff + yoff * 32;
        }
        if (repeats != 0) {
            repeats--;
            A = lastAttribute;
            goto jumpa0b0;
        }
        IX++;
    } while (IX < 0xffff);
    return 1;
}

//Routine at 9f16

void draw_image(int img)
{
    fprintf(stderr, "draw_image: First instruction at %x\n", HL);

    A = img - 1;

    // 0xa8c0 start of image offsets lookup
    // 0xa916 start of image data

    uint16_t address = (mem[0x9e80 + A] & 0x7f) * 2 + 0xa8c0;
    HL = 0xa916 + mem[address] + mem[address+1] * 256;
    currentinstruction = HL;
    width = mem[HL];
    columnstodraw = mem[HL];
    HL++;
    height = mem[HL];
    rowstodraw = mem[HL];
    HL++;
    xoff = mem[HL];
    BC = xoff;
    HL++;
    yoff = mem[HL];
    column = mem[HL];

    address = mem[HL] * 2 + 0x9d08;
    currentscreenaddress = mem[address] + mem[address+1] * 256 + BC; // store retrieved image address

    HL++;

    currentinstruction = HL;
    pushedIX = IX;

    uint8_t flipmode = 0; // 0x6446 and 0x6436 are reset to 0
    repeats = 0;
jump9f35:
    mem[0x9ebe] = 0; // 0x9ebe is reset to 0
    A = mem[HL]; // A = next draw instruction
    if (A >= 128) {
        // Bit 7 set, this is a command byte
        mem[0x9ebe] = A; // draw instruction is stored in 0x9ebe and 0x9ebf
        mem[0x9ebf] = A;
        A = A & 2; // Test bit 1, repeat flag
        if (A != 0) {
            HL++;
            A = mem[HL]; // Set A to next instruction
            repeats = A; // and store it in 0x9ec0
        }
        HL++;
        currentinstruction = HL;
        A = mem[HL];
    }
    HL = A;
    BC = 0xa100; // Lookup table for character offsets
    A = mem[0x9ebf];
    if ((A & 1) == 1) { // Test bit 0, whether to add 127 to character index
        uint8_t B = BC >> 8;
        B = B + 4;
        BC = (BC & 0xFF) + B * 256;
    }

    IX = HL * 8 + BC;
    HL = 0x9ec8;
    A = mem[0x9ebe]; // Current draw instruction
    A = (A & 0x30); // bits 4 and 5
    switch (A) {
        case 0: // do nothing, straight copy
                //            fprintf(stderr, "0: do nothing, straight copy\n");
            for (int i = 0; i < 8; i++) {
                mem[HL++] = mem[IX++];
            }
            break;
        case 0x10: // rotate 90 degrees
                   //            fprintf(stderr, "0x10: rotate 90 degrees\n");
            for (int C = 8 ; C > 0 ; C--) {
                A = mem[IX];
                for (int B = 8 ; B > 0 ; B--) {
                    carry = ((A & 128) == 128);
                    A = A << 1;
                    carry = rotate_right_with_carry(&mem[HL++], carry);
                }
                HL = 0x9ec8;
                IX++;
            }
            break;
        case 0x20:  // rotate 180 degrees
                    //            fprintf(stderr, "0x20: rotate 180 degrees\n");
            HL = 0x9ecf;
            for (int C = 0; C < 8; C++) {
                A = mem[IX];
                for (int B = 0; B < 8; B++) {
                    carry = ((A & 128) == 128);
                    A = A << 1;
                    carry = rotate_right_with_carry(&mem[HL], carry);
                } ;
                IX++;
                HL--;
            }
            break;

        case 0x30: // Rotate 270 degrees
                   //            fprintf(stderr, "0x30: rotate 270 degrees\n");
            for (int C = 0; C < 8 ; C++) {
                A = mem[IX];
                for (int B = 0; B < 8 ; B++) {
                    carry = (A & 1);
                    A = A >> 1;
                    carry = rotate_left_with_carry(&mem[HL++], carry);
                }
                HL = 0x9ec8;
                IX++;
            }
            break;
        default:
            break;
    }

    A = mem[0x9ebe];
    A = A & 0x40; // bit 6
    if (A != 0) {  // flip horizontally
                   //        fprintf(stderr, "bit 6 is set: flip horizontally\n");
        HL = 0x9ec8;
        for (int C = 0; C < 8 ; C++) {
            A = mem[HL];
            for (int B = 0; B < 8 ; B++) {
                carry = ((A & 128) == 128);
                A = (A << 1); //+ (A & 1);
                carry = rotate_right_with_carry(&mem[HL], carry);
            }
            HL++;
        }
    }
    DE = currentscreenaddress; // Screen adress
    HL = 0x9ec8;
    for (int B = 0; B < 8; B++) {
        A = flipmode; // flip mode
        switch(A) {
            case 4: // The screen memory value is ORed
                    //                fprintf(stderr, "4: OR\n");
                A = mem[DE] | mem[HL];
                break;
            case 8: // The screen memory is ANDed
                    //                fprintf(stderr, "8: AND\n");
                A = mem[DE] & mem[HL];
                break;
            case 12:  // The screen memory is XORed
                      //                fprintf(stderr, "12: XOR\n");
                A = mem[DE] ^ mem[HL];
                break;
            default:
                //                fprintf(stderr, "0: straight overlay \n");
                A = mem[HL];
                break;
        }
        mem[DE] = A; // set screen adress to new value
                     //        fprintf(stderr, "Set address %x to %x\n", DE, mem[DE]);
        mem[HL] = A;
        HL++;
        DE = DE + 256; // Next line
    }
    A = mem[0x9ebe];
    A = A & 0x0c;
    flipmode = A; // Flip mode
    if (flipmode == 0) { // No flip
    jumpa022:
        A = repeats;
        if (A != 0) {
            if (calla050())  // Get screen address for next character?
                return;
            HL = 0x9ec8;
            DE = currentscreenaddress; // Screen adress

            for (int B = 0; B < 8; B++) {
                A = mem[HL++];
                mem[DE] = A;
                DE = DE + 256;
            }

            repeats--;
            if (repeats != 0) {
                goto jumpa022;
            }
        }
        if (calla050())
            return;
    }

    currentinstruction++;
    HL = currentinstruction;
    goto jump9f35;
}




/*

 ; Routine at 9ed1  Get image address, width, height, xoff, yoff
 ;
 ; Used by the routine at #R$98df. Get image address, width, height, xoff, yoff
 c$9ed1 dec a         ;
 $9ed2 ld e,a        ;
 $9ed3 ld d,$00      ; DE = A
 $9ed5 ld hl,$9e80   ;
 $9ed8 add hl,de     ; HL = 9e80 + A
 $9ed9 ld a,(hl)     ; A = (HL) & $7f 0111 1111
 $9eda and $7f       ;
 $9edc ld l,a        ; HL = A = Image number
 $9edd ld h,$00      ;
 $9edf add hl,hl     ; HL = image number * 2
 $9ee0 ld de,$a8c0   ; 0xa8c0 Image adress lookup
 $9ee3 add hl,de     ; HL = 0xa8c0 + image number * 2
 $9ee4 call $9d38    ; DE = (HL), HL = HL + 2
 $9ee7 ld hl,$a8c0   ;
 $9eea push de       ;
 $9eeb ld de,$0056   ;
 $9eee add hl,de     ; HL += 86 (0x56)
 $9eef pop de        ;
 $9ef0 add hl,de     ; + DE
 $9ef1 ld ($9ebc),hl ; // Is this width, height, xoffset, yoffset?
 $9ef4 ld hl,($9ebc) ;
 $9ef7 ld a,(hl)     ;
 $9ef8 ld ($9ec4),a  ; width?
 $9efb ld ($9ec6),a  ; width counter?
 $9efe inc hl        ;
 $9eff ld a,(hl)     ;
 $9f00 ld ($9ec5),a  ; height counter?
 $9f03 ld ($9ec7),a  ; height?
 $9f06 inc hl        ;
 $9f07 ld a,(hl)     ;
 $9f08 ld ($9ec1),a  ; xoffset?
 $9f0b ld c,a        ;
 $9f0c ld b,$00      ;
 $9f0e inc hl        ;
 $9f0f ld a,(hl)     ;
 $9f10 ld ($9ec2),a  ; yoffset?
 $9f13 ld ($9ec3),a  ;



 $9f16 push hl       ; Routine at 9f16 Draw image
 $9f17 ld h,$00      ;
 $9f19 ld l,a        ;
 $9f1a add hl,hl     ;
 $9f1b ld de,$9d08   ;  Screen line adress lookup
 $9f1e add hl,de     ;
 $9f1f call $9d38    ; DE = (HL), HL = HL + 2
 $9f22 ex de,hl      ;
 $9f23 add hl,bc     ;
 $9f24 ld ($9eba),hl ;
 $9f27 pop hl        ;
 $9f28 inc hl        ;
 $9f29 ld ($9ebc),hl ; ($9ebc) = HL (Next instruction)
 $9f2c push ix       ;
 $9f2e xor a         ;
 $9f2f ld ($9ed0),a  ; ($9ed0) = 0 flip_mode
 $9f32 ld ($9ec0),a  ; ($9ec0) = 0 remaining
 *$9f35 xor a         ;
 $9f36 ld ($9ebe),a  ; ($9ebe) = 0
 $9f39 ld a,(hl)     ; A = (HL)
 $9f3a bit 7,a       ; test bit 7
 $9f3c jr z,$9f52    ;
 $9f3e ld ($9ebe),a  ;
 $9f41 ld ($9ebf),a  ;
 $9f44 and $02       ;
 $9f46 jr z,$9f4d    ;
 $9f48 inc hl        ;
 $9f49 ld a,(hl)     ;
 $9f4a ld ($9ec0),a  ; (Next instruction)
 *$9f4d inc hl        ;
 $9f4e ld ($9ebc),hl ;
 $9f51 ld a,(hl)     ;
 *$9f52 ld l,a        ;
 $9f53 ld h,$00      ;
 $9f55 ld bc,$a100   ; Start of characters
 $9f58 ld a,($9ebf)  ;
 $9f5b bit 0,a       ; Test bit 0
 $9f5d jr z,$9f63    ;
 $9f5f inc b         ;
 $9f60 inc b         ;
 $9f61 inc b         ;
 $9f62 inc b         ;
 *$9f63 add hl,hl     ;
 $9f64 add hl,hl     ;
 $9f65 add hl,hl     ; HL = HL * 8
 $9f66 add hl,bc     ; + BC
 $9f67 push hl       ;
 $9f68 pop ix        ;
 $9f6a ld hl,$9ec8   ;
 $9f6d ld b,$08      ;
 $9f6f ld a,($9ebe)  ;
 $9f72 and $30       ;  // Current draw instruction bits 4 and 5
 $9f74 jr nz,$9f81   ;
 *$9f76 ld a,(ix+$00) ; // do nothing, straight copy
 $9f79 ld (hl),a     ;
 $9f7a inc ix        ;
 $9f7c inc hl        ;
 $9f7d djnz $9f76    ;
 $9f7f jr $9fd3      ;
 *$9f81 cp $10        ; // rotate 90 degrees
 $9f83 jr nz,$9f9d   ;
 $9f85 ld c,$08      ;
 *$9f87 ld b,$08      ;
 $9f89 ld a,(ix+$00) ;
 *$9f8c sla a         ;
 $9f8e rr (hl)       ;
 $9f90 inc hl        ;
 $9f91 djnz $9f8c    ;
 $9f93 ld hl,$9ec8   ;
 $9f96 inc ix        ;
 $9f98 dec c         ;
 $9f99 jr nz,$9f87   ;
 $9f9b jr $9fd3      ;
 *$9f9d cp $20        ;
 $9f9f jr nz,$9fb9   ;
 $9fa1 ld c,$08      ;
 $9fa3 ld hl,$9ecf   ;
 *$9fa6 ld b,$08      ;
 $9fa8 ld a,(ix+$00) ;
 *$9fab sla a         ;
 $9fad rr (hl)       ;
 $9faf djnz $9fab    ;
 $9fb1 inc ix        ;
 $9fb3 dec hl        ;
 $9fb4 dec c         ;
 $9fb5 jr nz,$9fa6   ;
 $9fb7 jr $9fd3      ;
 *$9fb9 cp $30        ;
 $9fbb jr nz,$9fd3   ;
 $9fbd ld c,$08      ;
 *$9fbf ld b,$08      ;
 $9fc1 ld a,(ix+$00) ;
 *$9fc4 srl a         ;
 $9fc6 rl (hl)       ;
 $9fc8 inc hl        ;
 $9fc9 djnz $9fc4    ;
 $9fcb ld hl,$9ec8   ;
 $9fce inc ix        ;
 $9fd0 dec c         ;
 $9fd1 jr nz,$9fbf   ;
 *$9fd3 ld a,($9ebe)  ;
 $9fd6 and $40       ;
 $9fd8 or a          ;
 $9fd9 jr z,$9fed    ;
 $9fdb ld c,$08      ;
 $9fdd ld hl,$9ec8   ;
 *$9fe0 ld b,$08      ;
 $9fe2 ld a,(hl)     ;
 *$9fe3 sla a         ;
 $9fe5 rr (hl)       ;
 $9fe7 djnz $9fe3    ;
 $9fe9 inc hl        ;
 $9fea dec c         ;
 $9feb jr nz,$9fe0   ;
 *$9fed ld de,($9eba) ;
 $9ff1 ld hl,$9ec8   ;
 $9ff4 ld b,$08      ;
 *$9ff6 ld a,($9ed0)  ;
 $9ff9 cp $04        ;
 $9ffb jr nz,$a001   ;
 $9ffd ld a,(de)     ;
 $9ffe or (hl)       ;
 $9fff jr $a012      ;
 *$a001 cp $08        ;
 $a003 jr nz,$a009   ;
 $a005 ld a,(de)     ;
 $a006 and (hl)      ;
 $a007 jr $a012      ;
 *$a009 cp $0c        ;
 $a00b jr nz,$a011   ;
 $a00d ld a,(de)     ;
 $a00e xor (hl)      ;
 $a00f jr $a012      ;
 *$a011 ld a,(hl)     ;
 *$a012 ld (de),a     ;
 $a013 ld (hl),a     ;
 $a014 inc hl        ;
 $a015 inc d         ;
 $a016 djnz $9ff6    ;
 $a018 ld a,($9ebe)  ;
 $a01b and $0c       ;
 $a01d ld ($9ed0),a  ;
 $a020 jr nz,$a046   ;
 *$a022 ld a,($9ec0)  ;
 $a025 or a          ;
 $a026 jr z,$a043    ;
 $a028 call $a050    ;
 $a02b ld b,$08      ;
 $a02d ld hl,$9ec8   ;
 $a030 ld de,($9eba) ;
 *$a034 ld a,(hl)     ;
 $a035 ld (de),a     ;
 $a036 inc hl        ;
 $a037 inc d         ;
 $a038 djnz $a034    ;
 $a03a ld a,($9ec0)  ;
 $a03d dec a         ;
 $a03e ld ($9ec0),a  ;
 $a041 jr nz,$a022   ;
 *$a043 call $a050    ;
 *$a046 ld hl,($9ebc) ;
 $a049 inc hl        ;
 $a04a ld ($9ebc),hl ;
 $a04d jp $9f35      ;

 ; Routine at a050
 ;
 ; Used by the routine at #R$9ed1. Draw attributes
 c$a050 ld de,($9eba) ;
 $a054 inc de        ;
 $a055 ld ($9eba),de ;
 $a059 ld hl,$9ec6   ; width counter
 $a05c dec (hl)      ;
 $a05d jr nz,$a087   ;
 $a05f ld hl,$9ec7   ; height counter
 $a062 dec (hl)      ;
 $a063 jr z,$a088    ;
 $a065 ld a,($9ec4)  ; width
 $a068 ld ($9ec6),a  ; width counter
 $a06b ld a,($9ec3)  ;
 $a06e inc a         ;
 $a06f ld ($9ec3),a  ;
 $a072 ld l,a        ;
 $a073 ld h,$00      ;
 $a075 add hl,hl     ;
 $a076 ld de,$9d08   ;
 $a079 add hl,de     ;
 $a07a call $9d38    ; DE = (HL), HL = HL + 2
 $a07d ld a,($9ec1)  ; A = xpos
 $a080 ld l,a        ;
 $a081 ld h,$00      ;
 $a083 add hl,de     ;
 $a084 ld ($9eba),hl ;
 *$a087 ret
 ;
 *$a088 pop af        ; HÄR!!!!
 $a089 ld hl,($9ebc) ;
 $a08c inc hl        ;
 $a08d ld ($9ebc),hl ;
 $a090 ld a,($9ec4)  ; image width
 $a093 ld ($9ec6),a  ; width counter
 $a096 ld a,($9ec2)  ;
 $a099 call $a0e5    ; DE = 0x5800 + xpos + ypos * 32;
 $a09c ld ix,($9ebc) ;
 *$a0a0 ld a,(ix+$00) ;
 $a0a3 bit 7,a       ;
 $a0a5 jr z,$a0b0    ;
 $a0a7 and $7f       ; A = A & 127 0111 1111 - 1
 $a0a9 dec a         ;
 $a0aa ld ($9ec0),a  ;
 $a0ad ld a,($9eb9)  ;
 *$a0b0 ld (de),a     ;
 $a0b1 ld ($9eb9),a  ;
 $a0b4 inc de        ;
 $a0b5 ld hl,$9ec6   ; width counter
 $a0b8 dec (hl)      ;
 $a0b9 jr nz,$a0cf   ;
 $a0bb ld hl,$9ec5   ; height counter
 $a0be dec (hl)      ; height counter--
 $a0bf jr z,$a0e2    ;
 $a0c1 ld a,($9ec4)  ; image width
 $a0c4 ld ($9ec6),a  ; width counter
 $a0c7 ld hl,$9ec2   ;
 $a0ca inc (hl)      ;
 $a0cb ld a,(hl)     ;
 $a0cc call $a0e5    ; DE = 0x5800 + xpos + ypos * 32;
 *$a0cf ld a,($9ec0)  ;
 $a0d2 or a          ;
 $a0d3 jr z,$a0de    ;
 $a0d5 dec a         ;
 $a0d6 ld ($9ec0),a  ;
 $a0d9 ld a,($9eb9)  ;
 $a0dc jr $a0b0      ;
 *$a0de inc ix        ;
 $a0e0 jr $a0a0      ;
 *$a0e2 pop ix        ;
 $a0e4 ret           ;

 ; Routine at a0e5
 ;
 ; Used by the routine at #R$a050.
 c$a0e5 ld l,a        ;
 $a0e6 ld h,$00      ;
 $a0e8 add hl,hl     ;
 $a0e9 add hl,hl     ;
 $a0ea add hl,hl     ;
 $a0eb add hl,hl     ;
 $a0ec add hl,hl     ;
 $a0ed ld a,($9ec1)  ; A = xpos
 $a0f0 ld e,a        ;
 $a0f1 ld d,$58      ;
 $a0f3 add hl,de     ; 0x5800 + xpos + ypos * 32;
 $a0f4 ex de,hl      ;
 $a0f5 ret           ;
 */


