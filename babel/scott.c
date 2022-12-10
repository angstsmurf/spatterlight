/* scott.c  Treaty of Babel module for ScottFree files
 * GPL license.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT scott
#define HOME_PAGE "https://github.com/cspiegel/scottfree-glk"
#define FORMAT_EXT ".dat,.saga,.sna,.tzx,.tap,.z80,.d64,.t64,.dsk,.woz,.fiad,.atr"
#define NO_METADATA
#define NO_COVER

#define MAX_LENGTH 300000
#define MIN_LENGTH 24

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include "treaty_builder.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

static const char *ifids[] = {
    NULL,
    "3B1E4CB60F0063B49245B8D7C32DEE1E", // Adventureland
    "1A16C10E265A260429FD11B33E975017", // Pirate Adventure
    "2E50256FA2717A4AF3402E6CE18F623F", // Secret Mission
    "2D24B3D60A4605641C204C23511121AA", // Voodoo Castle
    "34894B343CAACE66763AE40114A1C12F", // The Count
    "BA821285F91F5F59A6DFDDD7999F7F75", // Strange Odyssey
    "CE3AEE5F23E49107BEB2E333C29F2AFC", // Mystery Fun House
    "BB7E4E7CB57249E7671E7DE081D715B7", // Pyramid of Doom
    "F0D164D13861E3FB074B9B0BAC810777", // Ghost Town
    "E247F4152EC664464BD9A6C0A092E05B", // Savage Island part I
    "87EAA235B329356EF902933F6B8A1141", // Savage Island part II
    "707015A14ADCE841F1E1DCC90C847BBA", // The Golden Voyage
    "B5AF6E4DB3C3B2118FAEA3849F807617", // The Sorcerer Of Claymorgue Castle
    "EEC3C968F850EDF00BC8A80BB3D69FF0", // Questprobe featuring The Hulk
    "DAEE386546CE71831DC365B0FF10F233", // Questprobe featuring Spider-Man
    "13EA7A22731E90598456D13311923833", // Buckaroo Banzai

    "186efec3-b22e-49af-b751-6b84f04b6957", // The Golden Baton
    "59121f68-6194-4b26-975a-b1b98598930c", // The Time Machine
    "2f527feb-1c68-4e83-8ec7-062f5f3de3a3", // Arrow of Death part 1
    "e1161865-90af-44c0-a706-acf069d5327a", // Arrow of Death part 2
    "5b92a1fd-03f9-43b3-8d93-f8034ae02534", // Escape from Pulsar 7
    "38a97e57-a465-4fb6-baad-835973b64b3a", // Circus
    "18f3167b-93c9-4323-8195-7b9166c9a304", // Feasibility Experiment
    "258b5da1-180c-48be-928e-add508478301", // The Wizard of Akyrz
    "0fa37e50-60ba-48eb-97ac-f4379a5f0096", // Perseus and Andromeda
    "c25e70bd-120a-4cfd-95b7-60d2faca026b", // Ten Little Indians
    "c3753c92-a85e-4bf1-8578-8b3623be8fca", // Waxworks

    "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A", // Gremlins
    "C9C5B722-D87E-46B4-9092-4DCA978A4668", // Gremlins - The Adventure (Spanish)
    "AA9FF03F-FBE3-4767-8ACD-29FBFDCD3A91", // Gremlins - The Adventure (German)
    "75EE0452-0A6A-4100-9185-A79316812E0B", // Super Gran - The Adventure
    "E8021308-8719-4A34-BDF9-C6F388129E53", // Robin Of Sherwood
    "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB"  // Seas Of Blood
};

typedef enum {
    NO_IFID,
    ADVENTURELAND_IFID,
    PIRATE_ADVENTURE_IFID,
    SECRET_MISSION_IFID,
    VOODOO_CASTLE_IFID,
    THE_COUNT_IFID,
    STRANGE_ODYSSEY_IFID,
    MYSTERY_FUNHOUSE_IFID,
    PYRAMID_OF_DOOM_IFID,
    GHOST_TOWN_IFID,
    SAVAGE_ISLAND_PART_I_IFID,
    SAVAGE_ISLAND_PART_II_IFID,
    GOLDEN_VOYAGE_IFID,
    SORCERER_OF_CLAYMORGUE_CASTLE_IFID,
    QUESTPROBE_HULK_IFID,
    QUESTPROBE_SPIDERMAN_IFID,
    BUCKAROO_BANZAI_IFID,
    GOLDEN_BATON_IFID,
    TIME_MACHINE_IFID,
    ARROW_OF_DEATH_PART_1_IFID,
    ARROW_OF_DEATH_PART_2_IFID,
    ESCAPE_FROM_PULSAR_7_IFID,
    CIRCUS_IFID,
    FEASIBILITY_EXPERIMENT_IFID,
    WIZARD_OF_AKYRZ_IFID,
    PERSEUS_AND_ANDROMEDA_IFID,
    TEN_LITTLE_INDIANS_IFID,
    WAXWORKS_IFID,
    GREMLINS_IFID,
    GREMLINS_SPANISH_IFID,
    GREMLINS_GERMAN_IFID,
    SUPERGRAN_IFID,
    ROBIN_OF_SHERWOOD_IFID,
    SEAS_OF_BLOOD_IFID,
} IfidType;

struct scottrec {
    int32 length;
    uint16_t chk;
    IfidType ifid;
};

static const struct scottrec scott_registry[] = {
    { 0x44cd, 0x8b8f, PIRATE_ADVENTURE_IFID }, // Pirate Adventure z80
    { 0x44af, 0x8c9e, PIRATE_ADVENTURE_IFID }, // Pirate Adventure z80 alt
    { 0x44af, 0x8847, PIRATE_ADVENTURE_IFID }, // Pirate Adventure z80 alt 2
    { 0x5bbf, 0x29fb, PIRATE_ADVENTURE_IFID }, // Pirate Adventure z80 Parsec
    { 0x432c, 0xa6df, VOODOO_CASTLE_IFID }, // Voodoo Castle z80
    { 0x4308, 0xa634, VOODOO_CASTLE_IFID }, // Voodoo Castle z80 a
    { 0x4308, 0xa2d2, VOODOO_CASTLE_IFID }, // Voodoo Castle z80 a3
    { 0x68ea, 0x0d4b, VOODOO_CASTLE_IFID }, // Voodoo Castle z80 Parsec
    { 0x431e, 0x7437, STRANGE_ODYSSEY_IFID }, // Strange Odyssey z80
    { 0x68de, 0xeead, STRANGE_ODYSSEY_IFID }, // Strange Odyssey z80 a
    { 0x42f8, 0x70e1, STRANGE_ODYSSEY_IFID }, // Strange Odyssey z80 Parsec

    { 0x4024, 0x62c7, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai z80
    { 0x3ff8, 0x5fb4, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai z80 alt
    { 0x65d0, 0xd983, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai z80 Parsec

    { 0x7d3c, 0x142b, GOLDEN_BATON_IFID }, // The Golden Baton Z80
    { 0x6f48, 0x2100, GOLDEN_BATON_IFID }, // The Golden Baton Z80 alt
    { 0x7f22, 0xe755, GOLDEN_BATON_IFID }, // The Golden Baton Z80 alt 2
    { 0x7ee3, 0xe070, GOLDEN_BATON_IFID }, // The Golden Baton Z80 alt 3
    { 0x70c8, 0xeecc, GOLDEN_BATON_IFID }, // The Golden Baton Z80 alt 4
    { 0x8b90, 0x85dc, GOLDEN_BATON_IFID }, // The Golden Baton tzx
    { 0x912e, 0xd9ad, GOLDEN_BATON_IFID }, // The Golden Baton alt tzx
    { 0x911c, 0xd15c, GOLDEN_BATON_IFID }, // The Golden Baton alt 2 tzx
    { 0xb840, 0x843c, GOLDEN_BATON_IFID }, // The Golden Baton (Channel 8).tzx
   { 0x2ab00, 0x9dca, GOLDEN_BATON_IFID }, // The Golden Baton C64, D64
    { 0x5170, 0xb240, GOLDEN_BATON_IFID }, // The Golden Baton C64, T64
    { 0x78d0, 0x3db6, TIME_MACHINE_IFID }, // The Time Machine Z80
    { 0xb829, 0x1730, TIME_MACHINE_IFID }, // The Time Machine tzx
    { 0x912e, 0x1e6d, TIME_MACHINE_IFID }, // The Time Machine tzx alt
    { 0x911c, 0x171f, TIME_MACHINE_IFID }, // The Time Machine (Channel 8).tzx
    { 0xb847, 0x21d6, TIME_MACHINE_IFID }, // The Time Machine Alt (Channel 8).tzx
    { 0xb847, 0x22f8, TIME_MACHINE_IFID }, // The Time Machine (Tynesoft).tzx
    { 0xb80d, 0x12c2, TIME_MACHINE_IFID }, // The Time Machine TAP
    { 0x5032, 0x5635, TIME_MACHINE_IFID }, // The Time Machine C64
    { 0x8539, 0xad02, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 z80
    { 0x8699, 0xbb7b, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 z80 alt
    { 0x7844, 0xd892, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 z80 alt 2
    { 0x8699, 0xbb89, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 z80 alt 3
    { 0x865c, 0xb434, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 z80 alt 4
    { 0xb847, 0xb686, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 tzx
    { 0x912e, 0x2b65, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 tzx alt
   { 0x12281, 0xeac2, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 (Channel 8).tzx
    { 0xb7f3, 0xa2d8, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 TAP
    { 0x5b46, 0x92db, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 C64
   { 0x2ab00, 0xe71d, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 C64 alt
   { 0x2ab00, 0x7687, ARROW_OF_DEATH_PART_1_IFID }, // Arrow of Death part 1 C64 alt 2
    { 0x949b, 0xb7e2, ARROW_OF_DEATH_PART_2_IFID }, // Arrow of Death part 2 z80
    { 0x87dd, 0x5560, ARROW_OF_DEATH_PART_2_IFID }, // Arrow of Death part 2 z80 alt
    { 0x86cf, 0xbefc, ARROW_OF_DEATH_PART_2_IFID }, // Arrow of Death part 2 z80 alt 2
    { 0xb7f3, 0x721e, ARROW_OF_DEATH_PART_2_IFID }, // Arrow of Death part 2 TAP
    { 0xb847, 0x8797, ARROW_OF_DEATH_PART_2_IFID }, // Arrow of Death part 2 tzx
    { 0xb847, 0x862c, ARROW_OF_DEATH_PART_2_IFID }, // Arrow of Death part 2 (Channel 8).tzx
    { 0x912e, 0xb912, ARROW_OF_DEATH_PART_2_IFID }, // Arrow of Death part 2 tzx alt
    { 0x5fe2, 0xe14f, ARROW_OF_DEATH_PART_2_IFID }, // Arrow of Death part 2 C64
    { 0x7509, 0x497d, ESCAPE_FROM_PULSAR_7_IFID }, // Escape from Pulsar 7 z80
    { 0x7551, 0x2c10, ESCAPE_FROM_PULSAR_7_IFID }, // Escape from Pulsar 7 z80 alt
    { 0x67d6, 0x6fa8, ESCAPE_FROM_PULSAR_7_IFID }, // Escape from Pulsar 7 z80 alt 2
    { 0xb847, 0xd29b, ESCAPE_FROM_PULSAR_7_IFID }, // Escape From Pulsar 7 tzx
    { 0x912e, 0xcf41, ESCAPE_FROM_PULSAR_7_IFID }, // Escape From Pulsar 7 - Alternate.tzx
    { 0x7bb8, 0x3ddb, ESCAPE_FROM_PULSAR_7_IFID }, // Escape From Pulsar 7 - Alternate #2.tzx
    { 0xb855, 0xd6f1, ESCAPE_FROM_PULSAR_7_IFID }, // Escape From Pulsar 7 (Channel 8).tzx
    { 0xb847, 0xd393, ESCAPE_FROM_PULSAR_7_IFID }, // Escape From Pulsar 7 (Tynesoft).tzx
    { 0xb80d, 0xc196, ESCAPE_FROM_PULSAR_7_IFID }, // Escape From Pulsar 7 TAP
    { 0x46bf, 0x1679, ESCAPE_FROM_PULSAR_7_IFID }, // Escape from Pulsar 7 C64
    { 0x6c62, 0xd002, CIRCUS_IFID }, // Circus z80
    { 0x9136, 0x8435, CIRCUS_IFID }, // Circus tzx
    { 0x912e, 0x80e5, CIRCUS_IFID }, // Circus - Alternate.tzx
    { 0x911c, 0x796d, CIRCUS_IFID }, // Circus - Alternate #2.tzx
    { 0x7d74, 0x73a8, CIRCUS_IFID }, // Circus TAP
    { 0x9136, 0x84b6, CIRCUS_IFID }, // Circus (Channel 8).tzx
    { 0x4269, 0xa449, CIRCUS_IFID }, // Circus C64
    { 0x9213, 0xf3ca, FEASIBILITY_EXPERIMENT_IFID }, // Feasibility Experiment z80
    { 0x912e, 0x21a4, FEASIBILITY_EXPERIMENT_IFID }, // Feasibility Experiment tzx
    { 0x913a, 0x220c, FEASIBILITY_EXPERIMENT_IFID }, // Feasibility Experiment (Channel 8).tzx
    { 0x5a7b, 0x0f48, FEASIBILITY_EXPERIMENT_IFID }, // Feasibility Experiment C64
    { 0x83d9, 0x1bb9, WIZARD_OF_AKYRZ_IFID }, // The Wizard of Akyrz z80
    { 0x8422, 0x2eb8, WIZARD_OF_AKYRZ_IFID }, // The Wizard of Akyrz z80 alt
    { 0x9232, 0x0c09, WIZARD_OF_AKYRZ_IFID }, // The Wizard of Akyrz TAP
    { 0x911c, 0x7943, WIZARD_OF_AKYRZ_IFID }, // The Wizard Of Akyrz tzx
    { 0x912e, 0x7fde, WIZARD_OF_AKYRZ_IFID }, // The Wizard Of Akyrz Alternate.tzx
    { 0xb846, 0x1de4, WIZARD_OF_AKYRZ_IFID }, // The Wizard Of Akyrz (Channel 8).tzx
   { 0x2ab00, 0x6cca, WIZARD_OF_AKYRZ_IFID }, // The Wizard of Akyrz C64, D64
    { 0x4be1, 0x5a00, WIZARD_OF_AKYRZ_IFID }, // The Wizard of Akyrz C64, T64
    { 0x7b10, 0x4f06, PERSEUS_AND_ANDROMEDA_IFID }, // Perseus and Andromeda.z80
    { 0x76fd, 0x3a77, PERSEUS_AND_ANDROMEDA_IFID }, // Perseus and Andromeda.z80 alt
    { 0x6db4, 0x84fe, PERSEUS_AND_ANDROMEDA_IFID }, // Perseus and Andromeda.z80 alt 2
    { 0x8a8a, 0x0069, PERSEUS_AND_ANDROMEDA_IFID }, // Perseus and Andromeda TAP
    { 0xb846, 0x1071, PERSEUS_AND_ANDROMEDA_IFID }, // Perseus and Andromeda (Channel 8).tzx
    { 0x912e, 0x0fa1, PERSEUS_AND_ANDROMEDA_IFID }, // Perseus and Andromeda - Alternate.tzx
    { 0x911c, 0x067e, PERSEUS_AND_ANDROMEDA_IFID }, // Perseus and Andromeda.tzx
    { 0x502b, 0x913b, PERSEUS_AND_ANDROMEDA_IFID }, // Perseus and Andromeda C64
   { 0x2ab00, 0xdc5e, PERSEUS_AND_ANDROMEDA_IFID }, // Perseus and Andromeda C64 D64
    { 0x6fce, 0x4bac, PERSEUS_AND_ANDROMEDA_IFID }, // Perseus and Andromeda Italian
    { 0x7bb0, 0x3877, TEN_LITTLE_INDIANS_IFID }, // Ten Little Indians z80
    { 0x7c16, 0xe269, TEN_LITTLE_INDIANS_IFID }, // Ten Little Indians z80 alt
    { 0x6fdc, 0xa77e, TEN_LITTLE_INDIANS_IFID }, // Ten Little Indians z80 alt 1
    { 0x6dee, 0x5156, TEN_LITTLE_INDIANS_IFID }, // Ten Little Indians z80 alt 2
    { 0x7c34, 0xe4db, TEN_LITTLE_INDIANS_IFID }, // Ten Little Indians z80 alt 3
    { 0xc01e, 0x6de7, TEN_LITTLE_INDIANS_IFID }, // Ten Little Indians z80 alt 4
    { 0x911c, 0xe5f0, TEN_LITTLE_INDIANS_IFID }, // Ten Little Indians tzx
    { 0x912e, 0xee20, TEN_LITTLE_INDIANS_IFID }, // Ten Little Indians tzx alternate
    { 0xb846, 0xeff2, TEN_LITTLE_INDIANS_IFID }, // Ten Little Indians (Channel 8).tzx
    { 0x4f9f, 0xe6c8, TEN_LITTLE_INDIANS_IFID }, // Ten Little Indians C64
    { 0x7f96, 0x4f96, WAXWORKS_IFID }, // Waxworks z80
    { 0x911c, 0xf961, WAXWORKS_IFID }, // Waxworks tzx
    { 0x912e, 0x01de, WAXWORKS_IFID }, // Waxworks Alternate.tzx
    { 0x9006, 0x060d, WAXWORKS_IFID }, // Waxworks (Dixons).tzx
    { 0x9136, 0x04a4, WAXWORKS_IFID }, // Waxworks (Channel 8).tzx
    { 0x8fcc, 0xf2fd, WAXWORKS_IFID }, // Waxworks TAP
    { 0x4a11, 0xa37a, WAXWORKS_IFID }, // Waxworks C64
    { 0x4a11, 0xa37a, WAXWORKS_IFID }, // Waxworks C64
   { 0x2ab00, 0x0a78, WAXWORKS_IFID }, // Waxworks C64 D64

   { 0x2ab00, 0xc3fc, GOLDEN_BATON_IFID }, // Mysterious Adventures C64 dsk 1
   { 0x2ab00, 0xbfbf, GOLDEN_BATON_IFID }, // Mysterious Adventures C64 dsk 1 alt
   { 0x2ab00, 0x9eaa, FEASIBILITY_EXPERIMENT_IFID }, // Mysterious Adventures C64 dsk 2
   { 0x2ab00, 0x9c18, FEASIBILITY_EXPERIMENT_IFID }, // Mysterious Adventures C64 dsk 2

    { 0xbcd5, 0xae3f, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk.tzx
    { 0xbc1e, 0xb4b6, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk (Americana).tzx
    { 0xafba, 0xb69b, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk Z80
    { 0x9f17, 0x4dae, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk Z80 alt
    { 0x9ef5, 0x8082, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk Z80 alt 2
    { 0x9ef9, 0x4a01, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk Z80 alt 3
    { 0xc01e, 0xa99d, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk Z80 alt 4
    { 0xc01e, 0x2f60, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk Z80 alt 5
   { 0x2ab00, 0xcdd8, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk C64 (D64)
    { 0x8534, 0x623a, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk C64 (T64)
   { 0x2ab00, 0x2918, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk US (D64)
   { 0x3954c, 0x8626, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk US (WOZ)
   { 0x3954c, 0x9126, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk US (WOZ) Graphics disk
   { 0x23000, 0x0160, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk US (DSK) pre-release
   { 0x23000, 0x93f6, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk US (DSK) pre-release grahics disk
   { 0x23000, 0x1cd4, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk US (DSK) (4am crack)
   { 0x23000, 0x6f7b, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk US (DSK) (4am crack) Graphics disk

   { 0x16810, 0x88e7, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk US v5.2-127 Atari 8-bit boot disk
   { 0x16810, 0x5728, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk US v5.2-127 Atari 8-bit boot disk alt

    { 0xbaef, 0x52cc, ADVENTURELAND_IFID }, // Adventureland.tzx
    { 0xbb84, 0x6d93, ADVENTURELAND_IFID }, // Adventureland - Alternate.tzx
    { 0xba74, 0x39c2, ADVENTURELAND_IFID }, // Adventureland TAP image
    { 0xaaae, 0x7cf6, ADVENTURELAND_IFID }, // Adventureland Z80
    { 0x9927, 0xd3d9, ADVENTURELAND_IFID }, // Adventureland Z80 alt
    { 0x9927, 0xd088, ADVENTURELAND_IFID }, // Adventureland Z80 alt 2
    { 0x6a10, 0x1910, ADVENTURELAND_IFID }, // Adventureland C64 (T64)
    { 0x6a10, 0x1b10, ADVENTURELAND_IFID }, // Adventureland C64 (T64) alt
   { 0x2ab00, 0x6638, ADVENTURELAND_IFID }, // Adventureland C64 (D64)
   { 0x2adab, 0x751f, ADVENTURELAND_IFID }, // Adventureland C64 (D64) alt
   { 0x2adab, 0x64a4, ADVENTURELAND_IFID }, // Adventureland C64 (D64) alt 2
   { 0x2adab, 0x8847, ADVENTURELAND_IFID }, // Adventureland C64 (D64) alt 3

    { 0x23000, 0x7e22, ADVENTURELAND_IFID }, // Adventureland Apple 2 (DSK)
    { 0x23000, 0x510b, ADVENTURELAND_IFID }, // Adventureland Apple 2 (DSK) Graphics disk
    { 0x23000, 0x81de, ADVENTURELAND_IFID }, // Adventureland Apple 2 (DSK) alt
    { 0x23000, 0xa915, ADVENTURELAND_IFID }, // Adventureland Apple 2 (DSK) alt graphics disk
    { 0x23000, 0x5750, ADVENTURELAND_IFID }, // Adventureland Apple 2 (DSK) v2.0-416 (4am crack)
    { 0x23000, 0xa989, ADVENTURELAND_IFID }, // Adventureland Apple 2 (DSK) v2.0-416 (4am crack) graphics disk
    { 0x39557, 0x3ff3, ADVENTURELAND_IFID }, // Adventureland Apple 2 (WOZ) Graphics disk
    { 0x39531, 0xa6a6, ADVENTURELAND_IFID }, // Adventureland Apple 2 (WOZ) alt
    { 0x39531, 0x7a7f, ADVENTURELAND_IFID }, // Adventureland Apple 2 (WOZ) alt graphics disk
    { 0x39557, 0x374e, ADVENTURELAND_IFID }, // Adventureland Apple 2 (WOZ) Boot

    { 0x16810, 0xa972, ADVENTURELAND_IFID }, // Adventureland v5.0-416 Atari 8-bit boot disk
    { 0x16810, 0x3bd4, ADVENTURELAND_IFID }, // Adventureland Atari 8-bit boot disk alt

    { 0x39567, 0x8aa4, PIRATE_ADVENTURE_IFID }, // Pirate Adventure v2.1-408 (WOZ) graphics disk
    { 0x39567, 0xcf60, PIRATE_ADVENTURE_IFID }, // Pirate Adventure v2.1-408 (WOZ) boot
    { 0x23000, 0x2615, PIRATE_ADVENTURE_IFID }, // Pirate Adventure v2.1-408 (4am crack) (DSK) graphics disk
    { 0x23000, 0x0530, PIRATE_ADVENTURE_IFID }, // Pirate Adventure v2.1-408 (4am crack) (DSK) boot

    { 0x16810, 0x65a1, PIRATE_ADVENTURE_IFID }, // Pirate Adventure v5.0-408 Atari 8 boot disk
    { 0x16810, 0x3074, PIRATE_ADVENTURE_IFID }, // Pirate Adventure v5.0-408 Atari 8 boot disk alt
    { 0x16810, 0x82c5, PIRATE_ADVENTURE_IFID }, // Pirate Adventure v5.0-408 Atari 8 boot disk alt 2

    { 0x2adab, 0x04c5, PIRATE_ADVENTURE_IFID }, // Pirate Adventure C64 S.A.G.A version

    { 0xbae1, 0x0ec0, SECRET_MISSION_IFID }, // Secret Mission.tzx
    { 0xbaa7, 0xfc85, SECRET_MISSION_IFID }, // Secret Mission TAP image
    { 0x8723, 0xc6da, SECRET_MISSION_IFID }, // Secret Mission Z80
    { 0x88be, 0xa122, SECRET_MISSION_IFID }, // Secret Mission  C64 (T64)
   { 0x2ab00, 0x04d6, SECRET_MISSION_IFID }, // Secret Mission  C64 (D64)
   { 0x2adab, 0x3ca3, SECRET_MISSION_IFID }, // Secret Mission  C64 (D64) alt

    { 0x23000, 0xc813, SECRET_MISSION_IFID }, // Secret Mission (DSK)
    { 0x23000, 0xa264, SECRET_MISSION_IFID }, // Secret Mission (DSK) alt
    { 0x39554, 0xbbd3, SECRET_MISSION_IFID }, // Mission Impossible v2.1-306 (WOZ) graphics disk
    { 0x39554, 0x361a, SECRET_MISSION_IFID }, // Mission Impossible v2.1-306 (WOZ) boot
    { 0x23000, 0x9f10, SECRET_MISSION_IFID }, // Mission Impossible v2.1-306 (4am crack)(DSK) graphics disk
    { 0x23000, 0x83e2, SECRET_MISSION_IFID }, // Mission Impossible v2.1-306 (4am crack)(DSK) boot

    { 0x16810, 0xae5d, SECRET_MISSION_IFID }, // Mission Impossible v5.0-306 Atari 8-bit boot

    { 0x38f00, 0xd6c2, THE_COUNT_IFID }, // The Count US (WOZ)
    { 0x39589, 0x7010, THE_COUNT_IFID }, // The Count US (WOZ) graphics disk
    { 0x39589, 0xbf0e, THE_COUNT_IFID }, // The Count US (WOZ) alt
    { 0x23000, 0x09f4, THE_COUNT_IFID }, // The Count US (DSK)
    { 0x23000, 0x0edf, THE_COUNT_IFID }, // The Count US (DSK) graphics disk
    { 0x23000, 0x0992, THE_COUNT_IFID }, // The Count US (DSK) alt

    { 0x16810, 0xc2f5, THE_COUNT_IFID }, // The Count US v5.1-115 Atari 8-bit boot disk
    { 0x16810, 0xa736, THE_COUNT_IFID }, // The Count US Atari 8-bit alt

    { 0x39558, 0xff6a, VOODOO_CASTLE_IFID }, // Voodoo Castle US (WOZ)
    { 0x39558, 0x958f, VOODOO_CASTLE_IFID }, // Voodoo Castle US (WOZ) graphics disk
    { 0x23000, 0xd585, VOODOO_CASTLE_IFID }, // Voodoo Castle US v2.1-119 (4am crack)(DSK) graphics disk
    { 0x23000, 0x823d, VOODOO_CASTLE_IFID }, // Voodoo Castle US v2.1-119 (4am crack)(DSK) boot
    { 0x23000, 0x81db, VOODOO_CASTLE_IFID }, // Voodoo Castle US (DSK) alt

    { 0x16810, 0x4389, VOODOO_CASTLE_IFID }, // Voodoo Castle US v5.1-119 Atari 8-bit boot disk
    { 0x16810, 0x578d, VOODOO_CASTLE_IFID }, // Voodoo Castle US Atari 8-bit boot disk alt
    { 0x2adab, 0xcb2b, VOODOO_CASTLE_IFID }, // Voodoo Castle US C64
    { 0x2ab00, 0x8969, VOODOO_CASTLE_IFID }, // Voodoo Castle US C64 packed
    { 0x2ab00, 0x2682, VOODOO_CASTLE_IFID }, // Voodoo Castle US C64 packed 2
    { 0x2ab00, 0xac79, VOODOO_CASTLE_IFID }, // Voodoo Castle US C64 packed 3 (toko)

    { 0x38f00, 0xf8eb, STRANGE_ODYSSEY_IFID }, // Strange Odyssey US (WOZ)
    { 0x39559, 0x4c17, STRANGE_ODYSSEY_IFID }, // Strange Odyssey US (WOZ) alt
    { 0x39559, 0xee5d, STRANGE_ODYSSEY_IFID }, // Strange Odyssey US (WOZ) graphics disk
    { 0x23000, 0xd700, STRANGE_ODYSSEY_IFID }, // Strange Odyssey US v2.1-119  (DSK)(4am crack)
    { 0x23000, 0xd8ca, STRANGE_ODYSSEY_IFID }, // Strange Odyssey US v2.1-119  (DSK)(4am crack) graphics

    { 0x16810, 0xdb2a, STRANGE_ODYSSEY_IFID }, // Strange Odyssey US v5.0-119 Atari 8-bit boot disk

    { 0xbae1, 0x83e9, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle.tzx
    { 0xbc2e, 0x4d84, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle - Alternate.tzx
    { 0xbaa7, 0x7371, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle TAP image
    { 0x726c, 0x0d57, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle Z80
    { 0x7298, 0x77fb, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle Z80 alt
    { 0x6ff7, 0xe4ed, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle C64 (T64)
    { 0x912f, 0xa69f, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle C64 (T64) alt
    { 0xc0dd, 0x3701, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle C64 (T64) alt 2
    { 0xbc5f, 0x492c, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle C64 (T64) alt 3
   { 0x2ab00, 0xfd67, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle C64 (D64)
   { 0x2ab00, 0x7ece, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle C64 (D64) alt
   { 0x2ab00, 0x0b54, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle C64 (D64) alt 2

   { 0x2adab, 0x1fac, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle US (D64)
   { 0x2ab00, 0xfbb6, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle US (D64)
   { 0x2ab00, 0xa957, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle US side A (D64)

   { 0x39567, 0x76be, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle US (WOZ)
   { 0x3af67, 0x9843, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle US (WOZ) graphics disk
   { 0x23000, 0xbd9b, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle US (DSK)
   { 0x23000, 0xd27a, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle US (DSK) alt
   { 0x23000, 0xdae0, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle US (DSK) alt Graphics disk
   { 0x23000, 0xbd39, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle US (DSK) alt 2

    { 0x16810, 0x7c32, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle Atari 8-bit (boot disk)
    { 0x16810, 0xac42, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle Atari 8-bit v5.1-125 (boot disk)
    { 0x16810, 0x28d7, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Sorcerer Of Claymorgue Castle Atari 8-bit v5.1-125 (boot disk)

    { 0xb36e, 0xbe5d, QUESTPROBE_SPIDERMAN_IFID }, // Questprobe 2 - Spiderman.tzx
    { 0xb280, 0x196d, QUESTPROBE_SPIDERMAN_IFID }, // Questprobe 2 - Spiderman - Alternate.tzx
    { 0xb36e, 0xbf88, QUESTPROBE_SPIDERMAN_IFID }, // Questprobe 2 - Spiderman (Americana).tzx
    { 0xa4c4, 0x1ae8, QUESTPROBE_SPIDERMAN_IFID }, // Spiderman Z80
    { 0x9575, 0x86ae, QUESTPROBE_SPIDERMAN_IFID }, // Spiderman Z80 alt
    { 0x9575, 0x8b6e, QUESTPROBE_SPIDERMAN_IFID }, // Spiderman Z80 alt 2
    { 0xb316, 0x3c3c, QUESTPROBE_SPIDERMAN_IFID }, // Spiderman TAP image
   { 0x08e72, 0xb2f4, QUESTPROBE_SPIDERMAN_IFID }, // Spiderman C64 (T64)
   { 0x2ab00, 0xde56, QUESTPROBE_SPIDERMAN_IFID }, // Spiderman C64 (D64)
   { 0x2ab00, 0x2736, QUESTPROBE_SPIDERMAN_IFID }, // Spiderman C64 (D64) alt
   { 0x2ab00, 0x490a, QUESTPROBE_SPIDERMAN_IFID }, // Spiderman C64 (D64) alt 2
   { 0x2ab00, 0xc4c4, QUESTPROBE_SPIDERMAN_IFID }, // Spiderman C64 (D64) alt 3

    { 0x9e51, 0x8be7, SAVAGE_ISLAND_PART_I_IFID }, // Savage Island Part 1.tzx
    { 0x9e46, 0x7792, SAVAGE_ISLAND_PART_I_IFID }, // Savage Island Part 1 - Alternate.tzx
   { 0x11886, 0xd588, SAVAGE_ISLAND_PART_I_IFID }, // Savage Island.tzx (Contains both parts, but ScottFree will only play the first)
    { 0x9d6a, 0x41d6, SAVAGE_ISLAND_PART_I_IFID }, // Savage Island TAP image
    { 0x9ab2, 0xfaaa, SAVAGE_ISLAND_PART_I_IFID }, // Savage Island Z80 image
    { 0x9a4c, 0xf104, SAVAGE_ISLAND_PART_I_IFID }, // Savage Island Part 1 Z80 image
   { 0x2ab00, 0xc361, SAVAGE_ISLAND_PART_I_IFID }, // Savage Island C64 (D64)
   { 0x2ab00, 0x8801, SAVAGE_ISLAND_PART_I_IFID }, // Savage Island C64 (D64) alt

    { 0x9d9e, 0x4d76, SAVAGE_ISLAND_PART_II_IFID }, // Savage Island Part 2.tzx
    { 0x9e59, 0x79f4, SAVAGE_ISLAND_PART_II_IFID }, // Savage Island Part 2 - Alternate.tzx
    { 0x9d6a, 0x3de4, SAVAGE_ISLAND_PART_II_IFID }, // Savage Island 2 TAP image

    { 0xbae8, 0x6d7a, GREMLINS_IFID }, // Gremlins - The Adventure.tzx
    { 0xbc36, 0x47cd, GREMLINS_IFID }, // Gremlins The Adventure - Alternate.tzx
    { 0xbacc, 0x69fa, GREMLINS_IFID }, // Gremlins TAP image
    { 0xb114, 0x8a72, GREMLINS_IFID }, // Gremlins z80 image
    { 0x9e5d, 0x86f5, GREMLINS_IFID }, // Gremlins z80 alt
    { 0xbbb6, 0x54cb, GREMLINS_GERMAN_IFID }, // Gremlins - The Adventure (German).tzx
    { 0xc003, 0x558c, GREMLINS_GERMAN_IFID }, // German Gremlins C64 (T64) version
    { 0xdd94, 0x25a8, GREMLINS_GERMAN_IFID }, // German Gremlins C64 (T64) version alt
   { 0x2ab00, 0x6729, GREMLINS_GERMAN_IFID }, // German Gremlins C64 (D64) version
    { 0xbb63, 0x3cc4, GREMLINS_SPANISH_IFID }, // Gremlins - The Adventure (Spanish).tzx
    { 0xbb63, 0x3ec7, GREMLINS_SPANISH_IFID }, // Gremlins (Spanish) alt.tzx
    { 0x9ece, 0x70cc, GREMLINS_SPANISH_IFID }, // Gremlins (Spanish) z80
    { 0xbab4, 0x05a6, GREMLINS_SPANISH_IFID }, // Gremlins Spanish TAP image
   { 0x2ab00, 0x2ad9, GREMLINS_SPANISH_IFID }, // Gremlins Spanish C64 (D64)
   { 0x2ab00, 0xc402, GREMLINS_IFID }, // Gremlins C64 (D64) version
   { 0x2ab00, 0xabf8, GREMLINS_IFID }, // Gremlins C64 (D64) version alt
   { 0x2ab00, 0xa265, GREMLINS_IFID }, // Gremlins C64 (D64) version alt 2
   { 0x2ab00, 0x3ccf, GREMLINS_IFID }, // Gremlins C64 (D64) version alt 3
   { 0x2ab00, 0xa1dc, GREMLINS_IFID }, // Gremlins C64 (D64) version alt 4
   { 0x2ab00, 0x4be2, GREMLINS_IFID }, // Gremlins C64 (D64) version alt 6
   { 0x2ab00, 0x5c7a, GREMLINS_IFID }, // Gremlins C64 (D64) version alt 7
   { 0x2ab00, 0x331a, GREMLINS_IFID }, // Gremlins C64 (D64) version alt 8

    { 0xba60, 0x3734, SUPERGRAN_IFID }, // Super Gran - The Adventure.tzx
    { 0xba71, 0x2d56, SUPERGRAN_IFID }, // Super Gran - Alternate.tzx
    { 0xba71, 0x2dc3, SUPERGRAN_IFID }, // Super Gran - Alternate 2.tzx
    { 0x9df0, 0x2238, SUPERGRAN_IFID }, // Super Gran z80 snap
    { 0x726f, 0x0901, SUPERGRAN_IFID }, // Super Gran C64 (T64)
   { 0x2ab00, 0x538c, SUPERGRAN_IFID }, // Super Gran C64 (D64)

    { 0xc581, 0x940f, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood.tzx
    { 0xc581, 0x940f, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood - Alternate.tzx
    { 0xae0c, 0x4661, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood TAP image
    { 0xc495, 0x4d3c, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood TAP image alt
    { 0xace8, 0x991c, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood z80 snap
    { 0xa202, 0x8b8b, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood z80 alt
   { 0x2ab00, 0xcf9e, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood C64 (D64)
   { 0x2ab00, 0xc0c7, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood C64 (D64) alt
   { 0x2ab00, 0xc6e6, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood C64 (D64) alt 2

    { 0xb2ef, 0x7c44, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood C64 (T64)
    { 0x8db6, 0x7853, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood C64 (T64) alt
    { 0xb690, 0x7b61, ROBIN_OF_SHERWOOD_IFID }, // Robin Of Sherwood C64 (T64) alt 2

    { 0xbc60, 0xce3d, SEAS_OF_BLOOD_IFID }, // Seas Of Blood.tzx
    { 0xbd61, 0x0277, SEAS_OF_BLOOD_IFID }, // Seas Of Blood - Alternate.tzx
    { 0xbc26, 0xbe47, SEAS_OF_BLOOD_IFID }, // Seas of Blood TAP image
    { 0xb4b1, 0x7ec8, SEAS_OF_BLOOD_IFID }, // Seas of Blood z80
    { 0xb3b8, 0x9ab5, SEAS_OF_BLOOD_IFID }, // Seas of Blood z80 alt
    { 0xa09a, 0x5930, SEAS_OF_BLOOD_IFID }, // Seas of Blood z80 alt 2
    { 0xa209, 0xf115, SEAS_OF_BLOOD_IFID }, // Seas of Blood C64 (T64)
   { 0x2ab00, 0x5c1d, SEAS_OF_BLOOD_IFID }, // Seas of Blood C64 (D64)
   { 0x2ab00, 0xe308, SEAS_OF_BLOOD_IFID }, // Seas of Blood C64 (D64) alt
   { 0x2ab00, 0x3df2, SEAS_OF_BLOOD_IFID }, // Seas of Blood C64 (D64) alt 2

   { 0, 0, 0 }
};

static const struct scottrec TI994A_registry[] = {
    { 0xff, 0x8618, ADVENTURELAND_IFID }, // Adventureland, TI-99/4A version
    { 0xff, 0x9333, PIRATE_ADVENTURE_IFID }, // Pirate Adventure, TI-99/4A version
    { 0xff, 0x9f67, SECRET_MISSION_IFID }, // Secret Mission, TI-99/4A version
    { 0xff, 0x96b7, VOODOO_CASTLE_IFID }, // Voodoo Castle, TI-99/4A version
    { 0xff, 0x9095, THE_COUNT_IFID }, // The Count, TI-99/4A version
    { 0xff, 0x9ae3, STRANGE_ODYSSEY_IFID }, // Strange Odyssey, TI-99/4A version
    { 0xff, 0x90ce, MYSTERY_FUNHOUSE_IFID }, // Mystery Fun House, TI-99/4A version
    { 0xff, 0x7d03, PYRAMID_OF_DOOM_IFID }, // Pyramid of Doom, TI-99/4A version
    { 0xff, 0x8dde, GHOST_TOWN_IFID }, // Ghost Town, TI-99/4A version
    { 0xff, 0x9bc6, SAVAGE_ISLAND_PART_I_IFID }, // Savage Island part I, TI-99/4A version
    { 0xff, 0xa6d2, SAVAGE_ISLAND_PART_II_IFID }, // Savage Island part II, TI-99/4A version
    { 0xff, 0x7f47, GOLDEN_VOYAGE_IFID }, // The Golden Voyage, TI-99/4A version
    { 0xff, 0x882d, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // The Sorcerer Of Claymorgue Castle, TI-99/4A version

    { 0xff, 0x80e9, BUCKAROO_BANZAI_IFID }, // The Adventures of Buckaroo Banzai, TI-99/4A version

    { 0xff, 0x80c0, QUESTPROBE_HULK_IFID }, // Questprobe 1 - The Hulk, TI-99/4A version
    { 0xff, 0x884b, QUESTPROBE_SPIDERMAN_IFID }, // Questprobe 2 - Spider-Man, TI-99/4A version

    { 0, 0, 0 }
};

static uint16_t checksum(unsigned char *sf, int32 extent)
{
    uint16_t c = 0;
    for (int i = 0; i < extent; i++)
        c+=sf[i];
    return c;
}


static int32 find_in_database(unsigned char *sf, int32 extent, char **ifid) {
    if (extent > MAX_LENGTH || extent < MIN_LENGTH)
        return INVALID_STORY_FILE_RV;

    int calculated_checksum = 0;
    uint16_t chksum;

    for (int i = 0; scott_registry[i].length != 0; i++) {
        if (extent == scott_registry[i].length) {
            if (!calculated_checksum) {
                chksum = checksum(sf, extent);
                calculated_checksum  = 1;
            }
            if (chksum == scott_registry[i].chk) {
                if (ifid != NULL) {
                    size_t length = strlen(ifids[scott_registry[i].ifid]);
                    strncpy(*ifid, ifids[scott_registry[i].ifid], length);
                    (*ifid)[length] = 0;
                }
                return VALID_STORY_FILE_RV;
            }
        }
    }
    return INVALID_STORY_FILE_RV;
}

static int32 find_in_TI994Adatabase(uint16_t chksum, char **ifid) {

    for (int i = 0; TI994A_registry[i].length != 0; i++) {
        if (chksum == TI994A_registry[i].chk) {
            if (ifid != NULL) {
                strncpy(*ifid, ifids[TI994A_registry[i].ifid], 32);
                (*ifid)[32] = '\0';
            }
            return VALID_STORY_FILE_RV;
        }
    }
    return INVALID_STORY_FILE_RV;
}

static int find_code(char *x, int codelen, unsigned char *sf, int32 extent) {
    if (codelen >= extent)
        return -1;
    unsigned const char *p = sf;
    while (p < sf + extent - codelen) {
        if (memcmp(p, x, codelen) == 0) {
            return p - sf;
        }
        p++;
    }
    return -1;
}

static int detect_ti994a(unsigned char *sf, int32 extent) {
    return find_code("\x30\x30\x30\x30\x00\x30\x30\x00\x28\x28", 10, sf, extent);
}

static int SanityCheckHeader(int ni, int na, int nw, int nr)
{
    if (ni < 10 || ni > 500)
        return 0;
    if (na < 100 || na > 500)
        return 0;
    if (nw < 50 || nw > 190)
        return 0;
    if (nr < 10 || nr > 100)
        return 0;
    return 1;
}

static int header[15];

static uint8_t *ReadHeader(uint8_t *ptr)
{
    int i, value;
    for (i = 0; i < 15; i++) {
        value = *ptr + 256 * *(ptr + 1);
        header[i] = value;
        ptr += 2;
    }
    return ptr - 1;
}

static int detect_atari(unsigned char *sf, int32 extent) {
    if (extent == 0x16810 && find_code("\x96\x02\x80\x16\x80\x00", 6, sf, 7) != -1) {
        ReadHeader(&sf[0x04f9]);
        int ni = header[3];
        int na = header[2];
        int nw = header[1];
        int nr = header[5];
        return SanityCheckHeader(ni, na, nw, nr);
    }
    return 0;
}

/* All numbers in ScottFree text format files are stored as text delimited by whitespace */
static int read_next_number(unsigned char *text, int32_t extent, int32_t *offset, bool *failure) {
    char numstring[100];
    int i;
    bool number_found = false;
    for (i = 0; i < extent - *offset && i < 99; i++) {
        char c = text[*offset + i];
        numstring[i] = c;
        if (isspace(c)) {
            if (number_found == true)
                break;
        } else if (isdigit(c) || c == '-') {
            number_found = true;
        } else {
            *failure = true;
            return 0;
        }
    }

    if (number_found == false) {
        *failure = true;
        return 0;
    }
    numstring[i+1] = '\0';
    *offset += i;
    int result = atoi(numstring);
    if (result > INT16_MAX || result < INT16_MIN)
        *failure = true;

    return result;
}

