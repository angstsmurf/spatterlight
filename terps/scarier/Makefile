# vi: set ts=8:
#
# Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
# USA
#

VERSION = 1.5.0

#
# Compiler and Glk library setup.  Configure this section to match the
# system you're using to build Scarier, or override with VARIABLE= on the
# make command line.
#
# The Scarier interpreter is now C-like C++; its sources are *.cpp and are
# compiled with $(CXX).  Unlike upstream SCARE, which unzipped a bundled
# zlib123.zip at build time, this tree links the system zlib (-lz).
#

# Compiler and linker flags.
CC	= cc
CXX	= c++
DEBUG	=
CEXTRA	=
CFLAGS	= -O2 -fPIC $(DEBUG) $(CEXTRA)
# -I. lets the command library in library/ and the runner in runner/ find
# the shared Scarier headers at the root.
CXXFLAGS = -O2 -fPIC -Wall -W $(DEBUG) $(CEXTRA) -I.
LDLIBS	= -lz
LDEXTRA	=
LDFLAGS	= $(DEBUG) $(LDEXTRA)

# Glk library selection and library location.  The default matches this tree:
# terps/cheapglk is the one Glk library checked in beside us, which is enough
# to smoke-test both engines from the command line.  (The shipping macOS build
# goes through the Xcode project, not this Makefile.)
GLK	= cheapglk
#GLK	= glkterm
#GLK	= xglk
#GLK	= winglk
GLKDIR	= ..

# Glk platform selection; normally, the platform's compiler will define
# one of these automatically, but if not, pick one to suit your system.
#GLKARCH= -D__unix
#GLKARCH= -D_WIN32

#
# End of compiler and Glk library set.  The rest of the Makefile is Scarier
# generic.
#

# Set up link libraries and path to Glk.  The Glk library make inclusion
# sets up LINKLIBS and GLKLIB for us; if the file is missing (say, we're
# building only the ANSI interface version), the include should not fail.
# This will of course make GLKLIBS gibberish, but we'll check that later.
# If your make can't handle sinclude or -include, and you're not building
# for Glk, comment out the include line.
GLKPATH	= $(GLKDIR)/$(GLK)
sinclude $(GLKPATH)/Make.$(GLK)
GLKLIBS	= -L$(GLKPATH) $(GLKLIB) $(LINKLIBS)

# Always use the Bourne shell.
SHELL	= /bin/sh

# Miscellaneous tools, overridable by the command line.
RM	= rm
TEST	= test

# Build C-like C++ interpreter objects from .cpp sources.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ADRIFT 5 engine objects live in adrift5/; -I. lets them include the top-level
# headers (scarier.h, scprotos.h) by bare name.
adrift5/%.o: adrift5/%.cpp
	$(CXX) $(CXXFLAGS) -I. -c -o $@ $<

