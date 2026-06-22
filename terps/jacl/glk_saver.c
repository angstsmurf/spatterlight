/* glk_saver.c --- Functions to save and load the game state to disk
 *                 using the GLK API.
  (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"

static void write_integer(strid_t stream, int x);
static void write_long(strid_t stream, long x);
static int  read_integer(strid_t stream);

int
save_game(frefid_t saveref)
{
	struct integer_type *current_integer = integer_table;
    struct function_type *current_function = function_table;
    struct string_type *current_string = string_table;

	int             index, counter;
	strid_t			bookmark = NULL;
		
	bookmark = glk_stream_open_file(saveref, filemode_Write, 0);

	if (bookmark == NULL) {
		return (FALSE);
	}

	/* WE'RE DONE WITH THE FILE REFERENCE NOW THAT THE STREAM
	 * HAS BEEN SUCCESSFULLY OPENED */
	glk_fileref_destroy (saveref);

	/* THIS IS WRITTEN TO HELP VALIDATE THE SAVED GAME 
	 * BEFORE CONTINUING TO LOAD IT */
	write_integer (bookmark, objects);
	write_integer (bookmark, integers);
	write_integer (bookmark, functions);
	write_integer (bookmark, strings);

	while (current_integer != NULL) {
		write_integer (bookmark, current_integer->value);
		current_integer = current_integer->next_integer;
	}

    while (current_function != NULL) {
		/* Filter by nosave so the format matches saver.c (CGI). The
		 * previous unfiltered write made glk saves silently
		 * incompatible with CGI saves of the same game. */
		if (current_function->nosave == FALSE) {
			write_integer (bookmark, current_function->call_count);
		}
        current_function = current_function->next_function;
    }

	for (index = 1; index <= objects; index++) {
		if (object[index]->nosave)
			continue;
		
		for (counter = 0; counter < 16; counter++) {
			write_integer (bookmark, object[index]->integer[counter]);
		}

		write_long (bookmark, object[index]->attributes);
		write_long (bookmark, object[index]->user_attributes);
	}

	/* WRITE OUT ALL THE CURRENT VALUES OF THE STRING VARIABLES */
    while (current_string != NULL) {
		for (index = 0; index < 1024; index++) {
    		glk_put_char_stream(bookmark, current_string->value[index]);
		}
        current_string = current_string->next_string;
    }

	/* Persist last_command so 'again' works after restore. saver.c
	 * already did this; glk_saver previously dropped it, leaving
	 * restored Glk saves with an empty or stale last-command slot. */
	for (index = 0; index < 1024; index++) {
		glk_put_char_stream(bookmark, last_command[index]);
	}

	write_integer (bookmark, player);
	write_integer (bookmark, noun[3]);

	/* SAVE THE CURRENT VOLUME OF EACH OF THE SOUND CHANNELS */
	for (index = 0; index < 8; index++) {
		sprintf(temp_buffer, "volume[%d]", index);
		write_integer (bookmark, cinteger_resolve(temp_buffer)->value);
	}

	/* SAVE THE CURRENT VALUE OF THE GLK TIMER */
	write_integer (bookmark, cinteger_resolve("event_timer")->value);

	/* CLOSE THE STREAM */
	glk_stream_close(bookmark, NULL);

	TIME->value = FALSE;
	return (TRUE);
}

