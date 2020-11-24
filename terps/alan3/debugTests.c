#include "cgreen/cgreen.h"
#include "cgreen/mocks.h"

#include "debug.c"

Describe(Debug);
BeforeEach(Debug) {}
AfterEach(Debug) {}

Ensure(Debug, parseDebugCommandReturnsHELP_COMMANDForHelp) {
	assert_equal(HELP_COMMAND, parseDebugCommand("Help"));
}

Ensure(Debug, parseDebugCommandReturnsHELP_COMMANDForQuestionMark) {
	assert_equal(HELP_COMMAND, parseDebugCommand("?"));
}

Ensure(Debug, parseDebugCommandReturnsHELP_COMMANDForH) {
	assert_equal(HELP_COMMAND, parseDebugCommand("H"));
}

Ensure(Debug, parseDebugCommandReturnsEXIT_COMMANDForX) {
	assert_equal(EXIT_COMMAND, parseDebugCommand("X"));
}

Ensure(Debug, parseDebugCommandReturnsBREAK_COMMANDForBr) {
	assert_equal(BREAK_COMMAND, parseDebugCommand("Br"));
}

Ensure(Debug, parseDebugCommandReturnsAMBIGUOUS_COMMANDForE) {
	assert_equal(AMBIGUOUS_COMMAND, parseDebugCommand("E"));
}

Ensure(Debug, findSourceLineIndexFindsSameLineInOtherFiles) {
	SourceLineEntry lineTable[] = {
		{0, 3},
		{0, 5},
		{0, 35},
		{1, 33},
		{2, 35},
		{EOF, EOF}
	};
	assert_equal(findSourceLineIndex(lineTable, 0, 3), 0);
	assert_equal(findSourceLineIndex(lineTable, 0, 5), 1);
	assert_equal(findSourceLineIndex(lineTable, 0, 35), 2);
	assert_equal(findSourceLineIndex(lineTable, 1, 33), 3);
	assert_equal(findSourceLineIndex(lineTable, 2, 35), 4);
}
