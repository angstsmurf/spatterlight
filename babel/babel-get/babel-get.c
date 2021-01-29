/* babel-get.c  The babel-get application
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
 * To build: compile all the files in the babel-get directory (You will
 * need the headers from babel), then link them to the babel handler library
 * and the babel ifiction library
 *
 */

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


char * get_dir(char *, char *);
char * get_ifiction(char *, char *);
char * get_story(char *, char *);
char * get_story_cover(char *, char *);
char * get_url(char *, char *);
char * get_url_cover(char *, char *);
typedef char * (*getter)(char *, char *);
/* For command line processing */
struct get_info
{
 char *cmd;
 char *source;
 getter func;
 char nonull;
};
struct get_info info[] = {
                { "-ifiction", "-ifiction", get_ifiction, 0 },
                { "-ifiction", "-story", get_story, 0 },
                { "-ifiction", "-dir", get_dir, 1 },
                { "-ifiction", "-url", get_url, 1 },
                { "-cover", "-story", get_story_cover, 0},
                { "-cover", "-url", get_url_cover, 1},
                { NULL, NULL, NULL }
                };

void show_usage(void)
{
 int i;
 printf("Usage:\n");
 for(i=0;info[i].cmd;i++)
  printf(" babel-get %s %s %s <source>%s\n",
           info[i].cmd, info[i].nonull ? "<ifid>": "[<ifid>]", info[i].source,
           info[i].cmd[1]=='c' ? " [-to <directory>]":"");

}
int main(int argc, char **argv)
{
 int srcc=2;
 char *ifid=NULL;
 int i;
 char cwd[512];

 if (argc<4 || argc > 7 || (argc >5 && strcmp(argv[argc-2],"-to")) ||
   (strcmp(argv[argc-2],"-to")==0 && strcmp(argv[1],"-ifiction")==0))
 {
  show_usage();
  return 1;
 }

 if (argv[2][0]!='-') { srcc=3; ifid=argv[2]; } 

 
 for(i=0;info[i].cmd;i++)
  if (strcmp(argv[1],info[i].cmd)==0 &&
      strcmp(argv[srcc],info[i].source)==0)
  { if (ifid==NULL && info[i].nonull)
    {
     printf("%s %s requires an explicit IFID.\n",argv[1],argv[srcc]);
     return 1;
    }
    else
    {
     if (strcmp(argv[argc-2],"-to")==0)
     {
      getcwd(cwd,512);
      chdir(argv[argc-1]);
     }
     if (!info[i].func(ifid,argv[srcc+1])) printf("Requested information not found.\n");
     if (strcmp(argv[argc-2],"-to")==0) chdir(cwd);
     return 0;
    }
  }
 if (strcmp(argv[argc-2],"-to")==0) chdir(cwd);
 show_usage();
 return 1;
}
