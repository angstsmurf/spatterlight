/* display.c --- Functions for language output.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"
#include "encapsulate.h"
#include "parser.h"

/* All the *_output helpers below share the global `temp_buffer`.
 * That means a caller who tries to use two macro outputs in the
 * same write expression (e.g. `write noun1{the} " sees " noun2{that}`)
 * will see the second call overwrite the first before write_text
 * gets to send them. The engine's write/print pipeline copies each
 * argument as it streams output, so the common idiom is safe, but
 * if you ever store the return value across another *_output call
 * you must strdup or strcpy it first. */

/* Look up a cstring by name and return its value, falling back to
 * the supplied literal if the library doesn't define it. Every
 * *_output below used `cstring_resolve("X")->value` directly, which
 * segfaults if a stripped-down or in-progress library is missing
 * X. The fallback keeps the engine alive and substitutes English
 * defaults the player will recognise. */
static const char *
safe_cstr(const char *name, const char *fallback)
{
    struct string_type *s = cstring_resolve(name);
    if (s == NULL) return fallback;
    return s->value;
}

/* Guard every *_output entry: many of them are called via macro_
 * resolve(), which can hand us an integer that came from a game-
 * side `noun3` pointer (zero / unset) or from a corrupted save.
 * Without this check object[0] (a sentinel) or object[index] past
 * the table both deref into garbage. */
static int
valid_object(int index)
{
    return (index >= 1 && index <= objects && object[index] != NULL);
}

int
check_light(int where)
{
	int             index;

	if ((object[where]->attributes & DARK) == FALSE)
		return (TRUE);
	else {
		for (index = 1; index <= objects; index++) {
			if ((object[index]->attributes & LUMINOUS)
				&& scope(index, "*present", FALSE))
				return (TRUE);
		}
	}
	return (FALSE);
}

char *
sentence_output(int index, int capital)
{
	const char *def;
	struct string_type *override;

	if (!valid_object(index)) { temp_buffer[0] = 0; return temp_buffer; }
	def = object[index]->definite;

	/* If the object still has the loader default "the" and the game
	 * has set a DEFAULT_DEFINITE constant (e.g. "none" in Indonesian),
	 * use the game's value instead. This lets a single compiled
	 * interpreter serve games in any language. */
	if (!strcmp(def, "the")) {
		override = cstring_resolve("DEFAULT_DEFINITE");
		if (override != NULL) {
			def = override->value;
		}
	}

	if (!strcmp(object[index]->article, "name")) {
		strcpy(temp_buffer, object[index]->inventory);
	} else if (def[0] == '\0' || !strcmp(def, "none")) {
		strcpy(temp_buffer, object[index]->inventory);
	} else if (def[0] == '-') {
		strcpy(temp_buffer, object[index]->inventory);
		strcat(temp_buffer, &def[1]);
	} else {
		strcpy(temp_buffer, def);
		strcat(temp_buffer, " ");
		strcat(temp_buffer, object[index]->inventory);
	}

	if (capital && (unsigned char) temp_buffer[0] < 0x80)
		temp_buffer[0] = toupper((unsigned char) temp_buffer[0]);

	return (temp_buffer);
}

char *
isnt_output(int index)
{
	if (!valid_object(index)) return (char *) safe_cstr("ISNT", "isn't");
	if (object[index]->attributes & PLURAL)
		return (char *) safe_cstr("ARENT", "aren't");
	else
		return (char *) safe_cstr("ISNT", "isn't");
}

char *
is_output(int index)
{
	if (!valid_object(index)) return (char *) safe_cstr("IS", "is");
	if (object[index]->attributes & PLURAL)
		return (char *) safe_cstr("ARE", "are");
	else
		return (char *) safe_cstr("IS", "is");
}

char *
sub_output(int index, int capital)
{
	if (!valid_object(index)) { temp_buffer[0] = 0; return temp_buffer; }
	if (object[index]->attributes & PLURAL) {
		strcpy(temp_buffer, safe_cstr("THEY_WORD", "they"));
	} else {
		if (index == player) {
			strcpy(temp_buffer, safe_cstr("YOU_WORD", "you"));
		} else if (object[index]->attributes & ANIMATE) {
			if (object[index]->attributes & FEMALE) {
				strcpy(temp_buffer, safe_cstr("SHE_WORD", "she"));
			} else {
				strcpy(temp_buffer, safe_cstr("HE_WORD", "he"));
			}
		} else {
			strcpy(temp_buffer, safe_cstr("IT_WORD", "it"));
		}
	}

	if (capital && (unsigned char) temp_buffer[0] < 0x80)
		temp_buffer[0] = toupper((unsigned char) temp_buffer[0]);

	return temp_buffer;
}

char *
obj_output(int index, int capital)
{
	if (!valid_object(index)) { temp_buffer[0] = 0; return temp_buffer; }
	if (object[index]->attributes & PLURAL) {
		strcpy(temp_buffer, safe_cstr("THEM_WORD", "them"));
	} else {
		if (index == player) {
			strcpy(temp_buffer, safe_cstr("YOURSELF_WORD", "yourself"));
		} else if (object[index]->attributes & ANIMATE) {
			if (object[index]->attributes & FEMALE) {
				strcpy(temp_buffer, safe_cstr("HER_WORD", "her"));
			} else {
				strcpy(temp_buffer, safe_cstr("HIM_WORD", "him"));
			}
		} else {
			strcpy(temp_buffer, safe_cstr("IT_WORD", "it"));
		}
	}

	if (capital && (unsigned char) temp_buffer[0] < 0x80)
		temp_buffer[0] = toupper((unsigned char) temp_buffer[0]);

	return temp_buffer;
}

