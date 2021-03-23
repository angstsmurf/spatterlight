/* babel_story_functions.c babel top-level operations for story files
 * (c) 2006 By L. Ross Raszewski
 *
 * This code is freely usable for all purposes.
 *
 * This work is licensed under the Creative Commons Attribution 4.0 License.
 * To view a copy of this license, visit
 * https://creativecommons.org/licenses/by/4.0/ or send a letter to
 * Creative Commons,
 * PO Box 1866,
 * Mountain View, CA 94042, USA.
 *
 * This file depends upon babel_handler.c, babel.h, and misc.c
 */

#include "babel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef THREE_LETTER_EXTENSIONS
#define IFICTION_EXT ".iFiction"
#else
#define IFICTION_EXT ".ifi"
#endif
void *my_malloc(uint32, char *);

void babel_story_ifid()
{
  char buffer[TREATY_MINIMUM_EXTENT];
  char *ep;
  int i;
  i=babel_treaty(GET_STORY_FILE_IFID_SEL,buffer,TREATY_MINIMUM_EXTENT);
  ep=strtok(buffer, ",");
  while(ep)
  {
   printf("IFID: %s\n",ep);
   ep=strtok(NULL,",");
  }
  if (!i)
    fprintf(stderr,"Unable to create an IFID (A serious problem occurred while loading the file).\n");

}


void babel_story_format()
{
  char *b;
  b=babel_get_format();
  if (!b) b="unknown";
  if (!babel_get_authoritative())
   printf("Format: %s (non-authoritative)\n",b);   
  else printf("Format: %s\n",b);
}

static void deep_babel_ifiction(char stopped)
{
  char buffer[TREATY_MINIMUM_EXTENT];
  char *md;
  char *ep;
  int32 i;
  FILE *f;

  if (stopped!=2)
  {
  i=babel_treaty(GET_STORY_FILE_IFID_SEL,buffer,TREATY_MINIMUM_EXTENT);
  if (i==0 && !babel_md5_ifid(buffer, TREATY_MINIMUM_EXTENT))
  {
   fprintf(stderr,"Unable to create an IFID (A serious problem occurred while loading the file).\n");
   return;
  }


  ep=strtok(buffer, ",");
  }
  else ep="-";
  i=babel_treaty(GET_STORY_FILE_METADATA_EXTENT_SEL,NULL,0);
  if (i<=0)
  {
   if (stopped) printf("No iFiction record for %s\n",buffer);
   return;
  }
  md=(char *)my_malloc(i,"Metadata buffer");
  if (babel_treaty(GET_STORY_FILE_METADATA_SEL,md,i)<0)
  {
    fprintf(stderr,"A serious error occurred while retrieving metadata.\n");
    free(md);
    return;
  }
  while(ep)
  {
   char epb[TREATY_MINIMUM_EXTENT+9];
   if (stopped!=2)
   {
   strcpy(epb,ep);
   strcat(epb, IFICTION_EXT);

   f=fopen(epb,"w");
   }
   else f=stdout;

   if (!f || fputs(md,f)==EOF)
    fprintf(stderr,"A serious error occurred writing to disk.\n");
   else if (stopped!=2) printf("Extracted %s\n",epb);
   if (f) fclose(f);
   if (stopped) break;
   ep=strtok(NULL,",");
  }
  free(md);
}

void babel_story_ifiction()
{
 deep_babel_ifiction(1);
}
static char *get_jpeg_dim(void *img, int32 extent)
{
  unsigned char *dp=(unsigned char *) img;
  unsigned char *ep=dp+extent;
  static char buffer[256];
  unsigned int t1, t2, w, h;


  t1=*(dp++);
  t2=*(dp++);
  if (t1!=0xff || t2!=0xD8 )
  {
   return "(invalid)";
  }

  while(1)
  {
   if (dp>ep) return "(invalid)";
   for(t1=*(dp++);t1!=0xff;t1=*(dp++)) if (dp>ep) return "(invalid)";
   do { t1=*(dp++); if (dp>ep) return "(invalid 4)";} while (t1 == 0xff);

   if ((t1 & 0xF0) == 0xC0 && !(t1==0xC4 || t1==0xC8 || t1==0xCC))
   {
    dp+=3;
    if (dp>ep) return "(invalid)";
    h=*(dp++) << 8;
    if (dp>ep) return "(invalid)";
    h|=*(dp++);
    if (dp>ep) return "(invalid)";
    w=*(dp++) << 8;
    if (dp>ep) return "(invalid)";
    w|=*(dp);
    sprintf(buffer, "(%dx%d)",w,h);
    return buffer;
   }
   else if (t1==0xD8 || t1==0xD9)
    break;
   else
   { int l;

    if (dp>ep) return "(invalid)";
     l=*(dp++) << 8;
    if (dp>ep) return "(invalid)";
     l|= *(dp++);
     l-=2;
     dp+=l;
     if (dp>ep) return "(invalid)";
    }
   }
  return "(invalid)";
}

