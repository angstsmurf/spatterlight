# Build the headless geas walkthrough/regression runner.
#
#   make            build ./geas_walkthrough_runner
#   make clean      remove it
#
# The runner unity-includes the engine sources from the parent directory; their
# own quoted includes resolve relative to that directory, so -I.. is enough.

CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -w -I..

BIN := geas_walkthrough_runner
SRC := geas_walkthrough_runner.cc
DEPS := $(wildcard ../*.cc ../*.hh)

$(BIN): $(SRC) $(DEPS)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

clean:
	rm -f $(BIN)

.PHONY: clean
