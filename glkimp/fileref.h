#ifndef GLKIMP_FILEREF
#define GLKIMP_FILEREF

#include "glkimp.h"

#define BUFLEN (1024)

extern char tempdir[BUFLEN];

extern char *autosavedir;

extern void getworkdir(void);
extern void getautosavedir(char *file);
extern void gettempdir(void);
extern void gli_cleanup_tempdir(void);
extern void gli_sweep_abandoned_tempdirs(void);
extern int create_workdir(void);
extern int create_autosavedir(char *file);

#ifdef __OBJC__
#import <Foundation/Foundation.h>
extern BOOL gli_is_tempfile(NSString *path);
extern NSString *gli_restore_tempfile(NSString *oldpath, NSData *contents, BOOL overwrite);
extern void gli_encode_file_url(NSCoder *encoder, NSURL *URL);
extern NSURL *gli_decode_file_url(NSCoder *decoder, BOOL overwrite);
#endif

#endif
