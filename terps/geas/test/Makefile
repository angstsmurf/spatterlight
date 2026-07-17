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

# Quest 5 (.aslx / .quest) milestone-1 loader test. It compiles ../aslx.cc via a
# unity include and links libexpat + zlib (the loader's only external deps).
ASLXBIN := aslx_loader_test
ASLXSRC := aslx_loader_test.cc
ASLXDEPS := ../aslx.cc ../aslx.hh
ASLXLIBS := -lexpat -lz

# Quest 5 milestone-2 script/expression runtime test.
ASLXRTBIN := aslx_runtime_test
ASLXRTSRC := aslx_runtime_test.cc
ASLXRTDEPS := ../aslx.cc ../aslx.hh ../aslx-runtime.cc ../aslx-runtime.hh \
              ../aslx-runtime-parse.inc ../aslx-runtime-builtins.inc \
              ../aslx-state.inc

# Quest 5 native walkthrough replayer (diff against quest5-oracle/golden/).
# Needs the local game corpus, so it is built on demand and not part of check.
ASLXREPLAY := aslx_replay

# Quest 5 Glk frontend smoke harness: the real aslxglk.cc against the in-repo
# CheapGlk, driven by piping commands to stdin. Built on demand.
ASLXGLK := aslxglk_smoke
CHEAPGLK := ../../cheapglk

all: $(BIN) $(TESTBIN) $(ASLXBIN) $(ASLXRTBIN)

$(BIN): $(SRC) $(DEPS)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

$(TESTBIN): $(TESTSRC) $(DEPS)
	$(CXX) $(CXXFLAGS) $(TESTSRC) -o $@

$(ASLXBIN): $(ASLXSRC) $(ASLXDEPS)
	$(CXX) $(CXXFLAGS) $(ASLXSRC) $(ASLXLIBS) -o $@

$(ASLXRTBIN): $(ASLXRTSRC) $(ASLXRTDEPS)
	$(CXX) $(CXXFLAGS) $(ASLXRTSRC) $(ASLXLIBS) -o $@

$(ASLXREPLAY): $(ASLXREPLAY).cc $(ASLXRTDEPS)
	$(CXX) $(CXXFLAGS) $(ASLXREPLAY).cc $(ASLXLIBS) -o $@

$(ASLXGLK): $(ASLXGLK).cc ../aslxglk.cc ../aslxglk-map.inc \
            ../questglk-common.inc $(ASLXRTDEPS)
	$(CXX) $(CXXFLAGS) -I$(CHEAPGLK) $(ASLXGLK).cc ../aslxglk.cc \
	    $(CHEAPGLK)/libcheapglk.a $(ASLXLIBS) -o $@

check: $(BIN) $(TESTBIN) $(ASLXBIN) $(ASLXRTBIN)
	./$(TESTBIN)
	./$(ASLXBIN)
	./$(ASLXRTBIN)
	./run_fixtures.sh

asan: $(SRC) $(TESTSRC) $(DEPS)
	$(CXX) $(ASANFLAGS) $(TESTSRC) -o $(TESTBIN)
	$(CXX) $(ASANFLAGS) $(SRC) -o $(BIN)
	$(CXX) $(ASANFLAGS) $(ASLXSRC) $(ASLXLIBS) -o $(ASLXBIN)
	$(CXX) $(ASANFLAGS) $(ASLXRTSRC) $(ASLXLIBS) -o $(ASLXRTBIN)
	./$(TESTBIN)
	./$(ASLXBIN)
	./$(ASLXRTBIN)
	./run_fixtures.sh

clean:
	rm -f $(BIN) $(TESTBIN) $(ASLXBIN) $(ASLXRTBIN)

.PHONY: all check asan clean
