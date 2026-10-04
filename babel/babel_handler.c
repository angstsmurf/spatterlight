/* babel_handler.c   dispatches Treaty of Babel queries to the treaty modules
 * (c) 2006 By L. Ross Raszewski
 *
 * This code is freely usable for all purposes.
 *
 * This work is licensed under the Creative Commons Attribution 4.0 License.
 * To view a copy of this license, visit
 * https://creativecommons.org/licenses/by/4.0/ or send a letter to
 * Creative Commons,
 * PO Box 1866,
 * Mountain View, CA 94042, USA.
 *
 * This file depends upon register.c, misc.c, babel.h, and treaty.h
 * and L. Peter Deutsch's md5.c
 * usage:
 *  char *babel_init(char *filename)
 *      Initializes babel to use the specified file. MUST be called before
 *      babel_treaty.  Returns the human-readable name of the format
 *      or NULL if the format is not known. Do not call babel_treaty unless
 *      babel_init returned a nonzero value.
 *      The returned string will be the name of a babel format, possibly
 *      prefixed by "blorbed " to indicate that babel will process this file
 *      as a blorb.
 * int32 babel_treaty(int32 selector, void *output, void *output_extent)
 *      Dispatches the call to the treaty handler for the currently loaded
 *      file.
 *      When processing a blorb, all treaty calls will be deflected to the
 *      special blorb handler.  For the case of GET_STORY_FILE_IFID_SEL,
 *      The treaty handler for the underlying format will be called if an
 *      IFID is not found in the blorb resources.
 * void babel_release()
 *      Frees all resources allocated during babel_init.
 *      You should do this even if babel_init returned NULL.
 *      After this is called, do not call babel_treaty until after
 *      another successful call to babel_init.
 * char *babel_get_format()
 *      Returns the same value as the last call to babel_init (ie, the format name)
 * int32 babel_md5_ifid(char *buffer, int extent);
 *      Generates an MD5 IFID from the loaded story.  Returns zero if something
 *      went seriously wrong.
 *
 * If you wish to use babel in multiple threads, you must use the contextualized
 * versions of the above functions.
 * Each function above has a companion function whose name ends in _ctx (eg.
 * "babel_treaty_ctx") which takes one additional argument.  This argument is
 * the babel context. A new context is returned by void *ctx=get_babel_ctx(),
 * and should be released when finished by calling release_babel_ctx(ctx);
 */


#include "treaty.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "md5.h"

void *my_malloc(uint32, char *);

struct babel_handler
{
    TREATY treaty_handler;
    TREATY treaty_backup;
    void *story_file;
    uint32 story_file_extent;
    void *story_file_blorbed;
    uint32 story_file_blorbed_extent;
    char blorb_mode;
    char *format_name;
    char auth;
};

static struct babel_handler default_ctx;

extern TREATY treaty_registry[];
extern TREATY container_registry[];

/* Does the comma-separated extension list contain exactly this extension?
 * (A plain strstr would let ".d" or "." match ".dat".)
 */
static int extension_listed(const char *list, const char *ext)
{
    size_t l=strlen(ext);
    const char *p;

    for(p=list;(p=strstr(p,ext))!=NULL;p+=l)
        if ((p==list || p[-1]==',') && (p[l]==0 || p[l]==','))
            return 1;
    return 0;
}

/* buffer must hold TREATY_MINIMUM_EXTENT bytes; on success the format name
 * is written there and buffer is returned.
 */
