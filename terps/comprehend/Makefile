# Standalone Makefile for the Comprehend interpreter port.
# Mirrors terps/archetype/Makefile.

GLKIMP_DIR := ../../glkimp
BUILD_DIR  := ../../build/Debug

CXX        ?= clang++
CC         ?= clang

READWOZ_DIR := ../readwoz
COMMON_DIR  := ../common_utils
SAGADRAW_DIR := ../common_sagadraw

CXXFLAGS   := -std=c++14 -O0 -g -Wall -Wno-unknown-pragmas \
              -Wno-macro-redefined -Wno-format \
              -I. -I$(GLKIMP_DIR) -I$(READWOZ_DIR) -I$(COMMON_DIR) -I$(SAGADRAW_DIR) \
              -DGARGLK -DSPATTERLIGHT
CFLAGS     := -O0 -g -Wall -w -I. -I$(GLKIMP_DIR) -I$(READWOZ_DIR) -I$(COMMON_DIR) -I$(SAGADRAW_DIR) \
              -DGARGLK -DSPATTERLIGHT

LDFLAGS    := -L$(BUILD_DIR)
LDLIBS     := -lglkimp -lglkmain

CXX_SRCS := \
    apple2.cpp \
    graphics_magician.cpp \
    graphics_magician_cga.cpp \
    graphics_magician_dhgr.cpp \
    charset.cpp \
    comprehend.cpp \
    comprehend_compat.cpp \
    dictionary.cpp \
    draw_surface.cpp \
    file_buf.cpp \
    game.cpp \
    game_cc.cpp \
    game_cm.cpp \
    game_data.cpp \
    game_oo.cpp \
    game_opcodes.cpp \
    game_tm.cpp \
    game_tr1.cpp \
    game_tr2.cpp \
    glk_main.cpp \
    pics.cpp

C_SRCS := glkstart.c

# Shared Apple II disk-image readers (DOS 3.3 / ProDOS + WOZ).
READWOZ_SRCS := $(READWOZ_DIR)/ciderpress.c $(READWOZ_DIR)/woz2nib.c \
                $(COMMON_DIR)/read_le16.c

OBJS := $(CXX_SRCS:.cpp=.o) $(C_SRCS:.c=.o) \
        ciderpress.o woz2nib.o read_le16.o gm_vector.o gm_artifact.o

comprehend: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

ciderpress.o: $(READWOZ_DIR)/ciderpress.c
	$(CC) $(CFLAGS) -c $< -o $@
woz2nib.o: $(READWOZ_DIR)/woz2nib.c
	$(CC) $(CFLAGS) -c $< -o $@
read_le16.o: $(COMMON_DIR)/read_le16.c
	$(CC) $(CFLAGS) -c $< -o $@
gm_vector.o: $(SAGADRAW_DIR)/gm_vector.c $(SAGADRAW_DIR)/gm_vector.h
	$(CC) $(CFLAGS) -c $< -o $@
gm_artifact.o: $(SAGADRAW_DIR)/gm_artifact.c $(SAGADRAW_DIR)/gm_artifact.h
	$(CC) $(CFLAGS) -c $< -o $@

# Shared Apple II renderer sources, compiled into the self-contained test tools.
SAGADRAW_SRCS := $(SAGADRAW_DIR)/gm_vector.c $(SAGADRAW_DIR)/gm_artifact.c

clean:
	rm -f $(OBJS) gm_vector.o gm_artifact.o comprehend test/test_talisman_pics test/test_gmcga_pics test/test_gmcgav1_pics test/gmcgav1test test/pixtest test/op5scan test/gmcgatest test/diff_gmcga test/rgbacrc

# --- Regression test: Apple II Talisman renderer vs golden MAME hi-res pages ---
# Self-contained: graphics_magician.cpp has no Glk/disk dependencies, so the test
# links only it. Fixtures live in test/talisman/.
test/test_talisman_pics: test/test_talisman_pics.cpp graphics_magician.cpp graphics_magician.h $(SAGADRAW_SRCS)
	$(CXX) -std=c++14 -O0 -g -Wall -I. -I$(SAGADRAW_DIR) -o $@ test/test_talisman_pics.cpp graphics_magician.cpp $(SAGADRAW_SRCS)

# --- Regression anchor: Apple II hi-res -> RGBA artifact renderer ---
# No byte-exact page corpus covers the colour kernel (gm_artifact), so this
# checksums the RGBA output of a synthetic page against a captured golden.
test/rgbacrc: test/rgbacrc.cpp graphics_magician.cpp graphics_magician.h $(SAGADRAW_SRCS)
	$(CXX) -std=c++14 -O0 -g -Wall -I. -I$(SAGADRAW_DIR) -o $@ test/rgbacrc.cpp graphics_magician.cpp $(SAGADRAW_SRCS)

# --- Regression test: DOS Talisman CGA renderer vs golden DOSBox framebuffers ---
# Self-contained: graphics_magician_cga.cpp has no Glk/disk dependencies, and the drawing
# tables / vector streams / goldens are committed in test/gmcga/.
test/test_gmcga_pics: test/test_gmcga_pics.cpp graphics_magician_cga.cpp graphics_magician_cga.h graphics_magician.h
	$(CXX) -std=c++14 -O0 -g -Wall -I. -o $@ test/test_gmcga_pics.cpp graphics_magician_cga.cpp

