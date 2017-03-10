/*
  ifiction-xtract : a demo of the ifiction api
  L. Ross Raszewski
  This code is freely usable for all purposes.
 
  This work is licensed under the Creative Commons Attribution2.5 License.
  To view a copy of this license, visit
  http://creativecommons.org/licenses/by/2.5/ or send a letter to
  Creative Commons,
  543 Howard Street, 5th Floor,
  San Francisco, California, 94105, USA.
 
  To build:
  compile this file. Link it with babel_handler.c, blorb.c, register.c,
  ifiction.c, md5.c, misc.c, register_ifiction.c,
  and the babel treaty modules (zcode.c, glulx.c,
  tads.c, etc.) from the babel distribution.

  Or use the babel makefile to build the babel and ifiction libraries
  (babel.lib and ifiction.lib or babel.a and ifiction.a) and link this
  file to them

  This program demonstrates the use of the ifiction API to extract
  information from an ifiction record.  Using the babel handler API, it
  can accept either an ifiction file itself or it can extract the metadata
  segment from a story file. 

*/

#include "ifiction.h"
#include "babel_handler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void * my_malloc(int, char *);
int main(int argc, char **argv)
{
 int32 l=0;
 char *md=NULL, *ft;
 if (argc !=4)
 { printf("Usage: %s <input file> <context> <tag>\n"
          "input file may be a story or ifiction file. The program will look for\n"
          "the specified tag within the given container. Outputs the contents of\n"
          "the tag if found.\nExamples:\ncontext        tag\n"
          "bibliographic  title\n"
          "identification ifid\n"
          "zcode          serial\n",
          argv[0]);
          exit(1);
          }

 if (strcmp(argv[1],"-"))
  ft=babel_init(argv[1]);
 else
 {
  int ll=0;
  while(!feof(stdin))
  {
   char *tt, mdb[1024];
   int ii;
   ii=fread(mdb,1,1024,stdin);
   tt=(char *)my_malloc(ll+ii,"file buffer");
   if (md) { memcpy(tt,md,ll); free(md); }
   memcpy(tt+ll,mdb,ii);
   md=tt;
   ll+=ii;
   if (ii<1024) break;
  }
  ft=babel_init_raw(md,ll);
  free(md);
 }


 if (ft)
 {
  l=babel_treaty(GET_STORY_FILE_METADATA_EXTENT_SEL, NULL, 0);
 }

 if (l>0)
 {
  md=(char *)malloc(l);
  babel_treaty(GET_STORY_FILE_METADATA_SEL,md,l);
 }
 else
 {
  void *mmd=babel_get_file();


  if (!mmd) {  exit(1); }
  l=babel_get_length();
  md=(char *)malloc(l+1);
  md[l]=0;
  memcpy(md,mmd,l);
 }

 {
  char *r=ifiction_get_tag(md, argv[2], argv[3], NULL);

  if (r) {
   printf("%s\n",r);
   free(r);
   }

 }
 free(md);
 return 0;
}
