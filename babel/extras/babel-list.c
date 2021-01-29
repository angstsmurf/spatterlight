/*
  babel-list : a demo of the babel handler api
  L. Ross Raszewski
  This code is freely usable for all purposes.
 
  This work is licensed under the Creative Commons Attribution 4.0 License.
  To view a copy of this license, visit
  https://creativecommons.org/licenses/by/4.0/ or send a letter to
  Creative Commons,
  PO Box 1866,
  Mountain View, CA 94042, USA.
 
  To build:
  compile this file. Link it with babel_handler.c, blorb.c, register.c,
  ifiction.c, md5.c, misc.c, register_ifiction.c,
  and the babel treaty modules (zcode.c, glulx.c,
  tads.c, etc.) from the babel distribution.

  Or use the babel makefile to build the babel and ifiction libraries
  (babel.lib and ifiction.lib or babel.a and ifiction.a) and link this
  file to them

  This program demonstrates use of the babel handler API and the
  babel ifiction API.  It scans all the files in the working directory
  and produces a list of stories, organized by author.


*/
#include "babel_handler.h"
#include "ifiction.h"
#include <dirent.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
int chdir(const char *);
char *getcwd(char *, int);
#ifdef __cplusplus
}
#endif



/* The output information */
struct iinfo {
        char auth[256];
        char title[256];
        char year[16];
        struct iinfo *next;
        };

/* A null error handler for ifiction_parse */
void erh(char *st, void *ctx)
{
 /* Suppress 'unused' warnings */
 if (st && ctx) { }

}

/* Comparitor function. First sort by author, then by title */
int asort(const void *a, const void *b)
{
 struct iinfo *aa=(struct iinfo *)a, *bb=(struct iinfo *)b;
 int i;
 i=strcmp(aa->auth,bb->auth);
 if (!i) i=strcmp(aa->title,bb->title);
 return i;
}

/* Close tag function for ifiction_parse: builds the iinfo as it closes
   the title, author, and publication date tags
*/
void fetchinfo(struct XMLTag *xtg, void *ctx)
{
 struct iinfo *inf=(struct iinfo *)ctx;
 if (strcmp(xtg->tag,"author")==0)
 {
  memcpy(inf->auth,xtg->begin,xtg->end-xtg->begin);
  inf->auth[xtg->end-xtg->begin]=0;
 }
 if (strcmp(xtg->tag,"firstpublished")==0)
 {
  memcpy(inf->year,xtg->begin,xtg->end-xtg->begin);
  inf->year[xtg->end-xtg->begin]=0;
 }
 if (strcmp(xtg->tag,"title")==0)
 {
  memcpy(inf->title,xtg->begin,xtg->end-xtg->begin);
  inf->title[xtg->end-xtg->begin]=0;
 }
}

int main(int argc, char **argv)
{
DIR *dd;
struct iinfo *stories=NULL;
struct iinfo *nn;
struct dirent *de;
char *la="NULL";
int i=0, j;
char cwd[512];

/* Read the current directory */
getcwd(cwd,512);
dd=opendir(cwd);
if (!dd) exit(1);
if (argc || argv) { }
while((de=readdir(dd))!=0)
{
 char *md;
 /* Send the file to the babel handler */
 if (de->d_name[0]=='.' || !babel_init(de->d_name)) continue;
 /* Snag the metadata */
 j=babel_treaty(GET_STORY_FILE_METADATA_EXTENT_SEL,NULL,0);
 if (!j) continue;
 md=(char *)malloc(j);
 babel_treaty(GET_STORY_FILE_METADATA_SEL,md,j);
 nn=(struct iinfo *)malloc(sizeof(struct iinfo));
 nn->next=stories;
 stories=nn;
 /* Fetch the info */
 ifiction_parse(md,fetchinfo,stories,erh,NULL);
 i++;
 free(md);
 babel_release();
}
closedir(dd);
nn=stories;
/* Convert the linked list into an array, and sort it */
stories=(struct iinfo *) malloc(i*sizeof(struct iinfo));
i=0;
while(nn)
{
 struct iinfo *nt;
 stories[i++]=*nn;
 nt=nn;
 nn=nn->next;
 free(nt);
}
qsort(stories,i,sizeof(struct iinfo),asort);
/* Print the results */
for(j=0;j<i;j++)
{
 if (strcmp(la,stories[j].auth))
  printf("%s\n",stories[j].auth);
 la=stories[j].auth;
 printf(" %s (%s)\n",stories[j].title, stories[j].year);
}
return 0;
}
