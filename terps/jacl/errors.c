/* errors.c --- Error messages.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"
#include "encapsulate.h"

/* error_buffer is 1024 bytes (jacl.c globals). Every formatter
 * below previously used sprintf with no length cap; a player
 * command containing a long quoted token meant `word[wordno]`
 * could be hundreds of chars, plus a function name (84) plus a
 * format string, easily overrunning the destination. ERRBUF_SIZE
 * is the snprintf cap we share across this file. */
#define ERRBUF_SIZE 1024

/* `executing_function` is set by the parser before running user
 * code, but error formatters can fire from many code paths (init,
 * loader, restart_game). A NULL deref here would mask the real
 * error with a SIGSEGV. */
static const char *
fn_name(void)
{
	if (executing_function == NULL || executing_function->name[0] == 0) {
		return "<no function>";
	}
	return executing_function->name;
}

/* `word[]` is bounded by MAX_WORDS; entries beyond the parsed
 * command are NULL. Some error sites pass a wordno that came from
 * grammar-table state and could exceed the player's actual word
 * count. Without a guard, sprintf %s on NULL is undefined and
 * crashes on glibc. */
static const char *
safe_word(int wordno)
{
	if (wordno < 0 || wordno >= MAX_WORDS || word[wordno] == NULL) {
		return "<eof>";
	}
	return word[wordno];
}

void
badparrun()
{
	snprintf(error_buffer, ERRBUF_SIZE, BAD_PARENT, fn_name());

	log_error(error_buffer, PLUS_STDERR);
}

void
notintrun()
{
	snprintf(error_buffer, ERRBUF_SIZE, NOT_INTEGER, fn_name(), safe_word(0));
	log_error(error_buffer, PLUS_STDERR);
}

void
unkfunrun(const char *name)
{
	snprintf(error_buffer, ERRBUF_SIZE, UNKNOWN_FUNCTION_RUN,
		name ? name : "<null>");
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkkeyerr(int line, int wordno)
{
	snprintf(error_buffer, ERRBUF_SIZE, UNKNOWN_KEYWORD_ERR, line, safe_word(wordno));
	log_error(error_buffer, PLUS_STDERR);
}

void
unkatterr(int line, int wordno)
{
	snprintf(error_buffer, ERRBUF_SIZE, UNKNOWN_ATTRIBUTE_ERR, line,
		safe_word(wordno));
	log_error(error_buffer, PLUS_STDERR);
}

void
unkvalerr(int line, int wordno)
{
	snprintf(error_buffer, ERRBUF_SIZE, UNKNOWN_VALUE_ERR, line,
		safe_word(wordno));
	log_error(error_buffer, PLUS_STDERR);
}

void
noproprun()
{
	snprintf(error_buffer, ERRBUF_SIZE, INSUFFICIENT_PARAMETERS_RUN, fn_name(), safe_word(0));
	log_error(error_buffer, PLUS_STDOUT);
}

void
noobjerr(int line)
{
	snprintf(error_buffer, ERRBUF_SIZE, NO_OBJECT_ERR,
		line, safe_word(0));
	log_error(error_buffer, PLUS_STDERR);
}

void
noproperr(int line)
{
	snprintf(error_buffer, ERRBUF_SIZE, INSUFFICIENT_PARAMETERS_ERR,
		line, safe_word(0));
	log_error(error_buffer, PLUS_STDERR);
}

void
nongloberr(int line)
{
	snprintf(error_buffer, ERRBUF_SIZE, NON_GLOBAL_FIRST, line);
	log_error(error_buffer, PLUS_STDERR);
}

void
nofnamerr(int line)
{
	snprintf(error_buffer, ERRBUF_SIZE, NO_NAME_FUNCTION, line);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkobjerr(int line, int wordno)
{
	snprintf(error_buffer, ERRBUF_SIZE, UNDEFINED_ITEM_ERR, line, safe_word(wordno));
	log_error(error_buffer, PLUS_STDERR);
}

void
maxatterr(int line, int wordno)
{
	snprintf(error_buffer, ERRBUF_SIZE,
		MAXIMUM_ATTRIBUTES_ERR, line, safe_word(wordno));
	log_error(error_buffer, PLUS_STDERR);
}

void
unkobjrun(int wordno)
{
	snprintf(error_buffer, ERRBUF_SIZE, UNDEFINED_ITEM_RUN, fn_name(), safe_word(wordno));
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkattrun(int wordno)
{
	snprintf(error_buffer, ERRBUF_SIZE, UNKNOWN_ATTRIBUTE_RUN, fn_name(), safe_word(wordno));
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkdirrun(int wordno)
{
	snprintf(error_buffer, ERRBUF_SIZE, UNDEFINED_DIRECTION_RUN,
		fn_name(), safe_word(wordno));
	log_error(error_buffer, PLUS_STDOUT);
}

void
badparun(void)
{
	snprintf(error_buffer, ERRBUF_SIZE, BAD_PARENT, fn_name());
	log_error(error_buffer, PLUS_STDOUT);
}

void
badplrrun(int value)
{
	snprintf(error_buffer, ERRBUF_SIZE, BAD_PLAYER, fn_name(), value);
	log_error(error_buffer, PLUS_STDOUT);
}

void
badptrrun(const char *name, int value)
{
	snprintf(error_buffer, ERRBUF_SIZE, BAD_POINTER, fn_name(),
		name ? name : "<null>", value);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkvarrun(const char *variable)
{
	const char *resolved = arg_text_of(variable);
	snprintf(error_buffer, ERRBUF_SIZE, UNDEFINED_CONTAINER_RUN, fn_name(),
		resolved ? resolved : "<null>");
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkstrrun(const char *variable)
{
	snprintf(error_buffer, ERRBUF_SIZE, UNDEFINED_STRING_RUN, fn_name(),
		variable ? variable : "<null>");
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkscorun(const char *scope)
{
	snprintf(error_buffer, ERRBUF_SIZE, UNKNOWN_SCOPE_RUN, fn_name(),
		scope ? scope : "<null>");
	log_error(error_buffer, PLUS_STDOUT);
}

void
totalerrs(int errors)
{
	if (errors == 1)
		snprintf(error_buffer, ERRBUF_SIZE, ERROR_DETECTED);
	else {
		snprintf(error_buffer, ERRBUF_SIZE, ERRORS_DETECTED, errors);
	}

	log_error(error_buffer, PLUS_STDERR);
}

_Noreturn void
outofmem()
{
	log_error(OUT_OF_MEMORY, PLUS_STDERR);
	terminate(49);
}