static uint32 read_int(unsigned char  *mem)
{
  uint32 i4 = mem[0],
                    i3 = mem[1],
                    i2 = mem[2],
                    i1 = mem[3];
  return i1 | (i2<<8) | (i3<<16) | (i4<<24);
}


static char *get_png_dim(void *img, int32 extent)
{
 unsigned char *dp=(unsigned char *)img;
 static char buffer[256];
 uint32 w, h;
 if (extent<33 ||
 !(dp[0]==137 && dp[1]==80 && dp[2]==78 && dp[3]==71 &&
        dp[4]==13 && dp[5] == 10 && dp[6] == 26 && dp[7]==10)||
 !(dp[12]=='I' && dp[13]=='H' && dp[14]=='D' && dp[15]=='R'))
 return "(invalid)";
 w=read_int(dp+16);
 h=read_int(dp+20);
 sprintf(buffer,"(%dx%d)",w,h);
 return buffer;
}
static char *get_image_dim(void *img, int32 extent, int fmt)
{
 if (fmt==JPEG_COVER_FORMAT) return get_jpeg_dim(img,extent);
 else if (fmt==PNG_COVER_FORMAT) return get_png_dim(img, extent);
 return "(unknown)";

}
static void deep_babel_cover(char stopped)
{
  char buffer[TREATY_MINIMUM_EXTENT];
  void *md;
  char *ep;
  char *ext;
  char *dim;
  int32 i,j;
  FILE *f;
  i=babel_treaty(GET_STORY_FILE_IFID_SEL,buffer,TREATY_MINIMUM_EXTENT);
  if (i==0)
   if (babel_md5_ifid(buffer, TREATY_MINIMUM_EXTENT))
    printf("IFID: %s\n",buffer);
   else
    {
     fprintf(stderr,"Unable to create an IFID (A serious problem occurred while loading the file).\n");
     return;
    }
  else 

  ep=strtok(buffer, ",");
  i=babel_treaty(GET_STORY_FILE_COVER_EXTENT_SEL,NULL,0);
  j=babel_treaty(GET_STORY_FILE_COVER_FORMAT_SEL,NULL,0);

  if (i<=0 || j<=0)
  {
   if (stopped) printf("No cover art for %s\n",buffer);
   return;
  }
  if (j==PNG_COVER_FORMAT) ext=".png";
  else if (j==JPEG_COVER_FORMAT) ext=".jpg";
  md=my_malloc(i,"Image buffer");
  if (babel_treaty(GET_STORY_FILE_COVER_SEL,md,i)<0)
  {
    fprintf(stderr,"A serious error occurred while retrieving cover art.\n");
    free(md);
    return;
  }
  dim=get_image_dim(md,i,j);
  while(ep)
  {
   char epb[TREATY_MINIMUM_EXTENT+9];
   strcpy(epb,ep);
   strcat(epb, ext);

   f=fopen(epb,"wb");
   if (!f || fwrite(md,1,i,f)==EOF)
    fprintf(stderr,"A serious error occurred writing to disk.\n");
   else printf("Extracted %s %s\n",epb, dim);
   if (f) fclose(f);
   if (stopped) break;
   ep=strtok(NULL,",");
  }
  free(md);
}

void babel_story_cover()
{
 deep_babel_cover(1);
}

void babel_story_fish()
{
 deep_babel_ifiction(0);
 deep_babel_cover(0);
}

