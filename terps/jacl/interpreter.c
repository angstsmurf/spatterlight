/* interpreter.c --- Main functions for executing JACL code.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"
#include "csv.h"
#include "errno.h"
#include "interpreter.h"
#include "parser.h"
#include "encapsulate.h"
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>

#ifdef _WIN32
struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_GETLK  5
#define F_SETLK  6
#define F_SETLKW 7

#define F_RDLCK  0
#define F_WRLCK  1
#define F_UNLCK  2

int fcntl(int __fd, int __cmd, ...) { return 0; }
#endif /* _WIN32 */

#ifndef strcasestr
char *strcasestr(const char *s, const char *find)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != 0) {
        c = (char)tolower((unsigned char)c);
        len = strlen(find);
        do {
            do {
                if ((sc = *s++) == 0)
                    return (NULL);
            } while ((char)tolower((unsigned char)sc) != c);
        } while (strncasecmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}
#endif

#ifdef __NDS__
extern void jflush(void);
#endif
#ifndef GLK
extern int web_status_emit_count;
#endif

#define MAX_TRY 10

#ifdef GLK
/* Cross-fade duration for sound transitions on the Glk side, in ms.
 * Mirrors JACL_FADE_MS in webinterface.library so both interpreters
 * sound the same. */
#define JACL_FADE_MS 600

/* Per-logical-channel target volume the next play should ramp UP to,
 * scaled to Glk's 0-65535 range. Updated by the `volume` command and
 * read by the `sound` command. Defaults to full volume so a game that
 * never sets volume still gets audible playback. */
static glui32 jacl_target_volume[8] = {
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535
};
#endif

static struct flock    read_lck;
static int            read_fd;
static struct flock    write_lck;
static int            write_fd;

#ifndef GLK
static char *url_encode(const char *str);
static char to_hex(char code);
void web_render_status_bar(void);
#endif

static const char * const location_attributes[] = {
 "VISITED ", "DARK ", "ON_WATER ", "UNDER_WATER ", "WITHOUT_AIR ", "OUTDOORS ",
 "MID_AIR ", "TIGHT_ROPE ", "POLLUTED ", "SOLVED ", "MID_WATER ", "DARKNESS ",
 "MAPPED ", "KNOWN ",
 NULL };

static const char * const object_attributes[] = {
 "CLOSED ", "LOCKED ", "DEAD ", "IGNITABLE ", "WORN ", "CONCEALING ",
 "LUMINOUS ", "WEARABLE ", "CLOSABLE ", "LOCKABLE ", "ANIMATE ", "LIQUID ",
 "CONTAINER ", "SURFACE ", "PLURAL ", "FLAMMABLE ", "BURNING ", "LOCATION ",
 "ON ", "DAMAGED ", "FEMALE ", "POSSESSIVE ", "OUT_OF_REACH ", "TOUCHED ",
 "SCORED ", "SITTING ", "NPC ", "DONE ", "GAS ", "NO_TAB ",
 "NOT_IMPORTANT ", NULL };

static const char * const object_elements[] = {
 "parent", "capacity", "mass", "bearing", "velocity", "next", "previous",
 "child", "index", "status", "timer", "counter", "points", "class", "x", "y", 
 NULL };

static const char * const location_elements[] = {
 "north", "south", "east", "west", "northeast", "northwest", "southeast",
 "southwest", "up", "down", "in", "out", "points", "class", "x", "y", 
 NULL };

struct csv_parser                parser_csv;
static char                        in_name[1024];
static char                        out_name[1024];
static FILE                     *infile, *outfile, *tempfile;
static int                        first_column;

static int                         stack = 0;
static int                         proxy_stack = 0;

static int                         field_no = 0;

static struct stack_type        backup[STACK_SIZE];
static struct proxy_type        proxy_backup[STACK_SIZE];

static struct function_type *resolved_function = NULL;
#if defined(GLK) || defined(__NDS__)
static struct string_type *resolved_string = NULL;
#endif

static struct string_type *new_string = NULL;
struct string_type *current_cstring = NULL;
static struct string_type *previous_cstring = NULL;

static struct cinteger_type *new_cinteger = NULL;
struct cinteger_type *current_cinteger = NULL;
static struct cinteger_type *previous_cinteger = NULL;

static unsigned int                bit_mask;
extern int                         encrypted;

extern char                        rpc_function_name[];
static char                        csv_buffer[2048];

int                                resolved_attribute;

/* THE ITERATION VARIABLE USED FOR LOOPS */
static int                        *loop_integer = NULL;
static int                        *select_integer = NULL;

static int                        criterion_value = 0;
static int                        criterion_type = 0;
static int                        criterion_negate = FALSE;
static int                         current_level;
static int                         execution_level;
#if defined(GLK) || defined(__NDS__)
static int                         *ask_integer;
#endif
static int                        new_x;
static int                        new_y;

int                                interrupted = FALSE;
static char                     string_buffer[2048];
static char                        argument_buffer[1024];
#ifdef GLK
static glsi32                      top_of_loop = 0;
static glsi32                      top_of_select = 0;
static glsi32                    top_of_while = 0;
static glsi32                    top_of_iterate = 0;
static glsi32                    top_of_update = 0;
static glsi32                     top_of_do_loop = 0;
#else
char                              option_buffer[2024];
int                                style_stack[100];
int                                style_index = 0;
long                               top_of_loop = 0;
long                               top_of_select = 0;
long                               top_of_while = 0;
long                               top_of_iterate = 0;
long                               top_of_update = 0;
long                               top_of_do_loop = 0;

#endif

#ifdef __NDS__
extern int                        bold_mode;
extern int                        pre_mode;
extern int                        reverse_mode;
extern int                        input_mode;
extern int                        subheader_mode;
extern int                        note_mode;
#endif

// VALUES FROM LOADER
extern int                        value_resolved;

stylehint_t                       jacl_stylehints[STYLEHINT_TABLE_SIZE];
int                               jacl_stylehints_dirty = 0;

int
stylehint_index_from_name(const char *name)
{
    if (name == NULL) return -1;
    if (!strcmp(name, "bold") || !strcmp(name, "emphasised")) return BOLD;
    if (!strcmp(name, "note")) return NOTE;
    if (!strcmp(name, "input")) return INPUT;
    if (!strcmp(name, "header")) return HEADER;
    if (!strcmp(name, "subheader")) return SUBHEADER;
    if (!strcmp(name, "reverse") || !strcmp(name, "inverse")) return REVERSE;
    if (!strcmp(name, "pre") || !strcmp(name, "preformatted")) return PRE;
    if (!strcmp(name, "alert")) return ALERT;
    if (!strcmp(name, "quote") || !strcmp(name, "blockquote")) return QUOTE;
    return -1;
}

int
stylehint_parse_color(const char *spec, unsigned long *out)
{
    static const struct { const char *name; unsigned long rgb; } named[] = {
        {"black",     0x000000UL},
        {"white",     0xffffffUL},
        {"red",       0xcc0000UL},
        {"green",     0x00aa00UL},
        {"blue",      0x0000ccUL},
        {"yellow",    0xcccc00UL},
        {"cyan",      0x00ccccUL},
        {"magenta",   0xcc00ccUL},
        {"gray",      0x808080UL},
        {"grey",      0x808080UL},
        {"lightgray", 0xc0c0c0UL},
        {"lightgrey", 0xc0c0c0UL},
        {"darkgray",  0x404040UL},
        {"darkgrey",  0x404040UL},
        {"orange",    0xff8000UL},
        {"purple",    0x800080UL},
        {"brown",     0x804000UL},
        {"pink",      0xff80c0UL},
        {NULL, 0}
    };
    char buf[32];
    size_t len, i;
    int j;

    if (spec == NULL || spec[0] == 0 || out == NULL) return -1;

    if (spec[0] == '#') {
        const char *hex = spec + 1;
        unsigned long val = 0;
        len = strlen(hex);
        if (len != 3 && len != 6) return -1;
        for (i = 0; i < len; i++) {
            char c = hex[i];
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return -1;
            val = (val << 4) | (unsigned long) d;
        }
        if (len == 3) {
            unsigned long r = (val >> 8) & 0xfUL;
            unsigned long g = (val >> 4) & 0xfUL;
            unsigned long b = val & 0xfUL;
            val = (r << 20) | (r << 16) | (g << 12) | (g << 8) | (b << 4) | b;
        }
        *out = val;
        return 0;
    }

    len = strlen(spec);
    if (len >= sizeof(buf)) return -1;
    for (i = 0; i <= len; i++) {
        buf[i] = (char) tolower((unsigned char) spec[i]);
    }
    for (j = 0; named[j].name != NULL; j++) {
        if (!strcmp(buf, named[j].name)) {
            *out = named[j].rgb;
            return 0;
        }
    }
    return -1;
}

char                              integer_buffer[16];
static char                        called_name[1024];
static char                        scope_criterion[24];
static const char                *output;

static int exit_function(int return_code);
static int select_next(void);
static void new_position(double x1, double y1, double bearing, double velocity);
static void pop_proxy(void);
static int push_proxy(void);
static void pop_stack(void);
static int condition(void);
static int and_condition(void);
static int and_strcondition(void);
static int bearing(double x1, double y1, double x2, double y2);
static int distance(double x1, double y1, double x2, double y2);
static int strcondition(void);
static void set_arguments(const char *function_call);

_Noreturn void
terminate(int code)
{
    // FREE ANY EXTRA RAM ALLOCATED BY THE CSV PARSER
    csv_free(&parser_csv);

#ifdef GLK
    int index;
    event_t            event;

    // FLUSH THE GLK WINDOW SO THE ERROR GETS DISPLAYED IMMEDIATELY.
    glk_select_poll(&event);

    /* CLOSE THE SOUND CHANNELS */
    for (index = 0; index < 8; index++) {
        if (sound_channel[index] != NULL) {
            glk_schannel_destroy(sound_channel[index]);
        }
        if (fade_channel[index] != NULL) {
            glk_schannel_destroy(fade_channel[index]);
        }
    }

    /* CLOSE THE STREAM */
    if (game_stream != NULL) {
        glk_stream_close(game_stream, NULL);
    }
    
    glk_exit();
#else
    if (file != NULL)           /* CLOSE THE GAME FILE */
        fclose(file);

    exit(code);
#endif
}

static void
build_proxy(void)
{
    int             index;
    size_t          used;
    size_t          room;
    const char     *piece;

    proxy_buffer[0] = 0;
    used = 0;

    /* LOOP THROUGH ALL THE PARAMETERS OF THE PROXY COMMAND
       AND BUILD THE MOVE TO BE ISSUED ON THE PLAYER'S BEHALF.
       Bounded concat: text_of_word can resolve to a 1024-byte
       cstring value, and several args quickly overflow the
       1024-byte proxy_buffer. */
    for (index = 1; word[index] != NULL; index++) {
        piece = text_of_word(index);
        if (piece == NULL) continue;
        room = (used + 1 < 1024)
               ? (1024 - used - 1) : 0;
        if (room == 0) break;
        strncat(proxy_buffer, piece, room);
        used += strlen(piece);
        if (used >= 1024 - 1) {
            break;
        }
    }

    for (index = 0; index < (int) strlen(proxy_buffer); index++) {
        if (proxy_buffer[index] == '~') {
            proxy_buffer[index] = '\"';
        }
    }

    //printf("--- proxy buffer = \"%s\"\n", proxy_buffer);
}

static void 
cb1 (void *s, size_t i, void *not_used) {
    struct string_type *resolved_cstring;

    //sprintf (temp_buffer, "Trying to set field %d to equal %s^", field_no, (char *) s);
    //write_text(temp_buffer);

    sprintf (temp_buffer, "field[%d]", field_no);

    if ((resolved_cstring = cstring_resolve(temp_buffer)) != NULL) {
        //write_text("Resolved ");
        //write_text(temp_buffer);
        //write_text("^");
        strncpy (resolved_cstring->value, s, i);
        resolved_cstring->value[i] = 0;
        //sprintf(temp_buffer, "Setting field %d to ~%s~^", field_no, (char *) s);
        //write_text(temp_buffer);
        // INCREMENT THE FIELD NUMBER SO THE NEXT ONE GETS STORED IN THE RIGHT CONSTANT
        field_no++;    
    } else {
        write_text("Can't resolve ");
        write_text(temp_buffer);
        write_text("^");
    }

}

static void 
cb2 (int c, void *not_used) {
    // THE END OF THE RECORD HAS BEEN REACHED, EXPORT THE NUMBER OF FIELDS READ
    struct cinteger_type *resolved_cinteger;

    if ((resolved_cinteger = cinteger_resolve("field_count")) != NULL) {
        resolved_cinteger->value = field_no;
    }
}

int
execute(const char *funcname)
{
    int             index;
    int             counter;
    int            *container;

    int             object_1,
                    object_2;

    /* THESE VARIABLE KEEP TRACK OF if AND endif COMMANDS TO DECIDE WHETHER
     *THE CURRENT LINE OF CODE SHOULD BE EXECUTED OR NOT */
    int             current_level = 0;
    int             execution_level = 0;

    /* THESE ARE USED AS FILE POINTER OFFSETS TO RETURN TO FIXED
     * POINTS IN THE GAME FILE */
#ifdef GLK
    glsi32            before_command = 0;
#else
    long            before_command = 0;
#endif

    /* called_name is [1024]; strncpy(..., 1023) leaves [1023]
     * un-NUL'd if the source fills the limit, so any subsequent
     * strlen/strcmp on called_name reads past the buffer. Force a
     * terminator after every copy. */
#ifdef GLK
    strncpy (called_name, funcname, 1023);
    called_name[1023] = 0;
#else
    if (!strcmp(funcname, "+rpc")) {
        strncpy (called_name, rpc_function_name, 1023);
        called_name[1023] = 0;
    } else {
        strncpy (called_name, funcname, 1023);
        called_name[1023] = 0;
    }

#endif
    /* GET THE FUNCTION OBJECT BY THE FUNCTION NAME */
    resolved_function = function_resolve(called_name);

    if (resolved_function == NULL) {
        //printf("--- failed to find %s\n", called_name);
        return (FALSE);
    }

    /* If the call stack is full, bail out cleanly. The previous code
     * called terminate(45) and killed the worker; now we surface the
     * failure as "function not callable" so the caller can return up
     * the chain. */
#ifdef GLK
    if (push_stack(glk_stream_get_position(game_stream)) == FALSE) {
        return (FALSE);
    }
#else
    if (push_stack(ftell(file)) == FALSE) {
        return (FALSE);
    }
#endif

    top_of_loop = 0;
    top_of_select = 0;
    top_of_while = 0;
    top_of_iterate = 0;
    top_of_update = 0;
    top_of_do_loop = 0;

    executing_function = resolved_function;
    executing_function->call_count++;

    // CREATE ALL THE PASSED ARGUMENTS AS JACL INTEGER CONSTANTS
    set_arguments(called_name);

    // SET function_name TO THE CORE NAME STORED IN THE FUNCTION OBJECT
    // LEAVING called_name TO CONTAIN THE FULL ARGUMENT LIST
    strncpy (function_name, executing_function->name, 80);
    function_name[80] = 0;
    {
        struct string_type *fname_cstring = cstring_resolve("function_name");
        strncpy (fname_cstring->value, executing_function->name, 80);
        fname_cstring->value[80] = 0;
    }

    //sprintf(temp_buffer, "--- starting to execute %s^", function_name);
    //write_text(temp_buffer);

    // JUMP TO THE POINT IN THE PROCESSED GAME FILE WHERE THIS FUNCTION STARTS 
#ifdef GLK
    glk_stream_set_position(game_stream, executing_function->position, seekmode_Start);
    before_command = executing_function->position;
    glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);
#else
    fseek(file, executing_function->position, SEEK_SET);
    before_command = executing_function->position;
    fgets(text_buffer, 1024, file);
#endif

    if (encrypted) jacl_deobfuscate(text_buffer);

    while (text_buffer[0] != '}' && !interrupted) {
        encapsulate();
        if (word[0] == NULL);
        else if (!strcmp(word[0], "endwhile")) {
            current_level--;
            if (current_level < execution_level) {
                // THIS ENDWHILE COMMAND WAS BEING EXECUTED, 
                // NOT JUST COUNTED.
                if (top_of_while == FALSE) {
                    sprintf(error_buffer, NO_WHILE, executing_function->name);
                    log_error(error_buffer, PLUS_STDOUT);
                } else {
#ifdef GLK
                    glk_stream_set_position(game_stream, top_of_while, seekmode_Start);
#else
                    fseek(file, top_of_while, SEEK_SET);
#endif
                    execution_level = current_level;
                }
            }
        } else if (!strcmp(word[0], "enditerate")) {
            current_level--;
            if (current_level < execution_level) {
                // THIS ENDITERATE COMMAND WAS BEING EXECUTED, 
                // NOT JUST COUNTED.
                if (top_of_iterate == FALSE) {
                    sprintf(error_buffer, NO_ITERATE, executing_function->name);
                    log_error(error_buffer, PLUS_STDOUT);
                } else {
#ifdef GLK
                    glk_stream_set_position(game_stream, top_of_iterate, seekmode_Start);
#else
                    fseek(file, top_of_iterate, SEEK_SET);
#endif
                    execution_level = current_level;
                }
            }
        } else if (!strcmp(word[0], "endupdate")) {
            current_level--;
            if (current_level < execution_level) {
                // THIS ENDUPDATE COMMAND WAS BEING EXECUTED, 
                // NOT JUST COUNTED.
                if (top_of_update == FALSE) {
                    sprintf(error_buffer, NO_UPDATE, executing_function->name);
                    log_error(error_buffer, PLUS_STDOUT);
                } else {
#ifdef GLK
                    glk_stream_set_position(game_stream, top_of_update, seekmode_Start);
#else
                    fseek(file, top_of_update, SEEK_SET);
#endif
                    execution_level = current_level;
                }
            }
        } else if (!strcmp(word[0], "print") && current_level != execution_level) {
            // SKIP THIS BLOCK OF PLAIN TEXT UNTIL IT FINDS A 
            // LINE THAT STARTS WITH A '.'
#ifdef GLK
            glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);
#else
            fgets(text_buffer, 1024, file);
#endif

            if (encrypted) jacl_deobfuscate(text_buffer);

            while (text_buffer[0] != '.') {
                // GET THE NEXT LINE
#ifdef GLK
                glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);
#else
                fgets(text_buffer, 1024, file);
#endif

                if (encrypted) jacl_deobfuscate(text_buffer);

            }
        } else if (!strcmp(word[0], "endif")) {
            current_level--;
            if (current_level < execution_level) {
                /* THIS SHOULD NEVER HAPPEN */    
                execution_level = current_level;
            }
        } else if (!strcmp(word[0], "endall")) {
            current_level = 0;
            execution_level = 0;
        } else if (!strcmp(word[0], "else")) {
            if (current_level == execution_level) {
                execution_level--;
            } else if (current_level == execution_level + 1) {
                execution_level++;
            }
        } else if (current_level == execution_level) {
            if (!strcmp(word[0], "look")) {
                // THIS IS JUST HERE FOR BACKWARDS COMPATIBILITY
                object[HERE]->attributes &= ~1L;
                look_around();
            } else if (!strcmp(word[0], "repeat")) {
#ifdef GLK
                top_of_do_loop = glk_stream_get_position(game_stream);
#else
                top_of_do_loop = ftell(file);
#endif
            } else if (!strcmp(word[0], "until")) {
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else {
                    if (top_of_do_loop == FALSE) {
                        sprintf(error_buffer, NO_REPEAT, executing_function->name);
                        log_error(error_buffer, PLUS_STDOUT);
                    } else if (!condition()) {
#ifdef GLK
                        glk_stream_set_position(game_stream, top_of_do_loop, seekmode_Start);
#else
                        fseek(file, top_of_do_loop, SEEK_SET);
#endif
                    }
                }
            } else if (!strcmp(word[0], "untilall")) {
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else {
                    if (top_of_do_loop == FALSE) {
                        sprintf(error_buffer, NO_REPEAT, executing_function->name);
                        log_error(error_buffer, PLUS_STDOUT);
                    } else if (!and_condition()) {
#ifdef GLK
                        glk_stream_set_position(game_stream, top_of_do_loop, seekmode_Start);
#else
                        fseek(file, top_of_do_loop, SEEK_SET);
#endif

                    }
                }
            } else if (!strcmp(word[0], "iterate")) {
                int i;

                // A NEW iterate LOOP MEANS STARTING BACK AT THE FIRST FIELD
                field_no = 0;

                current_level++;
                /* THIS LOOP COMES BACK TO THE START OF THE LINE CURRENTLY
                   EXECUTING, NOT THE LINE AFTER */

                top_of_iterate = before_command;

                // infile REMAINS OPEN DURING THE ITERATION, ONLY NEEDS
                // OPENING THE FIRST TIME
                if (infile == NULL) {
                    strcpy (temp_buffer, data_directory);
                    if (strstr(text_of_word(1), ".csv") != NULL) {
                        // NAME CONTAINS .csv, USE AS-IS (GLOBAL FILE)
                        strcat (temp_buffer, text_of_word(1));
                    } else {
                        // PREFIX WITH GAME NAME (PER-GAME FILE)
                        strcat (temp_buffer, prefix);
                        strcat (temp_buffer, "-");
                        strcat (temp_buffer, text_of_word(1));
                        strcat (temp_buffer, ".csv");
                    }

                    infile = fopen(temp_buffer, "rb");

                    if (infile != NULL
                        && word[2] != NULL
                        && !strcmp(word[2], "skip_header")) {
                        fgets(csv_buffer, 2048, infile);
                    }
                }

                if (infile == NULL) {
                        snprintf(error_buffer, 1024, "Failed to open file %s: %s\n", temp_buffer, strerror(errno));
                    log_error(error_buffer, LOG_ONLY);
                  } else {
                    if (word[1] == NULL) {
                        /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                        noproprun();
                        return(exit_function (TRUE));
                    } else {
                        // IF THERE IS ANOTHER RECORD TO READ FROM THE CSV FILE THEN
                        // SET THE field[] CONSTANTS AND INCREMENT THE execution_level
                        fgets(csv_buffer, 2048, infile);
                        if (!feof(infile)) {
                              i = strlen(csv_buffer);
                            //sprintf (temp_buffer, "Read ~%s~ with %d bytes.^", csv_buffer, i);
                            //write_text(temp_buffer);
                            if (csv_parse(&parser_csv, csv_buffer, i, cb1, cb2, (void *) NULL) != i) {
                                  snprintf(error_buffer, 1024, "Error parsing file: %s\n", csv_strerror(csv_error(&parser_csv)));
                                log_error(error_buffer, PLUS_STDOUT);
                                  fclose(infile);
                                  infile = NULL;
                            } else {
                                // A LINE HAS BEEN SUCCESSFULLY READ, EXECUTE THE CONTENTS OF THE LOOP
                                execution_level++;
                            }
                        } else {
                            fclose(infile);
                            infile = NULL;
                        }
                    }
                }
            } else if (!strcmp(word[0], "update")) {
                int i;
                int try = 0; 

                // SET UP THE RECORD LOCKING STRUCTURE, THE ADDRESS OF WHICH
                // IS PASSED TO THE fcntl() SYSTEM CALL
                write_lck.l_type = F_WRLCK;    // SETTING A WRITE LOCK
                write_lck.l_whence = 0;        // OFFSET l_start FROM BEGINNING OF FILE
                write_lck.l_start = 0LL; 
                write_lck.l_len = 0LL;        // UNTIL THE END OF THE FILE ADDRESS SPACE

                read_lck.l_type = F_RDLCK;    // SETTING A READ LOCK
                read_lck.l_whence = 0;        // OFFSET l_start FROM BEGINNING OF FILE
                read_lck.l_start = 0LL; 
                read_lck.l_len = 0LL;        // UNTIL THE END OF THE FILE ADDRESS SPACE

                // A NEW iterate LOOP MEANS STARTING BACK AT THE FIRST FIELD
                field_no = 0;

                current_level++;
                // THIS LOOP COMES BACK TO THE START OF THE LINE CURRENTLY
                // EXECUTING, NOT THE LINE AFTER 

                top_of_update = before_command;

                // infile REMAINS OPEN DURING THE ITERATION, ONLY NEEDS 
                // OPENING THE FIRST TIME
                if (infile == NULL) {
                    strcpy (in_name, data_directory);
                    if (strstr(text_of_word(1), ".csv") != NULL) {
                        strcat (in_name, text_of_word(1));
                    } else {
                        strcat (in_name, prefix);
                        strcat (in_name, "-");
                        strcat (in_name, text_of_word(1));
                        strcat (in_name, ".csv");
                    }

                    infile = fopen(in_name, "rb");
                }

                if (outfile == NULL) {
                    // OPEN A TEMPORARY OUTPUT FILE TO WRITE THE MODIFICATIONS TO
                    strcpy (out_name, data_directory);
                    if (strstr(text_of_word(1), ".csv") != NULL) {
                        // STRIP .csv AND ADD -<user_id>.csv
                        strcat (out_name, text_of_word(1));
                        out_name[strlen(out_name) - 4] = 0;
                        strcat (out_name, "-");
                        strcat (out_name, user_id);
                        strcat (out_name, ".csv");
                    } else {
                        strcat (out_name, prefix);
                        strcat (out_name, "-");
                        strcat (out_name, text_of_word(1));
                        strcat (out_name, "-");
                        strcat (out_name, user_id);
                        strcat (out_name, ".csv");
                    }

                    outfile = fopen(out_name, "wb");
                }

                if (infile == NULL) {
                    snprintf(error_buffer, 1024, "Failed to open input CSV file ~%s~: %s\n", in_name, strerror(errno));
                    log_error(error_buffer, LOG_ONLY);
                    if (outfile != NULL) {
                        fclose(outfile);
                        outfile = NULL;
                    }
                    return(exit_function (TRUE));
                  } else {
                    if (outfile == NULL) {
                        snprintf(error_buffer, 1024, "Failed to open output CSV file ~%s~: %s\n", out_name, strerror(errno));
                        log_error(error_buffer, LOG_ONLY);
                        if (infile != NULL) {
                            fclose(infile);
                            infile = NULL;
                        }
                        return(exit_function (TRUE));
                    } else {
                        write_fd = fileno(outfile);
                        // ATTEMPT LOCKING OUTPUT FILE MAX_TRY TIMES BEFORE GIVING UP. 
                          while (fcntl(write_fd, F_SETLK, &write_lck)<0) {  
                             if (errno == EAGAIN || errno == EACCES) { 
                                 // THERE MIGHT BE OTHER ERROR CASES IN WHICH 
                                  // USERS MIGHT TRY AGAIN 
                                  if (++try < MAX_TRY) { 
                                       jacl_sleep(1000); 
                                       continue; 
                                  } 
                                 sprintf(error_buffer, "File busy unable to get lock on output file.\n"); 
                                log_error(error_buffer, PLUS_STDOUT);
                                return(exit_function (TRUE));
                              } 
                          } 

                        try = 0;

                        read_fd = fileno(infile);
                        // ATTEMPT LOCKING OUTPUT FILE MAX_TRY TIMES BEFORE GIVING UP. 
                          while (fcntl(read_fd, F_SETLK, &read_lck)<0) {  
                             if (errno == EAGAIN || errno == EACCES) { 
                                 // THERE MIGHT BE OTHER ERROR CASES IN WHICH 
                                  // USERS MIGHT TRY AGAIN 
                                  if (++try < MAX_TRY) { 
                                       jacl_sleep(1000); 
                                       continue; 
                                  } 
                                 sprintf(error_buffer, "File busy unable to get lock on input file.\n"); 
                                log_error(error_buffer, PLUS_STDOUT);
                                return(exit_function (TRUE));
                              } 
                          } 

                        if (word[1] == NULL) {
                            /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                            noproprun();
                            return(exit_function (TRUE));
                        } else {
                            // IF THERE IS ANOTHER RECORD TO READ FROM THE CSV FILE THEN
                            // SET THE field[] CONSTANTS AND INCREMENT THE execution_level
                            fgets(csv_buffer, 2048, infile);
                            if (!feof(infile)) {
                                  i = strlen(csv_buffer);
                                if (csv_parse(&parser_csv, csv_buffer, i, cb1, cb2, (int *) &field_no) != i) {
                                      snprintf(error_buffer, 1024, "Error parsing file: %s\n", csv_strerror(csv_error(&parser_csv)));
                                    log_error(error_buffer, PLUS_STDOUT);
                                    read_lck.l_type = F_UNLCK;    // SETTING A READ LOCK
                                      fcntl(read_fd, F_SETLK, &read_lck);
                                      fclose(infile);
                                      infile = NULL;
                                } else {
                                    // A LINE HAS BEEN SUCCESSFULLY READ, EXECUTE THE CONTENTS OF THE LOOP
                                    execution_level++;
                                }
                            } else {
                                write_lck.l_type = F_UNLCK;    // REMOVE THE WRITE LOCK
                                  fcntl(write_fd, F_SETLK, &write_lck);
                                fclose(outfile);

                                read_lck.l_type = F_UNLCK;    // REMOVE THE READ LOCK
                                  fcntl(read_fd, F_SETLK, &read_lck);
                                fclose(infile);

                                rename (out_name, in_name);

                                outfile = NULL;
                                infile = NULL;
                            }
                        }
                    }
                }
            } else if (!strcmp(word[0], "while")) {
                current_level++;
                /* THIS LOOP COMES BACK TO THE START OF THE LINE CURRENTLY
                   EXECUTING, NOT THE LINE AFTER */
                top_of_while = before_command;
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else if (condition()) {
                    execution_level++;
                }
            } else if (!strcmp(word[0], "whileall")) {
                current_level++;
                /* THIS LOOP COMES BACK TO THE START OF THE LINE CURRENTLY
                   EXECUTING, NOT THE LINE AFTER */
                top_of_while = before_command;
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else if (and_condition()) {
                    execution_level++;
                }
            } else if (!strcmp(word[0], "loop")) {
                /* THE LOOP COMMAND LOOPS ONCE FOR EACH DEFINED 
                 * OBJECT (FOREACH) */
#ifdef GLK
                top_of_loop = glk_stream_get_position(game_stream);
#else
                top_of_loop = ftell(file);
#endif
                if (word[1] == NULL) {
                    // IF NONE IS SUPPLIED DEFAULT TO noun3
                    loop_integer = &noun[2];
                } else {
                    // STORE THE CONTAINER TO PUT THE CURRENT OBJECT IN
                    loop_integer = container_resolve(word[1]);

                    // IF THE SUPPLIED CONTAINER CAN'T BE RESOLVED
                    // DEFAULT TO noun3
                    if (loop_integer == NULL)
                        loop_integer = &noun[2];
                }

                // SET THE VALUE OF THE LOOP INDEX TO POINT TO THE FIRST OBJECT
                *loop_integer = 1;

            } else if (!strcmp(word[0], "endloop")) {
                if (top_of_loop == FALSE) {
                    sprintf(error_buffer, NO_LOOP, executing_function->name);
                    log_error(error_buffer, PLUS_STDOUT);
                } else {
                    *loop_integer += 1;
                    if (*loop_integer > objects) {
                        top_of_loop = FALSE;
                        *loop_integer = 0;
                    } else {
#ifdef GLK
                        glk_stream_set_position(game_stream, top_of_loop, seekmode_Start);
#else
                        fseek(file, top_of_loop, SEEK_SET);
#endif
                    }
                }
            } else if (!strcmp(word[0], "select")) {
                /* THE SELECT COMMAND LOOPS ONCE FOR EACH DEFINED 
                 * OBJECT THAT MATCHES THE SUPPLIED CRITERION */
#ifdef GLK
                top_of_select = glk_stream_get_position(game_stream);
#else
                top_of_select = ftell(file);
#endif
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else if (word[2] == NULL) {
                    // IF NONE IS SUPPLIED DEFAULT TO noun3
                    select_integer = &noun[2];
                } else {
                    // STORE THE CONTAINER TO PUT THE CURRENT OBJECT IN
                    select_integer = container_resolve(word[2]);

                    // IF THE SUPPLIED CONTAINER CAN'T BE RESOLVED
                    // DEFAULT TO noun3
                    if (select_integer == NULL) {
                        select_integer = &noun[2];
                    }
                }
                
                // SET THE VALUE OF THE SELECT INDEX TO ONE BEFORE THE 
                // FIRST OBJECT. THE NEXT FUNCTION AUTOMATICALLY INCREMENTS
                // THE INDEX BY ONE AT THE START OF THE WHILE LOOP.
                *select_integer = 0;

                if (word[1][0] == '!') {
                    criterion_negate = TRUE;
                    strcpy(argument_buffer, &word[1][1]);
                } else {
                    criterion_negate = FALSE;
                    strcpy(argument_buffer, word[1]);
                }

                // DETERMINE THE CRITERION FOR SELETION
                if (!strcmp(argument_buffer, "*held")
                            || !strcmp(argument_buffer, "*here")
                            || !strcmp(argument_buffer, "*anywhere")
                            || !strcmp(argument_buffer, "*present")) {
                    criterion_type = CRI_SCOPE;
                    strncpy(scope_criterion, argument_buffer, 23);
                    scope_criterion[23] = 0;
                } else if ((criterion_value = attribute_resolve(argument_buffer))) {
                    criterion_type = CRI_ATTRIBUTE;
                } else if ((criterion_value = user_attribute_resolve(argument_buffer))) {
                    criterion_type = CRI_USER_ATTRIBUTE;
                } else {
                    // USE VALUE OF AS A CATCH ALL IF IT IS NOT AN ATTRIBUTE OR SCOPE
                    criterion_value = value_of(argument_buffer, 0);

                    if (value_resolved) {
                        criterion_type = CRI_PARENT;
                    } else {
                        // CAN'T RESOLVE CRITERION
                        criterion_type = CRI_NONE;    
                    }
                }

                if (criterion_type != CRI_NONE) {
                    if (select_next() == FALSE) {
                        *select_integer = 0;
                        top_of_select = 0;
                    }
                } else {
                    *select_integer = 0;
                }

                if (*select_integer == 0) {
                    // THERE ARE NO MATCHING OBJECTS SO JUMP TO THE endselect
#ifdef GLK
                    glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);
#else
                    fgets(text_buffer, 1024, file);
#endif

                    if (encrypted) jacl_deobfuscate(text_buffer);

                    while (text_buffer[0] != '}') {
                        encapsulate();
                        if (word[0] != NULL && !strcmp(word[0], "endselect")) {
                            break;
                        }
#ifdef GLK
                        glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);
#else
                        fgets(text_buffer, 1024, file);
#endif
                    }
                }
            } else if (!strcmp(word[0], "endselect")) {
                if (top_of_select == FALSE) {
                    sprintf(error_buffer, NO_LOOP, executing_function->name);
                    log_error(error_buffer, PLUS_STDOUT);
                } else {
                    if (select_next()) {
#ifdef GLK
                        glk_stream_set_position(game_stream, top_of_select, seekmode_Start);
#else
                        fseek(file, top_of_select, SEEK_SET);
#endif
                    } else {
                        *select_integer = 0;
                        top_of_select = 0;
                    }
                }
            } else if (!strcmp(word[0], "break")) {
                current_level++;
                execution_level--;
            } else if (!strcmp(word[0], "cursor")) {
                if (word[2] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else {
#ifdef GLK
                    if (current_window == statuswin) {
                        glk_window_move_cursor(statuswin, value_of(word[1], TRUE), value_of(word[2], TRUE));
                    } else {
                        log_error(BAD_CURSOR, PLUS_STDOUT);
                    }
#else
                    /* Web build: only meaningful inside
                     * +update_status_window_web, where the grid is
                     * active. Outside of that the call is a no-op so
                     * games can share +print_centred / +print_right
                     * helpers between GLK and web. */
                    if (web_status_active()) {
                        web_status_cursor(value_of(word[1], TRUE),
                                          value_of(word[2], TRUE));
                    }
#endif
                }
#ifdef GLK
            } else if (!strcmp(word[0], "stop")) {
                int channel;

                if (SOUND_SUPPORTED->value) {
                    /* SET THE CHANNEL TO STOP, IF SUPPLIED */
                    if (word[1] == NULL) {
                        channel = 0;
                    } else {
                        channel = value_of(word[1], TRUE);

                        /* SANITY CHECK THE CHANNEL SELECTED */
                        if (channel < 0 || channel > 7) {
                            channel = 0;
                        }
                    }
                    /* Fade out instead of cutting. The schannel keeps
                     * running silently until the next play replaces it. */
                    glk_schannel_set_volume_ext(sound_channel[channel],
                                                0, JACL_FADE_MS, 0);
                }
            } else if (!strcmp(word[0], "volume")) {
                int channel, volume;

                if (SOUND_SUPPORTED->value) {
                    /* SET THE CHANNEL TO STOP, IF SUPPLIED */
                    if (word[2] == NULL) {
                        channel = 0;
                    } else {
                        channel = value_of(word[2], TRUE);

                        /* SANITY CHECK THE CHANNEL SELECTED */
                        if (channel < 0 || channel > 7) {
                            channel = 0;
                        }
                    }

                    if (word[1] == NULL) {
                        /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                        noproprun();
                        return(exit_function (TRUE));
                    } else {
                        volume = value_of(word[1], TRUE);

                        /* SANITY CHECK THE CHANNEL SELECTED */
                        if (volume < 0) {
                            volume = 0;
                        }

                        if (volume > 100) {
                            volume = 100;
                        }

                        /* STORE A COPY OF THE CURRENT VOLUME FOR ACCESS
                         * FROM JACL CODE */
                        sprintf(temp_buffer, "volume[%d]", channel);
                        cinteger_resolve(temp_buffer)->value = volume;

                        /* NOW SCALE THE 0-100 VOLUME TO THE 0-65536 EXPECTED
                         * BY Glk */
                        volume = volume * 655;

                        /* Remember this as the target the next play on
                         * this channel should ramp up to, AND apply it
                         * immediately to the currently-active sound. */
                        jacl_target_volume[channel] = (glui32) volume;
                        glk_schannel_set_volume(sound_channel[channel], (glui32) volume);
                    }
                }
            } else if (!strcmp(word[0], "timer")) {
                if (TIMER_SUPPORTED->value && TIMER_ENABLED->value) {
                    if (word[1] == NULL) {
                        /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                        noproprun();
                        return(exit_function (TRUE));
                    } else {
                        index = value_of(word[1], TRUE);
                        /* DON'T ALLOW NEGATIVE VALUES, BUT NO UPPER LIMIT */
                        if (index < 0) index = 0;

                        /* SET THE GLK TIMER */
                        glk_request_timer_events((glui32) index);

                        /* EXPOSE THE CURRENT VALUE THROUGH A JACL CONSTANT
                           SO THAT GAME CODE CAN READ THE IT */
                        cinteger_resolve("event_timer")->value = index;
                    }
                }
            } else if (!strcmp(word[0], "sound")) {
                int channel;
                glui32 repeats;

                if (SOUND_SUPPORTED->value && SOUND_ENABLED->value) {
                    /* SET THE CHANNEL TO USE, IF SUPPLIED */
                    if (word[2] == NULL) {
                        channel = 0;
                    } else {
                        channel = value_of(word[2], TRUE);

                        /* SANITY CHECK THE CHANNEL SELECTED */
                        if (channel < 0 || channel > 7) {
                            channel = 0;
                        }
                    }

                    /* SET THE NUMBER OF REPEATS, IF SUPPLIED */
                    if (word[3] == NULL) {
                        repeats = 1;
                    } else {
                        repeats = value_of(word[3], TRUE);
                    }

                    if (word[1] == NULL) {
                        /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                        noproprun();
                        return(exit_function (TRUE));
                    } else {
                        /* Cross-fade: swap primary and secondary slots so
                         * the new sound plays on a clean channel while the
                         * previous one keeps playing on the now-secondary
                         * slot, then ramp them in opposite directions. */
                        schanid_t old_chan = sound_channel[channel];
                        sound_channel[channel] = fade_channel[channel];
                        fade_channel[channel] = old_chan;

                        glk_schannel_stop(sound_channel[channel]);
                        glk_schannel_set_volume(sound_channel[channel], 0);
                        if (glk_schannel_play_ext(sound_channel[channel], (glui32) value_of(word[1], TRUE), repeats, channel + 1) == 0) {
                            /* THE CHANNEL NUMBER IS PASSED SO THAT THE SOUND
                             * NOTIFICATION EVENT CAN USE THE INFORMATION
                             * IT HAS 1 ADDED TO IT SO THAT IT IS A NON-ZERO
                             * NUMBER AND THE EVENT IS ACTIVATED */
                            sprintf(error_buffer, "Unable to play sound: %ld", value_of(word[1], FALSE));
                            log_error(error_buffer, PLUS_STDERR);
                        } else {
                            glk_schannel_set_volume_ext(sound_channel[channel],
                                                        jacl_target_volume[channel],
                                                        JACL_FADE_MS, 0);
                            if (fade_channel[channel] != NULL) {
                                glk_schannel_set_volume_ext(fade_channel[channel],
                                                            0, JACL_FADE_MS, 0);
                            }
                        }
                    }
                }
            } else if (!strcmp(word[0], "image")) {
                if (GRAPHICS_SUPPORTED->value && GRAPHICS_ENABLED->value) {
                    if (word[1] == NULL) {
                        /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                        noproprun();
                        return(exit_function (TRUE));
                    } else {
                        if (glk_image_draw(mainwin, (glui32) value_of(word[1], TRUE), imagealign_InlineDown, 0) == 0) {
                            sprintf(error_buffer, "Unable to draw image: %ld", value_of(word[1], FALSE));
                            log_error(error_buffer, PLUS_STDERR);
                        }
                    }
                }
            } else if (!strcmp(word[0], "askstring") || !strcmp(word[0], "getstring")) {
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    /* GET A POINTER TO THE STRING BEING MODIFIED */
                    if ((resolved_string = string_resolve(word[1])) == NULL) {
                        unkstrrun(word[1]);
                        return (exit_function(TRUE));
                    }

                    // PROMPT THE USER TO INPUT A STRING AND STORE IT IN THE
                    // RESOLVED VARIABLE
                    get_string(resolved_string->value, sizeof(resolved_string->value));
                }
                
            } else if (!strcmp(word[0], "asknumber") || !strcmp(word[0], "getnumber")) {
                int low, high;

                int insist = FALSE;

                /* THE ONLY DIFFERENCE WITH THE getnumber COMMAND IS THAT
                 * IT INSISTS THE PLAYER GIVES A LEGAL RESPONSE */
                if (!strcmp(word[0], "getnumber")) {
                    insist = TRUE;
                }

                if (word[3] != NULL) {
                    ask_integer = container_resolve(word[1]);
                    if (ask_integer == NULL) {
                        unkvarrun(word[1]);
                        return(exit_function (TRUE));
                    }

                    low = value_of(word[2], TRUE);
                    high = value_of(word[3], TRUE);
                
                    if (high == -1 || low == -1) {
                        return(exit_function (TRUE));
                    }

                    *ask_integer = get_number(insist, low, high);
                } else {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                }
            } else if (!strcmp(word[0], "getyesorno")) {
                if (word[1] != NULL) {
                    ask_integer = container_resolve(word[1]);
                    if (ask_integer == NULL) {
                        unkvarrun(word[1]);
                        return(exit_function (TRUE));
                    }

                    *ask_integer = get_yes_or_no();
                } else {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                }
            } else if (!strcmp(word[0], "clear")) {
                if (!walkthru_running) {
                    glk_window_clear(current_window);
                }
            } else if (!strcmp(word[0], "terminate")) {
                terminate(0);
            } else if (!strcmp(word[0], "more")) {
                if (word[1] == NULL) {
                    more ("[MORE]");
                } else {
                    more (word[1]);
                }
            } else if (!strcmp(word[0], "style")) {
                /* THIS COMMAND IS USED TO OUTPUT ANSI CODES OR SET GLK 
                 * STREAM STYLES */
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else {
                    if (!strcmp(word[1], "bold") 
                                || !strcmp(word[1], "emphasised")) {
                        glk_set_style(style_Emphasized);
                    } else if (!strcmp(word[1], "note")) {
                        glk_set_style(style_Note);
                    } else if (!strcmp(word[1], "input")) {
                        glk_set_style(style_Input);
                    } else if (!strcmp(word[1], "header")) {
                        glk_set_style(style_Header);
                    } else if (!strcmp(word[1], "subheader")) {
                        glk_set_style(style_Subheader);
                    } else if (!strcmp(word[1], "reverse")
                                || !strcmp(word[1], "inverse")) {
                        if (current_window == mainwin) {
                            glk_set_style(style_User2);
                        } else {
                            glk_set_style(style_User1);
                        }
                    } else if (!strcmp(word[1], "pre")
                                || !strcmp(word[1], "preformatted")) {
                        glk_set_style(style_Preformatted);
                    } else if (!strcmp(word[1], "alert")) {
                        glk_set_style(style_Alert);
                    } else if (!strcmp(word[1], "quote")
                                || !strcmp(word[1], "blockquote")) {
                        glk_set_style(style_BlockQuote);
                    } else if (!strcmp(word[1], "normal")) {
                        glk_set_style(style_Normal);
                    }
                }
            } else if (!strcmp(word[0], "flush")) {
            } else if (!strcmp(word[0], "hyperlink")) {
                /* OUTPUT LINK TEXT AS PLAIN TEXT UNDER Glk */
                if (word[2] == NULL) {
                    noproprun();
                    /* exit_function closes infile/outfile + releases
                     * the read/write locks, then pops the stack. The
                     * old pop_stack() alone leaked the CSV file
                     * descriptor if any of these CGI opcodes was hit
                     * inside an iterate block with too few args. */
                    return (exit_function(TRUE));
                } else {
                    write_text(text_of_word(1));
                }
#else
#ifdef __NDS__
            } else if (!strcmp(word[0], "flush")) {
                jflush();
            } else if (!strcmp(word[0], "cursor")) {
                if (word[2] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else {
                    printf("\x1b[%d;%dH", (int) value_of(word[1], TRUE), (int) value_of(word[2], TRUE));
                }
            } else if (!strcmp(word[0], "stop")) {
            } else if (!strcmp(word[0], "volume")) {
            } else if (!strcmp(word[0], "timer")) {
            } else if (!strcmp(word[0], "sound")) {
            } else if (!strcmp(word[0], "image")) {
            } else if (!strcmp(word[0], "askstring") || !strcmp(word[0], "getstring")) {
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    /* GET A POINTER TO THE STRING BEING MODIFIED */
                    if ((resolved_string = string_resolve(word[1])) == NULL) {
                        unkstrrun(word[1]);
                        return (exit_function(TRUE));
                    }

                    // PROMPT THE USER TO INPUT A STRING AND STORE IT IN THE
                    // RESOLVED VARIABLE
                    get_string(resolved_string->value, sizeof(resolved_string->value));
                }
                
            } else if (!strcmp(word[0], "asknumber") || !strcmp(word[0], "getnumber")) {
                int low, high;

                int insist = FALSE;

                /* THE ONLY DIFFERENCE WITH THE getnumber COMMAND IS THAT
                 * IT INSISTS THE PLAYER GIVES A LEGAL RESPONSE */
                if (!strcmp(word[0], "getnumber")) {
                    insist = TRUE;
                }

                if (word[3] != NULL) {
                    ask_integer = container_resolve(word[1]);
                    if (ask_integer == NULL) {
                        unkvarrun(word[1]);
                        return(exit_function (TRUE));
                    }

                    low = value_of(word[2], TRUE);
                    high = value_of(word[3], TRUE);
                
                    if (high == -1 || low == -1) {
                        return(exit_function (TRUE));
                    }

                    *ask_integer = get_number(insist, low, high);
                } else {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                }
            } else if (!strcmp(word[0], "getyesorno")) {
                if (word[1] != NULL) {
                    ask_integer = container_resolve(word[1]);
                    if (ask_integer == NULL) {
                        unkvarrun(word[1]);
                        return(exit_function (TRUE));
                    }

                    *ask_integer = get_yes_or_no();
                } else {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                }
            } else if (!strcmp(word[0], "clear")) {
                clrscrn();
            } else if (!strcmp(word[0], "terminate")) {
                terminate(0);
            } else if (!strcmp(word[0], "more")) {
                if (word[1] == NULL) {
                    more ("[MORE]");
                } else {
                    more (word[1]);
                }
            } else if (!strcmp(word[0], "style")) {
                /* THIS COMMAND IS USED TO OUTPUT ANSI CODES OR SET GLK 
                 * STREAM STYLES */
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else {
                    if (!strcmp(word[1], "bold") 
                                || !strcmp(word[1], "emphasised")) {
                        printf("\x1b[37;1m");     // SET TO BRIGHT WHITE
                        bold_mode = TRUE;
                    } else if (!strcmp(word[1], "note")) {
                        printf("\x1b[34;1m");     // SET TO BRIGHT BLUE
                        note_mode = TRUE;
                    } else if (!strcmp(word[1], "input")) {
                        printf("\x1b[32;0m");     // SET TO DIM GREEN
                        input_mode = TRUE;
                    } else if (!strcmp(word[1], "header")) {
                        printf("\x1b[37;0m");     // SET TO DIM WHITE
                    } else if (!strcmp(word[1], "subheader")) {
                        printf("\x1b[33;1m");     // SET TO BRIGHT YELLOW
                        subheader_mode = TRUE;
                    } else if (!strcmp(word[1], "reverse")
                                || !strcmp(word[1], "inverse")) {
                        printf("\x1b[7m");    // SET TO DIM WHITE
                        reverse_mode = TRUE;
                    } else if (!strcmp(word[1], "pre")
                                || !strcmp(word[1], "preformatted")) {
                        printf("\x1b[37;0m");     // SET TO DIM WHITE
                        pre_mode = TRUE;
                    } else if (!strcmp(word[1], "alert")) {
                        printf("\x1b[31;1m");     // SET TO BRIGHT RED
                    } else if (!strcmp(word[1], "quote")
                                || !strcmp(word[1], "blockquote")) {
                        printf("\x1b[36;1m");     // SET TO BRIGHT CYAN
                    } else if (!strcmp(word[1], "normal")) {
                        printf("\x1b[37;0m");     // SET TO DIM WHITE
                        bold_mode = FALSE;
                        pre_mode = FALSE;
                        reverse_mode = FALSE;
                        input_mode = FALSE;
                        subheader_mode = FALSE;
                        note_mode = FALSE;
                    }
                }
            } else if (!strcmp(word[0], "hyperlink")) {
                /* OUTPUT LINK TEXT AS PLAIN TEXT UNDER Glk */
                if (word[2] == NULL) {
                    noproprun();
                    /* exit_function closes infile/outfile + releases
                     * the read/write locks, then pops the stack. The
                     * old pop_stack() alone leaked the CSV file
                     * descriptor if any of these CGI opcodes was hit
                     * inside an iterate block with too few args. */
                    return (exit_function(TRUE));
                } else {
                    write_text(text_of_word(1));
                }
#else
/* HERE STARTS THE CGIJACL-ONLY FUNCTIONS */
            } else if (!strcmp(word[0], "option")) {
                /* USED TO ADD AN OPTION TO AN HTML LIST */
                if (word[1] == NULL) {
                    noproprun();
                    /* exit_function closes infile/outfile + releases
                     * the read/write locks, then pops the stack. The
                     * old pop_stack() alone leaked the CSV file
                     * descriptor if any of these CGI opcodes was hit
                     * inside an iterate block with too few args. */
                    return (exit_function(TRUE));
                } else {
                    index = value_of(word[1], 0);
                    if (word[2] != NULL) {
                        snprintf(option_buffer, sizeof option_buffer,
                                 "<option value=\"%d\">", index);
                    } else {
                        object_names(index, temp_buffer);
                        snprintf(option_buffer, sizeof option_buffer,
                                 "<option value=\"%s\">", temp_buffer);
                    }

                    write_text(option_buffer);
                    list_output(index, TRUE);
                    write_text(temp_buffer);

                }
            } else if (!strcmp(word[0], "getenv")) {
                struct string_type *resolved_setstring = NULL;

                if (word[2] == NULL) {
                    noproprun();
                    /* exit_function closes infile/outfile + releases
                     * the read/write locks, then pops the stack. The
                     * old pop_stack() alone leaked the CSV file
                     * descriptor if any of these CGI opcodes was hit
                     * inside an iterate block with too few args. */
                    return (exit_function(TRUE));
                } else {
                    // GET A POINTER TO THE STRING BEING MODIFIED
                    if ((resolved_setstring = string_resolve(word[1])) == NULL) {
                        unkstrrun(word[1]);
                        return (exit_function(TRUE));
                    }

                    // COPY THE VARIABLE OF THE CGI VARIABLE INTO THE SPECIFIED STRING VARIABLE
                    if (getenv(text_of_word(2)) != NULL) {
                        strncpy (resolved_setstring->value, getenv(text_of_word(2)), 1023);
                    } else {
                        strncpy (resolved_setstring->value, "", 1023);
                    }
                }
            } else if (!strcmp(word[0], "button")) {
                /* USED TO CREATE AN HTML BUTTON */
                if (word[1] == NULL) {
                    noproprun();
                    /* exit_function closes infile/outfile + releases
                     * the read/write locks, then pops the stack. The
                     * old pop_stack() alone leaked the CSV file
                     * descriptor if any of these CGI opcodes was hit
                     * inside an iterate block with too few args. */
                    return (exit_function(TRUE));
                } if (word[2] != NULL) {
                    snprintf(option_buffer, sizeof option_buffer,
                             "<input class=~button~ type=~image~ "
                             "src=~%s~ name=~verb~ value=~%s~>",
                             text_of_word(2), text_of_word(1));
                    write_text(option_buffer);
                } else {
                    snprintf(option_buffer, sizeof option_buffer,
                             "<input class=~button~ type=~submit~ "
                             "style=~width: 90px; margin: 5px;~ "
                             "name=~verb~ value=~%s~>",
                             text_of_word(1));
                    write_text(option_buffer);
                }
            } else if (!strcmp(word[0], "hidden")) {
                snprintf(temp_buffer, 1024,
                         "<INPUT TYPE=\"hidden\" NAME=\"user_id\" VALUE=\"%s\">",
                         user_id);
                write_text(temp_buffer);
            } else if (!strcmp(word[0], "control")) {
                /* USED TO CREATE A HYPERLINK THAT IS AN IMAGE */
                if (word[2] == NULL) {
                    noproprun();
                    /* exit_function closes infile/outfile + releases
                     * the read/write locks, then pops the stack. The
                     * old pop_stack() alone leaked the CSV file
                     * descriptor if any of these CGI opcodes was hit
                     * inside an iterate block with too few args. */
                    return (exit_function(TRUE));
                } else {
                    snprintf(option_buffer, sizeof option_buffer,
                             "<a href=\"?command=%s&amp;user_id=%s\">"
                             "<img border=0 SRC=\"%s\"></a>",
                             text_of_word(2), user_id, text_of_word(1));
                    write_text(option_buffer);
                }
            } else if (!strcmp(word[0], "hyperlink") || !strcmp(word[0], "hyperlinkNE")) {
                string_buffer[0] = 0;

                /* USED TO CREATE A HYPERLINK WITH SESSION INFORMATION INCLUDED */
                if (word[2] == NULL) {
                    noproprun();
                    /* exit_function closes infile/outfile + releases
                     * the read/write locks, then pops the stack. The
                     * old pop_stack() alone leaked the CSV file
                     * descriptor if any of these CGI opcodes was hit
                     * inside an iterate block with too few args. */
                    return (exit_function(TRUE));
                } else {
                    const char * encoded;

                    if (!strcmp(word[0], "hyperlink")) {
                        encoded = url_encode(text_of_word(2));
                    } else {
                        encoded = text_of_word(2);
                    }

                    {
                        struct cinteger_type *remote_user = REMOTE_USER_USED;
                        int use_user_id = (remote_user == NULL || remote_user->value == FALSE);

                        if (word[3] == NULL) {
                            if (use_user_id) {
                                snprintf(string_buffer, sizeof string_buffer,
                                         "<a href=\"?command=%s&amp;user_id=%s\">%s</a>",
                                         encoded, user_id, text_of_word(1));
                            } else {
                                snprintf(string_buffer, sizeof string_buffer,
                                         "<a href=\"?command=%s\">%s</a>",
                                         encoded, text_of_word(1));
                            }
                        } else {
                            if (use_user_id) {
                                snprintf(string_buffer, sizeof string_buffer,
                                         "<a class=\"%s\" href=\"?command=%s"
                                         "&amp;user_id=%s\">%s</a>",
                                         text_of_word(3), encoded, user_id,
                                         text_of_word(1));
                            } else {
                                snprintf(string_buffer, sizeof string_buffer,
                                         "<a class=\"%s\" href=\"?command=%s\">%s</a>",
                                         text_of_word(3), encoded, text_of_word(1));
                            }
                        }
                    }

                    if (!strcmp(word[0], "hyperlink")) {
                            free ((void *)encoded);
                    }

                    write_text(string_buffer);
                }
            } else if (!strcmp(word[0], "prompt")) {
                /* USED TO OUTPUT A HTML INPUT CONTROL THAT CONTAINS SESSION INFORMATION */
                if (word[1] != NULL) {
                    snprintf(temp_buffer, 1024,
                             "<input id=\"JACLCommandPrompt\" type=text "
                             "name=~command~ onKeyPress=~%s~>\n", word[1]);
                    write_text(temp_buffer);
                } else {
                    snprintf(temp_buffer, 1024,
                             "<input id=\"JACLCommandPrompt\" type=text "
                             "name=~command~>\n");
                    write_text(temp_buffer);
                }
                snprintf(temp_buffer, 1024,
                         "<input type=hidden name=\"user_id\" value=\"%s\">",
                         user_id);
                write_text(temp_buffer);
            } else if (!strcmp(word[0], "style")) {
                /* THIS COMMAND IS USED TO OUTPUT ANSI CODES OR SET GLK
                 * STREAM STYLES */
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else if (web_status_active()) {
                    /* The web status grid is monospace text only --
                     * skip the HTML tag emission so '<b>' etc. don't
                     * end up baked into the grid as literal characters.
                     * Push/pop are both skipped so the style stack
                     * stays balanced when the grid emit returns to
                     * normal text. */
                } else {
                    int sidx = stylehint_index_from_name(word[1]);
                    const char *open_tag = NULL;
                    int push_kind = 0;

                    if (sidx == BOLD)               { open_tag = "b";          push_kind = BOLD; }
                    else if (sidx == NOTE)          { open_tag = "i";          push_kind = NOTE; }
                    else if (sidx == INPUT)         { open_tag = "i";          push_kind = INPUT; }
                    else if (sidx == HEADER)        { open_tag = "h1";         push_kind = HEADER; }
                    else if (sidx == SUBHEADER)     { open_tag = "h2";         push_kind = SUBHEADER; }
                    else if (sidx == REVERSE)       { open_tag = "b";          push_kind = REVERSE; }
                    else if (sidx == PRE)           { open_tag = "pre";        push_kind = PRE; }
                    else if (sidx == ALERT)         { open_tag = "strong";     push_kind = ALERT; }
                    else if (sidx == QUOTE)         { open_tag = "blockquote"; push_kind = QUOTE; }

                    if (open_tag != NULL) {
                        stylehint_t *h = (sidx >= 0 && sidx < STYLEHINT_TABLE_SIZE)
                                         ? &jacl_stylehints[sidx] : NULL;
                        char tag_buffer[128];
                        if (h != NULL && (h->has_text_color || h->has_back_color)) {
                            char css[96];
                            css[0] = 0;
                            if (h->has_text_color) {
                                sprintf(css + strlen(css), "color: #%06lx;",
                                        h->text_color & 0xffffffUL);
                            }
                            if (h->has_back_color) {
                                sprintf(css + strlen(css), "background-color: #%06lx;",
                                        h->back_color & 0xffffffUL);
                            }
                            sprintf(tag_buffer, "<%s style=\"%s\">", open_tag, css);
                        } else {
                            sprintf(tag_buffer, "<%s>", open_tag);
                        }
                        write_text(tag_buffer);
                        style_stack[style_index++] = push_kind;
                    } else if (!strcmp(word[1], "normal")) {
                        style_index--;
                        for(; style_index > -1; style_index--) {
                            switch(style_stack[style_index]) {
                                case BOLD:
                                    write_text("</b>");
                                    break;
                                case NOTE:
                                    write_text("</i>");
                                    break;
                                case INPUT:
                                    write_text("</i>");
                                    break;
                                case HEADER:
                                    write_text("</h1>");
                                    break;
                                case SUBHEADER:
                                    write_text("</h2>");
                                    break;
                                case REVERSE:
                                    write_text("</b>");
                                    break;
                                case PRE:
                                    write_text("</pre>");
                                    break;
                                case ALERT:
                                    write_text("</strong>");
                                    break;
                                case QUOTE:
                                    write_text("</blockquote>");
                                    break;
                            }
                        }
                        style_index = 0;
                    } 
                }
            /* THESE FINAL COMMANDS HAVE NO EFFECT UNDER CGIJACL 
               AND THERE IS NO HARM IN IGNORING THEM */
            } else if (!strcmp(word[0], "flush")) {
            } else if (!strcmp(word[0], "image")) {
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    if (word[2] == NULL) {
                        sprintf(option_buffer, "<img src=~%s~>", text_of_word(1));
                    } else {
                        sprintf(option_buffer, "<img class=~%s~ src=~%s~>", text_of_word(2), text_of_word(1));
                    }

                    write_text(option_buffer);
                }
            } else if (!strcmp(word[0], "sound")) {
                /* Web sound forms:
                 *   sound URL mime [channel] [loop]   -- play (channel default 0)
                 *   sound 0 channel                   -- stop a channel
                 * The "stop" form is detected by word[1] being unquoted
                 * (i.e., a numeric/identifier expression, not a string
                 * literal) -- the play form requires a quoted URL. */
                if (word[1] == NULL || word[2] == NULL) {
                    noproprun();
                    return (exit_function(TRUE));
                }

                /* Honor sound_enabled / SOUND_ENABLED so a player who
                 * declines sound at intro (or types "sound off") gets
                 * silence. Skip the marker emission but stay in the
                 * function -- exit_function() here would prematurely
                 * terminate whatever JACL is currently running. */
                if (!SOUND_ENABLED->value) {
                    /* fall through, skipping the emission below */
                } else if (quoted[1] == 0) {
                    /* Stop form. */
                    int stop_channel = (int) value_of(word[2], TRUE);
                    sprintf(option_buffer,
                            "<jacl-sound-stop data-jacl-channel=~%d~></jacl-sound-stop>",
                            stop_channel);
                    write_text(option_buffer);
                } else {
                    /* Play form. Parse optional channel (integer arg) and
                     * loop (the literal word "loop", which resolves to -1
                     * via the constant defined in user games). */
                    int play_channel = 0;
                    int looping = 0;
                    int i;
                    for (i = 3; i < MAX_WORDS && word[i] != NULL; i++) {
                        if (quoted[i] == 0) {
                            int v = (int) value_of(word[i], TRUE);
                            if (v == -1) {
                                looping = 1;
                            } else {
                                play_channel = v;
                            }
                        }
                    }
                    if (looping) {
                        sprintf(option_buffer,
                                "<audio autoplay=~autoplay~ loop=~loop~ "
                                "data-jacl-channel=~%d~ data-jacl-pending=~1~>",
                                play_channel);
                    } else {
                        sprintf(option_buffer,
                                "<audio autoplay=~autoplay~ "
                                "data-jacl-channel=~%d~ data-jacl-pending=~1~>",
                                play_channel);
                    }
                    write_text(option_buffer);
                    sprintf(option_buffer, "<source src=~%s~ type=~%s~>",
                            text_of_word(1), text_of_word(2));
                    write_text(option_buffer);
                    write_text("</audio>");
                }
            } else if (!strcmp(word[0], "cursor")) {
            } else if (!strcmp(word[0], "timer")) {
                /* Emit a <jacl-timer data-ms="N"> marker the JS picks
                 * up to start (or stop, when N == 0) a setInterval
                 * that fires ?rpc=timer ajax requests. Each tick
                 * runs the game's +timer JACL function, just like
                 * Glk timer events on console. */
                if (word[1] != NULL) {
                    int interval_ms = (int) value_of(word[1], TRUE);
                    if (interval_ms < 0) interval_ms = 0;
                    sprintf(option_buffer,
                            "<jacl-timer data-ms=~%d~></jacl-timer>",
                            interval_ms);
                    write_text(option_buffer);
                    {
                        struct cinteger_type *et =
                            cinteger_resolve("event_timer");
                        if (et != NULL) et->value = interval_ms;
                    }
                }
            } else if (!strcmp(word[0], "volume")) {
                /* volume LEVEL channel -- emits a marker the web client
                 * applies to the audio element on that channel. LEVEL is
                 * 0-100; the JS divides by 100 to get the HTMLMediaElement
                 * volume range (0.0-1.0). When the player has disabled
                 * sound, silently skip the marker -- but stay in the
                 * function. The previous code did exit_function() which
                 * terminated whatever JACL was currently running, e.g.
                 * leaving +intro half-rendered when 'volume 65 channel'
                 * appeared in a scene block. */
                if (SOUND_ENABLED->value && word[2] != NULL) {
                    int vol_level = (int) value_of(word[1], TRUE);
                    int vol_channel = (int) value_of(word[2], TRUE);
                    sprintf(option_buffer,
                            "<jacl-sound-volume data-jacl-channel=~%d~ "
                            "data-jacl-level=~%d~></jacl-sound-volume>",
                            vol_channel, vol_level);
                    write_text(option_buffer);
                }
            } else if (!strcmp(word[0], "askstring") || !strcmp(word[0], "getstring")) {
                if (word[1] != NULL) {
                    struct integer_type *ptype = PENDING_QUESTION_TYPE;
                    struct string_type *ptarget = string_resolve("pending_target");
                    if (ptype != NULL && ptarget != NULL) {
                        if (!strcmp(word[0], "getstring")) {
                            ptype->value = 4;
                        } else {
                            ptype->value = 5;
                        }
                        strncpy(ptarget->value, word[1], 1023);
                        ptarget->value[1023] = 0;
                        return(exit_function(TRUE));
                    }
                }
            } else if (!strcmp(word[0], "asknumber") || !strcmp(word[0], "getnumber")) {
                if (word[3] != NULL) {
                    struct integer_type *ptype = PENDING_QUESTION_TYPE;
                    struct integer_type *plow = PENDING_NUMBER_LOW;
                    struct integer_type *phigh = PENDING_NUMBER_HIGH;
                    struct string_type *ptarget = string_resolve("pending_target");
                    if (ptype != NULL && plow != NULL && phigh != NULL && ptarget != NULL) {
                        if (!strcmp(word[0], "getnumber")) {
                            ptype->value = 2;
                        } else {
                            ptype->value = 3;
                        }
                        plow->value = value_of(word[2], TRUE);
                        phigh->value = value_of(word[3], TRUE);
                        strncpy(ptarget->value, var_text_of_word(1), 1023);
                        ptarget->value[1023] = 0;
                        return(exit_function(TRUE));
                    }
                }
            } else if (!strcmp(word[0], "getyesorno")) {
                if (word[1] != NULL) {
                    struct integer_type *ptype = PENDING_QUESTION_TYPE;
                    struct string_type *ptarget = string_resolve("pending_target");
                    if (ptype != NULL && ptarget != NULL) {
                        ptype->value = 1;
                        strncpy(ptarget->value, var_text_of_word(1), 1023);
                        ptarget->value[1023] = 0;
                        return(exit_function(TRUE));
                    }
                }
            } else if (!strcmp(word[0], "clear")) {
                /* Emit a <jacl-clear> marker that the web frontend uses
                 * to wipe everything in #maintext before the marker --
                 * matching the Glk glk_window_clear behaviour above. */
                write_text("<jacl-clear></jacl-clear>");
            } else if (!strcmp(word[0], "more")) {
            } else if (!strcmp(word[0], "terminate")) {
#endif
#endif
            } else if (!strcmp(word[0], "proxy")) {
                /* THE PROXY COMMAND ISSUES A MOVE ON THE PLAYER'S BEHALF
                 * ALL STATE MUST BE SAVED SO THE CURRENT MOVE CAN CONTINUE
                 * ONCE THE PROXIED MOVE IS COMPLETE */
#ifdef GLK
                if (push_stack(glk_stream_get_position(game_stream)) == FALSE) {
                    return (exit_function(TRUE));
                }
#else
                if (push_stack(ftell(file)) == FALSE) {
                    return (exit_function(TRUE));
                }
#endif
                if (push_proxy() == FALSE) {
                    /* push_stack succeeded but push_proxy didn't --
                     * pop the stack we just pushed before bailing
                     * so the caller's frame isn't leaked. */
                    pop_stack();
                    return (exit_function(TRUE));
                }

                build_proxy();

                // TEXT BUFFER IS THE NORMAL ARRAY FOR HOLDING THE PLAYERS
                // MOVE FOR PROCESSING. proxy_buffer is also 1024; strncpy
                // would skip the NUL if both ran to the wire, so force
                // termination after the copy.
                strncpy(text_buffer, proxy_buffer, 1023);
                text_buffer[1023] = 0;

                command_encapsulate();

                jacl_truncate();

                preparse();
                
                pop_proxy();

                pop_stack();
            } else if (!strcmp(word[0], "override")) {
                /* TELLS THE INTERPRETER TO LOOK FOR AN _override FUNCTION 
                 * TO EXECUTE IN PLACE OF ANY CODE THAT FOLLOWS THIS LINE. 
                 * THIS COMMAND IS USED EXCLUSIVELY IN GLOBAL FUNCTIONS 
                 * ASSOCIATED WITH GRAMMAR LINES */
                if (execute(override) == TRUE) {
                    return(exit_function (TRUE));
                } else {
                    if (execute(default_function) == TRUE) {
                        return(exit_function (TRUE));
                    }
                }
            } else if (!strcmp(word[0], "execute") || !strcmp(word[0], "call")) {
                /* CALLS ANOTHER JACL FUNCTION */
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else {
                    /* RESOLVE ALL THE TEXT AND STORE IT IN A TEMPORARY BUFFER*/
                    string_buffer[0] = 0;

                    for (counter = 1; word[counter] != NULL && counter < MAX_WORDS; counter++) {
                        strcat(string_buffer, arg_text_of_word(counter));
                    }

                    if (function_resolve(string_buffer) == NULL && !strcmp(word[0], "execute")) {
                        char * argstart;

                        /* REMOVE ANY PARAMETERS FROM FUNCTION NAME
                           BEFORE DISPLAYING ERROR MESSAGE */
                        argstart = strchr(string_buffer, '<');
                        if (argstart != NULL)
                            *argstart = 0;
                        
                        sprintf(error_buffer, UNDEFINED_FUNCTION, executing_function->name, string_buffer);
                        log_error(error_buffer, PLUS_STDOUT);
                    } else {
                        execute(string_buffer);
                    }
                }
            } else if (!strcmp(word[0], "points")) {
                /* INCREASE THE PLAYER'S SCORE AND POTENTIALLY INFORM THEM OF THE INCREASE */
                if (word[1] != NULL) {
                    SCORE->value += value_of(word[1], TRUE);
                    if (NOTIFY->value) {
#ifdef GLK
                        glk_set_style(style_Note);
#else
#ifdef __NDS__
                        printf("\x1b[34;1m");     // SET TO BRIGHT BLUE
                        note_mode = TRUE;
#else
                        write_text("<b><i>");
#endif
#endif
                        write_text(cstring_resolve("SCORE_UP")->value);
                        sprintf(temp_buffer, "%ld", value_of(word[1], TRUE));
                        write_text(temp_buffer);
                        if (value_of(word[1], TRUE) == 1) {
                            write_text(cstring_resolve("POINT")->value);
                        } else {
                            write_text(cstring_resolve("POINTS")->value);
                        }
#ifdef GLK
                        glk_set_style(style_Normal);
#else
#ifdef __NDS__
                        printf("\x1b[37;0m");     // SET TO DIM WHITE
                        note_mode = FALSE;
#else
                        write_text("</i></b>");
#endif
#endif
                    }
                } else {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                }
            } else if (!strcmp(word[0], "output")) {
                if (word[1] != NULL) {
                    strcpy (temp_buffer, data_directory);
                    strcat (temp_buffer, text_of_word(1));
                    tempfile = fopen(temp_buffer, "r");

                    if (tempfile == NULL) {
                            snprintf(error_buffer, 1024, "Failed to open file %s to output: %s\n", temp_buffer, strerror(errno));
                            log_error(error_buffer, PLUS_STDOUT);
                    } else {
                        /* Drive the loop off the read return, not feof,
                         * so the last line isn't reprocessed once on
                         * EOF. The prior form printed text_buffer one
                         * extra time when fgets returned NULL but the
                         * feof check was still false (read had failed
                         * but EOF not yet flagged). */
                        while (fgets(temp_buffer, 1024, tempfile) != NULL) {
                            write_text(temp_buffer);
                        }

                        fclose(tempfile);
                        tempfile = NULL;
                    }
                } else {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                }
            } else if (!strcmp(word[0], "print")) {
                int non_space = FALSE;

                // DISPLAYS A BLOCK OF PLAIN TEXT UNTIL IT FINDS A 
                // LINE THAT STARTS WITH A '.'
#ifdef GLK
                glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);
