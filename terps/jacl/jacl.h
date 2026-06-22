/* jacl.h --- Header file for all JACL code.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __NDS__
#include <nds.h>
#include <fat.h>
#endif

#ifdef GLK
#include "glk.h"
#include "gi_blorb.h"
#endif

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

#ifdef FCGIJACL
#include <fcgi_stdio.h>
#endif

#include "version.h"

#ifndef M_PI
#define M_PI        3.14159265358979323846
#endif

#ifdef _WIN32
#define DIR_SEPARATOR '\\'
#define TEMP_DIR "temp\\"
#define DATA_DIR "data\\"
#define INCLUDE_DIR "include\\"
#else
#define DIR_SEPARATOR '/'
#define DATA_DIR "data/"
#define TEMP_DIR "temp/"
#define INCLUDE_DIR "include/"
#endif

#ifdef __APPLE__
#define get_string jacl_get_string
#endif

extern short int spaced;

extern char		temp_directory[];
extern char		data_directory[];

extern int		walkthru_running;

extern int		start_of_last_command;
extern int		start_of_this_command;

extern int      objects, integers, functions, strings;

extern int		start_of_this_command;
extern int		start_of_last_command;
extern int		buffer_index;
extern int		objects;
extern int		integers;
extern int		player;
extern int		oec;
extern int		*object_element_address;
extern int		*object_backup_address;

extern char		error_buffer[];
extern char		temp_buffer[];

extern char		user_id[];
extern char		prefix[];
extern char		processed_file[];
extern char		function_name[];

extern char		override[];
extern char		game_file[];
extern char		game_path[];
extern char		blorb[];
extern char		bookmark[];
extern char		walkthru[];
extern char		include_directory[];
extern char		temp_directory[];
extern char		data_directory[];
extern int		noun[];
extern char		last_command[];

/* STYLE HINTS DECLARED AT THE TOP LEVEL OF A GAME FILE.
 * Indexed by the BOLD..PRE constants in constants.h (index 0 is unused).
 * Populated by the 'stylehint' loader handler, applied to Glk before windows
 * are opened, and consulted by the web interpreter when emitting style tags. */
typedef struct stylehint_t {
	int has_text_color;
	int has_back_color;
	int has_reverse;
	unsigned long text_color;
	unsigned long back_color;
	int reverse_value;
} stylehint_t;

#define STYLEHINT_TABLE_SIZE 10

extern stylehint_t				jacl_stylehints[STYLEHINT_TABLE_SIZE];
extern int						jacl_stylehints_dirty;

int  stylehint_index_from_name(const char *name);
int  stylehint_parse_color(const char *spec, unsigned long *out);

extern struct object_type		*object[];
extern struct integer_type		*integer_table;
extern struct integer_type		*integer[];
extern struct cinteger_type		*cinteger_table;
extern struct attribute_type	*attribute_table;
extern struct string_type		*string_table;
extern struct string_type		*cstring_table;
extern struct function_type		*function_table;
extern struct function_type		*executing_function;
extern struct command_type		*completion_list;
extern struct word_type			*grammar_table;
extern struct synonym_type		*synonym_table;
extern struct filter_type		*filter_table;

#ifdef GLK
extern schanid_t				sound_channel[];
extern schanid_t				fade_channel[];
extern strid_t					game_stream;
extern winid_t					mainwin;
extern winid_t 					statuswin;
extern winid_t 					current_window;

extern glui32 					status_width, status_height;

extern strid_t 					mainstr;
extern strid_t 					statusstr;
extern strid_t 					quotestr;
extern strid_t 					inputstr;
#else
extern FILE                     *file;
#endif
