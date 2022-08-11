//
//  stdetect.c
//  plus
//
//  Created by Administrator on 2022-08-02.
//

#define MAX_ST_LENGTH 368640
#define MIN_ST_LENGTH 213610

#include <stdlib.h>
#include <string.h>

#include "definitions.h"
#include "common.h"

#include "stdetect.h"

struct fat_boot_sector {
//    uint8_t    ignored[3];     /* Boot strap short or near jump */
//    uint8_t    system_id[8];   /* Name - can be used to special case
//                             partition manager volumes */
    uint16_t    sector_size; /* bytes per logical sector */
    uint8_t     sec_per_clus;   /* sectors/cluster */
    uint16_t    reserved;       /* reserved sectors */
    uint8_t     fats;           /* number of FATs */
    uint16_t    dir_entries;    /* root directory entries */
    uint16_t    sectors;        /* number of sectors */
    uint8_t     media;          /* media code */
    uint16_t    fat_length;     /* sectors/FAT */
    uint16_t    secs_track;     /* sectors per track */
    uint32_t    heads;          /* number of heads */
    uint32_t    total_sect;     /* number of sectors (if sectors == 0) */

    uint32_t    fat32_length;   /* sectors for FAT32 */
    uint16_t    flags;          /* bit 8: fat mirroring, low 4: active fat */
    uint8_t     boot_signature; /* Boot signature */
    uint32_t    volume_id;      /* Volume id */
    uint8_t     vol_label[12];  /* Volume label */
    uint8_t     system_type;    /* filesystem type */
    uint16_t    backup_boot;    /* backup boot sector */
};

#define MSDOS_NAME      11

struct msdos_dir_entry {
    char    name[MSDOS_NAME];/* name and extension */
    uint8_t    attr;           /* attribute bits */
    uint8_t    lcase;          /* Case for base and extension */
    uint8_t    ctime_cs;       /* Creation time, centiseconds (0-199) */
    uint16_t  ctime;          /* Creation time */
    uint16_t  cdate;          /* Creation date */
    uint16_t  adate;          /* Last access date */
    uint16_t  starthi;        /* High 16 bits of cluster in FAT32 */
    uint16_t  time,date,start;/* time, date and first cluster */
    uint32_t  size;           /* file size (in bytes) */
};

struct fat_boot_sector boot;
struct msdos_dir_entry dir;


uint16_t Read16LE(uint8_t **indata) {
    uint8_t *ptr = *indata;

    uint16_t val = *ptr++;
    val += *ptr++ * 0x100;
    *indata = ptr;
    return val;
}

typedef struct MsaImageInfo {
    int sectorsize;
    int starttrack;
    int endtrack;
    int sectors_per_track;
    int numheads;
    int numtracks;
    int totalsectors;
} MsaImageInfo;

#define READ_M16(address, offset) ((address[offset + 1] & 0xff) | ((address[offset] & 0xff) << 8))

void ReadMsaImageInfo(MsaImageInfo *msa_image_info, uint8_t *msa_image)
{
    msa_image_info->sectorsize = 512;
    msa_image_info->sectors_per_track = READ_M16(msa_image, 2);
    msa_image_info->numheads = READ_M16(msa_image, 4) + 1;
    msa_image_info->starttrack = READ_M16(msa_image, 6);
    msa_image_info->endtrack = READ_M16(msa_image, 8);
    msa_image_info->numtracks = msa_image_info->endtrack + 1;
    msa_image_info->totalsectors = msa_image_info->numtracks * msa_image_info->sectors_per_track * msa_image_info->numheads;
}