int
restore_game(frefid_t saveref, int warn)
{
	struct integer_type *current_integer = integer_table;
    struct function_type *current_function = function_table;
    struct string_type *current_string = string_table;

	int             index, counter;
	int             file_objects,
	                file_integers,
					file_functions,
					file_strings;
	strid_t			bookmark;
		
	bookmark = glk_stream_open_file(saveref, filemode_Read, 0);

	if (!bookmark) {
		return (FALSE);
	}

	/* WE'RE DONE WITH THE FILE REFERENCE NOW THAT THE STREAM
	 * HAS BEEN SUCCESSFULLY OPENED */
	glk_fileref_destroy (saveref);

	/* THIS IS WRITTEN TO HELP VALIDATE THE SAVED GAME 
	 * BEFORE CONTINUING TO LOAD IT */
	file_objects = read_integer(bookmark);
	file_integers = read_integer(bookmark);
	file_functions = read_integer(bookmark);
	file_strings = read_integer(bookmark);

	if (file_objects != objects 
		|| file_integers != integers 
		|| file_functions != functions 
		|| file_strings != strings) {
		if (warn == FALSE) {
			log_error(cstring_resolve("BAD_SAVED_GAME")->value, PLUS_STDOUT);
		}
		glk_stream_close(bookmark, NULL);
		return (FALSE);
	}

	while (current_integer != NULL) {
		current_integer->value = read_integer (bookmark);
		current_integer = current_integer->next_integer;
	}

	while (current_function != NULL) {
		/* Matched filter from save_game above; must agree with the
		 * write side or the byte stream shifts by 4 bytes per
		 * non-nosave function and the rest of the read goes haywire. */
		if (current_function->nosave == FALSE) {
			current_function->call_count = read_integer (bookmark);
		}
		current_function = current_function->next_function;
	}

	for (index = 1; index <= objects; index++) {
		if (object[index]->nosave)
			continue;

		for (counter = 0; counter < 16; counter++) {
			object[index]->integer[counter] = read_integer(bookmark);
		}

		object[index]->attributes = read_integer(bookmark);
		object[index]->user_attributes = read_integer(bookmark);
	}

    while (current_string != NULL) {
		for (index = 0; index < 1024; index++) {
			glsi32 c = glk_get_char_stream(bookmark);
			/* Truncated save returns -1 on EOF; without this check
			 * the value field gets filled with 0xFF for the whole
			 * 1024 bytes and the rest of the read goes haywire. */
			if (c < 0) c = 0;
			current_string->value[index] = (char) c;
		}
		/* string_type::value is [1025]; defensively NUL-terminate so
		 * subsequent strlen on a string whose last byte happens to
		 * be non-zero doesn't walk past the buffer end. */
		current_string->value[1024] = 0;
        current_string = current_string->next_string;
    }

	/* Matches the save side: pull last_command back so 'again' still
	 * has a target after the restore. */
	for (index = 0; index < 1024; index++) {
		glsi32 c = glk_get_char_stream(bookmark);
		if (c < 0) c = 0;
		last_command[index] = (char) c;
	}

	{
		int saved_player = read_integer(bookmark);
		int saved_noun3  = read_integer(bookmark);
		/* Same defence as saver.c: refuse a player value past the
		 * end of the object table. 0 is allowed (legitimate "no
		 * player yet" state). noun[3] is unchecked because -1
		 * ("no second noun") and 0 (FALSE) are both legal. */
		if (saved_player < 0 || saved_player > objects) {
			if (warn == FALSE) {
				sprintf(error_buffer,
				        "Saved file has out-of-range player (%d, objects=%d); refusing restore.",
				        saved_player, objects);
				log_error(error_buffer, PLUS_STDOUT);
			}
			glk_stream_close(bookmark, NULL);
			return (FALSE);
		}
		player  = saved_player;
		noun[3] = saved_noun3;
	}

	/* RESTORE THE CURRENT VOLUME OF EACH OF THE SOUND CHANNELS */
	for (index = 0; index < 8; index++) {
		sprintf(temp_buffer, "volume[%d]", index);
		counter = read_integer(bookmark);
		cinteger_resolve(temp_buffer)->value = counter;

		if (SOUND_SUPPORTED->value) {
			/* SET THE GLK VOLUME */
			glk_schannel_set_volume(sound_channel[index], (glui32) counter);
		}
	}

	/* RESTORE THE CURRENT VALUE OF THE GLK TIMER */
	counter = read_integer(bookmark);
	cinteger_resolve("event_timer")->value = counter;

	/* SET THE GLK TIMER */
	glk_request_timer_events((glui32) counter);

	/* CLOSE THE STREAM */
	glk_stream_close(bookmark, NULL);

	TIME->value = FALSE;
	return (TRUE);
}

void
write_integer(strid_t stream, int x)
{
	unsigned char c;

    c = (unsigned char) (x) & 0xFF;
    glk_put_char_stream(stream, c);
    c = (unsigned char) (x >> 8) & 0xFF;
    glk_put_char_stream(stream, c);
    c = (unsigned char) (x >> 16) & 0xFF;
    glk_put_char_stream(stream, c);
    c = (unsigned char) (x >> 24) & 0xFF;
    glk_put_char_stream(stream, c);
}

int 
read_integer(strid_t stream)
{
    int a, b, c, d;
    a = (int) glk_get_char_stream(stream);
    b = (int) glk_get_char_stream(stream);
    c = (int) glk_get_char_stream(stream);
    d = (int) glk_get_char_stream(stream);
    return a | (b << 8) | (c << 16) | (d << 24);
}

void
write_long(strid_t stream, long x)
{
	unsigned char c;

    c = (unsigned char) (x) & 0xFF;
    glk_put_char_stream(stream, c);
    c = (unsigned char) (x >> 8) & 0xFF;
    glk_put_char_stream(stream, c);
    c = (unsigned char) (x >> 16) & 0xFF;
    glk_put_char_stream(stream, c);
    c = (unsigned char) (x >> 24) & 0xFF;
    glk_put_char_stream(stream, c);
}


