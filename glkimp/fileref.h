#include "glkimp.h"

#define BUFLEN (1024)

extern char workingdir[BUFLEN];
extern char autosavedir[BUFLEN];
extern char tempdir[BUFLEN];


extern void getworkdir(void);
extern void getautosavedir(char *file);
extern void gettempdir(void);
