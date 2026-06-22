/* agtifid.h -- in-process Treaty-of-Babel IFID for raw AGT games. */
#ifndef AGTIFID_H
#define AGTIFID_H

/* Compute the Treaty-of-Babel IFID ("AGT-NNNNN-XXXXXXXX") for the raw AGT
   game whose data files share the root of `path` (typically a .D$$ or .DA1
   path).  Returns a freshly malloc'd, NUL-terminated string the caller must
   free(), or NULL if the file set could not be read as an AGT game.

   This reproduces, without any AGX conversion, exactly the value that
   babel/agt.c derives from an agt2agx-produced .agx file, so it can stand in
   for the babel identification of AGT games in the importer.

   Safe to call in-process and repeatedly: it never exit()s the host process
   (it traps the AGT reader's fatal() via longjmp) and resets the reader's
   global state on each call.  It is NOT thread-safe -- the underlying AGT
   reader uses global state -- so serialise calls. */
char *agt_copy_ifid_for_file(const char *path);

#endif /* AGTIFID_H */
