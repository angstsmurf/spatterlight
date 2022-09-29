//
// nib2dsk.c - convert Apple II NIB image file into DSK file
// Copyright (C) 1996, 2017 slotek@nym.hush.com
//
// Modified for Spatterlight by Petter Sjölund

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/********** Symbolic Constants **********/

#define VERSION_MAJOR       2
#define VERSION_MINOR       0

#define TRACKS_PER_DISK     35
#define SECTORS_PER_TRACK   16
#define BYTES_PER_SECTOR    256
#define BYTES_PER_TRACK     4096
#define DSK_LEN             143360L

#define PRIMARY_BUF_LEN     256
#define SECONDARY_BUF_LEN   86
#define DATA_LEN            ( PRIMARY_BUF_LEN + SECONDARY_BUF_LEN )

/********** Statics **********/
static const char *ueof = "Unexpected End of Data";
static const uint8_t addr_prolog[] = { 0xd5, 0xaa, 0x96 };
static const uint8_t addr_epilog[] = { 0xde, 0xaa, 0xeb };
static const uint8_t data_prolog[] = { 0xd5, 0xaa, 0xad };
static const uint8_t data_epilog[] = { 0xde, 0xaa, 0xeb };
static const int interleave[ SECTORS_PER_TRACK ] =
    { 0, 7, 0xE, 6, 0xD, 5, 0xC, 4, 0xB, 3, 0xA, 2, 9, 1, 8, 0xF };

/********** Globals **********/
static uint8_t sector, track, volume;
static uint8_t primary_buf[ PRIMARY_BUF_LEN ];
static uint8_t secondary_buf[ SECONDARY_BUF_LEN ];
static uint8_t *dsk_buf[ TRACKS_PER_DISK ];

/********** Prototypes **********/
static void convert_image( void );
static void process_data( uint8_t byte);
static uint8_t odd_even_decode( uint8_t byte1, uint8_t byte2 );
static uint8_t untranslate( uint8_t x );
static int get_byte( uint8_t *byte );
static void dsk_init( void );
static void dsk_reset( void );
static void myprintf( char *format, ... );
static void fatalerror( const char *format, ... );

static uint8_t *origdata = NULL;

static size_t offs = 0;
static size_t datasize = 0;

uint8_t *ReadFromFile(const char *name, size_t *size);

uint8_t *nib2dsk(uint8_t *data, size_t *origsize)
{
    datasize = *origsize;
    origdata = data;

    myprintf( "Apple II NIB to DSK Image Converter Version %d.%d\n\n",
        VERSION_MAJOR, VERSION_MINOR );

    // Init dsk_buf and open files
    //
    dsk_init();

    //
    // Do conversion and write DSK file
    //
    convert_image();

    size_t finalsize = TRACKS_PER_DISK * BYTES_PER_TRACK;
    uint8_t *result = malloc(finalsize);
    *origsize = finalsize;

    int i;
    size_t pos = 0;
    for ( i = 0; i < TRACKS_PER_DISK; i++ ) {
        memcpy(result + pos, dsk_buf[i], BYTES_PER_TRACK);
        pos += BYTES_PER_TRACK;
    }

    //
    // Close files & free dsk

    dsk_reset();

    return result;
}

