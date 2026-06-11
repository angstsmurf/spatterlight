# Fast byte-exact harness for the Saga/Scott Apple II + Atari 8-bit vector
# renderers (apple2_flood_fill / flood_fill). Mirrors -[SpatterlightTests
# testScottDraw]: loads the drawing tables from MI.dsk, renders mi_boom /
# so_console / so_painting on each platform and byte-compares the full screen
# buffer(s) against the saved .result fixtures.
#
#   make test    build and run against the SpatterlightTests fixtures
#   make clean

CC      ?= cc
SCOTT   := ../..
TERPS   := $(SCOTT)/..
CFLAGS  := -std=c11 -g -O0 -w -DSPATTERLIGHT \
           -I$(SCOTT) -I$(SCOTT)/saga -I$(SCOTT)/ai_uk \
           -I$(TERPS)/common_utils -I$(TERPS)/common_sagadraw \
           -I$(TERPS)/readwoz -I$(TERPS)/aiukgraphics -I$(TERPS)/../glkimp

SRCS := filltest.c \
        $(SCOTT)/saga/apple2_vector_draw.c \
        $(SCOTT)/saga/atari_8bit_vector_draw.c \
        $(SCOTT)/saga/vector_common.c \
        $(SCOTT)/saga/apple2detect.c \
        $(TERPS)/readwoz/ciderpress.c \
        $(TERPS)/common_utils/memory_allocation.c \
        $(TERPS)/common_utils/common_file_utils.c

FIXTURES := $(TERPS)/../SpatterlightTests/Supporting Files/

build/filltest: $(SRCS) filltest.c
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRCS)

test: build/filltest
	./build/filltest "$(FIXTURES)"

# Same as `test`, plus the 315-image Apple II ground-truth corpus.
groundtruth: build/filltest
	./build/filltest "$(FIXTURES)" ./groundtruth

clean:
	rm -rf build

.PHONY: test groundtruth clean