#else
                fgets(text_buffer, 1024, file);
#endif

                if (encrypted) jacl_deobfuscate(text_buffer);

                while (text_buffer[0] != '.') {
                    index = 0;
                    non_space = FALSE;

                    /* REMOVE ANY NEWLINE CHARACTERS */
                    while (text_buffer[index] != 0) {
                        if (text_buffer[index] == '|' && non_space == FALSE) {
                            /* THE BAR CHARACTER IS CHANGED TO A SPACE TO 
                             * ALLOW INDENTING OF NEW PARAGRAPHS ETC */
                            text_buffer[index] = ' ';
                        } else if (text_buffer[index] == '\r') {
                            // GOT TO THE END OF THE LINE, STRIP THE \r AND BREAK
                            text_buffer[index] = 0;
                            break;
                        } else if (text_buffer[index] == '\n') {
                            // GOT TO THE END OF THE LINE, STRIP THE \n AND BREAK
                            text_buffer[index] = 0;
                            break;
                        } else if (text_buffer[index] != ' ' && text_buffer[index] != '\t') {
                            // THE FIRST CHARACTER THAT IS NOT A SPACE OR A TAB HAS BEEN REACHED
                            non_space = TRUE;
                        }

                        index++;
                    }

                    if (text_buffer[0] != 0) {
                        // CHECK IF THERE IS THE NEED TO ADD AN 
                        // IMPLICIT SPACE
                        index = strlen(text_buffer);
    
                        if (text_buffer[index - 1] == '\\') {    
                            // A BACKSLASH IS USED TO INDICATE AN IMPLICIT 
                            // SPACE SHOULD NOT BE PRINTED
                            text_buffer[index - 1] = 0;
                        } else if (text_buffer[index - 1] != '^') {
                            // ADD AN IMPLICIT SPACE IF THE PREVIOUS LINE
                            // DIDN'T END WITH A CARRIAGE RETURN
                            strcat(text_buffer, " ");
                        }
    
                        // OUTPUT THE LINE READ AS PLAIN TEXT
                        write_text(text_buffer);
                    }

                    // GET THE NEXT LINE
#ifdef GLK
                    glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);
