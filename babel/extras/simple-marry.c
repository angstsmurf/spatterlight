/* Simplified version of babel-marry for machines without
   perl.

   babel-marry.pl performs a more sophisticated version of this
   procedure.

   babel must be in your path
*/

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#define DIR_SEP "\\"

char *cover_ext[] = { "png", "jpg", "jpeg", NULL };
int main(int argc, char **argv)
{
 struct dirent *de;
 DIR *dd;
 char *buf;
 char ifibuf[512];
 char rfn[256];
 if (argc != 1 && argc!=2)
 {
  printf("usage: %s [ifiction and cover art directory]\n",argv[0]);
  return 1;
 }
 
 dd=opendir(".");
 while((de=readdir(dd))!=0)
 {
  char *sp;
  FILE *f;
  int i;
  if (de->d_name[0]=='.' ||
      strstr(de->d_name, ".ifiction") ||
      strstr(de->d_name, ".iFiction") ||
      strstr(de->d_name, ".exe") ||
      strstr(de->d_name, ".blorb") ||
      strstr(de->d_name, ".blb") ||
      strstr(de->d_name, ".zblorb") ||
      strstr(de->d_name, ".gblorb") ||
      strstr(de->d_name, ".zbl") ||
      strstr(de->d_name, ".gbl") ||
      strstr(de->d_name, ".png") ||
      strstr(de->d_name, ".jpg") ||
      strstr(de->d_name, ".jpeg") ||
      strstr(de->d_name, ".txt"))
   continue;
   strcpy(rfn,de->d_name);
   sp=strrchr(rfn,'.');
   if (sp) *sp=0;
   printf("%s: ",rfn);
   if (argc==2)
    sprintf(ifibuf,"%s%s%s.iFiction",argv[1],DIR_SEP,rfn);
   else sprintf(ifibuf,"%s.iFiction",rfn);
   f=fopen(ifibuf,"r");
   if (!f) { printf("no metadata found\n"); continue; }
   fclose(f);
   buf=(char *)malloc(2*strlen(ifibuf)+strlen(de->d_name)+20);
   sprintf(buf, "babel -blorbs %s %s",de->d_name,ifibuf);
   printf("%s %s ", de->d_name, ifibuf);
   for(i=0;cover_ext[i];i++)
   {
   if (argc==2)
    sprintf(ifibuf,"%s%s%s.%s",argv[1],DIR_SEP,rfn,cover_ext[i]);
   else sprintf(ifibuf,"%s.%s",rfn,cover_ext[i]);
   f=fopen(ifibuf,"r");
   if (f) {
   fclose(f);
   break;
   }
   }
   if (cover_ext[i])
   {
    strcat(buf," ");
    strcat(buf,ifibuf);
    printf("%s",ifibuf);
   }
   printf(": blorbing\n");
/*   printf(buf); */
   system(buf);

   free(buf);
 }
 closedir(dd);

 return 0;
}

