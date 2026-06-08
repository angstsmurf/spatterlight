//
//  ciderpress.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-29.
//  This is based on parts of the Ciderpress source.
//  See https://github.com/fadden/ciderpress for the full version.
//
//  Includes fragments from a2tools by Terry Kyriacopoulos and Paul Schlyter
//

#ifndef ciderpress_h
#define ciderpress_h

void InitNibImage(uint8_t *data, size_t extent);
void InitDskImage(uint8_t *data, size_t extent);
void FreeDiskImage(void);

typedef struct {
    char *filename;
    uint8_t *data;
    size_t datasize;
    size_t index;
} A2FileRec;

uint8_t *ReadImageFromNib(size_t offset, size_t size, uint8_t *data, size_t datasize);
uint8_t *ReadFullImageFromNib(uint8_t *data, size_t datasize);
uint8_t *ReadApple2DOSFile(uint8_t *data, size_t *len, uint8_t **invimg, size_t *invimglen, uint8_t **m2, uint8_t **m3, size_t *m3len);
A2FileRec *GetAllApple2DOSFiles(uint8_t *data, size_t len, size_t *number_of_files);

/*
 * Extract every ProDOS BIN file from a disk image (DOS sector order), used by
 * the Apple II Comprehend games whose game-file directory is unlinked from the
 * volume root. Same ownership contract as GetAllApple2DOSFiles.
 */
A2FileRec *GetAllProDOSFiles(uint8_t *data, size_t len, size_t *number_of_files);

/* Select sector ordering for ProDOS block reads (nonzero = DOS order). */
void SetProDOSImageOrder(int isDOSOrder);

uint8_t *ReadInfocomV6File(uint8_t *data, size_t *len, int *game, int *diskindex);

#endif /* ciderpress_h */
