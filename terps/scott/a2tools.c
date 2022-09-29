/*
    This is based on parts of the Ciderpress source,
    and a2tools by Terry Kyriacopoulos and Paul Schlyter
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define FILETYPE_T 0x00
#define FILETYPE_I 0x01
#define FILETYPE_A 0x02
#define FILETYPE_B 0x04
#define FILETYPE_S 0x08
#define FILETYPE_R 0x10
#define FILETYPE_X 0x20 /* "new A" */
#define FILETYPE_Y 0x40 /* "new B" */
#define FILETYPE_UNKNOWN 0x100


/*
 * ===========================================================================
 *      DiskFSDOS33
 * ===========================================================================
 */

const int kMaxSectors = 32;

const int kSctSize = 256;
/* do we need a way to override these? */
const int kVTOCTrack = 17;
const int kVTOCSector = 0;
const int kCatalogEntryOffset = 0x0b;   // first entry in cat sect starts here
const int kCatalogEntrySize = 0x23;     // length in bytes of catalog entries
const int kCatalogEntriesPerSect = 7;   // #of entries per catalog sector
const int kEntryDeleted = 0xff;     // this is used for track# of deleted files
const int kEntryUnused = 0x00;      // this is track# in never-used entries
const int kMaxTSPairs = 0x7a;           // 122 entries for 256-byte sectors
const int kTSOffset = 0x0c;             // first T/S entry in a T/S list

const int kMaxTSIterations = 32;
const int kMaxFileName = 30;
const int kFileNameBufLen = kMaxFileName + 1;


/*
* Errors from the various disk image classes.
*/
typedef enum DIError {
    kDIErrNone                  = 0,

    kDIErrDataUnderrun          = -33,  // tried to read off end of the image

    kDIErrInvalidTrack          = -50,  // request for invalid track number
    kDIErrInvalidSector         = -51,  // request for invalid sector number

    kDIErrDirectoryLoop         = -60,  // directory chain points into itself
    kDIErrFileLoop              = -61,  // file sector or block alloc loops
    kDIErrBadDiskImage          = -62,  // the FS on the disk image is damaged
    kDIErrBadFile               = -63,  // bad file on disk image

    /* higher-level errors */
    kDIErrMalloc                = -103,
    kDIErrInvalidArg            = -104,
} DIError;

/* exact definition of off_t varies, so just define our own */
#ifdef _ULONGLONG_
typedef LONGLONG di_off_t;
#else
typedef off_t di_off_t;
#endif


#define kMaxCatalogSectors 64    // two tracks on a 32-sector disk

static size_t fLength = 143360;

#define kNumTracks 35
#define kNumSectPerTrack 16 // Is this ever 13?

/*
 * ==========================================================================
 *      Block/track/sector I/O
 * ==========================================================================
 */

/*
 * Handle sector order conversions.
 */
static DIError CalcSectorAndOffset(long track, int sector, di_off_t *pOffset, int *pNewSector)
{
    if (track < 0 || track >= kNumTracks) {
        fprintf(stderr, "DI read invalid track %ld", track);
        return kDIErrInvalidTrack;
    }
    if (sector < 0 || sector >= kNumSectPerTrack) {
        fprintf(stderr, " DI read invalid sector %d", sector);
        return kDIErrInvalidSector;
    }

    di_off_t offset;
    int newSector = -1;

    /*
     * 16-sector disks write sectors in ascending order and then remap
     * them with a translation table.
     */
    if (kNumSectPerTrack == 16 || kNumSectPerTrack == 32) {
        offset = track * kNumSectPerTrack * kSctSize;
            if (sector >= 16) {
                offset += 16*kSctSize;
                sector -= 16;
            }
        assert(sector >= 0 && sector < 16);

    offset += sector * kSctSize;
    } else if (kNumSectPerTrack == 13) {
        /* sector skew has no meaning, so assume no translation */
        offset = track * kNumSectPerTrack * kSctSize;
        offset += sector * kSctSize;

    } else {
        /* try to do something reasonable */
        offset = (di_off_t)track * kNumSectPerTrack * kSctSize;
        offset += sector * kSctSize;
    }

    *pOffset = offset;
    *pNewSector = newSector;
    return kDIErrNone;
}

static uint8_t *rawdata = NULL;
static size_t rawdatalen = 0;

/*
 * Copy a chunk of bytes out of the disk image.
 *
 * (This is the lowest-level read routine in this class.)
 */
static DIError CopyBytesOut(void* buf, di_off_t offset, int size)
{
    memcpy(buf, rawdata + offset, size);
    return kDIErrNone;
}


