/* Comprehend engine glue: text output, the status window, transcripts and
 * keyboard / script input. */

#include "comprehend.h"
#include "comprehend_internal.h"

namespace Glk {
namespace Comprehend {

void Comprehend::putBottom(const char *s) {
	// In redirect mode, send the text to the transcript only (or nowhere, if no
	// transcript is open); otherwise to the buffer window. Each destination has
	// its own trailing-newline counter so their collapse states stay separate.
	strid_t stream;
	int *trailing;
	if (_redirectToTranscript) {
		if (!_transcript) return;
		stream = _transcript;
		trailing = &_transcriptTrailingNewlines;
	} else {
		stream = glk_window_get_stream(_bottomWindow);
		trailing = &_trailingNewlines;
	}
	for (; *s; ++s) {
		if (*s == '\n') {
			if (*trailing >= 2)
				continue;  // already a blank line; collapse further newlines
			++*trailing;
		} else {
			*trailing = 0;
		}
		glk_put_char_stream(stream, (unsigned char)*s);
	}
}

void Comprehend::putBottomUni(const glui32 *s) {
	strid_t stream;
	int *trailing;
	if (_redirectToTranscript) {
		if (!_transcript) return;
		stream = _transcript;
		trailing = &_transcriptTrailingNewlines;
	} else {
		stream = glk_window_get_stream(_bottomWindow);
		trailing = &_trailingNewlines;
	}
	for (; *s; ++s) {
		if (*s == '\n') {
			if (*trailing >= 2)
				continue;  // already a blank line; collapse further newlines
			++*trailing;
		} else {
			*trailing = 0;
		}
		glk_put_char_stream_uni(stream, *s);
	}
}

void Comprehend::transcript(const char *arg) {
	// "#transcript off" stops; "#transcript [on]" starts; bare "#transcript"
	// toggles. (Mirrors the scott/taylor/plus metacommand behaviour.)
	while (*arg == ' ') ++arg;
	if (scumm_stricmp(arg, "off") == 0)
		transcriptOff();
	else if (scumm_stricmp(arg, "on") == 0)
		transcriptOn();
	else if (_transcript)
		transcriptOff();
	else
		transcriptOn();
}

void Comprehend::transcriptOn() {
	if (_transcript) {
		print("A transcript is already active.\n");
		return;
	}

	frefid_t ref = glk_fileref_create_by_prompt(
		fileusage_TextMode | fileusage_Transcript, filemode_Write, 0);
	if (!ref)
		return;

	_transcript = glk_stream_open_file_uni(ref, filemode_Write, 0);
	glk_fileref_destroy(ref);

	if (!_transcript) {
		print("Failed to create transcript file.\n");
		return;
	}

	glk_put_string_stream(_transcript, (char *)"Start of transcript\n\n");
	_transcriptTrailingNewlines = 2;  // header ended with a blank line
	print("Transcript is now on.\n");
	glk_window_set_echo_stream(_bottomWindow, _transcript);

	// Record the room the player is currently standing in, so the transcript
	// doesn't begin mid-room with no description (matches scott's behaviour).
	if (_game)
		_game->transcribeCurrentRoom();
}

void Comprehend::transcriptOff() {
	if (!_transcript) {
		print("No transcript is currently running.\n");
		return;
	}

	glk_window_set_echo_stream(_bottomWindow, nullptr);
	glk_put_string_stream(_transcript, (char *)"\n\nEnd of transcript\n");
	glk_stream_close(_transcript, nullptr);
	_transcript = nullptr;
	print("Transcript is now off.\n");
}

void Comprehend::setCentered(bool on) {
	// Toggle the centred justification style on the buffer window (used for the
	// title credits). Routed through the same stream putBottom() writes to, so
	// subsequent console_println() output is centred until switched back.
	glk_set_style_stream(glk_window_get_stream(_bottomWindow),
						 on ? style_User1 : style_Normal);
}

void Comprehend::setPreformatted(bool on) {
	// Toggle a fixed-width (monospace) style on the buffer window. Used for the
	// few game strings that are hand-drawn ASCII boxes (e.g. Talisman's
	// "departing the Persian Empire" sign): only a monospace font with the
	// original spacing preserved keeps their dash rules and '!' borders aligned.
	glk_set_style_stream(glk_window_get_stream(_bottomWindow),
						 on ? style_Preformatted : style_Normal);
}

void Comprehend::print(const char *fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	Common::String msg = Common::String::vformat(fmt, argp);
	va_end(argp);
	putBottom(msg.c_str());
}

void Comprehend::print_u32_internal(const Common::U32String *fmt, ...) {
	Common::U32String out;
	va_list argp;
	va_start(argp, fmt);
	Common::U32String::vformat(fmt->begin(), fmt->end(), out, argp);
	va_end(argp);

	// Spatterlight's Glk supports glk_put_string_stream_uni; output 32-bit
	// codepoints directly.
	putBottomUni((glui32 *)out.u32_str());
}

void Comprehend::printRoomDesc(const Common::String &desc) {
	if (!_statusWindow) return;

	// Remember it so onArrange() can re-wrap it after a resize.
	_lastRoomDesc = desc;

	glui32 width = 0;
	glk_window_get_size(_statusWindow, &width, nullptr);
	Common::String str = desc;
	if (width > 2)
		str.wordWrap(width - 2);
	str += '\n';

	// Size the window to the wrapped text plus one blank line at the bottom,
	// used as a delimiter (as UnQuill and others do), rather than leaving it at
	// a fixed height with empty rows below the description.
	int lines = 0;
	for (uint i = 0; i < str.size(); ++i)
		if (str[i] == '\n') ++lines;
	winid_t parent = glk_window_get_parent(_statusWindow);
	if (parent)
		glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
								   (glui32)(lines + 1), _statusWindow);

