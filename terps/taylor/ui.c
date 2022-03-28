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
}

void DrawRoomImage(void) {
    ClearGraphMem();
    draw_image(MyLoc);
    DrawSagaPictureFromBuffer();
}

uint16_t getImageAddress(uint8_t blocknum);

void call9b45(void);
void call9a3c(void);

void DisplayInit(void)
{
    LoadMemory();
    SagaSetup();
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    OpenTopWindow();
    A = 0;
    HL = 0x68a5;
    call9a3c();
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

uint16_t instructionaddress;
uint16_t currentscreenaddress;

int calla050(void) // Get next adress?
{
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

    // Draw attributes
    instructionaddress++;
    columnstodraw = width;
    uint8_t lastAttribute = 0;
    uint16_t screenMemPos = 0x5800 + xoff + yoff * 32;
    IX = instructionaddress;
    do {
        A = mem[IX];
        if (A >= 128) { // Bit 7 is repeat flag
            repeats = (A & 0x7f) - 1;
            A = lastAttribute;
        }
    jumpa0b0:
        mem[screenMemPos] = A;
//        fprintf(stderr, "Set attribute address 0x%04x to %x\n", DE, A);
        lastAttribute = A;
        screenMemPos++;
        columnstodraw--;
        if (columnstodraw == 0) {
            height--;
            if (height == 0) {
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
    instructionaddress = HL;
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

    instructionaddress = HL;
    pushedIX = IX;

    uint8_t flipmode = 0;
    repeats = 0;
    uint8_t current_instruction;
    do {
        current_instruction = 0; // 0x9ebe is reset to 0
        A = mem[HL]; // A = next draw instruction
        if (A >= 128) {
            // Bit 7 set, this is a command byte
            current_instruction = A; // draw instruction is stored in 0x9ebe and 0x9ebf
            mem[0x9ebf] = A;
            A = A & 2; // Test bit 1, repeat flag
            if (A != 0) {
                HL++;
                A = mem[HL]; // Set A to next instruction
                repeats = A; // and store it in 0x9ec0
            }
            HL++;
            instructionaddress = HL;
            A = mem[HL];
        }
        HL = A;
        BC = 0xa100; // Lookup table for character offsets
        if ((mem[0x9ebf] & 1) == 1) { // Test bit 0, whether to add 127 to character index
            BC += 0x400;
        }

        IX = HL * 8 + BC;
        HL = 0x9ec8;
// Current draw instruction bits 4 and 5
        switch (current_instruction & 0x30) {
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

        if ((current_instruction & 0x40) != 0) {  // bit 6 // flip horizontally
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
            switch(flipmode) {
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
        flipmode = current_instruction & 0x0c;
        if (flipmode == 0) { // No flip
            for (; repeats != 0; repeats--) {
                if (calla050())  // Get screen address for next character?
                    return;
                HL = 0x9ec8;
                DE = currentscreenaddress; // Screen adress

                for (int B = 0; B < 8; B++) {
                    A = mem[HL++];
                    mem[DE] = A;
                    DE = DE + 256;
                }
            }
            if (calla050())
                return;
        }

        instructionaddress++;
        HL = instructionaddress;
    } while (HL < 0xffff);
}

void call9a6e(int skip);

void call9b79(int skip);
void call9b27(void);
void call9c85(void);

//; Routine at 9a3c

void call9a3c(void) {
    A = mem[0x9971];
    BC = (BC & 0xff) + A * 256;
    if (A == 0)
        goto jump9a50;
    for (int i = A; i > 0; i--) {
    jump9a43:
        HL++;
        A = mem[HL];
        if (A == 0x18)
            continue;
        if (A != 0x1f)
            goto jump9a43;
    }
    HL++;
jump9a50:
    A = 0x80;
    mem[0x9a3b] = A;
jump9a55:
    A = mem[HL];
    if (A == 0x18)
        return;
    if (A != 0x1f)
        goto jump9a62;
    A = 0x0d;
    goto jump9b27;
jump9a62:
    mem[0x9ae8] = HL & 0xff;
    mem[0x9ae9] = HL >> 8;
    call9a6e(0);
    HL = mem[0x9ae8] + mem[0x9ae9] * 256 + 1;
    goto jump9a55;
jump9b27:
    pushedHL = HL;
    pushedBC = BC;
    pushedA = A;
if (A != 0x0d)
    goto jump9b40;
    A = mem[0x5bf6];
    BC = (BC & 0xff) + A * 256;
jump9b33:
    BC += 256;
    A = 0x2b;
    if ((BC >> 8) == A)
        goto jump9b74;
    A = 0x20;
    call9b79(0);
    goto jump9b33;
jump9b40:
    call9b79(0);
jump9b74:
    A = pushedA;
    BC = pushedBC;
    DE = pushedDE;
    HL = pushedHL;
}



// Routine at 9b45 Print Room description?

void call9b45(void) {
    pushedHL = HL;
    pushedDE = DE;
    pushedBC = BC;
    pushedA = A;
    mem[0x9971] = A;
    HL = 0x6f1d;
    call9a3c();
}

void call9c6c(void) {
    pushedDE = DE;
    A = mem[0x5b5f];
    A += A;
    DE = A;
    HL = 0x9d08 + A;
//    call9d38(); // DE = (HL), HL = HL + 2
    DE = mem[HL] + mem[HL+1] * 256;
    HL += 2;
    mem[0x5bf3] = DE & 0xff;
    mem[0x5bf4] = DE >> 8;
    A = 0;
    mem[0x5bf6] = 0;
    DE = pushedDE;
}

void call9b79(int skip) { // Draw letters to screen
    if (isascii(A))
        fprintf(stderr, "%c", A);
    if (!skip) {
        if (A != 0x0c)
            goto jump9b87;
        call9b79(1);
        A = 0x0c;
    }
jump9b87:
    pushedHL = HL;
    pushedBC = BC;
    pushedDE = DE;
    pushedA = A;
    if (A == 0x0d)
        goto jump9bbd;
    if (A != 0x0c)
        goto jump9bd7;
    A = mem[0x5bf6];
    if (A != 0)
        goto jump9ba8;
    A = 0x29;
    mem[0x5bf6] = A;
    A = mem[0x5bf5];
    A--;
    mem[0x5bf5] = A;
    goto jump9bb3;
jump9ba8:
    A--;
    mem[0x5bf6] = A;
    A = A & 0x03;
    if (A == 0x02)
        goto jump9c67;
jump9bb3:
    HL = mem[0x5bf3] + mem[0x5bf4] * 256;
    HL--;
    mem[0x5bf3] = HL & 0xff;
    mem[0x5bf4] = HL >> 8;
    goto jump9c67;
jump9bbd:
    A = mem[0x5bf5];
//    If A < N, then C flag is set.
//    If A >= N, then C flag is reset.
    if (A < 0x17)
        goto jump9bca;
    call9c85();
    goto jump9c67;
jump9bca:
    A = mem[0x5bf5];
    A++;
    mem[0x5bf5] = A;
    call9c6c();
    goto jump9c67;
jump9bd7:
    if (isascii(A))
        fprintf(stderr, "%c", A);
    DE = mem[0x5bf3] + mem[0x5bf4] * 256;
    BC = 0x8dcb;
    A = A - 0x20;
    if (isascii(A))
        fprintf(stderr, "%c", A);
    HL = A;
    HL += HL;
    HL += HL;
    HL += HL;
    HL += BC;
    temp = HL;
    HL = DE;
    DE = temp;
    pushedHL = HL;
    A = mem[0x5bf6];
    A = A & 0x03;
    if (A == 0x03)
        goto jump9c46;
    if (A == 0x02)
        goto jump9c21;
    if (A == 0x01)
        goto jump9c09;
    for (int i = 0; i < 8; i++) {
    jump9bfc:
        A = mem[DE];
    jump9bfd:
        mem[HL] = A;
        DE++;
        HL += 256;
    }
jump9c02:
    HL = pushedHL;
    HL++;
    mem[0x5bfe3] = HL & 0xff;
    mem[0x5bfe4] = HL >> 8;
    goto jump9c59;
jump9c09:
    for (int i = 0; i < 8; i++) {
        A = mem[DE];
        HL--;
        carry = rotate_right_with_carry(&mem[HL], 0);
        carry = rotate_right_with_carry(&mem[HL], 0);
        carry = rotate_left_with_carry(&A, (A & 1));
        carry = rotate_right_with_carry(&mem[HL], carry);
        carry = rotate_left_with_carry(&A, (A & 1));
        carry = rotate_right_with_carry(&mem[HL], carry);
        HL++;
        mem[HL] = A;
        HL += 256;
        DE += 256;
    }

    goto jump9c02;
jump9c21:
    for (int i = 0; i < 8; i++) {
        A = mem[DE];
        HL--;
        carry = rotate_right_with_carry(&mem[HL], 0);
        carry = rotate_right_with_carry(&mem[HL], 0);
        carry = rotate_right_with_carry(&mem[HL], 0);
        carry = rotate_right_with_carry(&mem[HL], 0);
        carry = rotate_left_with_carry(&A, (A & 1));
        carry = rotate_right_with_carry(&mem[HL], carry);
        carry = rotate_left_with_carry(&A, (A & 1));
        carry = rotate_right_with_carry(&mem[HL], carry);
        carry = rotate_left_with_carry(&A, (A & 1));
        carry = rotate_right_with_carry(&mem[HL], carry);
        carry = rotate_left_with_carry(&A, (A & 1));
        carry = rotate_right_with_carry(&mem[HL], carry);
        HL++;
        mem[HL] = A;
        HL += 256;
        DE++;
    }
    HL = pushedHL;
    goto jump9c59;
jump9c46:
    for (int i = 0; i < 8; i++) {
        A = 0xc0;
        A = A & mem[HL];
        mem[HL] = A;
        A = mem[DE];
        carry = rotate_right_with_carry(&A, 0);
        carry = rotate_right_with_carry(&A, 0);
        A = A | mem[HL];
        mem[HL] = A;
        HL += 256;
        DE++;
    }
    goto jump9c02;
jump9c59:
    A = pushedA;
    pushedA = A;
    A = mem[0x5bf6];
    if (A == 0x29)
        goto jump9bbd;
    A++;
    mem[0x5bf6] = A;
jump9c67:
    A = pushedA;
    DE = pushedDE;
    BC = pushedBC;
    HL = pushedHL;
}

void call9a6e(int skip) { // Print message?
    if (!skip) {
        if (A < 0x7b)
            goto jump9a90;
        HL = 0x8c8b;
        A = A - 0x7b;
        if (A == 0)
            goto jump9a83;
        for (int i = A; i > 0; i--) {
        jump9a7a:
            if ((mem[HL] & 0x80) == 0)
                continue;
            HL++;
            A = mem[HL];
            goto jump9a7a;
        }
    jump9a83:
        HL++;
    jump9a84:
        A = mem[HL];
        call9a6e(1);
        HL++;
        if ((mem[HL] & 0x80) == 0)
            goto jump9a84;
        A = mem[HL];
        A = A & 0x7f;
    }
jump9a90:
    pushedHL = HL;
    pushedBC = BC;
    pushedDE = DE;
    pushedA = A;
    if (A != 0x20)
        goto jump9afc;
    HL = mem[0x9ae8] + mem[0x9ae9] * 256;
    BC = BC & 0xff;
    A = mem[HL];
    if (A < 0xec)
        goto jump9aa7;
    if (A >= 0xf0)
        goto jump9aa7;
    BC += 256;
jump9aa7:
    HL++;
    A = mem[HL];
    if (A == 0x20)
        goto jump9aea;
    if (A == 0x18)
        goto jump9aea;
    if (A == 0x1f)
        goto jump9aea;
    BC += 256;
    uint8_t B = BC >> 8;
    if (B < 0x7b)
        goto jump9aa7;
    if (B < 0x8b)
        goto jump9aea;
    BC += 256;
    if (B < 0xec)
        goto jump9aa7;
    if (B >= 0xf0)
        goto jump9acb;
    BC -= 256;
    BC -= 256;
    goto jump9aea;
jump9acb:
    BC += 256;
    B = BC >> 8;
    if (B < 0xf3)
        goto jump9aa7;
    BC += 256;
    B = BC >> 8;
    if (B < 0xf8)
        goto jump9aea;
    BC += 256;
    BC += 256;
    B = BC >> 8;
    if (B < 0xfc)
        goto jump9aea;
    BC += 256;
    if (B < 0xfe)
        goto jump9aea;
    if (B == 0xfe)
    goto jump9aa7;
    BC += 256;
    BC += 256;
    BC += 256;
    BC += 256;
    goto jump9aa7;

jump9aea:
    A = mem[0x5bf6];
    fprintf(stderr, "%c", A);
    if (A == 0)
        goto jump9b74;
    A = A + (BC >> 8);
    if (A < 0x2a)
        goto jump9afa;
    A = 0x0d;
    goto jump9b21;
jump9afa:
    A = 0x20;
jump9afc:
    HL = 0x9a3b;
    if (mem[HL] & 0x80)
        goto jump9b09;
    if (mem[HL] < 0x60)
        goto jump9b09;
    A = A - 0x20;
jump9b09:
    if (A < 0x21)
        goto jump9b0f;
    if (mem[HL] & 0x80)
        mem[HL] -= 0x80;
jump9b0f:
    if (mem[HL] == 0x21)
        goto jump9b1f;
    if (mem[HL] == 0x3f)
        goto jump9b1f;
    if (mem[HL] == 0x3a)
        goto jump9b1f;

    if (mem[HL] != 0x2e)
        goto jump9b21;
jump9b1f:
    if (!(mem[HL] & 0x80))
        mem[HL] += 0x80;
jump9b21:
    call9b27();
    goto jump9b74;
jump9b74:
    A = pushedA;
    BC = pushedBC;
    DE = pushedDE;
    HL = pushedHL;
}

void call9b27(void) {
    pushedHL = HL;
    pushedDE = DE;
    pushedBC = BC;
    pushedA = A;
    if (A != 0x0d)
        goto jump9b40;
    A = mem[0x5bf6];
    BC = (BC & 0xff) + A *256;
jump9b33:
    BC += 256;
    A = 0x2b;
    if (A == (BC >> 8)) {
        goto jump9b74;
    }
    A = 0x20;
    call9b79(0);
    goto jump9b33;
jump9b40:
    call9b79(0);
    goto jump9b74;
jump9b74:
    A = pushedA;
    BC = pushedBC;
    DE = pushedDE;
    HL = pushedHL;
}

void call9c85(void) {
    A = mem[0x5bf1];
jump9c88:
    HL = A;
    DE = 0x9d08;
    HL += HL;
    HL += DE;
    // call9d38();  DE = (HL), HL = HL + 2
    DE = mem[HL] + mem[HL+1] * 256;
    HL += 2;
    pushedDE = DE;
    A++;
    HL = A;
    DE = 0x9d08;
    HL += HL;
    HL += DE;
    DE = mem[HL] + mem[HL+1] * 256;
    HL += 2;
    temp = DE;
    DE = HL;
    HL = temp;
    DE = pushedDE;
    for (int i = 0; i < 8; i++) {
        pushedHL = HL;
        pushedDE = DE;
        for (int j = 0; j < 32; j++) {
            mem[DE] = mem[HL];
            DE++;
            HL++;
        }
        HL = pushedHL;
        DE = pushedDE;
        HL += 256;
        DE += 256;
    }
    if (A != 0x17)
        goto jump9c88;
    HL = 0x50e0;
    DE = HL;
    DE++;
    for (int i = 0; i < 8; i++) {
        pushedHL = HL;
        pushedDE = DE;
        mem[HL] = 0;
        for (int j = 0; j < 0x1f; j++) {
            mem[DE] = mem[HL];
            DE++;
            HL++;
        }
        HL = pushedHL;
        DE = pushedDE;
        HL += 256;
        DE += 256;
    }
    HL = 0x50e0;
    mem[0x5bf3] = HL & 0xff;
    mem[0x5bf4] = HL >> 8;
    mem[0x5bf6] = 0;
}


/*

 ; Routine at 9b45 Print message A
 ;
 ; Used by the routines at #R$91c7, #R$9360, #R$94ad, #R$96f2, #R$9731, #R$973d,
 ; #R$9767, #R$97de and #R$9972. Print message A
 c$9b45 push hl       ; Print message A
 $9b46 push de       ;
 $9b47 push bc       ;
 $9b48 push af       ;
 $9b49 ld ($9971),a  ;
 $9b4c ld hl,$6f1d   ;
 $9b4f call $9a3c    ;
 $9b52 jr $9b74      ;


 ; Routine at 9a3c
 ;
 ; Used by the routines at #R$9972, #R$9b45, #R$9b54 and #R$9b64.
 c$9a3c ld a,($9971)  ;
 $9a3f ld b,a        ;
 $9a40 or a          ;
 $9a41 jr z,$9a50    ;
 *$9a43 inc hl        ;
 $9a44 ld a,(hl)     ;
 $9a45 cp $18        ;
 $9a47 jr z,$9a4d    ;
 $9a49 cp $1f        ;
 $9a4b jr nz,$9a43   ;
 *$9a4d djnz $9a43    ;
 $9a4f inc hl        ;
 *$9a50 ld a,$80      ;
 $9a52 ld ($9a3b),a  ;
 *$9a55 ld a,(hl)     ;
 $9a56 cp $18        ;
 $9a58 ret z         ;
 $9a59 cp $1f        ;
 $9a5b jr nz,$9a62   ;
 $9a5d ld a,$0d      ;
 $9a5f jp $9b27      ;
 *$9a62 ld ($9ae8),hl ;
 $9a65 call $9a6e    ;
 $9a68 ld hl,($9ae8) ;
 $9a6b inc hl        ;
 $9a6c jr $9a55      ;

 c$9b27 push hl       ;
 $9b28 push de       ;
 $9b29 push bc       ;
 $9b2a push af       ;
 $9b2b cp $0d        ;
 $9b2d jr nz,$9b40   ;
 $9b2f ld a,($5bf6)  ;
 $9b32 ld b,a        ;
 *$9b33 inc b         ;
 $9b34 ld a,$2b      ;
 $9b36 cp b          ;
 $9b37 jr z,$9b74    ;
 $9b39 ld a,$20      ;
 $9b3b call $9b79    ;
 $9b3e jr $9b33      ;
 *$9b40 call $9b79    ;
 $9b43 jr $9b74      ;


 c$9b45 push hl       ;
 $9b46 push de       ;
 $9b47 push bc       ;
 $9b48 push af       ;
 $9b49 ld ($9971),a  ;
 $9b4c ld hl,$6f1d   ;
 $9b4f call $9a3c    ;
 $9b52 jr $9b74      ;

 *$9b74 pop af        ;
 $9b75 pop bc        ;
 $9b76 pop de        ;
 $9b77 pop hl        ;
 $9b78 ret           ;

 ; Routine at 9b79
 ;
 ; Used by the routines at #R$9972 and #R$9b27.
 c$9b79 cp $0c        ;
 $9b7b jr nz,$9b87   ;
 $9b7d call $9b87    ;
 $9b80 ld a,$20      ;
 $9b82 call $9b87    ;
 $9b85 ld a,$0c      ;
 *$9b87 push hl       ;
 $9b88 push bc       ;
 $9b89 push de       ;
 $9b8a push af       ;
 $9b8b cp $0d        ;
 $9b8d jp z,$9bbd    ;
 $9b90 cp $0c        ;
 $9b92 jr nz,$9bd7   ;
 $9b94 ld a,($5bf6)  ;
 $9b97 or a          ;
 $9b98 jr nz,$9ba8   ;
 $9b9a ld a,$29      ;
 $9b9c ld ($5bf6),a  ;
 $9b9f ld a,($5bf5)  ;
 $9ba2 dec a         ;
 $9ba3 ld ($5bf5),a  ;
 $9ba6 jr $9bb3      ;
 *$9ba8 dec a         ;
 $9ba9 ld ($5bf6),a  ;
 $9bac and $03       ;
 $9bae cp $02        ;
 $9bb0 jp z,$9c67    ;
 *$9bb3 ld hl,($5bf3) ;
 $9bb6 dec hl        ;
 $9bb7 ld ($5bf3),hl ;
 $9bba jp $9c67      ;
 *$9bbd ld a,($5bf5)  ;
 $9bc0 cp $17        ;
 $9bc2 jr c,$9bca    ;
 $9bc4 call $9c85    ;
 $9bc7 jp $9c67      ;
 *$9bca ld a,($5bf5)  ;
 $9bcd inc a         ;
 $9bce ld ($5bf5),a  ;
 $9bd1 call $9c6c    ;
 $9bd4 jp $9c67      ;
 *$9bd7 ld de,($5bf3) ;
 $9bdb ld bc,$8dcb   ;
 $9bde sub $20       ;
 $9be0 ld h,$00      ;
 $9be2 ld l,a        ;
 $9be3 add hl,hl     ;
 $9be4 add hl,hl     ;
 $9be5 add hl,hl     ;
 $9be6 add hl,bc     ;
 $9be7 ex de,hl      ;
 $9be8 push hl       ;
 $9be9 ld a,($5bf6)  ;
 $9bec and $03       ;
 $9bee cp $03        ;
 $9bf0 jr z,$9c46    ;
 $9bf2 cp $02        ;
 $9bf4 jr z,$9c21    ;
 $9bf6 cp $01        ;
 $9bf8 jr z,$9c09    ;
 $9bfa ld b,$08      ;
 *$9bfc ld a,(de)     ;
 $9bfd ld (hl),a     ;
 $9bfe inc de        ;
 $9bff inc h         ;
 $9c00 djnz $9bfc    ;
 *$9c02 pop hl        ;
 $9c03 inc hl        ;
 $9c04 ld ($5bf3),hl ;
 $9c07 jr $9c59      ;
 *$9c09 ld b,$08      ;
 *$9c0b ld a,(de)     ;
 $9c0c dec hl        ;
 $9c0d srl (hl)      ;
 $9c0f srl (hl)      ;
 $9c11 sla a         ;
 $9c13 rl (hl)       ;
 $9c15 sla a         ;
 $9c17 rl (hl)       ;
 $9c19 inc hl        ;
 $9c1a ld (hl),a     ;
 $9c1b inc h         ;
 $9c1c inc de        ;
 $9c1d djnz $9c0b    ;
 $9c1f jr $9c02      ;
 *$9c21 ld b,$08      ;
 *$9c23 ld a,(de)     ;
 $9c24 dec hl        ;
 $9c25 srl (hl)      ;
 $9c27 srl (hl)      ;
 $9c29 srl (hl)      ;
 $9c2b srl (hl)      ;
 $9c2d sla a         ;
 $9c2f rl (hl)       ;
 $9c31 sla a         ;
 $9c33 rl (hl)       ;
 $9c35 sla a         ;
 $9c37 rl (hl)       ;
 $9c39 sla a         ;
 $9c3b rl (hl)       ;
 $9c3d inc hl        ;
 $9c3e ld (hl),a     ;
 $9c3f inc h         ;
 $9c40 inc de        ;
 $9c41 djnz $9c23    ;
 $9c43 pop hl        ;
 $9c44 jr $9c59      ;
 *$9c46 ld b,$08      ;
 *$9c48 ld a,$c0      ;
 $9c4a and (hl)      ;
 $9c4b ld (hl),a     ;
 $9c4c ld a,(de)     ;
 $9c4d srl a         ;
 $9c4f srl a         ;
 $9c51 or (hl)       ;
 $9c52 ld (hl),a     ;
 $9c53 inc h         ;
 $9c54 inc de        ;
 $9c55 djnz $9c48    ;
 $9c57 jr $9c02      ;
 *$9c59 pop af        ;
 $9c5a push af       ;
 $9c5b ld a,($5bf6)  ;
 $9c5e cp $29        ;
 $9c60 jp z,$9bbd    ;
 $9c63 inc a         ;
 $9c64 ld ($5bf6),a  ;
 *$9c67 pop af        ;
 $9c68 pop de        ;
 $9c69 pop bc        ;
 $9c6a pop hl        ;
 $9c6b ret           ;


 ; Routine at 9a6e
 ;
 ; Used by the routine at #R$9a3c. Something with printing text messages
 c$9a6e cp $7b        ;
 $9a70 jr c,$9a90    ;
 $9a72 ld hl,$8c8b   ;
 $9a75 sub $7b       ; A = A - 123
 $9a77 jr z,$9a83    ;
 $9a79 ld b,a        ;
 *$9a7a bit 7,(hl)    ;
 $9a7c jr nz,$9a81   ;
 *$9a7e inc hl        ;
 $9a7f jr $9a7a      ;
 *$9a81 djnz $9a7e    ;
 *$9a83 inc hl        ;
 *$9a84 ld a,(hl)     ;
 $9a85 call $9a90    ;
 $9a88 inc hl        ;
 $9a89 bit 7,(hl)    ;
 $9a8b jr z,$9a84    ;
 $9a8d ld a,(hl)     ;
 $9a8e and $7f       ; & 127
 *$9a90 push hl       ;
 $9a91 push de       ;
 $9a92 push bc       ;
 $9a93 push af       ;
 $9a94 cp $20        ; if A != 32 goto 9afc
 $9a96 jr nz,$9afc   ;
 $9a98 ld hl,($9ae8) ;
 $9a9b ld b,$00      ;
 $9a9d ld a,(hl)     ;
 $9a9e cp $ec        ; 236
 $9aa0 jr c,$9aa7    ;
 $9aa2 cp $f0        ; 240
 $9aa4 jr nc,$9aa7   ;
 $9aa6 inc b         ;
 *$9aa7 inc hl        ; Something with printing text
 $9aa8 ld a,(hl)     ;
 $9aa9 cp $20        ; 32
 $9aab jr z,$9aea    ;
 $9aad cp $18        ; 24
 $9aaf jr z,$9aea    ;
 $9ab1 cp $1f        ; 31
 $9ab3 jr z,$9aea    ;
 $9ab5 inc b         ;
 $9ab6 cp $7b        ; 123
 $9ab8 jr c,$9aa7    ;
 $9aba cp $8b        ; 139
 $9abc jr c,$9aea    ;
 $9abe inc b         ;
 $9abf cp $ec        ; 236
 $9ac1 jr c,$9aa7    ;
 $9ac3 cp $f0        ; 240
 $9ac5 jr nc,$9acb   ;
 $9ac7 dec b         ;
 $9ac8 dec b         ;
 $9ac9 jr $9aea      ;
 *$9acb inc b         ;
 $9acc cp $f3        ; 243
 $9ace jr c,$9aa7    ;
 $9ad0 inc b         ;
 $9ad1 cp $f8        ; 248
 $9ad3 jr c,$9aea    ;
 $9ad5 inc b         ;
 $9ad6 inc b         ;
 $9ad7 cp $fc        ; 252
 $9ad9 jr c,$9aea    ;
 $9adb inc b         ;
 $9adc cp $fe        ; 254
 $9ade jr c,$9aea    ;
 $9ae0 jr z,$9aa7    ;
 $9ae2 inc b         ;
 $9ae3 inc b         ;
 $9ae4 inc b         ;
 $9ae5 inc b         ;
 $9ae6 jr $9aa7      ;

 ; Routine at 9aea
 ;
 ; Used by the routine at #R$9a6e.
 c$9aea ld a,($5bf6)  ;
 $9aed or a          ;
 $9aee jp z,$9b74    ;
 $9af1 add a,b       ;
 $9af2 cp $2a        ;
 $9af4 jr c,$9afa    ;
 $9af6 ld a,$0d      ;
 $9af8 jr $9b21      ;
 *$9afa ld a,$20      ;
 ; This entry point is used by the routine at #R$9a6e.
 *$9afc ld hl,$9a3b   ;
 $9aff bit 7,(hl)    ;
 $9b01 jr z,$9b09    ;
 $9b03 cp $60        ;
 $9b05 jr c,$9b09    ;
 $9b07 sub $20       ;
 *$9b09 cp $21        ;
 $9b0b jr c,$9b0f    ;
 $9b0d res 7,(hl)    ;
 *$9b0f cp $21        ;
 $9b11 jr z,$9b1f    ;
 $9b13 cp $3f        ;
 $9b15 jr z,$9b1f    ;
 $9b17 cp $3a        ;
 $9b19 jr z,$9b1f    ;
 $9b1b cp $2e        ;
 $9b1d jr nz,$9b21   ;
 *$9b1f set 7,(hl)    ;
 *$9b21 call $9b27    ;
 $9b24 jp $9b74      ;

 *$9b74 pop af        ;
 $9b75 pop bc        ;
 $9b76 pop de        ;
 $9b77 pop hl        ;
 $9b78 ret           ;

 c$9b27 push hl       ;
 $9b28 push de       ;
 $9b29 push bc       ;
 $9b2a push af       ;
 $9b2b cp $0d        ;
 $9b2d jr nz,$9b40   ;
 $9b2f ld a,($5bf6)  ;
 $9b32 ld b,a        ;
 *$9b33 inc b         ;
 $9b34 ld a,$2b      ;
 $9b36 cp b          ;
 $9b37 jr z,$9b74    ;
 $9b39 ld a,$20      ;
 $9b3b call $9b79    ;
 $9b3e jr $9b33      ;
 *$9b40 call $9b79    ;
 $9b43 jr $9b74      ;


 ; Routine at 9c6c
 ;
 ; Used by the routine at #R$9b79.
 c$9c6c push de       ;
 $9c6d ld a,($5bf5)  ;
 $9c70 add a,a       ;
 $9c71 ld e,a        ;
 $9c72 ld d,$00      ;
 $9c74 ld hl,$9d08   ;
 $9c77 add hl,de     ;
 $9c78 call $9d38    ;
 $9c7b ld ($5bf3),de ;
 $9c7f xor a         ;
 $9c80 ld ($5bf6),a  ;
 $9c83 pop de        ;
 $9c84 ret

 ; Routine at 9c85
 ;
 ; Used by the routine at #R$9b79.
 c$9c85 ld a,($5bf1)  ;
 *$9c88 ld l,a        ;
 $9c89 ld h,$00      ;
 $9c8b ld de,$9d08   ;
 $9c8e add hl,hl     ;
 $9c8f add hl,de     ;
 $9c90 call $9d38    ;
 $9c93 push de       ;
 $9c94 inc a         ;
 $9c95 ld l,a        ;
 $9c96 ld h,$00      ;
 $9c98 ld de,$9d08   ;
 $9c9b add hl,hl     ;
 $9c9c add hl,de     ;
 $9c9d call $9d38    ;
 $9ca0 ex de,hl      ;
 $9ca1 pop de        ;
 $9ca2 ld b,$08      ;
 *$9ca4 push hl       ;
 $9ca5 push bc       ;
 $9ca6 push de       ;
 $9ca7 ld bc,$0020   ; copy 32 bytes
 $9caa ldir          ;
 $9cac pop de        ;
 $9cad pop bc        ;
 $9cae pop hl        ;
 $9caf inc h         ;
 $9cb0 inc d         ;
 $9cb1 djnz $9ca4    ;
 $9cb3 cp $17        ;
 $9cb5 jr nz,$9c88   ;
 $9cb7 ld hl,$50e0   ;
 $9cba push hl       ;
 $9cbb pop de        ;
 $9cbc inc de        ;
 $9cbd ld b,$08      ;
 *$9cbf push de       ;
 $9cc0 push hl       ;
 $9cc1 push bc       ;
 $9cc2 ld (hl),$00   ;
 $9cc4 ld bc,$001f   ;
 $9cc7 ldir          ;
 $9cc9 pop bc        ;
 $9cca pop hl        ;
 $9ccb pop de        ;
 $9ccc inc h         ;
 $9ccd inc d         ;
 $9cce djnz $9cbf    ;
 $9cd0 ld hl,$50e0   ;
 $9cd3 ld ($5bf3),hl ;
 $9cd6 xor a         ;
 $9cd7 ld ($5bf6),a  ;
 $9cda ret           ;
 */