static char *get_biblio(void)
{
 int32 i;
 char *md;
 char *bib="No bibliographic data";
 char *bibb; char *bibe; 
 char *t;
 static char buffer[TREATY_MINIMUM_EXTENT];

 i=babel_treaty(GET_STORY_FILE_METADATA_EXTENT_SEL,NULL,0);
 if (i<=0) return bib;

 md=(char *) my_malloc(i,"Metadata buffer");
 if (babel_treaty(GET_STORY_FILE_METADATA_SEL,md,i)<0) return bib;
 
 bibb=strstr(md,"<bibliographic>");
 if (!bibb) { free(md); return bib; }
 bibe=strstr(bibb,"</bibliographic>");
 if (bibe) *bibe=0;
 t=strstr(bibb,"<title>");
 if (t)
 {
  t+=7;
  bibe=strstr(t,"</title>");
  if (bibe)
  {
    *bibe=0;
    bib=buffer;
    for(i=0;t[i];i++) if (t[i]<0x20 || t[i]>0x7e) t[i]='_';
    sprintf(buffer, "\"%s\" ",t);
    *bibe='<';
  }
  else strcpy(buffer,"<no title found> ");
 }
 t=strstr(bibb,"<author>");
 if (t)
 {
  t+=8;
  bibe=strstr(t,"</author>");
  if (bibe)
  {
    bib=buffer;
    *bibe=0;
    for(i=0;t[i];i++) if (t[i]<0x20 || t[i]>0x7e) t[i]='_';
    strcat(buffer, "by ");
    strcat(buffer, t);
    *bibe='<';
  }
  else strcat(buffer, "<no author found>");
 }
 free(md);
 return bib;

}
void babel_story_identify()
{
 int32 i, j, l;
 char *b, *cf, *dim;
 char buffer[TREATY_MINIMUM_EXTENT];

 printf("%s\n",get_biblio());
 babel_story_ifid();
 b=babel_get_format();
 if (!b) b="unknown";
 l=babel_get_length() / 1024;
 

 i=babel_treaty(GET_STORY_FILE_COVER_EXTENT_SEL,NULL,0);
 j=babel_treaty(GET_STORY_FILE_COVER_FORMAT_SEL,NULL,0);

 if (i<=0 || j<=0)
 {
  cf="no cover"; 
 }
 else
 {
  char *md=my_malloc(i,"Image buffer");
  if (babel_treaty(GET_STORY_FILE_COVER_SEL,md,i)<0)
  {
   cf="no cover";
  }
  else
  {
   dim=get_image_dim(md,i,j)+1;
   dim[strlen(dim)-1]=0;
   if (j==JPEG_COVER_FORMAT) cf="jpeg";
   else if (j==PNG_COVER_FORMAT) cf="png";
   else cf="unknown format";
   sprintf(buffer,"cover %s %s",dim,cf);
   cf=buffer;
  }
 }
 printf("%s, %dk, %s\n",b, l,cf);
}

void babel_story_meta()
{
 deep_babel_ifiction(2);
}

void babel_story_story()
{
  int32 j,i;
  void *p;
  FILE *f;
  char *ep;
  char buffer[TREATY_MINIMUM_EXTENT+20];
  j=babel_get_story_length();
  p=babel_get_story_file();
  if (!j || !p)
  {
    fprintf(stderr,"A serious error occurred while retrieving the story file.\n");
    return;
  }

  i=babel_treaty(GET_STORY_FILE_IFID_SEL,buffer,TREATY_MINIMUM_EXTENT);
  if (i==0 && !babel_md5_ifid(buffer, TREATY_MINIMUM_EXTENT))
  {
   fprintf(stderr,"Unable to create an IFID (A serious problem occurred while loading the file).\n");
   return;
  }
  ep=strchr(buffer, ',');
  if (!ep) ep=buffer+strlen(buffer);
  *ep=0;
  babel_treaty(GET_STORY_FILE_EXTENSION_SEL,ep,19);
  f=fopen(buffer,"wb");
  if (!f || !fwrite(p,1,j,f))
    {
     fprintf(stderr,"A serious error occurred writing to disk.\n");
     return;
    }
 fclose(f);
 printf("Extracted %s\n",buffer);

  

}

void babel_story_unblorb()
{
 deep_babel_ifiction(1);
 deep_babel_cover(1);
 babel_story_story();

}
