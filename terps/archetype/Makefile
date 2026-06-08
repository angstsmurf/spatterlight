# Standalone Makefile for the Archetype interpreter port.
#
# This builds an `archetype` binary linked against Spatterlight's prebuilt
# libglkimp.dylib and libglkmain.a, mirroring how advsys/geas/etc. are
# wired up via Spatterlight.xcodeproj.
#
# Useful for iterating on the port without touching the .pbxproj; for a
# proper Spatterlight build the target should also be added to Xcode (see
# README.md).

GLKIMP_DIR := ../../glkimp
BUILD_DIR  := ../../build/Debug

CXX        ?= clang++
CC         ?= clang

CXXFLAGS   := -std=c++14 -O0 -g -Wall -Wno-unknown-pragmas \
              -I. -I$(GLKIMP_DIR) \
              -DGARGLK -DSPATTERLIGHT
CFLAGS     := -O0 -g -Wall -I. -I$(GLKIMP_DIR) \
              -DGARGLK -DSPATTERLIGHT

LDFLAGS    := -L$(BUILD_DIR)
LDLIBS     := -lglkimp -lglkmain

CXX_SRCS := \
    archetype_compat.cpp \
    archetype.cpp \
    archetype_string.cpp \
    array.cpp \
    crypt.cpp \
    error.cpp \
    expression.cpp \
    game_stat.cpp \
    glk_main.cpp \
    heap_sort.cpp \
    id_table.cpp \
    interpreter.cpp \
    keywords.cpp \
    linked_list.cpp \
    misc.cpp \
    parser.cpp \
    saveload.cpp \
    semantic.cpp \
    sys_object.cpp \
    timestamp.cpp \
    token.cpp

C_SRCS := glkstart.c

OBJS := $(CXX_SRCS:.cpp=.o) $(C_SRCS:.c=.o)

archetype: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) archetype

.PHONY: clean