//
// Convert NIB image into DSK image
//
#define STATE_INIT  0
#define STATE_DONE  666
static void convert_image( void )
{
    int state;
    int addr_prolog_index = 0, addr_epilog_index = 0;
    int data_prolog_index = 0, data_epilog_index = 0;
    uint8_t byte = 0;

    //
    // Image conversion FSM
    //
    if ( get_byte( &byte ) == 0 )
        fatalerror( ueof );

    for ( state = STATE_INIT; state != STATE_DONE; ) {

        switch( state ) {

            //
            // Scan for 1st addr prolog byte (skip gap bytes)
            //
            case 0:
                addr_prolog_index = 0;
                if ( byte == addr_prolog[ addr_prolog_index ] ) {
                    ++addr_prolog_index;
                    ++state;
                }
                if ( get_byte( &byte ) == 0 )
                    state = STATE_DONE;
                break;

            //
            // Accept 2nd and 3rd addr prolog bytes
            //
            case 1:
            case 2:
                if ( byte == addr_prolog[ addr_prolog_index ] ) {
                    ++addr_prolog_index;
                    ++state;
                    if ( get_byte( &byte ) == 0 )
                        fatalerror( ueof );
                } else
                    state = STATE_INIT;
                break;

            //
            // Read and decode volume number
            //
            case 3:
            {
                uint8_t byte2 = 0;
                if ( get_byte( &byte2 ) == 0 )
                    fatalerror( ueof );
                volume = odd_even_decode( byte, byte2 );
                myprintf( "V:%02x ", volume );
                myprintf( "{%02x%02x} ", byte, byte2 );
                ++state;
                if ( get_byte( &byte ) == 0 )
                    fatalerror( ueof );
                break;
            }

            //
            // Read and decode track number
            //
            case 4:
            {
                uint8_t byte2;
                if ( get_byte( &byte2 ) == 0 )
                    fatalerror( ueof );
                track = odd_even_decode( byte, byte2 );
                myprintf( "T:%02x ", track );
                myprintf( "{%02x%02x} ", byte, byte2 );
                ++state;
                if ( get_byte( &byte ) == 0 )
                    fatalerror( ueof );
                break;
            }

            //
            // Read and decode sector number
            //
            case 5:
            {
                uint8_t byte2;
                if ( get_byte( &byte2 ) == 0 )
                    fatalerror( ueof );
                sector = odd_even_decode( byte, byte2 );
                myprintf( "S:%02x ", sector );
                myprintf( "{%02x%02x} ", byte, byte2 );
                ++state;
                if ( get_byte( &byte ) == 0 )
                    fatalerror( ueof );
                break;
            }

            //
            // Read and decode addr field checksum
            //
            case 6:
            {
                uint8_t byte2, csum;
                if ( get_byte( &byte2 ) == 0 )
                    fatalerror( ueof );
                csum = odd_even_decode( byte, byte2 );
                myprintf( "C:%02x ", csum );
                myprintf( "{%02x%02x} - ", byte, byte2 );
                ++state;
                if ( get_byte( &byte ) == 0 )
                    fatalerror( ueof );
                break;
            }

            //
            // Accept 1st addr epilog byte
            //
            case 7:
                addr_epilog_index = 0;
                if ( byte == addr_epilog[ addr_epilog_index ] ) {
                    ++addr_epilog_index;
                    ++state;
                    if ( get_byte( &byte ) == 0 )
                        fatalerror( ueof );
                } else {
                    myprintf( "Reset!\n" );
                    state = STATE_INIT;
                }
                break;

            //
            // Accept 2nd addr epilog byte
            //
            case 8:
                if ( byte == addr_epilog[ addr_epilog_index ] ) {
                    ++state;
                    if ( get_byte( &byte ) == 0 )
                        fatalerror( ueof );
                } else {
                    myprintf( "Reset!\n" );
                    state = STATE_INIT;
                }
                break;

            //
            // Scan for 1st data prolog byte (skip gap bytes)
            //
            case 9:
                data_prolog_index = 0;
                if ( byte == data_prolog[ data_prolog_index ] ) {
                    ++data_prolog_index;
                    ++state;
                }
                if ( get_byte( &byte ) == 0 )
                    fatalerror( ueof );
                break;

            //
            // Accept 2nd and 3rd data prolog bytes
            //
            case 10:
            case 11:
                if ( byte == data_prolog[ data_prolog_index ] ) {
                    ++data_prolog_index;
                    ++state;
                    if ( get_byte( &byte ) == 0 )
                        fatalerror( ueof );
                } else
                    state = 9;
                break;

            //
            // Process data
            //
            case 12:
                process_data( byte );
                myprintf( "OK!\n" );
                ++state;
                if ( get_byte( &byte ) == 0 )
                    fatalerror( ueof );
                break;

            //
            // Scan(!) for 1st data epilog byte
            //
            case 13:
            {
                static int extra = 0;
                data_epilog_index = 0;
                if ( byte == data_epilog[ data_epilog_index ] ) {
                    if ( extra ) {
                        myprintf( "Warning: %d extra bytes before data epilog\n",
                            extra );
                        extra = 0;
                    }
                    ++data_epilog_index;
                    ++state;
                } else
                    ++extra;
                if ( get_byte( &byte ) == 0 )
                    fatalerror( ueof );
                break;
            }

            //
            // Accept 2nd data epilog byte
            //
            case 14:
                if ( byte == data_epilog[ data_epilog_index ] ) {
                    ++data_epilog_index;
                    ++state;
                    if ( get_byte( &byte ) == 0 )
                        fatalerror( ueof );
                } else {
                    // fatal( "data epilog mismatch (%02x)\n", byte );
                    myprintf( "data epilog mismatch (%02x)\n", byte );
                    state = STATE_INIT;
                }
                break;

            //
            // Accept 3rd data epilog byte
            //
            case 15:
                if ( byte == data_epilog[ data_epilog_index ] ) {
                    if ( get_byte( &byte ) == 0 )
                        state = STATE_DONE;
                    else
                        state = STATE_INIT;
                } else {
                    //  fatal( "data epilog mismatch (%02x)\n", byte );
                    myprintf( "data epilog mismatch (%02x)\n", byte );
                    state = STATE_INIT;
                }
                break;

            default:
                fatalerror( "Undefined state!" );
                break;
        }
    }
}

