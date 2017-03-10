/* babel.h  declarations for babel
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
 * This file depends upon treaty.h, babel_ifiction_functions.c,
 * babel_story_functions.c, and babel_handler.c
 *
 */

#define BABEL_VERSION "0.2b"

#include "treaty.h"
#include "babel_handler.h"
#include "ifiction.h"
/* Functions from babel_story_functions.c
 *
 * Each of these assumes that the story file has been loaded by babel_handler
 *
 * Each function babel_story_XXXX coresponds to the command line option -XXXX
 */
void babel_story_ifid(void);
void babel_story_cover(void);
void babel_story_ifiction(void);
void babel_story_meta(void);
void babel_story_fish(void);
void babel_story_format(void);
void babel_story_identify(void);
void babel_story_story(void);
void babel_story_unblorb(void);
/* Functions from babel_ifiction_functions.c
 *
 * as with babel_story_XXXX, but for metadata, which is handed in as the
 * C string parameter
 */
void babel_ifiction_ifid(char *);
void babel_ifiction_verify(char *);
void babel_ifiction_fish(char *);
void babel_ifiction_lint(char *);

/* Functions from babel_multi_functions.c
 *
 */
void babel_multi_blorb(char **, char * , int);
void babel_multi_blorb1(char **, char * , int);
void babel_multi_complete(char **, char *, int);

/* uncomment this line on platforms which limit extensions to 3 characters */
/* #define THREE_LETTER_EXTENSIONS */
