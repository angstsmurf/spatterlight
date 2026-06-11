# Standalone Makefile for the Comprehend interpreter port.
# Mirrors terps/archetype/Makefile.

GLKIMP_DIR := ../../glkimp
BUILD_DIR  := ../../build/Debug

CXX        ?= clang++
CC         ?= clang

READWOZ_DIR := ../readwoz
COMMON_DIR  := ../common_utils

CXXFLAGS   := -std=c++14 -O0 -g -Wall -Wno-unknown-pragmas \
              -Wno-macro-redefined -Wno-format \
              -I. -I$(GLKIMP_DIR) -I$(READWOZ_DIR) -I$(COMMON_DIR) \
              -DGARGLK -DSPATTERLIGHT
CFLAGS     := -O0 -g -Wall -w -I. -I$(GLKIMP_DIR) -I$(READWOZ_DIR) -I$(COMMON_DIR) \
              -DGARGLK -DSPATTERLIGHT

LDFLAGS    := -L$(BUILD_DIR)
LDLIBS     := -lglkimp -lglkmain

CXX_SRCS := \
    apple2.cpp \
    graphics_magician.cpp \
    hdos_talisman.cpp \
    charset.cpp \
    comprehend.cpp \
    comprehend_compat.cpp \
    dictionary.cpp \
    draw_surface.cpp \
    file_buf.cpp \
    game.cpp \
    game_cc.cpp \
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
        ciderpress.o woz2nib.o read_le16.o

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

clean:
	rm -f $(OBJS) comprehend test/test_talisman_pics test/test_hdos_pics test/pixtest test/op5scan test/hdostest

# --- Regression test: Apple II Talisman renderer vs golden MAME hi-res pages ---
# Self-contained: graphics_magician.cpp has no Glk/disk dependencies, so the test
# links only it. Fixtures live in test/talisman/.
test/test_talisman_pics: test/test_talisman_pics.cpp graphics_magician.cpp graphics_magician.h
	$(CXX) -std=c++14 -O0 -g -Wall -I. -o $@ test/test_talisman_pics.cpp graphics_magician.cpp

# --- Regression test: DOS Talisman CGA renderer vs golden DOSBox framebuffers ---
# Self-contained: hdos_talisman.cpp has no Glk/disk dependencies, and the drawing
# tables / vector streams / goldens are committed in test/hdos/.
test/test_hdos_pics: test/test_hdos_pics.cpp hdos_talisman.cpp hdos_talisman.h graphics_magician.h
	$(CXX) -std=c++14 -O0 -g -Wall -I. -o $@ test/test_hdos_pics.cpp hdos_talisman.cpp

test: test/test_talisman_pics test/test_hdos_pics
	./test/test_talisman_pics
	./test/test_hdos_pics

# Developer tool: extract/render a single Apple II Talisman picture from a disk
# image and log its hi-res writes. Used to capture new regression fixtures.
# See test/talisman/README.md.
test/pixtest: test/pixtest.cpp graphics_magician.cpp ciderpress.o woz2nib.o read_le16.o
	$(CXX) -std=c++14 -O0 -g -DGM_TRACE -I. -I$(READWOZ_DIR) -I$(COMMON_DIR) -o $@ \
	    test/pixtest.cpp graphics_magician.cpp ciderpress.o woz2nib.o read_le16.o

# Developer tool: render one DOS Talisman (CGA) picture and write it as a PPM,
# to eyeball against a DOSBox capture. Self-contained: hdos_talisman.cpp has no
# Glk/disk dependencies, so it links only that.
#   ./test/hdostest NOVEL1.EXE T0 4 title.ppm white
test/hdostest: test/hdostest.cpp hdos_talisman.cpp hdos_talisman.h graphics_magician.h
	$(CXX) -std=c++14 -O0 -g -Wall -I. -o $@ test/hdostest.cpp hdos_talisman.cpp

# Developer tool: scan every image in an Apple II Comprehend disk's graphics
# files and report which ones use a given drawing opcode (op5/TEXT_OUTLINE).
# Self-contained: replicates the graphics_magician opcode walker, so it only
# needs the disk-image readers. Handles both the 1985 16-image collection format
# and the older single-picture-per-file DOS 3.3 releases (via OP5_SINGLE).
test/op5scan: test/op5scan.c ciderpress.o woz2nib.o read_le16.o
	$(CC) -O2 -w -I. -I$(READWOZ_DIR) -I$(COMMON_DIR) -o $@ \
	    test/op5scan.c ciderpress.o woz2nib.o read_le16.o

.PHONY: clean test
