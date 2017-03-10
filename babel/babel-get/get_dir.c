/* get_dir.c directory source for babel-get
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
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
int chdir(const char *);
char *getcwd(char *, int);
#ifdef __cplusplus
}
#endif


char *get_ifiction(char *, char *);

char *get_dir(char *ifid, char *from)
{
char cwd[512];
char buff[520];
char *md;

if (!ifid) return NULL;

getcwd(cwd,512);
sprintf(buff,"%s.iFiction",ifid);
chdir(from);
md=get_ifiction(ifid,buff);

chdir(cwd);

return md;
}