static char *deeper_babel_init(char *story_name, void *bhp, char *buffer)
{
    struct babel_handler *bh=(struct babel_handler *) bhp;
    int i;
    char *ext;
    char extbuf[32];

    int best_candidate;
    char buffert[TREATY_MINIMUM_EXTENT];

    ext=NULL;
    if (story_name)
    {
        /* Work on a lowercased copy of the extension: the name belongs to
           the caller.  Only the last path component can have one.
         */
        char *base=story_name, *dot;
        for(dot=story_name;*dot;dot++)
            if (*dot=='/' || *dot=='\\') base=dot+1;
        dot=strrchr(base,'.');
        if (dot && dot[1] && strlen(dot)<sizeof(extbuf))
        {
            for(i=0;dot[i];i++) extbuf[i]=tolower((unsigned char) dot[i]);
            extbuf[i]=0;
            ext=extbuf;
        }
    }
    best_candidate=-1;
    i=0;
    if (ext) /* pass 1: try best candidates */
        for(i=0;container_registry[i];i++)
            if (container_registry[i](GET_FILE_EXTENSIONS_SEL,NULL,0,buffer,TREATY_MINIMUM_EXTENT) >=0 &&
                extension_listed(buffer,ext) &&
                container_registry[i](CLAIM_STORY_FILE_SEL,bh->story_file,bh->story_file_extent,NULL,0)>=NO_REPLY_RV)
                break;
    if (!ext || !container_registry[i]) /* pass 2: try all candidates */
    {

        for(i=0;container_registry[i];i++)
        {int l=container_registry[i](CLAIM_STORY_FILE_SEL,bh->story_file,bh->story_file_extent,NULL,0);

            if (l==VALID_STORY_FILE_RV)
                break;
            else if (l==NO_REPLY_RV && best_candidate < 0) best_candidate=i;
        }
    }
    if (!container_registry[i] && best_candidate >=0) { bh->auth=0; i=best_candidate; }
    if (container_registry[i])
    {
        char buffer2[TREATY_MINIMUM_EXTENT];
        int32 blorbed_extent;

        bh->treaty_handler=container_registry[i];
        container_registry[i](GET_FORMAT_NAME_SEL,NULL,0,buffert,TREATY_MINIMUM_EXTENT);
        bh->blorb_mode=1;

        /* Keep the reply signed until it is known not to be an error code */
        blorbed_extent=container_registry[i](CONTAINER_GET_STORY_EXTENT_SEL,bh->story_file,bh->story_file_extent,NULL,0);
        if (blorbed_extent>0)
        {
            bh->story_file_blorbed_extent=blorbed_extent;
            bh->story_file_blorbed=my_malloc(bh->story_file_blorbed_extent, "contained story file");
        }
        if (blorbed_extent<=0 ||
            container_registry[i](CONTAINER_GET_STORY_FORMAT_SEL,bh->story_file,bh->story_file_extent,buffer2,TREATY_MINIMUM_EXTENT)<0 ||
            container_registry[i](CONTAINER_GET_STORY_FILE_SEL,bh->story_file,bh->story_file_extent,bh->story_file_blorbed,bh->story_file_blorbed_extent)<=0
            )
            return NULL;

        for(i=0;treaty_registry[i];i++)
            if (treaty_registry[i](GET_FORMAT_NAME_SEL,NULL,0,buffer,TREATY_MINIMUM_EXTENT)>=0 &&
                strcmp(buffer,buffer2)==0 &&
                treaty_registry[i](CLAIM_STORY_FILE_SEL,bh->story_file_blorbed,bh->story_file_blorbed_extent,NULL,0)>=NO_REPLY_RV)
                break;
        if (!treaty_registry[i])
            return NULL;
        bh->treaty_backup=treaty_registry[i];
        snprintf(buffer,TREATY_MINIMUM_EXTENT,"%.200sed %.200s",buffert,buffer2);
        return buffer;
    }

    bh->blorb_mode=0;
    best_candidate=-1;

    if (ext) /* pass 1: try best candidates */
        for(i=0;treaty_registry[i];i++)
            if (treaty_registry[i](GET_FILE_EXTENSIONS_SEL,NULL,0,buffer,TREATY_MINIMUM_EXTENT) >=0 &&
                extension_listed(buffer,ext) &&
                treaty_registry[i](CLAIM_STORY_FILE_SEL,bh->story_file,bh->story_file_extent,NULL,0)>=NO_REPLY_RV)
                break;
    if (!ext || !treaty_registry[i]) /* pass 2: try all candidates */
    {

        for(i=0;treaty_registry[i];i++)
        {int l;
            l=treaty_registry[i](CLAIM_STORY_FILE_SEL,bh->story_file,bh->story_file_extent,NULL,0);

            if (l==VALID_STORY_FILE_RV)
                break;
            else if (l==NO_REPLY_RV && best_candidate < 0) best_candidate=i;
        }
    }
    if (!treaty_registry[i]) {
        if (best_candidate>=0) { i=best_candidate; bh->auth=0; }
        else return NULL;
    }
    bh->treaty_handler=treaty_registry[i];

    if (bh->treaty_handler(GET_FORMAT_NAME_SEL,NULL,0,buffer,TREATY_MINIMUM_EXTENT)>=0)
        return buffer;
    return NULL;


}

