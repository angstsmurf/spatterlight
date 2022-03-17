/*
 *	Simple curses UI implementation. You probably want to replace
 *	this with something more useful
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#include "glk.h"
#include "glkimp.h"
#include "glkstart.h"

#include "taylor.h"
#include "utility.h"
#include "sagadraw.h"


#define GLK_BUFFER_ROCK 1
#define GLK_STATUS_ROCK 1010
#define GLK_GRAPHICS_ROCK 1020

winid_t Bottom, Top, Graphics;
//static int OutputPos;
//static int OutLine;
//static int OutC;
//static char OutWord[128];
//static int SavedPos;

glui32 Width; /* Terminal width */
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



void PrintCharacter(unsigned char c)
{
    Display(Bottom, "%c", c);
//	if(OutC == 0 &&  c ==' ')
//		return;
//	if(Window == 1) {
//		if(isspace(c)) {
//            WriteChar(Bottom, " ");
//			return;
//		}
//		OutWord[OutC] = c;
//		OutC++;
//		if(OutC > 79)
//			WordFlush(Bottom);
//		else if(OutC + OutputPos > 78)
//			Scroll(Bottom);
//		return;
//	} else {
//		if(isspace(c)) {
//			WordFlush(Top);
//			WriteChar(Top, ' ');
//			if(c == '\n') {
//				OutLine++;
//				OutputPos = 0;
//				wmove(Top, OutLine, OutputPos);
//			}
//			return;
//		}
//		OutWord[OutC] = c;
//		OutC++;
//		if(OutC == 78)
//			WordFlush(Top);
//		else if(OutC + OutputPos > 78) {
//			OutLine++;
//			OutputPos = 0;
//		}
//		return;
//	}
}

unsigned char WaitCharacter(void)
{
    glk_request_char_event(Bottom);

    event_t ev;
    do {
        glk_select(&ev);
    } while (ev.type != evtype_CharInput);
    return ev.val1;
}

void Look(void);

void LineInput(char *buf, int len)
{
    event_t ev;

    glk_request_line_event(Bottom, buf, len - 1, 0);

    while(1)
    {
        glk_select(&ev);

        if(ev.type == evtype_LineInput)
            break;
        else if(ev.type == evtype_Arrange)
            Look();
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
        glk_window_get_size(Top, &Width, &TopHeight);
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
        glk_window_get_size(Top, &Width, &TopHeight);
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
            glk_window_get_size(Top, &Width, NULL);
        }
    }
}

void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, 0, x_offset, 0, 32 * 8 * pixel_size,
                         12 * 8 * pixel_size);
}

//7025
void DrawTaylorRoomImage(int flag);

uint16_t HL, pushedHL, BC, pushedBC, DE, pushedDE, IX, pushedIX, current_screen_address, temp, previousPush, attributes_start, global5bbc, stored_address, lastblockaddr;
uint8_t A, carry, pushedA, xpos, ypos, blockwidth, lastimgblock = 0xff;

uint8_t *mem;

void print_memory(int address, int length);

void LoadMemory(void) {
    mem = (uint8_t *)malloc(0xffff);
    memset(mem, 0, 0xffff);

    for (int i = 0; i<FileImageLen; i++) {
        mem[i+0x4000] = FileImage[i];
    }

    for (int i = 0x4000; i<0x5800; i++) {
        mem[i] = 0;
    }

    for (int i = 0x6e50; i<0x6fd0; i++) {
        mem[i] = 0;
    }
}

static void draw_spectrum_screen_from_mem(void);

