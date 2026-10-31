# Unit tests for terps/common_utils: `make check`, run with the address and
# undefined behaviour sanitizers. They need no game files.
#
# `make warnings` compiles the directory with a stricter set of warnings.

TERPS   := ../..
COMMON  := ..
CC      ?= cc
BUILD   := build
CFLAGS  := -std=c11 -D_DARWIN_C_SOURCE -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Wundef \
           -fsanitize=address,undefined -fno-sanitize-recover=undefined \
           -I$(COMMON) -I$(TERPS)/cheapglk
SRCS    := unit.c $(COMMON)/layouttext.c $(COMMON)/companion_search.c \
           $(COMMON)/common_file_utils.c $(COMMON)/memory_allocation.c \
           $(COMMON)/randomness.c
HDRS    := $(wildcard $(COMMON)/*.h)

.PHONY: check warnings clean

check: $(BUILD)/unit
	@./$(BUILD)/unit

$(BUILD)/unit: $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

warnings:
	@mkdir -p $(BUILD)
	$(CC) -std=c11 -fsyntax-only -Wall -Wextra -Wconversion -Wundef -Wshadow \
	    -I$(COMMON) -I$(TERPS)/cheapglk $(filter %.c,$(SRCS)) $(COMMON)/common_utils.c

clean:
	rm -rf $(BUILD)
