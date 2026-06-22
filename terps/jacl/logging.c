/* logging.c --- Logging functions
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "types.h"
#include "prototypes.h"
#include "language.h"

#ifdef __NDS__
void
log_error(const char *message, int console)
{
	/* consoleMessage is small (256B) but `message` is typically
	 * error_buffer[1024], so the prior sprintf could overflow on a
	 * long error string. snprintf truncates instead. */
	char 			consoleMessage[256];

	snprintf(consoleMessage, sizeof(consoleMessage), "ERROR: %s^", message);

	write_text(consoleMessage);
}
#endif
#ifdef GLK
void
log_error(const char *message, int console)
{
	/* LOG A MESSAGE TO THE CONSOLE */

	char 			consoleMessage[256];
    event_t			event;

	/* See __NDS__ branch above re: snprintf-vs-sprintf rationale. */
	snprintf(consoleMessage, sizeof(consoleMessage), "ERROR: %s^", message);

	glk_set_style(style_Alert);
	write_text(consoleMessage);
	glk_set_style(style_Normal);

	// FLUSH THE GLK WINDOW SO THE ERROR GETS DISPLAYED IMMEDIATELY.
    glk_select_poll(&event);
}
#endif
#ifndef GLK
#ifndef __NDS__
extern char						error_log[];
extern char						access_log[];

void
log_access(const char *message)
{
	/* LOG A MESSAGE TO THE ACCESS LOG */

	FILE           *accessLog = fopen(access_log, "a");

	if (accessLog != NULL) {
		fputs(message, accessLog);
		fflush(accessLog);
		fclose(accessLog);
	}
}

void
log_error(const char *message, int console)
{
	FILE           *errorLog = fopen(error_log, "a");
	time_t          tnow;
	struct tm       tm_local;
	char            timebuf[32];
	char 			consoleMessage[256];
	char 			temp_buffer[256];

	time(&tnow);
	/* ctime() returns a pointer into a static internal buffer that
	 * is shared with localtime/gmtime; under fcgijacl with multiple
	 * workers (or any future threaded build) one worker's strip_
	 * return on that pointer can corrupt another worker's read.
	 * Use the reentrant localtime_r + strftime instead. */
	if (localtime_r(&tnow, &tm_local) != NULL) {
		strftime(timebuf, sizeof timebuf,
			"%a %b %d %H:%M:%S %Y", &tm_local);
	} else {
		snprintf(timebuf, sizeof timebuf, "%ld", (long) tnow);
	}

	/* Both destinations are 256B but `message` is typically
	 * error_buffer[1024] and user_id / prefix are each up to 80
	 * chars. The prior sprintfs could blow the 256B stack buffers by
	 * ~1100 bytes on a long error string. snprintf truncates. */
	snprintf(temp_buffer, sizeof(temp_buffer), "%s - %s - %s - %s\n",
		timebuf, user_id, prefix, message);

	snprintf(consoleMessage, sizeof(consoleMessage), "%s: %s", prefix, message);

	if (console < ONLY_STDERR) {
		if (errorLog != NULL) {
			fputs(temp_buffer, errorLog);
			fflush(errorLog);
			fclose(errorLog);
		} else {
			/* If the configured error log can't be opened (bad
			 * path, no write permission on the dir, full disk),
			 * fall back to stderr so the operator notices.
			 * Previously the message vanished silently. */
			fprintf(stderr, "[log_error: cannot open %s] %s",
				error_log, temp_buffer);
			fflush(stderr);
		}
	}

	/* SEND THE MESSAGE TO STANDARD ERROR OR STANDARD OUT AS REQUIRED */
	if (console == PLUS_STDOUT || console == ONLY_STDOUT) {
		write_text(consoleMessage);
		write_text("^");
	} else if (console == PLUS_STDERR || console == ONLY_STDERR) {
		fprintf(stderr, "%s", consoleMessage);
		fprintf(stderr, "%s", "\n");
		fflush(stderr);
	}
}

void
log_debug(const char *message, int console)
{
	log_message(message, console);
}

void
log_message(const char *message, int console)
{
	/* Informational/debug messages are NOT written to error.log; that
	 * file is reserved for critical errors raised via log_error(). On a
	 * busy CGI host the per-request startup traffic was filling the disk.
	 * Stderr/stdout output is still honoured so log_message() remains a
	 * useful surface for interactive/console diagnostics. */

	if (console == 1) {
		write_text(message);
		write_text("^");
	} else if (console == 2) {
		fprintf(stderr, "%s\n", message);
		fflush(stderr);
	}
}
#endif
#endif
