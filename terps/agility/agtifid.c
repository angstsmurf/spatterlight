/* agtifid.c -- compute the Treaty-of-Babel IFID of a raw AGT game           */
/*              (.D$$/.DA1 file set) in-process, without converting to AGX.   */
/*                                                                            */
/* This lets Spatterlight's importer identify AGT games directly from their   */
/* original multi-file form, instead of spawning the agt2agx binary into a    */
/* temporary .agx just to read the IFID out of it with babel.                 */
/*                                                                            */
/* The IFID reproduced here is byte-for-byte what babel/agt.c computes from   */
/* the converted AGX file:                                                    */
/*                                                                            */
/*     IFID = "AGT-%05d-%08X"                                                 */
/*             |        |                                                     */
/*             |        +-- game_sig: a 16-bit running checksum accumulated   */
/*             |            while the data files are read (util.c)            */
/*             +----------- version code = agx_version[aver], +1 when the     */
/*                          game is "large" (ver==2) or "soggy" (ver==4)      */
/*                                                                            */
/* `aver`, `ver` and `game_sig` are all byproducts of a full readagt() parse  */
/* -- there is no shortcut header read as there is for AGX -- so this does a  */
/* real (read-only) parse, then frees everything again.                      */
/*                                                                            */
/* This program may be redistributed under the terms of the                   */
/* GNU General Public License, version 2; see agility.h for details.          */

#include <ctype.h>
#include <setjmp.h>

/* Allocate the AGT reader's global storage here, exactly as agt2agx.c and
   exec.c do, so this can be linked without exec.o / the interpreter core. */
#define global
#include "agility.h"

#include "agtifid.h"

/* Mirror of the (static) table in util.c that maps the internal AGT version
   enum (AGT10..AGX00 == 1..16) to its numeric version code.  Keep in sync
   with util.c:agx_version[]. */
static const int agx_version[] = {
    0, 0000, 1800, 2000, 3200, 3500, 8200, 8300,
    5000, 5050, 5070, 10000, 10050, 15000, 15500, 16000, 20000
};

char *agt_copy_ifid_for_file(const char *path)
{
    /* These are read after setjmp() returns from a longjmp(), so they must be
       volatile to survive the non-local jump. */
    volatile fc_type fc = NULL;
    char *volatile result = NULL;
    jmp_buf env;

    /* Route the AGT reader's fatal() back here instead of exit()ing. */
    agt_fatal_jmp = &env;
    if (setjmp(env) != 0) {
        /* A fatal error fired mid-parse: clean up and report failure. */
        agt_fatal_jmp = NULL;
        free_all_agtread();
        if (fc) {
            fc_type tmp = fc;
            release_file_context(&tmp);
        }
        return result;  /* still NULL */
    }

    /* --- Minimal reader init, matching agt2agx.c:main() so version/signature
       detection is identical to the converter. --- */
    init_flags();
    no_auxsyn = 1;
    fix_ascii_flag = 0;   /* don't translate the character set */

    PURE_ANSWER = PURE_TIME = PURE_ROOMTITLE = 2;
    PURE_AND = PURE_METAVERB = PURE_SYN = PURE_NOUN = PURE_ADJ = 2;
    PURE_DUMMY = PURE_SUBNAME = PURE_PROSUB = PURE_HOSTILE = 2;
    PURE_GETHOSTILE = PURE_DISAMBIG = PURE_ALL = 2;
    irun_mode = verboseflag = 2;

    build_trans_ascii();

    fc = init_file_context(path, fDA1);

    text_file = 1;
    read_config(openfile((fc_type)fc, fCFG, NULL, 0), 0);
    text_file = 0;

    if (readagt((fc_type)fc, 0) && aver >= 1 && aver <= AGX00) {
        int vercode = agx_version[aver];
        char buf[32];
        if (ver == 2 || ver == 4)   /* large / soggy */
            vercode += 1;
        snprintf(buf, sizeof buf, "AGT-%05d-%08X",
                 vercode, (unsigned int)(game_sig & 0xFFFFFFFFUL));
        result = strdup(buf);
    }

    free_all_agtread();
    if (fc) {
        fc_type tmp = fc;
        release_file_context(&tmp);
    }
    agt_fatal_jmp = NULL;
    return result;
}