#else
                    fgets(text_buffer, 1024, file);
#endif

                    if (encrypted) jacl_deobfuscate(text_buffer);
                }
            } else if (!strcmp(word[0], "mesg")) {
                for (counter = 1; word[counter] != NULL && counter < MAX_WORDS; counter++) {
                    fprintf(stderr, "%s", text_of_word(counter));
                }
            } else if (!strcmp(word[0], "error")) {
                write_text("ERROR: In function ~");
                write_text(executing_function->name);
                write_text("~, ");
                for (counter = 1; word[counter] != NULL && counter < MAX_WORDS; counter++) {
                    write_text(text_of_word(counter));
                }
            } else if (!strcmp(word[0], "debug") && DEBUG->value) {
                write_text("DEBUG: ");
                for (counter = 1; word[counter] != NULL && counter < MAX_WORDS; counter++) {
                    write_text(text_of_word(counter));
                }
            } else if (!strcmp(word[0], "write")) {
                for (counter = 1; word[counter] != NULL && counter < MAX_WORDS; counter++) {
                    output = text_of_word(counter);            
                    if (*output != 0) {
                        // IF THE OUTPUT ISN'T AN EMPTY STRING, DISPLAY IT
                        write_text(output);
                    }
                }
            } else if (!strcmp(word[0], "length")) {
                if (word[2] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return(exit_function (TRUE));
                } else {
                    if ((container = container_resolve(word[1])) == NULL) {
                        unkvarrun(word[1]);
                        return(exit_function (TRUE));
                    }
                    
                    *container = strlen(text_of(word[2]));    
                }
            } else if (!strcmp(word[0], "savegame")) {
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    if ((container = container_resolve(word[1])) == NULL) {
                        unkvarrun(word[1]);
                        return (exit_function(TRUE));
                    } else {
                        if (word[2] == NULL) {
                            *container = save_interaction(NULL);
                        } else {
                            *container = save_interaction(arg_text_of_word(2));
                        }
                    }
                } 
            } else if (!strcmp(word[0], "restoregame")) {
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    if ((container = container_resolve(word[1])) == NULL) {
                        unkvarrun(word[1]);
                        return (exit_function(TRUE));
                    } else {
                        if (word[2] == NULL) {
                            *container = restore_interaction(NULL);
                        } else {
                            *container = restore_interaction(arg_text_of_word(2));
                        }
                    }
                } 
            } else if (!strcmp(word[0], "restartgame")) {
                restart_game();
                execute ("+intro");
                eachturn();
#ifdef GLK
            } else if (!strcmp(word[0], "undomove")) {
                undoing();
            } else if (!strcmp(word[0], "updatestatus")) {
                status_line();
#else
            } else if (!strcmp(word[0], "undomove")) {
            } else if (!strcmp(word[0], "updatestatus")) {
                /* Web build: render the status bar through a virtual
                 * character grid so the same cursor X Y + write idiom
                 * works as on a GLK TextGrid. The body lives in
                 * web_render_status_bar() below; it is also invoked
                 * from utils.c's eachturn() (runtime safety net for
                 * games with older library copies) and from cgijacl.c
                 * on rpc=resize / end-of-ajax. */
                web_render_status_bar();
#endif
            } else if (!strcmp(word[0], "split")) {

                // 0     1       2      3         4
                // split counter source delimiter destination

                int *split_container;
                char split_buffer[1024] = "";
                char container_buffer[1024] = "";
                char delimiter[1024] = ""; 
                char *match = NULL;
                struct string_type *resolved_splitstring = NULL;

                /* text_of_word can resolve to a 1024-byte cstring;
                 * strcpy into a 1024-byte buffer would skip the NUL
                 * terminator at the boundary. Use strncpy + explicit
                 * NUL to keep both buffers safely terminated. */
                strncpy(split_buffer, text_of_word(2), sizeof split_buffer - 1);
                split_buffer[sizeof split_buffer - 1] = 0;
                strncpy(delimiter, text_of_word(3), sizeof delimiter - 1);
                delimiter[sizeof delimiter - 1] = 0;

                char *source = split_buffer;

                if (word[4] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    split_container = container_resolve(var_text_of_word(1));

                    if (split_container == NULL) {
                        unkvarrun(var_text_of_word(1));
                        return (exit_function(TRUE));
                    } else {
                        *split_container = 0;

                        // THERE IS ALWAYS ONE MATCH, EVEN IF NO DELIMETERS ARE FOUND
                        while ((match = strstr(source, delimiter))) {
                            *match = 0;
                            strcpy(container_buffer, var_text_of_word(4));
                            strcat(container_buffer, "[");
                            sprintf(integer_buffer, "%d", *split_container);
                            strcat(container_buffer, integer_buffer);
                            strcat(container_buffer, "]");

                            if ((resolved_splitstring = string_resolve(container_buffer)) == NULL) {
                                unkstrrun(var_text_of_word(4));
                                return (exit_function(TRUE));
                            } else {
                                strcpy(resolved_splitstring->value, source);
                                source = match + strlen(delimiter);
                                (*split_container)++;
                            }
                        }
                        strcpy(container_buffer, var_text_of_word(4));
                        strcat(container_buffer, "[");
                        sprintf(integer_buffer, "%d", *split_container);
                        strcat(container_buffer, integer_buffer);
                        strcat(container_buffer, "]");

                        if ((resolved_splitstring = string_resolve(container_buffer)) == NULL) {
                            unkstrrun(word[1]);
                            return (exit_function(TRUE));
                        } else {
                            strcpy(resolved_splitstring->value, source);
                            (*split_container)++;
                        }
                    }
                }
            } else if (!strcmp(word[0], "setstring") ||
                        !strcmp(word[0], "addstring")) {
                char setstring_buffer[2048] = "";
                struct string_type *resolved_setstring = NULL;

                if (word[2] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    /* GET A POINTER TO THE STRING BEING MODIFIED */
                    if ((resolved_setstring = string_resolve(var_text_of_word(1))) == NULL) {
                        unkstrrun(word[1]);
                        return (exit_function(TRUE));
                    }

                    /* RESOLVE ALL THE TEXT AND STORE IT IN A TEMPORARY BUFFER*/
                    for (counter = 2; word[counter] != NULL && counter < MAX_WORDS; counter++) {
                        strcat(setstring_buffer, text_of_word(counter));
                    }

                    /* setstring_buffer IS NOW FILLED, COPY THE UP TO 1023 BYTES OF
                     * IT INTO THE STRING */
                    if (!strcmp(word[0], "setstring")) {
                        strncpy (resolved_setstring->value, setstring_buffer, 1023);
                        resolved_setstring->value[1023] = 0;
                    } else {
                        /* CALCULATE HOW MUCH SPACE IS LEFT IN THE STRING */
                        counter = 1023 - strlen(resolved_setstring->value);
                        /* THIS IS A addstring COMMAND, SO USE STRNCAT INSTEAD */
                        strncat (resolved_setstring->value, setstring_buffer, counter);
                    }
                }
            } else if (!strcmp(word[0], "escapestring")) {
                /* HTML-escape concatenated source tokens into a
                 * destination string. Used by forms.library (and any
                 * other code reflecting untrusted input) to prevent
                 * a tilde-or-tag in user input from breaking out of
                 * an HTML attribute. cgijacl's output filter swaps
                 * `~` -> `"` on the way out, so a raw tilde in the
                 * source is the dangerous case here. */
                char setstring_buffer[2048] = "";
                struct string_type *resolved_setstring = NULL;

                if (word[2] == NULL) {
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    if ((resolved_setstring = string_resolve(var_text_of_word(1))) == NULL) {
                        unkstrrun(word[1]);
                        return (exit_function(TRUE));
                    }

                    for (counter = 2; word[counter] != NULL && counter < MAX_WORDS; counter++) {
                        strncat(setstring_buffer, text_of_word(counter),
                                sizeof(setstring_buffer) - strlen(setstring_buffer) - 1);
                    }

                    /* Write into the destination, escaping as we go.
                     * resolved_setstring->value is 1024 bytes; reserve
                     * one for NUL. If the next entity wouldn't fit, we
                     * stop before writing a partial entity. */
                    char       *dst       = resolved_setstring->value;
                    const char *dst_end   = resolved_setstring->value + 1023;
                    const char *src       = setstring_buffer;
                    while (*src != 0) {
                        const char *entity = NULL;
                        switch (*src) {
                            case '&':  entity = "&amp;";  break;
                            case '<':  entity = "&lt;";   break;
                            case '>':  entity = "&gt;";   break;
                            case '"':  entity = "&quot;"; break;
                            case '\'': entity = "&#39;";  break;
                            /* JACL uses `~` inside string literals as a
                             * stand-in for `"`, and cgijacl's output
                             * filter (cgijacl.c:1726, :1770) swaps
                             * tildes back to quotes on the way out.
                             * A user-supplied `~` would therefore
                             * close an attribute delimiter; emit a
                             * literal &quot; entity to short-circuit
                             * the swap. */
                            case '~':  entity = "&quot;"; break;
                            default:   break;
                        }
                        if (entity != NULL) {
                            size_t elen = strlen(entity);
                            if (dst + elen > dst_end) break;
                            memcpy(dst, entity, elen);
                            dst += elen;
                        } else {
                            if (dst + 1 > dst_end) break;
                            *dst++ = *src;
                        }
                        src++;
                    }
                    *dst = 0;
                }
            } else if (!strcmp(word[0], "padstring")) {
                char setstring_buffer[2048] = "";
                struct string_type *resolved_setstring = NULL;
                string_buffer[0] = 0;
    
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    /* GET A POINTER TO THE STRING BEING MODIFIED */
                    if ((resolved_setstring = string_resolve(word[1])) == NULL) {
                        unkstrrun(word[1]);
                        return (exit_function(TRUE));
                    }

                    index = value_of(word[3], TRUE);

                    {
                        const char *pad   = text_of_word(2);
                        size_t      plen  = (pad != NULL) ? strlen(pad) : 0;
                        size_t      used  = 0;
                        size_t      max   = sizeof setstring_buffer;
                        if (plen > 0) {
                            for (counter = 0; counter < index; counter++) {
                                if (used + plen + 1 > max) break;
                                memcpy(setstring_buffer + used, pad, plen);
                                used += plen;
                            }
                            setstring_buffer[used] = 0;
                        }
                    }

                    /* setstring_buffer IS NOW FILLED, COPY THE UP TO 1023 BYTES OF
                     * IT INTO THE STRING */
                    strncpy (resolved_setstring->value, setstring_buffer, 1023);
                    resolved_setstring->value[1023] = 0;
                }
            } else if (!strcmp(word[0], "return")) {
                /* RETURN FROM THIS FUNCTION, POSSIBLY RETURNING AN INTEGER VALUE */
                if (word[1] == NULL) {
                    return (exit_function(TRUE));
                } else {
                    index = value_of(word[1], TRUE);
                    return (exit_function(index));
                }
            } else if (!strcmp(word[0], "position")) {
                /* MOVE AN OBJECT TO ITS NEW X,Y COORDINATES BASED ON ITS CURRENT VALUES
                 * FOR x, y, bearing, velocity */
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    object_1 = value_of(word[1], TRUE);

                    if (object_1 < 1 || object_1 > objects) {
                        badptrrun(word[1], object_1);
                        return (exit_function(TRUE));
                    } else {
                        new_position((double) object[object_1]->X,
                                     (double) object[object_1]->Y,
                                     (double) object[object_1]->BEARING,
                                     (double) object[object_1]->VELOCITY);

                        object[object_1]->X = new_x;
                        object[object_1]->Y = new_y;
                    }
                }
            } else if (!strcmp(word[0], "bearing")) {
                /* CALCULATE THE BEARING BETWEEN TWO OBJECTS */
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    if ((container = container_resolve(word[1])) == NULL) {
                        unkvarrun(word[1]);
                        return (exit_function(TRUE));
                    }

                    object_1 = value_of(word[2], TRUE);

                    if (object_1 < 1 || object_1 > objects) {
                        badptrrun(word[2], object_1);
                        return (exit_function(TRUE));
                    } else {
                        object_2 = value_of(word[3], TRUE);

                        if (object_2 < 1 || object_2 > objects) {
                            badptrrun(word[3], object_2);
                            return (exit_function(TRUE));
                        } else {
                            if (container != NULL
                                && object_1 != FALSE
                                && object_2 != FALSE) {
                                *container = bearing((double) object[object_1]->X,
                                                     (double) object[object_1]->Y,
                                                     (double) object[object_2]->X,
                                                     (double) object[object_2]->Y);
                            }
                        }
                    }
                }
            } else if (!strcmp(word[0], "distance")) {
                /* CALCULATE THE DISTANCE BETWEEN TWO OBJECTS */
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    container = container_resolve(word[1]);

                    object_1 = value_of(word[2], TRUE);

                    if (object_1 < 1 || object_1 > objects) {
                        badptrrun(word[2], object_1);
                        return (exit_function(TRUE));
                    } else {
                        object_2 = value_of(word[3], TRUE);

                        if (object_2 < 1 || object_2 > objects) {
                            badptrrun(word[3], object_2);
                            return (exit_function(TRUE));
                        } else {
                            if (container != NULL
                                && object_1 != FALSE
                                && object_2 != FALSE) {
                                *container = distance((double)
                                                      object[object_1]->X,
                                                      (double)
                                                      object[object_1]->Y,
                                                      (double)
                                                      object[object_2]->X,
                                                      (double)
                                                      object[object_2]->Y);
                            }
                        }
                    }
                }
            } else if (!strcmp(word[0], "travel")) {
				// THIS IS COMMAND IS HERE FOR BACKWARDS COMPATIBILITY
                // ALL TRAVEL RELATED CODE IS NOW IN VERBS.LIBRARY
				if (word[1] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
                    index = value_of(word[1], TRUE);

                    // NOT A VALID DIRECTION
					if (index < 0 || index > 11) index = -1;

					if (index != -1) {                      
                        integer_resolve("destination")->value = object[HERE]->integer[index];
                        if (execute("+movement")) break;
                        if (execute("here.movement")) break;

                        if (integer_resolve("destination")->value == 0) {
                            write_text("You can't go that way.^");\
                            TIME->value = FALSE;
                        } else {
                            object[player]->PARENT = integer_resolve("destination")->value;
                            object[HERE]->attributes &= ~1L;
                            look_around();
                            TIME->value = FALSE;
                        }
					} else {
						unkdirrun(1);
						return (exit_function(TRUE));
					}
				}             
            } else if (!strcmp(word[0], "dir_to") ||
                       !strcmp(word[0], "npc_to")) {
                /* CALCULATE THE FIRST DIRECTION TO TRAVEL IN GET TO 
                 * A SPECIFIED LOCATION */
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    container = container_resolve(word[1]);

                    object_1 = value_of(word[2], TRUE);

                    if (object_1 < 1 || object_1 > objects) {
                        badptrrun(word[2], object_1);
                        return (exit_function(TRUE));
                    } else {
                        object_2 = value_of(word[3], TRUE);

                        if (object_2 < 1 || object_2 > objects) {
                            badptrrun(word[3], object_2);
                            return (exit_function(TRUE));
                        } else {
                            if (container != NULL
                                && object_1 != FALSE
                                && object_2 != FALSE) {
                                if (!strcmp(word[0], "dir_to")) {
                                    *container = find_route(object_1, object_2, TRUE);
                                } else {
                                    *container = find_route(object_1, object_2, FALSE);
                                }
                            }
                        }
                    }
                }
            } else if (!strcmp(word[0], "set")) {
                /* SET THE VALUE OF AN ELEMENT TO A SUPPLIED INTEGER */
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    container = container_resolve(var_text_of_word(1));

                    if (container == NULL) {
                        unkvarrun(word[1]);
                        return (exit_function(TRUE));
                    } else {
                        int mark = 2; // SET mark TO POINT TO THE FIRST OPERATOR
                        while (word[mark + 1] != NULL) {
                            counter = value_of(word[mark + 1], TRUE);
    
                            if (word[mark][0] == '+')
                                *container += counter;
                            else if (word[mark][0] == '-')
                                *container -= counter;
                            else if (word[mark][0] == '*')
                                *container = *container * counter;
                            else if (word[mark][0] == '%')
                                *container = *container % counter;
                            else if (word[mark][0] == '/') {
                                if (counter == 0) {
                                    sprintf(error_buffer, DIVIDE_BY_ZERO,
                                            executing_function->name);
                                    log_error(error_buffer, PLUS_STDOUT);
                                } else
                                    *container = *container / counter;
                            } else if (!strcmp(word[mark], "locationof")) {
                                /* grand_of dereferences object[counter];
                                 * validate before calling so a stale
                                 * pointer (-1, 0, or > objects) doesn't
                                 * read out-of-bounds. */
                                if (counter < 1 || counter > objects) {
                                    badptrrun(word[mark + 1], counter);
                                } else {
                                    *container = grand_of(counter, FALSE);
                                }
                            } else if (!strcmp(word[mark], "grandof")) {
                                if (counter < 1 || counter > objects) {
                                    badptrrun(word[mark + 1], counter);
                                } else {
                                    *container = grand_of(counter, TRUE);
                                }
                            } else if (word[mark][0] == '=') {
                                *container = counter; 
                            } else {
                                sprintf(error_buffer, ILLEGAL_OPERATOR,
                                        executing_function->name,
                                        word[2]);
                                log_error(error_buffer, PLUS_STDOUT);
                            }

                            mark += 2;
                        }
                    }
                }
            } else if (!strcmp(word[0], "ensure")) {
                /* USED TO GIVE OR TAKE AN ATTRIBUTE TO OR FROM AND OBJECT */
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    if ((bit_mask = attribute_resolve(arg_text_of(word[3])))) {
                        index = value_of(word[1], TRUE);
                        if (index < 1 || index > objects) {
                            badptrrun(word[1], index);
                            return (exit_function(TRUE));
                        } else {
                            if (!strcmp(word[2], "has")) {
                                object[index]->attributes =
                                    object[index]->attributes | bit_mask;
                            } else if (!strcmp(word[2], "hasnt")) {
                                bit_mask = ~bit_mask;
                                object[index]->attributes =
                                    object[index]->attributes & bit_mask;
                            }
                        }
                    } else if ((bit_mask = user_attribute_resolve(arg_text_of(word[3])))) {
                        index = value_of(word[1], TRUE);
                        if (index < 1 || index > objects) {
                            badptrrun(word[1], index);
                            return (exit_function(TRUE));
                        } else {
                            if (!strcmp(word[2], "has")) {
                                object[index]->user_attributes =
                                    object[index]->user_attributes | bit_mask;
                            } else if (!strcmp(word[2], "hasnt")) {
                                bit_mask = ~bit_mask;
                                object[index]->user_attributes =
                                    object[index]->user_attributes & bit_mask;
                            }
                        }
                    } else {
                        unkattrun(3);
                        return (exit_function(TRUE));
                    }
                }
            } else if (!strcmp(word[0], "append") 
                || !strcmp(word[0], "append_nt")
                || !strcmp(word[0], "append_fc")
                || !strcmp(word[0], "append_lc")) {

                if (!strcmp(word[0], "append") || !strcmp(word[0], "append_fc")) {
                    // INITIATE THE WRITING OF A NEW LINE
                    first_column = TRUE;

                    if (outfile != NULL) {
                        fclose(outfile);
                        outfile = NULL;
                    }
                } 

                if (word[2] == NULL) {
                    // NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    if (outfile == NULL) {
                        strcpy (temp_buffer, data_directory);
                        strcat (temp_buffer, prefix);
                        strcat (temp_buffer, "-");
                        strcat (temp_buffer, text_of_word(1));
                        strcat (temp_buffer, ".csv");

                        outfile = fopen(temp_buffer, "ab");
                    }

                    if (outfile == NULL) {
                        snprintf(error_buffer, 1024, "Failed to open file %s: %s\n", temp_buffer, strerror(errno));
                        log_error(error_buffer, PLUS_STDOUT);
                      } else {
                        for (counter = 2; word[counter] != NULL && counter < MAX_WORDS; counter++) {
                            output = text_of_word(counter);            
                            if (*output != 0) {
                                if (first_column == FALSE) {
                                      fputc(',', outfile);
                                }
                                  csv_fwrite(outfile, output, (size_t) strlen(output));
                                first_column = FALSE;
                            }
                        }
    
                        if (!strcmp(word[0], "append") || !strcmp(word[0], "append_lc")) {
                            // TERMINATE THE LINE
                            fputc('\n', outfile);

                            // FLUSH AND CLOSE THE FILE
                            fflush(outfile);
                            fclose(outfile);
                            outfile = NULL;
                        }
                        
                    }

                }
            } else if (!strcmp(word[0], "insert")) {
                int first = TRUE;

                if (word[1] == NULL) {
                    // NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    if (outfile == NULL) {
                        log_error("Insert statement not inside an 'update' loop.", PLUS_STDOUT);
                      } else {
                        for (counter = 1; word[counter] != NULL && counter < MAX_WORDS; counter++) {
                            output = text_of_word(counter);            
                            if (*output != 0) {
                                if (first == FALSE) {
                                      fputc(',',(FILE *)outfile);
                                }
                                  csv_fwrite((FILE *)outfile, output, (size_t) strlen(output));
                                first = FALSE;
                            }
                        }
    
                        // TERMINATE THE LINE
                        fputc('\n', (FILE *)outfile);
                    }
                }
            } else if (!strcmp(word[0], "endgame")) {
                    terminate(0);              
            } else if (!strcmp(word[0], "inspect")) {
                if (word[1] == NULL) {
                    // NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    inspect(value_of(word[1], TRUE));
                }
            } else if (!strcmp(word[0], "move")) {
                /* THIS COMMAND IS USED TO MOVE AN OBJECT TO HAVE ANOTHER PARENT
                 * INCLUDING MODIFYING ALL QUANTITY VALUES BASED ON THE OBJECTS MASS */
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                }

                index = value_of(word[1], TRUE);
                if (index < 1 || index > objects) {
                    badptrrun(word[1], index);
                    return (exit_function(TRUE));
                } else {
                    object_2 = object[index]->PARENT;
                    if (object_2 && !(object[object_2]->attributes & LOCATION)) {
                        object[object_2]->QUANTITY += object[index]->MASS;
                    }
                    object_1 = value_of(word[3], TRUE);
                    if (object_1 < 1 || object_1 > objects) {
                        badptrrun(word[3], object_1);
                        return (exit_function(TRUE));
                    } else {
                        object[index]->PARENT = object_1;
                        if (!(object[object_1]->attributes & LOCATION))
                            object[object_1]->QUANTITY -= object[index]->MASS;
                    } 
                }
            } else if (!strcmp(word[0], "ifstringall")) {
                /* CHECK IF A STRING EQUALS OR CONTAINS ANOTHER STRING */
                current_level++;
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else if (and_strcondition()) {
                    execution_level++;
                }
            } else if (!strcmp(word[0], "ifstring")) {
                /* CHECK IF A STRING EQUALS OR CONTAINS ANOTHER STRING */
                current_level++;
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else if (strcondition()) {
                    execution_level++;
                }
            } else if (!strcmp(word[0], "ifexecute")) {
                current_level++;
                if (word[1] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else {
                    /* RESOLVE ALL THE TEXT AND STORE IT IN A TEMPORARY BUFFER*/
                    string_buffer[0] = 0;

                    for (counter = 1; word[counter] != NULL && counter < MAX_WORDS; counter++) {
                        strcat(string_buffer, arg_text_of_word(counter));
                    }

                    if (execute(string_buffer)) {
                        execution_level++;
                    }
                }
            } else if (!strcmp(word[0], "if")) {
                current_level++;
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else if (condition()) {
                    execution_level++;
                }
            } else if (!strcmp(word[0], "ifall")) {
                current_level++;
                if (word[3] == NULL) {
                    /* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
                    noproprun();
                    return (exit_function(TRUE));
                } else if (and_condition()) {
                    execution_level++;
                }
            } else {
                sprintf(error_buffer, UNKNOWN_COMMAND,
                        executing_function->name, word[0]);
                log_error(error_buffer, PLUS_STDOUT);
            }
        } else if (!strcmp(word[wp], "if") 
                || !strcmp(word[wp], "ifall")
                || !strcmp(word[wp], "ifstring")
                || !strcmp(word[wp], "ifstringall")
                || !strcmp(word[wp], "ifexecute")
                || !strcmp(word[wp], "iterate")
                || !strcmp(word[wp], "update")
                || !strcmp(word[wp], "while")
                || !strcmp(word[wp], "whileall")) {
            current_level++;
        }

#ifdef GLK
        before_command = glk_stream_get_position(game_stream);
        glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);
#else
        before_command = ftell(file);
        fgets(text_buffer, 1024, file);
#endif
        if (encrypted) jacl_deobfuscate(text_buffer);
    };

    return (exit_function(TRUE));
}

int
exit_function(int return_code)
{
    if (infile != NULL) {
        read_lck.l_type = F_UNLCK;    // SETTING A READ LOCK
          fcntl(read_fd, F_SETLK, &read_lck);
        fclose(infile);
        infile = NULL;
    }

    if (outfile != NULL) {
        write_lck.l_type = F_UNLCK;    // SETTING A WRITE LOCK
          fcntl(write_fd, F_SETLK, &write_lck);
        fclose(outfile);
        outfile = NULL;
    }
    
    /* POP THE STACK REGARDLESS OF THE RETURN CODE */
    pop_stack();

    return (return_code);
}

char *
object_names(int object_index, char *names_buffer)
{
    /* THIS FUNCTION CREATES A LIST OF ALL AN OBJECT'S NAMES.
       THE escape ARGUMENT INDICATES WHETHER A + SIGN SHOULD BE
       USED IN PLACE OF A SPACE BETWEEN EACH OF THE NAMES */
    struct name_type *current_name = object[object_index]->first_name;
    names_buffer[0] = 0;

    while (current_name != NULL) {
        strcat(names_buffer, " ");
        strcat(names_buffer, current_name->name);
        current_name = current_name->next_name;
    }

    return names_buffer;
}

int
distance(double x1, double y1, double x2, double y2)
{
    /* THIS FUNCTION CALCULATES THE DISTANCE BETWEEN TWO POINTS IN A 
       TWO-DIMENSIONAL PLANE */
    double          delta_x,
                    delta_y;
    double          distance,
                    total;

    /*
     * Object two in which quadrant compared to object one? 0 x = opp, y = 
     * ajd + 0 degrees 1 x = adj, y = opp + 90 degrees 2 x = opp, y = ajd
     * + 180 degrees 3 x = adj, y = opp + 270 degrees 
     */

    /*
     * DETERMINE WHICH QUADRANT OBJECT TWO IS IN 
     */

    if (x2 > x1) {
        /*
         * OBJECT TWO IS IN 1 OR 2 
         */
        delta_x = x2 - x1;
        if (y2 > y1) {
            delta_y = y2 - y1;
        } else {
            delta_y = y1 - y2;
        }
    } else {
        /*
         * OBJECT TWO IS IN 3 OR 4 
         */
        delta_x = x1 - x2;
        if (y2 > y1) {
            delta_y = y2 - y1;
        } else {
            delta_y = y1 - y2;
        }
    }

    delta_y = delta_y * delta_y;
    delta_x = delta_x * delta_x;

    total = delta_y + delta_x;

    distance = sqrt(total);

    return ((int) distance);
}

void
new_position(double x1, double y1, double bearing, double velocity)
{
    double          delta_x,
                    delta_y;
    double          radians;

    /*
     * Object two in which quadrant compared to object one? 0 x = opp, y = 
     * ajd + 0 degrees 1 x = adj, y = opp + 90 degrees 2 x = opp, y = ajd
     * + 180 degrees 3 x = adj, y = opp + 270 degrees 
     */

    /*
     * sin finds opp, cos finds adj 
     */

    if (bearing < 91) {
        radians = bearing * 2.0 * M_PI / 360.;
        delta_x = velocity * sin(radians);
        delta_y = velocity * cos(radians);
        new_x = x1 + delta_x;
        new_y = y1 + delta_y;
    } else if (bearing < 181) {
        bearing -= 90;
        radians = bearing * 2.0 * M_PI / 360.;
        delta_y = velocity * sin(radians);
        delta_x = velocity * cos(radians);
        new_x = x1 + delta_x;
        new_y = y1 - delta_y;
    } else if (bearing < 271) {
        bearing -= 180;
        radians = bearing * 2.0 * M_PI / 360.;
        delta_x = velocity * sin(radians);
        delta_y = velocity * cos(radians);
        new_x = x1 - delta_x;
        new_y = y1 - delta_y;
    } else {
        bearing -= 270;
        radians = bearing * 2.0 * M_PI / 360.;
        delta_y = velocity * sin(radians);
        delta_x = velocity * cos(radians);
        new_x = x1 - delta_x;
        new_y = y1 + delta_y;
    }
}

int
bearing(double x1, double y1, double x2, double y2)
{
    int             quadrant;
    double          delta_x,
                    delta_y;
    double          oppoadj;
    double          bearing;

    /*
     * Object two in which quadrant compared to object one? 0 x = opp, y = 
     * ajd + 0 degrees 1 x = adj, y = opp + 90 degrees 2 x = opp, y = ajd
     * + 180 degrees 3 x = adj, y = opp + 270 degrees 
     */

    if (x2 > x1) {
        delta_x = x2 - x1;
        if (y2 > y1) {
            quadrant = 0;
            delta_y = y2 - y1;
            oppoadj = delta_x / delta_y;
        } else {
            quadrant = 1;
            delta_y = y1 - y2;
            oppoadj = delta_y / delta_x;
        }
    } else {
        delta_x = x1 - x2;
        if (y2 > y1) {
            quadrant = 3;
            delta_y = y2 - y1;
            oppoadj = delta_y / delta_x;
        } else {
            quadrant = 2;
            delta_y = y1 - y2;
            oppoadj = delta_x / delta_y;
        }
    }

    bearing = atan(oppoadj);
    bearing = bearing / (2.0 * M_PI) * 360.;
    bearing = bearing + (90 * quadrant);

    return ((int) bearing);
}

void
set_arguments(const char *function_call)
{
    /* THIS FUNCTION CREATES AN ARRAY OF JACL INTEGER CONSTANTS TO
       REPRESENT THE ARGUMENTS PASSED TO A JACL FUNCTION */
    int             index,
                    counter,
                    length;
    int             position = 0; /* STORE THE INDEX OF THE WORD */
                    /* SETTING new_word TO FALSE SKIPS THE FIRST */
                    /* WORD WHICH IS THE FUNCTION NAME */
    int             new_word = FALSE;

    char             *arg_ptr[MAX_WORDS];
    int             arg_value[MAX_WORDS];

    struct integer_type *resolved_integer;
    struct cinteger_type *resolved_cinteger;

    /* SPLIT UP THE FUNCTION CALL STRING AND EXTRACT THE ARGUMENTS */
    length = strlen(function_call);

    for (index = 0; index < length; index++) {
        if (function_call[index] == '<') {
            argument_buffer[index] = 0;
            new_word = TRUE;
        } else {
            // COPY THE CHARACTER FROM THE CALLED NAME INTO THE CURRENT
            // ARGUMENT BUFFER        
            argument_buffer[index] = function_call[index];
            if (new_word) {
                // THIS IS THE FIRST CHARACTER OF A NEW ARGUMENT SO STORE
                // THE ADDRESS OF THIS CHARACTER IN THE ARGUMENT BUFFER.
                // Gate the write inside the bounds check; the old form
                // wrote arg_ptr[position] before bumping position, so
                // a function call with MAX_WORDS+1 args wrote one
                // pointer past the end of the local array.
                if (position < MAX_WORDS) {
                    arg_ptr[position] = &argument_buffer[index];
                    position++;
                }
                new_word = FALSE;
            }
        }
    }

    argument_buffer[index] = 0;

    /* CLEAR THE NEXT ARGUMENT POINTER (sentinel for the consumer
     * loop below). If position already maxed out at MAX_WORDS the
     * sentinel slot doesn't exist, but the iterating loop's bound
     * check below also stops at MAX_WORDS, so the loop is safe. */
    if (position < MAX_WORDS) {
        arg_ptr[position] = NULL;
    }

    /* STORE THE INTEGER VALUE OF EACH ARGUMENT PASSED. The index
     * bound mirrors the writer's cap; without it, a function call
     * that filled every slot above would walk past the sentinel
     * into undefined memory. */
    index = 0;
    while (index < MAX_WORDS && arg_ptr[index] != NULL) {
        //arg_value[index] = value_of(arg_ptr[index], TRUE);

        if ((resolved_integer = integer_resolve(arg_ptr[index])) != NULL) {
            arg_value[index] = resolved_integer->value;
        } else if ((resolved_cinteger = cinteger_resolve(arg_ptr[index])) != NULL) {
            arg_value[index] = resolved_cinteger->value;
        } else if (object_element_resolve(arg_ptr[index])) {
            arg_value[index] = oec;
        } else if ((counter = object_resolve(arg_ptr[index])) != -1) {
            if (counter < 1 || counter > objects) {
                badptrrun(arg_ptr[index], counter);
                pop_stack();
                return;
            } else {
                arg_value[index] = counter;
            }
        } else if (validate(arg_ptr[index])) {
            arg_value[index] = atoi(arg_ptr[index]);
        } else {
            arg_value[index] = -1;
        }

        index++;
    }

    /* THE CURRENT ARGUMENTS HAVE ALREADY BEEN PUSHED ONTO THE STACK
     * AND STORED IF PASSED AS AN ARGUMENT TO THIS FUNCTION SO IT IS 
     * OKAY TO CLEAR THEM AND SET THE NEW VALUES */
    clear_cinteger("arg");
    clear_cstring("string_arg");

    /* CREATE A CONSTANT FOR EACH ARGUMENT AFTER THE CORE FUNCTION NAME */
    index = 0;
    while (arg_ptr[index] != NULL) {
        if (index == 0) noun[3] = arg_value[index];
        add_cinteger ("arg", arg_value[index]);
        //printf("--- %s = %s\n", arg_ptr[index], arg_text_of(arg_ptr[index]));
        add_cstring ("string_arg", arg_text_of(arg_ptr[index]));
        index++;
    }
}

void
pop_stack()
{
    int index, counter;

    stack--;

    clear_cinteger ("arg");
    clear_cstring ("string_arg");

    /* RECREATE THE arg ARRAY FOR THIS STACK FRAME */
    for (index = 0; index < backup[stack].argcount; index++) {
        if (index == 0) noun[3] = backup[stack].arguments[0];
        add_cinteger ("arg", backup[stack].arguments[index]);
    }

    /* RECREATE THE string_arg ARRAY FOR THIS STACK FRAME */
    for (index = 0; index < backup[stack].argcount; index++) {
        add_cstring ("string_arg", backup[stack].str_arguments[index]);
    }

    /* RESTORE THE CONTENTS OF text_buffer */
    for (counter = 0; counter < 1024; counter++)
        text_buffer[counter] = backup[stack].text_buffer[counter];

    /* RESTORE THE CONTENTS OF called_name */
    //for (counter = 0; counter < 256; counter++)
    //called_name[counter] = backup[stack].called_name[counter];
    strncpy(called_name, backup[stack].called_name, 1023);
    called_name[1023] = 0;

    /* RESTORE THE CONTENTS OF scope_criterion */
    //for (counter = 0; counter < 21; counter++)
    //    scope_criterion[counter] = backup[stack].scope_criterion[counter];
    strncpy(scope_criterion, backup[stack].scope_criterion, 23);
    scope_criterion[23] = 0;

    /* RESTORE THE STORED FUNCTION NAMES THAT ARE USED WHEN AN
     * 'override' COMMAND IS ENCOUNTERED IN THE CURRENT FUNCTION */
    strncpy (override, backup[stack].override, 80);
    strncpy (default_function, backup[stack].default_function, 80);

    /* RESTORE ALL THE WORD POINTERS */
    for (counter = 0; counter < MAX_WORDS; counter++) {
        word[counter] = backup[stack].word[counter];
        quoted[counter] = backup[stack].quoted[counter];
    }

    executing_function = backup[stack].function;

    if (executing_function != NULL) {
        struct string_type *fname_cstring;
        strncpy (function_name, executing_function->name, 80);
        function_name[80] = 0;
        fname_cstring = cstring_resolve("function_name");
        strncpy (fname_cstring->value, executing_function->name, 80);
        fname_cstring->value[80] = 0;
    }

    wp = backup[stack].wp;
    LOCAL->value = backup[stack].local;
    LOCAL_X->value = backup[stack].local_x;
    LOCAL_Y->value = backup[stack].local_y;
    LOCAL_A->value = backup[stack].local_a;
    top_of_loop = backup[stack].top_of_loop;
    outfile = backup[stack].outfile;
    infile = backup[stack].infile;
    top_of_select = backup[stack].top_of_select;
    top_of_while = backup[stack].top_of_while;
    top_of_iterate = backup[stack].top_of_iterate;
    top_of_update = backup[stack].top_of_update;
    top_of_do_loop = backup[stack].top_of_do_loop;
    criterion_value = backup[stack].criterion_value;
    criterion_type = backup[stack].criterion_type;
    criterion_negate = backup[stack].criterion_negate;
    current_level = backup[stack].current_level;
    execution_level = backup[stack].execution_level;
    loop_integer = backup[stack].loop_integer;
    select_integer = backup[stack].select_integer;

#ifdef GLK
    glk_stream_set_position(game_stream, backup[stack].address, seekmode_Start);
#else
    fseek(file, backup[stack].address, SEEK_SET);
#endif

}

/* Return the interpreter's recursion guards to their pristine state. JACL was
 * written to run one game per process and let exit() reclaim everything, so
 * `stack` / `proxy_stack` are never otherwise zeroed between games. When a
 * process is reused (the iOS app, long-lived FastCGI) and the previous game's
 * terp died mid-call, they stay non-zero, and the next game trips the
 * STACK_SIZE guard ("stack overflow ... while trying to call ...") on its first
 * nested call. clear_game_data() calls this so every load starts at depth 0. */
void
reset_interpreter_stacks(void)
{
    stack = 0;
    proxy_stack = 0;
}

#ifdef GLK
int
push_stack(glsi32 file_pointer)
#else
int
push_stack(long file_pointer)
#endif
{
    /* COPY ALL THE CURRENT SYSTEM DATA ONTO THE STACK */
    int index;
    int counter = 0;

    if (stack == STACK_SIZE) {
        /* Refuse to recurse further rather than killing the worker.
         * The caller (execute or the proxy opcode handler) treats
         * FALSE as "function not callable" and returns up the chain;
         * the player sees an error and the next request handles
         * cleanly. */
        sprintf(error_buffer,
                "Stack overflow at depth %d while trying to call '%s'; aborting this call.",
                STACK_SIZE, called_name);
        log_error(error_buffer, PLUS_STDERR);
        return (FALSE);
    } else {
        backup[stack].infile = infile;
        infile = NULL;
        backup[stack].outfile = outfile;
        outfile = NULL;
        backup[stack].function = executing_function;
        backup[stack].address = file_pointer;
        backup[stack].wp = wp;
        backup[stack].local = LOCAL->value;
        backup[stack].local_x = LOCAL_X->value;
        backup[stack].local_y = LOCAL_Y->value;
        backup[stack].local_a = LOCAL_A->value;
        backup[stack].top_of_loop = top_of_loop;
        backup[stack].top_of_select = top_of_select;
        backup[stack].top_of_while = top_of_while;
        backup[stack].top_of_iterate = top_of_iterate;
        backup[stack].top_of_update = top_of_update;
        backup[stack].top_of_do_loop = top_of_do_loop;
        backup[stack].criterion_value = criterion_value;
        backup[stack].criterion_type = criterion_type;
        backup[stack].criterion_negate = criterion_negate;
        backup[stack].current_level = current_level;
        backup[stack].execution_level = execution_level;
        backup[stack].loop_integer = loop_integer;
        backup[stack].select_integer = select_integer;
        
        /* MAKE A COPY OF THE CURRENT CONTENTS OF text_buffer */
        for (counter = 0; counter < 1024; counter++)
            backup[stack].text_buffer[counter] = text_buffer[counter];

        /* MAKE A COPY OF THE CURRENT CONTENTS OF called_name */
        strncpy(backup[stack].called_name, called_name, 1023);
        backup[stack].called_name[1023] = 0;

        // MAKE A COPY OF THE CURRENT CONTENTS OF scope_criterion
        strncpy(backup[stack].scope_criterion, scope_criterion, 23);
        backup[stack].scope_criterion[23] = 0;

        /* COPY THE STORED FUNCTION NAMES THAT ARE USED WHEN AN
         * 'override' COMMAND IS ENCOUNTERED IN THE CURRENT FUNCTION */
        strncpy (backup[stack].override, override, 80);
        strncpy (backup[stack].default_function, default_function, 80);

        /* PUSH ALL THE WORD POINTERS ONTO THE STACK */
        for (counter = 0; counter < MAX_WORDS; counter++) {
            backup[stack].word[counter] = word[counter];
            backup[stack].quoted[counter] = quoted[counter];
        }

        // PUSH ALL THE ARGUMENTS AS INTEGERS ONTO THE STACK
        index = 0;
        current_cinteger = cinteger_table;

        if (current_cinteger != NULL) {
            do {
                if (!strcmp(current_cinteger->name, "arg")) {
                    backup[stack].arguments[index++] = current_cinteger->value;
                }
                current_cinteger = current_cinteger->next_cinteger;
            }
            while (current_cinteger != NULL);
        }

        // STORE THE NUMBER OF ARGUMENTS PASSED TO THIS FUNCTION
        // THIS IS THE SAME NUMBER FOR STRINGS AND INTEGERS
        backup[stack].argcount = index;

        // PUSH ALL THE ARGUMENTS AS STRINGS STRING ONTO THE STACK
        index = 0;
        current_cstring = cstring_table;

        if (current_cstring != NULL) {
            do {
                if (!strcmp(current_cstring->name, "string_arg")) {
                    strncpy(backup[stack].str_arguments[index], current_cstring->value, 255);
                    backup[stack].str_arguments[index][255] = 0;
                    index++;
                }

                current_cstring = current_cstring->next_string;
            }
            while (current_cstring != NULL);
        }
    }

    // PUSH ON TO THE NEXT STACK FRAME
    stack++;
    return (TRUE);
}

void
pop_proxy()
{
    int index, counter;

    proxy_stack--;

    clear_cinteger ("$integer");
    clear_cstring ("$string");
    clear_cstring ("$word");

    /* RECREATE THE integer ARRAY FOR THIS STACK FRAME */
    for (index = 0; index < proxy_backup[proxy_stack].integercount; index++) {
        add_cinteger ("$integer", proxy_backup[proxy_stack].integer[index]);
    }

    /* RECREATE THE text ARRAY FOR THIS STACK FRAME */
    for (index = 0; index < proxy_backup[proxy_stack].textcount; index++) {
        add_cstring ("$string", proxy_backup[proxy_stack].text[index]);
    }

    /* RECREATE THE $word ARRAY FOR THIS STACK FRAME */
    for (index = 0; index < proxy_backup[proxy_stack].commandcount; index++) {
        add_cstring ("$word", proxy_backup[proxy_stack].command[index]);
    }

    /* RESTORE ALL THE NOUN POINTERS */
    for (counter = 0; counter < 4; counter++)
        noun[counter] = proxy_backup[proxy_stack].object_pointers[counter];

    /* PUSH ALL THE RESOLVED OBJECTS ONTO THE STACK */
    for (index = 0; index < 4; index++) {
        list_size[index] = proxy_backup[proxy_stack].list_size[index];
        max_size[index] = proxy_backup[proxy_stack].max_size[index];
        for (counter = 0; counter < max_size[index]; counter++) {
            object_list[index][counter] = proxy_backup[proxy_stack].object_list[index][counter];
        }
    }

    start_of_this_command = proxy_backup[proxy_stack].start_of_this_command;
    start_of_last_command = proxy_backup[proxy_stack].start_of_last_command;
    after_from = proxy_backup[proxy_stack].after_from;
    last_exact = proxy_backup[proxy_stack].last_exact;
}

int
push_proxy()
{
    /* COPY ALL THE CURRENT SYSTEM DATA ONTO THE STACK */
    int index;
    int    counter = 0;
    int    command = 0;
    int text = 0;

    current_cinteger = cinteger_table;
    current_cstring = cstring_table;

    if (proxy_stack == STACK_SIZE) {
        /* Same change-of-policy as push_stack: refuse to recurse
         * rather than killing the worker. Callers handle FALSE by
         * exiting the current function cleanly. */
        sprintf(error_buffer,
                "Proxy stack overflow at depth %d; aborting this call.",
                STACK_SIZE);
        log_error(error_buffer, PLUS_STDERR);
        return (FALSE);
    } else {
        proxy_backup[proxy_stack].start_of_this_command = start_of_this_command;
        proxy_backup[proxy_stack].start_of_last_command = start_of_last_command;
        
        /* PUSH ALL THE OBJECT POINTERS ONTO THE STACK */
        for (counter = 0; counter < 4; counter++)
            proxy_backup[proxy_stack].object_pointers[counter] = noun[counter];

        /* PUSH ALL THE RESOLVED OBJECTS ONTO THE STACK */
        for (index = 0; index < 4; index++) {
            for (counter = 0; counter < max_size[index]; counter++) {
                proxy_backup[proxy_stack].object_list[index][counter] 
                    =    object_list[index][counter];
            }
            proxy_backup[proxy_stack].list_size[index] = list_size[index];
            proxy_backup[proxy_stack].max_size[index] = max_size[index];
        }

        /* PUSH ALL THE CURRENT COMMAND INTEGERS ONTO THE STACK */
        counter = 0;

        if (current_cinteger != NULL) {
            do {
                if (!strcmp(current_cinteger->name, "$integer")) {
                    proxy_backup[proxy_stack].integer[counter++] = current_cinteger->value;                }
                current_cinteger = current_cinteger->next_cinteger;
            }
            while (current_cinteger != NULL);
        }

        proxy_backup[proxy_stack].integercount = counter;

        // PUSH ALL THE TEXT STRING SUPPLIED BY THE CURRENT COMMAND ONTO THE STACK
        text = 0;
        command = 0;

        if (current_cstring != NULL) {
            do {
                if (!strcmp(current_cstring->name, "$string")) {
                    strncpy(proxy_backup[proxy_stack].text[text], current_cstring->value, 255);
                    proxy_backup[proxy_stack].text[text][255] = 0;
                    text++;
                } else if (!strcmp(current_cstring->name, "$word")) {
                    strncpy(proxy_backup[proxy_stack].command[command], current_cstring->value, 255);
                    proxy_backup[proxy_stack].command[command][255] = 0;
                    command++;
                }

                current_cstring = current_cstring->next_string;
            }
            while (current_cstring != NULL);
        }

        proxy_backup[proxy_stack].textcount = text;
        proxy_backup[proxy_stack].commandcount = command;
        proxy_backup[proxy_stack].after_from = after_from;
        proxy_backup[proxy_stack].last_exact = last_exact;
    }

    // PUSH ON TO THE NEXT STACK FRAME
    proxy_stack++;
    return (TRUE);
}

int
condition()
{
    /* COMPARE GROUPS OF TWO ELEMENTS. RETURN TRUE IF ANY ONE GROUP OF 
     * ELEMENTS COMPARE 'TRUE' */
    int             first;

    first = 1;

    while (((first +2) < MAX_WORDS) && word[first + 2] != NULL) {
        if (logic_test(first))
            return (TRUE);
        else
            first = first + 3;
    }
    return (FALSE);
}

int
and_condition()
{
    /* COMPARE GROUPS OF TWO ELEMENTS. RETURN FALSE IF ANY ONE GROUP OF 
     * ELEMENTS COMPARE 'FALSE' */
    int             first;

    first = 1;

    while (((first +2) < MAX_WORDS) && word[first + 2] != NULL) {
        if (logic_test(first) == FALSE)
            return (FALSE);
        else
            first = first + 3;
    }
    return (TRUE);
}

int
logic_test(int first)
{
    long            index,
                    compare;

    resolved_attribute = FALSE;

    index = value_of(word[first], TRUE);
    compare = value_of(word[first + 2], TRUE);

    if (!strcmp(word[first + 1], "=") || !strcmp(word[first + 1], "==")) {
        if (index == compare)
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], ">")) {
        if (index > compare)
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], "<")) {
        if (index < compare)
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], "is")) {
        if (index < 1 || index > objects) {
            unkobjrun(first);
            return (FALSE);
        } else
            return (scope(index, word[first + 2], FALSE));
    } else if (!strcmp(word[first + 1], "isnt")) {
        if (index < 1 || index > objects) {
            unkobjrun(first);
            return (FALSE);
        } else
            return (!scope(index, word[first + 2], FALSE));
    } else if (!strcmp(word[first + 1], "has"))
        if (index < 1 || index > objects) {
            unkobjrun(first);
            return (FALSE);
        } else {
            if (resolved_attribute == SYSTEM_ATTRIBUTE) {
                return (object[index]->attributes & compare);
            } else {
                return (object[index]->user_attributes & compare);
            }
        }
    else if (!strcmp(word[first + 1], "hasnt"))
        if (index < 1 || index > objects) {
            unkobjrun(first);
            return (FALSE);
        } else {
            if (resolved_attribute == SYSTEM_ATTRIBUTE) {
                return (!(object[index]->attributes & compare));
            } else {
                return (!(object[index]->user_attributes & compare));
            }
        }
    else if (!strcmp(word[first + 1], "!=")
             || !strcmp(word[first + 1], "<>")) {
        if (index != compare)
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], ">=")
               || !strcmp(word[first + 1], "=>")) {
        if (index >= compare)
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], "<=")
               || !strcmp(word[first + 1], "=<")) {
        if (index <= compare)
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], "grandof")) {
        /* GRANDOF SAYS THAT AN OBJECT IS THE EVENTUAL PARENT OF ANOTHER OBJECT, NOT
         * NECESSARILY IMMEDIATE */
        if (index < 1 || index > objects) {
            unkobjrun(first);
            return (FALSE);
        } else {
            if (compare < 1 || compare > objects) {
                unkobjrun(first + 2);
                return (FALSE);
            } else {
                if (parent_of(index, compare, UNRESTRICT))
                    return (TRUE);
                else
                    return (FALSE);
            }
        }
    } else if (!strcmp(word[first + 1], "!grandof")) {
        if (index < 1 || index > objects) {
            unkobjrun(first);
            return (FALSE);
        } else {
            if (compare < 1 || compare > objects) {
                unkobjrun(first + 2);
                return (FALSE);
            } else {
                if (parent_of(index, compare, UNRESTRICT))
                    return (FALSE);
                else
                    return (TRUE);
            }
        }
    } else if (!strcmp(word[first + 1], "locationof")) {
        /* LOCATIONOF SAYS THAT THE LEFT VALUE IS THE LOCATION REACHED
         * BY WALKING THE RIGHT OBJECT'S PARENT CHAIN UP TO THE FIRST
         * LOCATION. Mirrors the value-expression form used by `set`. */
        if (index < 1 || index > objects) {
            unkobjrun(first);
            return (FALSE);
        } else if (compare < 1 || compare > objects) {
            unkobjrun(first + 2);
            return (FALSE);
        } else {
            return (grand_of(compare, FALSE) == index);
        }
    } else if (!strcmp(word[first + 1], "!locationof")) {
        if (index < 1 || index > objects) {
            unkobjrun(first);
            return (FALSE);
        } else if (compare < 1 || compare > objects) {
            unkobjrun(first + 2);
            return (FALSE);
        } else {
            return (grand_of(compare, FALSE) != index);
        }
    } else {
        /* The operator slot is word[first + 1]; the old form quoted
         * word[2], which was the operator of the FIRST triple in a
         * compound condition, not the failing one. */
        sprintf(error_buffer,
                "ERROR: In function \"%s\", illegal operator \"%s\".^",
                executing_function->name, word[first + 1]);
        write_text(error_buffer);
        return (FALSE);
    }
}

