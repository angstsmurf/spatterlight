//
//  journey.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

#ifndef journey_hpp
#define journey_hpp

#include <stdio.h>

extern "C" {
#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#include "spatterlight-autosave.h"
#endif
}

void journey_adjust_image(int picnum, uint16_t *x, uint16_t *y, int width, int height, int winwidth, int winheight, float *scale, float pixelwidth);

void journey_update_on_resize(void);
void journey_update_after_restore(void);
void journey_update_after_autorestore(void);
bool journey_autorestore_internal_read_char_hacks(void);

void after_INTRO(void);
void REFRESH_SCREEN(void);
void INIT_SCREEN(void);
void DIVIDER(void);
void WCENTER(void);
void BOLD_CURSOR(void);
void BOLD_PARTY_CURSOR(void);
void BOLD_OBJECT_CURSOR(void);
void PRINT_COLUMNS(void);
void PRINT_CHARACTER_COMMANDS(void);
void REFRESH_CHARACTER_COMMAND_AREA(void);
void GRAPHIC_STAMP(void);
void READ_ELVISH(void);
void CHANGE_NAME(void);
void ERASE_COMMAND(void);
void COMPLETE_DIAL_GRAPHICS(void);
void TELL_AMOUNTS(void);

extern int16_t selected_journey_line;
extern int16_t selected_journey_column;

typedef struct JourneyGlobals {
    uint8_t COMMAND_START_LINE; // G0e•
    uint8_t NAME_COLUMN; // Ga3•
    uint8_t PARTY_COMMAND_COLUMN; // Gb4•
    uint8_t CHR_COMMAND_COLUMN; // G28•
    uint8_t COMMAND_OBJECT_COLUMN; // Gb0•

    uint8_t COMMAND_WIDTH; // Gb8•
    uint8_t COMMAND_WIDTH_PIX; // G6d•
    uint8_t NAME_WIDTH; // Ga2•
    uint8_t NAME_WIDTH_PIX; // G82•

    uint8_t INTERPRETER; // G7f•
    uint8_t CHRV; // G89•
    uint8_t CHRH; // G92•
    uint8_t SCREEN_HEIGHT; // G64•
    uint8_t SCREEN_WIDTH; // G83•

    uint8_t BORDER_FLAG; // G9f•
    uint8_t FONT3_FLAG; // G31•
    uint8_t FWC_FLAG; // Gb9•
    uint8_t BLACK_PICTURE_BORDER; // G72•

    uint8_t HERE; // G0f•

    uint8_t TOP_SCREEN_LINE; // G25•
    uint8_t TEXT_WINDOW_LEFT; // G38•

    uint8_t E_LEXV; // G75•
    uint8_t E_INBUF; // G34•

    uint8_t E_TEMP; // G55•
    uint8_t E_TEMP_LEN; // G03•

    uint8_t PARTY; // G63•
    uint8_t PARTY_COMMANDS; // G06•
    uint8_t CHARACTER_INPUT_TBL; // G32•
    uint8_t O_TABLE; // G14•

    uint8_t TAG_NAME; // G96•
    uint8_t NAME_TBL; // Gba•
    uint8_t TAG_NAME_LENGTH; // G1b•

    uint8_t UPDATE_FLAG; // G79•
    uint8_t SMART_DEFAULT_FLAG; // G40•
    uint8_t SUBGROUP_MODE; // G80•
} JourneyGlobals;

extern JourneyGlobals jg;

typedef struct JourneyRoutines {
    uint32_t FILL_CHARACTER_TBL; // 0x4bf8•
    uint32_t SMART_DEFAULT; // 0x64bc•
    uint32_t MASSAGE_ELVISH; // 0x7b5c•
    uint32_t PARSE_ELVISH; // 0x1951c•
    uint32_t ILLEGAL_NAME; // 0x78a0•
    uint32_t REMOVE_TRAVEL_COMMAND; // 0x676c•

    uint32_t BOLD_PARTY_CURSOR; // 0x6228•
    uint32_t BOLD_CURSOR; // 0x529c•
    uint32_t BOLD_OBJECT_CURSOR; // 0x61a4•

    uint32_t GRAPHIC; // 0x49fc•
    uint32_t COMPLETE_DIAL_GRAPHICS; // 0x307a4•
} JourneyRoutines;

extern JourneyRoutines jr;

typedef struct JourneyObjects {
    uint16_t START_LOC; // 0xa0•
    uint16_t TAG_OBJECT; // 0x1a3•
    uint16_t TAG_ROUTE_COMMAND; // 0x3ea7•
    uint16_t TAG; // 0x78•
    uint16_t PRAXIX; // 0x4f•
    uint16_t BERGON; // 0x0196•

} JourneyObjects;

extern JourneyObjects jo;

typedef struct JourneyAttributes {
    uint8_t SEEN; // 0x2e•
    uint8_t KBD; // 0x39•
    uint8_t SHADOW; // 0x17•
    uint8_t SUBGROUP; // 0x2a•
    uint8_t SDESC; // 0x3a•
    uint8_t DESC12; // 0x29•
    uint8_t DESC8; // 0x2b•
    uint8_t buffer_window_index;
} JourneyAttributes;

extern JourneyAttributes ja;


void stash_journey_state(library_state_data *dat);
void recover_journey_state(library_state_data *dat);

void journey_pre_save_hacks(void);
void journey_post_save_hacks(void);

#endif /* journey_hpp */
