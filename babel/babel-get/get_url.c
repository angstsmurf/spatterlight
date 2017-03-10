/* get_url.c : URL source for babel-get
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

char *get_url(char *ifid, char *from)
{
 char cmdbuf[1024];

 sprintf(cmdbuf, "curl --silent --fail %s/metadata/%s",from,ifid);

 system(cmdbuf);
 return ifid;
}

char *get_url_cover(char *ifid, char *from)
{
 char cmdbuf[1024];

 sprintf(cmdbuf, "curl --silent --fail %s/cover/%s > %s.jpg",from,ifid, ifid);

 system(cmdbuf);
 return ifid;
}

