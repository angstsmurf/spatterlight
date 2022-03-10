#ifndef diskimage_h
#define diskimage_h

#include <stdlib.h>

/* constants for the supported disk formats */
#define MAXTRACKS 80
#define MAXSECTORS 40

#define D64SIZE 174848
#define D64ERRSIZE 175531
#define D71SIZE 349696
#define D71ERRSIZE 351062
#define D81SIZE 819200
#define D81ERRSIZE 822400

typedef enum imagetype { D64 = 1, D71, D81 } ImageType;

typedef enum filetype {
  T_DEL = 0,
  T_SEQ,
  T_PRG,
  T_USR,
  T_REL,
  T_CBM,
  T_DIR
} FileType;

typedef struct ts {
  unsigned char track;
  unsigned char sector;
} TrackSector;

typedef struct diskimage {
  char *filename;
  int size;
  ImageType type;
  unsigned char *image;
  unsigned char *errinfo;
  TrackSector bam;
  TrackSector bam2;
  TrackSector dir;
  int openfiles;
  int blocksfree;
  int modified;
  int status;
  int interleave;
  TrackSector statusts;
} DiskImage;

typedef struct rawdirentry {
  TrackSector nextts;
  unsigned char type;
  TrackSector startts;
  unsigned char rawname[16];
  TrackSector relsidets;
  unsigned char relrecsize;
  unsigned char unused[4];
  TrackSector replacetemp;
  unsigned char sizelo;
  unsigned char sizehi;
} RawDirEntry;

typedef struct imagefile {
  DiskImage *diskimage;
  RawDirEntry *rawdirentry;
  char mode;
  int position;
  TrackSector ts;
  TrackSector nextts;
  unsigned char *buffer;
  int bufptr;
  int buflen;
  unsigned char visited[MAXTRACKS][MAXSECTORS];
} ImageFile;

DiskImage *di_load_image(const char *name);
DiskImage *di_create_image(char *name, int size);
void di_free_image(DiskImage *di);
void di_sync(DiskImage *di);

int di_status(DiskImage *di, char *status);

ImageFile *di_open(DiskImage *di, unsigned char *rawname, FileType type,
                   char *mode);
void di_close(ImageFile *imgfile);
int di_read(ImageFile *imgfile, unsigned char *buffer, int len);
int di_write(ImageFile *imgfile, unsigned char *buffer, int len);

unsigned char *di_get_ts_addr(DiskImage *di, TrackSector ts);
int di_get_ts_err(DiskImage *di, TrackSector ts);

int di_format(DiskImage *di, unsigned char *rawname, unsigned char *rawid);
int di_delete(DiskImage *di, unsigned char *rawpattern, FileType type);
int di_rename(DiskImage *di, unsigned char *oldrawname,
              unsigned char *newrawname, FileType type);

int di_sectors_per_track(ImageType type, int track);
int di_tracks(ImageType type);
int di_get_block_num(ImageType type, TrackSector ts);

TrackSector di_get_dir_ts(DiskImage *di);
unsigned char *di_title(DiskImage *di);
int di_track_blocks_free(DiskImage *di, int track);
int di_is_ts_free(DiskImage *di, TrackSector ts);
void di_alloc_ts(DiskImage *di, TrackSector ts);
void di_free_ts(DiskImage *di, TrackSector ts);

int di_rawname_from_name(unsigned char *rawname, const char *name);
int di_name_from_rawname(char *name, unsigned char *rawname);

RawDirEntry *find_largest_file_entry(DiskImage *di);
DiskImage *di_create_from_data(uint8_t *data, int length);

#endif /* diskimage_h */
