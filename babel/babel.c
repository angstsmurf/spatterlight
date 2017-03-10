/* babel.c   The babel command line program
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
 * This file depends upon misc.c and babel.h
 *
 * This file exports one variable: char *rv, which points to the file name
 * for an ifiction file.  This is used only by babel_ifiction_verify
 */

#include "babel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
int chdir(const char *);
char *getcwd(char *, int);
#ifdef __cplusplus
}
#endif

char *fn;

/* checked malloc function */
void *my_malloc(int, char *);

/* babel performs several fundamental operations, which are specified
   by command-line objects. Each of these functions corresponds to
   a story function (defined in babel_story_functions.c) or an
   ifiction function (defined in babel_ifiction_functions.c) or both.
   These are the types of those functions.
*/

typedef void (*story_function)(void);
typedef void (*ifiction_function)(char *);
typedef void (*multi_function)(char **, char *, int);
/* This structure tells babel what to do with its command line arguments.
   if either of story or ifiction are NULL, babel considers this command line
   option inappropriate for that type of file.
*/
struct function_handler {
        char *function;         /* the textual command line option */
        story_function story;   /* handler for story files */
        ifiction_function ifiction; /* handler for ifiction files */
        char *desc;             /* Textual description for help text */
        };

struct multi_handler {
        char *function;
        char *argnames;
        multi_function handler;
        int nargsm;
        int nargsx;
        char *desc;
        };
/* This is an array of function_handler objects which specify the legal
   arguments.  It is terminated by a function_handler with a NULL function
 */
static struct function_handler functions[] = {
        { "-ifid", babel_story_ifid, babel_ifiction_ifid, "Deduce IFID"},
        { "-format", babel_story_format, NULL, "Deduce story format" },
        { "-ifiction", babel_story_ifiction, NULL, "Extract iFiction file" },
        { "-meta", babel_story_meta, NULL, "Print story metadata" },
        { "-identify", babel_story_identify, NULL, "Describe story file" },
        { "-cover", babel_story_cover, NULL, "Extract cover art" },
        { "-story", babel_story_story, NULL, "Extract story file (ie. from a blorb)" },
        { "-verify", NULL, babel_ifiction_verify, "Verify integrity of iFiction file" },
        { "-lint", NULL, babel_ifiction_lint, "Verify style of iFiction file" },
        { "-fish", babel_story_fish, babel_ifiction_fish, "Extract all iFiction and cover art"},
        { "-unblorb", babel_story_unblorb, NULL, "As -fish, but also extract story files"},
        { NULL, NULL, NULL }
        };
static struct multi_handler multifuncs[] = {
        { "-blorb", "<storyfile> <ifictionfile> [<cover art>]", babel_multi_blorb, 2, 3, "Bundle story file and (sparse) iFiction into blorb" },
        { "-blorbs", "<storyfile> <ifictionfile> [<cover art>]", babel_multi_blorb1, 2, 3, "Bundle story file and (sparse) iFiction into sensibly-named blorb" },
        { "-complete", "<storyfile> <ifictionfile>", babel_multi_complete, 2, 2, "Create complete iFiction file from sparse iFiction" },
        { NULL, NULL, NULL, 0, 0, NULL }
};

int main(int argc, char **argv)
{
 char *todir=".";
 char cwd[512];
 int ok=1,i, l, ll;
 FILE *f;
 char *md=NULL;
 /* Set the input filename.  Note that if this is invalid, babel should
   abort before anyone notices
 */
 fn=argv[2];

 if (argc < 3) ok=0;
 /* Detect the presence of the "-to <directory>" argument.
  */
 if (ok && argc >=5 && strcmp(argv[argc-2], "-to")==0)
 {
  todir=argv[argc-1];
  argc-=2;
 }
 if (ok) for(i=0;multifuncs[i].function;i++)
          if (strcmp(argv[1],multifuncs[i].function)==0 &&
              argc>= multifuncs[i].nargsm+2 &&
              argc <= multifuncs[i].nargsx+2)
          {

   multifuncs[i].handler(argv+2, todir, argc-2);
   exit(0);
          }

 if (argc!=3) ok=0;

 /* Find the apropriate function_handler */
 if (ok) {
 for(i=0;functions[i].function && strcmp(functions[i].function,argv[1]);i++);
 if (!functions[i].function) ok=0;
 else  if (strcmp(fn,"-")) {
   f=fopen(argv[2],"r");
   if (!f) ok=0;
  }
 }

 /* Print usage error if anything has gone wrong */
 if (!ok)
 {
  printf("%s: Treaty of Babel Analysis Tool (%s, %s)\n"
         "Usage:\n", argv[0],BABEL_VERSION, TREATY_COMPLIANCE);
  for(i=0;functions[i].function;i++)
  {
   if (functions[i].story)
    printf(" babel %s <storyfile>\n",functions[i].function);
   if (functions[i].ifiction)
    printf(" babel %s <ifictionfile>\n",functions[i].function);
   printf("   %s\n",functions[i].desc);
  }
  for(i=0;multifuncs[i].function;i++)
  {
   printf("babel %s %s\n   %s\n",
           multifuncs[i].function,
           multifuncs[i].argnames,
           multifuncs[i].desc);
  }

  printf ("\nFor functions which extract files, add \"-to <directory>\" to the command\n"
          "to set the output directory.\n"
          "The input file can be specified as \"-\" to read from standard input\n"
          "(This may only work for .iFiction files)\n");
  return 1;
 }

 /* For story files, we end up reading the file in twice.  This
    is unfortunate, but unavoidable, since we want to be all
    cross-platformy, so the first time we read it in, we
    do the read in text mode, and the second time, we do it in binary
    mode, and there are platforms where this makes a difference.
 */
 ll=0;
 if (strcmp(fn,"-"))
 {
  fseek(f,0,SEEK_END);
  l=ftell(f)+1;
  fseek(f,0,SEEK_SET);
  md=(char *)my_malloc(l,"Input file buffer");
  fread(md,1,l-1,f);
  md[l-1]=0;
 }
 else
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


  if (strstr(md,"<?xml version=") && strstr(md,"<ifindex"))
  { /* appears to be an ifiction file */
   char *pp;
   pp=strstr(md,"</ifindex>");
   if (pp) *(pp+10)=0;
   getcwd(cwd,512);
   chdir(todir);
   l=0;
   if (functions[i].ifiction)
    functions[i].ifiction(md);
   else
    fprintf(stderr,"Error: option %s is not valid for iFiction files\n",
             argv[1]);
   chdir(cwd);
 }

 if (strcmp(fn,"-"))
 {
 free(md);
 fclose(f);
 }
 if (l)
 { /* Appears to be a story */
   char *lt;
   if (functions[i].story)
   {
    if (strcmp(fn,"-")) lt=babel_init(argv[2]);
    else { lt=babel_init_raw(md,ll);
           free(md);
         }

    if (lt)
    {
     getcwd(cwd,512);
     chdir(todir);
     if (!babel_get_authoritative() && strcmp(argv[1],"-format"))
      printf("Warning: Story format could not be positively identified. Guessing %s\n",lt);
     functions[i].story();

     chdir(cwd);
    }
    else if (strcmp(argv[1],"-ifid")==0) /* IFID is calculable for all files */
    {
     babel_md5_ifid(cwd,512);
     printf("IFID: %s\n",cwd);
    }
    else
     fprintf(stderr,"Error: Did not recognize format of story file\n");
    babel_release();
   }
   else
    fprintf(stderr,"Error: option %s is not valid for story files\n",
             argv[1]);
  }    

 return 0;
}
