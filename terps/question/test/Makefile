# Build the headless Question harnesses and the unit tests, for both engines.
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
#   quest4/harness/   Quest 4 (.asl/.cas) -- the Question engine
#   quest5/harness/   Quest 5 (.aslx/.quest) -- the aslx engine + its oracle
#   ./                engine-agnostic only (questglk_unit_tests, glkdrive.py)
#
# The Quest 4 binaries unity-include the engine sources from the Question root;
# the Quest 5 ones include the engine's headers and link its objects, built
# once below.  Either way their own quoted includes spell out the full relative
# path from wherever they live, so they build the same whether make is run from
# here or the source is compiled by hand.  Harnesses that open a fixture at run
# time resolve it relative to their own directory, so the run rules below cd
# into it first.

CXX      ?= clang++
CC       ?= clang
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -I..
ASANFLAGS := -std=c++17 -O1 -g -Wall -Wextra -I.. -fsanitize=address,undefined

# questglk_unit_tests exercises a small subset of questglk-common.inc, a menu
# of static helpers shared by the two Glk frontends, so most of the menu is
# (correctly) unused in that one translation unit.
QGLKWARN := -Wno-unused-function

Q4 := quest4/harness
Q5 := quest5/harness

BIN := $(Q4)/question_walkthrough_runner
SRC := $(Q4)/question_walkthrough_runner.cc
TESTBIN := $(Q4)/question_unit_tests
TESTSRC := $(Q4)/question_unit_tests.cc
DEPS = $(wildcard ../*.cc ../*.hh ../*.inc) $(Q4)/headless_common.hh $(RANDSRC)

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

# The Quest 5 engine: the loader (aslx.cc) and the runtime's five translation
# units, each compiled once into $(ASLXOBJDIR) and linked into every aslx
# harness, plus the Glk frontend's map pane for the two harnesses that carry
# the frontend.  libexpat + zlib are the loader's only external deps.
ASLXHDRS := ../quest5/aslx.hh ../quest5/aslx-runtime.hh \
            ../quest5/aslx-runtime-internal.hh ../quest5/aslxglk-map.hh \
            ../quest5/aslxglk-form.hh
ASLXLOADSRC := ../quest5/aslx.cc
ASLXENGSRC := $(ASLXLOADSRC) ../quest5/aslx-runtime.cc \
              ../quest5/aslx-runtime-parse.cc ../quest5/aslx-runtime-builtins.cc \
              ../quest5/aslx-state.cc ../quest5/aslx-savenative.cc
ASLXOBJDIR := $(Q5)/build
ASLXLOADOBJ := $(patsubst ../quest5/%.cc,$(ASLXOBJDIR)/%.o,$(ASLXLOADSRC))
ASLXENGOBJ := $(patsubst ../quest5/%.cc,$(ASLXOBJDIR)/%.o,$(ASLXENGSRC))
ASLXMAPSRC := ../quest5/aslxglk-map.cc ../quest5/aslxglk-form.cc
ASLXMAPOBJ := $(patsubst ../quest5/%.cc,$(ASLXOBJDIR)/%.o,$(ASLXMAPSRC))
ASLXLIBS := -lexpat -lz

# Quest 5 (.aslx / .quest) milestone-1 loader test: the loader alone.
ASLXBIN := $(Q5)/aslx_loader_test
ASLXSRC := $(Q5)/aslx_loader_test.cc

# Quest 4 game dumper: prints a .asl/.cas game as the QuestionBlocks the engine's
# loader builds (all three QCGF versions, preprocessing applied).  The other
# decoder is ../uncas.pl, an independent Perl reimplementation that emits
# compilable ASL; casdump shows what the engine itself parsed.  A reading
# tool, so built on demand and not part of check.
#   make quest4/harness/casdump
CASDUMP := $(Q4)/casdump

# Quest 4 profiling driver: replays a walkthrough in-process N times so a
# sampler has one long-lived target.  Corpus-dependent and not a test, so
# built on demand.
#   make quest4/harness/question_profile_loop
PROFLOOP := $(Q4)/question_profile_loop

# Quest 5 milestone-2 script/expression runtime test.
ASLXRTBIN := $(Q5)/aslx_runtime_test
ASLXRTSRC := $(Q5)/aslx_runtime_test.cc

# Quest 5 native walkthrough replayer (diff against quest5/goldens/).
# Needs the local game corpus, so it is built on demand and not part of check.
ASLXREPLAY := $(Q5)/aslx_replay
# The .cmd script driver it shares with the link reporter below.
ASLXDRIVE := $(Q5)/aslx_drive.hh

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

# Unit tests for the Quest 5 frontend's HTML link parser. It #includes
# aslxglk.cc itself (link_action is file-local) and links CheapGlk for the glk_*
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

$(CASDUMP): $(CASDUMP).cc $(DEPS) $(RANDOBJ)
	$(CXX) $(CXXFLAGS) $(RANDFLAGS) $(CASDUMP).cc $(RANDOBJ) -o $@

$(PROFLOOP): $(PROFLOOP).cc $(DEPS) $(RANDOBJ)
	$(CXX) $(CXXFLAGS) $(RANDFLAGS) $(PROFLOOP).cc $(RANDOBJ) -o $@

# -I$(CHEAPGLK) and common_utils are for the map pane (glk.h, rgbsurface.h);
# the engine units include neither.
$(ASLXOBJDIR)/%.o: ../quest5/%.cc $(ASLXHDRS)
	@mkdir -p $(ASLXOBJDIR)
	$(CXX) $(CXXFLAGS) -I$(CHEAPGLK) -I../../common_utils -c $< -o $@

$(ASLXBIN): $(ASLXSRC) $(ASLXLOADOBJ) $(ASLXHDRS)
	$(CXX) $(CXXFLAGS) $(ASLXSRC) $(ASLXLOADOBJ) $(ASLXLIBS) -o $@

$(ASLXRTBIN): $(ASLXRTSRC) $(ASLXENGOBJ) $(ASLXHDRS)
	$(CXX) $(CXXFLAGS) $(ASLXRTSRC) $(ASLXENGOBJ) $(ASLXLIBS) -o $@

$(QGLKBIN): $(QGLKSRC) ../questglk-common.inc
	$(CXX) $(CXXFLAGS) $(QGLKWARN) -I$(CHEAPGLK) $(QGLKSRC) $(CHEAPGLK)/libcheapglk.a -o $@

$(ASLXLINKBIN): $(ASLXLINKSRC) ../quest5/aslxglk.cc ../questglk-common.inc \
	         $(ASLXENGOBJ) $(ASLXMAPOBJ) $(ASLXHDRS)
	$(CXX) $(CXXFLAGS) -I$(CHEAPGLK) $(ASLXLINKSRC) $(ASLXENGOBJ) $(ASLXMAPOBJ) \
	    $(CHEAPGLK)/libcheapglk.a $(ASLXLIBS) -o $@

$(ASLXREPLAY): $(ASLXREPLAY).cc $(ASLXDRIVE) $(ASLXENGOBJ) $(ASLXHDRS)
	$(CXX) $(CXXFLAGS) $(ASLXREPLAY).cc $(ASLXENGOBJ) $(ASLXLIBS) -o $@

$(ASLXLINKCAP): $(ASLXLINKCAP).cc $(ASLXDRIVE) $(ASLXENGOBJ) $(ASLXHDRS)
	$(CXX) $(CXXFLAGS) $(ASLXLINKCAP).cc $(ASLXENGOBJ) $(ASLXLIBS) -o $@

$(ASLXGLK): $(ASLXGLK).cc ../quest5/aslxglk.cc ../questglk-common.inc \
            $(ASLXENGOBJ) $(ASLXMAPOBJ) $(ASLXHDRS)
	$(CXX) $(CXXFLAGS) -I$(CHEAPGLK) $(ASLXGLK).cc ../quest5/aslxglk.cc \
	    $(ASLXENGOBJ) $(ASLXMAPOBJ) $(CHEAPGLK)/libcheapglk.a $(ASLXLIBS) -o $@

# Compile each engine source as its own translation unit, semantic analysis only.
#
# The Quest 4 harnesses above unity-include the engine sources -- the walkthrough
# runner #includes all six engine .cc files -- so a function defined in one .cc
# and called from another builds fine here even when no header declares it -- the
# caller just sees the earlier definition. The real Xcode build compiles the same
# files separately and rejects it. That is not hypothetical: a missing
# `starts_with_i` prototype in question-util.hh passed all three suites and broke
# the app build. (The Quest 5 engine is linked from separate objects here too,
# but its Glk frontend is only ever compiled by the on-demand harnesses.)
#
# -fsyntax-only skips codegen and linking, so this covers every source --
# including the two Glk frontends and questionglkterm.c, which no in-repo harness
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
check: syntax drivesync $(BIN) $(TESTBIN) $(ASLXBIN) $(ASLXRTBIN) $(QGLKBIN) $(ASLXLINKBIN)
	cd $(Q4) && ./question_unit_tests
	cd $(Q5) && ./aslx_loader_test
	cd $(Q5) && ./aslx_runtime_test
	./$(QGLKBIN)
	cd $(Q5) && ./aslxglk_link_tests
	$(Q4)/run_fixtures.sh

asan: $(SRC) $(TESTSRC) $(DEPS) $(RANDOBJ) $(ASLXENGSRC) $(ASLXMAPSRC) $(ASLXHDRS)
	$(CXX) $(ASANFLAGS) $(RANDFLAGS) $(TESTSRC) $(RANDOBJ) -o $(TESTBIN)
	$(CXX) $(ASANFLAGS) $(RANDFLAGS) $(SRC) $(RANDOBJ) -o $(BIN)
	$(CXX) $(ASANFLAGS) $(ASLXSRC) $(ASLXLOADSRC) $(ASLXLIBS) -o $(ASLXBIN)
	$(CXX) $(ASANFLAGS) $(ASLXRTSRC) $(ASLXENGSRC) $(ASLXLIBS) -o $(ASLXRTBIN)
	$(CXX) $(ASANFLAGS) $(QGLKWARN) -I$(CHEAPGLK) $(QGLKSRC) $(CHEAPGLK)/libcheapglk.a -o $(QGLKBIN)
	$(CXX) $(ASANFLAGS) -I$(CHEAPGLK) -I../../common_utils $(ASLXLINKSRC) $(ASLXENGSRC) \
	    $(ASLXMAPSRC) $(CHEAPGLK)/libcheapglk.a $(ASLXLIBS) -o $(ASLXLINKBIN)
	cd $(Q4) && ./question_unit_tests
	cd $(Q5) && ./aslx_loader_test
	cd $(Q5) && ./aslx_runtime_test
	./$(QGLKBIN)
	cd $(Q5) && ./aslxglk_link_tests
	$(Q4)/run_fixtures.sh

clean:
	rm -f $(BIN) $(TESTBIN) $(ASLXBIN) $(ASLXRTBIN) $(QGLKBIN) $(ASLXLINKBIN) \
	      $(CASDUMP) $(PROFLOOP) $(ASLXREPLAY) $(ASLXLINKCAP) $(ASLXGLK) \
	      $(RANDOBJ)
	rm -rf $(ASLXOBJDIR)

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

# Spatterlight autosave/autorestore regression, both engines. Not part of
# check: it needs the Xcode build of the terp (the autosave code is compiled
# only under SPATTERLIGHT) and a few seconds per case. `--build` runs
# xcodebuild first; without it the suite refuses a terp older than the sources.
autosave:
	python3 autosave/run_autosave_tests.py --build

# glkdrive.py (the fake Spatterlight app the autosave suites drive the terps
# with) is terp-agnostic and lives in both test trees so each stays
# self-contained; the copies must not drift apart again.  Scarier's tree has
# no Makefile, so the comparison lives here.
SCARIER_DRIVE := ../../scarier/test/glkdrive.py
drivesync:
	@if [ -e $(SCARIER_DRIVE) ] && ! cmp -s glkdrive.py $(SCARIER_DRIVE); then \
	    echo "drivesync: glkdrive.py differs from $(SCARIER_DRIVE) -- copy one over the other"; \
	    exit 1; \
	fi

.PHONY: all syntax check asan clean gamescheck gamesfetch autosave drivesync
