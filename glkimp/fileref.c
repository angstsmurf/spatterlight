#include "fileref.h"

#include <unistd.h> /* for unlink() */
#include <sys/stat.h> /* for stat() */

char *gli_workdir = NULL;
char *gli_game_path = NULL;
char *gli_parentdir = NULL;
int gli_parentdirlength = 0;

char *glkunix_fileref_get_filename(fileref_t *fref)
{
    return fref->filename;
}

char * garglk_fileref_get_name(frefid_t fref)
{
    return fref->filename;
}

/* This file implements filerefs as they work in a stdio system:
 * a fileref contains a pathname, a text/binary flag, and a file type.
 */

/* Linked list of all filerefs */
static fileref_t *gli_filereflist = NULL;

fileref_t *gli_new_fileref(char *filename, glui32 usage, glui32 rock)
{
	fileref_t *fref = (fileref_t *)malloc(sizeof(fileref_t));
	if (!fref)
		return NULL;

    fref->tag = generate_tag(); /* For serialization */

	fref->rock = rock;

    size_t length = 1 + strlen(filename);
	fref->filename = malloc(length);
    strncpy(fref->filename, filename, length);

	fref->textmode = ((usage & fileusage_TextMode) != 0);
	fref->filetype = (usage & fileusage_TypeMask);

	fref->prev = NULL;
	fref->next = gli_filereflist;
	gli_filereflist = fref;
	if (fref->next)
		fref->next->prev = fref;

	if (gli_register_obj)
		fref->disprock = (*gli_register_obj)(fref, gidisp_Class_Fileref);
	else
		fref->disprock.ptr = NULL;

	return fref;
}

void gli_delete_fileref(fileref_t *fref)
{
    fileref_t *prev, *next;

    if (gli_unregister_obj) {
        (*gli_unregister_obj)(fref, gidisp_Class_Fileref, fref->disprock);
        fref->disprock.ptr = NULL;
    }

    fref->magicnum = 0;

    if (fref->filename) {
        free(fref->filename);
        fref->filename = NULL;
    }

    prev = fref->prev;
    next = fref->next;
    fref->prev = NULL;
    fref->next = NULL;

    if (prev)
        prev->next = next;
    else
        gli_filereflist = next;
    if (next)
        next->prev = prev;

    free(fref);
}

void glk_fileref_destroy(fileref_t *fref)
{
	if (!fref)
	{
		gli_strict_warning("fileref_destroy: invalid ref");
		return;
	}
	gli_delete_fileref(fref);
}

static char *gli_suffix_for_usage(glui32 usage)
{
    switch (usage & fileusage_TypeMask) {
        case fileusage_Data:
            return ".glkdata";
        case fileusage_SavedGame:
            return ".glksave";
        case fileusage_Transcript:
        case fileusage_InputRecord:
            return ".txt";
        default:
            return "";
    }
}

frefid_t glk_fileref_create_temp(glui32 usage, glui32 rock)
{
    gettempdir();

    size_t temppathsize = sizeof(tempdir);
    char *temppath = malloc(temppathsize + 19);
    snprintf(temppath, temppathsize, "%s", tempdir);
    strncat(temppath, "/glktempfref-XXXXXX", temppathsize + 19);

    mkstemp(temppath);

    fileref_t *fref = gli_new_fileref(temppath, usage, rock);
	if (!fref)
	{
		gli_strict_warning("fileref_create_temp: unable to create fileref.");
        free(temppath);
		return NULL;
	}

    free(temppath);
	return fref;
}

frefid_t glk_fileref_create_from_fileref(glui32 usage, frefid_t oldfref, glui32 rock)
{
	fileref_t *fref;

	if (!oldfref)
	{
		gli_strict_warning("fileref_create_from_fileref: invalid ref");
		return NULL;
	}

	fref = gli_new_fileref(oldfref->filename, usage, rock);
	if (!fref)
	{
		gli_strict_warning("fileref_create_from_fileref: unable to create fileref.");
		return NULL;
	}

	return fref;
}

frefid_t gli_fileref_create_by_string_in_dir(glui32 usage, char *name, char *dirname, size_t dirlen,
                                    glui32 add_suffix, glui32 rock)
{
    fileref_t *fref;
    char buf[BUFLEN];
    char buf2[2*BUFLEN+10];
    unsigned long len;
    char *cx;
    char *suffix = NULL;

    /* The new spec recommendations: delete all characters in the
     string "/\<>:|?*" (including quotes). Truncate at the first
     period. Change to "null" if there's nothing left. Then append
     an appropriate suffix: ".glkdata", ".glksave", ".txt".
     */

    if (add_suffix) {
        for (cx=name, len=0; (*cx && *cx!='.' && len<BUFLEN-1); cx++) {
            switch (*cx) {
                case '"':
                case '\\':
                case '/':
                case '>':
                case '<':
                case ':':
                case '|':
                case '?':
                case '*':
                    break;
                default:
                    buf[len++] = *cx;
            }
        }
    } else {
        len = strnlen(name, BUFLEN);
        memcpy(buf, name, len);
    }

    buf[len] = '\0';

    if (len == 0) {
        strcpy(buf, "null");
        len = 5;
    }

    len += dirlen;
    if (add_suffix) {
        suffix = gli_suffix_for_usage(usage);
        len += strlen(suffix);
    }

    snprintf(buf2, len + 2, "%s/%s%s", dirname, buf, suffix);

    fref = gli_new_fileref(buf2, usage, rock);
    return fref;
}

