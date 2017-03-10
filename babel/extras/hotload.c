/* hotload.c : The babel registry hotloader
  L. Ross Raszewski
  This code is freely usable for all purposes.
 
  This work is licensed under the Creative Commons Attribution2.5 License.
  To view a copy of this license, visit
  http://creativecommons.org/licenses/by/2.5/ or send a letter to
  Creative Commons,
  543 Howard Street, 5th Floor,
  San Francisco, California, 94105, USA.
 

  This file provides the babel_hotload function, which can be used to
  implement a dynamic loader for babel treaty modules.

  int babel_hotload(char *tdir, char *cdir, load_treaty loader,
                    void *tctx, void *cctx);
  tdir:         the directory in which Treaty modules are stored
  cdir:         the directory in which container modules are stored
  loader:       this function, given a file name, loads the file from the
                current working directory and returns a TREATY function pointer.
  tctx:         Passed as second parameter to loader when loading a treaty module
  cctx:         Passed as second parameter to loader when loading a container

  babel_hotload returns the number of treaty modules loaded.

  The idea behind the babel hotloader is to provide support for a plugin-based
  architecture to provide treaty and container modules to a program using
  the babel_handler API. The idea is that plugins for each supported format
  (these would be DLL files in windows, or .so files in linux, DXE files in
  DOS, etc.) would be stored in some directory.  babel_hotload scans that
  directory and builds the babel module registry from its contents.

  To use the babel hotloader, link hotload.c instead of register.c,
  and invoke babel_hotload before using the babel API.
*/

#include "hotload.h"
#include <string.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif
int chdir(const char *);
char *getcwd(char *, int);
#ifdef __cplusplus
}
#endif

void * my_malloc(int, char *);
TREATY *treaty_registry;
TREATY *container_registry;
char **format_registry;

int babel_hotload(char *tdir, char *cdir, load_treaty hdlr, void *tctx, void *cctx)
{
DIR *d;
int i;
struct dirent *de;
char buf[512];
char cwd[1024];
getcwd(cwd,1024);
d=opendir(cdir);
if (d)
{
 chdir(cdir);
 i=0;
 for(de=readdir(d);de;de=readdir(d)) i++;
 i++;
 container_registry=(TREATY *)my_malloc(i*sizeof(TREATY),"Container Registry");
 rewinddir(d);
 i=0;
 for(de=readdir(d);de;de=readdir(d),i++) if (de->d_name[0]!='.')
  container_registry[i]=hdlr(de->d_name, cctx);
 container_registry[i]=NULL;
 closedir(d);
 chdir(cwd);
}
else
 container_registry=(TREATY *)my_malloc(sizeof(TREATY),"Container Registry");
d=opendir(tdir);
i=0;
if (d)
{
 chdir(tdir);
 for(de=readdir(d);de;de=readdir(d)) i++;
 i++;
 treaty_registry=(TREATY *)my_malloc(i*sizeof(TREATY),"Treaty Registry");
 format_registry=(char **)my_malloc(i*sizeof(char *),"Format Name Registry");
 rewinddir(d);
 i=0;
 for(de=readdir(d);de;de=readdir(d),i++) if (de->d_name[0]!='.')
 { treaty_registry[i]=hdlr(de->d_name, tctx);
   if (!treaty_registry[i]) i--;
   else {
   treaty_registry[i](GET_FORMAT_NAME_SEL,NULL,0,buf,512);
   format_registry[i]=strdup(buf);
   }
 }
 treaty_registry[i]=NULL;
 format_registry[i]=NULL;
 closedir(d);
 chdir(cwd);
}
else
{
 treaty_registry=(TREATY *)my_malloc(sizeof(TREATY),"Treaty Registry");
 format_registry=(char **)my_malloc(sizeof(char *),"Format Name Registry");

}

return i;
}