uint8_t *DecodeMsaImageToRawImage(uint8_t *msa_image, size_t *newsize)
{
    MsaImageInfo msa_image_info;

    ReadMsaImageInfo(&msa_image_info, msa_image);
    *newsize = msa_image_info.totalsectors * msa_image_info.sectorsize;
    uint8_t *raw_image = MemAlloc(*newsize);

    uint8_t *msa_pointer = msa_image + 10;
    uint8_t *raw_pointer = raw_image + msa_image_info.starttrack * msa_image_info.sectors_per_track * msa_image_info.numheads * msa_image_info.sectorsize;
    uint8_t *end_pointer;
    int trackindex;
    int headindex;
    int numbytes;
    uint8_t msa_data;
    uint8_t rle_data;
    int rle_count;

    for (trackindex = msa_image_info.starttrack; trackindex <= msa_image_info.endtrack; trackindex++) {
        for (headindex = 0; headindex < msa_image_info.numheads; headindex++) {
            numbytes = (*msa_pointer++ & 0xff) << 8;
            numbytes |= *msa_pointer++ & 0xff;

            if (numbytes < msa_image_info.sectors_per_track * msa_image_info.sectorsize) {
                end_pointer = msa_pointer + numbytes;

                while (msa_pointer < end_pointer) {
                    msa_data = *msa_pointer++;

                    if(msa_data != 0xe5) {
                        *raw_pointer++ = msa_data;
                    } else {
                        rle_data = *msa_pointer++;

                        rle_count = (*msa_pointer++ & 0xff) << 8;
                        rle_count |= *msa_pointer++ & 0xff;

                        while(rle_count) {
                            *raw_pointer++ = rle_data;
                            rle_count--;
                        }
                    }
                }
            } else {
                while(numbytes > 0) {
                    *raw_pointer++ = *msa_pointer++;
                    numbytes--;
                }
            }
        }
    }

    return raw_image;
}

int ReadFAT12BootSector(uint8_t **sf, size_t *extent) {
    uint8_t *ptr = &(*sf)[11];
    boot.sector_size = Read16LE(&ptr);
    debug_print("sector_size: %d\n", boot.sector_size);
    boot.sec_per_clus = *ptr++;
    debug_print("sec_per_clus: %d\n", boot.sec_per_clus);
    boot.reserved = Read16LE(&ptr);
    debug_print("reserved: %d\n", boot.reserved);
    boot.fats = *ptr++;
    debug_print("fats: %d\n", boot.fats);
    boot.dir_entries = Read16LE(&ptr);
    debug_print("dir_entries: %d\n", boot.dir_entries);
    boot.sectors = Read16LE(&ptr);
    debug_print("sectors: %d\n", boot.sectors);
    boot.media = *ptr++;
    debug_print("media: %d\n", boot.media);
    boot.fat_length = Read16LE(&ptr);
    debug_print("fat_length: %d\n", boot.fat_length);
    boot.secs_track = Read16LE(&ptr);
    debug_print("secs_track: %d\n", boot.secs_track);
    boot.heads = Read16LE(&ptr);
    debug_print("heads: %d\n", boot.heads);
    ptr += 10;
    boot.boot_signature = *ptr++;
    debug_print("boot_signature: %d\n", boot.boot_signature);
    ptr += 4;
    for (int i = 0; i < 11; i++) {
        boot.vol_label[i] = *ptr++;
    }
    boot.vol_label[11] = 0;
    return 1;
}

uint8_t *ReadFAT12DirEntry(uint8_t *ptr) {
    memcpy(dir.name, ptr, MSDOS_NAME + 3);
    ptr += MSDOS_NAME + 3;
    dir.ctime = Read16LE(&ptr);
    dir.cdate = Read16LE(&ptr);
    dir.adate = Read16LE(&ptr);
    dir.starthi = Read16LE(&ptr);
    dir.time = Read16LE(&ptr);
    dir.date = Read16LE(&ptr);
    dir.start = Read16LE(&ptr);
    dir.size = Read16LE(&ptr);
    dir.size += (Read16LE(&ptr) << 16);
    return ptr;
}

uint8_t read_fat12(uint8_t **sf, size_t offset, size_t fat_offset) {
    return (*sf)[fat_offset + offset];
}

/*
 * Return the next cluster in the chain of the provided cluster from the FAT.
 * If the cluster you find is out of bounds, return 0
 *
 * For FAT12, two entries are stored in 3 bytes. if the bytes are uv, wx, yz then the entries are xuv and yzw
 */
