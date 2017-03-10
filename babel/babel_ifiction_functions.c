/* babel_ifiction_functions.c babel top-level operations for ifiction
 * (c) 2006 By L. Ross Raszewski
 *
 * This code is freely usable for all purposes.
 *
 * This work is licensed under the Creative Commons Attribution2.5 License.
 * To view a copy of this license, visit
 * http://creativecommons.org/licenses/by/2.5/ or send a letter to
 * Creative Commons,
 * 543 Howard Street, 5th Floor,
 * San Francisco, California, 94105, USA.
 *
 * This file depends upon babel.c (for rv), babel.h, misc.c and ifiction.c
 */

#include "babel.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifndef THREE_LETTER_EXTENSIONS
#define IFICTION_EXT ".iFiction"
#else
#define IFICTION_EXT ".ifi"
#endif

void *my_malloc(int, char *);

struct IFiction_Info
{
 char ifid[256];
 int wmode;
};

static void write_story_to_disk(struct XMLTag *xtg, void *ctx)
{
 char *b, *ep;
 char *begin, *end;
 char buffer[TREATY_MINIMUM_EXTENT];
 int32 l, j;
 if (ctx) { }

 if (strcmp(xtg->tag,"story")==0)
 {
 begin=xtg->begin;
 end=xtg->end;
 l=end-begin+1;
 b=(char *)my_malloc(l,"XML buffer");
 memcpy(b,begin,l-1);
 b[l]=0;
 j=ifiction_get_IFID(b,buffer,TREATY_MINIMUM_EXTENT);
 if (!j)
 {
  fprintf(stderr,"No IFID found for this story\n");
  free(b);
  return;
 }
 ep=strtok(buffer,",");
 while(ep)
 {
  char buf2[256];
  FILE *f;
  sprintf(buf2,"%s%s",ep,IFICTION_EXT);
  f=fopen(buf2,"w");

  if (!f ||
  fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!-- Metadata extracted by Babel -->"
        "<ifindex version=\"1.0\" xmlns=\"http://babel.ifarchive.org/protocol/iFiction/\">\n"
        " <story>",
        f)==EOF ||
  fputs(b,f)==EOF ||
  fputs("/<story>\n</ifindex>\n",f)==EOF
  )
  {
   fprintf(stderr,"Error writing to file %s\n",buf2);
  } else
   printf("Extracted %s\n",buf2);
  if (f) fclose(f);

  ep=strtok(NULL,",");
 }

 free(b);
 }
}

void babel_ifiction_ifid(char *md)
{
 char output[TREATY_MINIMUM_EXTENT];
 int i;
 char *ep;
 i=ifiction_get_IFID(md,output,TREATY_MINIMUM_EXTENT);
 if (!i)

 {
  fprintf(stderr,"Error: No IFIDs found in iFiction file\n");
  return;
 }
 ep=strtok(output,",");
 while(ep)
 {
  printf("IFID: %s\n",ep);
  ep=strtok(NULL,",");
 }

}

static char isok;

static void examine_tag(struct XMLTag *xtg, void *ctx)
{
 struct IFiction_Info *xti=(struct IFiction_Info *)ctx;

 if (strcmp("ifid",xtg->tag)==0 && strcmp(xti->ifid,"UNKNOWN")==0)
 {
 memcpy(xti->ifid,xtg->begin,xtg->end-xtg->begin);
 xti->ifid[xtg->end-xtg->begin]=0;
 }

}
static void verify_eh(char *e, void *ctx)
{
 if (*((int *)ctx) < 0) return;
 if (*((int *)ctx) || strncmp(e,"Warning",7))
  { isok=0;
   fprintf(stderr, "%s\n",e);
  }
}



void babel_ifiction_fish(char *md)
{
 int i=-1;
 ifiction_parse(md,write_story_to_disk,NULL,verify_eh,&i);
}

void deep_ifiction_verify(char *md, int f)
{
 struct IFiction_Info ii;
 int i=0;
 ii.wmode=0;
 isok=1;
 strcpy(ii.ifid,"UNKNOWN");
 ifiction_parse(md,examine_tag,&ii,verify_eh,&i);
 if (f&& isok) printf("Verified %s\n",ii.ifid);
}
void babel_ifiction_verify(char *md)
{
 deep_ifiction_verify(md,1);

}


void babel_ifiction_lint(char *md)
{
 struct IFiction_Info ii;
 int i=1;
 ii.wmode=1;
 isok=1;
 strcpy(ii.ifid,"UNKNOWN");
 ifiction_parse(md,examine_tag,&ii,verify_eh,&i);
 if (isok) printf("%s conforms to iFiction style guidelines\n",ii.ifid);
}