frefid_t garglk_fileref_create_in_game_dir(glui32 usage, char *name, glui32 rock)
{
    if (gli_parentdir == NULL || gli_parentdirlength == 0) {
        gli_strict_warning("garglk_fileref_create_in_game_dir: no game directory is set.");
        return NULL;
    }

    fileref_t *fref = gli_fileref_create_by_string_in_dir(usage, name, gli_parentdir, gli_parentdirlength, 0, rock);

    if (!fref) {
        gli_strict_warning("garglk_fileref_create_in_game_dir: unable to create fileref.");
    }

    return fref;
}


frefid_t glk_fileref_create_by_name(glui32 usage, char *name,
                                    glui32 rock) {
    int result = create_workdir();
    if (result == 0) {
        gli_strict_warning("fileref_create_by_name: unable to create work directory.");
        return NULL;
    }

    size_t len = strlen(gli_workdir);
    fileref_t *fref = gli_fileref_create_by_string_in_dir(usage, name, gli_workdir, len, 1, rock);
    if (!fref) { gli_strict_warning("fileref_create_by_name: unable to create fileref.");
    }
    return fref;
}

frefid_t glk_fileref_create_by_prompt(glui32 usage, glui32 fmode, glui32 rock)
{
	fileref_t *fref;
	char *buf;
	unsigned long val = 0;

	if (fmode == filemode_Read)
		buf = win_promptopen(usage & fileusage_TypeMask);
	else
		buf = win_promptsave(usage & fileusage_TypeMask);
    if (buf != NULL)
        val = strlen(buf);
	if (!val)
	{
		/* The player just hit return. It would be nice to provide a
         default value, but this implementation is too cheap. */
		return NULL;
	}

	fref = gli_new_fileref(buf, usage, rock);
	if (!fref)
	{
		gli_strict_warning("fileref_create_by_prompt: unable to create fileref.");
		return NULL;
	}

	return fref;
}

frefid_t glk_fileref_iterate(fileref_t *fref, glui32 *rock)
{
	if (!fref)
	{
		fref = gli_filereflist;
	}
	else
	{
		fref = fref->next;
	}

	if (fref)
	{
		if (rock)
			*rock = fref->rock;
		return fref;
	}

	if (rock)
		*rock = 0;
	return NULL;
}

glui32 glk_fileref_get_rock(fileref_t *fref)
{
	if (!fref)
	{
		gli_strict_warning("fileref_get_rock: invalid ref.");
		return 0;
	}

	return fref->rock;
}

glui32 glk_fileref_does_file_exist(fileref_t *fref)
{
	struct stat buf;

	if (!fref)
	{
		gli_strict_warning("fileref_does_file_exist: invalid ref");
		return FALSE;
	}

	/* This is sort of Unix-specific, but probably any stdio library
     will implement at least this much of stat(). */

	if (stat(fref->filename, &buf))
		return 0;

	if (S_ISREG(buf.st_mode))
		return 1;
	else
		return 0;
}

void glk_fileref_delete_file(fileref_t *fref)
{
	if (!fref)
	{
		gli_strict_warning("fileref_delete_file: invalid ref");
		return;
	}

	/* If you don't have the unlink() function, obviously, change it
     to whatever file-deletion function you do have. */

	unlink(fref->filename);
}

/* This should only be called from startup code. */
void glkunix_set_base_file(char *filename)
{
    int ix;

    size_t len = strlen(filename);
    gli_game_path = malloc(len+1);
    strncpy(gli_game_path, filename, len);
    gli_game_path[len] = '\0';

    getworkdir();

    for (ix=(int)(strlen(filename)-1); ix >= 0; ix--)
        if (filename[ix] == '/')
            break;

    if (ix >= 0) {
        /* There is a slash. */
        gli_parentdir = malloc(ix + 1);
        gli_parentdirlength = ix;
        strncpy(gli_parentdir, filename, ix);
        gli_parentdir[ix] = '\0';
    }
    else {
        /* No slash, just a filename. */
        gli_parentdirlength = 2;
        gli_parentdir = malloc(2);
        strncpy(gli_parentdir, ".", 2);
    }
}

/* For autorestore */
fileref_t *gli_fileref_for_tag(int tag)
{
    fileref_t *fref = gli_filereflist;
    while (fref)
    {
        if (fref->tag == tag)
            return fref;
        fref = fref->next;
    }
    return NULL;
}

void gli_replace_fileref_list(fileref_t *newlist) /* Only used by autorestore */
{
    fileref_t *fref;

    if (!newlist)
    {
        gli_strict_warning("gli_replace_fileref_list: invalid ref");
        return;
    }

    /* At the time when this is called, the fileref list should be empty */
    while (gli_filereflist)
    {
        fref = gli_filereflist;
        gli_filereflist = gli_filereflist->next;
        glk_fileref_destroy(fref);
    }
    gli_filereflist = newlist;
}

void gli_sanity_check_filerefs(void)
{
    for (fileref_t *fref = glk_fileref_iterate(NULL, NULL); fref; fref = glk_fileref_iterate(fref, NULL)) {
        if (!fref->filename)
            fprintf(stderr, "gli_sanity_check_filerefs: fileref has no filename\n");
    }
}
