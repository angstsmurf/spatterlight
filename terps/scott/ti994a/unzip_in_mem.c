//
//  unzip_in_mem.c
//  scott
//
//  Created by Administrator on 2026-02-18.
//

// This is code from the MiniZip project (by Gilles Vollant and Mathias Svensson),
// modified in a quick-and-dirty way to decompress in memory instead of writing files.
// The original code is currently at https://github.com/madler/zlib/tree/develop/contrib/minizip
// See http://www.info-zip.org/pub/infozip/license.html for the Info-ZIP license.

// It is used here to extract a single file from a Return to Pirate's Isle cartridge image
// and may not work properly for any other purpose.


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "memory_allocation.h"
#include "unzip_in_mem.h"


#define UNZ_OK                          (0)
#define UNZ_END_OF_LIST_OF_FILE         (-100)
#define UNZ_ERRNO                       (Z_ERRNO)
#define UNZ_EOF                         (0)
#define UNZ_PARAMERROR                  (-102)
#define UNZ_BADZIPFILE                  (-103)
#define UNZ_INTERNALERROR               (-104)
#define UNZ_CRCERROR                    (-105)

#define CENTRALDIRINVALID (uint64_t)-1
#define SIZECENTRALDIRITEM (0x2e)
#define SIZEZIPLOCALHEADER (0x1e)
#define BUFREADCOMMENT (0x400)

#define CASESENSITIVITY (0)
#define CASESENSITIVITYDEFAULTVALUE 2

#define UNZ_MAXFILENAMEINZIP (256)
#define WRITEBUFFERSIZE (8192)
#define UNZ_BUFSIZE (16384)

#define Z_BZIP2ED 12


/* unz_global_info structure contain global data about the ZIPfile
 These data comes from the end of central dir */
typedef struct unz_global_info64_s
{
    uint64_t number_entry;         /* total number of entries in
                                    the central dir on this disk */
    uLong size_comment;         /* size of the global comment of the zipfile */
} unz_global_info64;

/* unz_file_info contain information about a file in the zipfile */
typedef struct unz_file_info64_s
{
    uLong version;              /* version made by                 2 bytes */
    uLong version_needed;       /* version needed to extract       2 bytes */
    uLong flag;                 /* general purpose bit flag        2 bytes */
    uLong compression_method;   /* compression method              2 bytes */
    uLong dosDate;              /* last mod file date in Dos fmt   4 bytes */
    uLong crc;                  /* crc-32                          4 bytes */
    uint64_t compressed_size;   /* compressed size                 8 bytes */
    uint64_t uncompressed_size; /* uncompressed size               8 bytes */
    uLong size_filename;        /* filename length                 2 bytes */
    uLong size_file_extra;      /* extra field length              2 bytes */
    uLong size_file_comment;    /* file comment length             2 bytes */

    uLong disk_num_start;       /* disk number start               2 bytes */
    uLong internal_fa;          /* internal file attributes        2 bytes */
    uLong external_fa;          /* external file attributes        4 bytes */

    //    tm_unz tmu_date;
} unz_file_info64;


/* unz_file_info64_internal contain internal info about a file in zipfile*/
typedef struct unz_file_info64_internal_s
{
    uint64_t offset_curfile;/* relative offset of local header 8 bytes */
} unz_file_info64_internal;


/* file_in_zip_read_info_s contain internal information about a file in zipfile,
 when reading and decompress it */
typedef struct
{
    char  *read_buffer;         /* internal buffer for compressed data */
    z_stream stream;            /* zLib stream structure for inflate */

#ifdef HAVE_BZIP2
    bz_stream bstream;          /* bzLib stream structure for bziped */
#endif

    uint64_t pos_in_zipfile;       /* position in byte on the zipfile, for fseek*/
    uLong stream_initialised;   /* flag set if stream structure is initialised*/

    uint64_t offset_local_extrafield;/* offset of the local extra field */
    uInt  size_local_extrafield;/* size of the local extra field */
    uint64_t pos_local_extrafield;   /* position in the local extra field in read*/
    uint64_t total_out_64;

    uLong crc32;                /* crc32 of all data uncompressed */
    uLong crc32_wait;           /* crc32 we must obtain after decompress all */
    uint64_t rest_read_compressed; /* number of byte to be decompressed */
    uint64_t rest_read_uncompressed;/*number of byte to be obtained after decomp*/
    voidpf filestream;        /* io structure of the zipfile */
    uLong compression_method;   /* compression method (0==store) */
    uint64_t byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
    int   raw;
} file_in_zip64_read_info_s;

/* unz64_s contain internal information about the zipfile
 */
typedef struct
{
    int is64bitOpenFunction;
    voidpf filestream;        /* io structure of the zipfile */
    size_t filesize;
    unz_global_info64 gi;       /* public global information */
    uint64_t byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
    uint64_t num_file;             /* number of the current file in the zipfile*/
    uint64_t pos_in_central_dir;   /* pos of the current file in the central dir*/
    uint64_t current_file_ok;      /* flag about the usability of the current file*/
    uint64_t central_pos;          /* position of the beginning of the central dir*/

    uint64_t size_central_dir;     /* size of the central directory  */
    uint64_t offset_central_dir;   /* offset of start of central directory with
                                    respect to the starting disk number */

    unz_file_info64 cur_file_info; /* public info about the current file in zip*/
    unz_file_info64_internal cur_file_info_internal; /* private info about it*/
    file_in_zip64_read_info_s* pfile_in_zip_read; /* structure about the current
                                                   file if we are decompressing it */
    int encrypted;

    int isZip64;
} unz64_s;



static int unz64local_getLong64(uint8_t **outptr,uint8_t *endptr,
                         uint64_t *pX) {
    uint8_t *ptr = *outptr;
    if (ptr - endptr < 8) {
        *pX = 0;
        return UNZ_EOF;
    }

    unsigned char c[8];
    memcpy(c, ptr, 8);
    ptr += 8;

    *pX = c[0] | ((uint64_t)c[1] << 8) | ((uint64_t)c[2] << 16) | ((uint64_t)c[3] << 24)
    | ((uint64_t)c[4] << 32) | ((uint64_t)c[5] << 40) | ((uint64_t)c[6] << 48) | ((uint64_t)c[7] << 56);
    *outptr = ptr;
    return UNZ_OK;
}

