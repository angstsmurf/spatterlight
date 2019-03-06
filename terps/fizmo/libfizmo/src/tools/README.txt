

 This "tools" directory contains some utilities which are not strictly
 fizmo-related. They were separated from the rest of the code in order
 to allow easier building of fizmo-related tools, such as the simple
 test programs or separate libraries like drilbo.

 In particular, the "tools" set is made up from the following utilies:

 - i18n: Modularizable localization-tools.
 - stringmap: A simple mapping associating *void indexed by *z_ucs.
 - types: All types related to fizmo, essentially providing Z-machine types.
 - z_ucs: Support for wide characters requiring no wchar-C-Compiler support.
 - list: A simple set of *void pointers for easier list managment.
 - tracelog: A simple set of macros allowing to trace code execution.
 - unused: A macro provoding the "UNUSED(<parameter>)" expression.

 The Makefile supports the folloing targets:

 all (default): Will build "libtools.a".
 clean: Will remove all objects, test code and "tracelog.txt".
 distclean: Will execute "clean" and also remove "libtools.a".
 test: Will build and execute a tiny tools-test program.