	glk_window_clear(_statusWindow);

	while (!str.empty()) {
		size_t idx = str.findFirstOf('\n');
		if (idx == std::string::npos) idx = str.size() - 1;
		Common::String line = Common::String::format(" %s",
			Common::String(str.c_str(), str.c_str() + idx + 1).c_str());
		glk_put_string_stream(glk_window_get_stream(_statusWindow),
							  (char *)line.c_str());
		str = Common::String(str.c_str() + idx + 1);
	}

	// Fill the bottom row with underscores as a delimiter (as UnQuill does).
	glk_window_move_cursor(_statusWindow, 0, (glui32)lines);
	strid_t stream = glk_window_get_stream(_statusWindow);
	for (glui32 col = 0; col < width; ++col)
		glk_put_char_stream(stream, '_');
}

static FILE *scriptFile() {
	static FILE *f = nullptr;
	static bool tried = false;
	if (!tried) {
		tried = true;
		const char *p = getenv("COMPREHEND_SCRIPT");
		if (p) f = fopen(p, "r");
	}
	return f;
}

void Comprehend::readLine(char *buffer, size_t maxLen) {
	if (scriptFile()) {
#ifdef COMPREHEND_HEADLESS
		maybeStartHourglassFall();   // picture log only; see drainAnimations()
#endif
		if (!fgets(buffer, (int)maxLen, scriptFile())) {
			// End of transcript: terminate the process outright. Feeding "quit"
			// here is not safe -- some games (e.g. Talisman) answer "quit" with
			// a Y/N confirmation read through readChar(), which in script mode
			// returns a space, declining the quit. The turn loop would then ask
			// for another line, hit EOF again, and spin forever emitting output.
			glk_exit();
		}
		size_t n = strlen(buffer);
		while (n && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r'))
			buffer[--n] = 0;
		glk_put_string(buffer);
		glk_put_char('\n');
		_trailingNewlines = 1;
		return;
	}
	maybeStartHourglassFall();
	event_t ev;
	glk_request_line_event(_bottomWindow, buffer, (glui32)(maxLen - 1), 0);
	for (;;) {
		glk_select(&ev);
		if (ev.type == evtype_LineInput) break;
		if (ev.type == evtype_Timer) {
			if (_slowDrawActive) tickSlowDraw();
			if (_hourglassFalling) tickHourglass();
		} else if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
			onArrange();
	}
	buffer[ev.val1] = 0;
	// Glk echoes the typed line plus a single newline to the buffer; reflect
	// that one trailing newline so the count stays accurate across the input.
	_trailingNewlines = 1;
}

int Comprehend::readChar() {
	if (scriptFile())
		return ' ';
	maybeStartHourglassFall();
	glk_request_char_event(_bottomWindow);
	setDisableSaves(true);
	event_t ev;
	ev.type = evtype_None;
	while (ev.type != evtype_CharInput) {
		glk_select(&ev);
		if (ev.type == evtype_Timer) {
			if (_slowDrawActive) tickSlowDraw();
			if (_hourglassFalling) tickHourglass();
		} else if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
			onArrange();
	}
	setDisableSaves(false);
	return ev.val1;
}

} // namespace Comprehend
} // namespace Glk