static int unz64local_getLong(uint8_t **outptr, uint8_t *endptr,
                       uLong *pX) {
    uint8_t *ptr = *outptr;
    if (endptr - ptr < 4) {
        *pX = 0;
        return UNZ_EOF;
    }

    unsigned char c[4];

    memcpy(c, ptr, 4);
    ptr += 4;
    *pX = c[0] | ((uLong)c[1] << 8) | ((uLong)c[2] << 16) | ((uLong)c[3] << 24);
    *outptr = ptr;
    return UNZ_OK;
}

static int unz64local_getShort(uint8_t **outptr, uint8_t *endptr,
                               uLong *pX) {
    uint8_t *ptr = *outptr;
    if (endptr - ptr < 2) {
        *pX = 0;
        return UNZ_EOF;
    }
    unsigned char c[2];
    memcpy(c, ptr, 2);
    ptr += 2;
    *pX = c[0] | ((uLong)c[1] << 8);
    *outptr = ptr;
    return UNZ_OK;
}


static uint64_t unz64local_SearchCentralDir64(voidpf filestream, size_t fileSize) {
    unsigned char* buf;
    uint64_t uBackRead;
    uint64_t uMaxBack=0xffff; /* maximum size of global comment */
    uint64_t uPosFound=CENTRALDIRINVALID;
    uLong uL;
    uint64_t relativeOffset;
    uint64_t uSizeFile = fileSize;

    if (uMaxBack>uSizeFile)
        uMaxBack = uSizeFile;

    buf = (unsigned char*)MemAlloc(BUFREADCOMMENT+4);

    uBackRead = 4;

    uint8_t *ptr = filestream;
    uint8_t *endptr = filestream + fileSize;

    while (uBackRead<uMaxBack)
    {
        uLong uReadSize;
        uint64_t uReadPos;
        int i;
        if (uBackRead+BUFREADCOMMENT>uMaxBack)
            uBackRead = uMaxBack;
        else
            uBackRead+=BUFREADCOMMENT;
        uReadPos = uSizeFile-uBackRead ;

        uReadSize = ((BUFREADCOMMENT+4) < (uSizeFile-uReadPos)) ?
        (BUFREADCOMMENT+4) : (uLong)(uSizeFile-uReadPos);

        ptr += uReadPos;
        if (ptr + uReadSize > endptr)
            break;

        memcpy(buf, ptr, uReadSize);

        for (i=(int)uReadSize-3; (i--)>0;)
            if (((*(buf+i))==0x50) && ((*(buf+i+1))==0x4b) &&
                ((*(buf+i+2))==0x06) && ((*(buf+i+3))==0x07))
            {
                uPosFound = uReadPos+(unsigned)i;
                break;
            }

        if (uPosFound!=CENTRALDIRINVALID)
            break;
    }
    free(buf);
    if (uPosFound == CENTRALDIRINVALID)
        return CENTRALDIRINVALID;

    /* Zip64 end of central directory locator */
    ptr = filestream + uPosFound;
    if (ptr > endptr)
        return CENTRALDIRINVALID;

    /* the signature, already checked */
    if (unz64local_getLong(&ptr, endptr, &uL)!=UNZ_OK)
        return CENTRALDIRINVALID;

    /* number of the disk with the start of the zip64 end of central directory */
    if (unz64local_getLong(&ptr,endptr,&uL)!=UNZ_OK)
        return CENTRALDIRINVALID;
    if (uL != 0)
        return CENTRALDIRINVALID;

    /* relative offset of the zip64 end of central directory record */
    if (unz64local_getLong64(&ptr,endptr,&relativeOffset)!=UNZ_OK)
        return CENTRALDIRINVALID;

    /* total number of disks */
    if (unz64local_getLong(&ptr,endptr,&uL)!=UNZ_OK)
        return CENTRALDIRINVALID;
    if (uL != 1)
        return CENTRALDIRINVALID;

    /* Goto end of central directory record */
    ptr = filestream + relativeOffset;
    if (ptr > endptr)
        return CENTRALDIRINVALID;

    /* the signature */
    if (unz64local_getLong(&ptr, endptr,&uL)!=UNZ_OK)
        return CENTRALDIRINVALID;

    if (uL != 0x06064b50)
        return CENTRALDIRINVALID;

    return relativeOffset;
}


/*
 Get Info about the current file in the zipfile, with internal only info
 */
