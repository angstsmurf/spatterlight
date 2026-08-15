# Build the headless geas harnesses and the unit tests, for both engines.
#
#   make            build the Quest 4 walkthrough runner
#   make check      syntax-check every engine source, build the in-repo
#                   harnesses, then run the unit tests and the fixture games
#   make syntax     compile each engine source on its own, no linking (see the
#                   note above the target -- the harnesses cannot catch this)
#   make asan       same, with AddressSanitizer/UBSan (several of the bugs the
#                   fixtures guard are undefined behaviour that a plain -O2
#                   build does not fault on -- it just returns garbage)
#   make clean      remove the binaries
#
# The test tree is split by engine generation, and each harness builds beside
# its own sources:
#
#   quest4/harness/   Quest 4 (.asl/.cas) -- the geas engine
#   quest5/harness/   Quest 5 (.aslx/.quest) -- the aslx engine + its oracle
#   ./                engine-agnostic only (questglk_unit_tests, glkdrive.py)
#
# The binaries unity-include the engine sources from the geas root; their own
# quoted includes spell out the full relative path from wherever they live, so
# they build the same whether make is run from here or the source is compiled
# by hand.  Harnesses that open a fixture at run time resolve it relative to
# their own directory, so the run rules below cd into it first.

CXX      ?= clang++
CC       ?= clang
CXXFLAGS ?= -std=c++17 -O2 -w -I..
ASANFLAGS := -std=c++17 -O1 -g -w -I.. -fsanitize=address,undefined

Q4 := quest4/harness
Q5 := quest5/harness

