/* 
 *   Copyright (c) 2001 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32chest.cpp - htmltads game chest for windows
Function

Notes

Modified
  09/08/01 MJRoberts  - creation
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>

#include <windows.h>
#include <wininet.h>

#include <os.h>

#include "tadshtml.h"
#include "tadsapp.h"
#include "htmlsys.h"
#include "tadswin.h"
#include "html_os.h"
#include "htmlw32.h"
#include "htmlfmt.h"
#include "htmlprs.h"
#include "htmlpref.h"
#include "tadsdlg.h"
#include "htmlres.h"
#include "tadsreg.h"
#include "foldsel.h"
#include "w32main.h"

/* tads 3 includes */
#include "vmmain.h"

/* tads 3 utilities */
#include "gameinfo.h"


/* ------------------------------------------------------------------------ */
/*
 *   Game chest name/value entry 
 */
class CHtmlGameChestValue
{
public:
    CHtmlGameChestValue(const char *nm, const char *val)
    {
        /* allocate and store the name and value */
        name_.set(nm);
        val_.set(val);
    }

    /* append text to the value */
    void append_val(const char *more_val) { val_.append(more_val); }

    /* name and value strings */
    CStringBuf name_;
    CStringBuf val_;
};

/*
 *   Game chest entry - this is an entry for a single item 
 */
class CHtmlGameChestEntry
{
public:
    CHtmlGameChestEntry()
    {
        /* allocate an initial set of name/value pair pointers */
        alloc_ = 5;
        used_ = 0;
        arr_ = (CHtmlGameChestValue **)th_malloc(
            alloc_ * sizeof(CHtmlGameChestValue *));
    }

    ~CHtmlGameChestEntry()
    {
        size_t i;

        /* delete each allocated entry */
        for (i = 0 ; i < used_ ; ++i)
            delete arr_[i];
        
        /* delete the array */
        th_free(arr_);
    }

    /* add an integer value */
    CHtmlGameChestValue *add_value(const char *nm, int val)
    {
        char buf[32];

        /* make a string out of the value */
        sprintf(buf, "%d", val);

        /* add it as a normal string value */
        return add_value(nm, buf);
    }

    /* add a new name/value pair, or change an existing value */
    CHtmlGameChestValue *add_value(const char *nm, const char *val)
    {
        CHtmlGameChestValue *new_val;

        /* try to find an existing instance of the value */
        new_val = find_value(nm);

        /* if we couldn't find an existing one, allocate a new one */
        if (new_val == 0)
        {
            /* allocate the value */
            new_val = new CHtmlGameChestValue(nm, val);

            /* expand our pointer array if necessary */
            if (used_ == alloc_)
            {
                /* increase the allocation size */
                alloc_ += 10;
                
                /* allocate the new space */
                arr_ = (CHtmlGameChestValue **)th_realloc(
                    arr_, alloc_ * sizeof(CHtmlGameChestValue *));
            }

            /* store the new value in our pointer array */
            arr_[used_++] = new_val;
        }
        else
        {
            /* update the existing value */
            new_val->val_.set(val);
        }

        /* return the new value object */
        return new_val;
    }

    /* delete an existing value from our array */
    void del_value(const char *nm)
    {
        size_t i;
        
        /* get the index of this value */
        i = find_value_idx(nm);

        /* if it's not in there, there's nothing to do */
        if (i == used_)
            return;

        /* delete this entry */
        delete arr_[i];

        /* move all the other entries down a notch */
        for ( ; i+1 < used_ ; ++i)
            arr_[i] = arr_[i+1];

        /* that's one less value used */
        --used_;
    }

    /* get the value for a given name string */
    const char *get_value(const char *nm)
    {
        CHtmlGameChestValue *val;
        
        /* try finding the value object */
        val = find_value(nm);

        /* if we found it, return its string value; otherwise, return null */
        return (val != 0 ? val->val_.get() : 0);
    }

    /* find a value entry */
    CHtmlGameChestValue *find_value(const char *nm) const
    {
        size_t i;

        /* get the index for this value */
        i = find_value_idx(nm);

        /* if we found it, return the value object; otherwise return null */
        return (i < used_ ? arr_[i] : 0);
    }

    /* find the index of a value - returns used_ if not found */
    size_t find_value_idx(const char *nm) const
    {
        size_t i;

        /* find the name, ignoring case */
        for (i = 0 ; i < used_ ; ++i)
        {
            /* if this name matches, return its index */
            if (stricmp(arr_[i]->name_.get(), nm) == 0)
                return i;
        }

        /* there's no such key - indicate this by returning used_ */
        return used_;
    }

    /* write a value to a file, if the value exists */
    void write_value_to_file(FILE *fp, const char *nm, int wrap)
    {
        const char *val;
        size_t max_brk_pt;

        /* get the value - if it doesn't exist, there's nothing to do */
        if ((val = get_value(nm)) == 0)
            return;

        /* write the name */
        fprintf(fp, "%s: ", nm);

        /* break at 80 characters minus the name prefix length */
        max_brk_pt = 79 - strlen(nm) - 2;

        /* check for wrapping */
        if (*val == '\0')
        {
            /* no value - just leave it blank */
            fprintf(fp, "\n");
        }
        else if (wrap)
        {
            /* we're wrapping - break long lines */
            while (strlen(val) > max_brk_pt)
            {
                const char *p;
                
                /* find the last space before the maximum break point */
                for (p = val + max_brk_pt ; p > val && !isspace(*p) ; --p);

                /* 
                 *   if we didn't find any spaces before the maximum break
                 *   point, find the next space after this point 
                 */
                if (p == val)
                {
                    /* 
                     *   scan for the next space after the maximum break
                     *   point 
                     */
                    for (p = val + max_brk_pt ;
                         *p != '\0' && !isspace(*p) ; ++p) ;
                }

                /* write out the part up to here */
                fprintf(fp, "%.*s\n", (int)(p - val), val);

                /* advance to the break character */
                val = p;

                /* 
                 *   wrap the next line at 79 (to account for the leading
                 *   space on continuation lines) 
                 */
                max_brk_pt = 79;
            }

            /* the remainder all fits on one line */
            if (*val != '\0')
                fprintf(fp, "%s\n", val);
        }
        else
        {
            /* not wrapping - just write the entire value at once */
            fprintf(fp, "%s\n", val);
        }
    }

    /* array of pointers to name/value pair entries */
    CHtmlGameChestValue **arr_;

    /* number of entries allocated in the array */
    size_t alloc_;

    /* number of entries actually used in the array */
    size_t used_;
};

/*
 *   Game chest entry sorting record.
 */
struct gch_entry_sorter
{
    /* this entry */
    CHtmlGameChestEntry *entry;

    /* the value on which to sort the record */
    const textchar_t *sort_val;
};

/*
 *   Game chest group array - each array holds entries for a type of item
 *   (URL, game) 
 */
class CHtmlGameChestGroup
{
public:
    CHtmlGameChestGroup(const char *const *sort_keys, size_t sort_key_cnt)
    {
        /* allocate an initial set of entries */
        alloc_ = 10;
        used_ = 0;
        arr_ = (CHtmlGameChestEntry **)th_malloc(
            alloc_ * sizeof(CHtmlGameChestEntry *));

        /* remember my sorting keys */
        sort_keys_ = sort_keys;
        sort_key_cnt_ = sort_key_cnt;
    }

    ~CHtmlGameChestGroup()
    {
        size_t i;

        /* delete each allocated entry */
        for (i = 0 ; i < used_ ; ++i)
            delete arr_[i];

        /* delete the array */
        th_free(arr_);
    }

    /* add a new array entry */
    CHtmlGameChestEntry *add_entry()
    {
        CHtmlGameChestEntry *new_entry;

        /* allocate our new entry */
        new_entry = new CHtmlGameChestEntry();
        
        /* expand our pointer array if necessary */
        if (used_ == alloc_)
        {
            /* increase the allocation size */
            alloc_ += 10;

            /* allocate the new space */
            arr_ = (CHtmlGameChestEntry **)th_realloc(
                arr_, alloc_ * sizeof(CHtmlGameChestEntry *));
        }

        /* store the new entry in our pointer array */
        arr_[used_++] = new_entry;

        /* return the new entry */
        return new_entry;
    }

    /* delete an entry */
    void del_entry(size_t idx)
    {
        CHtmlGameChestEntry **entry;

        /* delete the entry we're removing */
        delete arr_[idx];
        
        /* move everything down a slot in the web link array */
        for (entry = &arr_[idx] ; idx + 1 < used_ ; ++idx, ++entry)
        {
            /* move this entry down a slot */
            *entry = *(entry + 1);
        }
        
        /* that's one less entry in the list */
        --used_;
    }

    /* 
     *   Sort my list by the given values, in the given order of precedence.
     *   These aren't multiple keys - we'll sort by the first of the given
     *   keys we find a value to have.  
     */
    void sort()
    {
        gch_entry_sorter *sort_list;
        size_t i;
        
        /* allocate a sorting array with one entry per entry in our list */
        sort_list = (gch_entry_sorter *)
                    th_malloc(used_ * sizeof(gch_entry_sorter));

        /* set up the sorter list with the entries */
        for (i = 0 ; i < used_ ; ++i)
        {
            size_t k;

            /* set this sorter list entry to this array entry */
            sort_list[i].entry = arr_[i];

            /* use a null string if we fail to find any of the keys */
            sort_list[i].sort_val = "";

            /* find the first of the given keys this entry provides */
            for (k = 0 ; k < sort_key_cnt_ ; ++k)
            {
                const textchar_t *val;

                /* get the value for this key */
                if ((val = arr_[i]->get_value(sort_keys_[k])) != 0)
                {
                    /* 
                     *   this is the one - store a reference to this value
                     *   string in the sorter list 
                     */
                    sort_list[i].sort_val = val;

                    /* no need to look for another key */
                    break;
                }
            }
        }

        /* sort the list through our callback */
        qsort(sort_list, used_, sizeof(sort_list[0]), &sort_cb);

        /* 
         *   now copy the sort list back into our own array, since we have
         *   the same entries as our original list but now in the correct
         *   order 
         */
        for (i = 0 ; i < used_ ; ++i)
            arr_[i] = sort_list[i].entry;

        /* we're done with the sorter list - free it */
        th_free(sort_list);
    }

    /* 
     *   Assign numeric ID's with the given value name to all of our entries
     *   that don't have that named value defined. 
     */
    void validate_id_values(const char *id_name)
    {
        size_t i;
        int max_id;
        
        /*
         *   Run through the list and find the high-water mark of the
         *   existing entries. 
         */
        for (i = 0, max_id = 0 ; i < used_ ; ++i)
        {
            const char *p;
            int id;
            
            /* if this one has an ID, see if it's the highest yet */
            if ((p = arr_[i]->get_value("id")) != 0
                && (id = atoi(p)) > max_id)
            {
                /* it's the highest so far - note it */
                max_id = id;
            }
        }
        
        /* now assign new ID's to any items that don't have them */
        for (i = 0 ; i < used_ ; ++i)
        {
            const char *p;

            /* if this one doesn't have a non-zero ID, assign it a new ID */
            if ((p = arr_[i]->get_value("id")) == 0
                || atoi(p) == 0)
            {
                /* use the next available ID, and consume it */
                ++max_id;
                
                /* assign the ID to the group */
                arr_[i]->add_value("id", max_id);
            }
        }
    }

    /* qsort callback to compare two sort list entries */
    static int sort_cb(const void *a0, const void *b0)
    {
        gch_entry_sorter *a;
        gch_entry_sorter *b;
        
        /* cast the values to sort list entries */
        a = (gch_entry_sorter *)a0;
        b = (gch_entry_sorter *)b0;

        /* sort based on the key value strings, without regard to case */
        return stricmp(a->sort_val, b->sort_val);
    }

    /* array of pointers to entries */
    CHtmlGameChestEntry **arr_;

    /* number of entries allocated in the array */
    size_t alloc_;

    /* number of entries actually used in the array */
    size_t used_;

    /* my sorting key array */
    const char *const *sort_keys_;
    size_t sort_key_cnt_;
};

/* sorting keys for the web, game, and group groups */
static const char *const web_keys[] = { "name", "url" };
static const char *const game_keys[] = { "name", "file" };
static const char *const group_keys[] = { "name" };

/*
 *   Game Chest information 
 */
class CHtmlGameChest
{
public:
    /* create */
    CHtmlGameChest()
        : web_group_(web_keys, sizeof(web_keys)/sizeof(web_keys[0])),
          game_group_(game_keys, sizeof(game_keys)/sizeof(game_keys[0])),
          group_group_(group_keys, sizeof(group_keys)/sizeof(group_keys[0]))
    {
        /* no changes yet */
        need_to_save_ = FALSE;
    }

    /* delete */
    ~CHtmlGameChest()
    {
    }

    /* [web] group list */
    CHtmlGameChestGroup web_group_;

    /* [game] group list */
    CHtmlGameChestGroup game_group_;

    /* [group] group list */
    CHtmlGameChestGroup group_group_;