static int unz64local_GetCurrentFileInfoInternal(unz64_s *s,
                                                 unz_file_info64 *pfile_info,
                                                 unz_file_info64_internal
                                                 *pfile_info_internal,
                                                 char *szFileName,
                                                 uLong fileNameBufferSize,
                                                 void *extraField,
                                                 uLong extraFieldBufferSize,
                                                 char *szComment,
                                                 uLong commentBufferSize) {
    unz_file_info64 file_info;
    unz_file_info64_internal file_info_internal;
    int err=UNZ_OK;
    uLong uMagic;
    long lSeek=0;
    uLong uL;

    if (s==NULL)
        return UNZ_PARAMERROR;

    uint8_t *ptr = s->filestream;
    uint8_t *endptr = ptr + s->filesize;

    ptr = s->filestream + s->pos_in_central_dir+s->byte_before_the_zipfile;

    if (ptr > endptr) {
        err = UNZ_EOF;
    }

    /* we check the magic */
    if (err==UNZ_OK)
    {
        if (unz64local_getLong(&ptr,endptr,&uMagic) != UNZ_OK)
            err=UNZ_ERRNO;
        else if (uMagic!=0x02014b50)
            err=UNZ_BADZIPFILE;
    }

    if (unz64local_getShort(&ptr,endptr,&file_info.version) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getShort(&ptr,endptr,&file_info.version_needed) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getShort(&ptr,endptr,&file_info.flag) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getShort(&ptr,endptr,&file_info.compression_method) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getLong(&ptr,endptr,&file_info.dosDate) != UNZ_OK)
        err=UNZ_ERRNO;

    //    unz64local_DosDateToTmuDate(file_info.dosDate,&file_info.tmu_date);

    if (unz64local_getLong(&ptr,endptr,&file_info.crc) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getLong(&ptr,endptr,&uL) != UNZ_OK)
        err=UNZ_ERRNO;
    file_info.compressed_size = uL;

    if (unz64local_getLong(&ptr,endptr,&uL) != UNZ_OK)
        err=UNZ_ERRNO;
    file_info.uncompressed_size = uL;

    if (unz64local_getShort(&ptr,endptr,&file_info.size_filename) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getShort(&ptr,endptr,&file_info.size_file_extra) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getShort(&ptr,endptr,&file_info.size_file_comment) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getShort(&ptr,endptr,&file_info.disk_num_start) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getShort(&ptr,endptr,&file_info.internal_fa) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getLong(&ptr,endptr,&file_info.external_fa) != UNZ_OK)
        err=UNZ_ERRNO;

    /* relative offset of local header */
    if (unz64local_getLong(&ptr,endptr,&uL) != UNZ_OK)
        err=UNZ_ERRNO;
    file_info_internal.offset_curfile = uL;

    lSeek+=file_info.size_filename;
    if ((err==UNZ_OK) && (szFileName!=NULL))
    {
        uLong uSizeRead ;
        if (file_info.size_filename<fileNameBufferSize)
        {
            *(szFileName+file_info.size_filename)='\0';
            uSizeRead = file_info.size_filename;
        }
        else
            uSizeRead = fileNameBufferSize;

        if ((file_info.size_filename>0) && (fileNameBufferSize>0)) {
            if (ptr + uSizeRead > endptr) {
                err=UNZ_ERRNO;
            } else {
                memcpy(szFileName, ptr, uSizeRead);
                ptr += uSizeRead;
            }
        }
        lSeek -= uSizeRead;
    }

    /* Read extrafield */
    if ((err==UNZ_OK) && (extraField!=NULL))
    {
        uint64_t uSizeRead ;
        if (file_info.size_file_extra<extraFieldBufferSize)
            uSizeRead = file_info.size_file_extra;
        else
            uSizeRead = extraFieldBufferSize;

        if (lSeek!=0)
        {
            ptr += lSeek;
            lSeek = 0;
            if (ptr > endptr) {
                err=UNZ_ERRNO;
            }
        }

        if ((file_info.size_file_extra>0) && (extraFieldBufferSize>0)) {
            if (ptr + uSizeRead > endptr) {
                err=UNZ_ERRNO;
            } else {
                memcpy(extraField, ptr, uSizeRead);
                ptr += uSizeRead;
            }
        }

        lSeek += file_info.size_file_extra - (uLong)uSizeRead;
    }
    else
        lSeek += file_info.size_file_extra;


    if ((err==UNZ_OK) && (file_info.size_file_extra != 0))
    {
        uLong acc = 0;

        /* since lSeek now points to after the extra field we need to move back */
        lSeek -= file_info.size_file_extra;

        if (lSeek!=0)
        {
            ptr += (uint64_t)lSeek;
            lSeek = 0;
            if (ptr > endptr)
                err=UNZ_ERRNO;
        }

        while(acc < file_info.size_file_extra)
        {
            uLong headerId;
            uLong dataSize;

            if (unz64local_getShort(&ptr, endptr, &headerId) != UNZ_OK)
                err=UNZ_ERRNO;

            if (unz64local_getShort(&ptr, endptr, &dataSize) != UNZ_OK)
                err=UNZ_ERRNO;

            /* ZIP64 extra fields */
            if (headerId == 0x0001)
            {
                if(file_info.uncompressed_size == UINT32_MAX)
                {
                    if (unz64local_getLong64(&ptr, endptr,&file_info.uncompressed_size) != UNZ_OK)
                        err=UNZ_ERRNO;
                }

                if(file_info.compressed_size == UINT32_MAX)
                {
                    if (unz64local_getLong64(&ptr, endptr, &file_info.compressed_size) != UNZ_OK)
                        err=UNZ_ERRNO;
                }

                if(file_info_internal.offset_curfile == UINT32_MAX)
                {
                    /* Relative Header offset */
                    if (unz64local_getLong64(&ptr, endptr, &file_info_internal.offset_curfile) != UNZ_OK)
                        err=UNZ_ERRNO;
                }

                if(file_info.disk_num_start == 0xffff)
                {
                    /* Disk Start Number */
                    if (unz64local_getLong(&ptr, endptr, &file_info.disk_num_start) != UNZ_OK)
                        err=UNZ_ERRNO;
                }

            }
            else
            {
                ptr += dataSize;
                if (ptr > endptr)
                    err=UNZ_ERRNO;
            }

            acc += 2 + 2 + dataSize;
        }
    }

    if ((err==UNZ_OK) && (szComment!=NULL))
    {
        uLong uSizeRead ;
        if (file_info.size_file_comment<commentBufferSize)
        {
            *(szComment+file_info.size_file_comment)='\0';
            uSizeRead = file_info.size_file_comment;
        }
        else
            uSizeRead = commentBufferSize;

        if (lSeek!=0)
        {
            ptr += (uint64_t)lSeek;
            if (ptr > endptr)
                err=UNZ_ERRNO;
        }

        if ((file_info.size_file_comment>0) && (commentBufferSize>0)) {
            if (ptr + uSizeRead > endptr) {
                err=UNZ_ERRNO;
            } else {
                memcpy(szComment, ptr, uSizeRead);
                ptr += uSizeRead;
            }
        }
    }


    if ((err==UNZ_OK) && (pfile_info!=NULL))
        *pfile_info=file_info;

    if ((err==UNZ_OK) && (pfile_info_internal!=NULL))
        *pfile_info_internal=file_info_internal;

    return err;
}

/*
 Compare two filenames (fileName1,fileName2).
 If iCaseSensitivity = 1, comparison is case sensitive (like strcmp)
 If iCaseSensitivity = 2, comparison is not case sensitive (like strcmpi
 or strcasecmp)
 If iCaseSensitivity = 0, case sensitivity is default of your operating system
 (like 1 on Unix, 2 on Windows)

 */
static int unzStringFileNameCompare (const char*  fileName1,
                                             const char*  fileName2,
                                             int iCaseSensitivity) {
    if (iCaseSensitivity==0)
        iCaseSensitivity=CASESENSITIVITYDEFAULTVALUE;

    if (iCaseSensitivity==1)
        return strcmp(fileName1,fileName2);

    return strcasecmp(fileName1,fileName2);
}

/*
 Set the current file of the zipfile to the first file.
 return UNZ_OK if there is no problem
 */
static int unzGoToFirstFile(unz64_s *s) {
    int err=UNZ_OK;
    if (s==NULL)
        return UNZ_PARAMERROR;
    s->pos_in_central_dir=s->offset_central_dir;
    s->num_file=0;
    err=unz64local_GetCurrentFileInfoInternal(s,&s->cur_file_info,
                                              &s->cur_file_info_internal,
                                              NULL,0,NULL,0,NULL,0);
    s->current_file_ok = (err == UNZ_OK);
    return err;
}

/*
 Write info about the ZipFile in the *pglobal_info structure.
 No preparation of the structure is needed
 return UNZ_OK if there is no problem.
 */
static int unzGetCurrentFileInfo64(unz64_s *file,
                            unz_file_info64 * pfile_info,
                            char * szFileName, uLong fileNameBufferSize,
                            void *extraField, uLong extraFieldBufferSize,
                            char* szComment,  uLong commentBufferSize) {
    return unz64local_GetCurrentFileInfoInternal(file,pfile_info,NULL,
                                                 szFileName,fileNameBufferSize,
                                                 extraField,extraFieldBufferSize,
                                                 szComment,commentBufferSize);
}

/*
 Set the current file of the zipfile to the next file.
 return UNZ_OK if there is no problem
 return UNZ_END_OF_LIST_OF_FILE if the actual file was the latest.
 */
static int unzGoToNextFile(voidp file, size_t filesize) {
    unz64_s* s;
    int err;

    if (file==NULL)
        return UNZ_PARAMERROR;
    s=(unz64_s*)file;
    if (!s->current_file_ok)
        return UNZ_END_OF_LIST_OF_FILE;
    if (s->gi.number_entry != 0xffff)    /* 2^16 files overflow hack */
        if (s->num_file+1==s->gi.number_entry)
            return UNZ_END_OF_LIST_OF_FILE;

    s->pos_in_central_dir += SIZECENTRALDIRITEM + s->cur_file_info.size_filename +
    s->cur_file_info.size_file_extra + s->cur_file_info.size_file_comment ;
    s->num_file++;
    err = unz64local_GetCurrentFileInfoInternal(file,&s->cur_file_info,
                                                &s->cur_file_info_internal,
                                                NULL,0,NULL,0,NULL,0);
    s->current_file_ok = (err == UNZ_OK);
    return err;
}

/*
 Try locate the file szFileName in the zipfile.
 For the iCaseSensitivity signification, see unzStringFileNameCompare

 return value :
 UNZ_OK if the file is found. It becomes the current file.
 UNZ_END_OF_LIST_OF_FILE if the file is not found
 */
static int unzLocateFile(unz64_s *s, const char *szFileName, int iCaseSensitivity) {
    int err;

    /* We remember the 'current' position in the file so that we can jump
     * back there if we fail.
     */
    unz_file_info64 cur_file_infoSaved;
    unz_file_info64_internal cur_file_info_internalSaved;
    uint64_t num_fileSaved;
    uint64_t pos_in_central_dirSaved;


    if (s == NULL)
        return UNZ_PARAMERROR;

    if (strlen(szFileName)>=UNZ_MAXFILENAMEINZIP)
        return UNZ_PARAMERROR;

    if (!s->current_file_ok)
        return UNZ_END_OF_LIST_OF_FILE;

    /* Save the current state */
    num_fileSaved = s->num_file;
    pos_in_central_dirSaved = s->pos_in_central_dir;
    cur_file_infoSaved = s->cur_file_info;
    cur_file_info_internalSaved = s->cur_file_info_internal;

    err = unzGoToFirstFile(s);

    while (err == UNZ_OK)
    {
        char szCurrentFileName[UNZ_MAXFILENAMEINZIP+1];
        err = unzGetCurrentFileInfo64(s,NULL,
                                      szCurrentFileName,sizeof(szCurrentFileName)-1,
                                      NULL,0,NULL,0);
        if (err == UNZ_OK)
        {
            if (unzStringFileNameCompare(szCurrentFileName,
                                         szFileName,iCaseSensitivity)==0)
                return UNZ_OK;
            err = unzGoToNextFile(s, s->filesize);
        }
    }

    /* We failed, so restore the state of the 'current file' to where we
     * were.
     */
    s->num_file = num_fileSaved ;
    s->pos_in_central_dir = pos_in_central_dirSaved ;
    s->cur_file_info = cur_file_infoSaved;
    s->cur_file_info_internal = cur_file_info_internalSaved;
    return err;
}

/*
 Read bytes from the current file.
 buf contains buffer where data must be copied
 len the size of buf.

 return the number of byte copied if some bytes are copied
 return 0 if the end of file was reached
 return <0 with error code if there is an error
 (UNZ_ERRNO for IO error, or zLib error for uncompress error)
 */
