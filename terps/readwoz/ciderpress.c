/*
    This is based on parts of the Ciderpress source.
    See https://github.com/fadden/ciderpress for the full version.

    Includes fragments from a2tools by Terry Kyriacopoulos and Paul Schlyter
*/

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "debugprint.h"

//#define NIB_VERBOSE_DEBUG

#define MIN(a, b) ((a) < (b) ? (a) : (b))

enum {
    FILETYPE_T = 0x00,
    FILETYPE_I = 0x01,
    FILETYPE_A = 0x02,
    FILETYPE_B = 0x04,
    FILETYPE_S = 0x08,
    FILETYPE_R = 0x10,
    FILETYPE_X = 0x20, /* "new A" */
    FILETYPE_Y = 0x40, /* "new B" */
    FILETYPE_UNKNOWN = 0x100
};

/*
 * ===========================================================================
 *      DiskFSDOS33
 * ===========================================================================
 */

// clang-format off

/* do we need a way to override these? */
static const int kVTOCTrack = 17;
static const int kVTOCSector = 0;
static const int kCatalogEntryOffset = 0x0b;   // first entry in cat sect starts here
static const int kCatalogEntrySize = 0x23;     // length in bytes of catalog entries
static const int kCatalogEntriesPerSect = 7;   // #of entries per catalog sector
static const int kEntryDeleted = 0xff;         // this is used for track# of deleted files
static const int kEntryUnused = 0x00;          // this is track# in never-used entries
static const int kMaxTSPairs = 0x7a;           // 122 entries for 256-byte sectors
static const int kTSOffset = 0x0c;             // first T/S entry in a T/S list

// clang-format on

static const int kMaxTSIterations = 32;
#define kFileNameBufLen 31

#define kMaxCatalogSectors 64 // two tracks on a 32-sector disk

static size_t fLength = 143360;

#define kNumTracks 35
#define kNumSectPerTrack 16 // Is this ever 13?
#define kSectorSize 256

#define kChunkSize62 86 // (0x56)

#define kMaxNibbleTracks525 40 // max #of tracks on 5.25 nibble img
#define kTrackAllocSize 6656 // max 5.25 nibble track len; for buffers

/*
 * ==========================================================================
 *      Block/track/sector I/O
 * ==========================================================================
 */

/* a T/S pair */
typedef struct TrackSector {
    uint8_t track;
    uint8_t sector;
} TrackSector;

TrackSector fCatalogSectors[kMaxCatalogSectors];

/* assorted constants */
enum {
    kMaxFileName = 30,
    kFssep = ':',
    kInvalidBlockNum = 1,       // boot block, can't be in file
    kMaxBlocksPerIndex = 256,
};

// clang-format off

/*
 * Errors from the various disk image classes.
 */
typedef enum DIError {
    kDIErrNone = 0,

    /* I/O request errors (should renumber/rename to match GS/OS errors?) */
    kDIErrAccessDenied          = -10,
    kDIErrVWAccessForbidden     = -11,  // write access to volume forbidden
    kDIErrSharingViolation      = -12,  // file is in use and not shareable
    kDIErrNoExclusiveAccess     = -13,  // couldn't get exclusive access
    kDIErrWriteProtected        = -14,  // disk is write protected
    kDIErrCDROMNotSupported     = -15,  // access to CD-ROM drives not supptd
    kDIErrASPIFailure           = -16,  // generic ASPI failure result
    kDIErrSPTIFailure           = -17,  // generic SPTI failure result
    kDIErrSCSIFailure           = -18,  // generic SCSI failure result
    kDIErrDeviceNotReady        = -19,  // floppy or CD-ROM drive has no media

    kDIErrFileNotFound          = -20,
    kDIErrForkNotFound          = -21,  // requested fork does not exist
    kDIErrAlreadyOpen           = -22,  // already open, can't open a 2nd time
    kDIErrFileOpen              = -23,  // file is open, can't delete it
    kDIErrNotReady              = -24,
    kDIErrFileExists            = -25,  // file already exists
    kDIErrDirectoryExists       = -26,  // directory already exists

    kDIErrEOF                   = -30,  // end-of-file reached
    kDIErrReadFailed            = -31,
    kDIErrWriteFailed           = -32,
    kDIErrDataUnderrun          = -33,  // tried to read off end of the image
    kDIErrDataOverrun           = -34,  // tried to write off end of the image
    kDIErrGenericIO             = -35,  // generic I/O error

    kDIErrOddLength             = -40,  // image size not multiple of sectors
    kDIErrUnrecognizedFileFmt   = -41,  // file format just not recognized
    kDIErrBadFileFormat         = -42,  // filename ext doesn't match contents
    kDIErrUnsupportedFileFmt    = -43,  // recognized but not supported
    kDIErrUnsupportedPhysicalFmt = -44, // (same)
    kDIErrUnsupportedFSFmt      = -45,  // (and again)
    kDIErrBadOrdering           = -46,  // requested sector ordering no good
    kDIErrFilesystemNotFound    = -47,  // requested filesystem isn't there
    kDIErrUnsupportedAccess     = -48,  // e.g. read sectors from blocks-only
    kDIErrUnsupportedImageFeature = -49, // e.g. FDI image w/Amiga sectors

    kDIErrInvalidTrack          = -50,  // request for invalid track number
    kDIErrInvalidSector         = -51,  // request for invalid sector number
    kDIErrInvalidBlock          = -52,  // request for invalid block number
    kDIErrInvalidIndex          = -53,  // request with an invalid index

    kDIErrDirectoryLoop         = -60,  // directory chain points into itself
    kDIErrFileLoop              = -61,  // file sector or block alloc loops
    kDIErrBadDiskImage          = -62,  // the FS on the disk image is damaged
    kDIErrBadFile               = -63,  // bad file on disk image
    kDIErrBadDirectory          = -64,  // bad dir on disk image
    kDIErrBadPartition          = -65,  // bad partition on multi-part format

    kDIErrFileArchive           = -70,  // file archive, not disk archive
    kDIErrUnsupportedCompression = -71, // compression method is not supported
    kDIErrBadChecksum           = -72,  // image file's checksum is bad
    kDIErrBadCompressedData     = -73,  // data can't even be unpacked
    kDIErrBadArchiveStruct      = -74,  // bad archive structure

    kDIErrBadNibbleSectors      = -80,  // can't read sectors from this image
    kDIErrSectorUnreadable      = -81,  // requested sector not readable
    kDIErrInvalidDiskByte       = -82,  // invalid byte for encoding type
    kDIErrBadRawData            = -83,  // couldn't get correct nibbles

    kDIErrInvalidFileName       = -90,  // tried to create file with bad name
    kDIErrDiskFull              = -91,  // no space left on disk
    kDIErrVolumeDirFull         = -92,  // no more entries in volume dir
    kDIErrInvalidCreateReq      = -93,  // CreateImage request was flawed
    kDIErrTooBig                = -94,  // larger than we want to handle

    /* higher-level errors */
    kDIErrGeneric               = -101,
    kDIErrInternal              = -102,
    kDIErrMalloc                = -103,
    kDIErrInvalidArg            = -104,
    kDIErrNotSupported          = -105, // feature not currently supported
    kDIErrCancelled             = -106, // an operation was cancelled by user

    kDIErrNufxLibInitFailed     = -110
} DIError;

// clang-format on

struct ringbuf_t {
    uint8_t **buffer;
    size_t size; //of the buffer
    int initialized;
};

// Opaque circular buffer structure
typedef struct ringbuf_t ringbuf_t;
// Handle type, the way users interact with the API
typedef ringbuf_t *ringbuf_handle_t;

/*
 * Nibble encode/decode description.  Use no pointers here, so we
 * store as an array and resize at will.
 *
 * Should we define an enum to describe whether address and data
 * headers are standard or some wacky variant?
 */
enum {
    kNibbleAddrPrologLen = 3, // d5 aa 96
    kNibbleAddrEpilogLen = 3, // de aa eb
    kNibbleDataPrologLen = 3, // d5 aa ad
    kNibbleDataEpilogLen = 3 // de aa eb
};

typedef enum {              // sector ordering for "sector" format images
    kSectorOrderUnknown = 0,
    kSectorOrderProDOS = 1,     // written as series of ProDOS blocks
    kSectorOrderDOS = 2,        // written as series of DOS sectors
    kSectorOrderCPM = 3,        // written as series of 1K CP/M blocks
    kSectorOrderPhysical = 4,   // written as un-interleaved sectors
    kSectorOrderMax,            // (used for array sizing)
} SectorOrder;

typedef struct {
    char description[32];
    short numSectors; // 13 or 16 (or 18?)

    uint8_t addrProlog[kNibbleAddrPrologLen];
    uint8_t addrEpilog[kNibbleAddrEpilogLen];
    uint8_t addrChecksumSeed;
    int addrVerifyChecksum;
    int addrVerifyTrack;
    int addrEpilogVerifyCount;

    uint8_t dataProlog[kNibbleDataPrologLen];
    uint8_t dataEpilog[kNibbleDataEpilogLen];
    uint8_t dataChecksumSeed;
    int dataVerifyChecksum;
    int dataEpilogVerifyCount;
} NibbleDescr;

