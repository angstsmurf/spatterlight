# Regression tests for unp64: see README.md.
#
#   make check      unpack every sample, compare with expected.tsv; -c against
#                   repeated calls; unit tests
#   make bless      rewrite expected.tsv from what the samples unpack to now
#   make trace      one line per sample on stdout: a hash of every CPU step
#   make capture    take the samples from the Scott and Taylor test images

CXX      ?= c++
CXXFLAGS := -O1 -g -std=c++17 -Wall -I.. -MMD -MP
OBJDIR   := build/obj

SRCS := $(wildcard ../*.cpp) $(wildcard ../scanners/*.cpp)
OBJS := $(patsubst %.cpp,$(OBJDIR)/%.o,$(notdir $(SRCS)))
VPATH := ..:../scanners:.

all: unp64_probe unp64_unit

unp64_probe: $(OBJS) $(OBJDIR)/unp64_probe.o
	$(CXX) $(LDFLAGS) -o $@ $^

unp64_unit: $(OBJS) $(OBJDIR)/unp64_unit.o
	$(CXX) $(LDFLAGS) -o $@ $^

# The same sources with the per-instruction hook compiled in.
TRACEDIR := build/trace
TRACEOBJS := $(patsubst %.cpp,$(TRACEDIR)/%.o,$(notdir $(SRCS)))

unp64_trace: $(TRACEOBJS) $(TRACEDIR)/unp64_probe.o
	$(CXX) $(LDFLAGS) -o $@ $^

$(TRACEDIR)/%.o: %.cpp | $(TRACEDIR)
	$(CXX) $(CXXFLAGS) -DUNP64_TRACE -c $< -o $@

$(TRACEDIR):
	mkdir -p $(TRACEDIR)

trace: unp64_trace
	@python3 run_tests.py --trace

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

check: all
	@./unp64_unit
	@python3 run_tests.py
	@python3 run_tests.py --chain

bless: unp64_probe
	@python3 run_tests.py --bless

capture:
	sh capture.sh

clean:
	rm -rf build unp64_probe unp64_unit unp64_trace

.PHONY: all check bless trace capture clean

-include $(wildcard $(OBJDIR)/*.d) $(wildcard $(TRACEDIR)/*.d)