static int unzReadCurrentFile(unz64_s *s, voidp buf, unsigned len) {
    int err=UNZ_OK;
    uInt iRead = 0;
    file_in_zip64_read_info_s* pfile_in_zip_read_info;
    if (s==NULL)
        return UNZ_PARAMERROR;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

    if (pfile_in_zip_read_info==NULL)
        return UNZ_PARAMERROR;


    if (pfile_in_zip_read_info->read_buffer == NULL)
        return UNZ_END_OF_LIST_OF_FILE;
    if (len==0)
        return 0;

    pfile_in_zip_read_info->stream.next_out = (Bytef*)buf;

    pfile_in_zip_read_info->stream.avail_out = (uInt)len;

    if ((len>pfile_in_zip_read_info->rest_read_uncompressed) &&
        (!(pfile_in_zip_read_info->raw)))
        pfile_in_zip_read_info->stream.avail_out =
        (uInt)pfile_in_zip_read_info->rest_read_uncompressed;

    if ((len>pfile_in_zip_read_info->rest_read_compressed+
         pfile_in_zip_read_info->stream.avail_in) &&
        (pfile_in_zip_read_info->raw))
        pfile_in_zip_read_info->stream.avail_out =
        (uInt)pfile_in_zip_read_info->rest_read_compressed+
        pfile_in_zip_read_info->stream.avail_in;

    uint8_t *ptr = pfile_in_zip_read_info->filestream;
    uint8_t *endptr = ptr + s->filesize;

    while (pfile_in_zip_read_info->stream.avail_out>0)
    {
        if ((pfile_in_zip_read_info->stream.avail_in==0) &&
            (pfile_in_zip_read_info->rest_read_compressed>0))
        {
            uInt uReadThis = UNZ_BUFSIZE;
            if (pfile_in_zip_read_info->rest_read_compressed<uReadThis)
                uReadThis = (uInt)pfile_in_zip_read_info->rest_read_compressed;
            if (uReadThis == 0)
                return UNZ_EOF;
            ptr = pfile_in_zip_read_info->filestream + pfile_in_zip_read_info->pos_in_zipfile + pfile_in_zip_read_info->byte_before_the_zipfile;
            if (ptr > endptr) {
                return UNZ_ERRNO;
            }

            memcpy(pfile_in_zip_read_info->read_buffer, ptr, uReadThis);

            pfile_in_zip_read_info->pos_in_zipfile += uReadThis;

            pfile_in_zip_read_info->rest_read_compressed-=uReadThis;

            pfile_in_zip_read_info->stream.next_in =
            (Bytef*)pfile_in_zip_read_info->read_buffer;
            pfile_in_zip_read_info->stream.avail_in = (uInt)uReadThis;
        }

        if ((pfile_in_zip_read_info->compression_method==0) || (pfile_in_zip_read_info->raw))
        {
            uInt uDoCopy,i ;

            if ((pfile_in_zip_read_info->stream.avail_in == 0) &&
                (pfile_in_zip_read_info->rest_read_compressed == 0))
                return (iRead==0) ? UNZ_EOF : (int)iRead;

            if (pfile_in_zip_read_info->stream.avail_out <
                pfile_in_zip_read_info->stream.avail_in)
                uDoCopy = pfile_in_zip_read_info->stream.avail_out ;
            else
                uDoCopy = pfile_in_zip_read_info->stream.avail_in ;

            for (i=0;i<uDoCopy;i++)
                *(pfile_in_zip_read_info->stream.next_out+i) =
                *(pfile_in_zip_read_info->stream.next_in+i);

            pfile_in_zip_read_info->total_out_64 = pfile_in_zip_read_info->total_out_64 + uDoCopy;

            pfile_in_zip_read_info->crc32 = crc32(pfile_in_zip_read_info->crc32,
                                                  pfile_in_zip_read_info->stream.next_out,
                                                  uDoCopy);
            pfile_in_zip_read_info->rest_read_uncompressed-=uDoCopy;
            pfile_in_zip_read_info->stream.avail_in -= uDoCopy;
            pfile_in_zip_read_info->stream.avail_out -= uDoCopy;
            pfile_in_zip_read_info->stream.next_out += uDoCopy;
            pfile_in_zip_read_info->stream.next_in += uDoCopy;
            pfile_in_zip_read_info->stream.total_out += uDoCopy;
            iRead += uDoCopy;
        }
        else
        {
            uint64_t uTotalOutBefore,uTotalOutAfter;
            const Bytef *bufBefore;
            uint64_t uOutThis;
            int flush=Z_SYNC_FLUSH;

            uTotalOutBefore = pfile_in_zip_read_info->stream.total_out;
            bufBefore = pfile_in_zip_read_info->stream.next_out;

            err=inflate(&pfile_in_zip_read_info->stream,flush);

            if ((err>=0) && (pfile_in_zip_read_info->stream.msg!=NULL))
                err = Z_DATA_ERROR;

            uTotalOutAfter = pfile_in_zip_read_info->stream.total_out;
            /* Detect overflow, because z_stream.total_out is uLong (32 bits) */
            if (uTotalOutAfter<uTotalOutBefore)
                uTotalOutAfter += 1LL << 32; /* Add maximum value of uLong + 1 */
            uOutThis = uTotalOutAfter-uTotalOutBefore;

            pfile_in_zip_read_info->total_out_64 = pfile_in_zip_read_info->total_out_64 + uOutThis;

            pfile_in_zip_read_info->crc32 =
            crc32(pfile_in_zip_read_info->crc32,bufBefore,
                  (uInt)(uOutThis));

            pfile_in_zip_read_info->rest_read_uncompressed -=
            uOutThis;

            iRead += (uInt)(uTotalOutAfter - uTotalOutBefore);

            if (err==Z_STREAM_END)
                return (iRead==0) ? UNZ_EOF : (int)iRead;
            if (err!=Z_OK)
                break;
        }
    }

    if (err==Z_OK)
        return (int)iRead;
    return err;
}

/*
 Close the file in zip opened with unzOpenCurrentFile
 Return UNZ_CRCERROR if all the file was read but the CRC is not good
 */
static int unzCloseCurrentFile(unz64_s *s) {
    int err=UNZ_OK;

    file_in_zip64_read_info_s *pfile_in_zip_read_info;
    if (s==NULL)
        return UNZ_PARAMERROR;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

    if (pfile_in_zip_read_info==NULL)
        return UNZ_PARAMERROR;

    if ((pfile_in_zip_read_info->rest_read_uncompressed == 0) &&
        (!pfile_in_zip_read_info->raw))
    {
        if (pfile_in_zip_read_info->crc32 != pfile_in_zip_read_info->crc32_wait)
            err=UNZ_CRCERROR;
    }

    free(pfile_in_zip_read_info->read_buffer);
    pfile_in_zip_read_info->read_buffer = NULL;
    if (pfile_in_zip_read_info->stream_initialised == Z_DEFLATED)
        inflateEnd(&pfile_in_zip_read_info->stream);

    pfile_in_zip_read_info->stream_initialised = 0;
    free(pfile_in_zip_read_info);

    s->pfile_in_zip_read=NULL;

    return err;
}

/*
 Locate the Central directory of the zipfile data (at the end, just before
 the global comment)
 */
static uint64_t unz64local_SearchCentralDir(uint8_t *filestream, size_t size) {
    unsigned char* buf;
    uint64_t uSizeFile;
    uint64_t uBackRead;
    uint64_t uMaxBack=0xffff; /* maximum size of global comment */
    uint64_t uPosFound=CENTRALDIRINVALID;

    uint8_t *endptr = filestream + size;

    uSizeFile = size;

    if (uMaxBack>uSizeFile)
        uMaxBack = uSizeFile;

    buf = (unsigned char*)MemAlloc(BUFREADCOMMENT+4);

    uint8_t *ptr = filestream;

    uBackRead = 4;
    while (uBackRead<uMaxBack)
    {
        uLong uReadSize;
        uint64_t uReadPos ;
        int i;
        if (uBackRead+BUFREADCOMMENT>uMaxBack)
            uBackRead = uMaxBack;
        else
            uBackRead+=BUFREADCOMMENT;
        uReadPos = uSizeFile-uBackRead ;

        uReadSize = ((BUFREADCOMMENT+4) < (uSizeFile-uReadPos)) ?
        (BUFREADCOMMENT+4) : (uLong)(uSizeFile-uReadPos);
        ptr += uReadPos;
        if (ptr + uReadSize > endptr)
            break;

        memcpy(buf, ptr, uReadSize);
        ptr += uReadSize;

        for (i=(int)uReadSize-3; (i--)>0;)
            if (((*(buf+i))==0x50) && ((*(buf+i+1))==0x4b) &&
                ((*(buf+i+2))==0x05) && ((*(buf+i+3))==0x06))
            {
                uPosFound = uReadPos+(unsigned)i;
                break;
            }

        if (uPosFound!=CENTRALDIRINVALID)
            break;
    }
    free(buf);
    return uPosFound;
}

/*
 Read the local header of the current zipfile
 Check the coherency of the local header and info in the end of central
 directory about this file
 store in *piSizeVar the size of extra info in local header
 (filename and size of extra field data)
 */
static int unz64local_CheckCurrentFileCoherencyHeader(unz64_s* s, uInt* piSizeVar,
                                                      uint64_t * poffset_local_extrafield,
                                                      uInt  * psize_local_extrafield) {
    uLong uMagic,uData,uFlags;
    uLong size_filename;
    uLong size_extra_field;
    int err=UNZ_OK;

    *piSizeVar = 0;
    *poffset_local_extrafield = 0;
    *psize_local_extrafield = 0;

    uint8_t *ptr = s->filestream;
    uint8_t *endptr = s->filestream + s->filesize;

    ptr += s->cur_file_info_internal.offset_curfile + s->byte_before_the_zipfile;

    if (ptr > endptr)
        return UNZ_ERRNO;

    if (err==UNZ_OK)
    {
        if (unz64local_getLong(&ptr, endptr,&uMagic) != UNZ_OK)
            err=UNZ_ERRNO;
        else if (uMagic!=0x04034b50)
            err=UNZ_BADZIPFILE;
    }

    if (unz64local_getShort(&ptr, endptr,&uData) != UNZ_OK)
        err=UNZ_ERRNO;
    /*
     else if ((err==UNZ_OK) && (uData!=s->cur_file_info.wVersion))
     err=UNZ_BADZIPFILE;
     */
    if (unz64local_getShort(&ptr, endptr,&uFlags) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getShort(&ptr, endptr,&uData) != UNZ_OK)
        err=UNZ_ERRNO;
    else if ((err==UNZ_OK) && (uData!=s->cur_file_info.compression_method))
        err=UNZ_BADZIPFILE;

    if ((err==UNZ_OK) && (s->cur_file_info.compression_method!=0) &&
        /* #ifdef HAVE_BZIP2 */
        (s->cur_file_info.compression_method!=Z_BZIP2ED) &&
        /* #endif */
        (s->cur_file_info.compression_method!=Z_DEFLATED))
        err=UNZ_BADZIPFILE;

    if (unz64local_getLong(&ptr, endptr,&uData) != UNZ_OK) /* date/time */
        err=UNZ_ERRNO;

    if (unz64local_getLong(&ptr, endptr,&uData) != UNZ_OK) /* crc */
        err=UNZ_ERRNO;
    else if ((err==UNZ_OK) && (uData!=s->cur_file_info.crc) && ((uFlags & 8)==0))
        err=UNZ_BADZIPFILE;

    if (unz64local_getLong(&ptr, endptr,&uData) != UNZ_OK) /* size compr */
        err=UNZ_ERRNO;
    else if (uData != 0xFFFFFFFF && (err==UNZ_OK) && (uData!=s->cur_file_info.compressed_size) && ((uFlags & 8)==0))
        err=UNZ_BADZIPFILE;

    if (unz64local_getLong(&ptr, endptr,&uData) != UNZ_OK) /* size uncompr */
        err=UNZ_ERRNO;
    else if (uData != 0xFFFFFFFF && (err==UNZ_OK) && (uData!=s->cur_file_info.uncompressed_size) && ((uFlags & 8)==0))
        err=UNZ_BADZIPFILE;

    if (unz64local_getShort(&ptr, endptr,&size_filename) != UNZ_OK)
        err=UNZ_ERRNO;
    else if ((err==UNZ_OK) && (size_filename!=s->cur_file_info.size_filename))
        err=UNZ_BADZIPFILE;

    *piSizeVar += (uInt)size_filename;

    if (unz64local_getShort(&ptr, endptr,&size_extra_field) != UNZ_OK)
        err=UNZ_ERRNO;
    *poffset_local_extrafield= s->cur_file_info_internal.offset_curfile +
    SIZEZIPLOCALHEADER + size_filename;
    *psize_local_extrafield = (uInt)size_extra_field;

    *piSizeVar += (uInt)size_extra_field;

    return err;
}