int
strcondition()
{
    int             first;

    first = 1;

    while (((first +2) < MAX_WORDS) && word[first + 2] != NULL) {
        if (str_test(first))
            return (TRUE);
        else
            first = first + 3;
    }
    return (FALSE);
}

int
and_strcondition()
{
    int             first;

    first = 1;

    while (((first +2) < MAX_WORDS) && word[first + 2] != NULL) {
        if (str_test(first) == FALSE)
            return (FALSE);
        else
            first = first + 3;
    }
    return (TRUE);
}

int
str_test(int first)
{
    const char  *index;
    const char  *compare;

    // GET THE TWO STRING VALUES TO COMPARE

    index = arg_text_of_word(first);
    compare = arg_text_of_word(first + 2);

    if (!strcmp(word[first + 1], "==") || !strcmp(word[first + 1], "=")) {
        if (!strcasecmp(index, compare)) {
            return (TRUE);
        } else {
            return (FALSE);
        }
    } else if (!strcmp(word[first + 1], "!contains")) {
        if (strcasestr(index, compare))
            return (FALSE);
        else
            return (TRUE);
    } else if (!strcmp(word[first + 1], "contains")) {
        if (strcasestr(index, compare))
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], "!beginswith")) {
        if (strncmp(index, compare, strlen(compare)))
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], "beginswith")) {
        if (!strncmp(index, compare, strlen(compare)))
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], "<>") || !strcmp(word[first + 1], "!=")) {
        if (strcasecmp(index, compare))
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], "==C") || !strcmp(word[first + 1], "=C")) {
        if (!strcmp(index, compare)) {
            return (TRUE);
        } else {
            return (FALSE);
        }
    } else if (!strcmp(word[first + 1], "!containsC")) {
        if (strstr(index, compare))
            return (FALSE);
        else
            return (TRUE);
    } else if (!strcmp(word[first + 1], "containsC")) {
        if (strstr(index, compare))
            return (TRUE);
        else
            return (FALSE);
    } else if (!strcmp(word[first + 1], "<>C") || !strcmp(word[first + 1], "!=C")) {
        if (strcmp(index, compare))
            return (TRUE);
        else
            return (FALSE);
    } else {
        sprintf(error_buffer,
                "ERROR: In function \"%s\", illegal operator \"%s\".^",
                executing_function->name, word[2]);
        write_text(error_buffer);
        return (FALSE);
    }
}