static char *deep_babel_init(char *story_name, void *bhp, char *buffer)
{
    struct babel_handler *bh=(struct babel_handler *) bhp;
    FILE *file;
    long length;

    bh->treaty_handler=NULL;
    bh->treaty_backup=NULL;
    bh->story_file=NULL;
    bh->story_file_extent=0;
    bh->story_file_blorbed=NULL;
    bh->story_file_blorbed_extent=0;
    bh->format_name=NULL;
    file=fopen(story_name, "rb");
    if (!file) return NULL;
    /* The treaty measures files in int32s, so anything we cannot size (a
       directory, say) or that is too big for one is not a story file.
     */
    if (fseek(file,0,SEEK_END)!=0 || (length=ftell(file))<0 ||
        length>0x7FFFFFFFL || fseek(file,0,SEEK_SET)!=0)
    {
        fclose(file);
        return NULL;
    }
    bh->auth=1;
    bh->story_file=my_malloc(length,"story file storage");
    /* A short read leaves the true size as the extent */
    bh->story_file_extent=fread(bh->story_file,1,length,file);
    fclose(file);

    return deeper_babel_init(story_name, bhp, buffer);
}

char *babel_init_ctx(char *sf, void *bhp)
{
    struct babel_handler *bh=(struct babel_handler *) bhp;
    char buffer[TREATY_MINIMUM_EXTENT];
    char *b;
    /* The name is handed back from the context, not from shared static
       storage, so that contexts on different threads cannot clobber it.
     */
    b=deep_babel_init(sf,bh,buffer);
    if (b) bh->format_name=strdup(b);
    return bh->format_name;
}
char *babel_init(char *sf)
{
    return babel_init_ctx(sf, &default_ctx);
}

char *babel_init_raw_ctx(void *sf, int32 extent, void *bhp)
{
    struct babel_handler *bh=(struct babel_handler *) bhp;
    char buffer[TREATY_MINIMUM_EXTENT];
    char *b;
    bh->treaty_handler=NULL;
    bh->treaty_backup=NULL;
    bh->story_file=NULL;
    bh->story_file_extent=0;
    bh->story_file_blorbed=NULL;
    bh->story_file_blorbed_extent=0;
    bh->format_name=NULL;
    bh->story_file_extent=extent;
    bh->auth=1;
    bh->story_file=my_malloc(bh->story_file_extent,"story file storage");
    memcpy(bh->story_file,sf,extent);

    b=deeper_babel_init(NULL, bhp, buffer);
    if (b) bh->format_name=strdup(b);
    return bh->format_name;
}
char *babel_init_raw(void *sf, int32 extent)
{
    return babel_init_raw_ctx(sf, extent, &default_ctx);
}