/* Open the current file in the zipfile for reading data.
 If there is no error and the file is opened, the return value is UNZ_OK.
 */
static int unzOpenCurrentFile3(unz64_s *file, int* method,
                                int* level, int raw, const char* password) {
    int err=UNZ_OK;
    uInt iSizeVar;
    unz64_s* s;
    file_in_zip64_read_info_s* pfile_in_zip_read_info;
    uint64_t offset_local_extrafield;  /* offset of the local extra field */
    uInt  size_local_extrafield;    /* size of the local extra field */

    if (file==NULL)
        return UNZ_PARAMERROR;
    s=(unz64_s*)file;
    if (!s->current_file_ok)
        return UNZ_PARAMERROR;

    if (s->pfile_in_zip_read != NULL)
        unzCloseCurrentFile(file);

    if (unz64local_CheckCurrentFileCoherencyHeader(s,&iSizeVar, &offset_local_extrafield,&size_local_extrafield)!=UNZ_OK)
        return UNZ_BADZIPFILE;

    pfile_in_zip_read_info = (file_in_zip64_read_info_s*)MemAlloc(sizeof(file_in_zip64_read_info_s));
    if (pfile_in_zip_read_info==NULL)
        return UNZ_INTERNALERROR;

    pfile_in_zip_read_info->read_buffer=(char*)MemAlloc(UNZ_BUFSIZE);
    pfile_in_zip_read_info->offset_local_extrafield = offset_local_extrafield;
    pfile_in_zip_read_info->size_local_extrafield = size_local_extrafield;
    pfile_in_zip_read_info->pos_local_extrafield=0;
    pfile_in_zip_read_info->raw=raw;

    pfile_in_zip_read_info->stream_initialised=0;

    if (method!=NULL)
        *method = (int)s->cur_file_info.compression_method;

    if (level!=NULL)
    {
        *level = 6;
        switch (s->cur_file_info.flag & 0x06)
        {
            case 6 : *level = 1; break;
            case 4 : *level = 2; break;
            case 2 : *level = 9; break;
        }
    }

    if ((s->cur_file_info.compression_method!=0) &&
        /* #ifdef HAVE_BZIP2 */
        (s->cur_file_info.compression_method!=Z_BZIP2ED) &&
        /* #endif */
        (s->cur_file_info.compression_method!=Z_DEFLATED))

        err=UNZ_BADZIPFILE;

    pfile_in_zip_read_info->crc32_wait=s->cur_file_info.crc;
    pfile_in_zip_read_info->crc32=0;
    pfile_in_zip_read_info->total_out_64=0;
    pfile_in_zip_read_info->compression_method = s->cur_file_info.compression_method;
    pfile_in_zip_read_info->filestream=s->filestream;
    pfile_in_zip_read_info->byte_before_the_zipfile=s->byte_before_the_zipfile;

    pfile_in_zip_read_info->stream.total_out = 0;

    if ((s->cur_file_info.compression_method==Z_BZIP2ED) && (!raw))
    {
        pfile_in_zip_read_info->raw=1;
    }
    else if ((s->cur_file_info.compression_method==Z_DEFLATED) && (!raw))
    {
        pfile_in_zip_read_info->stream.zalloc = (alloc_func)0;
        pfile_in_zip_read_info->stream.zfree = (free_func)0;
        pfile_in_zip_read_info->stream.opaque = (voidpf)0;
        pfile_in_zip_read_info->stream.next_in = 0;
        pfile_in_zip_read_info->stream.avail_in = 0;

        err=inflateInit2(&pfile_in_zip_read_info->stream, -MAX_WBITS);
        if (err == Z_OK)
            pfile_in_zip_read_info->stream_initialised=Z_DEFLATED;
        else
        {
            free(pfile_in_zip_read_info->read_buffer);
            free(pfile_in_zip_read_info);
            return err;
        }
        /* windowBits is passed < 0 to tell that there is no zlib header.
         * Note that in this case inflate *requires* an extra "dummy" byte
         * after the compressed stream in order to complete decompression and
         * return Z_STREAM_END.
         * In unzip, i don't wait absolutely Z_STREAM_END because I known the
         * size of both compressed and uncompressed data
         */
    }
    pfile_in_zip_read_info->rest_read_compressed =
    s->cur_file_info.compressed_size ;
    pfile_in_zip_read_info->rest_read_uncompressed =
    s->cur_file_info.uncompressed_size ;


    pfile_in_zip_read_info->pos_in_zipfile =
    s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER +
    iSizeVar;

    pfile_in_zip_read_info->stream.avail_in = (uInt)0;

    s->pfile_in_zip_read = pfile_in_zip_read_info;
    s->encrypted = 0;

    return err;
}

/*
 Turn in-memory zipfile data into an unzFile Handle.
 */