void
add_cinteger(const char *name, int value)
{
    /* THIS FUNCTION ADDS A NEW JACL CONSTANT TO THE LIST */

    if ((new_cinteger = (struct cinteger_type *)
        malloc(sizeof(struct cinteger_type))) == NULL)
        outofmem();
    else {
        if (cinteger_table == NULL) {
            cinteger_table = new_cinteger;
        } else {
            /* FIND LAST CONSTANT IN LIST */    
            current_cinteger = cinteger_table;
            while (current_cinteger->next_cinteger != NULL) {
                current_cinteger = current_cinteger->next_cinteger;
            }
            current_cinteger->next_cinteger = new_cinteger;
        }
        strncpy(new_cinteger->name, name, 40);
        new_cinteger->name[40] = 0;
        new_cinteger->value = value;
        new_cinteger->next_cinteger = NULL;
    }
}

void
clear_cinteger(const char *name)
{
    /* FREE CONSTANTS THAT HAVE SUPPLIED NAME*/

    //printf("--- clear integer %s\n", name);
    if (cinteger_table != NULL) {
        current_cinteger = cinteger_table;
        previous_cinteger = cinteger_table;
        while (current_cinteger != NULL) {
            //sprintf(temp_buffer, "--- checking integer %s^", current_cinteger->name);
            //write_text(temp_buffer);
            if (!strcmp(current_cinteger->name, name)) {
                //sprintf(temp_buffer, "--- found integer %s^", name);
                //write_text(temp_buffer);
                /* FREE THIS CONSTANT */
                if (previous_cinteger == current_cinteger) {
                    // THE INTEGER BEING CLEARED IS THE FIRST INTEGER IN THE LIST
                    cinteger_table = current_cinteger->next_cinteger;
                    previous_cinteger = current_cinteger->next_cinteger;
                    free(current_cinteger);
                    current_cinteger = previous_cinteger;
                } else {
                    previous_cinteger->next_cinteger = current_cinteger->next_cinteger;
                    free(current_cinteger);
                    current_cinteger = previous_cinteger->next_cinteger;
                }
            } else {
                previous_cinteger = current_cinteger;
                current_cinteger = current_cinteger->next_cinteger;
            }
        }
    }
    //printf("--- leaving clear integer\n");
}