    /* flag: we need to save changes */
    int need_to_save_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Check for the presence of the Game Chest feature in the build 
 */
int CHtmlSys_mainwin::is_game_chest_present()
{
    /* game chest is present in this build */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Re-load the game chest data if present 
 */
void CHtmlSys_mainwin::owner_maybe_reload_game_chest()
{
    /* run through the activation procedure */
    do_activate_game_chest(TRUE);
}

/*
 *   Save the game chest database to the given file 
 */
void CHtmlSys_mainwin::owner_write_game_chest_db(const textchar_t *fname)
{
    /* write the game chest data to the given file */
    write_game_chest_file(fname);
}

/* ------------------------------------------------------------------------ */
/*
 *   offer the game chest, via a hyperlink in the current window 
 */
void CHtmlSys_mainwin::offer_game_chest()
{
    /* make sure we're in markup mode */
    parser_->obey_markups(TRUE);

    /* add a link to the game chest to the output */
    display_outputz("<p><br><br><font face=tads-roman size=-1>"
                    "(<a title='Go to the Game Chest' href='");
    display_outputz(SHOW_GAME_CHEST_CMD);
    display_outputz("' forced>Click here to go to the Game Chest page</a>)"
                    "</font>");
    flush_txtbuf(TRUE, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Look up an entry in the game chest records, given the filename of the
 *   game.  Returns the game chest entry.  
 */
CHtmlGameChestEntry *CHtmlSys_mainwin::look_up_fav_by_filename(
    const textchar_t *fname)
{
    size_t j;

    /* make sure we have the latest game chest data loaded */
    read_game_chest_file_if_needed();

    /* scan our game list for a game with the given filename */
    for (j = 0 ; j < gch_info_->game_group_.used_ ; ++j)
    {
        CHtmlGameChestEntry *entry;
        const textchar_t *entry_fname;
        
        /* get this item */
        entry = gch_info_->game_group_.arr_[j];
        
        /* if this entry has the same filename, use its name */
        if ((entry_fname = entry->get_value("file")) != 0
            && stricmp(fname, entry_fname) == 0)
        {
            /* this is it - return the entry */
            return entry;
        }
    }

    /* didn't find a match */
    return 0;
}

/*
 *   Look up a game chest entry by filename, returning the game title.
 *   Returns null if no such entry is found or it has no title. 
 */
const textchar_t *CHtmlSys_mainwin::look_up_fav_title_by_filename(
    const textchar_t *fname)
{
    CHtmlGameChestEntry *entry;
    
    /* look up the entry */
    entry = look_up_fav_by_filename(fname);

    /* 
     *   if we found an entry, return the "name" property; otherwise, there's
     *   obviously no title to return 
     */
    return (entry != 0 ? entry->get_value("name") : 0);
}


/* ------------------------------------------------------------------------ */
/*
 *   show the game chest
 */
void CHtmlSys_mainwin::show_game_chest(int refresh_only,
                                       int reload_file, int mark_dirty)
{
    size_t i;
    const char *fname;

    /* make sure we're on the active page (rather than a history page) */
    go_to_active_page();

    /*
     *   If we're not currently showing the game chest, and they're only
     *   refreshing, don't show the game chest anew, since they only want to
     *   refresh it if it's already showing.  
     */
    if (refresh_only && !main_panel_->is_in_game_chest())
    {
        /* if they want to mark the game chest dirty, save it */
        if (mark_dirty)
        {
            /* mark it as needing saving */
            gch_info_->need_to_save_ = TRUE;

            /* save it */
            save_game_chest_file();
        }

        /* that's all we need to do */
        return;
    }

    /* load the profile selected for Game Chest */
    load_game_specific_profile(":GameChest");

    /* clear out all of the pages from the previous game */
    clear_all_pages();

    /* tell the main panel we're showing the game chest */
    main_panel_->set_in_game_chest(TRUE);

    /* update the status line for game chest mode */
    if (statusline_ != 0)
        statusline_->update();

    /* make sure we're in markup mode */
    parser_->obey_markups(TRUE);
    parser_->obey_end_markups(TRUE);
    parser_->obey_whitespace(FALSE, FALSE);

    /* read the game chest file if desired */
    if (reload_file)
        read_game_chest_file();

    /* if there's no information object, there's nothing we can display */
    if (gch_info_ == 0)
        return;

    /* mark the information object as needing to be saved if desired */
    if (mark_dirty)
        gch_info_->need_to_save_ = TRUE;

    /* start the BODY tag for the page */
    display_outputz("<body");

    /* add the background image, if appropriate */
    fname = prefs_->get_gc_bkg();
    if (fname != 0 && fname[0] != '\0')
    {
        /* show the 'background' attribute and open quote */
        display_outputz(" background=");
        display_outputz_quoted(fname);
    }

    /* write the rest of our header text */
    display_outputz(" text=black>"
                    "<font size=+2 face=tads-sans color=#800080>"
                    "<b>My Game Chest</b></font>"
                    "<br>"
                    "Welcome to the TADS Game Chest!  You can use the Game "
                    "Chest to keep track of your favorite TADS story files "
                    "and Web sites."
                    "<p>To start a story listed below, just click on "
                    "the story's title."
                    "<br><br>"
                    "<a href='browse-play' title='Browse for a story file'>"
                    "<img src='exe:gamechest/browse.png' "
                    "alt='Browse my computer for a TADS story file'></a>"
                    "<br><br><br>"
                    "<font size=+1 face=tads-sans color=#800080>"
                    "<b>Most-Recent Stories</b></font>"
                    // "<br><font size=-1>"
                    // "(<a href='clear-recent'>clear this list</a>)"
                    // "</font>"
                    "<br><br>"
                   );

    /* add the recent game listing */
    {
        const textchar_t *order;
        size_t len;
        int num_listed;

        /* get the order string */
        order = prefs_->get_recent_game_order();

        /* start the list */
        display_outputz("<ul>");

        /* add each item in the list */
        for (i = 0, num_listed = 0, len = strlen(order) ; i < len ; ++i)
        {
            const textchar_t *fname;
            char cur_idx[2];

            /* get this item */
            fname = prefs_->get_recent_game(order[i]);

            /* set up a one-character string with the item number */
            cur_idx[0] = '0' + i;
            cur_idx[1] = '\0';

            /* if the file exists, add it to the list; otherwise skip it */
            if (osfacc(fname) == 0)
            {
                const char *nm;
                CHtmlGameChestEntry *entry;

                /* note that we're listing another item */
                ++num_listed;

                /* try to find the full game name */
                entry = look_up_fav_by_filename(fname);

                /* if we found an entry, get its name */
                nm = (entry != 0 ? entry->get_value("name") : 0);

                /* add this item's prefix */
                display_outputz("<li><a title='Begin this story' "
                                "href='play-recent/");
                display_outputz(cur_idx);
                display_outputz("'>");

                /* 
                 *   if we have a name, display it; otherwise, display the
                 *   root filename as the title 
                 */
                display_plain_output(nm != 0
                                     ? nm : os_get_root_name((char *)fname),
                                     TRUE);

                /* finish the link */
                display_outputz("</a> ");

                /* add the full filename, in small type */
                display_outputz("<font size=-1>");
                display_outputz(" (");
                display_plain_output(fname, TRUE);
                display_outputz(")");

                /* 
                 *   add an "add to favorites" link, if it's not already in
                 *   the favorites (it's not if we didn't find a record) 
                 */
                if (entry == 0)
                {
                    display_outputz("&nbsp;&nbsp;&nbsp;&nbsp;"
                                    "<a title='Add this to the favorites' "
                                    "href='add-recent/");
                    display_outputz(cur_idx);
                    display_outputz("'>add to favorites</a>");
                }

                /* end the entry */
                display_outputz("</font>");
            }
        }

        /* end the list */
        display_outputz("</ul>");

        /* if we didn't list anything, mention it */
        if (num_listed == 0)
            display_outputz("(You haven't viewed any TADS stories recently "
                            "on this computer.)");

        /* add some vertical whitespace */
        display_outputz("<br><br>");
    }

    /* add the favorite games prolog */
    display_outputz("<br><font size=+1 face=tads-sans color=#800080>"
                    "<b>Favorite Stories</b></font><br>"
                    "<b>Tip:</b> You can add a story file to this list by "
                    "dragging the file's icon from a Windows Explorer folder "
                    "and dropping it directly on this window.<br>");

    /* if there are no groups yet, show a tip about creating them */
    if (gch_info_->group_group_.used_ == 0)
        display_outputz("<b>Tip:</b> If you have lots of TADS story files, "
                        "you can organize them into groups for easier "
                        "browsing - "
                        "<a title='Create a new group' "
                        "href='edit-groups'>click here to create a new "
                        "group</a>.<br>");

    /* add the rest of the favorites prolog */
    display_outputz("<br>"
                    "<a title='Browse for a story file' href='browse-add'>"
                    "<img src='exe:gamechest/game.png' "
                    "alt='Browse my computer for a TADS "
                    "story file and add it to this list'></a>"
                    "&nbsp;&nbsp; <a title='Search for story files' "
                    "href='search-add'>"
                    "<img src='exe:gamechest/batch.png' "
                    "alt='Search a folder and its sub-folders "
                    "for story files and add them to this list'></a>"
                    "&nbsp;&nbsp;  <a title='Manage groups' "
                    "href='edit-groups'>"
                    "<img src='exe:gamechest/groups.png' "
                    "alt='Manage my groups'></a>"
                    "<br><br>");

    /* add the favorite games */
    if (gch_info_->game_group_.used_ != 0)
    {
        size_t group_idx;
        int cur_group_id;
        CHtmlGameChestEntry *cur_group_entry;

        /* 
         *   Run through the group list, showing each group's games
         *   together.  Show the unnamed default group's games first.  The
         *   default group has the special group ID zero and has no group
         *   list entry.  
         */
        for (cur_group_id = 0, cur_group_entry = 0, group_idx = 0 ; ;
             ++group_idx)
        {
            const char *group_name;
            
            /* 
             *   show this group's name, if we're on a real group (rather
             *   than the unnamed default group) 
             */
            if (cur_group_entry != 0
                && (group_name = cur_group_entry->get_value("name")) != 0)
            {
                display_outputz("<font face=tads-sans color=#4020D0><b>");
                display_plain_output(group_name, TRUE);
                display_outputz("</b></font><br>");
            }

            /* start the list */
            display_outputz("<ul>");

            /* list all of the games that belong to this group */
            for (i = 0 ; i < gch_info_->game_group_.used_ ; ++i)
            {
                CHtmlGameChestEntry *entry;
                const char *fname;
                const char *nm;
                const char *desc;
                const char *htmldesc;
                const char *byline;
                const char *htmlbyline;
                char idx_buf[20];

                /* build the index number for this entry */
                sprintf(idx_buf, "%d", i);

                /* get this entry */
                entry = gch_info_->game_group_.arr_[i];

                /* if this game isn't part of the current group, skip it */
                if (atoi(entry->get_value("group-id")) != cur_group_id)
                    continue;

                /* if this entry doesn't have a filename, skip it */
                if ((fname = entry->get_value("file")) == 0)
                    continue;

                /* get the entry's name and description */
                nm = entry->get_value("name");
                desc = entry->get_value("desc");
                htmldesc = entry->get_value("htmldesc");
                byline = entry->get_value("byline");
                htmlbyline = entry->get_value("htmlbyline");

                /* add its display */
                display_outputz("<li><a title='Begin this story' "
                                "href='play-game/");
                display_outputz(idx_buf);
                display_outputz("'>");
                
                /* 
                 *   if it has a name, display the name; otherwise, display
                 *   the filename as the title 
                 */
                display_plain_output(nm != 0 ? nm : fname, TRUE);
                display_outputz("</a> ");
                
                /* if it had a name, add the filename in parens */
                if (nm != 0)
                {
                    display_outputz("<font size=-1>(");
                    display_outputz(fname);
                    display_outputz(")</font>");
                }

                /* add the edit links */
                display_outputz("&nbsp;&nbsp;&nbsp;&nbsp;<font size=-1>"
                                "<a title='Edit this item' href='edit-game/");
                display_outputz(idx_buf);
                display_outputz("'>edit</a>&nbsp;&nbsp;&nbsp;"
                                "<a title='Remove this item from favorites' "
                                "href='remove-game/");
                display_outputz(idx_buf);
                display_outputz("'>remove</a></font>");

                /* if there's a byline, show it on the next line */
                if (htmlbyline != 0)
                {
                    /* write the html byline on a new line */
                    display_outputz("<br>");
                    display_outputz(htmlbyline);
                }
                else if (byline != 0)
                {
                    /* write the plain byline on a new line */
                    display_outputz("<br>");
                    display_plain_output(byline, TRUE);
                }
                
                /* if there's a description, show it */
                if (htmldesc != 0)
                {
                    /* write the html description on a new line */
                    display_outputz("<br>");
                    display_outputz(htmldesc);
                }
                else if (desc != 0)
                {
                    /* write the plain text description on a new line */
                    display_outputz("<br>");
                    display_plain_output(desc, TRUE);
                }
            }

            /* end the list */
            display_outputz("</ul><br>");

            /* if that was the last group, we're done */
            if (group_idx >= gch_info_->group_group_.used_)
                break;

            /* get the ID of the next group */
            cur_group_entry = gch_info_->group_group_.arr_[group_idx];
            cur_group_id = atoi(cur_group_entry->get_value("id"));
        }

        /* end the list of groups */
        display_outputz("<br>");
    }
    else
    {
        /* the favorites list is empty - add an explanation */
        display_outputz("(Your Favorite Stories list is currently empty.  "
                        "If you'd like, you can add story files to this list "
                        "so that you don't have to find the files "
                        "on your hard disk every time you want to play.)"
                        "<br><br>");
    }

    /* add the internet prolog */
    display_outputz("<br><font size=+1 face=tads-sans color=#800080>"
                    "<b>Web Sites</b></font><br>"
                    "This section lets you create links to your favorite "
                    "TADS sites on the Internet."
                    "<p>To view a story file you find on the Web, use your "
                    "Web browser to download the story file to your computer, "
                    "then start the story using your downloaded copy.  "
                    "<b>Tip:</b> If you're using Internet Explorer, simply "
                    "drag a story file's hyperlink from the browser window "
                    "and drop it directly on this window -  TADS will "
                    "automatically download the story file and add it to your "
                    "Favorites list.<br><br>"
                    "<a title='Add a favorite Web site' href='add-url'>"
                    "<img src='exe:gamechest/web.png' alt='Add a Web "
                    "site to this list'></a><br><br>"
                   );

    /* add the favorite web sites */
    if (gch_info_->web_group_.used_ != 0)
    {
        /* start the list */
        display_outputz("<ul>");

        /* add each site */
        for (i = 0 ; i < gch_info_->web_group_.used_ ; ++i)
        {
            CHtmlGameChestEntry *entry;
            const char *url;
            const char *nm;
            const char *desc;
            const char *htmldesc;
            char idx_buf[20];

            /* build the index number for this entry */
            sprintf(idx_buf, "%d", i);

            /* get this entry */
            entry = gch_info_->web_group_.arr_[i];

            /* if this entry doesn't have a url, skip it */
            if ((url = entry->get_value("url")) == 0)
                continue;

            /* get the entry's name and description */
            nm = entry->get_value("name");
            desc = entry->get_value("desc");
            htmldesc = entry->get_value("htmldesc");

            /* add its display */
            display_outputz("<li><a title='Go to this Web site' "
                            "href='go-url/");
            display_outputz(idx_buf);
            display_outputz("'>");

            /* 
             *   if it has a name, display the name; otherwise, display the
             *   url as the name 
             */
            display_plain_output(nm != 0 ? nm : url, TRUE);
            display_outputz("</a> ");

            /* if it had a name, add the url in parens */
            if (nm != 0)
            {
                display_outputz("<font size=-1>(");
                display_plain_output(url, TRUE);
                display_outputz(")</font>");
            }

            /* add the edit links */
            display_outputz("&nbsp;&nbsp;&nbsp;&nbsp;<font size=-1>"
                            "<a title='Edit this item' href='edit-url/");
            display_outputz(idx_buf);
            display_outputz("'>edit</a>&nbsp;&nbsp;&nbsp;"
                            "<a title='Remove this item' href='remove-url/");
            display_outputz(idx_buf);
            display_outputz("'>remove</a></font>");

            /* if there's a byline, show it */

            /* if there's a description, show it */
            if (htmldesc != 0)
            {
                /* write the html description on a new line */
                display_outputz("<br>");
                display_outputz(htmldesc);
            }
            else if (desc != 0)
            {
                /* write the plain text description on a new line */
                display_outputz("<br>");
                display_plain_output(desc, TRUE);
            }
        }

        /* end the list */
        display_outputz("</ul><br><br>");
    }
    else
    {
        /* the list is empty - note it */
        display_outputz("(Your Web Site list is currently empty.)<br><br>");
    }

    /* display the text */
    flush_txtbuf(TRUE, FALSE);

    /* clear the timer display, if present */
    if (prefs_->get_show_timer() && statusline_ != 0)
        SendMessage(statusline_->get_handle(), SB_SETTEXT, 1, (LPARAM)"");
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the name of the game chest file.  If the directory that
 *   conventionally contains the file doesn't exist, we'll attempt to create
 *   it.  
 */
const textchar_t *CHtmlSys_mainwin::get_game_chest_filename()
{
    /* return the game chest filename from the preferences */
    return prefs_->get_gc_database();
}

/*
 *   Get the timestamp on the game chest file 
 */
void CHtmlSys_mainwin::get_game_chest_timestamp(FILETIME *ft)
{
    const textchar_t *fname;
    WIN32_FIND_DATA info;
    HANDLE hfind;

    /* get the name of the game chest data file */
    fname = get_game_chest_filename();

    /* look up information on the file */
    hfind = FindFirstFile(fname, &info);

    /* if we got a valid handle, note the timestamp */
    if (hfind != INVALID_HANDLE_VALUE)
    {
        /* 
         *   remember the timestamp - we care about the modification date,
         *   since this will tell us if anyone has changed the file since we
         *   loaded it and thus if our in-memory information is out of sync
         *   with the underlying file 
         */
        memcpy(ft, &info.ftLastWriteTime, sizeof(*ft));

        /* we're done with the find, so close it */
        FindClose(hfind);
    }
    else
    {
        /* 
         *   we couldn't get information on the file - set the timestamp to
         *   all zeroes 
         */
        memset(ft, 0, sizeof(*ft));
    }
}

/*
 *   Note the game chest background image file, and check for changes since
 *   the last time we checked.  Returns true if the image has changed, which
 *   indicates that we need to redisplay the game chest page.  
 */
int CHtmlSys_mainwin::note_gc_bkg()
{
    const textchar_t *new_fname;
    const textchar_t *old_fname;
    int changed;

    /* get the new filename from the preferences */
    new_fname = prefs_->get_gc_bkg();
    if (new_fname == 0)
        new_fname = "";

    /* get the old filename */
    old_fname = gch_bkg_.get();
    if (old_fname == 0)
        old_fname = "";

    /* check for a change */
    changed = (stricmp(new_fname, old_fname) != 0);

    /* remember the new filename */
    gch_bkg_.set(new_fname);

    /* return the change indication */
    return changed;
}


/* ------------------------------------------------------------------------ */
/*
 *   Save the game chest file 
 */
void CHtmlSys_mainwin::save_game_chest_file()
{
    /* 
     *   if we don't have a game chest info object, or it's marked as clean,
     *   there's nothing to do 
     */
    if (gch_info_ == 0 || !gch_info_->need_to_save_)
        return;

    /* write the game chest data to the current game chest file */
    write_game_chest_file(get_game_chest_filename());
}

/*
 *   Write the game chest data to the given file 
 */
void CHtmlSys_mainwin::write_game_chest_file(const textchar_t *fname)
{
    FILE *fp;
    size_t i;

    /* open the file */
    fp = fopen(fname, "w");

    /* if we failed to open the file, there's nothing more to do */
    if (fp == 0)
        return;

    /* write out the header */
    fprintf(fp, "# TADS Game Chest information\n\n"
            "# This is a generated file, so users are advised against\n"
            "# editing the file manually.\n\n");

    /* write out each group entry */
    for (i = 0 ; i < gch_info_->group_group_.used_ ; ++i)
    {
        CHtmlGameChestEntry *entry;

        /* get this entry */
        entry = gch_info_->group_group_.arr_[i];

        /* write out its information */
        fprintf(fp, "[group]\n");
        entry->write_value_to_file(fp, "name", FALSE);
        entry->write_value_to_file(fp, "id", FALSE);

        /* add a blank line at the end of this definition */
        fprintf(fp, "\n");
    }

    /* write out each game entry */
    for (i = 0 ; i < gch_info_->game_group_.used_ ; ++i)
    {
        CHtmlGameChestEntry *entry;

        /* get this entry */
        entry = gch_info_->game_group_.arr_[i];

        /* write out its information */
        fprintf(fp, "[game]\n");
        entry->write_value_to_file(fp, "file", FALSE);
        entry->write_value_to_file(fp, "name", FALSE);
        entry->write_value_to_file(fp, "group-id", FALSE);
        if (entry->get_value("htmldesc") != 0)
            entry->write_value_to_file(fp, "htmldesc", TRUE);
        else
            entry->write_value_to_file(fp, "desc", TRUE);
        if (entry->get_value("htmlbyline") != 0)
            entry->write_value_to_file(fp, "htmlbyline", TRUE);
        else
            entry->write_value_to_file(fp, "byline", TRUE);

        /* add a blank line at the end of this definition */
        fprintf(fp, "\n");
    }

    /* write out each web entry */
    for (i = 0 ; i < gch_info_->web_group_.used_ ; ++i)
    {
        CHtmlGameChestEntry *entry;

        /* get this entry */
        entry = gch_info_->web_group_.arr_[i];

        /* write out its information */
        fprintf(fp, "[web]\n");
        entry->write_value_to_file(fp, "url", FALSE);
        entry->write_value_to_file(fp, "name", FALSE);
        if (entry->get_value("htmldesc") != 0)
            entry->write_value_to_file(fp, "htmldesc", TRUE);
        else
            entry->write_value_to_file(fp, "desc", TRUE);

        /* add a blank line at the end of this definition */
        fprintf(fp, "\n");
    }

    /* done - close the file */
    fclose(fp);

    /* note the new timestamp on the file */
    get_game_chest_timestamp(&gch_timestamp_);

    /* no longer need to save */
    gch_info_->need_to_save_ = FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Read the game chest file, but only if it's not currently in memory, or
 *   our in-memory copy is out of date with the copy on disk.  This should be
 *   done any time we need to read or modify the data, to ensure that we have
 *   the latest before we proceed.  
 */
void CHtmlSys_mainwin::read_game_chest_file_if_needed()
{
    /* 
     *   if we're not currently showing the game chest page, check to see if
     *   we need to reload - do this before making any changes, so that we
     *   make our changes to the latest copy of the data on disk in order to
     *   avoid overwriting the disk file with out-of-data data from our copy
     *   in memory 
     */
    if (!main_panel_->is_in_game_chest())
    {
        FILETIME ft;
        
        /* check the file's timestamp */
        get_game_chest_timestamp(&ft);
        
        /* 
         *   if it doesn't match the one in memory, or there is no game
         *   chest file in memory, reload the game chest file 
         */
        if (gch_info_ == 0 || memcmp(&ft, &gch_timestamp_, sizeof(ft)) != 0)
            read_game_chest_file();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Read the game chest file 
 */
void CHtmlSys_mainwin::read_game_chest_file()
{
    FILE *fp;
    const textchar_t *fname;
    size_t i;

    /* if we previously had a game chest information structure, delete it */
    delete_game_chest_info();

    /* allocate a new game chest object */
    gch_info_ = new CHtmlGameChest();

    /* get the game chest filename */
    fname = get_game_chest_filename();

    /* open the file */
    fp = fopen(fname, "r");

    /* if we failed to open the file, there's nothing more to do */
    if (fp == 0)
    {
        /* 
         *   clear out the timestamp, so that if a valid file is created
         *   behind our backs at some point, we'll recognize it 
         */
        memset(&gch_timestamp_, 0, sizeof(gch_timestamp_));

        /* done */
        return;
    }

    /* 
     *   note the background image we're showing, so we can detect changes
     *   that require redrawing 
     */
    note_gc_bkg();

    /* read the file */
    for (;;)
    {
        char buf[64];
        char *p;
        char *start;
        size_t len;
        CHtmlGameChestEntry *entry;
        CHtmlGameChestValue *val;

        /* read the next line */
        if (fgets(buf, sizeof(buf), fp) == 0)
            break;

    parse_section_line:
        /* remove trailing newlines */
        for (len = strlen(buf) ;
             len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r') ;
             buf[--len] = '\0') ;

        /* skip leading spaces */
        for (p = buf ; isspace(*p) ; ++p) ;

        /* if it's blank or a comment, skip it */
        if (*p == '#' || *p == '\0')
            continue;

        /* we need a section header - if we don't have one, skip it */
        if (*p != '[')
            continue;

        /* find the end of the section name */
        for (start = ++p ; *p != '\0' && *p != ']' ; ++p) ;

        /* if it's not well-formed, skip it */
        if (*p != ']')
            continue;

        /* see what type of section we have */
        len = p - start;
        if (len == 3 && memicmp(start, "web", 3) == 0)
        {
            /* create a new web group entry */
            entry = gch_info_->web_group_.add_entry();
        }
        else if (len == 4 && memicmp(start, "game", 4) == 0)
        {
            /* create a new game group entry */
            entry = gch_info_->game_group_.add_entry();
        }
        else if (len == 5 && memicmp(start, "group", 5) == 0)
        {
            /* create a new group group entry */
            entry = gch_info_->group_group_.add_entry();
        }
        else
        {
            /* not a valid section name - skip it */
            continue;
        }

        /* read name/value pairs and add them to the entry */
        for (val = 0 ;; )
        {
            int leading_sp;
            int nl_cnt;
            
            /* get the next line */
            if (fgets(buf, sizeof(buf), fp) == 0)
                break;

            /* remove trailing newlines */
            for (len = strlen(buf), nl_cnt = 0 ;
                 len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r') ;
                 buf[--len] = '\0', ++nl_cnt) ;

            /* if we have a leading space, so note */
            leading_sp = isspace(buf[0]);

            /* skip leading spaces */
            for (p = buf ; isspace(*p) ; ++p) ;

            /* if it's blank or a comment, skip it */
            if (*p == '#' || *p == '\0')
                continue;

            /* if it's a new group, we're done with this entry */
            if (*p == '[')
                goto parse_section_line;

            /* 
             *   if we have a leading space, and we have a previous value,
             *   the current line is a continuation line - append it to the
             *   value under construction 
             */
            if (leading_sp && val != 0)
            {
                /* 
                 *   it's a continuation line - conver the whitespace
                 *   character immediately preceding the first non-space
                 *   character to an ordinary space, so that we treat the
                 *   run of whitespace from the end of the previous line to
                 *   the start of the non-space text on this line as a
                 *   single ordinary space 
                 */
                --p;
                *p = ' ';

                /* append the value */
                val->append_val(p);
            }
            else
            {
                /* find the colon */
                start = p;
                for ( ; *p != '\0' && *p != ':' && !isspace(*p) ; ++p) ;
                
                /* note the length of the keyword */
                len = p - start;
                
                /* if we stopped at whitespace, skip the spaces */
                for ( ; isspace(*p) ; ++p) ;
                
                /* if we didn't find a colon, ignore this line */
                if (*p != ':')
                    continue;
                
                /* skip whitspace following the colon */
                for (++p ; isspace(*p) ; ++p) ;
                
                /* null-terminate the name */
                *(start + len) = '\0';
                
                /* 
                 *   add the new name/value pair to this entry; remember
                 *   this value so that if the next line is a continuation
                 *   line, we can append the continuation line to this value 
                 */
                val = entry->add_value(start, p);
            }

            /* 
             *   If the line didn't end in a newline, we might have simply
             *   run out of space in our buffer - in this case, read more of
             *   the same line and append it to the value so far.  Keep
             *   going like this until we find a line that does end in a
             *   newline.  
             */
            while (nl_cnt == 0)
            {
                /* read more */
                if (fgets(buf, sizeof(buf), fp) == 0)
                    break;

                /* remove trailing newlines */
                for (len = strlen(buf), nl_cnt = 0 ;
                     len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r') ;
                     buf[--len] = '\0', ++nl_cnt) ;

                /* append it to the value so far */
                val->append_val(buf);
            }
        }
    }

    /* 
     *   run through each game; for each that has no name, use the root
     *   filename as the name 
     */
    for (i = 0 ; i < gch_info_->game_group_.used_ ; ++i)
    {
        CHtmlGameChestEntry *entry;

        /* get the entry */
        entry = gch_info_->game_group_.arr_[i];

        /* if it doesn't have a name entry, synthesize one */
        if (entry->get_value("name") == 0)
        {
            const textchar_t *fname;
            
            /* get the filename */
            fname = entry->get_value("file");

            /* if we found one, use the root filename as the name */
            if (fname != 0)
                entry->add_value("name", os_get_root_name((char *)fname));
        }

        /* if it doesn't have a group ID, use the default group zero */
        if (entry->get_value("group-id") == 0)
            entry->add_value("group-id", "0");
    }

    /* make sure all of the group entries have valid group ID's */
    gch_info_->group_group_.validate_id_values("id");

    /* sort the game and web site lists by name */
    gch_info_->game_group_.sort();
    gch_info_->web_group_.sort();

    /* done - close the file */
    fclose(fp);

    /* 
     *   note the timestamp on the file, so that later on we'll be able to
     *   tell if it has been changed by an external program since we loaded
     *   the file
     */
    get_game_chest_timestamp(&gch_timestamp_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Delete the game chest information 
 */
void CHtmlSys_mainwin::delete_game_chest_info()
{
    /* if we have game chest information, delete it */
    if (gch_info_ != 0)
    {
        /* if it's dirty, save it */
        save_game_chest_file();

        /* delete it */
        delete gch_info_;

        /* forget it */
        gch_info_ = 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Mix-in class for game chest dialogs incorporating group popups 
 */
class CDlgWithGroupPopup
{
protected:
    /* 
     *   Initialize the group popup for the dialog, if we have one.  The
     *   popup must have the identifier IDC_POP_GROUP. 
     */
    void init_group_popup(HWND handle, CHtmlGameChest *gch_info,
                          int init_group_id)
    {
        HWND pop;
        size_t i;

        /* get the popup handle - if it's not there, there's nothing to do */
        pop = GetDlgItem(handle, IDC_POP_GROUP);
        if (pop == 0)
            return;

        /* fill in the list with the group names */
        for (i = 0 ; i < gch_info->group_group_.used_ ; ++i)
        {
            CHtmlGameChestEntry *entry;
            int idx;

            /* get this entry */
            entry = gch_info->group_group_.arr_[i];

            /* add this item */
            idx = SendMessage(pop, CB_ADDSTRING, 0,
                              (LPARAM)entry->get_value("name"));

            /* if this is the initial group, select it */
            if (init_group_id != 0
                && atoi(entry->get_value("id")) == init_group_id)
            {
                /* this is the current group - select this combo item */
                SendMessage(pop, CB_SETCURSEL, idx, 0);
            }
        }
    }

    /* 
     *   Retrieve the group ID of the current selection in the combo box, if
     *   we have a group combo box.  If the current combo box text is empty,
     *   we'll return zero to indicate that the default unnamed group should
     *   be used; if there's a non-empty name in the combo box but it isn't
     *   one of the existing groups, we'll create a new group with the given
     *   name and return the new group's ID.  
     */
    int get_group_id_from_popup(HWND handle, CHtmlGameChest *gch_info)
    {
        HWND pop;
        int idx;

        /* get the popup handle - if it's not there, there's nothing to do */
        pop = GetDlgItem(handle, IDC_POP_GROUP);
        if (pop == 0)
            return 0;

        /* get the current popup selection */
        idx = SendMessage(pop, CB_GETCURSEL, 0, 0);
        if (idx == CB_ERR)
        {
            char buf[1024];
            char *p;
            CHtmlGameChestEntry *entry;

            /* 
             *   It's not one of the valid groups, so we must either have no
             *   group (which is indicated by blank combo text), or a new
             *   group (whose name is given by the combo text).  Get the
             *   text of the combo.  
             */
            GetWindowText(pop, buf, sizeof(buf));

            /* skip leading spaces */
            for (p = buf ; isspace(*p) ; ++p) ;

            /* 
             *   if it's just blanks, we have no group at all, which means
             *   that we use the special default group with ID zero 
             */
            if (*p == '\0')
            {
                /* use the special default group, and we're done */
                return 0;
            }

            /* create a new group entry */
            entry = gch_info->group_group_.add_entry();

            /* set the item's name */
            entry->add_value("name", p);

            /* 
             *   validate group "id" values, which will assign the new entry
             *   a unique group ID 
             */
            gch_info->group_group_.validate_id_values("id");

            /* the group ID for our item is the new group's ID */
            return atoi(entry->get_value("id"));
        }
        else
        {
            /* 
             *   Get the group ID from the group array item.  The group
             *   array indices correspond exactly to the combo box indices,
             *   since we just load the combo box in array order. 
             */
            return atoi(gch_info->group_group_.arr_[idx]->get_value("id"));
        }
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Mix-in class for game chest dialogs incorporating profile popups
 */
class CDlgWithProfilePopup
{
protected:
    /* 
     *   Initialize the profile popup for the dialog, if we have one.  The
     *   popup must have the identifier IDC_CBO_PROFILE.  
     */
    void init_profile_popup(HWND handle, CHtmlGameChest *gch_info,
                            const char *cur_profile)
    {
        HWND cbo;
        char base_key[256];
        DWORD disp;
        int key_idx;
        HKEY key;
        int pop_idx;

        /* get the popup handle - if it's not there, there's nothing to do */
        cbo = GetDlgItem(handle, IDC_CBO_PROFILE);
        if (cbo == 0)
            return;

        /* fill in the list with the available profile names */
        sprintf(base_key, "%s\\Profiles", w32_pref_key_name);
        key = CTadsRegistry::open_key(HKEY_CURRENT_USER, base_key,
                                      &disp, TRUE);
        for (key_idx = 0 ; ; ++key_idx)
        {
            DWORD len;
            char subkey[128];
            FILETIME ft;

            /* get the profile at this index from the registry */
            len = sizeof(subkey);
            if (RegEnumKeyEx(key, key_idx, subkey, &len, 0, 0, 0, &ft)
                != ERROR_SUCCESS)
                break;

            /* add this profile name to the combo */
            pop_idx = SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)subkey);

            /* if this is the current profile, select it */
            if (stricmp(subkey, cur_profile) == 0)
                SendMessage(cbo, CB_SETCURSEL, pop_idx, 0);
        }

        /* we're done with the key */
        CTadsRegistry::close_key(key);

        /* add the "Default" theme */
        pop_idx = SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)"Default");

        /* select it if "Default" is the current profile */
        if (stricmp(cur_profile, "Default") == 0)
            SendMessage(cbo, CB_SETCURSEL, pop_idx, 0);

        /* set the initial profile name */
        SendMessage(cbo, WM_SETTEXT, 0, (LPARAM)cur_profile);
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Game Chest web site/game file editor dialog 
 */
class CHtmlGameChestEditDlg: public CTadsDialog,
    protected CDlgWithGroupPopup,
    protected CDlgWithProfilePopup
{
public:
    CHtmlGameChestEditDlg(CHtmlGameChest *gch_info, const char *title,
                          const char *key, const char *name,
                          const char *desc, int desc_is_html,
                          const char *byline, int byline_is_html,
                          int group_id, const char *profile)
    {
        /* remember our game chest information object */
        gch_info_ = gch_info;

        /* remember our title string */
        title_ = title;

        /* set the initial group ID */
        group_id_ = group_id;

        /* save our initial parameter settings */
        strcpy(key_, key != 0 ? key : "");
        strcpy(name_, name != 0 ? name : "");
        strcpy(desc_, desc != 0 ? desc : "");
        strcpy(byline_, byline != 0 ? byline : "");
        strcpy(profile_, profile != 0 ? profile : "");
        desc_is_html_ = desc_is_html;
        byline_is_html_ = byline_is_html;
    }

    virtual void init()
    {
        /* inherit default initialization */
        CTadsDialog::init();

        /* set our window title */
        SetWindowText(handle_, title_);

        /* initialize our fields */
        SetDlgItemText(handle_, IDC_FLD_KEY, key_);
        SetDlgItemText(handle_, IDC_FLD_NAME, name_);
        SetDlgItemText(handle_, IDC_FLD_DESC, desc_);
        SetDlgItemText(handle_, IDC_FLD_BYLINE, byline_);

        /* initialize our description html/plain-text radio buttons */
        CheckDlgButton(handle_, IDC_CB_DESC_HTML,
                       desc_is_html_ ? BST_CHECKED : BST_UNCHECKED);

        /* initialize our byline html/plain-text radio buttons */
        CheckDlgButton(handle_, IDC_CB_BYLINE_HTML,
                       byline_is_html_ ? BST_CHECKED : BST_UNCHECKED);

        /* 
         *   send ourselves a text-change notification for the URL, so we
         *   initially enable or disable the OK button appropriately 
         */
        PostMessage(handle_, WM_COMMAND,
                    (WPARAM)MAKELONG(IDC_FLD_KEY, EN_CHANGE),
                    (LPARAM)GetDlgItem(handle_, IDC_FLD_KEY));

        /* initialize the group popup, if present */
        init_group_popup(handle_, gch_info_, group_id_);

        /* initialize the profile popup, if present */
        init_profile_popup(handle_, gch_info_, profile_);
    }

    virtual int init_focus(int def_id)
    {
        /* 
         *   if we have a key field, set focus initially on the name rather
         *   than on the key, because the key was presumably filled in from
         *   some browse or drag-drop information and thus shouldn't need
         *   additional editing in most cases 
         */
        if (key_[0] != '\0')
        {
            /* move focus to the name field */
            SetFocus(GetDlgItem(handle_, IDC_FLD_NAME));

            /* select the entire current contents */
            SendMessage(GetDlgItem(handle_, IDC_FLD_NAME),
                        EM_SETSEL, 0, 32767);

            /* indicate that we want non-default focus */
            return FALSE;
        }

        /* there's no key value - accept the default focus on the key */
        return TRUE;
    }

    virtual int do_command(WPARAM id, HWND ctl)
    {
        switch(LOWORD(id))
        {
        case IDOK:
            /* copy the field values back into our internal buffers */
            GetDlgItemText(handle_, IDC_FLD_KEY, key_, sizeof(key_));
            GetDlgItemText(handle_, IDC_FLD_NAME, name_, sizeof(name_));
            GetDlgItemText(handle_, IDC_FLD_DESC, desc_, sizeof(desc_));
            GetDlgItemText(handle_, IDC_FLD_BYLINE, byline_, sizeof(byline_));
            GetDlgItemText(handle_, IDC_CBO_PROFILE,
                           profile_, sizeof(profile_));

            /* update the group ID from the popup contents, if appropriate */
            group_id_ = get_group_id_from_popup(handle_, gch_info_);

            /* 
             *   remove leading and trailing spaces, and remove newlines
             *   from the description and byline fields
             */
            trim_field(desc_);
            trim_field(byline_);

            /* get the current description html/plain radio button seting */
            desc_is_html_ = (IsDlgButtonChecked(handle_, IDC_CB_DESC_HTML)
                             == BST_CHECKED);

            /* get the current byline html/plain radio button seting */
            byline_is_html_ = (IsDlgButtonChecked(handle_, IDC_CB_BYLINE_HTML)
                               == BST_CHECKED);

            /* inherit default handling as well, to dismiss the dialog */
            return CTadsDialog::do_command(id, ctl);

        case IDC_BTN_BROWSE:
            {
                OPENFILENAME info;
                char fname[256];
                char dir[256];

                /* set up the dialog definition structure */
                info.lStructSize = sizeof(info);
                info.hwndOwner = handle_;
                info.hInstance = CTadsApp::get_app()->get_instance();
                info.lpstrFilter = "TADS Games\0*.gam;*.t3\0All Files\0*.*\0";
                info.lpstrCustomFilter = 0;
                info.nFilterIndex = 0;
                info.lpstrFile = fname;
                info.nMaxFile = sizeof(fname);
                info.lpstrFileTitle = 0;
                info.nMaxFileTitle = 0;
                info.lpstrTitle = "Select a file";
                info.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST
                             | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
                info.nFileOffset = 0;
                info.nFileExtension = 0;
                info.lpstrDefExt = 0;
                info.lCustData = 0;
                info.lpfnHook = 0;
                info.lpTemplateName = 0;
                CTadsDialog::set_filedlg_center_hook(&info);

                /* get the current key value as the initial filename */
                GetDlgItemText(handle_, IDC_FLD_KEY, fname, sizeof(fname));
                if (fname[0] != '\0')
                {
                    char *p;

                    /* start off with the current filename */
                    strcpy(dir, fname);

                    /* get the root of the name */
                    p = os_get_root_name(dir);

                    /* copy the filename part */
                    strcpy(fname, p);

                    /* remove the file portion */
                    if (p > dir && *(p-1) == '\\')
                        *(p-1) = '\0';

                    /* set the initial directory */
                    info.lpstrInitialDir = dir;
                }
                else
                {
                    /* there's no filename yet */
                    fname[0] = '\0';
                    
                    /* start off in the default open-file directory */
                    info.lpstrInitialDir =
                        CTadsApp::get_app()->get_openfile_dir();
                }

                /* ask for the file */
                if (GetOpenFileName(&info))
                {
                    /* got a file - store it in the key field */
                    SetDlgItemText(handle_, IDC_FLD_KEY, fname);
                }
            }

            /* handled */
            return TRUE;
            
        case IDC_FLD_KEY:
            /* see what kind of notification we're receiving */
            switch(HIWORD(id))
            {
            case EN_CHANGE:
                /* 
                 *   the URL is changing - if the field isn't empty, enable
                 *   the OK button; otherwise disable it 
                 */
                {
                    const char *p;
                        
                    /* get the latest text */
                    GetDlgItemText(handle_, IDC_FLD_KEY, key_, sizeof(key_));

                    /* search for a non-space */
                    for (p = key_ ; isspace(*p) ; ++p) ;

                    /* enable the "OK" button iff the URL is not all spaces */
                    EnableWindow(GetDlgItem(handle_, IDOK), *p != '\0');
                }
                
                /* handled */
                return TRUE;
            }
            break;
        }

        /* inherit default handling */
        return CTadsDialog::do_command(id, ctl);
    }

    /* get my key value */
    const char *get_key() const { return key_; }

    /* get my name value, or null if it's blank */
    const char *get_name() const { return get_val_or_null(name_); }

    /* get my description, or null if it's blank */
    const char *get_desc() const { return get_val_or_null(desc_); }

    /* get the byline, or null if it's blank */
    const char *get_byline() const { return get_val_or_null(byline_); }

    /* get the ID of the chosen group */
    int get_group_id() const { return group_id_; }

    /* get the profile name */
    const char *get_profile() const { return profile_; }

    /* is the description html (vs plain text)? */
    int desc_is_html() const { return desc_is_html_; }

    /* is the byline html (vs plain text)? */
    int byline_is_html() const { return byline_is_html_; }

protected:
    /* 
     *   Trim a long text field buffer: remove leading and trailing spaces,
     *   collapse each run of consecutive whitespace to a single space, and
     *   convert newlines to spaces.  Modifies the buffer in place.  
     */
    void trim_field(char *fld)
    {
        char *src;
        char *dst;
        
        /* skip leading whitspace */
        for (src = dst = fld ; isspace(*src) ; ++src) ;

        /* replace each newline with a space */
        for ( ; *src != '\0' ; ++src)
        {
            /* 
             *   If this is a newline, replace it with a space.  If it's a
             *   CR-LF sequence, replace the pair with a single space.  
             */
            if (*src == '\r' && *(src+1) == '\n')
            {
                /* skip the second character of the pair */
                ++src;
                
                /* add the single space to the output */
                *dst++ = ' ';
            }
            else if (*src == '\n')
            {
                /* add a space to the output */
                *dst++ = ' ';
            }
            else
            {
                /* copy this character to the output unchanged */
                *dst++ = *src;
            }
        }
        
        /* add a null terminator */
        *dst = '\0';
        
        /* remove trailing whitespace */
        for (src = fld + strlen(fld) ; src > fld && isspace(*(src-1)) ;
             *(src-1) = '\0', --src) ;
    }
    
    /* return a given string, or null if it's all whitespace */
    const char *get_val_or_null(const char *val) const
    {
        const char *p;

        /* check to see if we have any non-whitespace characters */
        for (p = val ; isspace(*p) ; ++p) ;

        /* if it's all blank, return null; otherwise return the string */
        return (*p == '\0' ? 0 : val);
    }

    /* our game chest information object */
    CHtmlGameChest *gch_info_;

    /* our title string */
    const char *title_;

    /* the ID of the chosen group */
    int group_id_;

    /* our value settings */
    char key_[1024];
    char name_[256];
    char desc_[4096];
    char byline_[2048];
    char profile_[128];
    int desc_is_html_;
    int byline_is_html_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Game Chest Group Manager dialog 
 */
class CHtmlGameChestGroupDlg: public CTadsDialog
{
public:
    CHtmlGameChestGroupDlg(CHtmlGameChest *gch_info)
    {
        /* remember the game chest information object */
        gch_info_ = gch_info;

        /* we haven't yet made any modifications */
        dirty_ = FALSE;

        /* we're not editing a label in the tree view yet */
        editing_label_ = FALSE;
    }

    /* initialize */
    virtual void init()
    {
        HWND tv;
        size_t i;
        HICON ico;
        
        /* inherit default initialization */
        CTadsDialog::init();

        /* initialize the "move up" button image */
        ico = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                               MAKEINTRESOURCE(IDI_MOVE_UP), IMAGE_ICON,
                               0, 0, LR_SHARED);
        SendMessage(GetDlgItem(handle_, IDC_BTN_MOVE_UP), BM_SETIMAGE,
                    IMAGE_ICON, (LPARAM)ico);

        /* initialize the "move down" button image */
        ico = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                               MAKEINTRESOURCE(IDI_MOVE_DOWN), IMAGE_ICON,
                               0, 0, LR_SHARED);
        SendMessage(GetDlgItem(handle_, IDC_BTN_MOVE_DOWN), BM_SETIMAGE,
                    IMAGE_ICON, (LPARAM)ico);

        /* populate the tree view with groups */
        tv = GetDlgItem(handle_, IDC_TREE_GROUPS);
        for (i = 0 ; i < gch_info_->group_group_.used_ ; ++i)
        {
            CHtmlGameChestEntry *entry;
            TVINSERTSTRUCT ins;
            
            /* get this entry */
            entry = gch_info_->group_group_.arr_[i];

            /* 
             *   add it to the tree - use the lParam to store the original
             *   group array index of the entry so that we can find the
             *   entry again later 
             */
            ins.hParent = TVI_ROOT;
            ins.hInsertAfter = TVI_LAST;
            ins.item.mask = TVIF_TEXT | TVIF_CHILDREN | TVIF_PARAM;
            ins.item.pszText = (char *)entry->get_value("name");
            ins.item.cChildren = 0;
            ins.item.lParam = (LPARAM)i;
            TreeView_InsertItem(tv, &ins);
        }

        /* initialize button states */
        update_button_states();
    }

    /* process a command */
    virtual int do_command(WPARAM id, HWND ctl)
    {
        HWND tv = GetDlgItem(handle_, IDC_TREE_GROUPS);
        HTREEITEM hitem;
        
        /* check what's going on */
        switch(LOWORD(id))
        {
        case IDOK:
        case IDCANCEL:
            /* 
             *   If the high-order word of the ID is zero, this was
             *   generated in response to a return or escape key - if we're
             *   editing in the tree control, end the editing; if it's the
             *   escape key, cancel the editing, otherwise accept the
             *   changes.
             *   
             *   (If the high-order word isn't zero, ignore the message,
             *   since it's a notification to the label's edit field - the
             *   tree seems to use IDOK as the control ID of the field.  So,
             *   when we have an edit field open and we get a message from
             *   control id == IDOK, ignore it.)  
             */
            if (editing_label_ && HIWORD(id) == 0)
                TreeView_EndEditLabelNow(tv, LOWORD(id) == IDCANCEL);

            /* handled */
            return TRUE;
            
        case IDC_BTN_NEW:
            {
                CHtmlGameChestEntry *entry;
                TVINSERTSTRUCT ins;

                /* create a new group in our internal list */
                entry = gch_info_->group_group_.add_entry();

                /* give it a default starting name */
                entry->add_value("name", "New Group");

                /* assign it an ID number */
                gch_info_->group_group_.validate_id_values("id");

                /* set up to add the item at the end of our list */
                ins.hParent = TVI_ROOT;
                ins.hInsertAfter = TVI_LAST;
                ins.item.mask = TVIF_TEXT | TVIF_CHILDREN | TVIF_PARAM;
                ins.item.pszText = (char *)entry->get_value("name");
                ins.item.cChildren = 0;

                /* 
                 *   the item's lparam is its internal group array index -
                 *   since this is the last item, it has the highest valid
                 *   index 
                 */
                ins.item.lParam = gch_info_->group_group_.used_ - 1;

                /* insert the item */
                hitem = TreeView_InsertItem(tv, &ins);

                /* select the new item */
                TreeView_Select(tv, hitem, TVGN_CARET);

                /* immediately edit it in place */
                TreeView_EditLabel(tv, hitem);

                /* we've modified our internal game chest database */
                dirty_ = TRUE;
            }
            return TRUE;

        case IDC_BTN_DELETE:
            /* confirm the deletion */
            if (MessageBox(handle_,
                           "Do you really want to delete this group?",
                           "TADS", MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
                size_t idx;
                TVITEM item;
                int group_id;
                size_t i;
                int keep_games;
                size_t game_cnt;

                /* get the selected item from the group list */
                hitem = TreeView_GetSelection(tv);

                /* 
                 *   get the item's lParam - this gives its index in our
                 *   internal group list 
                 */
                item.mask = TVIF_HANDLE | TVIF_PARAM;
                item.hItem = hitem;
                TreeView_GetItem(tv, &item);
                idx = (size_t)item.lParam;

                /* note the ID of the group being deleted */
                group_id = atoi(gch_info_->group_group_.arr_[idx]
                                ->get_value("id"));

                /* count the games that are part of this group */
                for (i = 0, game_cnt = 0 ;
                     i < gch_info_->game_group_.used_ ; ++i)
                {
                    CHtmlGameChestEntry *entry;

                    /* count the entry if it's part of this group */
                    entry = gch_info_->game_group_.arr_[i];
                    if (atoi(entry->get_value("group-id")) == group_id)
                        ++game_cnt;
                }
                
                /* 
                 *   ask what we want to do with the games in the group, if
                 *   there are indeed any games in the group 
                 */
                keep_games = TRUE;
                if (game_cnt != 0)
                {
                    int response;

                    /* ask what they want us to do */
                    response = MessageBox(
                        handle_,
                        "Do you also want to delete the story links in this "
                        "group?  If you answer No, the links will be "
                        "moved to the ungrouped section at the top of the "
                        "Favorite Stories list.", "TADS",
                        MB_YESNOCANCEL | MB_ICONQUESTION);
                    
                    /* if they said 'cancel', abort the whole thing */
                    if (response == IDCANCEL)
                        return TRUE;
                    
                    /* keep games if they said 'no' to deleting them */
                    keep_games = (response == IDNO);
                }

                /* delete the entry from our internal database */
                gch_info_->group_group_.del_entry(idx);

                /* delete the item from the tree view */
                TreeView_DeleteItem(tv, hitem);

                /* renumber the tree to adjust for the change */
                renumber_tree();

                /* 
                 *   run through the game list; for each game, either delete
                 *   the game or move it to the default group, according to
                 *   the user's wishes 
                 */
                for (i = 0 ; i < gch_info_->game_group_.used_ ; ++i)
                {
                    CHtmlGameChestEntry *entry;

                    /* get the game entry */
                    entry = gch_info_->game_group_.arr_[i];

                    /* 
                     *   if the entry's group ID matches that of the group
                     *   being deleted, move the game to the default unnamed
                     *   group 
                     */
                    if (atoi(entry->get_value("group-id")) == group_id)
                    {
                        /* check whether we're keeping games or not */
                        if (keep_games)
                        {
                            /* delete the old group ID of this entry */
                            entry->del_value("group-id");

                            /* add it back under the default group */
                            entry->add_value("group-id", "0");
                        }
                        else
                        {
                            /* they want to delete this entire entry */
                            gch_info_->game_group_.del_entry(i);

                            /* 
                             *   we've removed the entry at this index, so
                             *   consider the index once again
                             */
                            --i;
                        }
                    }
                }

                /* we've modified our internal game chest database */
                dirty_ = TRUE;
            }
            
            return TRUE;

        case IDC_BTN_RENAME:
            /* get the selected item from the group list */
            hitem = TreeView_GetSelection(tv);

            /* edit it in place */
            TreeView_EditLabel(tv, hitem);
            return TRUE;

        case IDC_BTN_MOVE_UP:
        case IDC_BTN_MOVE_DOWN:
            {
                TVITEM item;
                TVINSERTSTRUCT ins;
                HTREEITEM ins_after;
                size_t idx;
                size_t other_idx;
                CHtmlGameChestEntry *entry;
                CHtmlGameChestEntry *other_entry;
                
                /*
                 *   Moving an item up or down in the list.  We must swap
                 *   the item and its previous/next sibling in the order in
                 *   the tree view as well as in our internal array.  First,
                 *   get the selected item from the tree.  
                 */
                hitem = TreeView_GetSelection(tv);

                /* 
                 *   get the item's information - we want the lparam, which
                 *   will give us our internal array index 
                 */
                item.mask = TVIF_HANDLE | TVIF_PARAM;
                item.hItem = hitem;
                TreeView_GetItem(tv, &item);
                idx = (size_t)item.lParam;

                /* calculate the index of the entry we're swapping with */
                other_idx = idx + (LOWORD(id) == IDC_BTN_MOVE_UP ? -1 : 1);

                /* get the two entries */
                entry = gch_info_->group_group_.arr_[idx];
                other_entry = gch_info_->group_group_.arr_[other_idx];

                /* swap the two entries */
                gch_info_->group_group_.arr_[idx] = other_entry;
                gch_info_->group_group_.arr_[other_idx] = entry;

                /* 
                 *   get the re-insertion point in the tree view for our
                 *   item - if we're moving up, we want to insert after the
                 *   item two back; if we're moving down, we want to insert
                 *   after the next item 
                 */
                if (LOWORD(id) == IDC_BTN_MOVE_UP)
                {
                    /* 
                     *   moving up - insert after item back two, or as the
                     *   first item if there isn't anything two back 
                     */
                    ins_after = TreeView_GetPrevSibling(tv, hitem);
                    if (ins_after != 0)
                        ins_after = TreeView_GetPrevSibling(tv, ins_after);
                    if (ins_after == 0)
                        ins_after = TVI_FIRST;
                }
                else
                {
                    /* moving down - insert after the next item */
                    ins_after = TreeView_GetNextSibling(tv, hitem);
                }

                /* delete the item being moved */
                TreeView_DeleteItem(tv, hitem);

                /* insert a new item with the same name */
                ins.hParent = TVI_ROOT;
                ins.hInsertAfter = ins_after;
                ins.item.mask = TVIF_TEXT | TVIF_CHILDREN | TVIF_PARAM;
                ins.item.pszText = (char *)entry->get_value("name");
                ins.item.lParam = (LPARAM)other_idx;
                hitem = TreeView_InsertItem(tv, &ins);

                /* select the new item */
                TreeView_Select(tv, hitem, TVGN_CARET);

                /* fix up all of the lparams for the new order */
                renumber_tree();

                /* note that we've made a change */
                dirty_ = TRUE;
            }

            /* handled */
            return TRUE;

        case IDC_BTN_CLOSE:
            /* dismiss the dialog */
            EndDialog(handle_, id);
            return TRUE;
        }

        /* not handled here */
        return FALSE;
    }

    /* process a notification */
    int do_notify(NMHDR *nm, int id)
    {
        NMTREEVIEW *tvn;
        NMTVKEYDOWN *tvk;
        CHtmlGameChestEntry *entry;
        NMTVDISPINFO *evn;
        TVITEM item;
        
        /* check the control ID */
        switch(id)
        {
        case IDC_TREE_GROUPS:
            /* the notification info is actually for the tree view */
            tvn = (NMTREEVIEW *)nm;
            
            /* see what kind of notification we have */
            switch(nm->code)
            {
            case TVN_SELCHANGED:
                /* new selection - update button states accordingly */
                update_button_states();

                /* handled */
                return TRUE;

            case TVN_BEGINLABELEDIT:
                /* remember that we're editing a label */
                editing_label_ = TRUE;

                /* allow editing */
                return FALSE;

            case TVN_KEYDOWN:
                /* key press - check for special keys */
                tvk = (NMTVKEYDOWN *)nm;
                switch (tvk->wVKey)
                {
                case VK_F2:
                    /* F2 - edit the label of the selected item */
                    SendMessage(handle_, WM_COMMAND, IDC_BTN_RENAME, 0);

                    /* handled */
                    return TRUE;
                }

                /* we didn't handle it */
                break;

            case NM_DBLCLK:
                /* 
                 *   they double-clicked in the tree; treat this as label
                 *   edit 
                 */
                PostMessage(handle_, WM_COMMAND, IDC_BTN_RENAME, 0);

                /* handled */
                return TRUE;

            case TVN_ENDLABELEDIT:
                /* note that we're no longer editing a label */
                editing_label_ = FALSE;
                
                /* 
                 *   They've changed the name of a group - update our
                 *   internal name for the group.  Cast the notification
                 *   structure into the proper subclass form.  
                 */
                evn = (NMTVDISPINFO *)nm;

                /* 
                 *   if editing was cancelled, as indicated by the new text
                 *   being set to null, there's nothing for us to do - just
                 *   indicate that we've fully handled the notification 
                 */
                if (evn->item.pszText == 0)
                    return TRUE;

                /* 
                 *   the lParam is the group array index - retrieve the
                 *   group entry from the group list 
                 */
                entry = gch_info_->group_group_.arr_[(int)evn->item.lParam];

                /* update the text in the tree view */
                item.mask = TVIF_HANDLE | TVIF_TEXT;
                item.hItem = evn->item.hItem;
                item.pszText = evn->item.pszText;
                TreeView_SetItem(nm->hwndFrom, &item);

                /* delete the old name from the entry, and add the new one */
                entry->del_value("name");
                entry->add_value("name", evn->item.pszText);

                /* note that we've edited the game chest information */
                dirty_ = TRUE;

                /* handled - accept the change */
                return TRUE;
            }

            /* not handled */
            break;
        }

        /* not handled here */
        return FALSE;
    }

    /* have changes been made since the dialog was initialized? */
    int is_dirty() const { return dirty_; }

protected:
    /* update button states for the current selection status */
    void update_button_states()
    {
        HWND tv = GetDlgItem(handle_, IDC_TREE_GROUPS);
        HTREEITEM hitem;
        
        /* get the current selection in the tree view */
        hitem = TreeView_GetSelection(tv);

        /* 
         *   enable or disable the buttons that require a group to be
         *   selected, according to whether or not a group is selected 
         */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_DELETE), hitem != 0);
        EnableWindow(GetDlgItem(handle_, IDC_BTN_RENAME), hitem != 0);
        EnableWindow(GetDlgItem(handle_, IDC_BTN_MOVE_UP), hitem != 0);
        EnableWindow(GetDlgItem(handle_, IDC_BTN_MOVE_DOWN), hitem != 0);

        /* 
         *   additionally, if we have a valid item, check to see if it's the
         *   first and/or last item - if it is, disable the move-up and/or
         *   move-down button as appropriate
         */
        if (hitem != 0)
        {
            /* if this is the first item, disable the 'up' button */
            if (TreeView_GetPrevSibling(tv, hitem) == 0)
                EnableWindow(GetDlgItem(handle_, IDC_BTN_MOVE_UP), FALSE);

            /* if it's the last item, disable the 'down' button */
            if (TreeView_GetNextSibling(tv, hitem) == 0)
                EnableWindow(GetDlgItem(handle_, IDC_BTN_MOVE_DOWN), FALSE);  
        }
    }

    /* renumber the tree to adjust for insertion/deletion/reordering */
    void renumber_tree()
    {
        HWND tv = GetDlgItem(handle_, IDC_TREE_GROUPS);
        size_t idx;
        HTREEITEM cur;
        TVITEM item;
        
        /* 
         *   run through the entire tree - the items are in the same order
         *   as they appear in our internal list, so just given them numbers
         *   in the order in which they appear 
         */
        for (cur = TreeView_GetRoot(tv), idx = 0 ; cur != 0 ;
             cur = TreeView_GetNextSibling(tv, cur), ++idx)
        {
            /* set this item's new number to the current index */
            item.mask = TVIF_HANDLE | TVIF_PARAM;
            item.hItem = cur;
            item.lParam = (LPARAM)idx;
            TreeView_SetItem(tv, &item);
        }
    }

    /* flag: changes have been made */
    int dirty_;

    /* flag: we're editing a label in the tree control */
    int editing_label_;
    
    /* the game chest information object */
    CHtmlGameChest *gch_info_;
};

/* ------------------------------------------------------------------------ */
/*
 *   determine if a counted-length command string matches the given
 *   null-terminated constant string 
 */
static int gc_cmd_equals(const textchar_t *cmd, size_t cmdlen,
                         const char *str)
{
    size_t len;

    /* 
     *   get the length of the null-terminated constant string - if it
     *   doesn't match the length of the command string, we have no match 
     */
    if ((len = strlen(str)) != cmdlen)
        return FALSE;

    /* they're the same length, so check for a byte-by-byte match */
    return memcmp(cmd, str, len) == 0;
}

/*
 *   Determine if a counted-length command string matches the given prefix.
 *   If so, returns true and fills in the buffer with the suffix (the part
 *   after the matched prefix). 
 */
static int gc_cmd_prefix_eq(const textchar_t *cmd, size_t cmdlen,
                            const char *str,
                            char *suffix_buf, size_t suffix_buf_len)
{
    size_t len;
    size_t copy_len;

    /* 
     *   get the length of the prefix - if the command string isn't at least
     *   as long as the prefix, we have no match 
     */
    if (cmdlen < (len = strlen(str)))
        return FALSE;

    /* 
     *   it's at least as long as the prefix, so check for a match to the
     *   prefix text - if we don't have a byte-by-byte match, we don't have
     *   a prefix match 
     */
    if (memcmp(cmd, str, len) != 0)
        return FALSE;

    /* we have a prefix match - copy the suffix to the output buffer */
    copy_len = cmdlen - len;
    if (copy_len > suffix_buf_len - 1)
        copy_len = suffix_buf_len - 1;
    memcpy(suffix_buf, cmd + len, copy_len);

    /* null-terminate the result */
    suffix_buf[copy_len] = '\0';

    /* tell the caller we have a match */
    return TRUE;
}

/*
 *   Process a Game Chest command 
 */
void CHtmlSys_mainwin::process_game_chest_cmd(
    const textchar_t *cmd, size_t cmdlen)
{
    char suffix[OSFNMAX];
    
    /* determine which command we have, and process it accordingly */
    if (gc_cmd_equals(cmd, cmdlen, "browse-play"))
    {
        /* ask for a file and load it */
        load_new_game();
    }
    else if (gc_cmd_prefix_eq(cmd, cmdlen, "play-recent/",
                              suffix, sizeof(suffix)))
    {
        int idx;
        
        /* convert the suffix to the recent game index number */
        idx = suffix[0] - '0';

        /* load this recent game */
        load_recent_game(idx);
    }
    else if (gc_cmd_prefix_eq(cmd, cmdlen, "add-recent/",
                              suffix, sizeof(suffix)))
    {
        const textchar_t *order;

        /* get the recent game ordering array */
        order = prefs_->get_recent_game_order();
        
        /* add this file to the favorites */
        if (game_chest_add_fav(prefs_->get_recent_game(order[atol(suffix)]),
                               0, TRUE))
        {
            /* they added it - refresh the list */
            show_game_chest(FALSE, FALSE, TRUE);
        }
    }
    else if (gc_cmd_equals(cmd, cmdlen, "add-url"))
    {
        CHtmlGameChestEditDlg *dlg;

        /* run the web site dialog in "add" mode */
        dlg = new CHtmlGameChestEditDlg(gch_info_, "Add Web Site Link",
                                        "", "", "", FALSE, "", FALSE, 0, 0);
        if (dlg->run_modal(DLG_GAME_CHEST_URL, handle_,
                           CTadsApp::get_app()->get_instance()) == IDOK)
        {
            CHtmlGameChestEntry *entry;

            /* add a new web entry */
            entry = gch_info_->web_group_.add_entry();

            /* set the new entry's values from the dialog */
            entry->add_value("url", dlg->get_key());
            if (dlg->get_name() != 0)
                entry->add_value("name", dlg->get_name());
            if (dlg->get_desc() != 0)
                entry->add_value(dlg->desc_is_html() ? "htmldesc" : "desc",
                                 dlg->get_desc());

            /* re-sort the web site list */
            gch_info_->web_group_.sort();

            /* redisplay the list */
            show_game_chest(FALSE, FALSE, TRUE);
        }

        /* done with the dialog */
        delete dlg;
    }
    else if (gc_cmd_equals(cmd, cmdlen, "browse-add"))
    {
        OPENFILENAME info;
        char fname[256];

        /* set up the dialog definition structure */
        info.lStructSize = sizeof(info);
        info.hwndOwner = handle_;
        info.hInstance = CTadsApp::get_app()->get_instance();
        info.lpstrFilter = "TADS Stories\0*.gam;*.t3\0All Files\0*.*\0";
        info.lpstrCustomFilter = 0;
        info.nFilterIndex = 0;
        info.lpstrFile = fname;
        info.nMaxFile = sizeof(fname);
        info.lpstrFileTitle = 0;
        info.nMaxFileTitle = 0;
        info.lpstrTitle = "Select a story file";
        info.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST
                     | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        info.nFileOffset = 0;
        info.nFileExtension = 0;
        info.lpstrDefExt = 0;
        info.lCustData = 0;
        info.lpfnHook = 0;
        info.lpTemplateName = 0;
        CTadsDialog::set_filedlg_center_hook(&info);

        /* start off in the default open-file directory */
        info.lpstrInitialDir =
            CTadsApp::get_app()->get_openfile_dir();

        /* no initial filename */
        fname[0] = '\0';

        /* ask for a file */
        if (GetOpenFileName(&info))
        {
            /* got a file - add it to the favorites */
            if (game_chest_add_fav(fname, 0, TRUE))
            {
                /* they added it - redisplay the list */
                show_game_chest(FALSE, FALSE, TRUE);
            }
        }
    }
    else if (gc_cmd_equals(cmd, cmdlen, "search-add"))
    {
        /* 
         *   run the search-and-add dialog; use the c: root as the default
         *   starting point 
         */
        if (game_chest_add_fav_dir("C:\\"))
            show_game_chest(FALSE, FALSE, TRUE);
    }
    else if (gc_cmd_equals(cmd, cmdlen, "edit-groups"))
    {
        CHtmlGameChestGroupDlg *dlg;

        /* run the group manager dialog */
        dlg = new CHtmlGameChestGroupDlg(gch_info_);
        dlg->run_modal(DLG_GAME_CHEST_GROUPS, handle_,
                       CTadsApp::get_app()->get_instance());

        /* if they made changes, update the display */
        if (dlg->is_dirty())
            show_game_chest(FALSE, FALSE, TRUE);

        /* done with the dialog - it updates the gch_info_ for us */
        delete dlg;
    }
    else if (gc_cmd_prefix_eq(cmd, cmdlen, "play-game/",
                              suffix, sizeof(suffix)))
    {
        CHtmlGameChestEntry *entry;

        /* get the entry for our index, as given by the suffix */
        entry = gch_info_->game_group_.arr_[atoi(suffix)];

        /* load and run this game */
        load_new_game(entry->get_value("file"));
    }
    else if (gc_cmd_prefix_eq(cmd, cmdlen, "edit-game/",
                              suffix, sizeof(suffix)))
    {
        CHtmlGameChestEntry *entry;
        CHtmlGameChestEditDlg *dlg;
        const char *desc;
        const char *byline;
        int desc_is_html;
        int byline_is_html;
        char profile[128];
        const char *fname;

        /* get the entry for our index, as given by the suffix */
        entry = gch_info_->game_group_.arr_[atoi(suffix)];

        /* 
         *   get the description - look for "htmldesc" first, and the
         *   regular "desc" only if that fails 
         */
        desc = entry->get_value("htmldesc");
        desc_is_html = (desc != 0);
        if (desc == 0)
            desc = entry->get_value("desc");

        /* get the byline - try html first, then plain text */
        byline = entry->get_value("htmlbyline");
        byline_is_html = (byline != 0);
        if (byline == 0)
            byline = entry->get_value("byline");

        /* get the filename */
        fname = entry->get_value("file");

        /* get the current profile for the game */
        get_game_specific_profile(profile, sizeof(profile), fname, FALSE);

        /* run the game file dialog in "edit" mode */
        dlg = new CHtmlGameChestEditDlg(gch_info_, "Edit Story Link",
                                        fname,
                                        entry->get_value("name"),
                                        desc, desc_is_html,
                                        byline, byline_is_html,
                                        atoi(entry->get_value("group-id")),
                                        profile);
        if (dlg->run_modal(DLG_GAME_CHEST_GAME, handle_,
                           CTadsApp::get_app()->get_instance()) == IDOK)
        {
            /* delete the old values */
            entry->del_value("file");
            entry->del_value("name");
            entry->del_value("desc");
            entry->del_value("htmldesc");
            entry->del_value("byline");
            entry->del_value("htmlbyline");
            entry->del_value("group-id");

            /* set the new entry's values from the dialog */
            entry->add_value("file", dlg->get_key());
            entry->add_value("group-id", dlg->get_group_id());
            if (dlg->get_name() != 0)
                entry->add_value("name", dlg->get_name());
            if (dlg->get_desc() != 0)
                entry->add_value(dlg->desc_is_html() ? "htmldesc" : "desc",
                                 dlg->get_desc());
            if (dlg->get_byline() != 0)
                entry->add_value(dlg->byline_is_html()
                                 ? "htmlbyline" : "byline",
                                 dlg->get_byline());

            /* set the game's profile association */
            set_profile_assoc(dlg->get_key(), dlg->get_profile());

            /* 
             *   re-sort the game list, in case the entry's sort criteria
             *   changed 
             */
            gch_info_->game_group_.sort();

            /* redisplay the list */
            show_game_chest(FALSE, FALSE, TRUE);
        }

        /* done with the dialog */
        delete dlg;
    }
    else if (gc_cmd_prefix_eq(cmd, cmdlen, "remove-game/",
                              suffix, sizeof(suffix)))
    {
        /* confirm the removal */
        if (MessageBox(0, "Are you sure you want to remove this story link "
                       "from your Game Chest?\r\n\r\n"
                       "(The story file itself will NOT be deleted, so you "
                       "can always add the link again later if you want.)",
                       "TADS", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL)
            == IDYES)
        {
            /* delete the entry given by the index in the suffix */
            gch_info_->game_group_.del_entry(atoi(suffix));

            /* redisplay the list */
            show_game_chest(FALSE, FALSE, TRUE);
        }
    }
    else if (gc_cmd_prefix_eq(cmd, cmdlen, "edit-url/",
                              suffix, sizeof(suffix)))
    {
        CHtmlGameChestEntry *entry;
        CHtmlGameChestEditDlg *dlg;
        const char *desc;
        int desc_is_html;
        
        /* get the entry for our index, as given by the suffix */
        entry = gch_info_->web_group_.arr_[atoi(suffix)];

        /* 
         *   get the description - look for "htmldesc" first, and the
         *   regular "desc" only if that fails 
         */
        desc = entry->get_value("htmldesc");
        desc_is_html = (desc != 0);
        if (desc == 0)
            desc = entry->get_value("desc");

        /* run the web site dialog in "edit" mode */
        dlg = new CHtmlGameChestEditDlg(gch_info_, "Edit Web Site Link",
                                        entry->get_value("url"),
                                        entry->get_value("name"),
                                        desc, desc_is_html, "", FALSE, 0, 0);
        if (dlg->run_modal(DLG_GAME_CHEST_URL, handle_,
                           CTadsApp::get_app()->get_instance()) == IDOK)
        {
            /* delete the old values */
            entry->del_value("url");
            entry->del_value("name");
            entry->del_value("desc");
            entry->del_value("htmldesc");

            /* set the new entry's values from the dialog */
            entry->add_value("url", dlg->get_key());
            if (dlg->get_name() != 0)
                entry->add_value("name", dlg->get_name());
            if (dlg->get_desc() != 0)
                entry->add_value(dlg->desc_is_html() ? "htmldesc" : "desc",
                                 dlg->get_desc());

            /* 
             *   re-sort the web site list, in case the entry's sort
             *   criteria changed 
             */
            gch_info_->web_group_.sort();

            /* redisplay the list */
            show_game_chest(FALSE, FALSE, TRUE);
        }

        /* done with the dialog */
        delete dlg;
    }
    else if (gc_cmd_prefix_eq(cmd, cmdlen, "remove-url/",
                              suffix, sizeof(suffix)))
    {
        /* confirm the removal */
        if (MessageBox(0, "Are you sure you want to remove "
                       "this Web link from your Game Chest?", "TADS",
                       MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL) == IDYES)
        {
            /* delete the entry given by the index number in the suffix */
            gch_info_->web_group_.del_entry(atoi(suffix));

            /* redisplay the list */
            show_game_chest(FALSE, FALSE, TRUE);
        }
    }
    else if (gc_cmd_prefix_eq(cmd, cmdlen, "go-url/",
                              suffix, sizeof(suffix)))
    {
        const char *url;
        
        /* get the URL for the entry */
        url = gch_info_->web_group_.arr_[atoi(suffix)]->get_value("url");

        /* open the URL */
        if ((unsigned long)ShellExecute(0, "open", url, 0, 0, 0) <= 32)
            MessageBox(0, "Unable to start browser.  You must have a "
                       "Web browser installed to show this link.", "TADS",
                       MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Find-and-add wizard control object 
 */
class CHtmlGCSearchWiz
{
public:
    CHtmlGCSearchWiz(CHtmlGameChest *gch_info, CHtmlSys_mainwin *mainwin)
    {
        /* remember the game chest information object and main window */
        gch_info_ = gch_info;
        mainwin_ = mainwin;
        
        /* we haven't modified the game list yet */
        modified_ = FALSE;

        /* not yet initialized */
        inited_ = FALSE;

        /* we have no search root yet */
        root_[0] = '\0';

        /* we don't know the tree view handle yet */
        tv_ = 0;
    }

    /* Game Chest information object */
    CHtmlGameChest *gch_info_;

    /* main game window */
    CHtmlSys_mainwin *mainwin_;

    /* the root filename of the search */
    char root_[OSFNMAX];

    /* flag: we've initialized the wizard */
    int inited_;

    /* we modified the game list */
    int modified_;

    /* handle to tree view containing the game search results list */
    HWND tv_;
};

/*
 *   Root class for find-and-add wizard pages 
 */
class CHtmlGCSearchWizPage: public CTadsDialogPropPage
{
public:
    CHtmlGCSearchWizPage(CHtmlGCSearchWiz *wiz)
    {
        /* remember the wizard control object */
        wiz_ = wiz;
    }

    /* initialize */
    void init()
    {
        /* if we haven't yet done so, center the dialog */
        if (!wiz_->inited_)
        {
            /* center the parent dialog */
            center_dlg_win(GetParent(handle_));

            /* note that we've done this so that other pages don't */
            wiz_->inited_ = TRUE;
        }

        /* inherit default */
        CTadsDialogPropPage::init();
    }

protected:
    /* wizard control object */
    CHtmlGCSearchWiz *wiz_;
};

/*
 *   Find-and-add wizard - root folder page.  This page of the wizard lets
 *   the user select the root folder of the search. 
 */
class CHtmlGCSearchWizRoot: public CHtmlGCSearchWizPage
{
public:
    CHtmlGCSearchWizRoot(CHtmlGCSearchWiz *wiz)
        : CHtmlGCSearchWizPage(wiz)
    {
        /* we don't have a folder selected yet */
        have_folder_ = FALSE;
    }

    /* process a notification */
    int do_notify(NMHDR *nm, int ctl)
    {
        switch(nm->code)
        {
        case PSN_SETACTIVE:
            /* 
             *   activating the page - enable the 'next' button only if we
             *   have a folder, and disable the 'back' button 
             */
            SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                        have_folder_ ? PSWIZB_NEXT : 0);

            /* handled */
            return TRUE;
        }

        /* not handled - inherit default */
        return CHtmlGCSearchWizPage::do_notify(nm, ctl);
    }
    
    /* process a command */
    int do_command(WPARAM id, HWND ctl)
    {
        /* see what we have */
        switch(LOWORD(id))
        {
        case IDC_BTN_BROWSE:
            /* browse for a folder */
            {
                char prompt[256];
                char title[256];
                char fname[_MAX_PATH];

                /* get the title and prompt resource strings */
                LoadString(CTadsApp::get_app()->get_instance(),
                           IDS_GCH_SEARCH_FOLDER_TITLE,
                           title, sizeof(title));
                LoadString(CTadsApp::get_app()->get_instance(),
                           IDS_GCH_SEARCH_FOLDER_PROMPT,
                           prompt, sizeof(prompt));

                /* 
                 *   use the current field contents as the initial folder,
                 *   if we've already selected a folder; if we haven't, then
                 *   initialize to the current open-file directory 
                 */
                fname[0] = '\0';
                if (have_folder_)
                    GetDlgItemText(handle_, IDC_TXT_FOLDER,
                                   fname, sizeof(fname));
                else if (CTadsApp::get_app()->get_openfile_dir() != 0)
                    strcpy(fname, CTadsApp::get_app()->get_openfile_dir());

                /* if we still don't have anything, use the current folder */
                if (fname[0] == '\0')
                    GetCurrentDirectory(sizeof(fname), fname);

                /* run the folder selector dialog */
                if (CTadsDialogFolderSel2::run_select_folder(
                    handle_, CTadsApp::get_app()->get_instance(),
                    prompt, title, fname, sizeof(fname), fname, 0, 0))
                {
                    /* put the new folder into the field */
                    SetDlgItemText(handle_, IDC_TXT_FOLDER, fname);

                    /* copy it to the wizard object */
                    strcpy(wiz_->root_, fname);

                    /* enable the 'next' button */
                    SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                                PSWIZB_NEXT);

                    /* flag that we have a folder */
                    have_folder_ = TRUE;
                }
            }

            /* handled */
            return TRUE;
        }

        /* if we made it this far, we didn't handle it specially */
        return CHtmlGCSearchWizPage::do_command(id, ctl);
    }

protected:
    /* flag: we've selected a folder */
    int have_folder_;
};

/* our special 'background thread done' command code */
#define CMD_SEARCH_DONE   1001

/*
 *   Extra data object for results list entries.
 */
struct lb_results_info
{
    /* 
     *   the full filename of the item (the structure is over-allocated to
     *   accommodate this) 
     */
    char fname[1];
};

/*
 *   Find-and-add wizard - search busy dialog.  This is a simple pop-up
 *   dialog that appears while the search is running to show progress. 
 */
class CHtmlGCSearchBusyDlg: public CTadsDialog
{
public:
    CHtmlGCSearchBusyDlg(HWND pg, const char *root_dir)
    {
        /* remember the search results wizard page */
        pg_ = pg;

        /* remember the root directory of the search */
        root_dir_ = root_dir;
    }

    /* initialize */
    void init()
    {
        DWORD thread_id;
        HANDLE thread_hdl;
        LONG style;
        HWND tv;

        /* 
         *   make sure the "checkboxes" style is set the in treeview - we
         *   have to do this explicitly before we populate the tree in order
         *   to ensure that the checkmarks will show up 
         */
        tv = GetDlgItem(pg_, IDC_TV_GAMES);
        style = GetWindowLong(tv, GWL_STYLE);
        SetWindowLong(tv, GWL_STYLE, style | TVS_CHECKBOXES);

        /* start our search thread */
        thread_hdl = CreateThread(0, 0,
                                  (LPTHREAD_START_ROUTINE)search_thread_main,
                                  (void *)this, 0, &thread_id);

        /* if it failed, report the problem */
        if (thread_hdl == 0)
        {
            /* complain about it */
            MessageBox(handle_, "Error starting search - the "
                       "system might be running low on memory.",
                       "TADS", MB_OK | MB_ICONEXCLAMATION);

            /* terminate the dialog */
            EndDialog(handle_, 0);
        }
        else
        {
            /* 
             *   we don't need the thread handle for anything else, so close
             *   it 
             */
            CloseHandle(thread_hdl);
        }
    }

    /* command handler */
    virtual int do_command(WPARAM id, HWND ctl)
    {
        /* find out which command we have */
        switch(LOWORD(id))
        {
        case CMD_SEARCH_DONE:
            /* it's our special 'done' message */
            EndDialog(handle_, 0);
            return TRUE;

        case IDCANCEL:
        case IDOK:
            /* ignore any return/escape keys */
            return TRUE;
        }

        /* inherit default handling */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    /* search thread - static entrypoint */
    static void search_thread_main(void *ctx)
    {
        /* 
         *   the context is the 'this' pointer; use it to invoke our
         *   download thread member function 
         */
        ((CHtmlGCSearchBusyDlg *)ctx)->search_main();
    }

    /* main search */
    void search_main()
    {
        /* perform the search */
        search_dir(root_dir_, root_dir_);

        /* notify the main thread that it's time to terminate */
        PostMessage(handle_, WM_COMMAND, CMD_SEARCH_DONE, 0);
    }

    /* 
     *   search a directory and its subdirectories for game files, adding
     *   each valid file we find to our file listbox 
     */
    void search_dir(const char *start_dir, const char *dir)
    {
        char pat[_MAX_PATH];
        HANDLE fhnd;
        WIN32_FIND_DATA info;
        HWND tv;

        /* get the listbox control (actually a tree view control) */
        tv = GetDlgItem(pg_, IDC_TV_GAMES);

        /* show the current folder in the progress field */
        SetDlgItemText(handle_, IDC_TXT_FOLDER, dir);

        /* build a pattern string for everything in the directory */
        build_fname(pat, dir, "*");

        /* enumerate the directory's contents */
        fhnd = FindFirstFile(pat, &info);
        if (fhnd == INVALID_HANDLE_VALUE)
        {
            /* no files in the directory - there's nothing for us to do */
            return;
        }

        /* process each file */
        for (;;)
        {
            char fname[_MAX_PATH];
            char *disp_fname;

            /* build the full path name for this file */
            build_fname(fname, dir, info.cFileName);

            /* 
             *   For display purposes, get the part of the filename that
             *   comes after the common starting directory prefix. We can
             *   leave out the common prefix part in displays, because the
             *   user explicitly asked us to search below that point in the
             *   file hierarchy.
             *   
             *   If the filename doesn't in fact start with the root
             *   directory as the prefix, then use the entire filename as
             *   the display name.  
             */
            if (strlen(fname) > strlen(start_dir)
                && memicmp(fname, start_dir, strlen(start_dir)) == 0)
            {
                /* use the part after the starting directory prefix */
                disp_fname = fname + strlen(start_dir);

                /* if there's a leading path separator, skip it */
                if (*disp_fname == '\\')
                    ++disp_fname;
            }
            else
            {
                /* it doesn't have the common prefix - use the whole name */
                disp_fname = fname;
            }

            /* determine what we have */
            if (info.cFileName[0] == '.'
                && (info.cFileName[1] == '\0'
                    || (info.cFileName[1] == '.'
                        && info.cFileName[2] == '\0')))
            {
                /* 
                 *   '.' or '..' - ignore these, since they're just links to
                 *   self and parent - we don't even want to recurse into
                 *   these, since they're not subdirectories 
                 */
            }
            else if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                /* it's a subdirectory - simply recurse into it */
                search_dir(start_dir, fname);
            }
            else
            {
                /* 
                 *   It's an ordinary file.  If it ends in one of our game
                 *   file suffixes, check its contents to see if it looks
                 *   like a valid game file for tads 2 or tads 3.  
                 */
                if (str_ends_with(info.cFileName, ".gam")
                    || str_ends_with(info.cFileName, ".t3"))
                {
                    CTadsGameInfo *info;
                    char *title;
                    HWND lb;
                    TVINSERTSTRUCT ins;
                    char exe_name[OSFNMAX];

                    /* get the results listbox handle */
                    lb = GetDlgItem(handle_, IDC_LB_RESULTS);

                    /* check the game type */
                    switch(vm_get_game_type(fname, 0, 0, 0, 0))
                    {
                    case VM_GGT_TADS2:
                    case VM_GGT_TADS3:
                        /* 
                         *   It's a valid game, so we want to add it to the
                         *   listbox.
                         *   
                         *   Check to see if the game has built-in game
                         *   information; if so, show the game's title from
                         *   the game information rather than just the
                         *   filename.  Note before going in that we don't
                         *   have a title yet, in case we don't find game
                         *   info after all and need to use the filename.  
                         */
                        title = 0;

                        /* 
                         *   get the name of the executable (the game info
                         *   reader will need this, so it knows where to
                         *   find the character mapper file) 
                         */
                        GetModuleFileName(CTadsApp::get_app()->get_instance(),
                                          exe_name, sizeof(exe_name));

                        /* 
                         *   set up the reader (using the default local
                         *   display character set) 
                         */
                        info = new CTadsGameInfoLocal(exe_name);

                        /* try reading the game info */
                        if (info->read_from_file(fname))
                        {
                            const char *val;
                            
                            /* try to find a "name" field */
                            val = info->get_val("Name");

                            /* if we got a valid name, use it as the title */
                            if (val != 0)
                            {
                                /* 
                                 *   allocate space for our combination
                                 *   string: include the title from the
                                 *   gameinfo, and add space for a space, a
                                 *   pair of parens, the display filename,
                                 *   and a null terminator 
                                 */
                                title = (char *)_alloca(
                                    strlen(val) + 1 + 2
                                    + strlen(disp_fname) + 1);

                                /* build the string */
                                sprintf(title, "%s (%s)", val, disp_fname);
                            }
                            else
                            {
                                /* 
                                 *   no "Name" - the gameinfo is probably
                                 *   invalid, so remark about it in the
                                 *   display name 
                                 */
                                title = (char *)_alloca(
                                    strlen(disp_fname) + 25);

                                /* build the display filename */
                                sprintf(title, "%s (invalid GameInfo)",
                                        disp_fname);
                            }
                        }

                        /* done with the info object */
                        delete info;

                        /* 
                         *   if we didn't get a title from the game's
                         *   built-in information, just show the display
                         *   filename 
                         */
                        if (title == 0)
                            title = disp_fname;

                        /* add a listbox item using the title we settled on */
                        ins.hParent = TVI_ROOT;
                        ins.hInsertAfter = TVI_SORT;
                        ins.item.mask = TVIF_PARAM | TVIF_STATE | TVIF_TEXT;
                        ins.item.state = INDEXTOSTATEIMAGEMASK(2);
                        ins.item.stateMask = TVIS_STATEIMAGEMASK;
                        ins.item.pszText = title;
                        ins.item.lParam = (LPARAM)
                                          alloc_lb_results_info(fname);
                        TreeView_InsertItem(tv, &ins);

                        /* that's all we need to do with it */
                        break;

                    default:
                        /* it's not a valid game file - ignore it */
                        break;
                    }
                }
            }

            /* move on to the next file */
            if (!FindNextFile(fhnd, &info))
            {
                /* no more files - we're done */
                break;
            }
        }

        /* close the find handle */
        FindClose(fhnd);
    }

    /* check to see if a string ends with the given suffix */
    int str_ends_with(const char *str, const char *suffix)
    {
        size_t len;
        size_t suffix_len;

        /* get the length of the string and of the suffix */
        len = strlen(str);
        suffix_len = strlen(suffix);

        /* 
         *   if the string isn't at least as long as the suffix, it
         *   obviously doesn't end with the suffix 
         */
        if (len < suffix_len)
            return FALSE;

        /* 
         *   compare the end of the string to the suffix without regard to
         *   case, and return true if they match, false if not 
         */
        return (memicmp(str + len - suffix_len, suffix, suffix_len) == 0);
    }

    /*
     *   Build a full filename given the path prefix and root name 
     */
    void build_fname(char *result, const char *path, const char *fname)
    {
        size_t len;

        /* get the length of the path part, and copy the path */
        len = strlen(path);
        memcpy(result, path, len);
        
        /* if there's not already a '\' at the end of this, add one */
        if (len == 0 || result[len-1] != '\\')
            result[len++] = '\\';
        
        /* add the filename */
        strcpy(result + len, fname);
    }

    /* 
     *   allocate a results listbox item, adding it to our master list for
     *   eventual cleanup
     */
    lb_results_info *alloc_lb_results_info(const char *fname)
    {
        lb_results_info *info;
        
        /* 
         *   allocate a new structure, making extra room to contain the full
         *   filename string 
         */
        info = (lb_results_info *)th_malloc(sizeof(lb_results_info)
                                            + strlen(fname));

        /* save the filename */
        strcpy(info->fname, fname);

        /* return the new item */
        return info;
    }

    /* the search results wizard page */
    HWND pg_;

    /* root directory of the search */
    const char *root_dir_;
};

/* begin-search command code */
#define CMD_BEGIN_SEARCH   1001

/*
 *   Find-and-add wizard - files page.  This page shows the results of the
 *   search and allows the user to select which files to include in the Game
 *   Chest.  
 */
class CHtmlGCSearchWizFiles: public CHtmlGCSearchWizPage
{
public:
    CHtmlGCSearchWizFiles(CHtmlGCSearchWiz *wiz)
        : CHtmlGCSearchWizPage(wiz)
    {
        /* we haven't searched anything yet */
        old_root_[0] = '\0';
    }

    /* initialize */
    void init()
    {
        /* inherit default handling */
        CHtmlGCSearchWizPage::init();

        /* 
         *   tell the wizard controller object about our tree view - this
         *   will let us get at the search results list when we're ready to
         *   finish the wizard processing 
         */
        wiz_->tv_ = GetDlgItem(handle_, IDC_TV_GAMES);
    }

    /* handle destruction */
    void destroy()
    {
        /* 
         *   clear the listbox, so that we delete any extra information
         *   items we allocated with it 
         */
        clear_listbox();

        /* inherit default handling */
        CHtmlGCSearchWizPage::destroy();
    }

    /* command handler */
    virtual int do_command(WPARAM id, HWND ctl)
    {
        /* find out which command we have */
        switch(LOWORD(id))
        {
        case CMD_BEGIN_SEARCH:
            /* run the search */
            run_search();

            /* handled */
            return TRUE;
        }

        /* it's not one of ours - inherit default handling */
        return CTadsDialog::do_command(id, ctl);
    }

    /* process a notification */
    int do_notify(NMHDR *nm, int ctl)
    {
        switch(nm->code)
        {
        case PSN_SETACTIVE:
            /* enable the 'back' and 'next' button on this page */
            SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                        PSWIZB_NEXT | PSWIZB_BACK);

            /* 
             *   If the search root has changed since we were last
             *   activated, run a new search.  In order to allow the dialog
             *   to initialize properly before we begin the search, post a
             *   message to initiate the search, so we don't start it until
             *   the dialog's UI initialization is completed.  
             */
            if (strcmp(wiz_->root_, old_root_) != 0)
                PostMessage(handle_, WM_COMMAND, CMD_BEGIN_SEARCH, 0);

            /* handled */
            return TRUE;

        case TVN_DELETEITEM:
            /* 
             *   an item is being deleted from the game list - delete the
             *   extra information associated with the item 
             */
            th_free((lb_results_info *)((NMTREEVIEW *)nm)->itemOld.lParam);

            /* handled */
            return TRUE;
        }

        /* not handled - inherit default */
        return CHtmlGCSearchWizPage::do_notify(nm, ctl);
    }
    
protected:
    /* run the search */
    void run_search()
    {
        CHtmlGCSearchBusyDlg *dlg;

        /* show the busy cursor while we're working */
        SetCursor(LoadCursor(0, IDC_WAIT));

        /* clear any previous search results from the listbox */
        clear_listbox();

        /* 
         *   remember the root directory, so we'll know whether or not we
         *   have to run the search again later (we only have to run a new
         *   search when the directory changes) 
         */
        strcpy(old_root_, wiz_->root_);

        /* create and show our search dialog */
        dlg = new CHtmlGCSearchBusyDlg(handle_, wiz_->root_);
        dlg->run_modal(DLG_GAME_CHEST_SEARCH_BUSY, GetParent(handle_),
                       CTadsApp::get_app()->get_instance());

        /* done with the dialog - delete it */
        delete dlg;
    }

    /* clear the listbox */
    void clear_listbox()
    {
        /* delete all items from the list */
        TreeView_DeleteAllItems(GetDlgItem(handle_, IDC_TV_GAMES));
    }

    /* old search root */
    char old_root_[OSFNMAX];
};

/*
 *   Find-and-add wizard - group page.  This page lets the user select the
 *   group to which the new games will be added.  
 */
class CHtmlGCSearchWizGroup:
    public CHtmlGCSearchWizPage, public CDlgWithGroupPopup
{
public:
    CHtmlGCSearchWizGroup(CHtmlGCSearchWiz *wiz)
        : CHtmlGCSearchWizPage(wiz) { }

    /* initialize */
    void init()
    {
        /* inherit default handling */
        CHtmlGCSearchWizPage::init();

        /* load the group popup */
        init_group_popup(handle_, wiz_->gch_info_, 0);
    }

    /* process a notification */
    int do_notify(NMHDR *nm, int ctl)
    {
        switch(nm->code)
        {
        case PSN_SETACTIVE:
            /* enable the 'back' and 'finish' button on this page */
            SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                        PSWIZB_FINISH | PSWIZB_BACK);

            /* handled */
            return TRUE;

        case PSN_WIZFINISH:
            /* it's the 'finish' button - add the selected games */
            add_results_to_favorites();

            /* tell the wizard that finishing is okay */
            SetWindowLong(handle_, DWL_MSGRESULT, FALSE);

            /* handled */
            return TRUE;
        }

        /* not handled - inherit default */
        return CHtmlGCSearchWizPage::do_notify(nm, ctl);
    }

protected:
    /* add the results in the listbox to the game chest favorites */
    void add_results_to_favorites()
    {
        int group_id;
        size_t i;
        size_t cnt;
        size_t added_cnt;
        HTREEITEM hitem;
        HWND tv = wiz_->tv_;

        /* get the target group ID from the popup */
        group_id = get_group_id_from_popup(handle_, wiz_->gch_info_);

        /* get the number of listbox entries */
        cnt = TreeView_GetCount(wiz_->tv_);

        /* start at the first item in the tree */
        hitem = TreeView_GetRoot(tv);

        /* add each selected entry in the tree */
        for (i = 0, added_cnt = 0 ; i < cnt ;
             ++i, hitem = TreeView_GetNextItem(tv, hitem, TVGN_NEXT))
        {
            TVITEM item;
            lb_results_info *info;

            /* 
             *   get this item's information; we want the state image index,
             *   so we can tell whether or not the item is checked, and we
             *   want the lParam, which gives us our associated information
             *   structure for the item 
             */
            item.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
            item.hItem = hitem;
            item.state = 0;
            item.stateMask = TVIS_STATEIMAGEMASK;
            if (!TreeView_GetItem(tv, &item))
                continue;

            /* 
             *   if the item's state image index isn't 2 (which is the
             *   treeview code for 'checked'), they don't want to include
             *   this game in the added batch, so skip the item 
             */
            if ((item.state & TVIS_STATEIMAGEMASK)
                != INDEXTOSTATEIMAGEMASK(2))
                continue;

            /* 
             *   get the item's associated information structure; ignore the
             *   item if it has no such information 
             */
            info = (lb_results_info *)item.lParam;
            if (info == 0)
                continue;

            /* 
             *   add the game to the favorites directly, without going
             *   through the editor dialog 
             */
            wiz_->mainwin_->game_chest_add_fav(info->fname, group_id, FALSE);

            /* count this added item */
            ++added_cnt;
        }

        /* 
         *   if we've added any items, note that the game chest information
         *   has been modified 
         */
        if (added_cnt != 0)
            wiz_->modified_ = TRUE;
    }
};

/*
 *   Add a directory's contents to the favorite games list.  This runs the
 *   find-and-add wizard, and updates the game list if we complete it
 *   successfully.  Returns true if we modified the game list, false if not.
 */
int CHtmlSys_mainwin::game_chest_add_fav_dir(const char *dir)
{
    PROPSHEETPAGE psp[10];
    PROPSHEETHEADER psh;
    CHtmlGCSearchWiz wiz(gch_info_, this);
    CHtmlGCSearchWizRoot *root_page;
    CHtmlGCSearchWizFiles *files_page;
    CHtmlGCSearchWizGroup *group_page;
    int i;
    
    /* set up the wizard pages */
    root_page = new CHtmlGCSearchWizRoot(&wiz);
    files_page = new CHtmlGCSearchWizFiles(&wiz);
    group_page = new CHtmlGCSearchWizGroup(&wiz);

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_USEICONID | PSH_PROPSHEETPAGE | PSH_WIZARD;
    psh.hwndParent = handle_;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Find and Add Games";
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* set up the page descriptors */
    i = 0;
    root_page->init_ps_page(CTadsApp::get_app()->get_instance(),
                            &psp[i++], DLG_GAME_CHEST_SEARCH_ROOT,
                            IDS_GAME_CHEST_SEARCH_ROOT, 0, 0, &psh);
    files_page->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_GAME_CHEST_SEARCH_RESULTS,
                             IDS_GAME_CHEST_SEARCH_RESULTS, 0, 0, &psh);
    group_page->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_GAME_CHEST_SEARCH_GROUP,
                             IDS_GAME_CHEST_SEARCH_GROUP, 0, 0, &psh);

    /* set the number of pages */
    psh.nPages = i;

    /* run the property sheet */
    PropertySheet(&psh);

    /* delete the dialogs */
    delete root_page;
    delete files_page;
    delete group_page;

    /* indicate whether or not we modified anything */
    return wiz.modified_;
}


/* ------------------------------------------------------------------------ */
/*
 *   Add a file to the favorite games list
 */
int CHtmlSys_mainwin::game_chest_add_fav(const char *fname, int group_id,
                                         int interactive)
{
    CHtmlGameChestEditDlg *dlg;
    int added;
    CTadsGameInfo *info;
    char *tmp;
    const char *game_name;
    const char *game_desc;
    const char *game_byline;
    int game_desc_is_html;
    int game_byline_is_html;
    char exe_name[OSFNMAX];
    const char *profile;

    /* make sure we have the latest version in memory */
    read_game_chest_file_if_needed();

    /* we don't have a name or description for the game yet */
    game_name = 0;
    game_desc = 0;
    game_byline = 0;
    game_desc_is_html = FALSE;
    game_byline_is_html = FALSE;
    profile = 0;

    /* 
     *   get the name of the executable (the game info reader will need
     *   this, so it knows where to find the character mapper file) 
     */
    GetModuleFileName(CTadsApp::get_app()->get_instance(),
                      exe_name, sizeof(exe_name));
    
    /* set up the reader (using the default local display character set) */
    info = new CTadsGameInfoLocal(exe_name);

    /* try reading the game info */
    if (info->read_from_file(fname))
    {
        /* get the game's name (given by the "Name" field) */
        game_name = info->get_val("Name");
        if (game_name == 0)
        {
            /* no "Name" - show a warning about the GameInfo */
            MessageBox(handle_, "This story file contains a \"GameInfo\" "
                       "record, but it appears to be invalid.  This is okay; "
                       "it just means you'll have to fill in the name and "
                       "description manually.  You might want to contact "
                       "the story's author to let them know about the "
                       "problem.", "TADS - Missing GameInfo",
                       MB_OK | MB_ICONEXCLAMATION);
        }

        /* 
         *   First, try pulling the HTML description (given by the
         *   "HtmlDesc" field).  If we have both a plain and an HTML
         *   description, we'll use the HTML version, since we're capable of
         *   displaying it and it's an inherently richer format.  
         */
        game_desc = info->get_val("HtmlDesc");

        /* 
         *   if we got the html description, note that our description is in
         *   HTML format; otherwise, try the plain text description (given
         *   by the "Desc" field) 
         */
        if (game_desc != 0)
            game_desc_is_html = TRUE;
        else
            game_desc = info->get_val("Desc");

        /* try pulling the HTML byline information, if present */
        game_byline = info->get_val("HtmlByline");
        if (game_byline != 0)
            game_byline_is_html = TRUE;
        else
            game_byline = info->get_val("Byline");

        /* get the suggested profile */
        profile = info->get_val("PresentationProfile");
    }

    /* 
     *   if we don't have a name, assign a default of the root filename
     *   minus the extension
     */
    if (game_name == 0)
    {
        char *root_name;
        
        /* get the root of the game name */
        root_name = os_get_root_name((char *)fname);

        /* make a copy of the root name */
        game_name = tmp = (char *)_alloca(strlen(root_name) + 1);
        strcpy(tmp, root_name);

        /* remove the extension from our copy */
        os_remext(tmp);
    }

    /* if we don't have a description, make it empty by default */
    if (game_desc == 0)
        game_desc = "";

    /* use an empty profile if none is suggested */
    if (profile == 0)
        profile = "";

    /* presume we won't add anything */
    added = FALSE;

    /* check to see if we're running interactively or not */
    if (interactive)
    {
        /* interactive mode - run the game file dialog in "add" mode */
        dlg = new CHtmlGameChestEditDlg(gch_info_, "Add Story File Link",
                                        fname, game_name,
                                        game_desc, game_desc_is_html,
                                        game_byline, game_byline_is_html,
                                        group_id, profile);
        if (dlg->run_modal(DLG_GAME_CHEST_GAME, handle_,
                           CTadsApp::get_app()->get_instance()) == IDOK)
        {
            /* they accepted - note that we're adding the value */
            added = TRUE;

            /* retrieve the new values from the dialog */
            fname = dlg->get_key();
            game_name = dlg->get_name();
            game_desc = dlg->get_desc();
            game_desc_is_html = dlg->desc_is_html();
            game_byline = dlg->get_byline();
            game_byline_is_html = dlg->byline_is_html();
            group_id = dlg->get_group_id();
            profile = dlg->get_profile();
        }
    }
    else
    {
        /* non-interactive mode - there's no dialog */
        dlg = 0;

        /* definitely add the entry, using the values we already have */
        added = TRUE;
    }

    /* if we decided to add the new entry, do it */
    if (added)
    {
        CHtmlGameChestEntry *entry;
            
        /* create a new entry */
        entry = gch_info_->game_group_.add_entry();
            
        /* set the new entry's values from the dialog */
        entry->add_value("file", fname);
        entry->add_value("group-id", group_id);
        if (game_name != 0)
            entry->add_value("name", game_name);
        if (game_desc != 0)
            entry->add_value(game_desc_is_html ? "htmldesc" : "desc",
                             game_desc);
        if (game_byline != 0)
            entry->add_value(game_byline_is_html ? "htmlbyline" : "byline",
                             game_byline);

        /* save the profile association */
        set_profile_assoc(fname, profile);

        /* 
         *   re-sort the game list, in case the entry's sort criteria
         *   changed 
         */
        gch_info_->game_group_.sort();
    }

    /* 
     *   We're done with the game information object, so delete it.  Note
     *   that we wait until now, because we might have been keeping
     *   references to the value strings we obtained; once we've added the
     *   new gameinfo record to our database, we have our own copies of all
     *   of the value strings, so we don't need the game information
     *   object's values any more.  
     */
    delete info;

    /* 
     *   if we created a dialog, we're done with it now (we saved it this
     *   long because we had pointers [fname, game_name, game_desc] into
     *   memory it was managing, and we needed those pointers until now) 
     */
    if (dlg != 0)
        delete dlg;

    /* return an indication as to whether or not the file was added */
    return added;
}

/* ------------------------------------------------------------------------ */
/*
 *   Activate or deactivate the application - game-chest handling
 */
void CHtmlSys_mainwin::do_activate_game_chest(int flag)
{
    /* if we're not showing the game chest page, there's nothing to do */
    if (main_panel_ == 0 || !main_panel_->is_in_game_chest())
        return;

    /* check if we're coming or going */
    if (flag)
    {
        FILETIME ft;
        
        /* 
         *   We're activating - reload the game chest if it's been modified
         *   since the last time we loaded or saved it.  Also reload if the
         *   background image has changed.  
         */
        get_game_chest_timestamp(&ft);
        if (memcmp(&ft, &gch_timestamp_, sizeof(ft)) != 0
            || note_gc_bkg())
            show_game_chest(FALSE, TRUE, FALSE);
    }
    else
    {
        /* we're deactivating - save the game chest if it's dirty */
        save_game_chest_file();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Drop Target implementation for game chest mode 
 */

/*
 *   start dragging over this window 
 */
int CHtmlSysWin_win32_Input::DragEnter_game_chest(
    IDataObject __RPC_FAR *dataobj, DWORD grfKeyState, POINTL pt,
    DWORD __RPC_FAR *effect)
{
    FORMATETC fmtetc;

    /* presume we won't find one of our drag/drop types */
    drag_gch_file_valid_ = FALSE;
    drag_gch_url_valid_ = FALSE;

    /* if we're not in game-chest mode, it's definitely not for us */
    if (!in_game_chest_)
        return FALSE;

    /* 
     *   check to see if the data object can provide an HDROP object, which
     *   is a set of desktop files being dragged 
     */
    fmtetc.cfFormat = CF_HDROP;
    fmtetc.ptd = 0;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex = -1;
    fmtetc.tymed = TYMED_HGLOBAL;
    if (dataobj->QueryGetData(&fmtetc) == S_OK)
    {
        /* note that this is a game-chest drop */
        drag_gch_file_valid_ = TRUE;

        /* the drop effect for files is always "link" */
        *effect = DROPEFFECT_LINK;

        /* it's ours */
        return TRUE;
    }

    /* check to see if it's a URL drop */
    fmtetc.cfFormat = (CLIPFORMAT)url_clipboard_fmt_;
    fmtetc.ptd = 0;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex = -1;
    fmtetc.tymed = TYMED_HGLOBAL;
    if (dataobj->QueryGetData(&fmtetc) == S_OK)
    {
        /* note that this is a game-chest URL drop */
        drag_gch_url_valid_ = TRUE;

        /* 
         *   we're going to copy the file from the website to this location,
         *   but IE will only allow the drag if we indicate a drop effect of
         *   "link," so we're stuck with that even though it's not quite
         *   right 
         */
        *effect = DROPEFFECT_LINK;

        /* it's ours */
        return TRUE;
    }

    /* it's not for us */
    return FALSE;
}

/*
 *   continue dragging over this window
 */
int CHtmlSysWin_win32_Input::DragOver_game_chest(
    DWORD /*grfKeyState*/, POINTL pt, DWORD __RPC_FAR *effect)
{
    /* if this is a game-chest file drag, proceed */
    if (drag_gch_file_valid_)
    {
        /* proceed with the "link" effect */
        *effect = DROPEFFECT_LINK;

        /* it's ours */
        return TRUE;
    }

    /* check for a URL drag */
    if (drag_gch_url_valid_)
    {
        /* proceed with the "link" effect */
        *effect = DROPEFFECT_LINK;
        return TRUE;
    }

    /* it's not ours */
    return FALSE;
}

int CHtmlSysWin_win32_Input::DragLeave_game_chest()
{
    /* if it's ours, clean up */
    if (drag_gch_file_valid_ || drag_gch_url_valid_)
    {
        /* no longer in game chest drag/drop */
        drag_gch_file_valid_ = FALSE;
        drag_gch_url_valid_ = FALSE;

        /* handled */
        return TRUE;
    }

    /* not ours */
    return FALSE;
}

/*
 *   Check for and process a game chest drop 
 */
int CHtmlSysWin_win32_Input::Drop_game_chest(
    IDataObject __RPC_FAR *dataobj, DWORD /*grfKeyState*/, POINTL /*pt*/,
    DWORD __RPC_FAR *effect)
{
    /* if it's not ours, ignore it and tell the caller to handle it */
    if (drag_gch_file_valid_)
    {
        /* process the file drop */
        drop_gch_file(dataobj, effect);

        /* it was ours - the caller need not do anything more */
        return TRUE;
    }
    else if (drag_gch_url_valid_)
    {
        /* process the URL drop */
        drop_gch_url(dataobj, effect);

        /* it was ours - the caller need not do anything more */
        return TRUE;
    }
    else
    {
        /* it's not ours */
        return FALSE;
    }
}

/*
 *   Process a game chest desktop file drop 
 */
void CHtmlSysWin_win32_Input::drop_gch_file(IDataObject __RPC_FAR *dataobj,
                                            DWORD __RPC_FAR *effect)
{
    FORMATETC fmtetc;
    STGMEDIUM medium;

    /* get the data as an HDROP object */
    fmtetc.cfFormat = CF_HDROP;
    fmtetc.ptd = 0;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex = -1;
    fmtetc.tymed = TYMED_HGLOBAL;
    if (dataobj->GetData(&fmtetc, &medium) == S_OK)
    {
        HDROP hdrop;
        unsigned int cnt;
        unsigned int i;
        int num_added;

        /* bring us to the front before showing any dialogs */
        SetForegroundWindow(owner_->get_owner_frame_handle());

        /* get the hdrop object from the storage medium */
        hdrop = (HDROP)medium.hGlobal;

        /* get the number of files being dropped */
        cnt = DragQueryFile(hdrop, 0xffffffff, 0, 0);

        /* iterate over the files */
        for (i = 0, num_added = 0 ; i < cnt ; ++i)
        {
            char fname[OSFNMAX];
            DWORD attrs;
            char msg[512];
            int typ;

            /* get the name of this file */
            DragQueryFile(hdrop, i, fname, sizeof(fname));

            /* check to see if it's a directory */
            attrs = GetFileAttributes(fname);
            if (attrs != 0xFFFFFFFF
                && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                /* it's a directory - run the search dialog */
                if (owner_->owner_game_chest_add_fav_dir(fname))
                    ++num_added;
            }
            else
            {
                /* it's not a directory - make sure it's a valid game file */
                typ = vm_get_game_type(fname, 0, 0, 0, 0);
                switch (typ)
                {
                case VM_GGT_TADS2:
                case VM_GGT_TADS3:
                    /* valid game file - run the dialog to add this file */
                    if (owner_->owner_game_chest_add_fav(fname, 0, TRUE))
                        ++num_added;
                    break;
                    
                default:
                    /* it's not a valid game file */
                    sprintf(msg, "%s is not a TADS story file.", fname);
                    MessageBox(0, msg, "TADS",
                               MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
                    break;
                }
            }
        }

        /* finish the drag/drop operation */
        if (medium.pUnkForRelease != 0)
            medium.pUnkForRelease->Release();
        else
            DragFinish(hdrop);

        /* update the game chest listing if we added any files */
        if (num_added != 0)
            owner_->owner_show_game_chest(FALSE, FALSE, TRUE);
        
        /* we successfully accepted the drop as a link */
        *effect = DROPEFFECT_LINK;
    }
    else
    {
        /* they couldn't give us the data we wanted - reject it */
        *effect = DROPEFFECT_NONE;
    }
    
    /* we're done with this operation */
    drag_gch_file_valid_ = FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Game chest file download progress dialog 
 */
class CHtmlGameChestDownloadDlg: public CTadsDialog
{
public:
    CHtmlGameChestDownloadDlg(CHtmlSysWin_win32_owner *owner,
                              const char *url,
                              const char *fname)
    {
        /* remember the owner window */
        owner_ = owner;

        /* remember the url and filename */
        url_.set(url);
        fname_.set(fname);

        /* not yet cancelling */
        cancel_ = FALSE;

        /* not in a list yet */
        nxt_ = 0;
    }

    /* get/set the next dialog in the active list */
    CHtmlGameChestDownloadDlg *get_next_dlg() const { return nxt_; }
    void set_next_dlg(CHtmlGameChestDownloadDlg *nxt) { nxt_ = nxt; }

    /* 
     *   Set cancellation status - this sets the flag telling the background
     *   thread to terminate, and disables the "cancel" button in the UI.
     *   We don't process any messages here, so the dialog can't actually
     *   terminate, but we set things up so that the background thread exits
     *   as soon as possible.  
     */
    void set_cancel()
    {
        /* 
         *   set the cancel flag; the download thread will let us know when
         *   it's done 
         */
        cancel_ = TRUE;

        /* no need to keep clicking cancel */
        EnableWindow(GetDlgItem(handle_, IDCANCEL), FALSE);
    }

    /*
     *   Dialog implementation 
     */

    virtual void init()
    {
        DWORD thread_id;
        HANDLE thread_hdl;
        
        /* inherit default initialization */
        CTadsDialog::init();

        /* add myself to the application modal dialog list */
        CTadsApp::get_app()->add_modeless(handle_);

        /* 
         *   add myself to the list of active downloads in the application -
         *   this allows us to receive cancellation notice if the
         *   application is terminated before we're done with the download 
         */
        owner_->owner_add_download(this);

        /* show our text strings */
        SetDlgItemText(handle_, IDC_TXT_URL, url_.get());
        SetDlgItemText(handle_, IDC_TXT_FILE, fname_.get());
        SetDlgItemText(handle_, IDC_TXT_BYTES, "0");

        /* kick off our download thread */
        thread_hdl = CreateThread(0, 0,
                                  (LPTHREAD_START_ROUTINE)dl_thread_main,
                                  (void *)this, 0, &thread_id);

        /* make sure it started */
        if (thread_hdl == 0)
        {
            /* tell them about it */
            MessageBox(handle_, "Error starting download - the system "
                       "might be running low on memory.", "Download",
                       MB_OK | MB_ICONEXCLAMATION);

            /* destroy myself */
            DestroyWindow(handle_);
        }
        else
        {
            /* 
             *   the thread started - we don't need the handle for anything
             *   more, so close it out 
             */
            CloseHandle(thread_hdl);
        }
    }

    virtual void destroy()
    {
        /* remove myself from the modeless dialog list */
        CTadsApp::get_app()->remove_modeless(handle_);

        /* inherit default handling */
        CTadsDialog::destroy();

        /* remove myself from the active list */
        owner_->owner_remove_download(this);

        /* 
         *   Delete myself.  Note that we know for sure the background
         *   thread is done using 'this' at this point, because the only way
         *   we destroy this window is in response to an "ok" notification,
         *   which is sent only by the background thread when it's about to
         *   terminate 
         */
        delete this;
    }

    virtual int do_command(WPARAM id, HWND ctl)
    {
        /* find out which command we have */
        switch(LOWORD(id))
        {
        case IDCANCEL:
            /* 
             *   set the 'cancel' status to tell our download thread to
             *   terminate 
             */
            set_cancel();

            /* handled */
            return TRUE;

        case IDOK:
            /* 
             *   synthetic message telling us that the download completed
             *   successfully - we're done with the dialog visually, but we
             *   still want our object memory around, so hide the dialog for
             *   now 
             */
            ShowWindow(handle_, SW_HIDE);

            /* if we didn't cancel, have a look at the downloaded file */
            if (!cancel_)
            {
                const char *suffix;
                int openable;

                /* get the suffix pointer */
                suffix = strrchr(fname_.get(), '.');

                /* check the file type */
                switch (vm_get_game_type(fname_.get(), 0, 0, 0, 0))
                {
                case VM_GGT_TADS2:
                case VM_GGT_TADS3:
                    /* 
                     *   it's a TADS game file - run the add-to-favorites
                     *   dialog for the downloaded file 
                     */
                    if (owner_->owner_game_chest_add_fav(
                        fname_.get(), 0, TRUE))
                    {
                        /* 
                         *   update the game chest window if it's showing;
                         *   if it's not, don't show it, since we must be in
                         *   the midst of a game 
                         */
                        owner_->owner_show_game_chest(TRUE, FALSE, TRUE);
                    }
                    break;

                default:
                    /*
                     *   It's not a TADS game file.  If the file has a
                     *   suffix, and the suffix is registered as a file
                     *   class with Windows, offer to open the file. 
                     */
                    openable = FALSE;
                    if (suffix != 0)
                    {
                        HKEY suffix_key;
                        DWORD disp;
                        
                        /* try opening the suffix class key */
                        suffix_key = CTadsRegistry::open_key(
                            HKEY_CLASSES_ROOT, suffix, &disp, FALSE);

                        /* 
                         *   if we got that key, try getting the default
                         *   value, which gives the class name 
                         */
                        if (suffix_key != 0)
                        {
                            char val[128];

                            /* get the value */
                            if (CTadsRegistry::query_key_str(
                                suffix_key, "", val, sizeof(val)) != 0)
                            {
                                HKEY class_key;

                                /* ask for the class key */
                                class_key = CTadsRegistry::open_key(
                                    HKEY_CLASSES_ROOT, val, &disp, FALSE);

                                /* 
                                 *   if that succeeded, make sure it doesn't
                                 *   have a "NoOpen" value 
                                 */
                                if (class_key != 0)
                                {
                                    /* query the "NoOpen" value */
                                    if (!CTadsRegistry::value_exists(
                                        class_key, "NoOpen"))
                                    {
                                        /* 
                                         *   it has a class name, and the
                                         *   class key doesn't have a NoOpen
                                         *   attribute, so presume it's an
                                         *   openable file type 
                                         */
                                        openable = TRUE;
                                    }

                                    /* done with the class key */
                                    CTadsRegistry::close_key(class_key);
                                }
                            }

                            /* done with the suffix key - close it */
                            CTadsRegistry::close_key(suffix_key);
                        }
                    }

                    /* 
                     *   If it's openable, offer to open it.  Otherwise,
                     *   just explain the next step 
                     */
                    if (openable)
                    {
                        /* it's openable - offer to open it */
                        if (MessageBox(handle_, "The download was "
                                       "successful, but this is not a TADS "
                                       "story file.  If it's a compressed "
                                       "archive, you must now unpack the "
                                       "file.  After unpacking the file, "
                                       "you can add any .gam or .t3 files "
                                       "to the Game Chest.  Would you like "
                                       "TADS to attempt to open the file in "
                                       "its native application now?",
                                       fname_.get(),
                                       MB_YESNO | MB_ICONQUESTION) == IDYES)
                        {
                            /* try opening it */
                            if ((unsigned long)ShellExecute(
                                0, "open", fname_.get(), 0, 0,
                                SW_SHOWDEFAULT) <= 32)
                                MessageBox(0, "Unable to open file.  "
                                           "You must have an appropriate "
                                           "third-party application "
                                           "installed to open this type "
                                           "of file.", fname_.get(),
                                           MB_OK | MB_ICONEXCLAMATION
                                           | MB_TASKMODAL);
                        }
                    }
                    else
                    {
                        /* 
                         *   we can't open it automatically, so explain the
                         *   next manual step required of the user 
                         */
                        MessageBox(handle_, "The download was successful, "
                                   "but this is not a TADS story file.  If "
                                   "it's a compressed archive, you must now "
                                   "unpack it using a third-party tool "
                                   "(UNZIP, for example).  After you "
                                   "unpack the archive, you can add the "
                                   "extracted .gam or .t3 file(s) to the "
                                   "Game Chest.", fname_.get(),
                                   MB_OK | MB_ICONINFORMATION);
                    }
                    break;
                }
            }

            /* done with the dialog - delete it */
            DestroyWindow(handle_);

            /* handled */
            return TRUE;
            }

        /* not handled */
        return FALSE;
    }

protected:
    /* download thread - static entrypoint */
    static void dl_thread_main(void *ctx)
    {
        /* 
         *   the context is the 'this' pointer; use it to invoke our
         *   download thread member function 
         */
        ((CHtmlGameChestDownloadDlg *)ctx)->dl_main();
    }

    /* download thread - main routine */
    void dl_main()
    {
        FILE *fp;
        HINTERNET hnet = 0;
        HINTERNET hfile = 0;
        long timeout;
        long total_bytes;

        /* open the output file */
        if ((fp = fopen(fname_.get(), "wb")) == 0)
        {
            char buf[512];
            
            /* complain */
            sprintf(buf, "Error creating file %s - you may be low on "
                    "disk space or you may not have permission to write "
                    "to this directory.");
            MessageBox(handle_, buf, "File Download",
                       MB_OK | MB_ICONEXCLAMATION);

            /* give up */
            goto do_cancel;
        }

        /* open the internet connection */
        if ((hnet = InternetOpen("HtmlTADS", INTERNET_OPEN_TYPE_PRECONFIG,
                                 0, 0, 0)) == 0)
        {
            /* complain about it */
            MessageBox(handle_, "Unable to initialize Internet connection "
                       "for download.  You must have Microsoft Internet "
                       "Explorer version 3.0 or higher installed for this "
                       "feature to operate.  If you do not have IE "
                       "installed, please install it and try again.",
                       url_.get(), MB_OK | MB_ICONEXCLAMATION);

            /* give up */
            goto do_cancel;
        }

        /* check for cancellation, in case that took a while */
        if (cancel_)
            goto do_cancel;

        /* set the connection timeout to ensure we don't wait forever */
        timeout = 30000;
        InternetSetOption(hnet, INTERNET_OPTION_CONNECT_TIMEOUT,
                          &timeout, sizeof(timeout));

        /* open the file given by the URL */
        if ((hfile = InternetOpenUrl(hnet, url_.get(), 0, 0,
                                     INTERNET_FLAG_EXISTING_CONNECT,
                                     (DWORD)this)) == 0)
        {
            /* complain about it */
            MessageBox(handle_, "Error connecting to Internet site.",
                       url_.get(), MB_OK | MB_ICONEXCLAMATION);

            /* give up */
            goto do_cancel;
        }

        /* read the file */
        for (total_bytes = 0 ;; )
        {
            char buf[4096];
            DWORD actual;
            
            /* if we've cancelled, give up */
            if (cancel_)
                goto do_cancel;

            /* read some more */
            if (!InternetReadFile(hfile, buf, sizeof(buf), &actual))
            {
                /* complain about it */
                MessageBox(handle_, "Error reading data from Internet.  "
                           "Download aborted.", url_.get(),
                           MB_OK | MB_ICONEXCLAMATION);

                /* give up */
                goto do_cancel;
            }

            /* if we've cancelled, give up */
            if (cancel_)
                goto do_cancel;

            /* if we read zero bytes, we're done */
            if (actual == 0)
                break;

            /* write the data to the file */
            if (fwrite(buf, actual, 1, fp) != 1)
            {
                /* error writing - complain */
                MessageBox(handle_, "Error writing data to file - disk "
                           "may be full.  Download aborted.", fname_.get(),
                           MB_OK | MB_ICONEXCLAMATION);

                /* give up */
                goto do_cancel;
            }

            /* update the total byte display in the download dialog */
            total_bytes += actual;
            sprintf(buf, "%lu", total_bytes);
            SetDlgItemText(handle_, IDC_TXT_BYTES, buf);
        }
        
        /* if we've cancelled, give up */
        if (cancel_)
            goto do_cancel;

        /* we're done writing to the local file, so close it */
        fclose(fp);

        /* close the URL handle and the internet connection */
        InternetCloseHandle(hfile);
        InternetCloseHandle(hnet);

        /* send myself an 'ok' command to tell the dialog to exit */
        PostMessage(handle_, WM_COMMAND, IDOK, 0);

        /* we're done */
        return;

    do_cancel:
        /* if we opened the file, clean it up */
        if (fp != 0)
        {
            /* close the file */
            fclose(fp);

            /* delete the file, since we didn't finish saving it */
            remove(fname_.get());
        }

        /* close the URL handle and internet connection */
        if (hfile != 0)
            InternetCloseHandle(hfile);
        if (hnet != 0)
            InternetCloseHandle(hnet);

        /* set the cancel flag, so the main dialog knows we're done */
        cancel_ = TRUE;

        /* send an "ok" to the dialog to tell it to terminate */
        PostMessage(handle_, WM_COMMAND, IDOK, 0);
    }
    
    /* flag: we're cancelling the download */
    int cancel_;

    /* the URL we're downloading */
    CStringBuf url_;

    /* the local file we're saving */
    CStringBuf fname_;

    /* owner frame interface */
    CHtmlSysWin_win32_owner *owner_;

    /* next dialog in active list */
    CHtmlGameChestDownloadDlg *nxt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Add an active download dialog 
 */
void CHtmlSys_mainwin::add_download(CHtmlGameChestDownloadDlg *dlg)
{
    /* link it into our list */
    dlg->set_next_dlg(download_head_);
    download_head_ = dlg;

    /* the download thread keeps a reference to us */
    AddRef();
}

/*
 *   Remove an active download dialog 
 */
void CHtmlSys_mainwin::remove_download(CHtmlGameChestDownloadDlg *dlg)
{
    CHtmlGameChestDownloadDlg *prv;
    CHtmlGameChestDownloadDlg *cur;

    /* find the previous item in the list */
    for (prv = 0, cur = download_head_ ; cur != 0 && cur != dlg ;
         prv = cur, cur = cur->get_next_dlg()) ;

    /* if we found it, unlink it */
    if (cur == dlg)
    {
        /* unlink the active download from our list */
        if (prv == 0)
            download_head_ = download_head_->get_next_dlg();
        else
            prv->set_next_dlg(dlg->get_next_dlg());

        /* remove the reference from the dialog to us */
        Release();
    }
}

/*
 *   Check before closing to see if any downloads are in progress, and if
 *   so, ask for confirmation that the user wants to cancel them.  Returns
 *   true if closing can proceed, false if the user wants to cancel the
 *   close.  
 */
int CHtmlSys_mainwin::do_close_check_downloads()
{
    /* 
     *   if we have no downloads in progress, allow the close to proceed
     *   without further delay 
     */
    if (download_head_ == 0)
        return TRUE;

    /* 
     *   we have downloads in progress, so ask the caller what to do; return
     *   true if they say "yes" to cancelling the downloads, false if they
     *   say "no" 
     */
    return (MessageBox(handle_,
                       "One or more file downloads are in progress.  "
                       "If you quit now, the downloads will be cancelled.  "
                       "Do you want to cancel the downloads and quit?",
                       "TADS",
                       MB_YESNO | MB_ICONQUESTION) == IDYES);
}

/*
 *   Cancel all downloads and wait for the download threads to exit 
 */
void CHtmlSys_mainwin::cancel_downloads()
{
    CHtmlGameChestDownloadDlg *cur;

    /* 
     *   Tell each dialog to cancel its download thread.  This won't
     *   actually terminate any dialogs: under normal circumstances a dialog
     *   terminates when it processes the IDOK command from its background
     *   thread.  Since message processing takes place exclusively in the
     *   our current thread (i.e., the main app foreground thread), and
     *   we're not processing any messages in the course of this loop, no
     *   dialog will be destroyed.  We thus don't have to worry about the
     *   list changing out from under us or other concurrency concerns.
     *   
     *   Once we cancel all the dialogs, their background threads will
     *   terminate as soon as possible and tell the dialogs themselves to
     *   close, so we have nothing left to do but finish processing
     *   application messages until we terminate.  
     */
    for (cur = download_head_ ; cur != 0 ; cur = cur->get_next_dlg())
        cur->set_cancel();
}


/* ------------------------------------------------------------------------ */
/*
 *   Process a game chest desktop URL drop 
 */
void CHtmlSysWin_win32_Input::drop_gch_url(IDataObject __RPC_FAR *dataobj,
                                           DWORD __RPC_FAR *effect)
{
    FORMATETC fmtetc;
    STGMEDIUM medium;

    /* get the data as an HGLOBAL, which will contain the text of the URL */
    fmtetc.cfFormat = (CLIPFORMAT)url_clipboard_fmt_;
    fmtetc.ptd = 0;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex = -1;
    fmtetc.tymed = TYMED_HGLOBAL;
    if (dataobj->GetData(&fmtetc, &medium) == S_OK)
    {
        textchar_t *buf;
        const char *p;
        int id;

        /* bring myself to the front for any user interaction we require */
        SetForegroundWindow(owner_->get_owner_frame_handle());

        /* get the text */
        buf = (textchar_t *)GlobalLock(medium.hGlobal);

        /*
         *   Check the file extension - if it's not one of our extensions,
         *   warn about it. 
         */
        p = strrchr(buf, '.');
        if (p == 0)
            p = buf + strlen(buf);
        else
            ++p;

        /* check the type */
        if (stricmp(p, "t3") == 0 || stricmp(p, "gam") == 0)
        {
            /* it's a game file, so it's definitely a go at this point */
            id = IDYES;
        }
        else if (stricmp(p, "zip") == 0
                 || stricmp(p, "z") == 0
                 || stricmp(p, "gz") == 0
                 || stricmp(p, "arc") == 0
                 || stricmp(p, "arj") == 0
                 || stricmp(p, "tar") == 0
                 || stricmp(p, "hqx") == 0
                 || stricmp(p, "lha") == 0
                 || stricmp(p, "lzh") == 0
                 || stricmp(p, "cab") == 0
                 || stricmp(p, "taz") == 0
                 || stricmp(p, "uu") == 0
                 || stricmp(p, "uue") == 0)
        {
            /* it's probably a compressed file */
            id = MessageBox(0, "Based on its name, this file appears to be "
                            "a compressed archive.  TADS can download the "
                            "file for you, but you will have to use a "
                            "third-party tool to unpack the archive before "
                            "you can use any story files it contains.  "
                            "Would you still like to download this file?",
                            "Download File",
                            MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL);
        }
        else
        {
            /* we don't recognize the extension */
            id = MessageBox(0, "This filename doesn't end in one of the "
                            "usual TADS story file suffixes (.gam or .t3), "
                            "so it's possible that this is not a TADS story "
                            "file.  Would you like to download this file "
                            "anyway?",
                            "Download File",
                            MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL);
        }

        /* if we're proceeding, kick off the download */
        if (id == IDYES)
        {
            OPENFILENAME info;
            char fname[256];

            /* set up the dialog definition structure */
            info.lStructSize = sizeof(info);
            info.hwndOwner = handle_;
            info.hInstance = CTadsApp::get_app()->get_instance();
            info.lpstrFilter = "TADS Stories\0*.gam;*.t3\0All Files\0*.*\0";
            info.lpstrCustomFilter = 0;
            info.nFilterIndex = 0;
            info.lpstrFile = fname;
            info.nMaxFile = sizeof(fname);
            info.lpstrFileTitle = 0;
            info.nMaxFileTitle = 0;
            info.lpstrTitle = "Select location for downloaded file";
            info.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR
                         | OFN_OVERWRITEPROMPT;
            info.nFileOffset = 0;
            info.nFileExtension = 0;
            info.lpstrDefExt = 0;
            info.lCustData = 0;
            info.lpfnHook = 0;
            info.lpTemplateName = 0;
            CTadsDialog::set_filedlg_center_hook(&info);
            
            /* start off in the default open-file directory */
            info.lpstrInitialDir =
                CTadsApp::get_app()->get_openfile_dir();

            /* use the root name of the URL as the proposed filename */
            strcpy(fname, os_get_root_name(buf));

            /* ask for a file */
            if (GetSaveFileName(&info))
            {
                CHtmlGameChestDownloadDlg *dlg;
                
                /* create the dialog */
                dlg = new CHtmlGameChestDownloadDlg(owner_, buf, fname);

                /* 
                 *   Open it modelessly; ignore the window handle, since we
                 *   just want to open the dialog and let it run.  Open the
                 *   dialog with no parent, since we want to let it run
                 *   effectively in the background.  
                 */
                dlg->open_modeless(DLG_DOWNLOAD, 0,
                                   CTadsApp::get_app()->get_instance());
            }
        }

        /* we're done with the hglobal - unlock and delete it */
        GlobalUnlock(medium.hGlobal);
        if (medium.pUnkForRelease != 0)
            medium.pUnkForRelease->Release();
        else
            GlobalFree(medium.hGlobal);

        /* 
         *   we successfully accepted the drop - we always treat a URL drop
         *   as a copy of the data
         */
        *effect = DROPEFFECT_LINK;
    }
    else
    {
        /* they couldn't give us the data we wanted - reject it */
        *effect = DROPEFFECT_NONE;
    }

    /* we're done with this operation */
    drag_gch_file_valid_ = FALSE;
}

