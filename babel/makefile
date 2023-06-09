# provisional makefile for babel
#
# Note that to compile babel, it is necessary only to compile all the .c
# files in this distribution and link them.
#
# This makefile is provided purely as a convenience.
#
# The following targets are available:
#  babel:		make babel
#  babel.lib:		make babel handler library (for Borland)
#  ifiction.lib:	make babel ifiction library (for Borland)
#  babel.a:		make babel handler library (for gcc)
#  ifiction.a:		make babel ifiction library (for gcc)
#  dist:		make babel.zip, the babel source distribution
#
# Note that this is a GNU makefile, and may not work with other makes
#
# Add more checks for the OS/Platform to automatically switch
# compiler/linker settings. For temporary fixes just edit...

ifeq ($(shell uname),Linux)
  COMPILER=${CC} -g -Wall
  OBJ=.o
  BABEL_LIB=babel.a
  BABEL_FLIB=babel_functions.a
  IFICTION_LIB=ifiction.a
  OUTPUT_BABEL=-o babel
else ifeq ($(shell uname),Darwin)
  COMPILER=cc -g -Wall
  OBJ=.o
  BABEL_LIB=babel.a
  BABEL_FLIB=babel_functions.a
  IFICTION_LIB=ifiction.a
  OUTPUT_BABEL=-o babel
else
  COMPILER=bcc32
  OBJ=.obj
  BABEL_LIB=babel.lib
  IFICTION_LIB=ifiction.lib
  BABEL_FLIB=babel_functions.lib
  OUTPUT_BABEL=
endif

ifndef VERBOSE
  MAKEFLAGS+=--no-print-directory
endif

treaty_objs = zcode${OBJ} magscrolls${OBJ} blorb${OBJ} glulx${OBJ} hugo${OBJ} agt${OBJ} level9${OBJ} html${OBJ} executable${OBJ} advsys${OBJ} tads${OBJ} tads2${OBJ} tads3${OBJ} adrift${OBJ} alan${OBJ}
bh_objs = babel_handler${OBJ} register${OBJ} misc${OBJ} md5${OBJ} ${treaty_objs}
ifiction_objs = ifiction${OBJ} register_ifiction${OBJ}
babel_functions =  babel_story_functions${OBJ} babel_ifiction_functions${OBJ} babel_multi_functions${OBJ}
babel_objs = babel${OBJ} $(BABEL_FLIB) $(IFICTION_LIB) $(BABEL_LIB)

babel: ${babel_objs}
	${COMPILER} ${OUTPUT_BABEL} ${babel_objs}

%${OBJ} : %.c
	${COMPILER} -c $^

register${OBJ}: modules.h

babel.lib: ${foreach dep,${bh_objs},${dep}.bl}

ifiction.lib: ${foreach dep,${ifiction_objs},${dep}.il}

babel_functions.lib: ${foreach dep,${babel_functions},${dep}.fl}

%.obj.bl: %.obj
	tlib babel.lib +-$^
	echo made > $@

%.obj.il: %.obj
	tlib ifiction.lib +-$^
	echo made > $@
%.obj.fl: %.obj
	tlib babel_functions.lib +-$^
	echo made > $@

babel.a: $(bh_objs)
	ar -r babel.a $^

ifiction.a: $(ifiction_objs)
	ar -r ifiction.a $^

babel_functions.a: $(babel_functions)
	ar -r babel_functions.a $^

.PHONY: test
test:
	$(MAKE) -C test

dist:
	# MANIFEST filenames cannot contain spaces.
	-rm rf babel.zip
	cut -c1-31 MANIFEST | sed -e "s/ //g" | zip babel.zip -@

clean:
	-rm -f *${OBJ} babel *.lib *.a
	$(MAKE) -C test clean

testclean:
	$(MAKE) -C test clean
