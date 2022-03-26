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
uint8_t A, carry, pushedA, xpos, ypos, blockwidth, blockheight, lastimgblock = 0xff, repeats, widthcounter = 0, heightcounter = 0;

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

uint16_t getImgBlockAddress(uint8_t blocknum);

void DisplayInit(void)
{
    LoadMemory();
    SagaSetup();
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    OpenTopWindow();
    OpenGraphicsWindow();
    glk_window_clear(Graphics);
    for (int i = 1; i < 11; i++) {

//        current_block = A;
       HL = getImgBlockAddress(i);
//        xpos = ypos = 0;
//
        draw_image(A);
        attributes_start = 0x5800;
        draw_attributes();
        fprintf(stderr, "Room image: %d\n", i);
        draw_spectrum_screen_from_mem();
//        for (int i = 0x4000; i<0x5800; i++) {
//            mem[i] = 0;
//        }
//        for (int i = 0x5800; i<0x5800 + 384; i++) {
//            mem[i] = 0;
//        }
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

void call6B07(void);
void call6B27(void);
void call6A87(void);



static void draw_spectrum_screen_from_mem(void) {
    OpenGraphicsWindow();

    //    for (int i = 0; i < 6144; i++)
    //    {
    //        if (mem[0x4000+i] != forest_mem[i])
    //            fprintf(stderr, "Error: wrong byte in graphics memory at %x! Expected %x, got %x\n", 0x4000+i,forest_mem[i], mem[0x4000+i]);
    //    }




    print_screen_memory();


    for (int i = 0; i < 768; i++)
    {
//        glui32 paper = (mem[0x6b59+i] >> 3) & 0b111;
//        paper += 8 * ((mem[0x6b59+i] & 64) == 64);
                glui32 paper = (mem[0x5800+i] >> 3) & 0b111;
                paper += 8 * ((mem[0x5800+i] & 64) == 64);

 paper = 15;
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

//        glui32 ink = (mem[0x6b59+(y/8) * 32 + (x/8)] & 0b111);
//        ink += 8 * ((mem[0x6b59+(y/8) * 32 + (x/8)] & 64) == 64);

                glui32 ink = (mem[0x5800+(y/8) * 32 + (x/8)] & 0b111);
                ink += 8 * ((mem[0x5800+(y/8) * 32 + (x/8)] & 64) == 64);

ink = 0;

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

void call6B07(void)
{
    mem[0x643A] = mem[HL];
    mem[0x643C] = mem[HL++];
    mem[0x643B] = mem[HL];
    mem[0x643D] = mem[HL++];
    mem[0x6437] = mem[HL];
    BC = mem[HL++];
    A = mem[HL];
    mem[0x6438] = A;
    mem[0x6439] = A;
}

void call6B27(void)
{
    BC = mem[HL] * 256 + mem[HL+1];
    HL = HL +2;

    uint16_t pushedHL = HL;
    A = mem[0x6404];
    HL = 0x60D3;
    carry = (A & 1);
    if (!carry)
        HL = 0x60D7;
    uint16_t pushedHL2 = HL;
    mem[HL++] = BC >> 8;
    mem[HL++] = BC & 0xff;
    if (!carry) {
        A = mem[0x60D2];
        if (A >= 8)
        {
            BC = 0x0700;
            if (A == 8)
                goto jump6B5F;
            BC = 0xA00;
            if (A == 9)
                goto jump6B5F;
            BC = 0x070C;
        } else {
            carry = ((A & 128) == 128);
            A = (A<<2);
            BC = A;
        }
    jump6B5F:
        mem[HL++] = BC & 0xff;
        mem[HL] = BC >> 8;
    }
    HL = pushedHL2;

    mem[0x60CB] = HL & 0xff;
    mem[0x60CC] = HL >> 8;

    call6B07();

    HL = pushedHL;
    HL++;
}

void call60c1(void) {
    // Move down one line?
    HL = 0x5bd0 + A * 2;
    DE = mem[HL] + mem[HL+1] * 256;
    HL++;
}

int calla050(void) // Get next adress?
{
    current_screen_address++; // byte to the right
    DE = current_screen_address;
    widthcounter--;
    if (widthcounter != 0)
        return 0;
    heightcounter--;
    if (heightcounter == 0)
        return 1;
    widthcounter = blockwidth;
    mem[0x9ec3]++;
    HL = 0x9d08 + mem[0x9ec3] * 2;
    current_screen_address = xpos + mem[HL] + mem[HL+1] * 256;
    return 0;
}

void call7532(void) {
    HL = A * 2;
    DE = 0x5bd0;
    print_memory(0x5bd0, 8);
    HL = HL + DE;
    DE = (DE & 0xFF00) + mem[HL];
    HL++;
    DE = (DE & 0xFF) + mem[HL] * 256;
}

uint16_t stored_address;

void call7131(void) {
    uint16_t addr = 0x5b9e;
    HL += 2;
    fprintf(stderr, "Copying %d bytes from 0x%04x to 0x5b9e\n", mem[HL-1], HL);
    for (int i = mem[HL-1]; i > 0; i--) {
        fprintf(stderr, "Copying %x from 0x%04x to 0x%04x\n", mem[HL], HL, addr);
        mem[addr++] = mem[HL++];
    }
    stored_address = HL;
    fprintf(stderr, "Stored address 0x%04x\n", stored_address);
    HL = 0x5b9f;
    fprintf(stderr, "Set HL to point at second copied instruction (%02x).\n", mem[HL]);
    mem[addr] = 0xfb;
}

void call711d(void) {
    if (mem[0x5b9e] == 0) {
        HL = stored_address;
        fprintf(stderr, "Restored HL to stored address 0x%04x\n", stored_address);
    } else {
        mem[0x5b9e]--;
        HL = 0x5b9f;
        fprintf(stderr, "Set HL to point at second copied instruction (%02x).\n", mem[HL]);
}
    A = mem[HL];
}

//Routine at 6ec0

void draw_image(int img)
{
    fprintf(stderr, "draw_image %d\n", img);
    A = img;



    uint16_t address = 0x9d08 + A * 2;

    current_screen_address = mem[address] + mem[address+1] * 256 + BC; // 0x5bba = current_screen_address
    fprintf(stderr, "xpos: %d ypos: %d\n", xpos, ypos);
    fprintf(stderr, "current_screen_address: %04x\n", current_screen_address);
    if (current_screen_address < 0x4000 || current_screen_address > 0x5B00) {
        fprintf(stderr, "Something went wrong\n");
        //        return;
    }

    HL++; // Address to current draw instruction in "image data"
    instructionaddress = HL;
    uint8_t current_instruction;
    uint8_t flip_mode = 0;
    repeats = 0;
    do {
    jump6ed5:
        current_instruction = 0;
        A = mem[HL];


        instructionaddress = HL;
        if (A >= 128) {
            // Bit 7 set, this is a command byte
            current_instruction = A; // draw instruction is stored in 0x5bbe and 0x9ebf
            mem[0x9ebf] = A;
            // Test bit 1, repeat flag
            if ((A & 2) == 2) {
                HL++;
                repeats = mem[HL]; // and store it in repeats
            }
            HL++;
            instructionaddress = HL;
            A = mem[HL];
        }
    jump6f32:
        HL = A;
        BC = 0xa100; // Start of characters
        if ((mem[0x9ebf] & 1) == 1) { // Test bit 0, whether to add 127 to character index
            BC += 0x400;
        }

        IX = HL * 8 + BC;
        HL = 0x9ec8;
        BC = (BC & 0xff) + 0x08 * 256;
        A = (current_instruction & 0x30);  // Current draw instruction bits 4 and 5
        switch (A) {
            case 0: // do nothing, straight copy
                for (int i = 0; i < 8; i++)
                    mem[HL++] = mem[IX++];
                break;
            case 0x10: // rotate 90 degrees
                for (int C = 8 ; C > 0 ; C--) {
                    A = mem[IX];
                    for (int B = 8 ; B > 0 ; B--) {
                        carry = (A >= 128);
                        A = A << 1;
                        carry = rotate_right_with_carry(&mem[HL++], carry);
                    }
                    HL = 0x9ec8;
                    IX++;
                }
                break;
            case 0x20:  // rotate 180 degrees
                HL = 0x5bb9;
                for (int C = 0; C < 8; C++) {
                    A = mem[IX];
                    for (int B = 0; B < 8; B++) {
                        carry = (A >= 128);
                        A = A << 1;
                        carry = rotate_right_with_carry(&mem[HL], carry);
                    }
                    IX++;
                    HL--;
                }
                break;

            case 0x30: // Rotate 270 degrees
                for (int C = 0; C < 8 ; C++) {
                    A = mem[IX];
                    for (int B = 0; B < 8 ; B++) {
                        carry = A & 1;
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

        A = current_instruction & 0x40; // bit 6
        if (A != 0) {  // flip horizontally
            HL = 0x9ec8;
            for (int C = 0; C < 8 ; C++) {
                A = mem[HL];
                for (int B = 0; B < 8 ; B++) {
                    carry = (A >= 128);
                    A = A << 1;
                    carry = rotate_right_with_carry(&mem[HL], carry);
                }
                HL++;
            }
        }
        DE = current_screen_address; // Screen adress
        HL = 0x9ec8;
        for (int B = 0; B < 8; B++) {
            A = flip_mode;
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
                         //                          fprintf(stderr, "Set address 0x%04x to 0x%02x\n", DE, mem[DE]);
            mem[HL++] = A;
            DE = DE + 256; // Next line
        }
        flip_mode = current_instruction & 0x0c;
        if (flip_mode == 0) { // No flip
            while (repeats != 0) {
                if (calla050())  // Get screen address for next character?
                    return;
                HL = 0x9ec8;
                DE = current_screen_address; // Screen adress

                for (int B = 0; B < 8; B++) {
                    A = mem[HL++];
                    mem[DE] = A;
                    //                    fprintf(stderr, "Set address 0x%04x to 0x%02x\n", DE, mem[DE]);
                    DE = DE + 256;
                }

                repeats--;
            }
            if (calla050())
                return;
        }
        instructionaddress++;
        HL = instructionaddress;
    } while (HL < 0xffff);
}


// 7077
void draw_attributes(void) { // Draw attributes
    uint8_t lastAttribute = 0;
    fprintf(stderr, "Draw attributes for image\n");
    HL = instructionaddress + 1;
    widthcounter = blockwidth;
    uint16_t screenAddress = attributes_start + ypos * 32 + xpos;
    fprintf(stderr, "draw_attributes: draw instructions at 0x%04x. x:%d y:%d width:%d\n", HL, xpos, ypos, blockwidth);
    do {

        IX = instructionaddress;
        A = mem[instructionaddress];
        if (A >= 128) { // Bit 7 is repeat flag
            repeats = (A & 0x7f) - 1;
            A = lastAttribute;
        }
    jump70c2:
        mem[screenAddress] = A;
        //        fprintf(stderr, "Attribute address 0x%04x set to 0x%02x\n", DE, A);
        lastAttribute = A;
        screenAddress++;
        widthcounter--;
        if (widthcounter == 0) {
            widthcounter = blockwidth;
            screenAddress += 32 - widthcounter;
        }

        if (repeats != 0) {
            repeats--;
            A = lastAttribute;
            goto jump70c2;
        }
        HL++;
    } while (HL < 0xffff);
}

void call22aa(void);
void call743c(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
uint16_t getImgBlockAddress(uint8_t blocknum);
void call74eb(int skip);
void call6fd0(void);
void call74e9(int skip);
void call7736(int skip);
void call73ae(void);
void call73f2(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
//void mirror_attributes(void);
void call74ae(void);
void call73f1(void);
void call72d5(void);
void call748b(void);
void call76f7(void);
void call74be(void);
void call74dd(void);
void call76c9(void);

static void flip_horizontally() {
    uint16_t lastPushed;
    BC = 0x6000;
    uint8_t C = BC & 0xff;
    uint8_t B = BC >> 8;
    carry = 0;
jump6e5a:
    pushedBC = BC;
    A = 0x4f + (BC >> 8);
    BC = (BC & 0xff) + A * 256;
    call22aa();
    DE = HL;
    HL = HL + 0x001f;
    BC = (BC & 0xff) + 0x10 * 256;
jump6e6a:
    A = mem[DE];
    BC = (BC & 0xff00) + 0x80;
jump6e6d:
    carry = rotate_left_with_carry(&A, carry);
    C = BC & 0xff;
    carry = rotate_right_with_carry(&C, carry);
    BC = (BC & 0xff00) + C;
    if (carry == 0)
        goto jump6e6d;
    A = mem[HL];
    mem[HL] = BC & 0xff;
    BC = (BC & 0xff00) + 0x80;
jump6e77:
    carry = rotate_left_with_carry(&A, carry);
    C = BC & 0xff;
    carry = rotate_right_with_carry(&C, carry);
    BC = (BC & 0xff00) + C;
    if (carry == 0)
        goto jump6e77;
    A = BC & 0xff;
    mem[DE++] = A;
    HL--;
    B = BC >> 8;
    //    fprintf(stderr, "Inner loop. B is now %d\n", B);
    B--;
    BC = (BC & 0xff) + B * 256;
    if (B != 0)
        goto jump6e6a;
    BC = pushedBC;
    B = BC >> 8;
    //    fprintf(stderr, "Outer loop. B is now %d\n", B);
    B--;
    BC = (BC & 0xff) + B * 256;
    if (B != 0)
        goto jump6e5a;

    HL = 0x6b59;
    DE = 0x6b78;
    BC = (BC & 0xff) + 0x0c * 256;
jump6e8e:
    pushedBC = BC;
    BC = (BC & 0xff) + 0x10 * 256;
    pushedDE = DE;
    lastPushed = pushedDE;
jump6e92:
    A = mem[HL];
    mem[HL] = mem[DE];
    mem[DE] = A;
    DE--;
    HL++;
    B = BC >> 8;
    B--;
    BC = (BC & 0xff) + B * 256;
    if (B != 0)
        goto jump6e92;
    HL = lastPushed;
    HL++;
    pushedHL = HL;
    lastPushed = HL;
    DE = 0x001f;
    HL = HL + DE;
    temp = HL;
    HL = DE;
    DE = temp;
    HL = pushedHL;
    BC = pushedBC;
    B = BC >> 8;
    B--;
    BC = (BC & 0xff) + B * 256;
    if (B != 0)
        goto jump6e8e;
}


//void call743c(int skip) {
//    if (!skip) {
//        BC = mem[IX + 2] + mem[IX + 1] * 256;
//        mem[0x741c] = mem[IX + 3];
//        mem[0x7437] = mem[0x741c];
//        A = mem[IX + 4];
//    }
//    mem[0x73f8] = A;
//    mem[0x7426] = A;
//    A = A >> 1;
//    mem[0x7403] = A;
//    mem[0x742d] = A;
//    uint16_t oldPushedBC = BC;
//    call73f2();
//    BC = oldPushedBC;
//    mirror_attributes();
//}

// 7421
void mirror_attributes(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    do {
        HL = y1 * 32 + 0x6b59; // Attributes buffer
        HL = HL + x1;
        DE = HL;
        HL += x2 - 1;
        for (int i = 0; i < x2 / 2; i++) {
            mem[HL--] = mem[DE++];
        }
        y1++;
    } while (y2 != y1);
}

void call73f2(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    uint16_t myPushedHL;
    uint16_t myPushedDE;
    carry = 0;
    do {
        uint16_t addr = 0x5bd0 + y1 * 2;
        DE = mem[addr] + mem[addr+1] * 256 + x1;
        HL = DE + x2 - 1;
        for (int j = 0; j < 8; j++) {
            myPushedHL = HL;
            myPushedDE = DE;
            for (int i = 0; i < x2 / 2; i++) {
                A = mem[DE];
                uint8_t C = 0x80;
                do {
                    carry = rotate_left_with_carry(&A, carry);
                    carry = rotate_right_with_carry(&C, carry);
                } while (!carry);
                mem[HL] = C;
                DE++;
                HL--;
            }
            DE = myPushedDE + 256;
            HL = myPushedHL + 256;
        }
        y1++;
    } while (y1 != y2);
}



uint16_t getImgBlockAddress(uint8_t img) {
    A = img - 1;

    // 0xa8c0 start of image offsets lookup
    // 0xa916 start of image data

    uint16_t address = (mem[0x9e80 + A] & 0x7f) * 2 + 0xa8c0;
    HL = 0xa916 + mem[address] + mem[address+1] * 256;
    mem[0x9ebc] = HL & 0xff;
    mem[0x9ebd] = HL >> 8;
    blockwidth = mem[HL]; // (9ebc) = blockwidth
    widthcounter = blockwidth;
    HL++;
    heightcounter = mem[HL];
    blockheight = heightcounter;
    HL++;
    xpos = mem[HL];
    BC = xpos;
    HL++;
    ypos = mem[HL];
    mem[0x9ec2] = ypos;
    mem[0x9ec3] = ypos;

    return HL;
}

void call74eb(int skip) {
    if (!skip) {
        A = mem[IX + 1];
        mem[0x74e7] = A;
        mem[0x74e9] = A;
        A = mem[IX + 2];
        mem[0x74e8] = A;
        A = mem[IX + 3];
        mem[0x74ea] = A;
        A = mem[IX + 4];
        mem[0x74c9] = A;
        mem[0x7496] = A;
        A = mem[IX + 3] - mem[IX + 2];
        carry = rotate_right_with_carry(&A, 0);
    }
    mem[0x7333] = A;
    mem[0x72fa] = A;
    BC = mem[0x7337] + mem[0x7338] * 256;
    DE = mem[0x7733] + mem[0x733a] * 256;
    uint16_t myPushedBC = BC;
    uint16_t myPushedDE = DE;
    call74ae();
    DE = myPushedDE;
    BC = myPushedBC;
    mem[0x7337] = BC & 0xff;
    mem[0x7338] = BC >> 8;
    mem[0x7339] = DE & 0xff;
    mem[0x733a] = DE >> 8;
    call72d5();
}

void call22aa(void) { // THE 'PIXEL ADDRESS' SUBROUTINE
    A = 0xaf - (BC >> 8);
    if ((BC >> 8) > 175) {
        fprintf(stderr, "B too large, returning\n");
        return;
    }
    BC = (BC & 0xff) + A * 256;
    //    fprintf(stderr, "B now contains 175 minus y: %x (%d)\n", BC >> 8, BC >> 8);

    rotate_right_with_carry(&A, 0);
    rotate_right_with_carry(&A, 1);
    rotate_right_with_carry(&A, 0);
    A = A ^ (BC >> 8);
    A = A & 0xf8; // 11111000
    A = A ^ (BC >> 8);
    HL = (HL & 0xff) + A * 256;
    A = BC & 0xff;
    A = A << 3;
    A = A ^ (BC >> 8);
    A = A & 0xc7; // 11000111
    A = A ^ (BC >> 8);
    A = A << 2;
    HL = (HL & 0xff00) + A;
    A = BC & 0xff;
    A = A & 0x7;
}

void call6fd0(void) {
    HL = 0x5800; // Attributes memory
    DE = 0x5801;
    BC = 0x17f;
    mem[HL] = 0;
    do{
        mem[DE++] = mem[HL++];
        BC--;
    } while (BC != 0);
    HL = 0x5800; // Attributes memory
    DE = 0x6b59; // Attributes buffer
    BC = 0x180;
    do{
        mem[DE++] = mem[HL++];
        BC--;
    } while (BC != 0);
    BC = 0xaf00;
jump6feb:
    pushedBC = BC;
    call22aa(); // THE 'PIXEL ADDRESS' SUBROUTINE
    DE = HL + 1;
    BC = 0x1f;
    do{
        mem[DE++] = mem[HL++];
        BC--;
    } while (BC != 0);
    BC = pushedBC;
    BC = BC - 256;
    A = 0x4f;
    if (A != (BC >> 8))
        goto jump6feb;
}

void call74e9(int skip) {
    if (!skip) {
        A = mem[IX + 3];
        mem[0x74b9] = A;
        mem[0x74d8] = A;
        A = mem[IX + 4];
        BC = mem[IX + 2] + mem[IX + 1] * 256;
    }
    mem[0x7491] = A;
    mem[0x74c3] = A;
    carry = rotate_right_with_carry(&A, 0);
    mem[0x749c] = A;
    mem[0x74ca] = A;
    uint16_t oldPushedBC = BC;
    call748b();
    BC = oldPushedBC;
    call74be();
}

void call7736(int skip) {
    if (!skip) {
        A = mem[IX + 1];
        mem[0x74e7] = A; // y1
        mem[0x74e9] = A; // y1
        A = mem[IX + 2];
        mem[0x74e8] = A; // x1
        A = mem[IX + 3];
        mem[0x74ea] = A; // y2
        A = mem[IX + 4];
        mem[0x76da] = A; // width
        mem[0x7712] = A; // width
        A = mem[IX + 3] + mem[IX + 2]; // A = (x1 + width) / 2
        carry = rotate_right_with_carry(&A, 0);
    }
    mem[0x76f3] = A;
    mem[0x7732] = A;
    BC = mem[0x74e7] + mem[0x74e8] * 256;
    DE = mem[0x74e9] + mem[0x74ea] * 256;
    uint16_t myPushedBC = BC;
    uint16_t myPushedDE = DE;
    call76f7();
    BC = myPushedBC;
    DE = myPushedDE;
    mem[0x74e7] = BC & 0xff;
    mem[0x74e8] = BC >> 8;

    mem[0x74e9] = DE & 0xff;
    mem[0x74ea] = DE >> 8;
    call76c9();
}


void call73ae(void) {
    DE = 0x6b59; // Attributes buffer
    HL = (BC >> 8) * 32 + DE;
    BC = BC & 0xff;
    HL = HL + BC;
}




//void mirror_attributes(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
//    do {
//        HL = (BC >> 8) * 32 + 0x6b59; // Attributes buffer
//        HL = HL + (BC & 0xff);
//        DE = HL;
//        HL += mem[0x7426] + mem[0x7427] * 256 - 1;
//        for (int i = 0; i < mem[0x742d]; i++) {
//            mem[HL--] = mem[DE++];
//        }
//        BC += 256;
//    } while (mem[0x7437] != BC >> 8);
//}

void call74ae(void) {
    do {
        uint16_t addr1 = 0x5bd0 + mem[0x74e8] * 2;
        HL = mem[addr1] + mem[addr1 + 1] * 256 + mem[0x74e7];
        uint16_t addr2 = 0x5bd0 + mem[0x74ea] * 2;
        DE = mem[addr2] + mem[addr2 + 1] * 256 + mem[0x74e9];
        A = (DE >> 8) + 7;
        DE = (DE & 0xff) + A * 256;
        uint16_t dest, source;
        for (int j = 0; j < 8; j++) {
            dest = DE;
            source = HL;
            for (uint i = mem[0x74c9] + mem[0x74ca] * 256; i != 0; i--) {
                mem[dest++] = mem[source++];
            }
            uint8_t H = (HL >> 8) + 1;
            HL = (HL & 0xff) + H * 256;
            uint8_t D = (DE >> 8) - 1;
            DE = (DE & 0xff) + D * 256;
        }
        mem[0x74e8]++;
        mem[0x74ea]--;
    } while (mem[0x74ea] != mem[0x74e3]);
}

void call73f1(void) {
    A = BC >> 8;
    call60c1();
    HL = DE;
    BC = BC & 0xff;
    HL = HL + BC;
}

void call72d5(void) {
jump7485:
    BC = mem[0x74e7] + mem[0x74e8] * 256;
    call73ae();
    uint16_t var = HL;
    BC = mem[0x74e9] + mem[0x74ea] * 256;
    call73ae();
    DE = var;
    BC = mem[0x7496] + mem[0x7497] * 256;
    temp = HL;
    HL = DE;
    DE = temp;
    do {
        mem[DE++] = mem[HL++];
        BC--;
    } while (BC != 0);
    mem[0x74e8]++;
    mem[0x74ea]--;
    if (mem[0x74ea] != mem[0x74aa])
        goto jump7485;
}

void call748b(void) {
    uint16_t prevPushedBC;
jump75b6:
    prevPushedBC = BC;
    call73f1();
    uint16_t var = HL;
    DE = mem[0x7491] + mem[0x7492] * 256;
    BC = (BC & 0xff) + 0x08 * 256;
    HL += DE;
    HL--;
    DE = var;
jump75c3:
    pushedHL = HL;
    pushedDE = DE;
    pushedBC = BC;
    BC = (BC & 0xff) + mem[0x749c] * 256;
jump75c8:
    call74dd();
    temp = HL;
    HL = DE;
    DE = temp;
    call74dd();
    temp = HL;
    HL = DE;
    DE = temp;
    A = mem[DE];
    BC = (BC & 0xff00) + mem[HL];
    temp = HL;
    HL = DE;
    DE = temp;
    mem[DE] = A;
    mem[HL] = BC & 0xff;
    temp = HL;
    HL = DE;
    DE = temp;
    DE++;
    HL--;
    uint8_t B = BC >> 8;
    B--;
    BC = (BC & 0xff) + B * 256;
    if (B != 0)
        goto jump75c8;
    BC = pushedBC;
    DE = pushedDE;
    HL = pushedHL;
    DE += 256;
    HL += 256;
    B = BC >> 8;
    B--;
    BC = (BC & 0xff) + B * 256;
    if (B != 0)
        goto jump75c3;
    BC = prevPushedBC + 256;
    A = mem[0x74b9];
    if (A != (BC >> 8))
        goto jump75b6;
}

void call76f7(void) {
    uint16_t myPushedDE;
    uint16_t myPushedHL;
    do {
        HL = 0x5bd0 + mem[0x74e8] * 2;
        HL = mem[HL] + mem[HL+1] * 256 + mem[0x74e7];
        myPushedHL = HL;
        HL = 0x5bd0 + mem[0x74ea] * 2;
        HL = mem[HL] + mem[HL+1] * 256 + mem[0x74e9];
        A = 7 + (HL >> 8);
        DE = (HL & 0xff) + A * 256;
        HL = myPushedHL;
        for (uint8_t j = 0; j < 8; j++) {
            myPushedHL = HL;
            myPushedDE = DE;
            for (uint8_t i = 0; i < mem[0x7712]; i++) {
                uint8_t C = mem[HL];
                A = mem[DE];
                temp = HL;
                HL = DE;
                DE = temp;
                mem[HL++] = C;
                mem[DE++] = A;
            }
            DE = myPushedDE;
            HL = myPushedHL;
            HL += 256;
            DE -= 256;
        }
        mem[0x74e8]++;
        mem[0x74ea]--;
    } while (mem[0x74ea] != mem[0x7732]);
}

void call74be(void) {
jump74be:
    pushedBC = BC;
    call73ae();
    DE = mem[0x74c3] + mem[0x74c4] * 256;
    pushedHL = HL;
    HL += DE;
    HL--;
    DE = pushedHL;
    BC = (BC & 0xff) + mem[0x74ca] * 256;
jump74cb:
    A = mem[DE];
    BC = (BC & 0xff00) + mem[HL];
    temp = HL;
    HL = DE;
    DE = temp;
    mem[HL] = BC & 0xff;
    mem[DE] = A;
    temp = HL;
    HL = DE;
    DE = temp;
    DE++;
    HL--;
    uint8_t B = BC >> 8;
    B--;
    BC = (BC & 0xff) + B * 256;
    if (B != 0)
        goto jump74cb;
    BC = pushedBC;
    BC += 256;
    A = mem[0x74d8];
    if (A != (BC >> 8))
        goto jump74be;
}

void call74dd(void) {
    BC = (BC & 0xff00) + 0x80;
jump74e0:
    carry = rotate_left_with_carry(&mem[HL], carry);
    uint8_t C = BC & 0xff;
    carry = rotate_right_with_carry(&C, carry);
    BC = (BC & 0xff00) + C;
    if (!carry)
        goto jump74e0;
    mem[HL] = C;
}

void call76c9(void) {
    do {
        DE = 0x6b59 + mem[0x74e8] * 32 + mem[0x74e7];
        HL = 0x6b59 + mem[0x74ea] * 32 + mem[0x74e9];
        for (int i = 0; i < mem[0x76da]; i++) {
            A = mem[DE];
            uint8_t C = mem[HL];
            temp = HL;
            HL = DE;
            DE = temp;
            mem[HL] = C;
            mem[DE] = A;
            HL++;
            DE++;
        }
        mem[0x74e8]++;
        mem[0x74ea]--;
    } while (mem[0x74ea] != mem[0x76f3]);
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
 *$a088 pop af        ;
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