void DisplayInit(void)
{
    LoadMemory();
    SagaSetup(0);
//    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
//    OpenTopWindow();
    OpenGraphicsWindow();
    glk_window_clear(Graphics);
    for (int i = 45; i < 100; i++) {
        mem[0x5d5c] = i;
        fprintf(stderr, "Room image: %d\n", i);
        DrawTaylorRoomImage(0);
        draw_spectrum_screen_from_mem();
        for (int i = 0x4000; i<0x5800; i++) {
            mem[i] = 0;
        }

        for (int i = 0x6e50; i<0x6e50 + 384; i++) {
            mem[i] = 0;
        }

        for (int i = 0x5800; i<0x5800 + 768; i++) {
            mem[i] = 0;
        }
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
    //    fprintf(stderr, "Graphics memory:\n");
    //    print_memory(0x4000, 768);
    //    fprintf(stderr, "Attributes memory:\n");
    //    print_memory(0x5800, 768);
}


void draw_image_block(void);

void call6B07(void);
void call6B27(void);
void call6A87(void);



static void draw_spectrum_screen_from_mem(void) {
//    OpenGraphicsWindow();

    //    for (int i = 0; i < 6144; i++)
    //    {
    //        if (mem[0x4000+i] != forest_mem[i])
    //            fprintf(stderr, "Error: wrong byte in graphics memory at %x! Expected %x, got %x\n", 0x4000+i,forest_mem[i], mem[0x4000+i]);
    //    }




    //    print_screen_memory();


    for (int i = 0; i < 768; i++)
    {
        glui32 paper = (mem[0x6e50+i] >> 3) & 0b111;
        paper += 8 * ((mem[0x6e50+i] & 64) == 64);
//        glui32 paper = (mem[0x5800+i] >> 3) & 0b111;
//        paper += 8 * ((mem[0x5800+i] & 64) == 64);
        RectFill((i % 32) * 8, (i / 32) * 8, 8, 8, paper);
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

        glui32 ink = (mem[0x6e50+(y/8) * 32 + (x/8)] & 0b111);
        ink += 8 * ((mem[0x6e50+(y/8) * 32 + (x/8)] & 64) == 64);

//        glui32 ink = (mem[0x5800+(y/8) * 32 + (x/8)] & 0b111);
//        ink += 8 * ((mem[0x5800+(y/8) * 32 + (x/8)] & 64) == 64);

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

void get_block_instruction(void) { // Get next draw instruction and put it in A
    A = mem[HL];
//    fprintf(stderr, "Value at address 0x%04x is 0x%02x\n", HL, A);
    if (A == 0xaa) {
//        fprintf(stderr, "Value at address 0x%04x was 0xaa, so look at ", HL);
        HL = stored_address + 1;
        A = mem[HL];
//        fprintf(stderr, "0x%04x instead (value 0x%02x)\n", HL, A);
    }
    pushedHL = HL;
    HL = 0x7836;
    BC = 0x12;
    do {
        HL++;
        BC--;
    } while (mem[HL] != A && BC != 0 && HL != 0xffff);

    if (mem[HL] == A) {
//        fprintf(stderr, "(0x11 - BC (%d)) == %d  ", BC, 0x11 - BC);
//        fprintf(stderr, "Found 0x%02x at address 0x%04x, so ", A, HL);
        stored_address = pushedHL;
        uint16_t addr = 0x7849 + (0x11 - BC) * 2;
        pushedHL = mem[addr] + mem[addr + 1] * 256;
//        fprintf(stderr, "store 0x%04x and jump to 0x%04x\n", stored_address, pushedHL);
    }
    HL = pushedHL;
    A = mem[HL];
//    fprintf(stderr, "Read block draw instruction 0x%02x at address 0x%04x\n", A, HL);
}

int call72b8(void) // Get next adress?
{
    current_screen_address++; // byte to the right
    DE = current_screen_address;
    mem[0x5bc6]--;
    if (mem[0x5bc6] != 0)
        return 0;
    mem[0x5bc7]--;
    if (mem[0x5bc7] == 0)
        return 1;
    mem[0x5bc6] = blockwidth;
    mem[0x5bc3]++;
    HL = 0x5bd0 + mem[0x5bc3] * 2;
    current_screen_address = xpos + mem[HL] + mem[HL+1] * 256;
    return 0;
}


//Routine at 717f

void draw_image_block(void)
{
    fprintf(stderr, "draw_image_block: A == 0x%02x BC = 0x%04x HL = 0x%04x\n", A, BC, HL); // Which number is it?
    uint16_t address = 0x5bd0 + A * 2;
    current_screen_address = mem[address] + mem[address+1] * 256 + BC;
    HL++; // Address to current draw instruction in "image data"
    uint8_t flip_mode = 0;
    uint8_t remaining = 0;
    do {
        mem[0x5bbe] = 0; // 0x5bbe is reset to 0
        get_block_instruction(); // Get next draw instruction (at address HL, into A)
        global5bbc = HL;
        if (A >= 128) {
            // Bit 7 set, this is a command byte
            mem[0x5bbe] = A; // draw instruction is stored in 0x5bbe and 0x5bbf
            mem[0x5bbf] = A;
            // Test bit 1, repeat flag
            if ((A & 2) == 2) {
                HL++;
                remaining = mem[HL]; // and store it in remaining
            }
            HL++;
            get_block_instruction();
            global5bbc = HL;
        }
        HL = A;
        BC = 0xc3cb; // Start of characters
        if ((mem[0x5bbf] & 1) == 1) { // Test bit 0, whether to add 127 to character index
            BC += 0x400;
        }

        IX = HL * 8 + BC;
        HL = 0x5bb2;
        BC = (BC & 0xff) + 0x08 * 256;
        A = (mem[0x5bbe] & 0x30);  // Current draw instruction bits 4 and 5
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
                    HL = 0x5bb2;
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
                    HL = 0x5bb2;
                    IX++;
                }
                break;
            default:
                break;
        }

        A = mem[0x5bbe] & 0x40; // bit 6
        if (A != 0) {  // flip horizontally
            HL = 0x5bb2;
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
        HL = 0x5bb2;
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
                         //        fprintf(stderr, "Set address %x to %x\n", DE, mem[DE]);
            mem[HL++] = A;
            DE = DE + 256; // Next line
        }
        flip_mode = mem[0x5bbe] & 0x0c;
        if (flip_mode == 0) { // No flip
            while (remaining != 0) {
                if (call72b8())  // Get screen address for next character?
                    return;
                HL = 0x5bb2;
                DE = current_screen_address; // Screen adress

                for (int B = 0; B < 8; B++) {
                    A = mem[HL++];
                    mem[DE] = A;
                    if (DE < 0x4000 || DE > 0x5B00)
                        fprintf(stderr, "Something went wrong\n");
                    DE = DE + 256;
                }

                remaining--;
            }
            if (call72b8())
                return;
        }
        global5bbc++;
        HL = global5bbc;
    } while (HL < 0xffff);
}

// 72ff
void draw_attributes(void) { // Draw attributes?
    HL = global5bbc + 1;
    mem[0x5bc6] = blockwidth;
    DE = attributes_start + ypos * 32 + xpos;
//    fprintf(stderr, "draw_attributes: draw instructions at 0x%04x. x:%d y:%d\n", HL, xpos, ypos);
    do {
        get_block_instruction();
        if (A == 0xfe) {
//            fprintf(stderr, "read 0xfe. Returning\n");
            A = pushedA;
            return;
        }
        if (A >= 128) { // Bit 7 is repeat flag
            mem[0x5bc0] = (A & 0x7f) - 1;
            A = mem[0x5bcb];
        }
    jump7336:
        mem[DE] = A;
//        fprintf(stderr, "Attribute address 0x%04x set to 0x%02x\n", DE, A);
        mem[0x5bcb] = A;
        DE++;
        mem[0x5bc6]--;
        if (mem[0x5bc6] == 0) {
            mem[0x5bc6] = blockwidth;
            DE += 0x20 - mem[0x5bc6];
        }

        if (mem[0x5bc0] != 0) {
            mem[0x5bc0]--;
            A = mem[0x5bcb];
            goto jump7336;
        }
        HL++;
    } while (HL < 0xffff);
}

static void replace_colour(uint8_t before, uint8_t after);
void call743c(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void call7368(void);
void call74eb(int skip);
void call6fd0(void);
void call7612(int skip);
void call7736(int skip);
static void replace(uint8_t before, uint8_t attr1, uint8_t attr2);
static uint8_t ink2paper(uint8_t ink);
static void replace_attr(void);
void call755e(void);
void call73f2(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
//void mirror_attributes(void);
void call74ae(void);
void call75a1(void);
void call7485(void);
void call75b6(void);
void call76f7(void);
void call75e9(void);
void call7608(void);
void call76c9(void);

// Routine at 7025
void DrawTaylorRoomImage(int flag) // Draw Taylor room image
{
    int instruction = 1;
    if (!flag) {
        A = mem[0x5d5c];
        if (A == 0) // return if initial room number is 0
            return;
    }
    fprintf(stderr, "Image number %d\n", A);
    IX = 0xbb75; // Start of image instruction data
    BC = (BC & 0xff) + A * 256; // Image number stored in B
    A = 0xff;
    int result;
jump7031: // Count 0xFFs until we are at the right place
    result = (mem[IX] == A);
    IX++;
    if (!result)
        goto jump7031;
    BC -= 256;
    if ((BC >> 8) != 0)
        goto jump7031;
jump703a:
    A = mem[IX];
    fprintf(stderr, "DrawTaylorRoomImage: Instruction %d: 0x%02x\n", instruction++, A);
    draw_spectrum_screen_from_mem();
    A++; //0xff, stop drawing
    if (A == 0) {
        fprintf(stderr, "End of picture\n");
        return;
    }
    A++; //0xfe
    if (A == 0) {
        fprintf(stderr, "0xfe (7470) mirror_left_half\n");
        goto jump7470;
    }
    A++; //0xfd
    if (A == 0) {
        fprintf(stderr, "0xfd (7126) Replace colour %x with %x\n", mem[IX+1], mem[IX+2]);
        goto jump7126; // Replace colour
    }
    A++; //0xfc
    if (A == 0) {
        fprintf(stderr, "0xfc (7808) Draw attribute %x at %d,%d height %d width %d\n", mem[IX+4], mem[IX+2], mem[IX+1], mem[IX+3], mem[IX+5]);
        goto jump7808;
    }
    A++; //0xfb Make picture area bright
    if (A == 0) {
        fprintf(stderr, "Make colours in picture area bright\n");
        goto jump713e;
    }
    A++; //0xfa
    if (A == 0) {
        fprintf(stderr, "0xfa (7646) Flip entire image horizontally\n");
        goto jump7646;
    }
    A++; //0xf9 Draw picture n recursively
    if (A != 0)
        goto jump7067;
    IX++;
    A = mem[IX];
    uint16_t myPushedIX = IX;
    fprintf(stderr, "Draw Room Image %d recursively\n", mem[IX]);

    DrawTaylorRoomImage(1); // Recursion
    IX = myPushedIX;
    IX++;
    goto jump703a;
jump7067:
    A = mem[IX];
    switch(A) {
        case 0xf8:
            fprintf(stderr, "0xf8: Skip rest of picture if object %d is not present\n", mem[IX+1]);
            goto jump73d1;
            break;
        case 0xf7:
            fprintf(stderr, "0xf7: goto 756e: set A to 12 and call 70b7\n");
            goto jump756e;
            break;
        case 0xf6:
            fprintf(stderr, "0xf6: goto 7582: set A to 4 and call 70b7\n");
            goto jump7582;
            break;
        case 0xf5:
            fprintf(stderr, "0xf5: goto 7578: set A to 8 and call 70b7\n");
            goto jump7578;
            break;
        case 0xf4:
            fprintf(stderr, "0xf4: goto 758c Stop drawing if object %d is present\n", mem[IX+1]);
            goto jump758c;
            break;
        case 0xf3:
            fprintf(stderr, "0xf3: goto 753d Mirror top half\n");
            goto jump753d;
            break;
        case 0xf2:
            fprintf(stderr, "0xf2: Mirror area x: %d y: %d x2:%d y2:%d horizontally?\n", mem[IX+2], mem[IX+1], mem[IX+4],  mem[IX+3]);
            goto jump7465;
            break;
        case 0xf1:
            fprintf(stderr, "0xf1: goto 7532 arg1: %d arg2: %d arg3:%d arg4:%d\n", mem[IX+1], mem[IX+2], mem[IX+3], mem[IX+4]);
//            mirror_area_vertically();
            goto jump7532;
            break;
        case 0xee:
            fprintf(stderr, "0xee: goto 763b arg1: %d arg2: %d arg3:%d arg4:%d\n", mem[IX+1], mem[IX+2], mem[IX+3], mem[IX+4]);
            goto jump763b;
            break;
        case 0xed:
            fprintf(stderr, "0xed: goto 7788 Flip entire image vertically\n");
            goto jump7788;
            break;
        case 0xec:
            fprintf(stderr, "0xec: goto 777d Flip area vertically?\n");
            goto jump777d;
            break;
        case 0xeb:
        case 0xea:
            fprintf(stderr, "%x: Crash\n", mem[IX]);
            glk_exit();
        case 0xe9:
            fprintf(stderr, "0xe9: (77ac) swap attribute %d with attribute %d\n", mem[IX+1], mem[IX+2]);
            goto jump77ac;
            break;
        case 0xe8:
            fprintf(stderr, "Clear graphics memory\n");
            goto jump75ae;
            break;
        default:
            BC = BC & 0xff + A * 256;
            A = 0;
jump70b7:  //Default: Draw picture arg at x, y
            fprintf(stderr, "Default: Draw picture %d at %d,%d\n", mem[IX], mem[IX+1], mem[IX+2]);
            mem[0x70e8] = A;
            A = BC >> 8;
            call7368();
            IX++;
            BC = mem[IX];
            xpos = BC;
            IX++;
            A = mem[IX];
            ypos = A; //ypos = A
            mem[0x5bc3] = A;
            IX++;
            myPushedIX = IX;
            draw_image_block();
            attributes_start = 0x6e50;
            draw_attributes();
            IX = myPushedIX;
            break;
    }

    goto jump703a;
jump7126: // Replace colour
    replace_colour(mem[IX+1], mem[IX+2]);
    IX += 3;
    goto jump703a;
jump713e: // Set all attributes to bright
    for (int i = 0x6e50; i < 0x6e50 + 0x0180; i++) {
        mem[i] = mem[i] | 0x40;;
    }
    IX++;
    goto jump703a;
jump73d1:
    IX++;
    BC = mem[IX];
    HL = 0x5d9f; // Start of object locations
    HL = HL + BC; // Location of object BC
    A = mem[0x5d5c]; // Current location
    if (mem[HL] != A)
        goto jump73e7;
    IX++;
    goto jump703a;
jump73e7:
    IX++;
    A = mem[IX];
    A++;
    if (A != 0)
        goto jump73e7;
    goto jump703a;
jump7465:
    call743c(mem[IX + 2], mem[IX + 1], mem[IX + 4], mem[IX + 3]);
    IX = IX + 5;
    goto jump703a;
jump7470:
//    mem[0x741c] = 0x0c;
//    mem[0x7437] = 0x0c;
//    A = 0x20;
//    BC = 0;
    call743c(0, 0, 0x20, 0x0c);
    IX++;
    goto jump703a;
jump7532:
    call74eb(0);
    IX = IX + 5;
    goto jump703a;
jump753d:
    A = 0;
    mem[0x74e7] = A;
    mem[0x74e9] = A;
    mem[0x74e8] = A;
    A = 0x0b;
    mem[0x74ea] = A;
    A = 0x20;
    mem[0x74c9] = A;
    mem[0x7496] = A;
    A = 0x05;
    call74eb(1);
    IX++;
    goto jump703a;
jump756e:
    BC = (BC & 0xff) + mem[IX + 1] * 256;
    A = 0x0c;
    IX++;
    goto jump70b7;
jump7578:
    BC = (BC & 0xff) + mem[IX+1] * 256;
    A = 4;
    IX++;
    goto jump70b7;
jump7582:
    BC = (BC & 0xff) + mem[IX+1] * 256;
    A = 8;
    IX++;
    goto jump70b7;
jump758c:
    DE = mem[IX + 1];
    HL = 0x5d9f; // Start of object locations
    HL = HL + DE;
    A = mem[0x5d5c]; // Current location
    if (mem[HL] == A)
        return;
    IX += 2;
    goto jump703a;
jump75ae:
    call6fd0();
    IX++;
    goto jump703a;
jump763b:
    call7612(0);
    IX = IX + 5;
    goto jump703a;
jump7646:
    A = 0x0c;
    mem[0x75e4] = A;
    mem[0x7603] = A;
    A = 0x20;
    BC = 0;
    call7612(1);
    IX++;
    goto jump703a;
jump777d:
    call7736(0);
    IX = IX + 5;
    goto jump703a;
jump7788:
    A = 0;
    mem[0x74e7] = A;
    mem[0x74e8] = A;
    mem[0x74e9] = A;
    A = 0x0b;
    mem[0x74ea] = A;
    A = 0x20;
    mem[0x74da] = A;
    mem[0x7712] = A;
    A = 0x05;
    call7736(1);
    IX++;
    goto jump703a;
jump77ac:
    replace_attr();
    goto jump703a;
jump7808:
    IX++;
    BC = (BC & 0xff) + mem[IX] * 256;
    IX++;
    BC = (BC & 0xff00) + mem[IX];
    IX++;
    call755e();
    BC = (BC & 0xff) + mem[IX] * 256;
    IX++;
    A = mem[IX];
    IX++;
    uint16_t myPushedBC;
jump7821:
    myPushedBC = BC;
    uint16_t myPushedHL = HL;
    BC = (BC & 0xff) + mem[IX] * 256;
jump7826:
    mem[HL] = A;
    HL++;
    BC = BC - 256;
    if ((BC >> 8) != 0)
        goto jump7826;
    HL = myPushedHL;
    BC = myPushedBC;
    DE = 0x0020;
    HL = HL + DE;
    BC = BC - 256;
    if ((BC >> 8) != 0)
        goto jump7821;
    IX++;
    goto jump703a;
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
        HL = y1 * 32 + 0x6e50; // Attributes buffer
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

void call743c(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    call73f2(x1, y1, x2, y2);
    mirror_attributes(x1, y1, x2, y2);
}


static void replace_colour(uint8_t before, uint8_t after) {
    // I don't think any of the data has bit 7 set,
    // so masking it is probably unnecessary, but
    // this is what the original code does.
    uint8_t beforeink = before & 7;
    uint8_t afterink = after & 7;
    uint8_t inkmask = 0x07;

    uint8_t beforepaper = beforeink << 3;
    uint8_t afterpaper = afterink << 3;
    uint8_t papermask = 0x38;

    for (int j = 0x6e50; j < 0x6e50 + 384; j++) {
        if ((mem[j] & inkmask) == beforeink) {
            mem[j]  = (mem[j]  & ~inkmask) | afterink;
        }

        if ((mem[j]  & papermask) == beforepaper) {
            mem[j]  = (mem[j] & ~papermask) | afterpaper;
        }
    }
}

void call7368(void) {
    if (lastimgblock != A) { // If last image block wasn't A (current image block number)
        HL = 0xca33;
        lastimgblock = A;
        if (A != 0) {
            for (int i = 0; i < A; i++) { // Skip A 0xfe until HL is the address of current image block
                do {
                    HL++;
                } while (mem[HL] != 0xfe);
            }
            HL++;
        }
    } else {
        HL = lastblockaddr;
    }
    lastblockaddr = HL;
    blockwidth = ((mem[HL] & 0xf0) >> 4) + 1; // (0x5bc4) = width
    mem[0x5bc6] = blockwidth;
    mem[0x5bc7] =  (mem[HL] & 0x0f) + 1; // (0x5bc5) = height
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
    mem[0x74e3] = A;
    mem[0x74aa] = A;
    BC = mem[0x74e7] + mem[0x74e8] * 256;
    DE = mem[0x74e9] + mem[0x74ea] * 256;
    uint16_t myPushedBC = BC;
    uint16_t myPushedDE = DE;
    call74ae();
    DE = myPushedDE;
    BC = myPushedBC;
    mem[0x74e7] = BC & 0xff;
    mem[0x74e8] = BC >> 8;
    mem[0x74e9] = DE & 0xff;
    mem[0x74ea] = DE >> 8;
    call7485();
}

void call22aa(void) { // THE 'PIXEL ADDRESS' SUBROUTINE
    if ((BC >> 8) > 175)
        return;
    BC = BC & 0xff + (175 - (BC >> 8)) * 256;
    carry = rotate_right_with_carry(&A, 0);
    carry = 1;
    carry = rotate_right_with_carry(&A, carry);
    carry = 0;
    carry = rotate_right_with_carry(&A, carry);
    A = A ^ (BC >> 8);
    A = A & 0xf8; // 11111000
    A = A ^ (BC >> 8);
    HL = HL & 0xff + A * 256;
    A = BC & 0xff;
    carry = rotate_left_with_carry(&A, carry);
    carry = rotate_left_with_carry(&A, carry);
    carry = rotate_left_with_carry(&A, carry);
    A = A ^ (BC >> 8);
    A = A & 0xc7; // 11000111
    A = A ^ (BC >> 8);
    carry = rotate_left_with_carry(&A, carry);
    carry = rotate_left_with_carry(&A, carry);
    HL = HL & 0xff00 + A;
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
    DE = 0x6e50; // Attributes buffer
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

void call7612(int skip) {
    if (!skip) {
        A = mem[IX + 3];
        mem[0x75e4] = A;
        mem[0x7603] = A;
        A = mem[IX + 4];
        BC = mem[IX + 2] + mem[IX + 1] * 256;
    }
    mem[0x75bc] = A;
    mem[0x75ee] = A;
    carry = rotate_right_with_carry(&A, 0);
    mem[0x75c7] = A;
    mem[0x75f5] = A;
    uint16_t oldPushedBC = BC;
    call75b6();
    BC = oldPushedBC;
    call75e9();
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

static void replace_attr(void) {
    uint8_t before = mem[IX + 1];
    uint8_t after = mem[IX + 2];
    uint8_t beforeink = before & 0x47; // 0100 0111 ink and brightness
    replace(beforeink, after, 0x47);
    uint8_t beforepaper = ink2paper(before);
    uint8_t afterpaper = ink2paper(after);
    replace(beforepaper, afterpaper, 0x78); // 0111 1000 mask paper and brightness
    IX += 3;
}

static uint8_t ink2paper(uint8_t ink) {
    uint8_t paper = (ink & 0x07) << 3; // 0000 0111 mask ink
    paper = paper & 0x38; // 0011 1000 mask paper
    return (ink & 0x40) | paper; // 0x40 = 0100 0000 preserve brightness bit from ink
}

static void replace(uint8_t before, uint8_t after, uint8_t mask) {
    for (int i = 0x6e50; i < 0x6e50 + 0x180; i++) {
        uint8_t col = mem[i] & mask;
        if (col == before) {
            col = mem[i];
            col = col | mask;
            col = col ^ mask;
            col = col | after;
            mem[i] = col;
        }
    }
}



void call755e(void) {
    DE = 0x6e50; // Attributes buffer
    HL = (BC >> 8) * 32 + DE;
    BC = BC & 0xff;
    HL = HL + BC;
}




//void mirror_attributes(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
//    do {
//        HL = (BC >> 8) * 32 + 0x6e50; // Attributes buffer
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

void call75a1(void) {
    A = BC >> 8;
    call60c1();
    HL = DE;
    BC = BC & 0xff;
    HL = HL + BC;
}

void call7485(void) {
jump7485:
    BC = mem[0x74e7] + mem[0x74e8] * 256;
    call755e();
    uint16_t var = HL;
    BC = mem[0x74e9] + mem[0x74ea] * 256;
    call755e();
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

void call75b6(void) {
    uint16_t prevPushedBC;
jump75b6:
    prevPushedBC = BC;
    call75a1();
    uint16_t var = HL;
    DE = mem[0x75bc] + mem[0x75bd] * 256;
    BC = (BC & 0xff) + 0x08 * 256;
    HL += DE;
    HL--;
    DE = var;
jump75c3:
    pushedHL = HL;
    pushedDE = DE;
    pushedBC = BC;
    BC = (BC & 0xff) + mem[0x75c7] * 256;
jump75c8:
    call7608();
    temp = HL;
    HL = DE;
    DE = temp;
    call7608();
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
    A = mem[0x75e4];
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

void call75e9(void) {
jump75e9:
    pushedBC = BC;
    call755e();
    DE = mem[0x75ee] + mem[0x75ef] * 256;
    pushedHL = HL;
    HL += DE;
    HL--;
    DE = pushedHL;
    BC = (BC & 0xff) + mem[0x75f5] * 256;
jump75f6:
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
        goto jump75f6;
    BC = pushedBC;
    BC += 256;
    A = mem[0x7603];
    if (A != (BC >> 8))
        goto jump75e9;
}

void call7608(void) {
    BC = (BC & 0xff00) + 0x80;
jump760a:
    carry = rotate_left_with_carry(&mem[HL], carry);
    uint8_t C = BC & 0xff;
    carry = rotate_right_with_carry(&C, carry);
    BC = (BC & 0xff00) + C;
    if (!carry)
        goto jump760a;
    mem[HL] = C;
}

void call76c9(void) {
    do {
        DE = 0x6e50 + mem[0x74e8] * 32 + mem[0x74e7];
        HL = 0x6e50 + mem[0x74ea] * 32 + mem[0x74e9];
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
 ; Routine at 72ff
 ;
 ; Used by the routine at #R$7025.
 c$72ff ld hl,($5bbc) ; Draw attributes
 $7302 inc hl        ;
 $7303 push hl       ;
 $7304 ld a,($5bc4)  ;
 $7307 ld ($5bc6),a  ; Image height = a
 $730a ld hl,($5bc2) ;
 $730d ld h,$00      ;
 $730f call $0e93    ;
 $7312 ld a,($5bc1)  ; A = xpos
 $7315 ld e,a        ;
 $7316 ld d,$00      ;
 $7318 add hl,de     ;
 $7319 ld de,($5bc9) ; start address
 $731d add hl,de     ;
 $731e ex de,hl      ; de = start address + xpos + ypos * 32
 $731f pop hl        ;
 *$7320 ld a,(hl)     ;
 $7321 call $7151    ;
 $7324 push hl       ;
 $7325 cp $fe        ;
 $7327 jr z,$72e7    ;
 $7329 bit 7,a       ;
 $732b jr z,$7336    ;
 $732d and $7f       ; // Bit 7 is repeat flag
 $732f dec a         ;
 $7330 ld ($5bc0),a  ;
 $7333 ld a,($5bcb)  ;
 *$7336 ld (de),a     ;
 $7337 ld ($5bcb),a  ;
 $733a inc de        ;
 $733b ld hl,$5bc6   ; Image height
 $733e dec (hl)      ;
 $733f jr nz,$7352   ;
 $7341 push hl       ;
 $7342 ld a,($5bc4)  ;
 $7345 ld ($5bc6),a  ; Image width
 $7348 ld l,a        ;
 $7349 ld a,$20      ;
 $734b sub l         ;
 $734c ld l,a        ;
 $734d ld h,$00      ;
 $734f add hl,de     ;
 $7350 ex de,hl      ;
 $7351 pop hl        ;
 *$7352 ld a,($5bc0)  ;
 $7355 or a          ;
 $7356 jr z,$7361    ;
 $7358 dec a         ;
 $7359 ld ($5bc0),a  ;
 $735c ld a,($5bcb)  ;
 $735f jr $7336      ;
 *$7361 pop hl        ;
 $7362 inc hl        ;
 $7363 jr $7320      ;


 ; Routine at 7151
 ;
 ; Used by the routines at #R$717f and #R$72ff.
 c$7151 push bc       ;
 $7152 cp $aa        ;
 $7154 jr nz,$715b   ;
 $7156 ld hl,($717d) ;
 $7159 inc hl        ;
 $715a ld a,(hl)     ;
 *$715b push hl       ;
 $715c ld hl,$7837   ;
 $715f ld bc,$0012   ;
 $7162 cpir          ;
 $7164 dec hl        ;
 $7165 cp (hl)       ;
 $7166 jr nz,$7179   ;
 $7168 ld hl,$7849   ;
 $716b ld a,$11      ;
 $716d sub c         ;
 $716e ld c,a        ;
 $716f add hl,bc     ;
 $7170 add hl,bc     ;
 $7171 ld c,(hl)     ;
 $7172 inc hl        ;
 $7173 ld h,(hl)     ;
 $7174 ld l,c        ;
 $7175 ex (sp),hl    ;
 $7176 ld ($717d),hl ;
 *$7179 pop hl        ;
 $717a pop bc        ;
 $717b ld a,(hl)     ;
 $717c ret           ;

 ; Used by the routines at #R$664b, #R$665e, #R$66ed, #R$717f, #R$72b8 and
 ; #R$75a1.
 c$60c1 ld l,a        ;
 $60c2 ld h,$00      ;
 $60c4 add hl,hl     ; HL = A  * 2
 $60c5 ld de,$5bd0   ; DE = 5bd0
 $60c8 add hl,de     ; HL = A * 2 + 5bd0
 $60c9 jp $2af0      ;

 2AF0    LD E,(HL)    In effect - LD E,(DE+1).
 2AF1    INC HL    Point to 'DE+2'.
 2AF2    LD D,(HL)    In effect - LD D,(DE+2).
 2AF3    RET    Finished.



 Routine at 717f
 ;
 ; Used by the routine at #R$7025. // transform character at A
 c$717f push hl       ;
 $7180 call $60c1    ;
 $7183 ex de,hl      ;
 $7184 add hl,bc     ;
 $7185 ld ($5bba),hl ;
 $7188 pop hl        ;
 $7189 inc hl        ;
 $718a ld ($5bbc),hl ;
 $718d xor a         ;
 $718e ld ($5bc8),a  ;
 $7191 ld ($5bc0),a  ;
 *$7194 xor a         ;
 $7195 ld ($5bbe),a  ;
 $7198 ld a,(hl)     ;
 $7199 call $7151    ;
 $719c ld ($5bbc),hl ;
 $719f bit 7,a       ;
 $71a1 jr z,$71ba    ;
 $71a3 ld ($5bbe),a  ;
 $71a6 ld ($5bbf),a  ;
 $71a9 and $02       ;
 $71ab jr z,$71b2    ;
 $71ad inc hl        ;
 $71ae ld a,(hl)     ;
 $71af ld ($5bc0),a  ;
 *$71b2 inc hl        ;
 $71b3 ld a,(hl)     ;
 $71b4 call $7151    ;
 $71b7 ld ($5bbc),hl ;
 *$71ba ld l,a        ;
 $71bb ld h,$00      ;
 $71bd ld bc,$c3cb   ;
 $71c0 ld a,($5bbf)  ;
 $71c3 bit 0,a       ;
 $71c5 jr z,$71cb    ;
 $71c7 inc b         ;
 $71c8 inc b         ;
 $71c9 inc b         ;
 $71ca inc b         ;
 *$71cb add hl,hl     ;
 $71cc add hl,hl     ;
 $71cd add hl,hl     ;
 $71ce add hl,bc     ;
 $71cf push hl       ;
 $71d0 pop ix        ;
 $71d2 ld hl,$5bb2   ;
 $71d5 ld b,$08      ;
 $71d7 ld a,($5bbe)  ;
 $71da and $30       ;
 $71dc jr nz,$71e9   ;
 *$71de ld a,(ix+$00) ;
 $71e1 ld (hl),a     ;
 $71e2 inc ix        ;
 $71e4 inc hl        ;
 $71e5 djnz $71de    ;
 $71e7 jr $723b      ;
 *$71e9 cp $10        ;
 $71eb jr nz,$7205   ;
 $71ed ld c,$08      ;
 *$71ef ld b,$08      ;
 $71f1 ld a,(ix+$00) ;
 *$71f4 sla a         ;
 $71f6 rr (hl)       ;
 $71f8 inc hl        ;
 $71f9 djnz $71f4    ;
 $71fb ld hl,$5bb2   ;
 $71fe inc ix        ;
 $7200 dec c         ;
 $7201 jr nz,$71ef   ;
 $7203 jr $723b      ;
 *$7205 cp $20        ;
 $7207 jr nz,$7221   ;
 $7209 ld c,$08      ;
 $720b ld hl,$5bb9   ;
 *$720e ld b,$08      ;
 $7210 ld a,(ix+$00) ;
 *$7213 sla a         ;
 $7215 rr (hl)       ;
 $7217 djnz $7213    ;
 $7219 inc ix        ;
 $721b dec hl        ;
 $721c dec c         ;
 $721d jr nz,$720e   ;
 $721f jr $723b      ;
 *$7221 cp $30        ;
 $7223 jr nz,$723b   ;
 $7225 ld c,$08      ;
 *$7227 ld b,$08      ;
 $7229 ld a,(ix+$00) ;
 *$722c srl a         ;
 $722e rl (hl)       ;
 $7230 inc hl        ;
 $7231 djnz $722c    ;
 $7233 ld hl,$5bb2   ;
 $7236 inc ix        ;
 $7238 dec c         ;
 $7239 jr nz,$7227   ;
 *$723b ld a,($5bbe)  ;
 $723e and $40       ;
 $7240 or a          ;
 $7241 jr z,$7255    ;
 $7243 ld c,$08      ;
 $7245 ld hl,$5bb2   ;
 *$7248 ld b,$08      ;
 $724a ld a,(hl)     ;
 *$724b sla a         ;
 $724d rr (hl)       ;
 $724f djnz $724b    ;
 $7251 inc hl        ;
 $7252 dec c         ;
 $7253 jr nz,$7248   ;
 *$7255 ld de,($5bba) ;
 $7259 ld hl,$5bb2   ;
 $725c ld b,$08      ;
 *$725e ld a,($5bc8)  ;
 $7261 cp $04        ;
 $7263 jr nz,$7269   ;
 $7265 ld a,(de)     ;
 $7266 or (hl)       ;
 $7267 jr $727a      ;
 *$7269 cp $08        ;
 $726b jr nz,$7271   ;
 $726d ld a,(de)     ;
 $726e and (hl)      ;
 $726f jr $727a      ;
 *$7271 cp $0c        ;
 $7273 jr nz,$7279   ;
 $7275 ld a,(de)     ;
 $7276 xor (hl)      ;
 $7277 jr $727a      ;
 *$7279 ld a,(hl)     ;
 *$727a ld (de),a     ;
 $727b ld (hl),a     ;
 $727c inc hl        ;
 $727d inc d         ;
 $727e djnz $725e    ;
 $7280 ld a,($5bbe)  ;
 $7283 and $0c       ;
 $7285 ld ($5bc8),a  ;
 $7288 jr nz,$72ae   ;
 *$728a ld a,($5bc0)  ;
 $728d or a          ;
 $728e jr z,$72ab    ;
 $7290 call $72b8    ;
 $7293 ld b,$08      ;
 $7295 ld hl,$5bb2   ;
 $7298 ld de,($5bba) ;
 *$729c ld a,(hl)     ;
 $729d ld (de),a     ;
 $729e inc hl        ;
 $729f inc d         ;
 $72a0 djnz $729c    ;
 $72a2 ld a,($5bc0)  ;
 $72a5 dec a         ;
 $72a6 ld ($5bc0),a  ;
 $72a9 jr nz,$728a   ;
 *$72ab call $72b8    ;
 *$72ae ld hl,($5bbc) ;
 $72b1 inc hl        ;
 $72b2 ld ($5bbc),hl ;
 $72b5 jp $7194      ;

 ; Routine at 72b8
 ;
 ; Used by the routine at #R$717f.
 c$72b8 ld de,($5bba) ;
 $72bc inc de        ;
 $72bd ld ($5bba),de ;
 $72c1 ld hl,$5bc6   ; Image height
 $72c4 dec (hl)      ;
 $72c5 ret nz        ;
 $72c6 ld hl,$5bc7   ;
 $72c9 dec (hl)      ;
 $72ca jr z,$72e7    ;
 $72cc ld a,($5bc4)  ;
 $72cf ld ($5bc6),a  ; Image height = a
 $72d2 ld a,($5bc3)  ;
 $72d5 inc a         ;
 $72d6 ld ($5bc3),a  ;
 $72d9 call $60c1    ;
 $72dc ld a,($5bc1)  ;
 $72df ld l,a        ;
 $72e0 ld h,$00      ;
 $72e2 add hl,de     ;
 $72e3 ld ($5bba),hl ;
 $72e6 ret           ;
 ; This entry point is used by the routine at #R$72ff.
 *$72e7 pop af        ;
 $72e8 ret           ;


 ; Routine at 7025
 ;
 ; Used by the routine at #R$7001.
 c$7025 ld a,($5d5c)  ;
 $7028 or a          ;
 $7029 ret z         ;
 *$702a ld ix,$bb75   ;
 $702e ld b,a        ;
 $702f ld a,$ff      ;
 *$7031 cp (ix+$00)   ;
 $7034 inc ix        ;
 $7036 jr nz,$7031   ;
 $7038 djnz $7031    ;
 ; This entry point is used by the routines at #R$7126, #R$713e, #R$73d1,
 ; #R$7465, #R$7470, #R$758c, #R$763b, #R$7646, #R$77ac and #R$7808.
 *$703a ld a,(ix+$00) ;
 $703d inc a         ;
 $703e ret z         ; Return on 0xff
 $703f inc a         ; 0xfe
 $7040 jp z,$7470    ;
 $7043 inc a         ;
 $7044 jp z,$7126    ; Replace colours 0xfd
 $7047 inc a         ;
 $7048 jp z,$7808    ; 0xfc arg1 arg2 arg3 arg4
 ;    Seems to be
 ; setattributes (arg1,arg2, height, attribute, width)
 ; where arg1=y arg2=x in chars 0,0 top left
 $704b inc a         ;
 $704c jp z,$713e    ; 0xfb Make picture area bright
 $704f inc a         ;
 $7050 jp z,$7646    ; 0xfa
 $7053 inc a         ;
 $7054 jp nz,$7067   ; 0xf9
 $7057 inc ix        ; Draw picture n
 $7059 ld a,(ix+$00) ;
 $705c push ix       ;
 $705e call $702a    ;
 $7061 pop ix        ;
 $7063 inc ix        ;
 $7065 jr $703a      ;
 *$7067 ld a,(ix+$00) ;
 $706a cp $f8        ; 73d1
 $706c jp z,$73d1    ;
 $706f cp $f7        ; 756e arg1 }
 $7071 jp z,$756e    ;
 $7074 cp $f6        ; 7582 arg1 } set A to 0 4 or 8 and call same method
 $7076 jp z,$7582    ;
 $7079 cp $f5        ; 7578 arg1 }
 $707b jp z,$7578    ;
 $707e cp $f4        ; 758c arg1 End of object arg1 is not present
 $7080 jp z,$758c    ;
 $7083 cp $f3        ; 753d
 $7085 jp z,$753d    ;
 $7088 cp $f2        ; 7465 arg1 arg2 arg3 arg4
 $708a jp z,$7465    ;
 $708d cp $f1        ; 7532
 $708f jp z,$7532    ;
 $7092 cp $ee        ; 763b
 $7094 jp z,$763b    ;
 $7097 cp $ed        ; 7788
 $7099 jp z,$7788    ;
 $709c cp $ec        ; 777d
 $709e jp z,$777d    ;
 $70a1 cp $eb        ; CRASH
 $70a3 jp z,$0000    ;
 $70a6 cp $ea        ; CRASH
 $70a8 jp z,$0000    ;
 $70ab cp $e9        ; 77ac arg1 arg2 ..
 ; arg1 = colour to match
 ; arg2 = new colour
 ; Swap colour (paper or ink)
 $70ad jp z,$77ac    ; Replace colour
 $70b0 cp $e8        ; 75ae Clear upper
 $70b2 jp z,$75ae    ;
 $70b5 ld b,a        ;
 $70b6 xor a         ;
 ; This entry point is used by the routines at #R$756e, #R$7578 and #R$7582.
 *$70b7 ld ($70e8),a  ;
 $70ba ld a,b        ;
 $70bb call $7368    ;
 $70be inc ix        ;
 $70c0 ld a,(ix+$00) ;
 $70c3 ld ($5bc1),a  ;
 $70c6 ld c,a        ;
 $70c7 inc ix        ;
 $70c9 ld a,(ix+$00) ;
 $70cc ld ($5bc2),a  ;
 $70cf ld ($5bc3),a  ;
 $70d2 inc ix        ;
 $70d4 push ix       ;
 $70d6 call $717f    ; Draw image / character
 $70d9 ld de,$6e50   ;
 $70dc ld ($5bc9),de ;
 $70e0 call $72ff    ; Draw attributes
 $70e3 pop ix        ;
 $70e5 jp $703a      ;


 ; Routine at 7126
 ;
 ; Used by the routine at #R$7025. Replace colours
 c$7126 inc ix        ; Replace colours
 $7128 ld a,(ix+$00) ;
 $712b and $07       ;
 $712d ld c,a        ;
 $712e inc ix        ;
 $7130 ld a,(ix+$00) ;
 $7133 and $07       ;
 $7135 ld l,a        ;
 $7136 call $70e9    ;
 $7139 inc ix        ;
 $713b jp $703a      ;

 c$713e ld hl,$6e50   ;
 $7141 ld bc,$0180   ;
 *$7144 set 6,(hl)    ;
 $7146 inc hl        ;
 $7147 dec bc        ;
 $7148 ld a,c        ;
 $7149 or b          ;
 $714a jr nz,$7144   ;
 $714c inc ix        ;
 $714e jp $703a      ;

 ; Routine at 73d1
 ;
 ; Used by the routine at #R$7025.
 c$73d1 inc ix        ;
 $73d3 ld c,(ix+$00) ;
 $73d6 ld b,$00      ;
 $73d8 ld hl,$5d9f   ; // Start of object locations
 $73db add hl,bc     ;
 $73dc ld a,($5d5c)  ; A = Current location
 $73df cp (hl)       ;
 $73e0 jr nz,$73e7   ;
 $73e2 inc ix        ;
 $73e4 jp $703a      ;
 *$73e7 inc ix        ;
 $73e9 ld a,(ix+$00) ;
 $73ec inc a         ;
 $73ed jr nz,$73e7   ;
 $73ef jp $703a      ;

 ; Routine at 743c
 ;
 ; Used by the routine at #R$7465.
 c$743c ld a,(ix+$03) ;
 $743f ld ($741c),a  ;
 $7442 ld ($7437),a  ;
 $7445 ld a,(ix+$04) ;
 $7448 ld b,(ix+$01) ;
 $744b ld c,(ix+$02) ;
 ; This entry point is used by the routine at #R$7470.
 *$744e ld ($73f8),a  ;
 $7451 ld ($7426),a  ;
 $7454 and a         ;
 $7455 rra           ;
 $7456 ld ($7403),a  ;
 $7459 ld ($742d),a  ;
 $745c push bc       ;
 $745d call $73f2    ;
 $7460 pop bc        ;
 $7461 call $7421    ;
 $7464 ret           ;

 ; Routine at 7465
 ;
 ; Used by the routine at #R$7025.
 c$7465 call $743c    ;
 $7468 ld de,$0005   ;
 $746b add ix,de     ;
 $746d jp $703a      ;

 ; Routine at 7470
 ;
 ; Used by the routine at #R$7025.
 c$7470 ld a,$0c      ;
 $7472 ld ($741c),a  ;
 $7475 ld ($7437),a  ;
 $7478 ld a,$20      ;
 $747a ld bc,$0000   ;
 $747d call $744e    ;
 $7480 inc ix        ;
 $7482 jp $703a      ;

 ; Used by the routine at #R$7126.
 c$70e9 ld b,$07      ;
 $70eb ld h,$f8      ;
 $70ed call $7104    ;
 $70f0 ld a,c        ;
 $70f1 sla a         ;
 $70f3 sla a         ;
 $70f5 sla a         ;
 $70f7 ld c,a        ;
 $70f8 ld a,l        ;
 $70f9 sla a         ;
 $70fb sla a         ;
 $70fd sla a         ;
 $70ff ld l,a        ;
 $7100 ld b,$38      ;
 $7102 ld h,$c7      ;
 *$7104 push ix       ;
 $7106 ld ix,$6e50   ;
 $710a ld de,$0180   ;
 *$710d ld a,(ix+$00) ;
 $7110 and b         ;
 $7111 cp c          ;
 $7112 jr nz,$711c   ;
 $7114 ld a,(ix+$00) ;
 $7117 and h         ;
 $7118 or l          ;
 $7119 ld (ix+$00),a ;
 *$711c inc ix        ;
 $711e dec de        ;
 $711f ld a,d        ;
 $7120 or e          ;
 $7121 jr nz,$710d   ;
 $7123 pop ix        ;
 $7125 ret           ;

 ; Used by the routine at #R$7025.
 c$7532 call $74eb    ;
 $7535 ld de,$0005   ;
 $7538 add ix,de     ;
 $753a jp $703a      ;

 ; Routine at 753d
 ;
 ; Used by the routine at #R$7025.
 c$753d xor a ;
 $753e ld ($74e7),a  ;
 $7541 ld ($74e9),a  ;
 $7544 ld ($74e8),a  ;
 $7547 ld a,$0b      ;
 $7549 ld ($74ea),a  ;
 $754c ld a,$20      ;
 $754e ld ($74c9),a  ;
 $7551 ld ($7496),a  ;
 $7554 ld a,$05      ;
 $7556 call $7511    ;
 $7559 inc ix        ;
 $755b jp $703a      ;

 ; Routine at 756e
 ;
 ; Used by the routine at #R$7025.
 c$756e ld b,(ix+$01) ;
 $7571 ld a,$0c      ;
 $7573 inc ix        ;
 $7575 jp $70b7      ;


 ; Routine at 7578
 ;
 ; Used by the routine at #R$7025.
 c$7578 ld b,(ix+$01) ;
 $757b ld a,$04      ;
 $757d inc ix        ;
 $757f jp $70b7      ;

 ; Routine at 7582
 ;
 ; Used by the routine at #R$7025.
 c$7582 ld b,(ix+$01) ;
 $7585 ld a,$08      ;
 $7587 inc ix        ;
 $7589 jp $70b7      ;

 ; Routine at 758c
 ;
 ; Used by the routine at #R$7025.
 c$758c ld e,(ix+$01) ; // e = arg1
 $758f ld hl,$5d9f   ; // Start of object locations
 $7592 ld d,$00      ;
 $7594 add hl,de     ;
 $7595 ld a,($5d5c)  ; A = Current location
 $7598 cp (hl)       ;
 $7599 ret z         ; Return if object arg1 is in location
 $759a inc ix        ;
 $759c inc ix        ;
 $759e jp $703a      ; Else continue reading

 ; Routine at 75ae
 ;
 ; Used by the routine at #R$7025.
 c$75ae call $6fd0    ;
 $75b1 inc ix        ;
 $75b3 jp $703a      ;

 ; Routine at 763b
 ;
 ; Used by the routine at #R$7025.
 c$763b call $7612    ;
 $763e ld de,$0005   ;
 $7641 add ix,de     ;
 $7643 jp $703a      ;

 ; Routine at 7646
 ;
 ; Used by the routine at #R$7025.
 c$7646 ld a,$0c      ;
 $7648 ld ($75e4),a  ;
 $764b ld ($7603),a  ;
 $764e ld a,$20      ;
 $7650 ld bc,$0000   ;
 $7653 call $7624    ;
 $7656 inc ix        ;
 $7658 jp $703a      ;

 ; Routine at 777d
 ;
 ; Used by the routine at #R$7025.
 c$777d call $7736    ;
 $7780 ld de,$0005   ;
 $7783 add ix,de     ;
 $7785 jp $703a      ;

 ; Routine at 7788
 ;
 ; Used by the routine at #R$7025.
 c$7788 xor a         ;
 $7789 ld ($74e7),a  ;
 $778c ld ($74e8),a  ;
 $778f ld ($74e9),a  ;
 $7792 ld a,$0b      ;
 $7794 ld ($74ea),a  ;
 $7797 ld a,$20      ;
 $7799 ld ($76da),a  ;
 $779c ld ($7712),a  ;
 $779f ld a,$05      ;
 $77a1 call $775c    ;
 $77a4 inc ix        ;
 $77a6 jp $703a      ;

 ; Routine at 77ac
 ;
 ; Used by the routine at #R$7025.
 c$77ac inc ix        ;
 $77ae ld a,(ix+$00) ;
 $77b1 and $47       ;
 $77b3 inc ix        ;
 $77b5 ld d,(ix+$00) ;
 $77b8 ld e,$47      ;
 $77ba call $77e4    ;
 $77bd ld c,(ix-$01) ;
 $77c0 call $77d6    ;
 $77c3 push af       ;
 $77c4 ld c,(ix+$00) ;
 $77c7 call $77d6    ;
 $77ca ld d,a        ;
 $77cb pop af        ;
 $77cc ld e,$78      ;
 $77ce call $77e4    ;
 $77d1 inc ix        ;
 $77d3 jp $703a      ;

 ; Routine at 7808
 ;
 ; Used by the routine at #R$7025.
 c$7808 inc ix        ;
 $780a ld b,(ix+$00) ;
 $780d inc ix        ;
 $780f ld c,(ix+$00) ;
 $7812 inc ix        ;
 $7814 call $755e    ;
 $7817 ld b,(ix+$00) ;
 $781a inc ix        ;
 $781c ld a,(ix+$00) ;
 $781f inc ix        ;
 *$7821 push bc       ;
 $7822 push hl       ;
 $7823 ld b,(ix+$00) ;
 *$7826 ld (hl),a     ;
 $7827 inc hl        ;
 $7828 djnz $7826    ;
 $782a pop hl        ;
 $782b pop bc        ;
 $782c ld de,$0020   ;
 $782f add hl,de     ;
 $7830 djnz $7821    ;
 $7832 inc ix        ;
 $7834 jp $703a      ;

 ; Routine at 7368
 ;
 ; Used by the routine at #R$7025.
 c$7368 ld hl,$7365   ;
 $736b cp (hl)       ;
 $736c jr nz,$7376   ;
 $736e ld hl,($7366) ;
 $7371 ld b,$00      ;
 $7373 ld c,a        ;
 $7374 jr $738c      ;
 *$7376 ld hl,$ca33   ;
 $7379 ld b,a        ;
 $737a ld c,a        ;
 $737b ld ($7365),a  ;
 $737e or a          ;
 $737f jr z,$738c    ;
 *$7381 inc hl        ;
 $7382 ld a,(hl)     ;
 $7383 cp $fe        ;
 $7385 jr z,$7389    ;
 $7387 jr $7381      ;
 *$7389 djnz $7381    ;
 $738b inc hl        ;
 *$738c ld ($7366),hl ;
 $738f ld a,(hl)     ;
 $7390 push af       ;
 $7391 and $f0       ;
 $7393 rra           ;
 $7394 rra           ;
 $7395 rra           ;
 $7396 rra           ;
 $7397 inc a         ;
 $7398 ld ($5bc4),a  ;
 $739b ld ($5bc6),a  ;
 $739e pop af        ;
 $739f and $0f       ;
 $73a1 inc a         ;
 $73a2 ld ($5bc5),a  ;
 $73a5 ld ($5bc7),a  ;
 $73a8 ret           ;

 Routine at 74eb
 ;
 ; Used by the routine at #R$7532.
 c$74eb ld a,(ix+$01) ;
 $74ee ld ($74e7),a  ;
 $74f1 ld ($74e9),a  ;
 $74f4 ld a,(ix+$02) ;
 $74f7 ld ($74e8),a  ;
 $74fa ld a,(ix+$03) ;
 $74fd ld ($74ea),a  ;
 $7500 ld a,(ix+$04) ;
 $7503 ld ($74c9),a  ;
 $7506 ld ($7496),a  ;
 $7509 ld a,(ix+$03) ;
 $750c sub (ix+$02)  ;
 $750f and a         ;
 $7510 rra           ;
 ; This entry point is used by the routine at #R$753d.
 *$7511 ld ($74e3),a  ;
 $7514 ld ($74aa),a  ;
 $7517 ld bc,($74e7) ;
 $751b ld de,($74e9) ;
 $751f push bc       ;
 $7520 push de       ;
 $7521 call $74ae    ;
 $7524 pop de        ;
 $7525 pop bc        ;
 $7526 ld ($74e7),bc ;
 $752a ld ($74e9),de ;
 $752e call $7485    ;
 $7531 ret           ;

 ; Routine at 6fd0
 ;
 ; Used by the routine at #R$7001.
 c$6fd0 ld hl,$5800   ;
 $6fd3 ld de,$5801   ;
 $6fd6 ld bc,$017f   ;
 $6fd9 ld (hl),$00   ;
 $6fdb ldir          ;
 $6fdd ld hl,$5800   ;
 $6fe0 ld de,$6e50   ;
 $6fe3 ld bc,$0180   ;
 $6fe6 ldir          ;
 $6fe8 ld bc,$af00   ;
 *$6feb push bc       ;
 $6fec call $22aa    ;
 $6fef push hl       ;
 $6ff0 pop de        ;
 $6ff1 inc de        ;
 $6ff2 ld bc,$001f   ;
 $6ff5 ld (hl),$00   ;
 $6ff7 ldir          ;
 $6ff9 pop bc        ;
 $6ffa dec b         ;
 $6ffb ld a,$4f      ;
 $6ffd cp b          ;
 $6ffe jr nz,$6feb   ;
 $7000 ret           ;

 22AA    LD A,$AF    Test that the y co-ordinate (in                    B) is not greater than 175.
 22AC    SUB B
 22AD    JP C,REPORT_B_3
 22B0    LD B,A    B now contains 175 minus y.
 22B1    AND A    A holds b7b6b5b4b3b2b1b0, the bits                of B.
 22B2    RRA    And now 0b7b6b5b4b3b2b1.
 22B3    SCF    Now 10b7b6b5b4b3b2.
 22B4    RRA
 22B5    AND A    Now 010b7b6b5b4b3.
 22B6    RRA
 22B7    XOR B    Finally 010b7b6b2b1b0, so that H becomes 64+8*INT(B/64)+(B mod 8), the high byte of the pixel address.
 22B8    AND %11111000
 22BA    XOR B
 22BB    LD H,A
 22BC    LD A,C    C contains x.
 22BD    RLCA    A starts as c7c6c5c4c3c2c1c0 and becomes c4c3c2c1c0c7c6c5.
 22BE    RLCA
 22BF    RLCA
 22C0    XOR B    Now c4c3b5b4b3c7c6c5.
 22C1    AND %11000111
 22C3    XOR B
 22C4    RLCA    Finally b5b4b3c7c6c5c4c3, so that L becomes 32*INT((B mod 64)/8)+INT(x/8), the low byte.
 22C5    RLCA
 22C6    LD L,A
 22C7    LD A,C    A holds x mod 8, so the pixel is bit (7-A) within the byte.
 22C8    AND $07
 22CA    RET

 ; Routine at 7612
 ;
 ; Used by the routine at #R$763b.
 c$7612 ld a,(ix+$03) ;
 $7615 ld ($75e4),a  ;
 $7618 ld ($7603),a  ;
 $761b ld a,(ix+$04) ;
 $761e ld b,(ix+$01) ;
 $7621 ld c,(ix+$02) ;

 ; Routine at 7624
 ;
 ; Used by the routine at #R$7646.
 c$7624 ld ($75bc),a  ;
 $7627 ld ($75ee),a  ;
 $762a and a         ;
 $762b rra           ;
 $762c ld ($75c7),a  ;
 $762f ld ($75f5),a  ;
 $7632 push bc       ;
 $7633 call $75b6    ;
 $7636 pop bc        ;
 $7637 call $75e9    ;
 $763a ret           ;

 *$7736 ld a,(ix+$01)  ;
 $7739 ld ($74e7),a   ;
 $773c ld ($74e9),a   ;
 $773f ld a,(ix+$02)  ;
 $7742 ld ($74e8),a   ;
 $7745 ld a,(ix+$03)  ;
 $7748 ld ($74ea),a   ;
 $774b ld a,(ix+$04)  ;
 $774e ld ($76da),a   ;
 $7751 ld ($7712),a   ;
 $7754 ld a,(ix+$03)  ;
 $7757 add a,(ix+$02) ;
 $775a and a          ;
 $775b rra

 ; Routine at 775c
 ;
 ; Used by the routine at #R$7788.
 c$775c ld ($76f3),a  ;
 $775f ld ($7732),a  ;
 $7762 ld bc,($74e7) ;
 $7766 ld de,($74e9) ;
 $776a push bc       ;
 $776b push de       ;
 $776c call $76f7    ;
 $776f pop de        ;
 $7770 pop bc        ;
 $7771 ld ($74e7),bc ;
 $7775 ld ($74e9),de ;
 $7779 call $76c9    ;
 $777c ret           ;

 ; Routine at 77e4
 ;
 ; Used by the routine at #R$77ac.
 c$77e4 ld bc,$0180   ; The number of attributes
 $77e7 push ix       ;
 $77e9 ld ix,$6e50   ;
 $77ed ld l,a        ;
 *$77ee ld a,(ix+$00) ;
 $77f1 and e         ;
 $77f2 cp l          ;
 $77f3 jr nz,$77fe   ;
 $77f5 ld a,(ix+$00) ;
 $77f8 or e          ;
 $77f9 xor e         ;
 $77fa or d          ;
 $77fb ld (ix+$00),a ;
 *$77fe inc ix        ;
 $7800 dec bc        ;
 $7801 ld a,b        ;
 $7802 or c          ;
 $7803 jr nz,$77ee   ;
 $7805 pop ix        ;
 $7807 ret           ;

 ; Routine at 77d6
 ;
 ; Used by the routine at #R$77ac.
 c$77d6 ld a,c        ;
 $77d7 and $07       ;
 $77d9 rla           ;
 $77da rla           ;
 $77db rla           ;
 $77dc and $78       ;
 $77de ld b,a        ;
 $77df ld a,c        ;
 $77e0 and $40       ;
 $77e2 or b          ;
 $77e3 ret           ;

 ; Used by the routines at #R$7421, #R$75e9 and #R$7808.
 c$755e ld h,$00      ;
 $7560 ld l,b        ;
 $7561 add hl,hl     ;
 $7562 add hl,hl     ;
 $7563 add hl,hl     ;
 $7564 add hl,hl     ;
 $7565 add hl,hl     ;
 $7566 ld de,$6e50   ;
 $7569 add hl,de     ;
 $756a ld b,$00      ;
 $756c add hl,bc     ;
 $756d ret           ;

 ; Routine at 73f2
 ;
 ; Used by the routine at #R$743c. mirror left half?
 c$73f2 push bc       ;
 $73f3 call $75a1    ;
 $73f6 push hl       ;
 $73f7 ld de,$0020   ;
 $73fa ld b,$08      ;
 $73fc add hl,de     ;
 $73fd dec hl        ;
 $73fe pop de        ;
 *$73ff push hl       ;
 $7400 push de       ;
 $7401 push bc       ;
 $7402 ld b,$10      ;
 *$7404 ld a,(de)     ;
 $7405 ld c,$80      ;
 *$7407 rl a          ;
 $7409 rr c          ;
 $740b jr nc,$7407   ;
 $740d ld (hl),c     ;
 $740e inc de        ;
 $740f dec hl        ;
 $7410 djnz $7404    ;
 $7412 pop bc        ;
 $7413 pop de        ;
 $7414 pop hl        ;
 $7415 inc d         ;
 $7416 inc h         ;
 $7417 djnz $73ff    ;
 $7419 pop bc        ;
 $741a inc b         ;
 $741b ld a,$0c      ;
 $741d cp b          ;
 $741e jr nz,$73f2   ;
 $7420 ret           ;

 ; Routine at 7421
 ;
 ; Used by the routine at #R$743c.
 c$7421 push bc       ;
 $7422 call $755e    ;
 $7425 ld de,$0020   ;
 $7428 push hl       ;
 $7429 add hl,de     ;
 $742a dec hl        ;
 $742b pop de        ;
 $742c ld b,$10      ;
 *$742e ld a,(de)     ;
 $742f ld (hl),a     ;
 $7430 inc de        ;
 $7431 dec hl        ;
 $7432 djnz $742e    ;
 $7434 pop bc        ;
 $7435 inc b         ;
 $7436 ld a,$0c      ;
 $7438 cp b          ;
 $7439 jr nz,$7421   ;
 $743b ret           ;

 ; Routine at 74ae
 ;
 ; Used by the routine at #R$74eb.
 c$74ae ld bc,($74e7) ;
 $74b2 call $75a1    ;
 $74b5 push hl       ;
 $74b6 ld bc,($74e9) ;
 $74ba call $75a1    ;
 $74bd pop de        ;
 $74be ld a,h        ;
 $74bf add a,$07     ;
 $74c1 ld h,a        ;
 $74c2 ld b,$08      ;
 $74c4 ex de,hl      ;
 *$74c5 push bc       ;
 $74c6 push de       ;
 $74c7 push hl       ;
 $74c8 ld bc,$0000   ;
 $74cb ldir          ;
 $74cd pop hl        ;
 $74ce pop de        ;
 $74cf pop bc        ;
 $74d0 inc h         ;
 $74d1 dec d         ;
 $74d2 djnz $74c5    ;
 $74d4 ld a,($74e8)  ;
 $74d7 inc a         ;
 $74d8 ld ($74e8),a  ;
 $74db ld a,($74ea)  ;
 $74de dec a         ;
 $74df ld ($74ea),a  ;
 $74e2 cp $00        ;
 $74e4 jr nz,$74ae   ;
 $74e6 ret           ;

 ; Routine at 75a1
 ;
 ; Used by the routines at #R$73f2 and #R$75b6.
 c$75a1 ld a,b        ;
 $75a2 call $60c1    ;
 $75a5 push de       ;
 $75a6 pop hl        ;
 $75a7 ld b,$00      ;
 $75a9 add hl,bc     ;
 $75aa ret           ;

 ; Routine at 7485
 ;
 ; Used by the routine at #R$74eb.
 c$7485 ld bc,($74e7) ;
 $7489 call $755e    ;
 $748c push hl       ;
 $748d ld bc,($74e9) ;
 $7491 call $755e    ;
 $7494 pop de        ;
 $7495 ld bc,$0000   ;
 $7498 ex de,hl      ;
 $7499 ldir          ;
 $749b ld a,($74e8)  ;
 $749e inc a         ;
 $749f ld ($74e8),a  ;
 $74a2 ld a,($74ea)  ;
 $74a5 dec a         ;
 $74a6 ld ($74ea),a  ;
 $74a9 cp $00        ;
 $74ab jr nz,$7485   ;
 $74ad ret           ;

 ; Routine at 75b6
 ;
 ; Used by the routine at #R$7624.
 c$75b6 push bc       ;
 $75b7 call $75a1    ;
 $75ba push hl       ;
 $75bb ld de,$0020   ;
 $75be ld b,$08      ;
 $75c0 add hl,de     ;
 $75c1 dec hl        ;
 $75c2 pop de        ;
 *$75c3 push hl       ;
 $75c4 push de       ;
 $75c5 push bc       ;
 $75c6 ld b,$10      ;
 *$75c8 call $7608    ;
 $75cb ex de,hl      ;
 $75cc call $7608    ;
 $75cf ex de,hl      ;
 $75d0 ld a,(de)     ;
 $75d1 ld c,(hl)     ;
 $75d2 ex de,hl      ;
 $75d3 ld (de),a     ;
 $75d4 ld (hl),c     ;
 $75d5 ex de,hl      ;
 $75d6 inc de        ;
 $75d7 dec hl        ;
 $75d8 djnz $75c8    ;
 $75da pop bc        ;
 $75db pop de        ;
 $75dc pop hl        ;
 $75dd inc d         ;
 $75de inc h         ;
 $75df djnz $75c3    ;
 $75e1 pop bc        ;
 $75e2 inc b         ;
 $75e3 ld a,$0c      ;
 $75e5 cp b          ;
 $75e6 jr nz,$75b6   ;
 $75e8 ret           ;

 ; Routine at 76f7
 ;
 ; Used by the routine at #R$775c.
 c$76f7 ld bc,($74e7)  ;
 $76fb call $75a1     ;
 $76fe push hl        ;
 $76ff ld bc,($74e9)  ;
 $7703 call $75a1     ;
 $7706 pop de         ;
 $7707 ld a,h         ;
 $7708 add a,$07      ;
 $770a ld h,a         ;
 $770b ld b,$08       ;
 $770d ex de,hl       ;
 *$770e push hl        ;
 $770f push de        ;
 $7710 push bc        ;
 $7711 ld b,$00       ;
 *$7713 ld c,(hl)      ;
 $7714 ld a,(de)      ;
 $7715 ex de,hl       ;
 $7716 ld (hl),c      ;
 $7717 ld (de),a      ;
 $7718 inc de         ;
 $7719 inc hl         ;
 $771a djnz $7713     ;
 $771c pop bc         ;
 $771d pop de         ;
 $771e pop hl         ;
 $771f inc h          ;
 $7720 dec d          ;
 $7721 djnz $770e     ;
 $7723 ld a,($74e8)   ;
 $7726 inc a          ;
 $7727 ld ($74e8),a   ;
 $772a ld a,($74ea)   ;
 $772d dec a          ;
 $772e ld ($74ea),a   ;
 $7731 cp $00         ;
 $7733 jr nz,$76f7    ;
 $7735 ret            ;

 ; Routine at 75e9
 ;
 ; Used by the routine at #R$7624.
 c$75e9 push bc       ;
 $75ea call $755e    ;
 $75ed ld de,$0020   ;
 $75f0 push hl       ;
 $75f1 add hl,de     ;
 $75f2 dec hl        ;
 $75f3 pop de        ;
 $75f4 ld b,$10      ;
 *$75f6 ld a,(de)     ;
 $75f7 ld c,(hl)     ;
 $75f8 ex de,hl      ;
 $75f9 ld (hl),c     ;
 $75fa ld (de),a     ;
 $75fb ex de,hl      ;
 $75fc inc de        ;
 $75fd dec hl        ;
 $75fe djnz $75f6    ;
 $7600 pop bc        ;
 $7601 inc b         ;
 $7602 ld a,$0c      ;
 $7604 cp b          ;
 $7605 jr nz,$75e9   ;
 $7607 ret           ;

 ; Routine at 7608
 ;
 ; Used by the routine at #R$75b6.
 c$7608 ld c,$80      ;
 *$760a rl (hl)       ;
 $760c rr c          ;
 $760e jr nc,$760a   ;
 $7610 ld (hl),c     ;
 $7611 ret           ;

 ; Routine at 76c9
 ;
 ; Used by the routine at #R$775c.
 c$76c9 ld bc,($74e7) ;
 $76cd call $755e    ;
 $76d0 push hl       ;
 $76d1 ld bc,($74e9) ;
 $76d5 call $755e    ;
 $76d8 pop de        ;
 $76d9 ld b,$00      ;
 *$76db ld a,(de)     ;
 $76dc ld c,(hl)     ;
 $76dd ex de,hl      ;
 $76de ld (hl),c     ;
 $76df ld (de),a     ;
 $76e0 inc hl        ;
 $76e1 inc de        ;
 $76e2 djnz $76db    ;
 $76e4 ld a,($74e8)  ;
 $76e7 inc a         ;
 $76e8 ld ($74e8),a  ;
 $76eb ld a,($74ea)  ;
 $76ee dec a         ;
 $76ef ld ($74ea),a  ;
 $76f2 cp $00        ;
 $76f4 jr nz,$76c9   ;
 $76f6 ret           ;
 */
