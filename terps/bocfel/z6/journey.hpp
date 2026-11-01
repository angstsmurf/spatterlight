//
//  journey.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

#ifndef journey_hpp
#define journey_hpp

#include <cstdint>

extern "C" {
#include "glk.h"
}

#include "spatterlight-autosave.h"

struct JourneyGlobals {
    uint8_t COMMAND_START_LINE;
    uint8_t NAME_COLUMN;
    uint8_t PARTY_COMMAND_COLUMN;
    uint8_t CHR_COMMAND_COLUMN;
    uint8_t COMMAND_OBJECT_COLUMN;

    uint8_t COMMAND_WIDTH;
    uint8_t COMMAND_WIDTH_PIX;
    uint8_t NAME_WIDTH;
    uint8_t NAME_WIDTH_PIX;

    uint8_t INTERPRETER;
    uint8_t CHRV;
    uint8_t CHRH;
    uint8_t SCREEN_HEIGHT;
    uint8_t SCREEN_WIDTH;

    uint8_t BORDER_FLAG;
    uint8_t FONT3_FLAG;
    uint8_t FWC_FLAG;
    uint8_t BLACK_PICTURE_BORDER;

    uint8_t HERE;

    uint8_t TOP_SCREEN_LINE;
    uint8_t TEXT_WINDOW_LEFT;

    uint8_t E_LEXV;
    uint8_t E_INBUF;

    uint8_t E_TEMP;
    uint8_t E_TEMP_LEN;

    uint8_t PARTY;
    uint8_t PARTY_COMMANDS;
    uint8_t CHARACTER_INPUT_TBL;
    uint8_t O_TABLE;

    uint8_t TAG_NAME;
    uint8_t NAME_TBL;
    uint8_t TAG_NAME_LENGTH;

    uint8_t UPDATE_FLAG;
    uint8_t SMART_DEFAULT_FLAG;
    uint8_t SUBGROUP_MODE;
};

struct JourneyRoutines {
    uint32_t FILL_CHARACTER_TBL;
    uint32_t SMART_DEFAULT;
    uint32_t MASSAGE_ELVISH;
    uint32_t PARSE_ELVISH;
    uint32_t ILLEGAL_NAME;
    uint32_t REMOVE_TRAVEL_COMMAND;

    uint32_t BOLD_PARTY_CURSOR;
    uint32_t BOLD_CURSOR;
    uint32_t BOLD_OBJECT_CURSOR;

    uint32_t GRAPHIC;
    uint32_t COMPLETE_DIAL_GRAPHICS;
};

struct JourneyObjects {
    uint16_t START_LOC;
    uint16_t TAG_OBJECT;
    uint16_t TAG_ROUTE_COMMAND;
    uint16_t TAG;
    uint16_t PRAXIX;
    uint16_t BERGON;

};

struct JourneyAttributes {
    uint8_t SEEN;
    uint8_t KBD;
    uint8_t SHADOW;
    uint8_t SUBGROUP;
    uint8_t SDESC;
    uint8_t DESC12;
    uint8_t DESC8;
    uint8_t buffer_window_index;
};

// The rows of the entrypoint table: entered when the game reaches the routine
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

// The V6GameHooks of this game (v6_shared.cpp)
int journey_draw_picture(int pic, winid_t journey_window);
void journey_update_on_resize(void);
void journey_stash_state(library_state_data *dat);
void journey_recover_state(library_state_data *dat);
void journey_update_after_restore(void);
void journey_autorestore_internal_read_char_hacks(void);

// Called from other files
void journey_pre_save_hacks(void);
void journey_post_save_hacks(void);

// State other files read
extern JourneyGlobals jg;
extern JourneyRoutines jr;
extern JourneyObjects jo;
extern JourneyAttributes ja;

#endif /* journey_hpp */
