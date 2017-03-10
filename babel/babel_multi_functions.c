#include "babel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif
int chdir(const char *);
char *getcwd(char *, int);
#ifdef __cplusplus
}
#endif

void deep_ifiction_verify(char *md, int f);
void * my_malloc(int32, char *);
char *blorb_chunk_for_name(char *name);
#ifndef THREE_LETTER_EXTENSIONS
static char *ext_table[] = { "zcode", ".zblorb",
                             "glulx", ".gblorb",
                             NULL, NULL
                             };
#else
static char *ext_table[] = { "zcode", ".zlb",
                             "glulx", ".glb",
                             NULL, NULL
                             };

#endif
char *blorb_ext_for_name(char *fmt)
{
 int i;
 for(i=0;ext_table[i];i+=2)
  if (strcmp(ext_table[i],fmt)==0) return ext_table[i+1];
#ifndef THREE_LETTER_EXTENSIONS
 return ".blorb";
#else
 return ".blb";
#endif
}

char *deep_complete_ifiction(char *fn, char *ifid, char *format)
{
 FILE *f;
 int32 i;
 char *md;
 char *id, *idp;
 char *idb;
 f=fopen(fn,"r");
 if (!f) { fprintf(stderr,"Error: Can not open file %s\n",fn);
          return NULL;
          }
 fseek(f,0,SEEK_END);
 i=ftell(f);
 fseek(f,0,SEEK_SET);
 md=(char *) my_malloc(i+1,"Metadata buffer");
 fread(md,1,i,f);
 md[i]=0;
 id=strstr(md,"</ifindex>");
 if (id) *(id+10)=0;
 fclose(f);
 id=strdup(ifid);
 idp=strtok(id,",");
 /* Find the identification chunk */
 {
   char *bp, *ep;
   bp=strstr(md,"<identification>");
   if (!bp)
   {
    idb=(char *)my_malloc(TREATY_MINIMUM_EXTENT+128,"ident buffer");
    sprintf(idb,"<format>%s</format>\n", format);
   }
   else
   {
    int ii;
    ep=strstr(bp,"</identification>");
    idb=(char *)my_malloc(TREATY_MINIMUM_EXTENT+128+(ep-bp),"ident buffer");
    for(ii=16;bp+ii<ep;ii++)
    idb[ii-16]=bp[ii];
    idb[ii]=0;
    for(ep+=18;*ep;ep++)
     *bp++=*ep;
    *bp=0;
    bp=strstr(idb,"<format>");
    if (bp)
     if (memcmp(bp+8,format,strlen(format)))
      fprintf(stderr,"Error: Format in sparse .iFiction does not match story\n");

   }

 }
 /* Insert the new ifids */
 while(idp)
 {
  char bfr[TREATY_MINIMUM_EXTENT];
  sprintf(bfr,"<ifid>%s</ifid>",idp);
  if (!strstr(idb,bfr)) { strcat(idb,bfr); strcat(idb,"\n"); }
  idp=strtok(NULL,",");

 }
 free(id);
 idp=(char *) my_malloc(strlen(md)+strlen(idb)+64, "Output metadata");
/* printf("%d bytes for metadata\n",strlen(md)+strlen(idb)+64);*/
 id=strstr(md,"<story>");
 id[0]=0;
 id+=7;
 strcpy(idp,md);
 strcat(idp,"<story>\n <identification>\n");
 strcat(idp,idb);
 free(idb);
 strcat(idp," </identification>\n");
 strcat(idp,id);
 free(md);
 md=idp;
 deep_ifiction_verify(md, 0);
 return md;
}

