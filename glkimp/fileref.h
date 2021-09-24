#include "glkimp.h"

#define BUFLEN (1024)

extern char tempdir[BUFLEN];

extern char *autosavedir;
extern char *gli_parentdir;

extern void getworkdir(void);
extern void getautosavedir(char *file);
extern void gettempdir(void);