void
add_cstring(const char *name, const char *value)
{
    /* ADD A STRING CONSTANT WITH THE SUPPLIED NAME AND VALUE */

    if ((new_string = (struct string_type *)
         malloc(sizeof(struct string_type))) == NULL)
        outofmem();
    else {
        if (cstring_table == NULL) {
            cstring_table = new_string;
        } else {
            /* FIND LAST STRING IN LIST */    
            current_cstring = cstring_table;
            while (current_cstring->next_string != NULL) {
                current_cstring = current_cstring->next_string;
            }
            current_cstring->next_string = new_string;
        }
        strncpy(new_string->name, name, 40);
        new_string->name[40] = 0;
        strncpy(new_string->value, value, 1023);
        /* NUL goes at index 1023 (one past the 1023 chars strncpy
         * may have written), not 1024. The struct is [1025] so
         * writing [1024] was in bounds but left byte 1023
         * potentially non-NUL, which would corrupt strlen/strcpy
         * callers on a 1023+-char source. */
        new_string->value[1023] = 0;
        new_string->next_string = NULL;
    }
}

void
clear_cstring(const char *name)
{
    /* FREE CONSTANTS THAT HAVE SUPPLIED NAME*/
    if (cstring_table != NULL) {
        current_cstring = cstring_table;
        previous_cstring = cstring_table;
        while (current_cstring != NULL) {
            if (!strcmp(current_cstring->name, name)) {
                /* FREE THIS STRING */
                if (previous_cstring == current_cstring) {
                    cstring_table = current_cstring->next_string;
                    previous_cstring = current_cstring->next_string;
                    free(current_cstring);
                    current_cstring = previous_cstring;
                } else {
                    previous_cstring->next_string = current_cstring->next_string;
                    free(current_cstring);
                    current_cstring = previous_cstring->next_string;
                }
            } else {
                previous_cstring = current_cstring;
                current_cstring = current_cstring->next_string;
            }
        }
    }
}

