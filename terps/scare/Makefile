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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
# USA
#

VERSION = 1.3.9

# Miscellaneous tools, overridable by the command line.
ZIP	= zip
RM	= rm
CP	= cp
LN	= ln
TAR	= tar

# Default make target is the basic ANSI interpreter.
all:	scare

# Pass-through build targets.
clean distclean scare glkscare scare-$(VERSION).so sx:
	cd source; $(MAKE) $@

# Build SCARE's self-test and run it.
check test: sx
	source/sx \
	test/sccore test/scdebug test/scevents test/sclogic		\
	test/scmaths test/scnpcs test/scprintf test/screstrs		\
	test/scstring test/sctasks test/scvars test/scrooms		\
	test/hamper test/humbug test/darkwoods test/akron test/wrecked

# Cleanup.
maintainer-clean: clean
	$(RM) -f scare-$(VERSION).zip scare-$(VERSION)_linux.tgz

clobber allclean: maintainer-clean

# Build the SCARE distributions.
linux-dist:
	$(MAKE) maintainer-clean
	cd source; $(MAKE) 'CEXTRA=-DLINUX_GRAPHICS'			\
	  scare glkscare scare-$(VERSION).so
	cd source; $(CP) scare glkscare scare-$(VERSION).so ..
	cd doc; $(CP) RUNNING ..
	$(TAR) zcvf scare-$(VERSION)_linux.tgz				\
	  scare glkscare scare-$(VERSION).so				\
	  ChangeLog README COPYING RUNNING
	$(RM) -f scare glkscare scare-$(VERSION).so RUNNING

source-dist:
	$(MAKE) clean
	$(LN) -s . scare-$(VERSION)
	$(ZIP) -9 scare-$(VERSION).zip					\
	  scare-$(VERSION)/ChangeLog scare-$(VERSION)/COPYING		\
	  scare-$(VERSION)/Makefile scare-$(VERSION)/README		\
	  scare-$(VERSION)/source/* scare-$(VERSION)/test/*		\
	  scare-$(VERSION)/doc/*
	$(RM) -f scare-$(VERSION)

dist: linux-dist source-dist