char *
it_output(int index)
{
	/* {it} contract per the Author's Guide: prints "they" if PLURAL,
	 * "he" if ANIMATE, "she" if ANIMATE+FEMALE, "it" otherwise.
	 * Previously the ANIMATE branch called sentence_output(), which
	 * returns the object's full noun phrase ("the man"), not a
	 * pronoun -- a long-standing bug visible to every game that
	 * used a `noun{it}` macro on an NPC. */
	if (!valid_object(index)) return (char *) safe_cstr("IT_WORD", "it");
	if (object[index]->attributes & PLURAL) {
		return (char *) safe_cstr("THEY_WORD", "they");
	} else if (object[index]->attributes & ANIMATE) {
		if (object[index]->attributes & FEMALE) {
			return (char *) safe_cstr("SHE_WORD", "she");
		} else {
			return (char *) safe_cstr("HE_WORD", "he");
		}
	} else {
		return (char *) safe_cstr("IT_WORD", "it");
	}
}

char *
that_output(int index, int capital)
{
	if (!valid_object(index)) { temp_buffer[0] = 0; return temp_buffer; }
	if (object[index]->attributes & PLURAL) {
		strcpy(temp_buffer, safe_cstr("THOSE_WORD", "those"));
	} else {
		strcpy(temp_buffer, safe_cstr("THAT_WORD", "that"));
	}

	if (capital && (unsigned char) temp_buffer[0] < 0x80)
		temp_buffer[0] = toupper((unsigned char) temp_buffer[0]);

	return temp_buffer;
}

char *
doesnt_output(int index)
{
	if (!valid_object(index)) return (char *) safe_cstr("DOESNT", "doesn't");
	if (object[index]->attributes & PLURAL)
		return (char *) safe_cstr("DONT", "don't");
	else
		return (char *) safe_cstr("DOESNT", "doesn't");
}

char *
does_output(int index)
{
	if (!valid_object(index)) return (char *) safe_cstr("DOES", "does");
	if (object[index]->attributes & PLURAL)
		return (char *) safe_cstr("DO", "do");
	else
		return (char *) safe_cstr("DOES", "does");
}

char *
list_output(int index, int capital)
{
	if (!valid_object(index)) { temp_buffer[0] = 0; return temp_buffer; }
	if (!strcmp(object[index]->article, "name")) {
		strcpy(temp_buffer, object[index]->inventory);
	} else {
		strcpy(temp_buffer, object[index]->article);
		strcat(temp_buffer, " ");
		strcat(temp_buffer, object[index]->inventory);
	}

	if (capital && (unsigned char) temp_buffer[0] < 0x80)
		temp_buffer[0] = toupper((unsigned char) temp_buffer[0]);

	return (temp_buffer);
}

char *
plain_output(int index, int capital)
{
	if (!valid_object(index)) { temp_buffer[0] = 0; return temp_buffer; }
	strcpy(temp_buffer, object[index]->inventory);

	if (capital && (unsigned char) temp_buffer[0] < 0x80)
		temp_buffer[0] = toupper((unsigned char) temp_buffer[0]);

	return (temp_buffer);
}

char *
long_output(int index)
{
	if (!valid_object(index)) { temp_buffer[0] = 0; return temp_buffer; }
	if (!strcmp(object[index]->described, "function")) {
		strcpy(function_name, "long_");
		strcat(function_name, object[index]->label);
		if (execute(function_name) == FALSE) {
			unkfunrun(function_name);
		}

		// THE BUFFER IS RETURNED EMPTY AS THE TEXT IS OUTPUT BY
		// WRITE STATEMENTS IN THE FUNCTION CALLED
		temp_buffer[0] = 0;
		return (temp_buffer);
	} else {
		return (object[index]->described);
	}
}

void
no_it()
{
	write_text(safe_cstr("NO_IT", "I don't see "));
	write_text(word[wp]);
	write_text(safe_cstr("NO_IT_END", " here."));
	custom_error = TRUE;
}

void
look_around()
{
	/* THIS FUNCTION DISPLAYS THE DESCRIPTION OF THE CURRENT LOCATION ALONG
	 * WITH ANY OBJECTS CURRENTLY IN IT */

	if (!check_light(HERE)) {
		/* THE CURRENT LOCATION HAS 'DARK' AND NO SOURCE OF LIGHT IS
		 * CURRENTLY PRESENT */
		execute("+dark_description");
		return;
	}

	if (execute("+before_look") != FALSE)
		return;

	execute("+title");

	if (DISPLAY_MODE->value) {
		/* THE INTERPRETER IS IN VERBOSE MODE SO TEMPORARILY TAKE AWAYS THE
		 * 'VISITED' ATTRIBUTE */
		object[HERE]->attributes &= ~1L;
	}

	strcpy(function_name, "look_");
	strcat(function_name, object[HERE]->label);
	execute(function_name);

	/* GIVE THE LOCATION THE ATTRIBUTES 'VISITED', 'KNOWN', AND 'MAPPED' NOW 
     * THAT THE LOOK FUNCTION HAS RUN */
	object[HERE]->attributes = object[HERE]->attributes | KNOWN;
	object[HERE]->attributes = object[HERE]->attributes | VISITED;
	object[HERE]->attributes = object[HERE]->attributes | MAPPED;

	execute("+object_descriptions");

	strcpy(function_name, "after_look_");
	strcat(function_name, object[HERE]->label);
	execute(function_name);

	execute("+after_look");
}