void write_int(int32 i, FILE *f)
{
 char bf[4];
 bf[0]=(((unsigned) i) >> 24) & 0xFF;
 bf[1]=(((unsigned) i) >> 16) & 0xFF;
 bf[2]=(((unsigned) i) >> 8) & 0xFF;
 bf[3]=(((unsigned) i)) & 0xFF;
 fwrite(bf,1,4,f);
}
static void _babel_multi_blorb(char *outfile, char **args, char *todir , int argc)
{
 int32 total, storyl, coverl, i;
 char buffer[TREATY_MINIMUM_EXTENT+10];
 char b2[TREATY_MINIMUM_EXTENT];

 char cwd[512];
 char *cover, *md, *cvrf, *ep;

 FILE *f, *c;
 if (argc!=2 && argc !=3)
 {
  fprintf(stderr,"Invalid usage\n");
  return;
 }
 if (!babel_init(args[0]))
 {
  fprintf(stderr,"Error: Could not determine the format of file %s\n",args[0]);
  return;
 }
 if (babel_treaty(GET_STORY_FILE_IFID_SEL,buffer,TREATY_MINIMUM_EXTENT)<=0 ||
     babel_treaty(GET_FORMAT_NAME_SEL,b2,TREATY_MINIMUM_EXTENT)<0
    )
 {
  fprintf(stderr,"Error: Could not deduce an IFID for file %s\n",args[0]);
  return;
 }
 if (babel_get_length() != babel_get_story_length())
 {
  fprintf(stderr,"Warning: Story file will be extacted from container before blorbing\n");
 }
/* printf("Completing ifiction\n");*/
 md=deep_complete_ifiction(args[1],buffer,b2);
/* printf("Ifiction is %d bytes long\n",strlen(md));*/
 ep=strchr(buffer,',');
 if (ep) *ep=0;
 if (outfile)
  strcpy(buffer,outfile);
 strcat(buffer,blorb_ext_for_name(b2));
 getcwd(cwd,512);
 chdir(todir);
 f=fopen(buffer,"wb");
 chdir(cwd);
 if (!f)
 {
  fprintf(stderr,"Error: Error writing to file %s\n",buffer);
  return;
 }
 storyl=babel_get_story_length();
 total=storyl + (storyl%2) + 36;
 if (md) total+=8+strlen(md)+strlen(md)%2;
 if (argc==3)
 {
  c=fopen(args[2],"rb");
  if (c)
  {
   fseek(c,0,SEEK_END);
   coverl=ftell(c);
   if (coverl > 5){

   cover=(char *) my_malloc(coverl+2,"Cover art buffer");
   fseek(c,0,SEEK_SET);
   fread(cover,1,coverl,c);
   if (memcmp(cover+1,"PNG",3)==0) cvrf="PNG ";
   else cvrf="JPEG";
   total += 32+coverl + (coverl%2);
   }
   else argc=2;
   fclose(c);
  }
  else argc=2;
 }
/* printf("Writing header\n;");*/
 fwrite("FORM",1,4,f);
 write_int(total,f);
/* printf("Writing index\n;");*/
 fwrite("IFRSRIdx",1,8,f);
 write_int(argc==3 ? 28:16,f);
 write_int(argc==3 ? 2:1,f);
/* printf("Writing story\n;");*/
 fwrite("Exec", 1,4,f);
 write_int(0,f);
 write_int(argc==3 ? 48:36,f);
 if (argc==3)
 {
/* printf("Writing image\n;"); */
 fwrite("Pict", 1,4,f);
 write_int(1,f);
 write_int(56+storyl+(storyl%2),f);
 }
/* printf("Invoking chunk for name %s\n",b2); */
 fwrite(blorb_chunk_for_name(b2),1,4,f);
 write_int(storyl,f);
/* printf("Writing story data\n"); */
 fwrite(babel_get_story_file(),1,storyl,f);
 if (storyl%2) fwrite("\0",1,1,f);
 if (argc==3)
 {
/*  printf("Writing cover data header %s\n",cvrf); */
  fwrite(cvrf,1,4,f);
/*  printf("Writing cover data size %d\n",coverl); */
  write_int(coverl,f);
/*  printf("Writing cover data\n"); */
  fwrite(cover,1,coverl,f);
  if (coverl%2) fwrite("\0",1,1,f);
/*  printf("Done with cover\n");*/
/*  free(cover);*/
/*  printf("Writing frontispiece\n;");*/
  fwrite("Fspc\0\0\0\004\0\0\0\001",1,12,f);
 }

 if (md) {
/*  printf("Writing metadata\n;");*/
 fwrite("IFmd",1,4,f);
 write_int(strlen(md),f);
 fwrite(md,1,strlen(md),f);
 if (strlen(md)%2)
  fwrite("\0",1,1,f);
 free(md);
 }

 fclose(f);
 printf("Created %s\n",buffer);
 
}
void babel_multi_complete(char **args, char *todir, int argc)
{
 char buffer[TREATY_MINIMUM_EXTENT+10];
  char b2[TREATY_MINIMUM_EXTENT];
 char cwd[512];
 char *ep, *md;
 FILE *f;
 if (argc!=2)
 {
  fprintf(stderr,"Invalid usage\n");
  return;
 }
 if (!babel_init(args[0]))
 {
  fprintf(stderr,"Error: Could not determine the format of file %s\n",args[0]);
  return;
 }
 if (babel_treaty(GET_STORY_FILE_IFID_SEL,buffer,TREATY_MINIMUM_EXTENT)<=0
    ||  babel_treaty(GET_FORMAT_NAME_SEL,b2,TREATY_MINIMUM_EXTENT)<0)
 {
  fprintf(stderr,"Error: Could not deduce an IFID for file %s\n",args[0]);
  return;
 }
 md=deep_complete_ifiction(args[1],buffer, b2);
 if (!md) return;
 ep=strchr(buffer,',');
 if (ep) *ep=0;
 strcat(buffer,".iFiction");
 getcwd(cwd,512);
 chdir(todir);
 f=fopen(buffer,"w");
 chdir(cwd);
 if (!f || !fputs(md,f))
 {
  fprintf(stderr,"Error: Error writing to file %s\n",buffer);
  return;
 }
 fclose(f);
 free(md);
 printf("Created %s\n",buffer);
}
void babel_multi_blorb(char **args, char *todir , int argc)
{
 _babel_multi_blorb(NULL,args,todir,argc);
}
void babel_multi_blorb1(char **args, char *todir , int argc)
{
 char *buf;
 char *bb;
 buf=(char *)my_malloc(strlen(args[0])+1,"blorb name buffer");
 strcpy(buf,args[0]);
 bb=strrchr(buf,'.');
 if (bb) *bb=0;
 _babel_multi_blorb(buf,args,todir,argc);
 free(buf);
 

}