static const uint8_t kDiskBytes62[64] = {
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

/* static data tables */
static uint8_t *kInvDiskBytes62 = NULL;
#define kInvInvalidValue 0xff

static uint8_t *fNibbleTrackBuf = NULL; // allocated on heap
static int fNibbleTrackLoaded = -1; // track currently in buffer

static uint8_t *rawdata = NULL;
static size_t rawdatalen = 0;

typedef enum { // format of the image data stream
    kPhysicalFormatUnknown = 0,
    kPhysicalFormatSectors = 1, // sequential 256-byte sectors (13/16/32)
    kPhysicalFormatNib525_6656 = 2, // 5.25" disk ".nib" (6656 bytes/track)
    kPhysicalFormatNib525_Var = 4, // 5.25" disk (variable len, e.g. ".app")
} PhysicalFormat;

static const int kTrackLenNib525 = 6656;

static const int kBlkSize = 512;
static const int kVolHeaderBlock = 2;          // block where Volume Header resides
static const int kMinReasonableBlocks = 16;    // min size for ProDOS volume
static const int kExpectedBitmapStart = 6;     // block# where vol bitmap should start
static const int kMaxCatalogIterations = 1024; // theoretical max is 32768?
static const int kMaxDirectoryDepth = 64;      // not sure what ProDOS limit is

/*
 * Directory header.  All fields not marked as "only for subdirs" also apply
 * to the volume directory header.
 */
typedef struct DirHeader {
    uint8_t     storageType;
    char        dirName[kMaxFileName+1];
    uint8_t     version;
    uint8_t     minVersion;
    uint8_t     access;
    uint8_t     entryLength;
    uint8_t     entriesPerBlock;
    uint16_t    fileCount;
    /* the rest are only for subdirs */
    uint16_t    parentPointer;
    uint8_t     parentEntry;
    uint8_t     parentEntryLength;
} DirHeader;

/* contents of a directory entry */
typedef struct DirEntry {
    int             storageType;
    char            fileName[kMaxFileName+1];   // shows lower case
    uint8_t         fileType;
    uint16_t        keyPointer;
    uint16_t        blocksUsed;
    uint32_t        eof;
    uint8_t         version;
    uint8_t         minVersion;
    uint8_t         access;
    uint16_t        auxType;
    uint16_t        headerPointer;
} DirEntry;

typedef struct DirEntry DirEntry;


struct A2File {
    int starting_block;
    int fTSListTrack;
    int fTSListSector;
    int fLocked;
    int fFileType;
    uint8_t fRawFileName[kFileNameBufLen];
    uint8_t fFileName[kFileNameBufLen];
    int fLengthInSectors;
    int fCatTStrack;
    int fCatTSsector;
    int fCatEntryNum;
    TrackSector *tsList;
    int tsCount;
    TrackSector *indexList;
    int indexCount;
    int fOffset;
    int fOpenEOF;
    int fOpenSectorsUsed;
    struct A2File *prev;
    struct A2File *next;

    // ProDOS-specific
    long fBlockCount;
    uint16_t *fBlockList;
    char *pathName;
    DirEntry fDirEntry;
};

typedef struct A2File A2File;

typedef enum {
    kStorageDeleted         = 0,      /* indicates deleted file */
    kStorageSeedling        = 1,      /* <= 512 bytes */
    kStorageSapling         = 2,      /* < 128KB */
    kStorageTree            = 3,      /* < 16MB */
    kStoragePascalVolume    = 4,      /* see ProDOS technote 25 */
    kStorageExtended        = 5,      /* forked */
    kStorageDirectory       = 13,
    kStorageSubdirHeader    = 14,
    kStorageVolumeDirHeader = 15,
} StorageType;


static A2File *firstfile = NULL;
static A2File *lastfile = NULL;

static int fFirstCatTrack;
static int fFirstCatSector;
static int fVTOCVolumeNumber;
static int fVTOCNumTracks;
static int fVTOCNumSectors;
uint8_t fVTOC[kSectorSize];

static char fVolumeName[30];
static char fVolumeID[31];

static int fBitMapPointer;
static int fTotalBlocks;

static long            fNumTracks = -1;     // for sector-addressable images
static int             fNumSectPerTrack = -1;   // (ditto)
static long            fNumBlocks = -1;     // for 512-byte block-addressable images

typedef enum {              // main filesystem format (based on NuFX enum)
    kFormatUnknown = 0,
    kFormatProDOS = 1,
    kFormatDOS33 = 2,
    kFormatDOS32 = 3,
    kFormatGenericPhysicalOrd = 30, // unknown, but physical-sector-ordered
    kFormatGenericProDOSOrd = 31,   // unknown, but ProDOS-block-ordered
    // try to keep this in an unsigned char, e.g. for CP clipboard
} FSFormat;

PhysicalFormat  fPhysical = kPhysicalFormatNib525_6656;;

static int fHasSectors;    // image is sector-addressable
static int fHasBlocks;     // image is block-addressable
static int fHasNibbles;    // image is nibble-addressable

static const int kDefaultTSAlloc = 2;
static const int kDefaultIndexAlloc = 8;

static const int kTrackCount525 = 35;      // expected #of tracks on 5.25 img

typedef enum { kInitUnknown = 0, kInitHeaderOnly, kInitFull } InitMode;

static void *MemAlloc(size_t size)
{
    void *t = (void *)malloc(size);
    if (t == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return (t);
}

/*
 * Get values from a memory buffer.
 */
static uint16_t GetShortLE(const uint8_t* ptr)
{
    return *ptr | (uint16_t) *(ptr+1) << 8;
}

static uint32_t GetLongLE(const uint8_t* ptr)
{
    return *ptr |
    (uint32_t) *(ptr+1) << 8 |
    (uint32_t) *(ptr+2) << 16 |
    (uint32_t) *(ptr+3) << 24;
}

static uint8_t access_ringbuf(ringbuf_handle_t ringbuf, int index)
{
    if (ringbuf == NULL || ringbuf->initialized == 0 || ringbuf->buffer == NULL || *(ringbuf->buffer) == NULL) {
        debug_print("ERROR! Ringbuf not ready!\n");
        return -1;
    }

    if (index >= ringbuf->size)
        index -= ringbuf->size;
    if (index < 0)
        index = (int)ringbuf->size - index;
    uint8_t *buffer = *(ringbuf->buffer);

    return buffer[index];
}

/*
 * Compute tables to convert disk bytes back to values.
 *
 * Should be called once, at DLL initialization time.
 */
static void CalcNibbleInvTables(void)
{
    unsigned int i;

    if (kInvDiskBytes62 != NULL)
        return;

    kInvDiskBytes62 = MemAlloc(256);

    memset(kInvDiskBytes62, kInvInvalidValue, 256);
    for (i = 0; i < sizeof(kDiskBytes62); i++) {
        assert(kDiskBytes62[i] >= 0x96);
        kInvDiskBytes62[kDiskBytes62[i]] = i;
    }
}

static uint16_t ConvFrom44(uint8_t val1, uint8_t val2)
{
    return ((val1 << 1) | 0x01) & val2;
}

/*
 * Decode the values in the address field.
 */
static void DecodeAddr(ringbuf_handle_t ringbuffer, int offset,
    short *pVol, short *pTrack, short *pSector, short *pChksum)
{
    //unsigned int vol, track, sector, chksum;

    *pVol = ConvFrom44(access_ringbuf(ringbuffer, offset), access_ringbuf(ringbuffer, offset + 1));
    *pTrack = ConvFrom44(access_ringbuf(ringbuffer, offset + 2), access_ringbuf(ringbuffer, offset + 3));
    *pSector = ConvFrom44(access_ringbuf(ringbuffer, offset + 4), access_ringbuf(ringbuffer, offset + 5));
    *pChksum = ConvFrom44(access_ringbuf(ringbuffer, offset + 6), access_ringbuf(ringbuffer, offset + 7));
}

static const NibbleDescr nibbleDescr = {
    "DOS 3.3 Standard",
    16, // number of sectors
    { 0xd5, 0xaa, 0x96 }, // addrProlog
    { 0xde, 0xaa, 0xeb }, // addrEpilog
    0x00, // checksum seed
    0, // verify checksum
    0, // verify track
    2, // epilog verify count
    { 0xd5, 0xaa, 0xad }, // dataProlog
    { 0xde, 0xaa, 0xeb }, // dataEpilog
    0x00, // checksum seed
    0, // verify checksum
    2, // epilog verify count
};

static const NibbleDescr *fpNibbleDescr = &nibbleDescr;

/*
 * Find the start of the data field of a sector in nibblized data.
 *
 * Returns the index start on success or -1 on failure.
 */
static int FindNibbleSectorStart(ringbuf_handle_t ringbuffer, int track,
    int sector, int *pVol)
{
    long trackLen = ringbuffer->size;
    const int kMaxDataReach = (int)trackLen; // fairly arbitrary

    assert(sector >= 0 && sector < 16);

    int i;

    for (i = 0; i < trackLen; i++) {
        int foundAddr = 0;

        if (access_ringbuf(ringbuffer, i) == fpNibbleDescr->addrProlog[0] && access_ringbuf(ringbuffer, i + 1) == fpNibbleDescr->addrProlog[1] && access_ringbuf(ringbuffer, i + 2) == fpNibbleDescr->addrProlog[2]) {
            foundAddr = 1;
        }

        if (foundAddr) {
            //i += 3;

            /* found the address header, decode the address */
            short hdrVol, hdrTrack, hdrSector, hdrChksum;
            DecodeAddr(ringbuffer, i + 3, &hdrVol, &hdrTrack, &hdrSector,
                &hdrChksum);

            if (fpNibbleDescr->addrVerifyTrack && track != hdrTrack) {
                debug_print("Track mismatch");
                debug_print("  Track mismatch (T=%d) got T=%d,S=%d", track, hdrTrack, hdrSector);
                continue;
            }

            if (fpNibbleDescr->addrVerifyChecksum) {
                if ((fpNibbleDescr->addrChecksumSeed ^ hdrVol ^ hdrTrack ^ hdrSector ^ hdrChksum) != 0) {
                    debug_print("   Addr checksum mismatch (want T=%d,S=%d, got T=%d,S=%d)", track, sector, hdrTrack, hdrSector);
                    continue;
                }
            }

            i += 3;

            int j;
            for (j = 0; j < fpNibbleDescr->addrEpilogVerifyCount; j++) {
                if (access_ringbuf(ringbuffer, i + 8 + j) != fpNibbleDescr->addrEpilog[j]) {
                    //debug_print("   Bad epilog byte %d (%02x vs %02x)",
                    //    j, buffer[i+8+j], pNibbleDescr->addrEpilog[j]);
                    break;
                }
            }
            if (j != fpNibbleDescr->addrEpilogVerifyCount)
                continue;

#ifdef NIB_VERBOSE_DEBUG
            debug_print("    Good header, T=%d,S=%d (looking for T=%d,S=%d)\n",
                hdrTrack, hdrSector, track, sector);
#endif

            if (sector != hdrSector)
                continue;

            /*
             * Scan forward and look for data prolog.  We want to limit
             * the reach of our search so we don't blunder into the data
             * field of the next sector.
             */
            for (j = 0; j < kMaxDataReach; j++) {
                if (access_ringbuf(ringbuffer, i + j) == fpNibbleDescr->dataProlog[0] && access_ringbuf(ringbuffer, i + j + 1) == fpNibbleDescr->dataProlog[1] && access_ringbuf(ringbuffer, i + j + 2) == fpNibbleDescr->dataProlog[2]) {
                    *pVol = hdrVol;
                    int idx = i + j + 3;
                    while (idx >= ringbuffer->size)
                        idx -= ringbuffer->size;
                    return idx;
                }
            }
        }
    }

#ifdef NIB_VERBOSE_DEBUG
    debug_print("   Couldn't find T=%d,S=%d", track, sector);
#endif
    return -1;
}

/*
 * Decode 6&2 encoding.
 */
static DIError DecodeNibbleData(ringbuf_handle_t ringbuffer, int idx,
    uint8_t *sctBuf)
{
    uint8_t twos[kChunkSize62 * 3]; // 258
    int chksum = fpNibbleDescr->dataChecksumSeed;
    uint8_t decodedVal;
    int i;

    /*
     * Pull the 342 bytes out, convert them from disk bytes to 6-bit
     * values, and arrange them into a DOS-like pair of buffers.
     */
    for (i = 0; i < kChunkSize62; i++) {
        decodedVal = kInvDiskBytes62[access_ringbuf(ringbuffer, idx++)];
        if (decodedVal == kInvInvalidValue)
            return kDIErrInvalidDiskByte;
        assert(decodedVal < sizeof(kDiskBytes62));

        chksum ^= decodedVal;
        twos[i] = ((chksum & 0x01) << 1) | ((chksum & 0x02) >> 1);
        twos[i + kChunkSize62] = ((chksum & 0x04) >> 1) | ((chksum & 0x08) >> 3);
        twos[i + kChunkSize62 * 2] = ((chksum & 0x10) >> 3) | ((chksum & 0x20) >> 5);
    }

    for (i = 0; i < 256; i++) {
        decodedVal = kInvDiskBytes62[access_ringbuf(ringbuffer, idx++)];
        if (decodedVal == kInvInvalidValue)
            return kDIErrInvalidDiskByte;
        assert(decodedVal < sizeof(kDiskBytes62));

        chksum ^= decodedVal;
        sctBuf[i] = (chksum << 2) | twos[i];
    }

    /*
     * Grab the 343rd byte (the checksum byte) and see if we did this
     * right.
     */
    debug_print("Dec checksum value is 0x%02x\n", chksum);
    decodedVal = kInvDiskBytes62[access_ringbuf(ringbuffer, idx++)];
    if (decodedVal == kInvInvalidValue)
        return kDIErrInvalidDiskByte;
    assert(decodedVal < sizeof(kDiskBytes62));
    chksum ^= decodedVal;

    if (fpNibbleDescr->dataVerifyChecksum && chksum != 0) {
        debug_print("    NIB bad data checksum");
        return kDIErrBadChecksum;
    }
    return kDIErrNone;
}

/*
 * Copy a chunk of bytes out of the disk image.
 *
 * (This is the lowest-level read routine in this class.)
 */
static DIError CopyBytesOut(void *buf, size_t offset, int size)
{
    //    if (offset + size > rawdatalen)
    //        return kDIErrDataUnderrun;
    memcpy(buf, rawdata + offset, size);
    return kDIErrNone;
}

/*
 * Handle sector order conversions.
 */
static DIError CalcSectorAndOffset(long track, int sector, SectorOrder imageOrder,
                                   SectorOrder fsOrder, size_t *pOffset, int *pNewSector)
{
    /*
     * Sector order conversions.  No table is needed for Copy ][+ format,
     * which is equivalent to "physical".
     */

    static const int dos2raw[16] = {
        0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
    };

    static const int raw2prodos[16] = {
        0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
    };
    static const int prodos2raw[16] = {
        0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
    };

    if (track < 0 || track >= kNumTracks) {
        debug_print(" DI read invalid track %ld\n", track);
        return kDIErrInvalidTrack;
    }
    if (sector < 0 || sector >= kNumSectPerTrack) {
        debug_print(" DI read invalid sector %d\n", sector);
        return kDIErrInvalidSector;
    }

    size_t offset;
    int newSector = -1;

    /*
     * 16-sector disks write sectors in ascending order and then remap
     * them with a translation table.
     */
    offset = track * kNumSectPerTrack * kSectorSize;
    if (sector >= 16) {
        offset += 16 * kSectorSize;
        sector -= 16;
    }
    assert(sector >= 0 && sector < 16);

    /* convert request to "raw" sector number */

    /* convert request to "raw" sector number */
    switch (fsOrder) {
        case kSectorOrderProDOS:
            newSector = prodos2raw[sector];
            break;
        case kSectorOrderDOS:
            newSector = dos2raw[sector];
            break;
        case kSectorOrderPhysical:  // used for Copy ][+
            newSector = sector;
            break;
        case kSectorOrderUnknown:
            // should never happen; fall through to "default"
        default:
            newSector = sector;
            break;
    }

    /* convert "raw" request to the image's ordering */
    switch (imageOrder) {
        case kSectorOrderProDOS:
            newSector = raw2prodos[newSector];
            break;
//        case kSectorOrderDOS:
//            newSector = raw2dos[newSector];
//            break;
        case kSectorOrderPhysical:
            //newSector = newSector;
            break;
        case kSectorOrderUnknown:
            // should never happen; fall through to "default"
        default:
            //newSector = newSector;
            break;
    }

    offset += newSector * kSectorSize;

    *pOffset = offset;
    *pNewSector = newSector;
    return kDIErrNone;
}

/*
 * Load a nibble track into our track buffer.
 */
static DIError LoadNibbleTrack(long track, long *pTrackLen)
{
    DIError dierr = kDIErrNone;
    long offset;
    assert(track >= 0 && track < kMaxNibbleTracks525);

    if (fPhysical != kPhysicalFormatNib525_6656)
        return kDIErrNotSupported;

    *pTrackLen = kTrackLenNib525;
    offset = kTrackLenNib525 * track;
    assert(*pTrackLen > 0);
    assert(offset >= 0);

    if (track == fNibbleTrackLoaded) {
#ifdef NIB_VERBOSE_DEBUG
        debug_print("  DI track %ld already loaded\n", track);
#endif
        return kDIErrNone;
    } else {
        debug_print("  DI loading track %ld\n", track);
    }

    /* invalidate in case we fail with partial read */
    fNibbleTrackLoaded = -1;

    /* alloc track buffer if needed */
    if (fNibbleTrackBuf == NULL) {
        fNibbleTrackBuf = MemAlloc(kTrackAllocSize);
    }

    /*
     * Read the entire track into memory.
     */
    dierr = CopyBytesOut(fNibbleTrackBuf, offset, (int)*pTrackLen);
    if (dierr != kDIErrNone)
        return dierr;

    fNibbleTrackLoaded = (int)track;

    return dierr;
}

/*
 * Read a sector from a nibble image.
 *
 * While fNumTracks is valid, fNumSectPerTrack is a little flaky, because
 * in theory each track could be formatted differently.
 */
static DIError ReadNibbleSector(long track, int sector, uint8_t *buf)
{
    if (fpNibbleDescr == NULL) {
        /* disk has no recognizable sectors */
        debug_print(" DI ReadNibbleSector: pNibbleDescr is NULL, returning failure");
        return kDIErrBadNibbleSectors;
    }
    if (sector >= fpNibbleDescr->numSectors) {
        /* e.g. trying to read sector 14 on a 13-sector disk */
        debug_print(" DI ReadNibbleSector: bad sector number request");
        return kDIErrInvalidSector;
    }

    assert(fpNibbleDescr != NULL);
    assert(track >= 0 && track < kNumTracks);
    assert(sector >= 0 && sector < fpNibbleDescr->numSectors);

    DIError dierr = kDIErrNone;
    long trackLen;
    int sectorIdx, vol;

    dierr = LoadNibbleTrack(track, &trackLen);
    if (dierr != kDIErrNone) {
        debug_print("   DI ReadNibbleSector: LoadNibbleTrack %ld failed", track);
        return dierr;
    }

    ringbuf_t circularbuf;
    ringbuf_handle_t ringbuffer = &circularbuf;
    ringbuffer->size = trackLen;
    ringbuffer->buffer = &fNibbleTrackBuf;
    ringbuffer->initialized = 1;

    sectorIdx = FindNibbleSectorStart(ringbuffer, (int)track, sector,
        &vol);
    if (sectorIdx < 0) {
        return kDIErrSectorUnreadable;
    }

    dierr = DecodeNibbleData(ringbuffer, sectorIdx, buf);

    return dierr;
}

void InitNibImage(uint8_t *data, size_t datasize)
{
    if (kInvDiskBytes62 == NULL)
        CalcNibbleInvTables();

    rawdata = data;
    rawdatalen = datasize;
    fNibbleTrackLoaded = -1;

    fPhysical = kPhysicalFormatNib525_6656;
}

void InitDskImage(uint8_t *data, size_t datasize)
{
    rawdata = data;
    rawdatalen = datasize;

    fPhysical = kPhysicalFormatSectors;
}

void FreeDiskImage(void)
{
    rawdata = NULL;
    rawdatalen = 0;
    fNibbleTrackLoaded = -1;
    if (fNibbleTrackBuf != NULL) {
        free(fNibbleTrackBuf);
        fNibbleTrackBuf = NULL;
    }
    while (firstfile != NULL) {
        A2File *nextfile = firstfile->next;
        if (firstfile->tsList != NULL)
            free(firstfile->tsList);
        if (firstfile->indexList != NULL)
            free(firstfile->indexList);
        if (firstfile->fBlockList != NULL)
            free(firstfile->fBlockList);
        if (firstfile->pathName != NULL)
            free(firstfile->pathName);
        free(firstfile);
        firstfile = nextfile;
    }
    lastfile = NULL;
}

uint8_t *ReadImageFromNib(size_t offset, size_t size, uint8_t *data, size_t datasize)
{
    uint8_t *result = MemAlloc(size);

    rawdata = data;
    rawdatalen = datasize;

    fPhysical = kPhysicalFormatNib525_6656;

    uint8_t buf[0x100];

    size_t pOffset;
    int pNewSector;
    int track = (int)(offset / 0x1000);
    int sector = (offset % 0x1000) / 0x100;

    for (int remaining = (int)size; remaining > 0; remaining -= 0x100) {
        DIError error = CalcSectorAndOffset(track, sector, kSectorOrderPhysical, kSectorOrderDOS, &pOffset, &pNewSector);
        if (error != kDIErrNone) {
            fprintf(stderr, "Error while calculating sector and offset of nib!\n");
            free(result);
            return NULL;
        }
        error = ReadNibbleSector(track, pNewSector, buf);
        if (error != kDIErrNone) {
            fprintf(stderr, "Error while reading nibble sector!\n");
            free(result);
            return NULL;
        }
        sector++;
        if (sector > 15) {
            sector = 0;
            track++;
        }
        memcpy(result + (size - remaining), buf, MIN(0x100, remaining));
    }

    return result;
}

/*
 * Read the specified track and sector, adjusting for sector ordering as
 * appropriate.
 *
 * Copies 256 bytes into "*buf".
 *
 * Returns 0 on success, nonzero on failure.
 */
static DIError ReadTrackSector(long track, int sector, SectorOrder imageOrder, SectorOrder fsOrder, void *buf)
{
    DIError dierr;
    size_t offset;
    int newSector = -1;

    if (buf == NULL)
        return kDIErrInvalidArg;

    dierr = CalcSectorAndOffset(track, sector, imageOrder, fsOrder, &offset, &newSector);
    if (dierr != kDIErrNone)
        return dierr;

    assert(offset + kSectorSize <= fLength);

    if (fPhysical == kPhysicalFormatNib525_6656)
        dierr = ReadNibbleSector(track, newSector, buf);
    else
        dierr = CopyBytesOut(buf, offset, kSectorSize);

    return dierr;
}

static void AddFileToList(A2File *file)
{
    if (firstfile == NULL) {
        firstfile = file;
    } else if (lastfile == NULL) {
        lastfile = file;
        firstfile->next = file;
        file->prev = firstfile;
    } else {
        lastfile->next = file;
        file->prev = lastfile;
        lastfile = file;
    }
}

/*
 * Convert high ASCII to low ASCII.
 *
 * Some people put inverse and flashing text into filenames, not to mention
 * control characters, so we have to cope with those too.
 *
 * We modify the first "len" bytes of "buf" in place.
 */
static void LowerASCII(uint8_t filename[kFileNameBufLen])
{
    int len = kMaxFileName;
    uint8_t *buf = filename;
    while (len--) {
        if (*buf & 0x80) {
            if (*buf >= 0xa0)
                *buf &= 0x7f;
            else
                *buf = (*buf & 0x7f) + 0x20;
        } else
            *buf = ((*buf & 0x3f) ^ 0x20) + 0x20;

        buf++;
    }
}

/*
 * Trim the spaces off the end of a filename.
 *
 * Assumes the filename has already been converted to low ASCII.
 */
static void TrimTrailingSpaces(uint8_t filename[kFileNameBufLen])
{
    uint8_t *lastspc = filename + strlen((char *)filename);

    assert(*lastspc == '\0');

    while (--lastspc) {
        if (*lastspc != ' ')
            break;
    }

    *(lastspc + 1) = '\0';
}

/*
 * "Fix" a DOS3.3 filename.  Convert DOS-ASCII to normal ASCII, and strip
 * trailing spaces.
 */
static void FixFilename(uint8_t filename[kFileNameBufLen])
{
    LowerASCII(filename);
    TrimTrailingSpaces(filename);
}

/*
 * Extract the track/sector pairs from the TS list in "sctBuf".  The entries
 * are copied to "tsList", which is assumed to have enough space to hold
 * at least kMaxTSPairs entries.
 *
 * The last non-zero entry will be identified and stored in "*pLastNonZero".
 * If all entries are zero, it will be set to -1.
 *
 * Sometimes files will have junk at the tail end of an otherwise valid
 * T/S list.  We can't just stop when we hit the first (0,0) entry because
 * that'll screw up random-access text file handling.  What we can do is
 * try to detect the situation, and mark the file as "suspicious" without
 * returning an error if we see it.
 *
 * If a TS entry appears to be invalid, this returns an error after all
 * entries have been copied.  If it looks to be partially valid, only the
 * valid parts are copied out, with the rest zeroed.
 */
static DIError ExtractTSPairs(A2File *DOSFile, const uint8_t *sctBuf, TrackSector *tsList,
    int *pLastNonZero)
{
    DIError dierr = kDIErrNone;
    //    const DiskImg* pDiskImg = fpDiskFS->GetDiskImg();
    const uint8_t *ptr;
    int i, track, sector;

    *pLastNonZero = -1;
    memset(tsList, 0, sizeof(TrackSector) * kMaxTSPairs);

    ptr = &sctBuf[kTSOffset]; // offset of first T/S entry (0x0c)

    for (i = 0; i < kMaxTSPairs; i++) {
        track = *ptr++;
        sector = *ptr++;

        if (dierr == kDIErrNone && (track >= kNumTracks || sector >= kNumSectPerTrack || (track == 0 && sector != 0))) {
            debug_print(" DOS33 invalid T/S %d,%d in '%s'\n", track, sector,
                DOSFile->fFileName);

            if (i > 0 && tsList[i - 1].track == 0 && tsList[i - 1].sector == 0) {
                debug_print("  T/S list looks partially valid\n");
                goto bail; // quit immediately
            } else {
                dierr = kDIErrBadFile;
                // keep going, just so caller has the full set to stare at
            }
        }

        if (track != 0 || sector != 0)
            *pLastNonZero = i;

        tsList[i].track = track;
        tsList[i].sector = sector;
    }

bail:
    return dierr;
}


/*
 * Load the T/S list for this file.
 *
 * A single T/S sector holds 122 entries, enough to store a 30.5K file.
 * It's very unlikely that a file will need more than two, although it's
 * possible for a random-access text file to have a very large number of
 * entries.
 *
 * If "pIndexList" and "pIndexCount" are non-NULL, the list of index blocks is
 * also loaded.
 *
 * It's entirely possible to get a large T/S list back that is filled
 * entirely with zeroes.  This can happen if we have a large set of T/S
 * index sectors that are all zero.  We have to leave space for them so
 * that the Write function can use the existing allocated index blocks.
 *
 * THOUGHT: we may want to use the file type to tighten this up a bit.
 * For example, we're currently very careful around random-access text
 * files, but if the file doesn't have type 'T' then random access is
 * impossible.  Currently this isn't a problem, but for e.g. T/S lists
 * with garbage at the end would could deal with the problem more generally.
 */
static DIError LoadTSList(A2File *DOSFile)
{
    DIError dierr = kDIErrNone;

    TrackSector *tsList = NULL;
    TrackSector *indexList = NULL;
    int tsCount, tsAlloc;
    int indexCount, indexAlloc;
    uint8_t sctBuf[kSectorSize];
    int track, sector, iterations;

    debug_print("--- DOS loading T/S list for '%s'\n", DOSFile->fFileName);

    /* over-alloc for small files to reduce reallocs */
    tsAlloc = kMaxTSPairs * kDefaultTSAlloc;
    tsList = MemAlloc(tsAlloc * sizeof(TrackSector));
    tsCount = 0;

    indexAlloc = kDefaultIndexAlloc;
    indexList = MemAlloc(indexAlloc * sizeof(TrackSector));
    indexCount = 0;

    /* get the first T/S sector for this file */
    track = DOSFile->fTSListTrack;
    sector = DOSFile->fTSListSector;
    if (track >= kNumTracks || sector >= kNumSectPerTrack) {
        debug_print(" DOS33 invalid initial T/S %d,%d in '%s'\n", track, sector,
            DOSFile->fFileName);
        dierr = kDIErrBadFile;
        goto bail;
    }

    /*
     * Run through the set of t/s pairs.
     */
    iterations = 0;
    do {
        uint16_t sectorOffset;
        int lastNonZero;

        /*
         * Add the current T/S sector to the index list.
         */
        if (indexCount == indexAlloc) {
            debug_print("+++ expanding index list\n");
            TrackSector *newList;
            indexAlloc += kDefaultIndexAlloc;
            newList = MemAlloc(indexAlloc * sizeof(TrackSector));
            memcpy(newList, indexList, indexCount * sizeof(TrackSector));
            free(indexList);
            indexList = newList;
        }
        indexList[indexCount].track = track;
        indexList[indexCount].sector = sector;
        indexCount++;

        dierr = ReadTrackSector(track, sector, kSectorOrderPhysical, kSectorOrderDOS, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        /* grab next track/sector */
        track = sctBuf[0x01];
        sector = sctBuf[0x02];
        sectorOffset = sctBuf[0x05] + sctBuf[0x06] * 256;

        /* if T/S link is bogus, whole sector is probably bad */
        if (track >= kNumTracks || sector >= kNumSectPerTrack) {
            // bogus T/S, mark file as damaged and stop
            debug_print(" DOS33 invalid T/S link %d,%d in '%s'\n", track, sector,
                DOSFile->fFileName);
            dierr = kDIErrBadFile;
            goto bail;
        }
        if ((sectorOffset % kMaxTSPairs) != 0) {
            debug_print(" DOS33 invalid T/S header sector offset %u in '%s'\n",
                sectorOffset, DOSFile->fFileName);
            // not fatal, just weird
        }

        /*
         * Make sure we have enough room to hold an entire sector full of
         * T/S pairs in the list.
         */
        if (tsCount + kMaxTSPairs > tsAlloc) {
            debug_print("+++ expanding ts list\n");
            TrackSector *newList;
            tsAlloc += kMaxTSPairs * kDefaultTSAlloc;
            newList = MemAlloc(tsAlloc * sizeof(TrackSector));
            memcpy(newList, tsList, tsCount * sizeof(TrackSector));
            free(tsList);
            tsList = newList;
        }

        /*
         * Add the entries.  If there's another T/S list linked, we just
         * grab the entire sector.  If not, we grab every entry until the
         * last 0,0.  (Can't stop at the first (0,0), or we'll drop a
         * piece of a random access text file.)
         */
        dierr = ExtractTSPairs(DOSFile, sctBuf, &tsList[tsCount], &lastNonZero);
        if (dierr != kDIErrNone)
            goto bail;

        if (track != 0 && sector != 0) {
            /* more T/S lists to come, so we keep all entries */
            tsCount += kMaxTSPairs;
        } else {
            /* this was the last one */
            if (lastNonZero == -1) {
                /* this is ALWAYS the case for a newly-created file */
                //debug_print(" DOS33 odd -- last T/S sector of '%s' was empty",
            }
            tsCount += lastNonZero + 1;
        }

        iterations++; // watch for infinite loops
    } while (!(track == 0 && sector == 0) && iterations < kMaxTSIterations);

    if (iterations == kMaxTSIterations) {
        dierr = kDIErrFileLoop;
        goto bail;
    }

    DOSFile->tsList = tsList;
    DOSFile->tsCount = tsCount;
    tsList = NULL;

    if (DOSFile->indexList != NULL) {
        DOSFile->indexList = indexList;
        DOSFile->indexCount = indexCount;
        indexList = NULL;
    }

bail:
    free(tsList);
    free(indexList);
    return dierr;
}

/*
 * Read data from the current offset.
 *
 * Files read back as they would from ProDOS, i.e. if you read a binary
 * file you won't see the 4 bytes of length and address.
 */
static DIError Read(A2File *pFile, uint8_t *buf, size_t len, size_t *pActual)
{
    debug_print(" DOS reading %lu bytes from '%s' (offset=%ld)\n",
        (unsigned long)len, pFile->fFileName, (long)pFile->fOffset);

    /*
     * Don't allow them to read past the end of the file.  The length value
     * stored in pFile->fLength already has pFile->fDataOffset subtracted
     * from the actual data length, so don't factor it in again.
     */
    if (pFile->fOffset + (long)len > pFile->fOpenEOF) {
        if (pActual == NULL)
            return kDIErrDataUnderrun;
        len = (size_t)(pFile->fOpenEOF - pFile->fOffset);
    }
    if (pActual != NULL)
        *pActual = len;
    long incrLen = len;

    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSectorSize];
    size_t actualOffset = pFile->fOffset; //+ pFile->fDataOffset;   // adjust for embedded len
    int tsIndex = (int)(actualOffset / kSectorSize);
    int bufOffset = (int)(actualOffset % kSectorSize); // (& 0xff)
    size_t thisCount;

    if (len == 0)
        return kDIErrNone;
    assert(pFile->fOpenEOF != 0);

    assert(tsIndex >= 0 && tsIndex < pFile->tsCount);

    /* could be more clever in here and avoid double-buffering */
    while (len) {
        if (tsIndex >= pFile->tsCount) {
            /* should've caught this earlier */
            debug_print(" DOS ran off the end (fTSCount=%d). len == %zu\n", pFile->tsCount, len);
            return kDIErrDataUnderrun;
        }

        if (pFile->tsList[tsIndex].track == 0 && pFile->tsList[tsIndex].sector == 0) {
            memset(sctBuf, 0, sizeof(sctBuf));
        } else {
            dierr = ReadTrackSector(pFile->tsList[tsIndex].track, pFile->tsList[tsIndex].sector, kSectorOrderPhysical, kSectorOrderDOS, sctBuf);
            if (dierr != kDIErrNone) {
                debug_print(" DOS error reading file '%s'\n", pFile->fFileName);
                return dierr;
            }
        }
        thisCount = kSectorSize - bufOffset;
        if (thisCount > len)
            thisCount = len;
        memcpy(buf, sctBuf + bufOffset, thisCount);
        len -= thisCount;
        buf = buf + thisCount;

        bufOffset = 0;
        tsIndex++;
    }

    pFile->fOffset += incrLen;

    return dierr;
}

/*
 * Determine whether an image uses a linear mapping.  This allows us to
 * optimize block reads & writes, very useful when dealing with logical
 * volumes under Windows (which also use 512-byte blocks).
 */
static int IsLinearBlocks(SectorOrder imageOrder, SectorOrder fsOrder)
{
    /*
     * Any time fOrder==fFileSysOrder, we know that we have a linear
     * mapping.  This holds true for reading ProDOS blocks from a ".po"
     * file or reading DOS sectors from a ".do" file.
     */
    return (fsOrder == kPhysicalFormatSectors && fHasBlocks &&
            imageOrder == fsOrder);
}


/*
 * Read a 512-byte block.
 *
 * Copies 512 bytes into "*buf".
 */
static DIError ReadBlockSwapped(long block, void* buf, SectorOrder imageOrder,
                         SectorOrder fsOrder)
{
    if (!fHasBlocks)
        return kDIErrUnsupportedAccess;
    if (block < 0 || block >= fNumBlocks)
        return kDIErrInvalidBlock;
    if (buf == NULL)
        return kDIErrInvalidArg;

    DIError dierr;
    long track, blkInTrk;

    if (fHasSectors && !IsLinearBlocks(imageOrder, fsOrder)) {
        /* run it through the t/s call so we handle DOS ordering */
        track = block / (fNumSectPerTrack/2);
        blkInTrk = block - (track * (fNumSectPerTrack/2));
        dierr = ReadTrackSector(track, (int)blkInTrk*2, imageOrder, fsOrder, buf);
        if (dierr != kDIErrNone)
            return dierr;
        dierr = ReadTrackSector(track, (int)blkInTrk*2+1, imageOrder, fsOrder,                                       (char*)buf+kSectorSize);
    } else if (fHasBlocks) {
        /* no sectors, so no swapping; must be linear blocks */
        if (imageOrder != fsOrder) {
            debug_print(" DI NOTE: ReadBlockSwapped on non-sector (%d/%d)",
                 imageOrder, fsOrder);
        }
        dierr = CopyBytesOut(buf, (off_t) block * kBlkSize, kBlkSize);
    } else {
        dierr = kDIErrInternal;
    }

bail:
    return dierr;
}


/*
 * Read a chunk of data from whichever fork is open.
 */
static DIError ProDOSRead(A2File *pFile, void* buf, size_t len, size_t* pActual)
{
    /*
     * Don't allow them to read past the end of the file.  The length value
     * stored in pFile->fLength already has pFile->fDataOffset subtracted
     * from the actual data length, so don't factor it in again.
     */
    if (pFile->fOffset + (long)len > pFile->fOpenEOF) {
        if (pActual == NULL)
            return kDIErrDataUnderrun;
        len = (size_t)(pFile->fOpenEOF - pFile->fOffset);
    }
    if (pActual != NULL)
        *pActual = len;
    long incrLen = len;

    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    long blockIndex = (long) (pFile->fOffset / kBlkSize);
    int bufOffset = (int) (pFile->fOffset % kBlkSize);
    size_t thisCount;

    if (len == 0) {
        return kDIErrNone;
    }
    assert(pFile->fOpenEOF != 0);

    assert(blockIndex >= 0 && blockIndex < pFile->fBlockCount);

    while (len) {
        dierr = ReadBlockSwapped(pFile->fBlockList[blockIndex],
                                 blkBuf, kSectorOrderPhysical,
                                 kSectorOrderProDOS);
        if (dierr != kDIErrNone) {
            debug_print(" ProDOS error reading block [%ld]=%d of '%s'",
                        blockIndex, pFile->fBlockList[blockIndex], pFile->pathName);
            return dierr;
        }
        thisCount = kBlkSize - bufOffset;
        if (thisCount > len)
            thisCount = len;

        memcpy(buf, blkBuf + bufOffset, thisCount);
        len -= thisCount;
        buf = (char*)buf + thisCount;

        bufOffset = 0;
        blockIndex++;
    }

    pFile->fOffset += incrLen;

    return dierr;
}

/*
 * Set up state for this file.
 */
static DIError Open(A2File *pOpenFile)
{
    DIError dierr = kDIErrNone;

    dierr = LoadTSList(pOpenFile);
    if (dierr != kDIErrNone) {
        debug_print("DOS33 unable to load TS for '%s' open\n", pOpenFile->fFileName);
        goto bail;
    }

    pOpenFile->fOffset = 0;
    pOpenFile->fOpenEOF = (int)fLength;
    pOpenFile->fOpenSectorsUsed = pOpenFile->fLengthInSectors;

bail:
    return dierr;
}

/*
 * Load the block list from a directory, which is essentially a linear
 * linked list.
 */
static DIError LoadDirectoryBlockList(A2File *pFile, uint16_t keyBlock,
                                             long eof, long* pBlockCount, uint16_t** pBlockList)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    uint16_t* list = NULL;
    uint16_t* listPtr;
    int iterations;
    long count;

    assert(pFile->fDirEntry.eof < 1024*1024*16);
    count = (pFile->fDirEntry.eof + kBlkSize -1) / kBlkSize;
    if (count == 0)
        count = 1;
    list = MemAlloc(sizeof(uint16_t) * count+1);
    if (list == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    /* this should take care of trailing sparse entries */
    memset(list, 0, sizeof(uint16_t) * count);
    list[count] = kInvalidBlockNum;     // overrun check

    iterations = 0;
    listPtr = list;

    while (keyBlock && iterations < kMaxCatalogIterations) {
        if (keyBlock < 2 ||
            keyBlock >= fNumBlocks)
        {
            debug_print(" ProDOS ERROR: directory block %u out of range", keyBlock);
            dierr = kDIErrInvalidBlock;
            goto bail;
        }

        *listPtr++ = keyBlock;

        dierr = ReadBlockSwapped(keyBlock, blkBuf,kSectorOrderPhysical,
                                 kSectorOrderProDOS);
        if (dierr != kDIErrNone)
            goto bail;

        keyBlock = GetShortLE(&blkBuf[0x02]);
        iterations++;
    }
    if (iterations == kMaxCatalogIterations) {
        debug_print(" ProDOS subdir iteration count exceeded");
        dierr = kDIErrDirectoryLoop;
        goto bail;
    }

    assert(list[count] == kInvalidBlockNum);

    *pBlockCount = count;
    *pBlockList = list;

bail:
    if (dierr != kDIErrNone)
        free(list);
    return dierr;
}


/*
 * Copy the entries from the index block in "block" to "list", copying
 * at most "maxCount" entries.
 */
static DIError LoadIndexBlock(uint16_t block, uint16_t* list,
                                     long maxCount)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    int i;

    if (maxCount > kMaxBlocksPerIndex)
        maxCount = kMaxBlocksPerIndex;

    dierr = ReadBlockSwapped(block, blkBuf, kSectorOrderPhysical, kSectorOrderProDOS);
    if (dierr != kDIErrNone)
        goto bail;

    //debug_print("LOADING 0x%04x", block);
    for (i = 0; i < maxCount; i++) {
        *list++ = blkBuf[i] | (uint16_t) blkBuf[i+256] << 8;
    }

bail:
    return dierr;
}



/*
 * Gather a linear, non-sparse list of file blocks into an array.
 *
 * Pass in the storage type and top-level key block.  Separation of
 * extended files should have been handled by the caller.  This loads the
 * list for only one fork.
 *
 * There are two kinds of sparse: sparse *inside* data, and sparse
 * *past* data.  The latter is interesting, because there is no need
 * to create space in index blocks to hold it.  Thus, a sapling could
 * hold a file with an EOF of 16MB.
 *
 * If "pIndexBlockCount" and "pIndexBlockList" are non-NULL, then we
 * also accumulate the list of index blocks and return those as well.
 * For a Tree-structured file, the first entry in the index list is
 * the master index block.
 *
 * The caller must delete[] "*pBlockList" and "*pIndexBlockList".
 */
static DIError LoadBlockList(A2File *pFile, int storageType, uint16_t keyBlock,
                                    long eof, long* pBlockCount, uint16_t** pBlockList,
                                    long* pIndexBlockCount, uint16_t** pIndexBlockList)
{
    if (storageType == kStorageDirectory ||
        storageType == kStorageVolumeDirHeader)
    {
        assert(pIndexBlockList == NULL && pIndexBlockCount == NULL);
        return LoadDirectoryBlockList(pFile, keyBlock, eof, pBlockCount, pBlockList);
    }

    assert(keyBlock != 0);
    assert(pBlockCount != NULL);
    assert(pBlockList != NULL);
    assert(*pBlockList == NULL);
    if (storageType != kStorageSeedling &&
        storageType != kStorageSapling &&
        storageType != kStorageTree)
    {
        /*
         * We can get here if somebody puts a bad storage type inside the
         * extended key block of a forked file.  Bad storage types on other
         * kinds of files are caught earlier.
         */
        debug_print(" ProDOS unexpected storageType %d in '%s'",
             storageType, pFile->pathName);
        return kDIErrNotSupported;
    }

    DIError dierr = kDIErrNone;
    uint16_t* list = NULL;
    long count;

    assert(eof < 1024*1024*16);
    count = (eof + kBlkSize -1) / kBlkSize;
    if (count == 0)
        count = 1;
    list = MemAlloc(sizeof(uint16_t) * (count + 1));
    if (list == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    if (pIndexBlockList != NULL) {
        assert(pIndexBlockCount != NULL);
        assert(*pIndexBlockList == NULL);
    }

    /* this should take care of trailing sparse entries */
    memset(list, 0, sizeof(uint16_t) * count);
    list[count] = kInvalidBlockNum;     // overrun check

    if (storageType == kStorageSeedling) {
        list[0] = keyBlock;

        if (pIndexBlockList != NULL) {
            *pIndexBlockCount = 0;
            *pIndexBlockList = NULL;
        }
    } else if (storageType == kStorageSapling) {
        dierr = LoadIndexBlock(keyBlock, list, count);
        if (dierr != kDIErrNone)
            goto bail;

        if (pIndexBlockList != NULL) {
            *pIndexBlockCount = 1;
            *pIndexBlockList = MemAlloc(sizeof(uint16_t));
            **pIndexBlockList = keyBlock;
        }
    } else if (storageType == kStorageTree) {
        uint8_t blkBuf[kBlkSize];
        uint16_t* listPtr = list;
        uint16_t* outIndexPtr = NULL;
        long countDown = count;
        int idx = 0;

        dierr = ReadBlockSwapped(keyBlock, blkBuf, kSectorOrderPhysical, kSectorOrderProDOS);
        if (dierr != kDIErrNone)
            goto bail;

        if (pIndexBlockList != NULL) {
            int numIndices = ((int)count + kMaxBlocksPerIndex-1) / kMaxBlocksPerIndex;
            numIndices++;   // add one for the master index block
            *pIndexBlockList = MemAlloc(sizeof(uint16_t) * numIndices);
            outIndexPtr = *pIndexBlockList;
            *outIndexPtr++ = keyBlock;
            *pIndexBlockCount = 1;
        }

        while (countDown) {
            long blockCount = countDown;
            if (blockCount > kMaxBlocksPerIndex)
                blockCount = kMaxBlocksPerIndex;
            uint16_t idxBlock;

            idxBlock = blkBuf[idx] | (uint16_t) blkBuf[idx+256] << 8;
            if (idxBlock == 0) {
                /* fully sparse index block */
                //debug_print(" ProDOS that's seriously sparse (%d)!", idx);
                memset(listPtr, 0, blockCount * sizeof(uint16_t));
                if (pIndexBlockList != NULL) {
                    *outIndexPtr++ = idxBlock;
                    (*pIndexBlockCount)++;
                }
            } else {
                dierr = LoadIndexBlock(idxBlock, listPtr, blockCount);
                if (dierr != kDIErrNone)
                    goto bail;

                if (pIndexBlockList != NULL) {
                    *outIndexPtr++ = idxBlock;
                    (*pIndexBlockCount)++;
                }
            }

            idx++;
            listPtr += blockCount;
            countDown -= blockCount;
        }
    }

    assert(list[count] == kInvalidBlockNum);

    *pBlockCount = count;
    *pBlockList = list;

bail:
    if (dierr != kDIErrNone) {
        free(list);
        assert(*pBlockList == NULL);

        if (pIndexBlockList != NULL && *pIndexBlockList != NULL) {
            free(*pIndexBlockList);
            *pIndexBlockList = NULL;
        }
    }
    return dierr;
}


static DIError ProDOSOpen(A2File *pOpenFile)
{
    DIError dierr = kDIErrNone;

    DirEntry dirEntry = pOpenFile->fDirEntry;
        if (dirEntry.storageType == kStorageDirectory ||
            dirEntry.storageType == kStorageVolumeDirHeader)
    {
        dierr = LoadDirectoryBlockList(pOpenFile, dirEntry.keyPointer,
                                       dirEntry.eof, &pOpenFile->fBlockCount,
                                       &pOpenFile->fBlockList);
        pOpenFile->fOpenEOF = dirEntry.eof;
    } else if (dirEntry.storageType == kStorageSeedling ||
               dirEntry.storageType == kStorageSapling ||
               dirEntry.storageType == kStorageTree)
    {
        dierr = LoadBlockList(pOpenFile, dirEntry.storageType, dirEntry.keyPointer,
                              dirEntry.eof, &pOpenFile->fBlockCount,
                              &pOpenFile->fBlockList, NULL, NULL);
        pOpenFile->fOpenEOF = dirEntry.eof;
    } else {
        debug_print("PrODOS can't open unknown storage type %d",
             dirEntry.storageType);
        dierr = kDIErrBadDirectory;
        goto bail;
    }
    if (dierr != kDIErrNone) {
        debug_print(" ProDOS open failed");
        goto bail;
    }

    pOpenFile->fOffset = 0;

bail:
//    delete pOpenFile;
    return dierr;
}


/*
 * Process the list of files in one sector of the catalog.
 *
 * Pass in the track, sector, and the contents of that track and sector.
 * (We only use "catTrack" and "catSect" to fill out some fields.)
 */
static DIError ProcessCatalogSector(int catTrack, int catSect,
    const uint8_t *sctBuf)
{
    A2File *pFile;
    const uint8_t *pEntry;
    int i;

    pEntry = &sctBuf[kCatalogEntryOffset];

    for (i = 0; i < kCatalogEntriesPerSect; i++) {
        if (pEntry[0x00] != kEntryUnused && pEntry[0x00] != kEntryDeleted) {
            pFile = MemAlloc(sizeof(A2File));
            pFile->prev = NULL;
            pFile->next = NULL;

            pFile->fTSListTrack = pEntry[0x00];
            pFile->fTSListSector = pEntry[0x01];
            pFile->fLocked = (pEntry[0x02] & 0x80) != 0;
            switch (pEntry[0x02] & 0x7f) {
            case 0x00:
                pFile->fFileType = FILETYPE_T;
                break;
            case 0x01:
                pFile->fFileType = FILETYPE_I;
                break;
            case 0x02:
                pFile->fFileType = FILETYPE_A;
                break;
            case 0x04:
                pFile->fFileType = FILETYPE_B;
                break;
            case 0x08:
                pFile->fFileType = FILETYPE_S;
                break;
            case 0x10:
                pFile->fFileType = FILETYPE_R;
                break;
            case 0x20:
                pFile->fFileType = FILETYPE_A;
                break;
            case 0x40:
                pFile->fFileType = FILETYPE_B;
                break;
            default:
                /* some odd arrangement of bit flags? */
                debug_print(" DOS33 peculiar filetype byte 0x%02x\n", pEntry[0x02]);
                pFile->fFileType = FILETYPE_UNKNOWN;
                break;
            }

            memcpy(pFile->fRawFileName, &pEntry[0x03], kMaxFileName);
            pFile->fRawFileName[kMaxFileName] = '\0';

            memcpy(pFile->fFileName, &pEntry[0x03], kMaxFileName);
            pFile->fFileName[kMaxFileName] = '\0';
            FixFilename(pFile->fFileName);

            debug_print("File name: %s\n", pFile->fFileName);

            pFile->fLengthInSectors = pEntry[0x21];
            pFile->fLengthInSectors |= (uint16_t)pEntry[0x22] << 8;

            pFile->fCatTStrack = catTrack;
            pFile->fCatTSsector = catSect;
            pFile->fCatEntryNum = i;

            pFile->tsList = NULL;
            pFile->tsCount = 0;
            pFile->indexList = NULL;
            pFile->fBlockList = NULL;
            pFile->pathName = NULL;
            pFile->indexCount = 0;
            pFile->fOffset = 0;
            pFile->fOpenEOF = 0;
            pFile->fOpenSectorsUsed = 0;

            AddFileToList(pFile);
        }

        pEntry += kCatalogEntrySize;
    }

    return kDIErrNone;
}


/*
 * Run through the list of files and find one that matches (case-insensitive).
 *
 * This does not attempt to open files in sub-volumes.  We could, but it's
 * likely that the application has "decorated" the name in some fashion,
 * e.g. by prepending the sub-volume's volume name to the filename.  May
 * be best to let the application dig for the sub-volume.
 */
static A2File* GetFileByName(A2File* pFile, const char* fileName)
{
    pFile = pFile->next;
    while (pFile != NULL) {
        if (strcasecmp(pFile->pathName, fileName) == 0)
            return pFile;

        pFile = pFile->next;
    }

    return NULL;
}


/*
 * Pull the directory header out of the first block of a directory.
 */
static DIError GetDirHeader(const uint8_t* blkBuf, DirHeader* pHeader)
{
    int nameLen;

    pHeader->storageType = (blkBuf[0x04] & 0xf0) >> 4;

    nameLen = blkBuf[0x04] & 0x0f;
    memcpy(pHeader->dirName, &blkBuf[0x05], nameLen);
    pHeader->dirName[nameLen] = '\0';
    pHeader->version = blkBuf[0x20];
    pHeader->minVersion = blkBuf[0x21];
    pHeader->access = blkBuf[0x22];
    pHeader->entryLength = blkBuf[0x23];
    pHeader->entriesPerBlock = blkBuf[0x24];
    pHeader->fileCount = GetShortLE(&blkBuf[0x25]);
    pHeader->parentPointer = GetShortLE(&blkBuf[0x27]);
    pHeader->parentEntry = blkBuf[0x29];
    pHeader->parentEntryLength = blkBuf[0x2a];

    if (pHeader->entryLength * pHeader->entriesPerBlock > kBlkSize ||
        pHeader->entryLength * pHeader->entriesPerBlock == 0)
    {
        debug_print(" ProDOS invalid subdir header: entryLen=%d, entriesPerBlock=%d",
             pHeader->entryLength, pHeader->entriesPerBlock);
        return kDIErrBadDirectory;
    }

    return kDIErrNone;
}

static DIError SlurpEntries(A2File* pParent, const DirHeader* pHeader,
                     const uint8_t* blkBuf, int skipFirst, int* pCount,
                     const char* basePath, uint16_t thisBlock, int depth);

/*
 * Pass in the number of the first block of the directory.
 *
 * Start with "pParent" set to the magic entry for the volume dir.
 */
static DIError RecursiveDirAdd(A2File* pParent, uint16_t dirBlock,
                        const char* basePath, int depth)
{
    DIError dierr = kDIErrNone;
    DirHeader header;
    uint8_t blkBuf[kBlkSize];
    int numEntries, iterations, foundCount;
    int first;

    /* if we get too deep, assume it's a loop */
    if (depth > kMaxDirectoryDepth) {
        dierr = kDIErrDirectoryLoop;
        goto bail;
    }

    if (dirBlock < kVolHeaderBlock || dirBlock >= fNumBlocks) {
        debug_print(" ProDOS ERROR: directory block %u out of range", dirBlock);
        dierr = kDIErrInvalidBlock;
        goto bail;
    }

    numEntries = 1;
    iterations = 0;
    foundCount = 0;
    first = 1;

    while (dirBlock && iterations < kMaxCatalogIterations) {
        dierr = ReadBlockSwapped(dirBlock, blkBuf, kSectorOrderPhysical, kSectorOrderProDOS);
        if (dierr != kDIErrNone)
            goto bail;

        if (first) {
            /* this is the directory header entry */
            dierr = GetDirHeader(blkBuf, &header);
            if (dierr != kDIErrNone)
                goto bail;
            numEntries = header.fileCount;
        }

        /* slurp the entries out of this block */
        dierr = SlurpEntries(pParent, &header, blkBuf, first, &foundCount,
                             basePath, dirBlock, depth);
        if (dierr != kDIErrNone)
            goto bail;

        dirBlock = GetShortLE(&blkBuf[0x02]);
        if (dirBlock != 0 &&
            (dirBlock < 2 || dirBlock >= fNumBlocks))
        {
            debug_print(" ProDOS ERROR: invalid dir link block %u in base='%s'",
                        dirBlock, basePath);
            dierr = kDIErrInvalidBlock;
            goto bail;
        }
        first = 0;
        iterations++;
    }
    if (iterations == kMaxCatalogIterations) {
        debug_print(" ProDOS subdir iteration count exceeded");
        dierr = kDIErrDirectoryLoop;
        goto bail;
    }
    if (foundCount != numEntries) {
        /* not significant; just means somebody isn't updating correctly */
        debug_print(" ProDOS WARNING: numEntries=%d foundCount=%d in base='%s'",
                    numEntries, foundCount, basePath);
    }

bail:
    return dierr;
}

static char NameToLower(char ch)
{
    if (ch == '.')
        return ' ';
    else
        return tolower(ch);
}


static void GenerateLowerCaseName(const char* upperName,
                                         char* lowerName, uint16_t lcFlags)
{
    int nameLen = (int)strlen(upperName);
    int bit;
    assert(nameLen <= kMaxFileName);

    /* handle lower-case conversion; see GS/OS tech note #8 */
    if (lcFlags != 0 && !(lcFlags & 0x8000)) {
        // Should be zero or 0x8000 plus other bits; shouldn't be
        //  bunch of bits without 0x8000 or 0x8000 by itself.  Not
        //  really a problem, just unexpected.
        memcpy(lowerName, upperName, kMaxFileName);
        return;
    }
    for (bit = 0; bit < nameLen; bit++) {
        lcFlags <<= 1;
        if ((lcFlags & 0x8000) != 0)
            lowerName[bit] = NameToLower(upperName[bit]);
        else
            lowerName[bit] = upperName[bit];
    }
    for ( ; bit < kMaxFileName; bit++)
        lowerName[bit] = '\0';
}

static void InitDirEntry(DirEntry* pEntry,
                                const uint8_t* entryBuf)
{
    int nameLen;

    pEntry->storageType = (entryBuf[0x00] & 0xf0) >> 4;
    nameLen = entryBuf[0x00] & 0x0f;
    memcpy(pEntry->fileName, &entryBuf[0x01], nameLen);
    pEntry->fileName[nameLen] = '\0';
    pEntry->keyPointer = GetShortLE(&entryBuf[0x11]);
    pEntry->blocksUsed = GetShortLE(&entryBuf[0x13]);
    pEntry->eof = GetLongLE(&entryBuf[0x15]);
    pEntry->eof &= 0x00ffffff;
    pEntry->version = entryBuf[0x1c];
    pEntry->minVersion = entryBuf[0x1d];
    pEntry->access = entryBuf[0x1e];
    pEntry->auxType = GetShortLE(&entryBuf[0x1f]);
    pEntry->headerPointer = GetShortLE(&entryBuf[0x25]);

    /* generate the name into the buffer; does not null-terminate */
    if (pEntry->minVersion & 0x80) {
        GenerateLowerCaseName(pEntry->fileName, pEntry->fileName,
                                            GetShortLE(&entryBuf[0x1c]));
    }
    pEntry->fileName[sizeof(pEntry->fileName)-1] = '\0';
}

/*
 * Set the full pathname to a combination of the base path and the
 * current file's name.
 *
 * If we're in the volume directory, pass in "" for the base path (not NULL).
 */
static void SetPathName(A2File *pFile, const char* basePath, const char* fileName)
{
    assert(basePath != NULL && fileName != NULL);
    if (pFile->pathName != NULL)
        free(pFile->pathName);

    size_t baseLen = strlen(basePath);
    size_t fileNameLen = strlen(fileName);
    pFile->pathName = MemAlloc(baseLen + 1 + fileNameLen + 1);
    strncpy(pFile->pathName, basePath, baseLen + 1);
    if (baseLen != 0 &&
        !(baseLen == 1 && basePath[0] == ':'))
    {
        *(pFile->pathName + baseLen) = kFssep;
        baseLen++;
    }
    strncpy(pFile->pathName + baseLen, fileName, baseLen + 1 + fileNameLen + 1);
}


/*
 * Slurp the entries out of a single ProDOS directory block.
 *
 * Recursively calls RecursiveDirAdd for directories.
 *
 * "*pFound" is increased by the number of valid entries found in this block.
 */
static DIError SlurpEntries(A2File* pParent, const DirHeader* pHeader,
                                   const uint8_t* blkBuf, int skipFirst, int* pCount,
                                   const char* basePath, uint16_t thisBlock, int depth)
{
    DIError dierr = kDIErrNone;
    int entriesThisBlock = pHeader->entriesPerBlock;
    const uint8_t* entryBuf;
    A2File* pFile;

    int idx = 0;
    entryBuf = &blkBuf[0x04];
    if (skipFirst) {
        entriesThisBlock--;
        entryBuf += pHeader->entryLength;
        idx++;
    }

    for ( ; entriesThisBlock > 0 ;
         entriesThisBlock--, idx++, entryBuf += pHeader->entryLength)
    {
        if (entryBuf >= blkBuf + kBlkSize) {
            debug_print("  ProDOS whoops, just walked out of dirent buffer");
            return kDIErrBadDirectory;
        }

        if ((entryBuf[0x00] & 0xf0) == kStorageDeleted) {
            /* skip deleted entries */
            continue;
        }

        pFile = MemAlloc(sizeof(A2File));
        if (pFile == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        }
        memset(pFile, 0, sizeof(A2File));
        pFile->tsList = NULL;

        DirEntry* pEntry;
        pEntry = &pFile->fDirEntry;
        InitDirEntry(pEntry, entryBuf);

        SetPathName(pFile, basePath, pEntry->fileName);

        if (pEntry->keyPointer <= kVolHeaderBlock) {
            debug_print("ProDOS invalid key pointer %d on '%s'",
                 pEntry->keyPointer, pFile->pathName);
        }

        AddFileToList(pFile);
        (*pCount)++;

        if (pEntry->storageType == kStorageDirectory) {
            // don't need to check for kStorageVolumeDirHeader here
            dierr = RecursiveDirAdd(pFile, pEntry->keyPointer,
                                    pFile->pathName, depth+1);
            if (dierr != kDIErrNone) {
                if (dierr == kDIErrCancelled)
                    goto bail;

                dierr = kDIErrNone;
            }
        }
    }

bail:
    return dierr;
}

/*
 * Set the volume ID field.
 */
static void SetVolumeID(void)
{
    snprintf(fVolumeID, 31, "ProDOS /%s", fVolumeName);
}

static DIError DetermineVolDirLen(uint16_t nextBlock, uint16_t* pBlocksUsed) {
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    uint16_t blocksUsed = 1;
    int iterCount = 0;

    // Traverse the volume directory chain, counting blocks.  Normally this will have 4, but
    // variations are possible.
    while (nextBlock != 0) {
        blocksUsed++;

        if (nextBlock < 2 || nextBlock >= fNumBlocks) {
            debug_print(" ProDOS ERROR: invalid volume dir link block %u", nextBlock);
            dierr = kDIErrInvalidBlock;
            goto bail;
        }
        dierr = ReadBlockSwapped(nextBlock, blkBuf, kSectorOrderPhysical, kSectorOrderProDOS);
        if (dierr != kDIErrNone) {
            goto bail;
        }

        nextBlock = GetShortLE(&blkBuf[0x02]);

        // Watch for infinite loop.
        iterCount++;
        if (iterCount > fNumBlocks) {
            debug_print(" ProDOS ERROR: infinite vol directory loop found");
            dierr = kDIErrDirectoryLoop;
            goto bail;
        }
    }

bail:
    *pBlocksUsed = blocksUsed;
    return dierr;
}


/*
 * Read some interesting fields from the volume header.
 *
 * The "test" function verified certain things, e.g. the storage type
 * is $f and the volume name length is nonzero.
 */
static DIError LoadVolHeaderProDOS(void)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    int nameLen;

    A2File* pFile = NULL;

    dierr = ReadBlockSwapped(kVolHeaderBlock, blkBuf, kSectorOrderPhysical, kSectorOrderProDOS);
    if (dierr != kDIErrNone)
        goto bail;

    nameLen = blkBuf[0x04] & 0x0f;
    memcpy(fVolumeName, &blkBuf[0x05], nameLen);
    fVolumeName[nameLen] = '\0';

    fBitMapPointer = GetShortLE(&blkBuf[0x27]);
    fTotalBlocks = GetShortLE(&blkBuf[0x29]);

    if (blkBuf[0x1b] & 0x80) {
        /*
         * Handle lower-case conversion; see GS/OS tech note #8.  Unlike
         * filenames, volume names are not allowed to contain spaces.  If
         * they try it we just ignore them.
         *
         * Technote 8 doesn't actually talk about volume names.  By
         * experimentation the field was discovered at offset 0x1a from
         * the start of the block, which is marked as "reserved" in Beneath
         * Apple ProDOS.
         */
        uint16_t lcFlags = GetShortLE(&blkBuf[0x1a]);

        GenerateLowerCaseName(fVolumeName, fVolumeName, lcFlags);
    }

    if (fTotalBlocks <= kVolHeaderBlock) {
        /* incr to min; don't use max, or bitmap count may be too large */
        debug_print(" ProDOS found tiny fTotalBlocks (%d), increasing to minimum",
             fTotalBlocks);
        fTotalBlocks = kMinReasonableBlocks;
    } else if (fTotalBlocks != fNumBlocks) {
        if (fTotalBlocks != 65535 || fNumBlocks != 65536) {
            debug_print(" ProDOS WARNING: total (%u) != img (%ld)",
                 fTotalBlocks, fNumBlocks);
        }

        /*
         * For safety (esp. vol bitmap read), constrain fTotalBlocks.  We might
         * consider not doing this for ".hdv", which can start small and then
         * expand as files are added.  (Check "fExpanded".)
         */
        if (fTotalBlocks > fNumBlocks) {
            fTotalBlocks = (uint16_t) fNumBlocks;
        }
    }

    /*
     * Test for funky volume bitmap pointer.  Some disks (e.g. /RAM and
     * ProSel-16) truncate the volume directory to eke a little more storage
     * out of a disk.  There's nothing wrong with that, but we don't want to
     * try to use a volume bitmap pointer of zero or 0xffff, because it's
     * probably garbage.
     */
    if (fBitMapPointer != kExpectedBitmapStart) {
        if (fBitMapPointer <= kVolHeaderBlock ||
            fBitMapPointer > kExpectedBitmapStart)
        {
            fBitMapPointer = 6;     // just fix it and hope for the best
        }
    }

    SetVolumeID();

    /*
     * Create a "magic" directory entry for the volume directory.
     *
     * Normally these values are pulled out of the file entry in the parent
     * directory.  Here, we synthesize them from the volume dir header.
     */
    pFile = MemAlloc(sizeof(A2File));
    if (pFile == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }
    memset(pFile, 0, sizeof(A2File));
    pFile->tsList = NULL;

    DirEntry* pEntry;
    pEntry = &pFile->fDirEntry;

    int foundStorage;
    foundStorage = (blkBuf[0x04] & 0xf0) >> 4;
    if (foundStorage != kStorageVolumeDirHeader) {
        debug_print(" ProDOS WARNING: unexpected vol dir file type %d",
             pEntry->storageType);
        /* keep going */
    }
    pEntry->storageType = kStorageVolumeDirHeader;
    strncpy(pEntry->fileName, fVolumeName, sizeof(fVolumeName));
    pFile->pathName = MemAlloc(nameLen + 2);
    pFile->pathName[0] = ':';
    memcpy(pFile->pathName + 1, pEntry->fileName, nameLen);
    pEntry->fileName[nameLen] = '\0';
    pEntry->keyPointer = kVolHeaderBlock;
    dierr = DetermineVolDirLen(GetShortLE(&blkBuf[0x02]), &pEntry->blocksUsed);
    if (dierr != kDIErrNone) {
        goto bail;
    }
    pEntry->eof = pEntry->blocksUsed * 512;
    pEntry->version = blkBuf[0x20];
    pEntry->minVersion = blkBuf[0x21];
    pEntry->access = blkBuf[0x22];
    pEntry->auxType = 0;
    pEntry->headerPointer = 0;

    AddFileToList(pFile);
    pFile = NULL;

bail:
    if (pFile)
        free(pFile);
    return dierr;
}

/*
 * Read some fields from the disk Volume Table of Contents.
 */
static DIError ReadVTOC(void)
{
    DIError dierr;

    dierr = ReadTrackSector(kVTOCTrack, kVTOCSector, kSectorOrderPhysical, kSectorOrderDOS, fVTOC);
    if (dierr != kDIErrNone)
        goto bail;

    fFirstCatTrack = fVTOC[0x01];
    fFirstCatSector = fVTOC[0x02];
    fVTOCVolumeNumber = fVTOC[0x06];
    fVTOCNumTracks = fVTOC[0x34];
    fVTOCNumSectors = fVTOC[0x35];

    if (fFirstCatTrack >= kNumTracks)
        return kDIErrBadDiskImage;
    if (fFirstCatSector >= kNumSectPerTrack)
        return kDIErrBadDiskImage;

    if (fVTOCNumTracks != kNumTracks) {
        debug_print(" DOS33 warning: VTOC numtracks %d vs %d\n",
            fVTOCNumTracks, kNumTracks);
    }
    if (fVTOCNumSectors != kNumSectPerTrack) {
        debug_print(" DOS33 warning: VTOC numsect %d vs %d\n",
            fVTOCNumSectors, kNumSectPerTrack);
    }

bail:
    return dierr;
}

/*
 * Read the disk's catalog.
 *
 * NOTE: supposedly DOS stops reading the catalog track when it finds the
 * first entry with a 00 byte, which is why deleted files use ff.  If so,
 * it *might* make sense to mimic this behavior, though on a health disk
 * we shouldn't be finding garbage anyway.
 *
 * Fills out "fCatalogSectors" as it works.
 */
static DIError ReadCatalog(void)
{
    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSectorSize];
    int catTrack, catSect;
    int iterations;

    catTrack = fFirstCatTrack;
    catSect = fFirstCatSector;
    iterations = 0;

    memset(fCatalogSectors, 0, sizeof(fCatalogSectors));

    while (catTrack != 0 && catSect != 0 && iterations < kMaxCatalogSectors) {
        dierr = ReadTrackSector(catTrack, catSect, kSectorOrderPhysical, kSectorOrderDOS, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        /*
         * Watch for flaws that the DOS detector allows.
         */
        if (catTrack == sctBuf[0x01] && catSect == sctBuf[0x02]) {
            debug_print(" DOS detected self-reference on cat (%d,%d)\n",
                catTrack, catSect);
            break;
        }

        /*
         * Check the next track/sector in the chain.  If the pointer is
         * broken, there's a very good chance that this isn't really a
         * catalog sector, so we want to bail out now.
         */
        if (sctBuf[0x01] >= kNumTracks || sctBuf[0x02] >= kNumSectPerTrack) {
            debug_print(" DOS bailing out early on catalog read due to funky T/S\n");
            break;
        }

        dierr = ProcessCatalogSector(catTrack, catSect, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        fCatalogSectors[iterations].track = catTrack;
        fCatalogSectors[iterations].sector = catSect;

        catTrack = sctBuf[0x01];
        catSect = sctBuf[0x02];

        iterations++; // watch for infinite loops
    }
    if (iterations >= kMaxCatalogSectors) {
        dierr = kDIErrDirectoryLoop;
        goto bail;
    }

bail:
    return dierr;
}

static A2File *find_file_named(char *name)
{
    debug_print("find_file_named: \"%s\"\n", name);
    A2File *file = firstfile;
    while (file != NULL) {
        debug_print("Comparing to file named: \"%s\"\n", (char *)file->fFileName);
        if (strcmp(name, (char *)file->fFileName) == 0)
            break;
        file = file->next;
    }
    return file;
}

static A2File *find_SAGA_database(void)
{
    A2File *file = firstfile;
    while (file != NULL) {
        char *name = (char *)file->fFileName;
        debug_print("Comparing to file named: \"%s\"\n", name);
        if (strcmp("DATABASE", name) == 0)
            break;
        if (strcmp("SORCEROR OF CLAYMORGUE CASTLE", name) == 0)
            break;
        if (strcmp("THE INCREDIBLE HULK", name) == 0)
            break;
        if (name[0] == 'A' && name[2] == '.' && name[3] == 'D' && name[4] == 'A' && name[5] == 'T') {
            break; //A(adventure number).DAT
        }
        file = file->next;
    }
    return file;
}

uint8_t *ReadApple2DOSFile(uint8_t *data, size_t *len, uint8_t **invimg, size_t *invimglen, uint8_t **m2)
{
    rawdata = data;
    rawdatalen = *len;

    uint8_t *buf = NULL;
    ReadVTOC();
    ReadCatalog();
    A2File *file = find_SAGA_database();
    if (file) {
        Open(file);
        buf = MemAlloc(file->fLengthInSectors * kSectorSize);
        size_t actual;
        Read(file, buf, (file->fLengthInSectors - 1) * kSectorSize, &actual);
        *len = actual;
    }
    /* Many games have the inventory image on the boot disk (and the rest of the images on the other disk) */
    file = find_file_named("PAK.INVEN");
    if (!file) {
        file = find_file_named("PAC.INVEN");
    }
    if (file) {
        Open(file);
        uint8_t *inventemp = MemAlloc(file->fLengthInSectors * kSectorSize);
        debug_print("inventemp is size %d\n", file->fLengthInSectors * kSectorSize);
        Read(file, inventemp, (file->fLengthInSectors - 1) * kSectorSize, invimglen);
        debug_print("*invimglen is %zu\n", *invimglen);
        debug_print("*invimglen - 4 is %zu\n", *invimglen);
        *invimg = MemAlloc(*invimglen);
        if (*invimglen < 4 || *invimglen > 100000) {
            *invimglen = 0;
            *invimg = NULL;
        } else {
            memcpy(*invimg, inventemp + 4, *invimglen - 4);
        }
        free(inventemp);
    }

    /* Image unscrambling tables are usually found somewhere in a file named M2 (As in memory part 2?) */
    file = find_file_named("M2");
    if (file) {
        Open(file);
        uint8_t *m2temp = MemAlloc(file->fLengthInSectors * kSectorSize);
        size_t m2len;
        Read(file, m2temp, (file->fLengthInSectors - 1) * kSectorSize, &m2len);

        if (m2len > 0x1747 + 0x180 && strncmp((char *)(m2temp + 0x172C), "COPYRIGHT 1983 NORMAN L. SAILER", 31) == 0) {
            *m2 = MemAlloc(0x182);
            memcpy(*m2, m2temp + 0x174B, 0x182);
        }
        free(m2temp);
    }

    return buf;
}

A2File* GetNextFile(A2File *pFile)
{
    if (pFile == NULL)
        return firstfile;
    else
        return pFile->next;
}

/*
 * Get things rolling.
 *
 * Since we're assured that this is a valid disk, errors encountered from here
 * on out must be handled somehow, possibly by claiming that the disk has
 * no files on it.
 */
DIError InitializeProDOS(InitMode initMode)
{
    DIError dierr = kDIErrNone;

    fHasNibbles = fHasSectors = 1;
    fNumTracks = kTrackCount525;
    fNumSectPerTrack = fpNibbleDescr->numSectors;

    /*
     * Compute the number of blocks.  For a 13-sector disk, block access
     * is not possible.
     *
     * For nibble formats, we have to base the block count on the number
     * of sectors rather than the file length.
     */
    assert(fNumSectPerTrack > 0);

    if ((fNumSectPerTrack & 0x01) == 0) {
        /* not a 13-sector disk, so define blocks in terms of sectors */
        /* (effects of sector pairing are already taken into account) */
        fHasBlocks = 1;
        fNumBlocks = (fNumTracks * fNumSectPerTrack) / 2;
    }

    dierr = LoadVolHeaderProDOS();
    if (dierr != kDIErrNone)
        goto bail;

    /* volume dir is guaranteed to come first; if not, we need a lookup func */
    A2File* pVolumeDir = GetNextFile(NULL);

    dierr = RecursiveDirAdd(pVolumeDir, kVolHeaderBlock, "", 0);
    if (dierr != kDIErrNone) {
        debug_print(" ProDOS RecursiveDirAdd failed");
        goto bail;
    }

bail:
    return dierr;
}

const char *titles[] = { "ZORK0", "SHOGUN", "JOURNEY", "ARTHUR", NULL };

uint8_t *ReadInfocomV6File(uint8_t *data, size_t *len, int *game, int *diskindex)
{
    rawdata = data;
    rawdatalen = *len;

    uint8_t *buf = NULL;

    DIError dierr = InitializeProDOS(kInitFull);

    if (dierr != kDIErrNone) {
        fprintf(stderr, "Error reading list of files from disk\n");
        goto bail;
    }

    int index = -1;

    int i;
    for (i = 0; titles[i]; i++) {
        size_t length = strlen (titles[i]);
        if (!strncmp (fVolumeName, titles[i], length)) {
            index = fVolumeName[length + 1] - '0';
            break;
        }
    }
    if (i > 3 || index < 1 || index > 5) {
        fprintf(stderr, "Not a recoginized Apple 2 Infocom V6 game!\n");
        goto bail;
    }

    if (game)
        *game = i;
    if (diskindex)
        *diskindex = index;

    char filename[64];
    snprintf(filename, 64, "%s.D%d", titles[i], index);

    A2File *file = GetFileByName(firstfile, filename);
    if (file) {
        if (ProDOSOpen(file) != kDIErrNone) {
            fprintf(stderr, "ProDOSOpen returned error\n");
        }
        buf = MemAlloc(file->fDirEntry.blocksUsed * kBlkSize);
        size_t actual;
        if (ProDOSRead(file, buf, file->fDirEntry.blocksUsed * kBlkSize - 1, &actual) != kDIErrNone) {
            fprintf(stderr, "ProDOSRead returned error\n");
        }
        *len = actual;
    }

bail:
    FreeDiskImage();
    return buf;
}