void babel_release_ctx(void *bhp)
{
    struct babel_handler *bh=(struct babel_handler *) bhp;
    if (bh->story_file) free(bh->story_file);
    bh->story_file=NULL;
    if (bh->story_file_blorbed) free(bh->story_file_blorbed);
    bh->story_file_blorbed=NULL;
    if (bh->format_name) free(bh->format_name);
    bh->format_name=NULL;
}
void babel_release(void)
{
    babel_release_ctx(&default_ctx);
}
int32 babel_md5_ifid_ctx(char *buffer, int32 extent, void *bhp)
{
    struct babel_handler *bh=(struct babel_handler *) bhp;
    md5_state_t md5;
    int i;
    unsigned char ob[16];
    if (extent <33 || bh->story_file==NULL)
        return 0;
    md5_init(&md5);
    md5_append(&md5,bh->story_file,bh->story_file_extent);
    md5_finish(&md5,ob);
    for(i=0;i<16;i++)
        sprintf(buffer+(2*i),"%02X",ob[i]);
    buffer[32]=0;
    return 1;

}
int32 babel_md5_ifid(char *buffer, int32 extent)
{
    return babel_md5_ifid_ctx(buffer, extent,
                              &default_ctx);
}

int32 babel_treaty_ctx(int32 sel, void *output, int32 output_extent,void *bhp)
{
    int32 rv;
    struct babel_handler *bh=(struct babel_handler *) bhp;
    if (!(sel & TREATY_SELECTOR_INPUT) && bh->blorb_mode)
        rv=bh->treaty_backup(sel,bh->story_file_blorbed,bh->story_file_blorbed_extent,output, output_extent);
    else
    {
        rv=bh->treaty_handler(sel,bh->story_file,bh->story_file_extent,output,output_extent);
        if ((!rv|| rv==UNAVAILABLE_RV) && bh->blorb_mode)
            rv=bh->treaty_backup(sel,bh->story_file_blorbed,bh->story_file_blorbed_extent,output, output_extent);
    }
    if (!rv && sel==GET_STORY_FILE_IFID_SEL)
        return babel_md5_ifid_ctx(output,output_extent, bh);
    if (rv==INCOMPLETE_REPLY_RV && sel==GET_STORY_FILE_IFID_SEL)
        return babel_md5_ifid_ctx((void *)((char *) output+strlen((char *)output)),
                                  output_extent-strlen((char *)output),
                                  bh);

    return rv;
}
int32 babel_treaty(int32 sel, void *output, int32 output_extent)
{
    return babel_treaty_ctx(sel, output, output_extent, &default_ctx);
}
char *babel_get_format_ctx(void *bhp)
{
    struct babel_handler *bh=(struct babel_handler *) bhp;
    return bh->format_name;
}
char *babel_get_format(void)
{
    return babel_get_format_ctx(&default_ctx);
}
void *get_babel_ctx(void)
{
    return my_malloc(sizeof(struct babel_handler), "babel handler context");
}
void release_babel_ctx(void *b)
{
    free(b);
}

uint32 babel_get_length_ctx(void *bhp)
{
    struct babel_handler *bh=(struct babel_handler *) bhp;
    return bh->story_file_extent;
}
uint32 babel_get_length(void)
{
    return babel_get_length_ctx(&default_ctx);
}

int32 babel_get_authoritative_ctx(void *bhp)
{
    struct babel_handler *bh=(struct babel_handler *) bhp;
    return bh->auth;
}
int32 babel_get_authoritative(void)
{
    return babel_get_authoritative_ctx(&default_ctx);
}
void *babel_get_file_ctx(void *bhp)
{
    struct babel_handler *bh=(struct babel_handler *) bhp;
    return bh->story_file;
}
void *babel_get_file(void)
{
    return babel_get_file_ctx(&default_ctx);
}

uint32 babel_get_story_length_ctx(void *ctx)
{
    struct babel_handler *bh=(struct babel_handler *) ctx;
    if (bh->blorb_mode) return bh->story_file_blorbed_extent;
    return bh->story_file_extent;
}
uint32 babel_get_story_length(void)
{

    return babel_get_story_length_ctx(&default_ctx);
}
void *babel_get_story_file_ctx(void *ctx)
{
    struct babel_handler *bh=(struct babel_handler *) ctx;
    if (bh->blorb_mode) return bh->story_file_blorbed;
    return bh->story_file;
}
void *babel_get_story_file(void)
{
    return babel_get_story_file_ctx(&default_ctx);
}