static int read_string(unsigned char *text, int32_t extent, int32_t *offset, bool *failure)
{
    int c,nc;
    int ct=0;
    do {
        c=text[(*offset)++];
    } while(*offset < extent && isspace(c));
    
    if(c!='"') {
        *failure = true;
        return 0;
    }
    do {
        c=text[(*offset)++];
        if(*offset >= extent) {
            *failure = true;
            return 0;
        }
        if(c=='"') {
            nc=text[(*offset)++];
            if(nc!='"') {
                (*offset)--;
                break;
            }
        }
        if (!isprint(c)) {
            *failure = true;
            return 0;
        }

       ct++;

    } while(*offset < extent);
    return ct;
}

static int32 detect_scottfree(unsigned char *storystring, int32 extent) {
    /* Load the header */

    int32 offset = 0;
    int header[12];
    bool failure = false;

    /* We simply run a sped-up version of the first three parts of reading the database
     * from the file, and bail at the first sign of failure.
     */

    /* First the 12 integers of the header */
    for (int i = 0; i < 12; i++) {
        header[i] = read_next_number(storystring, extent, &offset, &failure);
        if (failure == true)
            return INVALID_STORY_FILE_RV;
    }

    /* Then the number of actions given in header[2] + 1 */
    for (int i = 0; i <= header[2]; i++) {
        /* 8 integers per action */
        for (int j = 0; j < 8; j++) {
            read_next_number(storystring, extent, &offset, &failure);
            if (failure == true)
                return INVALID_STORY_FILE_RV;
        }
    }

    /* Finally the number of dictionary words given in header[3] + 1 */
    for (int i = 0; i <= header[3]; i++) {
        for (int j = 0; j < 2; j++) {
            /* The word length is given in header[8], but many games have words exceeding this */
            /* in the dictionary. We use 10 characters as a reasonable upper limit */
            if (read_string(storystring, extent, &offset, &failure) > 10 || failure == true)
                return INVALID_STORY_FILE_RV;
        }
    }

    return VALID_STORY_FILE_RV;
}