static unz64_s *unzOpenInternal(uint8_t *zipdata, size_t datasize) {
    unz64_s us;
    unz64_s *s;
    uint64_t central_pos;
    uLong   uL;

    uLong number_disk;          /* number of the current disk, used for
                                 spanning ZIP, unsupported, always 0*/
    uLong number_disk_with_CD;  /* number the disk with central dir, used
                                 for spanning ZIP, unsupported, always 0*/
    uint64_t number_entry_CD;      /* total number of entries in
                                    the central dir
                                    (same than number_entry on nospan) */

    int err=UNZ_OK;

    us.filestream = zipdata;
    if (us.filestream==NULL)
        return NULL;

    uint8_t *endptr = zipdata + datasize;

    central_pos = unz64local_SearchCentralDir64(us.filestream, datasize);
    if (central_pos!=CENTRALDIRINVALID)
    {
        uLong uS;
        uint64_t uL64;

        us.isZip64 = 1;

        uint8_t *ptr = zipdata + central_pos;
        if (ptr > endptr)
            return NULL;


        /* the signature, already checked */
        if (unz64local_getLong(&ptr, endptr,&uL)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* size of zip64 end of central directory record */
        if (unz64local_getLong64(&ptr, endptr,&uL64)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* version made by */
        if (unz64local_getShort(&ptr,endptr,&uS)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* version needed to extract */
        if (unz64local_getShort(&ptr,endptr,&uS)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* number of this disk */
        if (unz64local_getLong(&ptr,endptr,&number_disk)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* number of the disk with the start of the central directory */
        if (unz64local_getLong(&ptr,endptr,&number_disk_with_CD)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* total number of entries in the central directory on this disk */
        if (unz64local_getLong64(&ptr,endptr,&us.gi.number_entry)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* total number of entries in the central directory */
        if (unz64local_getLong64(&ptr,endptr,&number_entry_CD)!=UNZ_OK)
            err=UNZ_ERRNO;

        if ((number_entry_CD!=us.gi.number_entry) ||
            (number_disk_with_CD!=0) ||
            (number_disk!=0))
            err=UNZ_BADZIPFILE;

        /* size of the central directory */
        if (unz64local_getLong64(&ptr,endptr,&us.size_central_dir)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* offset of start of central directory with respect to the
         starting disk number */
        if (unz64local_getLong64(&ptr,endptr,&us.offset_central_dir)!=UNZ_OK)
            err=UNZ_ERRNO;

        us.gi.size_comment = 0;
    }
    else
    {
        central_pos = unz64local_SearchCentralDir(zipdata, datasize);
        if (central_pos==CENTRALDIRINVALID)
            err=UNZ_ERRNO;

        us.isZip64 = 0;

        uint8_t *ptr = zipdata + central_pos;
        if (ptr > endptr)
            return NULL;

        /* the signature, already checked */
        if (unz64local_getLong(&ptr,endptr,&uL)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* number of this disk */
        if (unz64local_getShort(&ptr,endptr,&number_disk)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* number of the disk with the start of the central directory */
        if (unz64local_getShort(&ptr,endptr,&number_disk_with_CD)!=UNZ_OK)
            err=UNZ_ERRNO;

        /* total number of entries in the central dir on this disk */
        if (unz64local_getShort(&ptr,endptr,&uL)!=UNZ_OK)
            err=UNZ_ERRNO;
        us.gi.number_entry = uL;

        /* total number of entries in the central dir */
        if (unz64local_getShort(&ptr,endptr,&uL)!=UNZ_OK)
            err=UNZ_ERRNO;
        number_entry_CD = uL;

        if ((number_entry_CD!=us.gi.number_entry) ||
            (number_disk_with_CD!=0) ||
            (number_disk!=0))
            err=UNZ_BADZIPFILE;

        /* size of the central directory */
        if (unz64local_getLong(&ptr,endptr,&uL)!=UNZ_OK)
            err=UNZ_ERRNO;
        us.size_central_dir = uL;

        /* offset of start of central directory with respect to the
         starting disk number */
        if (unz64local_getLong(&ptr,endptr,&uL)!=UNZ_OK)
            err=UNZ_ERRNO;
        us.offset_central_dir = uL;

        /* zipfile comment length */
        if (unz64local_getShort(&ptr,endptr,&us.gi.size_comment)!=UNZ_OK)
            err=UNZ_ERRNO;
    }

    if ((central_pos<us.offset_central_dir+us.size_central_dir) &&
        (err==UNZ_OK))
        err=UNZ_BADZIPFILE;

    if (err!=UNZ_OK)
    {
        return NULL;
    }

    us.byte_before_the_zipfile = central_pos -
    (us.offset_central_dir+us.size_central_dir);
    us.central_pos = central_pos;
    us.pfile_in_zip_read = NULL;
    us.encrypted = 0;

    s=(unz64_s*)MemAlloc(sizeof(unz64_s));
    memcpy(s, &us, sizeof(unz64_s));
    s->filesize = datasize;
    unzGoToFirstFile(s);

    return s;
}

static uint8_t *do_extract_currentfile(unz64_s *uf) {
    char filename_inzip[65536+1];
    int err=UNZ_OK;
    void* buf;
    uInt size_buf;

    unz_file_info64 file_info;
    err = unzGetCurrentFileInfo64(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

    if (err!=UNZ_OK)
    {
        fprintf(stderr, "error %d with zipfile in unzGetCurrentFileInfo\n",err);
        return NULL;
    }

    size_buf = file_info.uncompressed_size;
    buf = (void*)MemAlloc(size_buf);
    err = unzReadCurrentFile(uf,buf,size_buf);
    if (err<0) {
        fprintf(stderr, "error %d with zipfile in unzReadCurrentFile\n",err);
    }

    return buf;
}


static uint8_t *do_extract_onefile(unz64_s *uf, const char* filename) {
    if (unzLocateFile(uf,filename, 0) != UNZ_OK)
    {
        fprintf(stderr, "file %s not found in the zipfile\n",filename);
        return NULL;
    }

    if (unzOpenCurrentFile3(uf, NULL, NULL, 0, NULL) != UNZ_OK) {
        fprintf(stderr, "Error when opening file \"%s\" in archive\n",filename);
        return NULL;
    }

    return do_extract_currentfile(uf);
}

uint8_t *extract_file_from_zip_data(uint8_t *zipdata, size_t zipdatasize, const char *filename_to_extract, size_t *unzipped_size) {

    unz64_s *uf = unzOpenInternal(zipdata, zipdatasize);

    uint8_t *result = do_extract_onefile(uf, filename_to_extract);
    if (result)
        *unzipped_size = uf->cur_file_info.uncompressed_size;
    else
        *unzipped_size = 0;

    unzCloseCurrentFile(uf);
    free(uf);

    return result;
}
