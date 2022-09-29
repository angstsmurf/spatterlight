//
//  readnib.c
//  scott
//
//  Created by Administrator on 2022-09-24.
//
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "readnib.h"

#define DEBUG_PRINT 0

//#define NIB_VERBOSE_DEBUG

#define debug_print(fmt, ...) \
do { if (DEBUG_PRINT) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

/*
 * Errors from the various disk image classes.
 */
typedef enum DIError {
    kDIErrNone                  = 0,

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

    kDIErrNufxLibInitFailed     = -110,
} DIError;

struct ringbuf_t {
    uint8_t **buffer;
    size_t size; //of the buffer
    int initialized;
};

// Opaque circular buffer structure
typedef struct ringbuf_t ringbuf_t;
// Handle type, the way users interact with the API
typedef ringbuf_t* ringbuf_handle_t;

uint8_t access_ringbuf(ringbuf_handle_t ringbuf, int index) {
    if (ringbuf == NULL || ringbuf->initialized == 0 || ringbuf->buffer == NULL || *(ringbuf->buffer) == NULL) {
        debug_print("ERROR! Ringbuf not ready!\n");
        return -1;
    }

    if (index >= ringbuf->size)
        index -= ringbuf->size;
    if (index < 0)
        index = ringbuf->size - index;
    uint8_t *buffer = *(ringbuf->buffer);

    return buffer[index];
}

/*
 * Nibble encode/decode description.  Use no pointers here, so we
 * store as an array and resize at will.
 *
 * Should we define an enum to describe whether address and data
 * headers are standard or some wacky variant?
 */
enum {
    kNibbleAddrPrologLen = 3,       // d5 aa 96
    kNibbleAddrEpilogLen = 3,       // de aa eb
    kNibbleDataPrologLen = 3,       // d5 aa ad
    kNibbleDataEpilogLen = 3,       // de aa eb
};

typedef struct {
    char            description[32];
    short           numSectors;     // 13 or 16 (or 18?)

    uint8_t         addrProlog[kNibbleAddrPrologLen];
    uint8_t         addrEpilog[kNibbleAddrEpilogLen];
    uint8_t         addrChecksumSeed;
    int             addrVerifyChecksum;
    int             addrVerifyTrack;
    int             addrEpilogVerifyCount;

    uint8_t         dataProlog[kNibbleDataPrologLen];
    uint8_t         dataEpilog[kNibbleDataEpilogLen];
    uint8_t         dataChecksumSeed;
    int             dataVerifyChecksum;
    int             dataEpilogVerifyCount;
} NibbleDescr;


/*
 * 5.25" nibble image access.
 */
#define kChunkSize62 86      // (0x56)


/*static*/ uint8_t kDiskBytes53[32] = {
    0xab, 0xad, 0xae, 0xaf, 0xb5, 0xb6, 0xb7, 0xba,
    0xbb, 0xbd, 0xbe, 0xbf, 0xd6, 0xd7, 0xda, 0xdb,
    0xdd, 0xde, 0xdf, 0xea, 0xeb, 0xed, 0xee, 0xef,
    0xf5, 0xf6, 0xf7, 0xfa, 0xfb, 0xfd, 0xfe, 0xff
};
/*static*/ uint8_t kDiskBytes62[64] = {
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
static uint8_t kInvDiskBytes53[256];
static uint8_t kInvDiskBytes62[256];
enum { kInvInvalidValue = 0xff };

/*
 * Compute tables to convert disk bytes back to values.
 *
 * Should be called once, at DLL initialization time.
 */
/*static*/ void CalcNibbleInvTables(void)
{
    unsigned int i;

    memset(kInvDiskBytes53, kInvInvalidValue, sizeof(kInvDiskBytes53));
    for (i = 0; i < sizeof(kDiskBytes53); i++) {
        assert(kDiskBytes53[i] >= 0x96);
        kInvDiskBytes53[kDiskBytes53[i]] = i;
    }

    memset(kInvDiskBytes62, kInvInvalidValue, sizeof(kInvDiskBytes62));
    for (i = 0; i < sizeof(kDiskBytes62); i++) {
        assert(kDiskBytes62[i] >= 0x96);
        kInvDiskBytes62[kDiskBytes62[i]] = i;
    }
}

uint16_t ConvFrom44(uint8_t val1, uint8_t val2) {
    return ((val1 << 1) | 0x01) & val2;
}

/*
 * Decode the values in the address field.
 */
void DecodeAddr(ringbuf_handle_t ringbuffer, int offset,
                         short* pVol, short* pTrack, short* pSector, short* pChksum)
{
    //unsigned int vol, track, sector, chksum;

    *pVol = ConvFrom44(access_ringbuf(ringbuffer, offset), access_ringbuf(ringbuffer, offset+1));
    *pTrack = ConvFrom44(access_ringbuf(ringbuffer, offset+2), access_ringbuf(ringbuffer, offset+3));
    *pSector = ConvFrom44(access_ringbuf(ringbuffer, offset+4), access_ringbuf(ringbuffer, offset+5));
    *pChksum = ConvFrom44(access_ringbuf(ringbuffer, offset+6), access_ringbuf(ringbuffer, offset+7));
}


/*
 * Find the start of the data field of a sector in nibblized data.
 *
 * Returns the index start on success or -1 on failure.
 */
int  FindNibbleSectorStart(ringbuf_handle_t ringbuffer, int track,
                           int sector, const NibbleDescr* pNibbleDescr, int* pVol)
{
                                        //DIError dierr;
    long trackLen = ringbuffer->size;
    const int kMaxDataReach = trackLen;       // fairly arbitrary

    assert(sector >= 0 && sector < 16);

    int i;

    for (i = 0; i < trackLen; i++) {
        int foundAddr = 0;

            if (access_ringbuf(ringbuffer, i) == pNibbleDescr->addrProlog[0] &&
                access_ringbuf(ringbuffer, i+1) == pNibbleDescr->addrProlog[1] &&
                access_ringbuf(ringbuffer, i+2) == pNibbleDescr->addrProlog[2])
            {
                foundAddr = 1;
            }


        if (foundAddr) {
            //i += 3;

            /* found the address header, decode the address */
            short hdrVol, hdrTrack, hdrSector, hdrChksum;
            DecodeAddr(ringbuffer, i+3, &hdrVol, &hdrTrack, &hdrSector,
                       &hdrChksum);

            if (pNibbleDescr->addrVerifyTrack && track != hdrTrack) {
                debug_print("Track mismatch");
                debug_print("  Track mismatch (T=%d) got T=%d,S=%d", track, hdrTrack, hdrSector);
                continue;
            }

            if (pNibbleDescr->addrVerifyChecksum) {
                if ((pNibbleDescr->addrChecksumSeed ^
                     hdrVol ^ hdrTrack ^ hdrSector ^ hdrChksum) != 0)
                {
                    debug_print("   Addr checksum mismatch (want T=%d,S=%d, got T=%d,S=%d)", track, sector, hdrTrack, hdrSector);
                    continue;
                }
            }

            i += 3;

            int j;
            for (j = 0; j < pNibbleDescr->addrEpilogVerifyCount; j++) {
                if (access_ringbuf(ringbuffer, i+8+j) != pNibbleDescr->addrEpilog[j]) {
                    //debug_print("   Bad epilog byte %d (%02x vs %02x)",
                    //    j, buffer[i+8+j], pNibbleDescr->addrEpilog[j]);
                    break;
                }
            }
            if (j != pNibbleDescr->addrEpilogVerifyCount)
                continue;

#ifdef NIB_VERBOSE_DEBUG
            debug_print("    Good header, T=%d,S=%d (looking for T=%d,S=%d)",
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
                if (access_ringbuf(ringbuffer, i + j) == pNibbleDescr->dataProlog[0] &&
                    access_ringbuf(ringbuffer, i + j + 1) == pNibbleDescr->dataProlog[1] &&
                    access_ringbuf(ringbuffer, i + j + 2) == pNibbleDescr->dataProlog[2])
                {
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
DIError DecodeNibbleData(ringbuf_handle_t ringbuffer, int idx,
                       uint8_t* sctBuf, const NibbleDescr* pNibbleDescr)
{
    uint8_t twos[kChunkSize62 * 3];   // 258
    int chksum = pNibbleDescr->dataChecksumSeed;
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
        twos[i] =
        ((chksum & 0x01) << 1) | ((chksum & 0x02) >> 1);
        twos[i + kChunkSize62] =
        ((chksum & 0x04) >> 1) | ((chksum & 0x08) >> 3);
        twos[i + kChunkSize62*2] =
        ((chksum & 0x10) >> 3) | ((chksum & 0x20) >> 5);
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

    if (pNibbleDescr->dataVerifyChecksum && chksum != 0) {
        debug_print("    NIB bad data checksum");
        return kDIErrBadChecksum;
    }
    return kDIErrNone;
}

uint8_t *fNibbleTrackBuf = NULL;    // allocated on heap
#define kMaxNibbleTracks525 40 // max #of tracks on 5.25 nibble img
int fNibbleTrackLoaded = -1; // track currently in buffer
#define kTrackAllocSize 6656   // max 5.25 nibble track len; for buffers

/* exact definition of off_t varies, so just define our own */
#ifdef _ULONGLONG_
typedef LONGLONG di_off_t;
#else
typedef off_t di_off_t;
#endif


uint8_t *rawdata = NULL;
size_t rawdatalen = 0;

#define kNumTracks 35
#define kNumSectPerTrack 16
#define kSectorSize 256

/*
 * Copy a chunk of bytes out of the disk image.
 *
 * (This is the lowest-level read routine in this class.)
 */
static DIError CopyBytesOut(void *buf, di_off_t offset, int size)
{
    if (offset + size > rawdatalen)
        return kDIErrDataUnderrun;
    memcpy(buf, rawdata + offset, size);
    return kDIErrNone;
}

/*
 * Handle sector order conversions.
 */
DIError CalcSectorAndOffset(long track, int sector, di_off_t* pOffset, int* pNewSector)
{
    /*
     * Sector order conversions.  No table is needed for Copy ][+ format,
     * which is equivalent to "physical".
     */

    static const int dos2raw[16] = {
        0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
    };


    if (track < 0 || track >= kNumTracks) {
        debug_print(" DI read invalid track %ld", track);
        return kDIErrInvalidTrack;
    }
    if (sector < 0 || sector >= kNumSectPerTrack) {
        debug_print(" DI read invalid sector %d", sector);
        return kDIErrInvalidSector;
    }

    di_off_t offset;
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

    newSector = dos2raw[sector];

    offset += newSector * kSectorSize;

    *pOffset = offset;
    *pNewSector = newSector;
    return kDIErrNone;
}


typedef enum {              // format of the image data stream
    kPhysicalFormatUnknown = 0,
    kPhysicalFormatSectors = 1, // sequential 256-byte sectors (13/16/32)
    kPhysicalFormatNib525_6656 = 2, // 5.25" disk ".nib" (6656 bytes/track)
    kPhysicalFormatNib525_Var = 4,  // 5.25" disk (variable len, e.g. ".app")
} PhysicalFormat;

const int kTrackLenNib525 = 6656;
const int kTrackLenNb2525 = 6384;

PhysicalFormat physical = kPhysicalFormatNib525_6656;

/*
 * Some additional goodies required for accessing variable-length nibble
 * tracks in TrackStar images.  A default implementation is provided and
 * used for everything but TrackStar.
 */
int GetNibbleTrackLength(PhysicalFormat physical, int track)
{
    if (physical == kPhysicalFormatNib525_6656)
        return kTrackLenNib525;
    else {
        assert(0);
        return -1;
    }
}

/*
 * Load a nibble track into our track buffer.
 */
DIError LoadNibbleTrack(long track, long* pTrackLen)
{
    DIError dierr = kDIErrNone;
    long offset;
    assert(track >= 0 && track < kMaxNibbleTracks525);

    *pTrackLen = GetNibbleTrackLength(physical, track);
    offset = GetNibbleTrackLength(physical, 0) * track;
    assert(*pTrackLen > 0);
    assert(offset >= 0);

    fprintf(stderr, "offset of track %ld: %lx\n", track, offset);

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
        fNibbleTrackBuf = malloc(kTrackAllocSize);
        if (fNibbleTrackBuf == NULL)
            return kDIErrMalloc;
    }

    /*
     * Read the entire track into memory.
     */
    dierr = CopyBytesOut(fNibbleTrackBuf, offset, *pTrackLen);
    if (dierr != kDIErrNone)
        return dierr;

    fNibbleTrackLoaded = track;

    return dierr;
}

/*
 * Get the contents of the nibble track.
 *
 * "buf" must be able to hold kTrackAllocSize bytes.
 */
DIError ReadNibbleTrack(long track, uint8_t* buf, long* pTrackLen)
{
    DIError dierr;

    dierr = LoadNibbleTrack(track, pTrackLen);
    if (dierr != kDIErrNone) {
        debug_print("   DI ReadNibbleTrack: LoadNibbleTrack %ld failed", track);
        return dierr;
    }

    memcpy(buf, fNibbleTrackBuf, *pTrackLen);
    return kDIErrNone;
}

#define kNumTracks 35

/*
 * Read a sector from a nibble image.
 *
 * While fNumTracks is valid, fNumSectPerTrack is a little flaky, because
 * in theory each track could be formatted differently.
 */
DIError ReadNibbleSector(long track, int sector, uint8_t *buf,
                         const NibbleDescr *pNibbleDescr)
{
    if (pNibbleDescr == NULL) {
        /* disk has no recognizable sectors */
        debug_print(" DI ReadNibbleSector: pNibbleDescr is NULL, returning failure");
        return kDIErrBadNibbleSectors;
    }
    if (sector >= pNibbleDescr->numSectors) {
        /* e.g. trying to read sector 14 on a 13-sector disk */
        debug_print(" DI ReadNibbleSector: bad sector number request");
        return kDIErrInvalidSector;
    }

    assert(pNibbleDescr != NULL);
//    assert(IsNibbleFormat(fPhysical));
    assert(track >= 0 && track < kNumTracks);
    assert(sector >= 0 && sector < pNibbleDescr->numSectors);

    DIError dierr = kDIErrNone;
    long trackLen;
    int sectorIdx, vol;

    dierr = LoadNibbleTrack(track, &trackLen);
    if (dierr != kDIErrNone) {
        debug_print("   DI ReadNibbleSector: LoadNibbleTrack %ld failed", track);
        return dierr;
    }

    ringbuf_t circularbuf;
    ringbuf_handle_t ringbuffer = &circularbuf; //(fNibbleTrackBuf, trackLen);
            ringbuffer->size = trackLen;
            ringbuffer->buffer = &fNibbleTrackBuf;
            ringbuffer->initialized = 1;

    sectorIdx = FindNibbleSectorStart(ringbuffer, track, sector, pNibbleDescr,
                                      &vol);
    if (sectorIdx < 0) {
        return kDIErrSectorUnreadable;
    }

    dierr = DecodeNibbleData(ringbuffer, sectorIdx, buf,
                             pNibbleDescr);

    return dierr;
}

size_t writeToFile(const char *name, uint8_t *data, size_t size);


#define MIN(a, b) ((a) < (b) ? (a) : (b))

uint8_t *ReadImageFromNib(size_t offset, size_t size, uint8_t *data, size_t datasize) {
    uint8_t *result = malloc(size);

    CalcNibbleInvTables();

    rawdata = data;
    rawdatalen = datasize;

    uint8_t buf[0x100];

    NibbleDescr nibbledesc = {
        "DOS 3.3 Standard",
        16, // number of sectors
        { 0xd5, 0xaa, 0x96 }, // addrProlog
        { 0xde, 0xaa, 0xeb }, // addrEpilog
        0x00,   // checksum seed
        0,   // verify checksum
        0,   // verify track
        2,      // epilog verify count
        { 0xd5, 0xaa, 0xad }, // dataProlog
        { 0xde, 0xaa, 0xeb }, // dataEpilog
        0x00,   // checksum seed
        0,   // verify checksum
        2,      // epilog verify count
    };

    di_off_t pOffset;
    int pNewSector;
    int track = offset / 0x1000;
    int sector = (offset % 0x1000) / 0x100;

    for (int remaining = (int)size; remaining > 0; remaining -= 0x100) {
        DIError error = CalcSectorAndOffset(track, sector, &pOffset, &pNewSector);
        error = ReadNibbleSector(track, pNewSector, buf, &nibbledesc);
        sector++;
        if (sector > 15) {
            sector = 0;
            track++;
        }
        memcpy(result + (size - remaining), buf, MIN(0x100, remaining));
    }

    return result;
}

//
//{
//    "DOS 3.3 Standard",
//    16,
//    { 0xd5, 0xaa, 0x96 }, { 0xde, 0xaa, 0xeb },
//    0x00,   // checksum seed
//    true,   // verify checksum
//    true,   // verify track
//    2,      // epilog verify count
//    { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
//    0x00,   // checksum seed
//    true,   // verify checksum
//    2,      // epilog verify count
//    kNibbleEnc62,
//    kNibbleSpecialNone,
//},
//{
//    "DOS 3.3 Patched",
//    16,
//    { 0xd5, 0xaa, 0x96 }, { 0xde, 0xaa, 0xeb },
//    0x00,   // checksum seed
//    false,  // verify checksum
//    false,  // verify track
//    0,      // epilog verify count
//    { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
//    0x00,   // checksum seed
//    true,   // verify checksum
//    0,      // epilog verify count
//    kNibbleEnc62,
//    kNibbleSpecialNone,
//},
//{
//    "DOS 3.3 Ignore Checksum",
//    16,
//    { 0xd5, 0xaa, 0x96 }, { 0xde, 0xaa, 0xeb },
//    0x00,   // checksum seed
//    false,  // verify checksum
//    false,  // verify track
//    0,      // epilog verify count
//    { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
//    0x00,   // checksum seed
//    false,  // verify checksum
//    0,      // epilog verify count
//    kNibbleEnc62,
//    kNibbleSpecialNone,
//},
//{
//    "DOS 3.2 Standard",
//    13,
//    { 0xd5, 0xaa, 0xb5 }, { 0xde, 0xaa, 0xeb },
//    0x00,
//    true,
//    true,
//    2,
//    { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
//    0x00,
//    true,
//    2,
//    kNibbleEnc53,
//    kNibbleSpecialNone,
//},
//{
//    "DOS 3.2 Patched",
//    13,
//    { 0xd5, 0xaa, 0xb5 }, { 0xde, 0xaa, 0xeb },
//    0x00,
//    false,
//    false,
//    0,
//    { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
//    0x00,
//    true,
//    0,
//    kNibbleEnc53,
//    kNibbleSpecialNone,
//    },
