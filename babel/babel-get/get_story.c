/* get_story.c : story file source for babel-get
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
 *
 */

#include "babel_handler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *_get_story(char *ifid, char *from)
{
 char buf[512];
 int32 l;
 char *md;
 if (!babel_init(from)) return NULL;
 if (ifid)
   if (babel_treaty(GET_STORY_FILE_IFID_SEL,buf,512)<=0 ||
       !strstr(buf,ifid)) { babel_release(); return NULL; }
 l=babel_treaty(GET_STORY_FILE_METADATA_EXTENT_SEL,0, NULL);
 if (l<=0) { babel_release(); return NULL; }
 md=(char *)malloc(l);
 if (!md)  { babel_release(); return NULL; }
 babel_treaty(GET_STORY_FILE_METADATA_SEL,md,l);
 babel_release();
 return md;

}

char * get_story(char *ifid, char *from)
{
char *rv=_get_story(ifid,from);
if (rv) printf("%s",rv);
else return NULL;
return ifid;
}

char *get_story_cover(char *ifid, char *from)
{
 static char buf[512];
 int32 l;
 int32 fmt;
 char *md;
 FILE *f;

 if (!babel_init(from)) return NULL;
 if (babel_treaty(GET_STORY_FILE_IFID_SEL,buf,512)<=0 ||
       (ifid && !strstr(buf,ifid))) { babel_release(); return NULL; }
 l=babel_treaty(GET_STORY_FILE_COVER_EXTENT_SEL,0, NULL);
 if (l<=0) { babel_release(); return NULL; }
 md=(char *)malloc(l);
 if (!md)  { babel_release(); return NULL; }
 babel_treaty(GET_STORY_FILE_COVER_SEL,md,l);
 fmt=babel_treaty(GET_STORY_FILE_COVER_FORMAT_SEL,NULL,0);
 babel_release();
 if (strchr(buf,',')) *(strchr(buf,','))=0;
 if (fmt==JPEG_COVER_FORMAT)
  if (ifid) sprintf(buf,"%s.jpg",ifid); else strcat(buf,".jpg");
 if (fmt==PNG_COVER_FORMAT)
  if (ifid) sprintf(buf,"%s.png",ifid); else   strcat(buf,".png");
 f=fopen(buf,"wb");
 if (!f) { free(md); return NULL; }
 fwrite(md,1,l,f);
 fclose(f);
 free(md);
 return buf;
}
