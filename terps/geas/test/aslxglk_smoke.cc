// aslxglk_smoke.cc -- headless smoke harness for the Quest 5 Glk frontend
// (aslxglk.cc), built against the in-repo CheapGlk.  It stands in for
// geasglk.cc's glk_main (which needs glkimp's garglk extensions and the
// whole classic engine): a minimal glkunix startup that dispatches straight
// to aslx_glk_main, so the prompt loop / HTML renderer / menu-question-wait
// routing can be exercised by piping a command script to stdin.
//
//   make aslxglk_smoke
//   ASLX_CORE=../quest5/aslx-core ./aslxglk_smoke "<game.quest>" < commands.txt

extern "C" {
#include "glk.h"
#include "glkstart.h"
}

/* glkimp's real-time-delays preference; CheapGlk has no timers anyway. */
extern "C" int gli_sa_delays;
int gli_sa_delays = 0;

extern "C" int aslx_is_quest5_file(const char *path);
extern "C" void aslx_glk_main(const char *path);

static const char *storyfile = nullptr;

glkunix_argumentlist_t glkunix_arguments[] = {
    { (char *) "", glkunix_arg_ValueFollows, (char *) "filename: The game file to load." },
    { nullptr, glkunix_arg_End, nullptr }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    if (data->argc > 1)
        storyfile = data->argv[1];
    return 1;
}

void glk_main(void)
{
    if (!storyfile) {
        glk_put_string((char *) "usage: aslxglk_smoke <game.quest|game.aslx>\n");
        return;
    }
    if (!aslx_is_quest5_file(storyfile)) {
        glk_put_string((char *) "not a Quest 5 game file\n");
        return;
    }
    aslx_glk_main(storyfile);
}