/* a T/S pair */
typedef struct TrackSector {
    uint8_t    track;
    uint8_t    sector;
} TrackSector;

/*
 * Read the specified track and sector, adjusting for sector ordering as
 * appropriate.
 *
 * Copies 256 bytes into "*buf".
 *
 * Returns 0 on success, nonzero on failure.
 */
static DIError ReadTrackSector(long track, int sector, void *buf)
{
    DIError dierr;
    di_off_t offset;
    int newSector = -1;

    if (buf == NULL)
        return kDIErrInvalidArg;

    dierr = CalcSectorAndOffset(track, sector,
                                &offset, &newSector);
    if (dierr != kDIErrNone)
        return dierr;

        assert(offset + kSctSize <= fLength);

    dierr = CopyBytesOut(buf, offset, kSctSize);

    return dierr;
}

struct A2FileDOS {
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
    TrackSector* tsList;
    int tsCount;
    TrackSector* indexList;
    int indexCount;
    int fOffset;
    int fOpenEOF;
    int fOpenSectorsUsed;
    struct A2FileDOS *prev;
    struct A2FileDOS *next;
};

typedef struct A2FileDOS A2FileDOS;

A2FileDOS *firstfile = NULL;
A2FileDOS *lastfile = NULL;

static void AddFileToList(A2FileDOS *file) {
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
static void LowerASCII(uint8_t filename[kFileNameBufLen]) {

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
static void TrimTrailingSpaces(uint8_t filename[kFileNameBufLen]) {
    uint8_t *lastspc = filename + strlen((char *)filename);

    assert(*lastspc == '\0');

    while (--lastspc) {
        if (*lastspc != ' ')
            break;
    }

    *(lastspc+1) = '\0';
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
static DIError ExtractTSPairs(A2FileDOS *DOSFile, const uint8_t* sctBuf, TrackSector* tsList,
                                  int* pLastNonZero)
{
    DIError dierr = kDIErrNone;
//    const DiskImg* pDiskImg = fpDiskFS->GetDiskImg();
    const uint8_t* ptr;
    int i, track, sector;

    *pLastNonZero = -1;
    memset(tsList, 0, sizeof(TrackSector) * kMaxTSPairs);

    ptr = &sctBuf[kTSOffset];       // offset of first T/S entry (0x0c)

    for (i = 0; i < kMaxTSPairs; i++) {
        track = *ptr++;
        sector = *ptr++;

        if (dierr == kDIErrNone &&
            (track >= kNumTracks ||
             sector >= kNumSectPerTrack ||
             (track == 0 && sector != 0)))
        {
            fprintf(stderr, " DOS33 invalid T/S %d,%d in '%s'\n", track, sector,
                    DOSFile->fFileName);

            if (i > 0 && tsList[i-1].track == 0 && tsList[i-1].sector == 0) {
                fprintf(stderr, "  T/S list looks partially valid\n");
                goto bail;  // quit immediately
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


const int kDefaultTSAlloc = 2;
const int kDefaultIndexAlloc = 8;

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
static DIError LoadTSList(A2FileDOS *DOSFile)
{
    DIError dierr = kDIErrNone;

    TrackSector* tsList = NULL;
    TrackSector* indexList = NULL;
    int tsCount, tsAlloc;
    int indexCount, indexAlloc;
    uint8_t sctBuf[kSctSize];
    int track, sector, iterations;

    fprintf(stderr, "--- DOS loading T/S list for '%s'\n", DOSFile->fFileName);

    /* over-alloc for small files to reduce reallocs */
    tsAlloc = kMaxTSPairs * kDefaultTSAlloc;
    tsList = malloc(tsAlloc * sizeof(TrackSector));
    tsCount = 0;

    indexAlloc = kDefaultIndexAlloc;
    indexList = malloc(indexAlloc * sizeof(TrackSector));
    indexCount = 0;

    if (tsList == NULL || indexList == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    /* get the first T/S sector for this file */
    track = DOSFile->fTSListTrack;
    sector = DOSFile->fTSListSector;
    if (track >= kNumTracks ||
        sector >= kNumSectPerTrack)
    {
        fprintf(stderr, " DOS33 invalid initial T/S %d,%d in '%s'\n", track, sector,
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
            fprintf(stderr, "+++ expanding index list\n");
            TrackSector* newList;
            indexAlloc += kDefaultIndexAlloc;
            newList = malloc(indexAlloc * sizeof(TrackSector));
            if (newList == NULL) {
                dierr = kDIErrMalloc;
                goto bail;
            }
            memcpy(newList, indexList, indexCount * sizeof(TrackSector));
            free(indexList);
            indexList = newList;
        }
        indexList[indexCount].track = track;
        indexList[indexCount].sector = sector;
        indexCount++;

        dierr = ReadTrackSector(track, sector, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        /* grab next track/sector */
        track = sctBuf[0x01];
        sector = sctBuf[0x02];
        sectorOffset = sctBuf[0x05] +  sctBuf[0x06] * 256;

        /* if T/S link is bogus, whole sector is probably bad */
        if (track >= kNumTracks ||
            sector >= kNumSectPerTrack)
        {
            // bogus T/S, mark file as damaged and stop
            fprintf(stderr, " DOS33 invalid T/S link %d,%d in '%s'\n", track, sector,
                 DOSFile->fFileName);
            dierr = kDIErrBadFile;
            goto bail;
        }
        if ((sectorOffset % kMaxTSPairs) != 0) {
            fprintf(stderr, " DOS33 invalid T/S header sector offset %u in '%s'\n",
                 sectorOffset,  DOSFile->fFileName);
            // not fatal, just weird
        }

        /*
         * Make sure we have enough room to hold an entire sector full of
         * T/S pairs in the list.
         */
        if (tsCount + kMaxTSPairs > tsAlloc) {
            fprintf(stderr, "+++ expanding ts list\n");
            TrackSector* newList;
            tsAlloc += kMaxTSPairs * kDefaultTSAlloc;
            newList = malloc(tsAlloc * sizeof(TrackSector));
            if (newList == NULL) {
                dierr = kDIErrMalloc;
                goto bail;
            }
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
                //LOGI(" DOS33 odd -- last T/S sector of '%s' was empty",
                //  GetPathName());
            }
            tsCount += lastNonZero +1;
        }

        iterations++;       // watch for infinite loops
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
static DIError Read(A2FileDOS *pFile, void* buf, size_t len, size_t *pActual)
{
    fprintf(stderr, " DOS reading %lu bytes from '%s' (offset=%ld)\n",
         (unsigned long) len, pFile->fFileName, (long) pFile->fOffset);

    /*
     * Don't allow them to read past the end of the file.  The length value
     * stored in pFile->fLength already has pFile->fDataOffset subtracted
     * from the actual data length, so don't factor it in again.
     */
    if (pFile->fOffset + (long)len > pFile->fOpenEOF) {
        if (pActual == NULL)
            return kDIErrDataUnderrun;
        len = (size_t) (pFile->fOpenEOF - pFile->fOffset);
    }
    if (pActual != NULL)
        *pActual = len;
    long incrLen = len;

    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];
    di_off_t actualOffset = pFile->fOffset; //+ pFile->fDataOffset;   // adjust for embedded len
    int tsIndex = (int) (actualOffset / kSctSize);
    int bufOffset = (int) (actualOffset % kSctSize);        // (& 0xff)
    size_t thisCount;

    if (len == 0)
        return kDIErrNone;
    assert(pFile->fOpenEOF != 0);

    assert(tsIndex >= 0 && tsIndex < pFile->tsCount);

    /* could be more clever in here and avoid double-buffering */
    while (len) {
        if (tsIndex >= pFile->tsCount) {
            /* should've caught this earlier */
            fprintf(stderr, " DOS ran off the end (fTSCount=%d). len == %zu\n",  pFile->tsCount, len);
            return kDIErrDataUnderrun;
        }

        if (pFile->tsList[tsIndex].track == 0 && pFile->tsList[tsIndex].sector == 0) {
            //LOGI(" DOS sparse sector T=%d S=%d",
            //  TSTrack(fTSList[tsIndex]), TSSector(fTSList[tsIndex]));
            memset(sctBuf, 0, sizeof(sctBuf));
        } else {
            dierr = ReadTrackSector(pFile->tsList[tsIndex].track, pFile->tsList[tsIndex].sector, sctBuf);
            if (dierr != kDIErrNone) {
                fprintf(stderr, " DOS error reading file '%s'\n", pFile->fFileName);
                return dierr;
            }
        }
        thisCount = kSctSize - bufOffset;
        if (thisCount > len)
            thisCount = len;
        memcpy(buf, sctBuf + bufOffset, thisCount);
        len -= thisCount;
        buf = (char*)buf + thisCount;

        bufOffset = 0;
        tsIndex++;
    }

    pFile->fOffset += incrLen;

    return dierr;
}




/*
 * Set up state for this file.
 */
static DIError Open(A2FileDOS* pOpenFile)
{
    DIError dierr = kDIErrNone;

    dierr = LoadTSList(pOpenFile);
    if (dierr != kDIErrNone) {
        fprintf(stderr, "DOS33 unable to load TS for '%s' open\n", pOpenFile->fFileName);
        goto bail;
    }

    pOpenFile->fOffset = 0;
    pOpenFile->fOpenEOF = fLength;
    pOpenFile->fOpenSectorsUsed = pOpenFile->fLengthInSectors;

//    fpOpenFile = pOpenFile;     // add it to our single-member "open file set"
//    *ppOpenFile = pOpenFile;
//    pOpenFile = NULL;
//
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
                             const uint8_t* sctBuf)
{
    A2FileDOS* pFile;
    const uint8_t* pEntry;
    int i;

    pEntry = &sctBuf[kCatalogEntryOffset];

    for (i = 0; i < kCatalogEntriesPerSect; i++) {
        if (pEntry[0x00] != kEntryUnused && pEntry[0x00] != kEntryDeleted) {
            pFile = malloc(sizeof(A2FileDOS));
            pFile->prev = NULL;
            pFile->next = NULL;

            pFile->fTSListTrack = pEntry[0x00];
            pFile->fTSListSector = pEntry[0x01];
            pFile->fLocked = (pEntry[0x02] & 0x80) != 0;
            switch (pEntry[0x02] & 0x7f) {
                case 0x00:  pFile->fFileType = FILETYPE_T;    break;
                case 0x01:  pFile->fFileType = FILETYPE_I;    break;
                case 0x02:  pFile->fFileType = FILETYPE_A;    break;
                case 0x04:  pFile->fFileType = FILETYPE_B;    break;
                case 0x08:  pFile->fFileType = FILETYPE_S;    break;
                case 0x10:  pFile->fFileType = FILETYPE_R;    break;
                case 0x20:  pFile->fFileType = FILETYPE_A;    break;
                case 0x40:  pFile->fFileType = FILETYPE_B;    break;
                default:
                    /* some odd arrangement of bit flags? */
                    fprintf(stderr, " DOS33 peculiar filetype byte 0x%02x\n", pEntry[0x02]);
                    pFile->fFileType = FILETYPE_UNKNOWN;
                    break;
            }

            memcpy(pFile->fRawFileName, &pEntry[0x03], kMaxFileName);
            pFile->fRawFileName[kMaxFileName] = '\0';

            memcpy(pFile->fFileName, &pEntry[0x03], kMaxFileName);
            pFile->fFileName[kMaxFileName] = '\0';
            FixFilename(pFile->fFileName);

            fprintf(stderr, "File name: %s\n", pFile->fFileName);

            pFile->fLengthInSectors = pEntry[0x21];
            pFile->fLengthInSectors |= (uint16_t) pEntry[0x22] << 8;

            pFile->fCatTStrack = catTrack;
            pFile->fCatTSsector = catSect;
            pFile->fCatEntryNum = i;

            pFile->tsList = NULL;
            pFile->tsCount = 0;
            pFile->indexList = NULL;
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

static int fFirstCatTrack;
static int fFirstCatSector;
static int fVTOCVolumeNumber;
static int fVTOCNumTracks;
static int fVTOCNumSectors;

uint8_t   fVTOC[kSctSize];

/*
 * Read some fields from the disk Volume Table of Contents.
 */
static DIError ReadVTOC(void)
{
    DIError dierr;

    dierr = ReadTrackSector(kVTOCTrack, kVTOCSector, fVTOC);
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
        fprintf(stderr, " DOS33 warning: VTOC numtracks %d vs %d\n",
             fVTOCNumTracks, kNumTracks);
    }
    if (fVTOCNumSectors != kNumSectPerTrack) {
        fprintf(stderr, " DOS33 warning: VTOC numsect %d vs %d\n",
             fVTOCNumSectors, kNumSectPerTrack);
    }

bail:
    return dierr;
}

TrackSector fCatalogSectors[kMaxCatalogSectors];


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
    uint8_t sctBuf[kSctSize];
    int catTrack, catSect;
    int iterations;

    catTrack = fFirstCatTrack;
    catSect = fFirstCatSector;
    iterations = 0;

    memset(fCatalogSectors, 0, sizeof(fCatalogSectors));

    while (catTrack != 0 && catSect != 0 && iterations < kMaxCatalogSectors)
    {

        fprintf(stderr, " DOS33 reading catalog sector T=%d S=%d\n", catTrack, catSect);
        dierr = ReadTrackSector(catTrack, catSect, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        /*
         * Watch for flaws that the DOS detector allows.
         */
        if (catTrack == sctBuf[0x01] && catSect == sctBuf[0x02]) {
            fprintf(stderr, " DOS detected self-reference on cat (%d,%d)\n",
                 catTrack, catSect);
            break;
        }

        /*
         * Check the next track/sector in the chain.  If the pointer is
         * broken, there's a very good chance that this isn't really a
         * catalog sector, so we want to bail out now.
         */
        if (sctBuf[0x01] >= kNumTracks ||
            sctBuf[0x02] >= kNumSectPerTrack)
        {
            fprintf(stderr, " DOS bailing out early on catalog read due to funky T/S\n");
            break;
        }

        dierr = ProcessCatalogSector(catTrack, catSect, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        fCatalogSectors[iterations].track = catTrack;
        fCatalogSectors[iterations].sector = catSect;

        catTrack = sctBuf[0x01];
        catSect = sctBuf[0x02];

        iterations++;       // watch for infinite loops

    }
    if (iterations >= kMaxCatalogSectors) {
        dierr = kDIErrDirectoryLoop;
        goto bail;
    }

bail:
    return dierr;
}

static A2FileDOS *find_file_named(char *name) {
    fprintf(stderr, "find_file_named: \"%s\"\n", name);
    A2FileDOS *file = firstfile;
    while (file != NULL) {
        fprintf(stderr, "Comparing to file named: \"%s\"\n", (char *)file->fFileName);
        if (strcmp(name, (char *)file->fFileName) == 0)
            break;
        file = file->next;
    }
    return file;
}

static A2FileDOS *find_SAGA_database(void) {
    A2FileDOS *file = firstfile;
    while (file != NULL) {
        char *name = (char *)file->fFileName;
        fprintf(stderr, "Comparing to file named: \"%s\"\n", name);
        if (strcmp("DATABASE", name) == 0)
            break;
        if (strcmp("SORCEROR OF CLAYMORGUE CASTLE", name) == 0)
            break;
        if (strcmp("THE INCREDIBLE HULK", name) == 0)
            break;
        if (name[0] == 'A' && name[2] == '.' &&
            name[3] == 'D' && name[4] == 'A' && name[5] == 'T') {
            break; //A(adventure number).DAT
        }
        file = file->next;
    }
    return file;
}

size_t writeToFile(const char *name, uint8_t *data, size_t size);

uint8_t *ReadApple2DOSFile(uint8_t *data, size_t *len, uint8_t **invimg, size_t *invimglen, uint8_t **m2)
{
    rawdata = data;
    rawdatalen = *len;

    uint8_t *buf = NULL;
    ReadVTOC();
    ReadCatalog();
    A2FileDOS *file = find_SAGA_database();
    if (file) {
        Open(file);
        buf = malloc(file->fLengthInSectors * kSctSize);
        size_t actual;
        Read(file, buf, (file->fLengthInSectors - 1) * kSctSize, &actual);
        *len = actual;
    }
    /* Many games have the inventory image on the boot disk (and the rest of the images on the other disk) */
    file = find_file_named("PAC.INVEN");
    if (!file) {
        file = find_file_named("PAK.INVEN");
    }
    if (file) {
        Open(file);
        uint8_t inventemp[file->fLengthInSectors * kSctSize];
        fprintf(stderr, "inventemp is size %d\n", file->fLengthInSectors * kSctSize);
        Read(file, inventemp, (file->fLengthInSectors - 1) * kSctSize, invimglen);
        fprintf(stderr, "*invimglen is %zu\n", *invimglen);
        *invimglen -= 4;
        fprintf(stderr, "*invimglen - 4 is %zu\n", *invimglen);
        *invimg = malloc(*invimglen);
        memcpy(*invimg, inventemp + 4, *invimglen);
    }

    /* Image unscrambling tables are usually found somewhere in a file named M2 (As in memory part 2?) */
    file = find_file_named("M2");
    if (file) {
        Open(file);
        uint8_t m2temp[file->fLengthInSectors * kSctSize];
        fprintf(stderr, "m2temp is size %d\n", file->fLengthInSectors * kSctSize);
        size_t m2len;
        Read(file, m2temp, (file->fLengthInSectors - 1) * kSctSize, &m2len);

        writeToFile("/Users/administrator/Desktop/m1", m2temp, m2len);

        if (m2len > 0x1747 + 0x180 && strncmp((char *)(m2temp + 0x172C), "COPYRIGHT 1983 NORMAN L. SAILER", 31) == 0) {
            *m2 = malloc(0x182);
            memcpy(*m2, m2temp + 0x174B, 0x182);
        }
    }

    return buf;
}

