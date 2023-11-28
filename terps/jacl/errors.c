/* errors.c --- Error messages.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"

extern struct function_type		*executing_function;
extern char           			*word[];

extern char						error_buffer[1024];

void
badparrun()
{
	snprintf(error_buffer, sizeof(error_buffer), BAD_PARENT, executing_function->name);

	log_error(error_buffer, PLUS_STDERR);
}

void
notintrun()
{
	snprintf(error_buffer, sizeof(error_buffer), NOT_INTEGER, executing_function->name, word[0]);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkfunrun(char *name)
{
	snprintf(error_buffer, sizeof(error_buffer), UNKNOWN_FUNCTION_RUN, name);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkkeyerr(int line, int wordno)
{
	snprintf(error_buffer, sizeof(error_buffer), UNKNOWN_KEYWORD_ERR, line, word[wordno]);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkatterr(int line, int wordno)
{
	snprintf(error_buffer, sizeof(error_buffer), UNKNOWN_ATTRIBUTE_ERR, line,
			word[wordno]);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkvalerr(int line, int wordno)
{
	snprintf(error_buffer, sizeof(error_buffer), UNKNOWN_VALUE_ERR, line,
			word[wordno]);
	log_error(error_buffer, PLUS_STDERR);
}

void
noproprun(void)
{
	snprintf(error_buffer, sizeof(error_buffer), INSUFFICIENT_PARAMETERS_RUN, executing_function->name, word[0]);
	log_error(error_buffer, PLUS_STDOUT);
}

void
noobjerr(int line)
{
	snprintf(error_buffer, sizeof(error_buffer), NO_OBJECT_ERR,
			line, word[0]);
	log_error(error_buffer, PLUS_STDERR);
}

void
noproperr(int line)
{
	snprintf(error_buffer, sizeof(error_buffer), INSUFFICIENT_PARAMETERS_ERR,
			line, word[0]);
	log_error(error_buffer, PLUS_STDERR);
}

void
nongloberr(int line)
{
	snprintf(error_buffer, sizeof(error_buffer), NON_GLOBAL_FIRST, line);
	log_error(error_buffer, PLUS_STDERR);
}

void
nofnamerr(int line)
{
	snprintf(error_buffer, sizeof(error_buffer), NO_NAME_FUNCTION, line);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkobjerr(int line, int wordno)
{
	snprintf(error_buffer, sizeof(error_buffer), UNDEFINED_ITEM_ERR, line, word[wordno]);
	log_error(error_buffer, PLUS_STDERR);
}

void
maxatterr(int line, int wordno)
{
	snprintf(error_buffer, sizeof(error_buffer),
			MAXIMUM_ATTRIBUTES_ERR, line, word[wordno]);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkobjrun(int wordno)
{
	snprintf(error_buffer, sizeof(error_buffer), UNDEFINED_ITEM_RUN, executing_function->name, word[wordno]);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkattrun(int wordno)
{
	snprintf(error_buffer, sizeof(error_buffer), UNKNOWN_ATTRIBUTE_RUN, executing_function->name, word[wordno]);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkdirrun(int wordno)
{
	snprintf(error_buffer, sizeof(error_buffer), UNDEFINED_DIRECTION_RUN,
			executing_function->name, word[wordno]);
	log_error(error_buffer, PLUS_STDOUT);
}

void
badparun()
{
	snprintf(error_buffer, sizeof(error_buffer), BAD_PARENT, executing_function->name);
	log_error(error_buffer, PLUS_STDOUT);
}

void
badplrrun(int value)
{
	snprintf(error_buffer, sizeof(error_buffer), BAD_PLAYER, executing_function->name, value);
	log_error(error_buffer, PLUS_STDOUT);
}

void
badptrrun(char *name, int value)
{
	snprintf(error_buffer, sizeof(error_buffer), BAD_POINTER, executing_function->name, name, value);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkvarrun(char *variable)
{
	snprintf(error_buffer, sizeof(error_buffer), UNDEFINED_CONTAINER_RUN, executing_function->name, arg_text_of(variable));
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkstrrun(char *variable)
{
	snprintf(error_buffer, sizeof(error_buffer), UNDEFINED_STRING_RUN, executing_function->name, variable);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkscorun(char *scope)
{
	snprintf(error_buffer, sizeof(error_buffer), UNKNOWN_SCOPE_RUN, executing_function->name, scope);
	log_error(error_buffer, PLUS_STDOUT);
}

void
totalerrs(int errors)
{
	if (errors == 1)
		snprintf(error_buffer, sizeof(error_buffer), ERROR_DETECTED);
	else {
		snprintf(error_buffer, sizeof(error_buffer), ERRORS_DETECTED, errors);
	}

	log_error(error_buffer, PLUS_STDERR);
}

void
outofmem()
{
	log_error(OUT_OF_MEMORY, PLUS_STDERR);
	terminate(49);
}

