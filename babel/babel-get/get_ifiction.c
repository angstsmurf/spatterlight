/* get_ifiction.c: ifiction source for babel-get
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ifiction.h"
static const char xml_husk[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        " <!-- Metadata extracted from %s by babel-get -->\n"
        "<ifindex version=\"1.0\" xmlns=\"http://babel.ifarchive.org/protocol/iFiction/\">\n"
        " <story>%s </story>\n</ifindex>";


char *_get_ifiction_data(char *ifid, char *from, char *md)
{
 char *st, *sta=NULL;
 char *xmlb, anifid[512];
 int32 ic;

 while(1)
 {
  st=ifiction_get_tag(md,"ifindex","story",sta);
  if (sta) free(sta);
  if (!st) { return NULL; }
  xmlb=(char *)malloc(strlen(xml_husk)+strlen(st)+strlen(from)+1);
  if (!xmlb) { free(st); return NULL; }
  sprintf(xmlb,xml_husk,from,st);
  sta=st;
  ic=ifiction_get_IFID(xmlb, anifid, 512);
  if (ic<=0) { free(xmlb); continue; }
  if (!ifid || strstr(anifid,ifid))
   return xmlb;

  free(xmlb);
 }

}

char *_get_ifiction(char *ifid, char *from)
{
 char *rv;
 FILE *f;
 char *md;
 int32 l;

 f=fopen(from,"r");
 if (!f) return NULL;
 fseek(f,0,SEEK_END);
 l=ftell(f);
 if (!l) { fclose(f); return NULL; }
 md=(char *)malloc(l+1);
 fseek(f,0,SEEK_SET);
 if (!md) { fclose(f); return NULL; }
 fread(md,1,l,f);
 md[l]=0;
 fclose(f);
 rv=strstr(md,"</ifindex>");
 if (rv) *(rv+10)=0;
 rv=_get_ifiction_data(ifid,from,md);
 free(md);
 return rv;

}

void get_ifiction_data(char *ifid, char *from, char *md)
{
 char *rv=_get_ifiction_data(ifid,from,md);
 if (rv) printf("%s",rv);
}
char * get_ifiction(char *ifid, char *from)
{
 char *rv=_get_ifiction(ifid,from);
 if (rv) printf("%s",rv);
 else return NULL;
 return rv;
}