static int32 claim_story_file(void *storyvp, int32 extent)
{
    unsigned char *storystring = (unsigned char *)storyvp;

#ifdef DEBUG
    fprintf(stderr, "The length of this file is %x, and its checksum %x\n", extent, checksum(storystring, extent));
#endif

    if (extent < 24 || extent > 300000)
        return INVALID_STORY_FILE_RV;

    if (detect_atari(storystring, extent))
        return VALID_STORY_FILE_RV;

    if (detect_scottfree(storystring, extent) == VALID_STORY_FILE_RV)
        return VALID_STORY_FILE_RV;

    if (detect_ti994a(storystring, extent) != -1)
        return VALID_STORY_FILE_RV;

    if (find_in_database(storystring, extent, NULL) == VALID_STORY_FILE_RV)
        return VALID_STORY_FILE_RV;

    return INVALID_STORY_FILE_RV;
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
    ASSERT_OUTPUT_SIZE(37);

    unsigned char *storystring = (unsigned char *)storyvp;

    if (detect_atari(storystring, extent)) {
        int index = storystring[0x04e7];
        if (index > 13)
            return INVALID_STORY_FILE_RV;
        if (index == 1) {
            int version = storystring[0x04e3] + storystring[0x04e4] * 0x100;
            if (version == 127)
                index = 14;
        }
        const char *ifid = ifids[index];
        if (ifid != NULL) {
            int len = strlen(ifid);
            strncpy(output, ifid, len);
            output[len] = '\0';
            return VALID_STORY_FILE_RV;
        }

    }
    int ti99offset = detect_ti994a(storystring, extent);

    if (ti99offset != -1) {
        int title_screen_offset = ti99offset - 0x509;
        title_screen_offset = MAX(title_screen_offset, 0);
        /* Calculate checksum using 3 Kb after the title screen */
        int checksum_length = MIN(3072, extent - title_screen_offset);

        int sum = checksum(storyvp + title_screen_offset, checksum_length);
        if (find_in_TI994Adatabase(sum, &output) == VALID_STORY_FILE_RV) {
            return VALID_STORY_FILE_RV;
        }
    }

    if (find_in_database(storystring, extent, &output) == VALID_STORY_FILE_RV) {
        size_t length = strlen(output);
        for (int i = 0; i< length; i++)
            output[i] = toupper(output[i]);
        output[length] = 0;
        return VALID_STORY_FILE_RV;
    }

    if (detect_scottfree(storystring, extent) == VALID_STORY_FILE_RV
     || ti99offset != -1)
    {
        strcpy(output, "\0");
        return INCOMPLETE_REPLY_RV;
    }
    return INVALID_STORY_FILE_RV;
}