//
// Convert 343 6+2 encoded bytes into 256 data bytes and 1 checksum
//
static void process_data(uint8_t byte)
{
    int i, sec;
    uint8_t checksum;
    uint8_t bit0 = 0, bit1 = 0;

    //
    // Fill primary and secondary buffers according to iterative formula:
    //    buf[0] = trans(byte[0])
    //    buf[1] = trans(byte[1]) ^ buf[0]
    //    buf[n] = trans(byte[n]) ^ buf[n-1]
    //
    checksum = untranslate( byte );
    secondary_buf[ 0 ] = checksum;

    for ( i = 1; i < SECONDARY_BUF_LEN; i++ ) {
        if ( get_byte( &byte ) == 0 )
            fatalerror( ueof );
        checksum ^= untranslate( byte );
        secondary_buf[ i ] = checksum;
    }

    for ( i = 0; i < PRIMARY_BUF_LEN; i++ ) {
        if ( get_byte( &byte ) == 0 )
            fatalerror( ueof );
        checksum ^= untranslate( byte );
        primary_buf[ i ] = checksum;
    }

    //
    // Validate resultant checksum
    //
    if ( get_byte( &byte ) == 0 )
        fatalerror( ueof );
    checksum ^= untranslate( byte );
    if ( checksum != 0 )
        myprintf( "Warning: data checksum mismatch\n" );

    //
    // Denibbilize
    //
    for ( i = 0; i < PRIMARY_BUF_LEN; i++ ) {
        int index = i % SECONDARY_BUF_LEN;

        switch( i / SECONDARY_BUF_LEN ) {
            case 0:
                bit0 = ( secondary_buf[ index ] & 2 ) > 0;
                bit1 = ( secondary_buf[ index ] & 1 ) > 0;
                break;
            case 1:
                bit0 = ( secondary_buf[ index ] & 8 ) > 0;
                bit1 = ( secondary_buf[ index ] & 4 ) > 0;
                break;
            case 2:
                bit0 = ( secondary_buf[ index ] & 0x20 ) > 0;
                bit1 = ( secondary_buf[ index ] & 0x10 ) > 0;
                break;
            default:
                fatalerror( "huh?" );
                break;
        }

        sec = interleave[ sector ];

        *( dsk_buf[ track ] + (sec*BYTES_PER_SECTOR) + i )
            = ( primary_buf[ i ] << 2 ) | ( bit1 << 1 ) | bit0;
    }
}

//
// decode 2 "4 and 4" bytes into 1 byte
//
static uint8_t odd_even_decode( uint8_t byte1, uint8_t byte2 )
{
    uint8_t byte;

    byte = ( byte1 << 1 ) & 0xaa;
    byte |= byte2 & 0x55;

    return byte;
}

//
// do "6 and 2" un-translation
//
#define TABLE_SIZE 0x40
static uint8_t table[ TABLE_SIZE ] = {
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
static uint8_t untranslate( uint8_t x )
{
    uint8_t *ptr;
    int index;

    if ( ( ptr = memchr( table, x, TABLE_SIZE ) ) == NULL )
        fatalerror( "Non-translatable byte %02x\n", x );

    index = (int)(ptr - table);

    return index;
}

//
// Read byte from input file
// Returns 0 on EOF
//
//HACK #define BUFLEN 16384
//static uint8_t buf[ BUFLEN ];

static int get_byte( uint8_t *byte )
{
//    myprintf("(%d)", readpos);

    if ( offs >= datasize ) {
            return 0;
    }

    *byte = origdata[ offs++ ];
    return 1;
}

//
// Alloc dsk_buf
//
static void dsk_init( void )
{
    offs = 0;
    int i;
    for ( i = 0; i < TRACKS_PER_DISK; i++ )
        if ( ( dsk_buf[ i ] = (uint8_t *) malloc( BYTES_PER_TRACK ) ) == NULL )
            fatalerror( "cannot allocate %ld bytes", DSK_LEN );
}

//
// Free dsk_buf
//
static void dsk_reset( void )
{
    int i;
    for ( i = 0; i < TRACKS_PER_DISK; i++ )
        free( dsk_buf[ i ] );
}

//
// myprintf
//
#pragma argsused
static void myprintf( char *format, ... )
{
#ifdef DEBUG
    va_list argp;
    va_start( argp, format );
    vfprintf( stderr, format, argp );
    va_end( argp );
#endif
}

//
// fatal
//
static void fatalerror( const char *format, ... )
{
    va_list argp;

    myprintf( "\nFatal: " );

    va_start( argp, format );
    vfprintf( stderr, format, argp );
    va_end( argp );

    myprintf( "\n" );

    exit( 1 );
}