# --- Regression test: v1 DOS CGA renderer (Crimson Crown / Transylvania v1) ---
# Same renderer, v1 table-load path: fill tables from PC_GRAPH.OVR, brushes from
# NOVEL.EXE. Committed slices + DOSBox goldens in test/gmcgav1/.
test/test_gmcgav1_pics: test/test_gmcgav1_pics.cpp graphics_magician_cga.cpp graphics_magician_cga.h graphics_magician.h
	$(CXX) -std=c++14 -O0 -g -Wall -I. -o $@ test/test_gmcgav1_pics.cpp graphics_magician_cga.cpp

# --- Regression test: Apple II DHGR ("<D>") renderer, op15/0 RLE bitmap ---
# Renders The Coveted Mirror's throne room and asserts the double hi-res pages
# are byte-exact vs the MAME ground truth. Self-contained: graphics_magician_dhgr.cpp
# has no Glk/disk dependencies; fixtures are committed in test/cm_dhgr/.
# Links graphics_magician.cpp too: the DHGR hourglass shares its grain-position
# helper (gmCMHourglassGrainPos) with the standard renderer.
test/cmdhgrtest: test/cmdhgrtest.cpp graphics_magician_dhgr.cpp graphics_magician_dhgr.h graphics_magician.cpp graphics_magician.h $(SAGADRAW_SRCS)
	$(CXX) -std=c++14 -O0 -g -Wall -I. -I$(SAGADRAW_DIR) -o $@ test/cmdhgrtest.cpp graphics_magician_dhgr.cpp graphics_magician.cpp $(SAGADRAW_SRCS)

test: test/test_talisman_pics test/test_gmcga_pics test/test_gmcgav1_pics test/rgbacrc test/cmdhgrtest
	./test/test_talisman_pics
	./test/test_gmcga_pics
	./test/test_gmcgav1_pics
	./test/rgbacrc
	./test/cmdhgrtest

# Developer tool: extract/render a single Apple II Talisman picture from a disk
# image and log its hi-res writes. Used to capture new regression fixtures.
# See test/talisman/README.md.
test/pixtest: test/pixtest.cpp graphics_magician.cpp graphics_magician_dhgr.cpp ciderpress.o woz2nib.o read_le16.o gm_vector.o gm_artifact.o
	$(CXX) -std=c++14 -O0 -g -DGM_TRACE -I. -I$(READWOZ_DIR) -I$(COMMON_DIR) -I$(SAGADRAW_DIR) -o $@ \
	    test/pixtest.cpp graphics_magician.cpp graphics_magician_dhgr.cpp ciderpress.o woz2nib.o read_le16.o gm_vector.o gm_artifact.o

# Developer tool: render one DOS Talisman (CGA) picture and write it as a PPM,
# to eyeball against a DOSBox capture. Self-contained: graphics_magician_cga.cpp has no
# Glk/disk dependencies, so it links only that.
#   ./test/gmcgatest NOVEL1.EXE T0 4 title.ppm white
test/gmcgatest: test/gmcgatest.cpp graphics_magician_cga.cpp graphics_magician_cga.h graphics_magician.h
	$(CXX) -std=c++14 -O0 -g -Wall -I. -o $@ test/gmcgatest.cpp graphics_magician_cga.cpp

# Developer tool: render one v1 (Crimson Crown / Transylvania v1) picture from a
# .MS1 file, loading the split v1 tables from PC_GRAPH.OVR + NOVEL.EXE; writes a
# PPM and optionally pixel-compares against a DOSBox CGA VRAM golden.
#   ./test/gmcgav1test PC_GRAPH.OVR NOVEL.EXE RA.MS1 0 out.ppm [golden.fb]
test/gmcgav1test: test/gmcgav1test.cpp graphics_magician_cga.cpp graphics_magician_cga.h graphics_magician.h
	$(CXX) -std=c++14 -O0 -g -Wall -I. -o $@ test/gmcgav1test.cpp graphics_magician_cga.cpp

# Developer tool: re-render every committed DOS Talisman fixture and report, per
# scene, the rendered->golden colour-transition histogram, per-row diff bands, a
# spatial diff PPM (/tmp/diff_<scene>.ppm), and raw index dumps -- the analysis
# used to drive the renderer toward pixel-exactness. Self-contained.
test/diff_gmcga: test/diff_gmcga.cpp graphics_magician_cga.cpp graphics_magician_cga.h graphics_magician.h
	$(CXX) -std=c++14 -O0 -g -Wall -I. -o $@ test/diff_gmcga.cpp graphics_magician_cga.cpp

# Developer tool: scan every image in an Apple II Comprehend disk's graphics
# files and report which ones use a given drawing opcode (op5/TEXT_OUTLINE).
# Self-contained: replicates the graphics_magician opcode walker, so it only
# needs the disk-image readers. Handles both the 1985 16-image collection format
# and the older single-picture-per-file DOS 3.3 releases (via OP5_SINGLE).
test/op5scan: test/op5scan.c ciderpress.o woz2nib.o read_le16.o
	$(CC) -O2 -w -I. -I$(READWOZ_DIR) -I$(COMMON_DIR) -o $@ \
	    test/op5scan.c ciderpress.o woz2nib.o read_le16.o

# Developer tool: list/extract files from a Comprehend Apple II .woz/.dsk disk,
# used to pull the G-files and capture the DHGR fixtures in test/cm_dhgr/.
#   ./test/cmextract disk.woz --list
#   ./test/cmextract disk.woz T5 out.bin
test/cmextract: test/cmextract.cpp ciderpress.o woz2nib.o read_le16.o
	$(CXX) -std=c++14 -O0 -g -w -I. -I$(READWOZ_DIR) -I$(COMMON_DIR) -o $@ \
	    test/cmextract.cpp ciderpress.o woz2nib.o read_le16.o

.PHONY: clean test
