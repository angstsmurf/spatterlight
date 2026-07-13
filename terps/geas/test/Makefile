# Build the headless geas walkthrough/regression runner and the unit tests.
#
#   make            build ./geas_walkthrough_runner
#   make check      build both, then run the unit tests and the fixture games
#   make asan       same, with AddressSanitizer/UBSan (several of the bugs the
#                   fixtures guard are undefined behaviour that a plain -O2
#                   build does not fault on -- it just returns garbage)
#   make clean      remove the binaries
#
# The binaries unity-include the engine sources from the parent directory; their
# own quoted includes resolve relative to that directory, so -I.. is enough.

CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -w -I..
ASANFLAGS := -std=c++17 -O1 -g -w -I.. -fsanitize=address,undefined

BIN := geas_walkthrough_runner
SRC := geas_walkthrough_runner.cc
TESTBIN := geas_unit_tests
TESTSRC := geas_unit_tests.cc
DEPS := $(wildcard ../*.cc ../*.hh)

all: $(BIN) $(TESTBIN)

$(BIN): $(SRC) $(DEPS)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

$(TESTBIN): $(TESTSRC) $(DEPS)
	$(CXX) $(CXXFLAGS) $(TESTSRC) -o $@

check: $(BIN) $(TESTBIN)
	./$(TESTBIN)
	./run_fixtures.sh

asan: $(SRC) $(TESTSRC) $(DEPS)
	$(CXX) $(ASANFLAGS) $(TESTSRC) -o $(TESTBIN)
	$(CXX) $(ASANFLAGS) $(SRC) -o $(BIN)
	./$(TESTBIN)
	./run_fixtures.sh

clean:
	rm -f $(BIN) $(TESTBIN)

.PHONY: all check asan clean
