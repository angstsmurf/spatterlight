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
	rm -f $(OBJS) comprehend

.PHONY: clean
