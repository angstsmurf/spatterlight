## Version 0.6, Treaty of Babel Revision 11

This is the source code for babel, the [Treaty of Babel][babel] analysis tool.

[babel]: https://babel.ifarchive.org/

Most of this code is (c) 2006-2010 by L. Ross Raszewski

The following files are public domain:

- zcode.c
- glulx.c
- executable.c
- level9.c
- magscrolls.c
- agt.c
- hugo.c
- advsys.c
- misc.c
- alan.c
- adrift.c
- html.c
- twine.c
- archetype.c
- comprehend.c
- treaty.h
- treaty_builder.h

The following format modules were added for Spatterlight and are under the
GPL (see the header of each file):

- quest4.c
- quest5.c
- quill.c (from UnQuill, (c) 1996-2002 John Elliott)
- sagaplus.c
- scott.c
- taylor.c

jacl.c and babel_util.h carry no licence statement of their own.

The following files are Copyright (C) 1999, 2000, 2002 Aladdin Enterprises:

- md5.c
- md5.h

And are used in accordance with their licenses.

The following files are Copyright 2006 by Emily Short:

- test/bronze/Bronze.zblorb

All other files are (c) 2006 by L. Ross Raszewski and are released under
the Creative Commons Attribution 4.0 License.

To view a copy of this license, visit
https://creativecommons.org/licenses/by/4.0/ or send a letter to

    Creative Commons,
    PO Box 1866,
    Mountain View, CA 94042, USA.

To build babel:

1. compile all the source files in this directory
2. link them together
3. the end

For folks who find makefiles more useful than generalizations, there is a
GNU makefile provided for babel. It uses the system C compiler on Linux and
macOS and Borland's 32-bit compiler elsewhere. `make test` runs the tests in
test/; see test/README.md.

Spatterlight does not use the makefile: its Xcode project compiles
babel_handler.c, register.c, register_ifiction.c, misc.c, md5.c, ifiction.c,
blorb.c and the format modules listed in modules.h straight into the app.
babel.c, babel_*_functions.c, html.c, extras/ and babel-get/ are only built
by the makefiles.

To compile babel-get, first compile babel, then do the same thing in the
babel-get directory.

To compile ifiction-aggregate, ifiction-xtract, babel-list, and simple-marry,
first compile babel, then compile the relevant C file in the extras/ directory
(These may rely on #include files from the babel directory, so, for example,
to compile ifiction-aggregate, `gcc -c -I.. ifiction-aggregate.c`), then link the
opbject file to the babel and ifiction libraries (babel.lib and ifiction.lib
under Windows, babel.a and ifiction.a most everywhere else.  eg.
`gcc -o ifiction-aggregate ifiction-aggregate.o ../babel.a ../ifiction.a`)

Babel is intended to accept contributions in the form of treaty modules
as defined by the treaty of babel section 2.3.2.

These modules should use the declarations made in treaty.h.
The file treaty_builder.h generates a generic framework which simplifies
the task of writing treaty modules.  Its use is not required for treaty
compliance, but it should prove useful.

Parts of babel are intended for use in other programs.  When adapting
babel's source, the files babel.c, babel_story_functions.c and
babel_ifiction_functions.c will probably not prove useful.  However, you
may wish to use babel_handler, which provides a framework for loading a
story file, selecting the proper treaty modules, and seamlessly handling
blorb-wrapped files.

