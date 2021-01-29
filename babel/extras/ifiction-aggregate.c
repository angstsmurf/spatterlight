/*
  ifiction-aggregate : combine multiple ifiction files
  L. Ross Raszewski
  This code is freely usable for all purposes.
 
  This work is licensed under the Creative Commons Attribution 4.0 License.
  To view a copy of this license, visit
  https://creativecommons.org/licenses/by/4.0/ or send a letter to
  Creative Commons,
  PO Box 1866,
  Mountain View, CA 94042, USA.
 
  To build:
  compile this file. Link it with
  ifiction.c,  misc.c, register_ifiction.c,
  from the babel distribution.

  Or use the babel makefile to build the ifiction library
  (ifiction.lib or ifiction.a) and link this
  file to them


*/

#include "ifiction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *my_malloc(int, char *);

/* XML Header */
static const char xml_husk[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!-- Metadata aggregated by babel-aggregate -->\n"
        "<ifindex version=\"1.0\" xmlns=\"http://babel.ifarchive.org/protocol/iFiction/\">\n";

/* A <story> tag from an iFiction file */
struct storyblock
{
 char *story;
 char ifid[512];
 struct storyblock *next;
};

/* The linked list of story blocks */
struct storyblock *stories=NULL;
/* temporary storage for the last ifid encountered */
char current_ifid[512];

/* If fromsf contains <annotation> and tosf does not, move it over */
void merge_anno(struct storyblock *tosf, struct storyblock *fromsf)
{
 char *annos, *ane;

 if (strstr(tosf->story,"<annotation>")) return;
 annos=strstr(fromsf->story,"<annotation>");

 if (!annos) return;
 ane=strstr(annos,"</annotation>");

 if (!ane) return;
 /* Move the end pointer past the close tag */
 ane+=13;
 *ane=0;
 tosf->story=(char *)realloc(tosf->story,strlen(tosf->story)+strlen(annos)+4);
 strcat(tosf->story,annos);
 strcat(tosf->story,"\n");

}

/* Close tag function: builds a storyblock for each story encountered and
adds it to the list */
void steal_story(struct XMLTag *xtg, void *ctx)
{
 if (ctx) {}
 if (strcmp(xtg->tag,"ifid")==0)
 {
  char c;
  c=*(xtg->end);
  *(xtg->end)=0;
  strcpy(current_ifid,xtg->begin);
  *(xtg->end)=c;
 }
 else if (strcmp(xtg->tag,"story")==0 && current_ifid[0])
 {
  char c;
  struct storyblock *n;
  c=*(xtg->end);
  *(xtg->end)=0;
  n=(struct storyblock *)my_malloc(sizeof(struct storyblock), "story block");
  n->next=stories;
  stories=n;
  strcpy(n->ifid,current_ifid);
/*  printf("created %s\n", current_ifid);*/
  n->story=(char *)my_malloc(strlen(xtg->begin)+16, "Story record");
  strcpy(n->story,xtg->begin);
  *(xtg->end)=c;
  current_ifid[0]=0;

 }
}

/* Null error handler (to suppress warnings) */
void null_eh(char *md, void *ctx)
{
 if (md || ctx) { }
}

int main(int argc, char **argv)
{
 FILE *f;
 char *md=NULL, *mdo, *ep;
 int ll=0;

 struct storyblock *sf, *sf2, *sf3;
 if (argc!=2)
 {
  printf("Usage: ifiction-aggregate ifiction-file < new-data\n");
  exit(1);
 }

 /* Read stdin into md */
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

  /* read input file into mdo */
  f=fopen(argv[1],"r");
  if (!f)
  {
   mdo=NULL;
   
  }
  else
  {
  fseek(f,0,SEEK_END);
  ll=ftell(f);
  if (ll) {
  mdo=(char *)my_malloc(ll+2,"file buffer");
  fseek(f,0,SEEK_SET);
  fread(mdo,1,ll,f);
  mdo[ll]=0;
  ep=strstr(mdo,"</ifindex>");
  if (ep) *(ep+10)=0;
  }
  else mdo=NULL;
  fclose(f);
  }

  /* Reopen the file for output */
  f=fopen(argv[1],"w");
  if (!f) {
           printf("Error opening output file %s\n",argv[1]);
                exit(1);
          }
  /* Spit out an XML header */
  fputs(xml_husk,f);

  /* Load all the story chunks from the old file */
  current_ifid[0]=0;
  if (mdo) ifiction_parse(mdo, steal_story, NULL, null_eh, NULL);

  /* Reverse the order of the linked list -- this ensures that if nothing
     happens, the output file will not be reordered
  */
  sf=stories;
  sf3=NULL;
  while(sf)
  {
   stories=sf;
   sf2=sf->next;
   sf->next=sf3;
   sf3=sf;
   sf=sf2;
  }
  /* Load the new story chunks */
  current_ifid[0]=0;
  ifiction_parse(md, steal_story, NULL, null_eh, NULL);

  /* Output each story chunk */
  for(sf=stories;sf;sf=sf->next)
  {
    char buf[512];
    int fl=0;
    /* the story has been nulled out if it lost an adjudication round */
    if (!sf->story) continue;
/*    printf("Considering %s (%d bytes)\n",sf->ifid, strlen(sf->story));*/
    sprintf(buf,"<ifid>%s</ifid>",sf->ifid);
    for(sf2=sf->next;sf2;sf2=sf2->next)
    /* Check the remaining chunks for a collision */
    if (/*printf("Checking for collision %x with %x\n", sf, sf2) && */sf2->story && strstr(sf2->story,buf))
    {
      /* A collision occurred (the current chunk has the same ifid as
         a later chunk) We adjudicate.  If the current one loses, we
         set a flag. If the other does, we null its story.
         Before we do either, though, we call merge_anno to
         save the loser's annotation tag */
      char *cp1, *cp2, *ep1, *ep2, *dp1, *dp2;
      int dt1, dt2;
      /* Adjudication: Round 1: If either story lacks a colophon,
         the current one wins (Since it's more likely to be the one
         we just tried to add) */
      cp1=strstr(sf->story,"<colophon>");
      cp2=strstr(sf2->story,"<colophon>");
      if (!cp1 || !cp2) { merge_anno(sf, sf2); free(sf2->story); sf2->story=NULL; }
      ep1=strstr(cp1,"</colophon>");
      ep2=strstr(cp2,"</colophon>");
      dp1=strstr(cp1,"<originated>");
      dp2=strstr(cp2,"<originated>");
      if (dp1 > ep1 || dp2 > ep2) { merge_anno(sf, sf2); free(sf2->story); sf2->story=NULL; }
      /* Adjudication: Round 2: Compare the years. Newest wins */
      dp1+=12; dp2+=12;
      sscanf(dp1,"%d",&dt1);
      sscanf(dp2,"%d",&dt2);
      dp1+=5;
      dp2+=5;
      if (dt1 < dt2) { merge_anno(sf2, sf); fl=1; break; }
      else if (dt2 < dt1) { merge_anno(sf, sf2); free(sf2->story); sf2->story=NULL; }
      else
      {
       /* Adjudication: Round 3: Compare the months. Newest wins. */
       dt1=0; dt2=0;
       sscanf(dp1,"%d",&dt1);
       sscanf(dp2,"%d",&dt2);
       if (dt1<10) dp1+=2; else dp1+=3;
       if (dt2<10) dp2+=2; else dp2+=3;
       if (dt1 < dt2) { merge_anno(sf2, sf); fl=1; break; }
       else if (dt2 < dt1) { merge_anno(sf, sf2); free(sf2->story); sf2->story=NULL; }
       else
       {      
       /* Adjudication: Round 3: Compare the days. Newest wins. */
       /* If round 3 does not declare a winner, the current one wins */
        dt1=0; dt2=0;
        sscanf(dp1,"%d",&dt1);
        sscanf(dp2,"%d",&dt2);
        if (dt1 <= dt2) { merge_anno(sf2, sf); fl=1; break; }
        else { merge_anno(sf, sf2); free(sf2->story); sf2->story=NULL; }
       }

      }
    }

    /* if we didn't declare this one a loser, zap it into the output file */
    if (!fl)
    { fputs("<story>",f);
      fputs(sf->story,f);
/*      printf("Zap complete\n");*/
      fputs("</story>\n",f);
    }
   free(sf->story);
   sf->story=NULL;
    

  }
  /* Close the ifiction file */
  fputs("</ifindex>\n",f);
  fclose(f);
  return 0;
}