# Scarier objects and headers: every sc*.cpp at the top level plus the library/
# and runner/ topic files.  tools/ is the SCARIER_DUMP_TOOLS diagnostic
# instrumentation, compiled only by Makefile.headless; scmap.cpp is linked
# through MAPOBJECTS below.  A new .cpp in any of these three places is picked
# up without editing this file (the Xcode project still needs its own entry).
LIBDIR	= library
RUNDIR	= runner
OBJECTS	= $(filter-out scmap.o,$(patsubst %.cpp,%.o,$(wildcard sc*.cpp))) \
	  $(patsubst %.cpp,%.o,$(wildcard $(LIBDIR)/*.cpp) $(wildcard $(RUNDIR)/*.cpp))
HEADERS	= $(wildcard *.h $(LIBDIR)/*.h $(RUNDIR)/*.h)

# The map pane: scmap.cpp builds the ADRIFT 4 map model, a5map.cpp the ADRIFT 5
# one, and both draw into the engine-neutral rasteriser in mapdraw.cpp.  Only
# the Glk port has somewhere to show a map, but the engine calls
# scmap_forget_game(), so every build links them.
MAPOBJECTS = mapdraw.o scmap.o
MAPHEADERS = mapdraw.h scmap.h

# The shared, seedable cross-platform RNG (erkyrath xoshiro128**) the v5 engine
# draws from, so seeded walkthroughs reproduce across platforms.  Plain C, and
# it pulls glui32 in from glk.h, so it is built only for the Glk port.
COMMON	= ../common_utils
RANDOBJ	= a5_randomness.o

# ADRIFT 5 engine objects and headers (driven by glk/os_glk_a5.cpp via
# gsc_a5_main when the loaded file is an ADRIFT 5 game).
A5DIR	= adrift5
A5SRCDIRS = $(A5DIR) $(A5DIR)/text $(A5DIR)/run
A5OBJECTS = $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,$(A5SRCDIRS))))
A5HEADERS = $(wildcard $(addsuffix /*.h,$(A5SRCDIRS))) sessrec.h

# Default make target is the basic ANSI interpreter.
all:	scarier

$(OBJECTS): $(HEADERS)

scarier: $(OBJECTS) $(MAPOBJECTS) os_ansi.o
	$(CXX) $(LDFLAGS) -o scarier $(OBJECTS) $(MAPOBJECTS) os_ansi.o	\
		$(LDLIBS)

# Option to build the Glk port of Scarier.  The Glk front end lives in glk/:
# os_glk.cpp (module state, startup, event loop, linkage) plus the parts split
# out of it; os_glk_internal.h is what they share.
OSGLKDIR = glk
GLKOBJECTS = $(patsubst %.cpp,%.o,$(wildcard $(OSGLKDIR)/*.cpp))

glkscarier: $(OBJECTS) $(A5OBJECTS) $(MAPOBJECTS) $(RANDOBJ) $(GLKOBJECTS)
	@$(TEST) -n "$(GLKLIB)" \
		|| ( echo "Could not include $(GLKPATH)/Make.$(GLK)"; exit 1 )
	$(CXX) $(LDFLAGS) -o glkscarier $(OBJECTS) $(A5OBJECTS)		\
		$(MAPOBJECTS) $(RANDOBJ) $(GLKOBJECTS) $(LDLIBS) $(GLKLIBS)

$(A5OBJECTS): $(A5HEADERS)

$(MAPOBJECTS): $(MAPHEADERS)

$(RANDOBJ): $(COMMON)/randomness.c $(COMMON)/randomness.h
	$(CC) $(CFLAGS) -I$(GLKPATH) -c -o $@ $(COMMON)/randomness.c

$(GLKOBJECTS): %.o: %.cpp $(OSGLKDIR)/os_glk_internal.h $(HEADERS)	\
		$(A5HEADERS) $(MAPHEADERS)
	$(CXX) $(GLKARCH) $(CXXFLAGS) -Wno-write-strings			\
		-I $(GLKDIR)/$(GLK) -c -o $@ $<

# Option to build an IFP plugin version of Scarier.
scarier_plugin.c: scarier.hdr
	ifphdr $< >$@

scarier_plugin.o: scarier_plugin.c
	@$(TEST) -n "$(GLKLIB)" \
		|| ( echo "Could not include $(GLKPATH)/Make.$(GLK)"; exit 1 )
	$(CC) -I$(GLKPATH) $(CFLAGS) -c -o $@ $<

PLUGIN_LIBS = -lifppi
PLUGIN_OBJS = scarier_plugin.o
scarier-$(VERSION).so: $(OBJECTS) $(A5OBJECTS) $(MAPOBJECTS) $(RANDOBJ)	\
		$(GLKOBJECTS) $(PLUGIN_OBJS)
	$(CXX) -shared -Wl,-u,ifpi_force_link -Wl,-Bsymbolic		\
		-o $@ $(OBJECTS) $(A5OBJECTS) $(MAPOBJECTS) $(RANDOBJ)	\
		$(GLKOBJECTS) $(PLUGIN_OBJS) $(PLUGIN_LIBS) -lc

# Cleanup.
clean:
	$(RM) -f scarier glkscarier *.o adrift5/*.o library/*.o runner/*.o glk/*.o core core.* *.gcov *.gcno *.gcda
	$(RM) -f scarier_plugin.c scarier-$(VERSION).so

distclean: clean

clobber: clean

maintainer-clean: clobber

# Unimplemented targets.
dist check:
