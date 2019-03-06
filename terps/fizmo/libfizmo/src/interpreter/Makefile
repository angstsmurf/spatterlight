
include ../../config.mk

ifneq ($(ENABLE_GDB_SYMBOLS),)
override CFLAGS += -g
endif

ifneq ($(ENABLE_OPTIMIZATION),)
override CFLAGS += -O2
endif

ifneq ($(ENABLE_TRACING),)
override CFLAGS += -DENABLE_TRACING=
endif

ifneq ($(ENABLE_STRICT_Z),)
override CFLAGS += -DSTRICT_Z=
endif

ifneq ($(THROW_SIGFAULT_ON_ERROR),)
override CFLAGS += -DTHROW_SIGFAULT_ON_ERROR=
endif

ifneq ($(DISABLE_SIGNAL_HANDLERS),)
override CFLAGS += -DDISABLE_SIGNAL_HANDLERS=
endif

ifneq ($(DISABLE_BABEL),)
CFLAGS_LIBXML2 = -DDISABLE_BABEL=
else
CFLAGS_LIBXML2 = $(LIBXML2_PKG_CFLAGS) $(LIBXML2_NONPKG_CFLAGS)
endif

ifeq ($(DISABLE_FILELIST),)
FILELIST_DEP_H = filelist.h
FILELIST_DEP_O = filelist.o
else
override CFLAGS += -DDISABLE_FILELIST=
endif

ifeq ($(DISABLE_BLOCKBUFFER),)
BLOCKBUF_DEP_H = blockbuf.h
BLOCKBUF_DEP_O = blockbuf.o
else
override CFLAGS += -DDISABLE_BLOCKBUFFER=
endif

ifeq ($(DISABLE_COMMAND_HISTORY),)
CMD_HST_DEP_H = cmd_hst.h
CMD_HST_DEP_O = cmd_hst.o
else
override CFLAGS += -DDISABLE_COMMAND_HISTORY
endif

ifeq ($(DISABLE_OUTPUT_HISTORY),)
OUTPUT_HST_DEP_H = history.h
OUTPUT_HST_DEP_O = history.o
else
override CFLAGS += -DDISABLE_OUTPUT_HISTORY
endif

ifneq ($(DISABLE_CONFIGFILES),)
override CFLAGS += -DDISABLE_CONFIGFILES=
endif

ifneq ($(DISABLE_PREFIX_COMMANDS),)
override CFLAGS += -DDISABLE_PREFIX_COMMANDS=
endif

ifneq ($(ENABLE_DEBUGGER),)
override CFLAGS += -DENABLE_DEBUGGER=
DEBUG_DEP_H = debugger.h
DEBUG_DEP_O = debugger.o
endif



.PHONY : clean

all: libinterpreter.a

libinterpreter.a: fizmo.o zpu.o stack.o streams.o routine.o variable.o object.o property.o text.o savegame.o iff.o misc.o mt19937ar.o math.o output.o sound.o wordwrap.o table.o config.o undo.o babel.o hyphenation.o blorb.o $(BLOCKBUF_DEP_O) $(CMD_HST_DEP_O) $(OUTPUT_HST_DEP_O) $(FILELIST_DEP_O) $(DEBUG_DEP_O)
	"$(AR)" rcs libinterpreter.a fizmo.o zpu.o stack.o streams.o routine.o variable.o object.o property.o text.o savegame.o iff.o misc.o mt19937ar.o math.o output.o sound.o wordwrap.o table.o config.o undo.o babel.o hyphenation.o blorb.o $(BLOCKBUF_DEP_O) $(CMD_HST_DEP_O) $(OUTPUT_HST_DEP_O) $(FILELIST_DEP_O) $(DEBUG_DEP_O)

fizmo.o: ../tools/tracelog.h fizmo.h zpu.h text.h math.h stack.h ../tools/i18n.h misc.h streams.h config.h savegame.h output.h ../tools/z_ucs.h fizmo.c $(BLOCKBUF_DEP_H) $(OUTPUT_HST_DEP_H) $(FILELIST_DEP_H) $(DEBUG_DEP_H)
	"$(CC)" $(CFLAGS) -c fizmo.c

fizmo.h: ../screen_interface/screen_interface.h
	touch fizmo.h

zpu.o: zpu.h ../tools/tracelog.h fizmo.h savegame.h misc.h routine.h variable.h property.h object.h math.h text.h ../tools/i18n.h streams.h output.h sound.h stack.h table.h undo.h zpu.c $(DEBUG_DEP_H)
	"$(CC)" $(CFLAGS) -c zpu.c

stack.o: stack.h ../tools/tracelog.h config.h ../tools/i18n.h routine.h stack.c
	"$(CC)" $(CFLAGS) -c stack.c

streams.o: streams.h ../tools/tracelog.h fizmo.h ../screen_interface/screen_interface.h ../tools/i18n.h wordwrap.h text.h zpu.h output.h ../tools/z_ucs.h blockbuf.h history.h ../tools/types.h streams.c $(BLOCKBUF_DEP_H) $(OUTPUT_HST_DEP_H)
	"$(CC)" $(CFLAGS) -c streams.c

routine.o: routine.h ../tools/tracelog.h stack.h variable.h zpu.h ../tools/i18n.h fizmo.h routine.c
	"$(CC)" $(CFLAGS) -c routine.c

variable.o: ../tools/tracelog.h fizmo.h stack.h zpu.h ../tools/i18n.h config.h variable.c
	"$(CC)" $(CFLAGS) -c variable.c

object.o: object.h ../tools/tracelog.h fizmo.h variable.h zpu.h ../tools/i18n.h object.c
	"$(CC)" $(CFLAGS) -c object.c

property.o: ../tools/tracelog.h fizmo.h variable.h object.h zpu.h ../tools/i18n.h property.c
	"$(CC)" $(CFLAGS) -c property.c

text.o: text.h ../tools/tracelog.h fizmo.h routine.h object.h zpu.h ../tools/i18n.h variable.h misc.h streams.h ../tools/types.h history.h fizmo.h math.h stack.h config.h output.h undo.h $(CMD_HST_DEP_H) text.c $(DEBUG_DEP_H)
	"$(CC)" $(CFLAGS) -c text.c

savegame.o: savegame.h ../tools/tracelog.h streams.h fizmo.h iff.h stack.h zpu.h ../tools/i18n.h variable.h text.h ../tools/z_ucs.h routine.h savegame.c
	"$(CC)" $(CFLAGS) -c savegame.c

iff.o: iff.h ../tools/tracelog.h ../tools/i18n.h ../tools/types.h iff.c
	"$(CC)" $(CFLAGS) -c iff.c

misc.o: misc.h ../tools/tracelog.h fizmo.h streams.h stack.h zpu.h ../tools/i18n.h routine.h misc.c $(DEBUG_DEP_H)
	"$(CC)" $(CFLAGS) -c misc.c

mt19937ar.o: mt19937ar.h mt19937ar.c
	"$(CC)" $(CFLAGS) -c mt19937ar.c

math.o: math.h ../tools/tracelog.h mt19937ar.h variable.h zpu.h ../tools/i18n.h math.c
	"$(CC)" $(CFLAGS) -c math.c

streams.h: config.h
	touch streams.h

output.o: output.h ../tools/tracelog.h ../tools/types.h fizmo.h ../tools/i18n.h zpu.h variable.h $(BLOCKBUF_DEP_H) $(OUTPUT_HST_DEP_H) output.c
	"$(CC)" $(CFLAGS) -c output.c

sound.o: fizmo.h ../tools/i18n.h ../tools/tracelog.h zpu.h sound.c
	"$(CC)" $(CFLAGS) -c sound.c

wordwrap.o: wordwrap.h ../tools/tracelog.h ../tools/types.h ../tools/i18n.h ../tools/z_ucs.h wordwrap.c
	"$(CC)" $(CFLAGS) -c wordwrap.c

history.o: history.h ../tools/tracelog.h config.h ../tools/i18n.h ../tools/z_ucs.h ../tools/types.h ../screen_interface/screen_interface.h fizmo.h history.c
	"$(CC)" $(CFLAGS) -c history.c

table.o: table.h ../tools/tracelog.h zpu.h variable.h ../tools/i18n.h table.c
	"$(CC)" $(CFLAGS) -c table.c

cmd_hst.o: cmd_hst.h ../tools/tracelog.h ../tools/types.h ../tools/i18n.h cmd_hst.c
	"$(CC)" $(CFLAGS) -c cmd_hst.c

blockbuf.o: blockbuf.h ../tools/types.h ../tools/tracelog.h ../tools/i18n.h ../tools/z_ucs.h blockbuf.c
	"$(CC)" $(CFLAGS) -c blockbuf.c

config.o: config.h ../tools/types.h ../tools/i18n.h config.c
	"$(CC)" $(CFLAGS) -c config.c

undo.o: undo.h ../tools/tracelog.h ../tools/types.h zpu.h variable.h fizmo.h stack.h undo.c
	"$(CC)" $(CFLAGS) -c undo.c

filelist.o: filelist.h fizmo.h ../tools/tracelog.h babel.h iff.h ../tools/types.h filelist.c
	"$(CC)" $(CFLAGS) $(CFLAGS_LIBXML2) -c filelist.c

babel.o: babel.h fizmo.h ../tools/tracelog.h babel.c
	"$(CC)" $(CFLAGS) $(CFLAGS_LIBXML2) -c babel.c

hyphenation.o: hyphenation.h ../tools/tracelog.h hyphenation.c
	"$(CC)" $(CFLAGS) -c hyphenation.c

blorb.o: blorb.h ../tools/tracelog.h blorb.c
	"$(CC)" $(CFLAGS) -c blorb.c

debugger.o: debugger.h debugger.c
	"$(CC)" $(CFLAGS) -c debugger.c

clean:
	rm -f *.o

distclean: clean
	rm -f libinterpreter.a