void
inspect (int object_num)
{
    // THIS FUNCTION DISPLAYS THE STATE OF A JACL OBJECT FOR DEBUGGING

    int index, attribute_value;

    struct attribute_type *pointer = attribute_table;

    if (object_num < 1 || object_num > objects) {
        badptrrun(word[1], object_num);
        return;
    }

    write_text("label: ");
    write_text(object[object_num]->label);

    if (object[object_num]->attributes & LOCATION) {
        // OUTPUT ALL THE ATTRIBUTES WITH LOCATION ATTRIBUTE TEXT
        write_text("^has location attributes: ");
        index = 0;
        attribute_value = 1;
        while (location_attributes[index] != NULL) {
            if (object[object_num]->attributes & attribute_value) {
                write_text(location_attributes[index]);
            }
            index++;
            attribute_value *= 2;
        }
    } else {
        // OUTPUT ALL THE ATTRIBUTES WITH OBJECT ATTRIBUTE TEXT
        write_text("^has object attributes: ");
        index = 0;
        attribute_value = 1;
        while (object_attributes[index] != NULL) {
            if (object[object_num]->attributes & attribute_value) {
                write_text(object_attributes[index]);
            }
            index++;
            attribute_value *= 2;
        }

        write_text("^has user attributes: ");
    }

    if (pointer != NULL) {
        // THERE ARE USER ATTRIBUTES, SO CHECK IF THIS OBJECT OR LOCATION 
        // HAS ANY OF THEM
        do {
            if (object[object_num]->user_attributes & pointer->value) { 
                write_text(pointer->name);
                write_text(" ");
            }

            pointer = pointer->next_attribute;
        } while (pointer != NULL);
    }

    write_text("^");

    index = 0;
    if (object[object_num]->attributes & LOCATION) {
        while (location_elements[index] != NULL) {
            if (index < 12) {
                if (object[object_num]->integer[index] < 1 || object[object_num]->integer[index] > objects) {
                    sprintf(temp_buffer, "%s: nowhere (%d)^", location_elements[index], object[object_num]->integer[index]);
                } else {
                    sprintf(temp_buffer, "%s: %s (%d)^", location_elements[index], object[object[object_num]->integer[index]]->label, object[object_num]->integer[index]);
                }
            } else {
                sprintf(temp_buffer, "%s: %d^", location_elements[index], object[object_num]->integer[index]);
            }
            write_text(temp_buffer);
            index++;
        }
    } else {
        while (object_elements[index] != NULL) {
            if (index == 0) {
                /* PARENT slot: mirror the bounds check the LOCATION
                 * branch above does, otherwise objects with PARENT == 0
                 * (limbo) or a corrupt index crash on object[oob]->label. */
                if (object[object_num]->integer[index] < 1 || object[object_num]->integer[index] > objects) {
                    sprintf(temp_buffer, "%s: nowhere (%d)^", object_elements[index], object[object_num]->integer[index]);
                } else {
                    sprintf(temp_buffer, "%s: %s (%d)^", object_elements[index], object[object[object_num]->integer[index]]->label, object[object_num]->integer[index]);
                }
            } else {
                sprintf(temp_buffer, "%s: %d^", object_elements[index], object[object_num]->integer[index]);
            }
            write_text(temp_buffer);
            index++;
        }
    }
}

int
grand_of(int child, int objs_only)
{
    /* THIS FUNCTION WILL CLIMB THE OBJECT TREE STARTING AT 'CHILD' UNTIL
     * A 'PARENT' IS REACHED */
   
    /* objs_only ARGUMENT TELLS FUNCTION TO IGNORE OBJECT IF IT IS IN A
     * LOCATION */

    int             parent;

    if (object[child]->PARENT != NOWHERE) {
        /* STORE THE CHILDS PARENT OBJECT */
        parent = object[child]->PARENT;

        if (object[parent]->attributes & LOCATION) {
            if (objs_only) {
                /* THE CHILDS PARENT IS LOCATION AND SEARCH IS RESTRICTED TO 
                  * OBJECTS */
                return (child);
            } else {
                return (parent);
            }
        } else {
            /* KEEP LOOKING UP THE TREE UNTIL THE CHILD HAS NO
             * PARENT */
            return (grand_of(parent, objs_only));
        }
    } else {
        /* THE SPECIFIED OBJECT HAS NO PARENT */
        return (child);
    }
}

int
select_next()
{

    while (++*select_integer <= objects) {
        switch (criterion_type) {
            case CRI_ATTRIBUTE:
                if (object[*select_integer]->attributes & criterion_value) {
                    if (!criterion_negate) {
                        return TRUE;
                    }
                } else {
                    if (criterion_negate) {
                        return TRUE;
                    }
                }
                break;
            case CRI_USER_ATTRIBUTE:
                if (object[*select_integer]->user_attributes & criterion_value) {
                    if (!criterion_negate) {
                        return TRUE;
                    }
                } else {
                    if (criterion_negate) {
                        return TRUE;
                    }
                }
                break;
            case CRI_PARENT:
                if (object[*select_integer]->PARENT == criterion_value) {
                    if (!criterion_negate) {
                        return TRUE;
                    }
                } else {
                    if (criterion_negate) {
                        return TRUE;
                    }
                }
                break;
            case CRI_SCOPE:
                if (scope(*select_integer, scope_criterion, FALSE)) {
                    if (!criterion_negate) {
                        return TRUE;
                    }
                } else {
                    if (criterion_negate) {
                        return TRUE;
                    }
                }
                break;
        }
    }

    return (FALSE);
}

#ifndef GLK
/* Render the web status bar through the virtual character grid, then
 * emit it wrapped in a <jacl-status> marker for the JS to relocate
 * into #statuswin. status_window holds the row count;
 * status_window_width (default 80) the column count.
 *
 * Factored out of the `updatestatus` opcode handler in `execute()`
 * above so that utils.c's eachturn() can also invoke it directly --
 * gives the runtime a belt-and-suspenders guarantee that the status
 * bar reflects the latest status_window_width after every player
 * command, even if the game's library copy predates the
 * `if interpreter = CGI: updatestatus` block in +system_eachturn
 * (verbs.library, commit 248b2e9, 2026-05-04). */
void
web_render_status_bar(void)
{
    struct integer_type *sw = integer_resolve("status_window");
    if (sw == NULL || sw->value <= 0) return;

    struct integer_type *sww = integer_resolve("status_window_width");
    int cols = (sww != NULL && sww->value > 0) ? sww->value : 80;

    /* Clamp TOTAL_MOVES to >= 0 for the duration of the bar emission.
     * The loader starts it at -1 ("TIME PASSES BEFORE THE FIRST
     * PROMPT") so a status bar rendered during +intro -- before
     * eachturn() has bumped it to 0 -- would otherwise show
     * 'Moves: -1'. Restore the original value after so other JACL
     * code (e.g. Eria.jacl which checks total_moves = -1) keeps
     * working. */
    struct integer_type *tm = integer_resolve("total_moves");
    int saved_tm = (tm != NULL) ? tm->value : 0;
    if (tm != NULL && tm->value < 0) {
        tm->value = 0;
    }

    sprintf(temp_buffer,
            "<jacl-status data-lines=~%d~ style=~display:none~>",
            sw->value);
    write_text(temp_buffer);
    web_status_begin(sw->value, cols);
    /* Counter incremented each time the bar is emitted this request.
     * cgijacl.c's end-of-ajax safety net checks it and emits one
     * more if no updatestatus fired during the command -- catches
     * TIME=false verbs like look / inventory that skip eachturn. */
    web_status_emit_count++;

    /* Try a web-specific override first, then the GLK function name
     * (so games that already have +update_status_window can share it
     * between targets thanks to the grid emulation), then fall back
     * to a sensible default. */
    if (execute("+update_status_window_web") == FALSE
        && execute("+update_status_window") == FALSE) {
        /* Default: location title left-justified, 'Score: N
         * Moves: N' right-justified. Skipped when HERE doesn't
         * resolve to a real object (e.g. utility games like life.jacl
         * whose player has no parent). */
        char defstr[64];
        int len, pad, j;
        int here_idx = HERE;
        if (here_idx > 0 && here_idx <= objects
            && object[here_idx] != NULL) {
            const char *here_text = sentence_output(here_idx, TRUE);
            if (here_text != NULL) {
                web_status_cursor(1, 0);
                for (j = 0; here_text[j] && j + 2 < cols; j++) {
                    web_status_putchar((unsigned char) here_text[j]);
                }
            }
        }
        sprintf(defstr, "Score: %d  Moves: %d",
                SCORE->value, TOTAL_MOVES->value);
        len = (int) strlen(defstr);
        pad = cols - len - 1;
        if (pad < 0) pad = 0;
        web_status_cursor(pad, 0);
        for (j = 0; defstr[j]; j++) {
            web_status_putchar((unsigned char) defstr[j]);
        }
    }
    web_status_end();
    write_text("</jacl-status>");
    if (tm != NULL) {
        tm->value = saved_tm;
    }
}

/* Converts an integer value to its hex character*/
char to_hex(char code) {
    static char hex[] = "0123456789abcdef";
    return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(const char *str) {
    const char *pstr = str;
    char *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
    while (*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')
            *pbuf++ = *pstr;
        else if (*pstr == ' ')
            *pbuf++ = '+';
        else {
            *pbuf++ = '%'; *pbuf++ = to_hex(*pstr >> 4); *pbuf++ = to_hex(*pstr & 15);
        }
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}
#endif