BIN := $(Q4)/geas_walkthrough_runner
SRC := $(Q4)/geas_walkthrough_runner.cc
TESTBIN := $(Q4)/geas_unit_tests
TESTSRC := $(Q4)/geas_unit_tests.cc
DEPS = $(wildcard ../*.cc ../*.hh) $(RANDSRC)

# The engine draws its random numbers from erkyrath_random() -- xoshiro128**
# when seeded -- so that a seeded run gives the same numbers here, in the app
# and on any other platform, which is what makes the corpus transcripts
# diffable.  That lives in common_utils, and its header includes glk.h, hence
# the CheapGlk include path (no Glk function is called or linked).
# It is C, and the engine declares it extern "C", so it has to be compiled by
# the C driver and linked in as an object rather than fed to clang++.
RANDSRC := ../../common_utils/randomness.c
RANDOBJ := $(Q4)/randomness.o
RANDFLAGS = -I../../common_utils -I$(CHEAPGLK)

# Quest 5 (.aslx / .quest) milestone-1 loader test. It compiles ../quest5/aslx.cc via a
# unity include and links libexpat + zlib (the loader's only external deps).
ASLXBIN := $(Q5)/aslx_loader_test
ASLXSRC := $(Q5)/aslx_loader_test.cc
ASLXDEPS := ../quest5/aslx.cc ../quest5/aslx.hh
ASLXLIBS := -lexpat -lz

# Quest 5 milestone-2 script/expression runtime test.
ASLXRTBIN := $(Q5)/aslx_runtime_test
ASLXRTSRC := $(Q5)/aslx_runtime_test.cc
ASLXRTDEPS := ../quest5/aslx.cc ../quest5/aslx.hh ../quest5/aslx-runtime.cc ../quest5/aslx-runtime.hh \
              ../quest5/aslx-runtime-parse.inc ../quest5/aslx-runtime-builtins.inc \
              ../quest5/aslx-state.inc ../quest5/aslx-savenative.inc

# Quest 5 native walkthrough replayer (diff against quest5/goldens/).
# Needs the local game corpus, so it is built on demand and not part of check.
ASLXREPLAY := $(Q5)/aslx_replay

# Quest 5 native link reporter: replays a .cmd walkthrough like aslx_replay but
# prints the visible TEXT of the command link each step clicks (harvested from
# the Core text processor's <a data-command="..."> markup). Corpus-dependent,
# so built on demand and not part of check.
#   make quest5/harness/aslx_linkcapture
#   cd quest5/harness && ASLX_CORE=../../../quest5/aslx-core \
#       ./aslx_linkcapture "<game.quest>" "<script.cmd>"
ASLXLINKCAP := $(Q5)/aslx_linkcapture

# Quest 5 Glk frontend smoke harness: the real aslxglk.cc against the in-repo
# CheapGlk, driven by piping commands to stdin. Built on demand.
ASLXGLK := $(Q5)/aslxglk_smoke
CHEAPGLK := ../../cheapglk

# Unit tests for the helpers both Glk frontends share (../questglk-common.inc).
# Engine-agnostic, so it stays at the root of the test tree.  It includes the
# .inc directly and links CheapGlk for the glk_* symbols the other helpers in
# there reference.
QGLKBIN := questglk_unit_tests
QGLKSRC := questglk_unit_tests.cc

# Unit tests for the Quest 5 frontend's HTML link parser. It unity-includes
# aslxglk.cc (link_action is file-local) and links CheapGlk for the glk_*
# symbols the frontend references -- CheapGlk reports no hyperlink support, so
# the smoke harness never registers or clicks a link and cannot cover this.
ASLXLINKBIN := $(Q5)/aslxglk_link_tests
ASLXLINKSRC := $(Q5)/aslxglk_link_tests.cc

all: $(BIN) $(TESTBIN) $(ASLXBIN) $(ASLXRTBIN) $(QGLKBIN) $(ASLXLINKBIN)

$(RANDOBJ): $(RANDSRC)
	$(CC) -O2 -w $(RANDFLAGS) -c $(RANDSRC) -o $@

$(BIN): $(SRC) $(DEPS) $(RANDOBJ)
	$(CXX) $(CXXFLAGS) $(RANDFLAGS) $(SRC) $(RANDOBJ) -o $@

$(TESTBIN): $(TESTSRC) $(DEPS) $(RANDOBJ)
	$(CXX) $(CXXFLAGS) $(RANDFLAGS) $(TESTSRC) $(RANDOBJ) -o $@

$(ASLXBIN): $(ASLXSRC) $(ASLXDEPS)
	$(CXX) $(CXXFLAGS) $(ASLXSRC) $(ASLXLIBS) -o $@

$(ASLXRTBIN): $(ASLXRTSRC) $(ASLXRTDEPS)
	$(CXX) $(CXXFLAGS) $(ASLXRTSRC) $(ASLXLIBS) -o $@

$(QGLKBIN): $(QGLKSRC) ../questglk-common.inc
	$(CXX) $(CXXFLAGS) -I$(CHEAPGLK) $(QGLKSRC) $(CHEAPGLK)/libcheapglk.a -o $@

$(ASLXLINKBIN): $(ASLXLINKSRC) ../quest5/aslxglk.cc ../quest5/aslxglk-map.inc \
	         ../questglk-common.inc $(ASLXRTDEPS)
	$(CXX) $(CXXFLAGS) -I$(CHEAPGLK) $(ASLXLINKSRC) \
	    $(CHEAPGLK)/libcheapglk.a $(ASLXLIBS) -o $@

$(ASLXREPLAY): $(ASLXREPLAY).cc $(ASLXRTDEPS)
	$(CXX) $(CXXFLAGS) $(ASLXREPLAY).cc $(ASLXLIBS) -o $@

$(ASLXLINKCAP): $(ASLXLINKCAP).cc $(ASLXRTDEPS)
	$(CXX) $(CXXFLAGS) $(ASLXLINKCAP).cc $(ASLXLIBS) -o $@

$(ASLXGLK): $(ASLXGLK).cc ../quest5/aslxglk.cc ../quest5/aslxglk-map.inc \
            ../questglk-common.inc $(ASLXRTDEPS)
	$(CXX) $(CXXFLAGS) -I$(CHEAPGLK) $(ASLXGLK).cc ../quest5/aslxglk.cc \
	    $(CHEAPGLK)/libcheapglk.a $(ASLXLIBS) -o $@

# Compile each engine source as its own translation unit, semantic analysis only.
#
# Every harness above unity-includes the engine sources -- the walkthrough runner
# #includes all six engine .cc files -- so a function defined in one .cc and called
# from another builds fine here even when no header declares it -- the caller
# just sees the earlier definition. The real Xcode build compiles the same files
# separately and rejects it. That is not hypothetical: a missing `starts_with_i`
# prototype in geas-util.hh passed all three suites and broke the app build.
#
# -fsyntax-only skips codegen and linking, so this covers every source --
# including the two Glk frontends and geasglkterm.c, which no in-repo harness
# compiles at all -- in a few seconds. It is a prerequisite of `check`.
SYNTAXCXX := $(wildcard ../*.cc) $(wildcard ../quest5/*.cc)
SYNTAXC := $(wildcard ../*.c)
# -I$(CHEAPGLK) is only for glk.h; the frontends are never linked here.
SYNTAXFLAGS := -w -I.. -I$(CHEAPGLK) -I../../common_utils

syntax:
	@fail=0; \
	for f in $(SYNTAXCXX); do \
	    $(CXX) -std=c++17 -fsyntax-only $(SYNTAXFLAGS) $$f || fail=1; \
	done; \
	for f in $(SYNTAXC); do \
	    $(CC) -fsyntax-only $(SYNTAXFLAGS) $$f || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "syntax: FAILED"; exit 1; fi; \
	echo "syntax: $(words $(SYNTAXCXX) $(SYNTAXC)) sources OK"

# The aslx tests open their fixtures by a path relative to their own directory
# (../fixtures/...), so run them from there rather than from here.
check: syntax $(BIN) $(TESTBIN) $(ASLXBIN) $(ASLXRTBIN) $(QGLKBIN) $(ASLXLINKBIN)
	cd $(Q4) && ./geas_unit_tests
	cd $(Q5) && ./aslx_loader_test
	cd $(Q5) && ./aslx_runtime_test
	./$(QGLKBIN)
	cd $(Q5) && ./aslxglk_link_tests
	$(Q4)/run_fixtures.sh

asan: $(SRC) $(TESTSRC) $(DEPS) $(RANDOBJ)
	$(CXX) $(ASANFLAGS) $(RANDFLAGS) $(TESTSRC) $(RANDOBJ) -o $(TESTBIN)
	$(CXX) $(ASANFLAGS) $(RANDFLAGS) $(SRC) $(RANDOBJ) -o $(BIN)
	$(CXX) $(ASANFLAGS) $(ASLXSRC) $(ASLXLIBS) -o $(ASLXBIN)
	$(CXX) $(ASANFLAGS) $(ASLXRTSRC) $(ASLXLIBS) -o $(ASLXRTBIN)
	$(CXX) $(ASANFLAGS) -I$(CHEAPGLK) $(QGLKSRC) $(CHEAPGLK)/libcheapglk.a -o $(QGLKBIN)
	cd $(Q4) && ./geas_unit_tests
	cd $(Q5) && ./aslx_loader_test
	cd $(Q5) && ./aslx_runtime_test
	./$(QGLKBIN)
	$(Q4)/run_fixtures.sh

clean:
	rm -f $(BIN) $(TESTBIN) $(ASLXBIN) $(ASLXRTBIN) $(QGLKBIN) $(ASLXLINKBIN) \
	      $(RANDOBJ)

# Check the (untracked) game corpora against their committed manifests.  Not a
# prerequisite of `check`: that suite is deliberately corpus-free, and this one
# says nothing at all on a machine that has no corpus.  See GAMES.md.
gamescheck:
	@Q4_GAMES_DIR="$(Q4_GAMES_DIR)" Q5_GAMES_DIR="$(Q5_GAMES_DIR)" \
	  sh ./fetch_games.sh verify

# Download every corpus row that still has a live source, verifying each against
# the manifest.  Hits the network, so it is never wired into anything.
gamesfetch:
	@Q4_GAMES_DIR="$(Q4_GAMES_DIR)" Q5_GAMES_DIR="$(Q5_GAMES_DIR)" \
	  sh ./fetch_games.sh fetch

.PHONY: all syntax check asan clean gamescheck gamesfetch
