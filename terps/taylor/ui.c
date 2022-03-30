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


uint16_t HL, BC, DE, IX, current_screen_address, temp, attributes_start, instructionaddress, stored_address, lastblockaddr, SP, AF;
uint8_t A, carry, xoff, yoff, width, height, column, lastimgblock = 0xff, columnstodraw = 0, rowstodraw = 0, repeats;

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


void DisplayInit(void)
{
    LoadMemory();
    SagaSetup();
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    OpenTopWindow();
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
    fprintf(stderr, "BC:%X DE:%X IX:%X carry:%d SP:%X\n", BC, DE, IX, carry, SP);

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


void push(uint16_t reg) {
    SP -= 2;
    mem[SP] = reg & 0xff;
    mem[SP + 1] = reg >> 8;
    //
    //    fprintf(stderr, "Pushing 0x%04x onto the stack. SP = 0x%04x\n", mem[SP] + mem[SP + 1] * 256, SP);
    //
}

uint16_t pop(void) {
    uint16_t reg = mem[SP] + mem[SP + 1] * 256;
    //    fprintf(stderr, "Popping 0x%04x from the stack. SP = 0x%04x\n", mem[SP] + mem[SP + 1] * 256, SP + 2);
    SP += 2;
    return reg;
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
    push(IX);

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

void print_token(uint8_t token);
void print_char(uint8_t c);
void call9b27(void);

void pop_registers(void) {
    AF = pop();
    A = AF >> 8;
    BC = pop();
    DE = pop();
    HL = pop();
}

void push_registers(void) {
    push(HL);
    push(DE);
    push(BC);
    AF = A * 256;
    push(AF);
}

//; Routine at 9a3c

uint8_t printxpos, uppercase;

void print_string(int index, uint16_t address) {
    while (index > 0) {
        while (mem[address] != 0x1f && mem[address] != 0x18) {
            address++;
        }
        index--;
        address++;
    }
    uppercase = 1;
    do  {
        if (mem[address] == 0x18) {
            return;
    }
        print_token(mem[address]);
        address++;
    } while (mem[address] != 0x1f);
    uint8_t i = printxpos;
    while(1) {
        i++;
        if (i == 0x2b) {
        return;
    }
        print_char(' ');
    }
}

void print_room(int room) {
    print_string(room, 0x68a5); // Room descriptions
}

// Routine at 9b45 Print system messages

void print_system_message(void) { // Print sysmess
    print_string(A, 0x6f1d);  // System messages
}

void print_object(void) { // Print object
    print_string(A - 1, 0x6c7a); // Object descriptions
    }

void print_message(void) { // Print message
    print_string(A , 0x6000); // Messages
    }

int skip = 0;

void print_char(uint8_t c) { // Print character
    if (c == 0x0d)
        return;
    if (isascii(c))
        fprintf(stderr, "%c", c);
    if (printxpos != 0x29)
        printxpos++;
    }

static uint16_t TT(uint8_t n)
{
    uint16_t address = 0x8c8b;
    n -= 0x7b;
    while(n > 0) {
        while((mem[address] & 0x80) == 0)
            address++;
        n--;
        address++;
        }
    return address;
    }

void print_token(uint8_t c) {
    uint16_t address = 0;
    if (!skip && c >= 0x7b) { // if c is >= 0x7b it is a token
        uint16_t addr = TT(c);
        do {
            c = mem[addr];
            skip = 1;
            print_token(c);
            addr++;
        } while ((mem[addr] & 0x80) == 0);
        c = mem[addr] & 0x7f;
    }
    skip = 0;
    if (c == ' ') {
        uint8_t counter = 0;
        if (mem[address] >= 0xec && mem[address] < 0xf0)
            counter++;
    nextChar:
        address++;
        c = mem[address];
        if (c == 0x20 || c == 0x18 || c == 0x1f)
        goto jump9aea;
        counter++;
        if (counter < 0x7b)
            goto nextChar;
        if (counter < 0x8b)
        goto jump9aea;
        counter++;
        if (counter < 0xec)
            goto nextChar;
        if (counter < 0xf0) {
            counter -= 2;
        goto jump9aea;
        }
        counter++;
        if (counter < 0xf3)
            goto nextChar;
        counter++;
        if (counter < 0xf8)
        goto jump9aea;
        counter += 2;
        if (counter < 0xfc)
    goto jump9aea;
        counter++;
        if (counter < 0xfe)
        goto jump9aea;
        if (counter == 0xfe)
            goto nextChar;
        counter += 4;
        goto nextChar;
jump9aea:
        if (printxpos == 0) {
            return;
        }
        if (printxpos + counter >= 0x2a) {
            c = '\n';
            goto printChar;
        }
        c = 0x20;
    }
    if (uppercase && c >= 'a') {
        c -= 0x20; // token is made uppercase
    }
    if (c > '!') {
        uppercase = 0;
        }
    if (c == '!' || c == '?' || c == ':' || c == '.') {
        uppercase = 1;
        }

printChar:
    if (c != '\n') {
        print_char(c);
    } else {
        uint8_t i = printxpos + 1;
        while (i != 0x2b) { // print iterations spaces
            print_char(' ');
            i++;
        }
    }
}