uint32_t get_next_cluster12(uint8_t *sf, uint32_t cluster) {
    uint8_t fat_entry[2];
    uint32_t new_clust;

    int fat_offset = boot.reserved * boot.sector_size;
    // If the cluster number is odd, we're getting the last half of the three bytes.
    if (cluster % 2) {
        memcpy(&fat_entry[0], &sf[fat_offset + ((cluster/2)*3)+1], 1);
        memcpy(&fat_entry[1], &sf[fat_offset + ((cluster/2)*3)+2], 1);
        memcpy(&new_clust, &fat_entry[0], 2);
        new_clust = new_clust>>4;
    }
    // If it's even we're getting the first half.
    else {
        memcpy(&fat_entry[0], &sf[fat_offset + ((cluster/2)*3)], 1);
        memcpy(&fat_entry[1], &sf[fat_offset + ((cluster/2)*3)+1], 1);
        fat_entry[1] = fat_entry[1]&0x0f;
        memcpy(&new_clust, &fat_entry[0], 2);
    }
    // for FAT12, the valid entries are between 0x2 and0xfef.
    if ((new_clust > 2) && (new_clust < 4079)) {
        return new_clust;
    }
    return 0;
}

uint8_t *GetFile(uint8_t *sf, int cluster) {

    size_t datasection = boot.reserved + boot.fats * boot.fat_length + (boot.dir_entries * 32 / boot.sector_size);
    int bytespercluster = boot.sector_size * boot.sec_per_clus;
    uint8_t *result = MemAlloc(dir.size);
    size_t offset = 0;
    while (offset < dir.size && cluster) {
        size_t bytestoread = MIN(bytespercluster,  dir.size - offset);
        if (offset + bytestoread > dir.size) {
            bytestoread = dir.size - offset;
        }
        size_t offset2 = (datasection + (cluster - 2) * boot.sec_per_clus) * boot.sector_size;
        uint8_t *ptr = sf + offset2;
        memcpy(result + offset, ptr, bytestoread);
        offset += bytestoread;
        cluster = get_next_cluster12(sf, cluster);
    }

    return result;
}

size_t writeToFile(const char *name, uint8_t *data, size_t size);

int issagaimg(const char *name);

int DetectST(uint8_t **sf, size_t *extent) {
    if (*extent > MAX_ST_LENGTH || *extent < MIN_ST_LENGTH)
        return 0;

    if ((*sf)[0] == 0x0e && (*sf)[1] == 0x0f) {
        uint8_t *new = DecodeMsaImageToRawImage(*sf, extent);
        free(*sf);
        *sf = new;
        writeToFile("/Users/administrator/Desktop/QP3.ST", *sf, *extent);
    }

    ReadFAT12BootSector(sf, extent);

    size_t root_entry_offset = (boot.reserved + boot.fats * boot.fat_length) * boot.sector_size;

    uint8_t *ptr = &(*sf)[root_entry_offset];
    uint8_t *database = NULL;
    size_t databasesize = 0;
    int found = 0;

    struct imgrec imgs[100];
    int imgidx = 0;
    
    for (int i = 0; i < 143; i++) {
        ptr = ReadFAT12DirEntry(ptr);
        dir.name[8] = 0;
        if (strncmp(dir.name, "DATABASE", 8) == 0) {
            uint8_t *file = GetFile(*sf, dir.start);
            database = MemAlloc(dir.size + 2);
            memcpy(database + 2, file, dir.size);
            databasesize = dir.size + 2;
            CurrentSys = SYS_ST;
            found++;
        } else if (issagaimg(dir.name) && imgidx < 100) {
            imgs[imgidx].data = GetFile(*sf, dir.start);
            imgs[imgidx].filename = MemAlloc(5);
            memcpy(imgs[imgidx].filename, dir.name, 4);
            imgs[imgidx].filename[4] = 0;
            imgs[imgidx].size = dir.size;
            debug_print("Adding image %d: %s\n", imgidx, imgs[imgidx].filename);
            imgidx++;
        }

    }

    if (found) {
        free(*sf);
        *sf = database;
        *extent = databasesize;
        if (imgidx) {
            Images = MemAlloc((imgidx + 1) * sizeof(imgrec));
            memcpy(Images, imgs, imgidx * sizeof(imgrec));
            Images[imgidx].filename = NULL;
        }
        return 1;
    }

    return 0;
}
