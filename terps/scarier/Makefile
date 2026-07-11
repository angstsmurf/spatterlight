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

VERSION = 1.4.0

#
# Compiler and Glk library setup.  Configure this section to match the
# system you're using to build Scarier, or override with VARIABLE= on the
# make command line.
#
# The Scarier interpreter is now C-like C++; its sources are *.cpp and are
# compiled with $(CXX).  The bundled zlib is plain C and stays compiled with
# $(CC).  Linking is done with $(CXX) so the C++ runtime is pulled in.
#

# Compiler and linker flags.
CC	= cc
CXX	= c++
DEBUG	=
CEXTRA	=
CFLAGS	= -O2 -fPIC $(DEBUG) $(CEXTRA)
CXXFLAGS = -O2 -fPIC -Wall -W $(DEBUG) $(CEXTRA)
LDLIBS	=
LDEXTRA	=
LDFLAGS	= $(DEBUG) $(LDEXTRA)

# Glk library selection and library location.
GLK	= xglk
#GLK	= glkterm
#GLK	= cheapglk
#GLK	= winglk
GLKDIR	= ../..

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
UNZIP	= unzip
RM	= rm
TEST	= test

# Build C-like C++ interpreter objects from .cpp sources.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Scarier objects and headers.
OBJECTS	= sctafpar.o sctaffil.o scprops.o scvars.o scexpr.o scprintf.o	\
	  scinterf.o scparser.o sclibrar.o scrunner.o scevents.o	\
	  scnpcs.o scbattle.o scobjcts.o sctasks.o screstrs.o scgamest.o scserial.o\
	  scresour.o scmemos.o scutils.o sclocale.o scdebug.o
HEADERS	= scarier.h scprotos.h scgamest.h

# ADRIFT 5 engine objects and headers (driven by os_glk.cpp via gsc_a5_main
# when the loaded file is an ADRIFT 5 game).
A5OBJECTS = a5blorb.o a5deobf.o a5xml.o a5model.o a5state.o a5restr.o	\
	  a5expr.o a5sexpr.o a5text.o a5parse.o a5rand.o a5run.o
A5HEADERS = a5blorb.h a5deobf.h a5xml.h a5model.h a5state.h a5restr.h	\
	  a5expr.h a5sexpr.h a5text.h a5parse.h a5rand.h a5run.h

# Scarier verifier objects and headers.
XOBJECTS= sxmain.o sxutils.o sxglob.o sxtester.o sxscript.o sxfile.o	\
	  sxstubs.o
XHEADERS= sxprotos.h

# Zlib components used by Scarier (plain C).
ZLIBVER	= 123
ZLIBSRC	= zlib$(ZLIBVER).zip
ZOBJECTS= crc32.o inffast.o inflate.o inftrees.o zutil.o compress.o	\
	  adler32.o deflate.o trees.o
ZSOURCES= crc32.c inffast.c inflate.c inftrees.c zutil.c compress.c	\
	  adler32.c deflate.c trees.c
ZHEADERS= crc32.h inffast.h inflate.h inftrees.h inffixed.h zutil.h	\
	  zconf.h deflate.h trees.h zlib.h

# Default make target is the basic ANSI interpreter.
all:	scarier

$(ZOBJECTS): $(ZHEADERS) $(ZSOURCES)

$(ZHEADERS) $(ZSOURCES): $(ZLIBSRC)
	@$(UNZIP) -uqqa $(ZLIBSRC) $@

$(OBJECTS): $(HEADERS)

scarier: $(ZOBJECTS) $(OBJECTS) os_ansi.o
	$(CXX) $(LDFLAGS) -o scarier $(OBJECTS) $(ZOBJECTS) os_ansi.o $(LDLIBS)

# Option to build the Scarier verifier.
$(XOBJECTS): $(XHEADERS)

sx: $(ZOBJECTS) $(OBJECTS) $(XOBJECTS)
	$(CXX) $(LDFLAGS) -o sx $(OBJECTS) $(ZOBJECTS) $(XOBJECTS) $(LDLIBS)

# Option to build the Glk port of Scarier.
glkscarier: $(ZOBJECTS) $(OBJECTS) $(A5OBJECTS) os_glk.o
	@$(TEST) -n "$(GLKLIB)" \
		|| ( echo "Could not include $(GLKPATH)/Make.$(GLK)"; exit 1 )
	$(CXX) $(LDFLAGS) -o glkscarier $(OBJECTS) $(ZOBJECTS) $(A5OBJECTS)	\
		os_glk.o $(LDLIBS) $(GLKLIBS)

$(A5OBJECTS): $(A5HEADERS)

os_glk.o: os_glk.cpp $(HEADERS) $(A5HEADERS)
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
scarier-$(VERSION).so: $(ZOBJECTS) $(OBJECTS) os_glk.o $(PLUGIN_OBJS)
	$(CXX) -shared -Wl,-u,ifpi_force_link -Wl,-Bsymbolic		\
		-o $@ $(ZOBJECTS) $(OBJECTS) os_glk.o $(PLUGIN_OBJS)	\
		$(PLUGIN_LIBS) -lc

# Cleanup.
clean:
	$(RM) -f scarier sx glkscarier *.o core core.* *.gcov *.gcno *.gcda
	$(RM) -f scarier_plugin.c scarier-$(VERSION).so
	$(RM) -f $(ZSOURCES)
	$(RM) -f $(ZHEADERS)

distclean: clean

clobber: clean

maintainer-clean: clobber

# Unimplemented targets.
dist check:
