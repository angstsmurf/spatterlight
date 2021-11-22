/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsstat.h - Windows status line object
Function
  The status line object uses a Windows status line control to place a
  status line at the bottom of a window, optionally with a size-box.

  Ownership and deletion: the parent window must delete this object when
  the parent window is deleted.

  Notifications: the parent window must pass certain notifications to the
  status line object.  For example, when the parent window is resized, it
  must call the status line object so that it can resposition itself
  correctly for the new parent window size.

  Setting the status text:

  The status line object is designed to be shared by several objects, and
  so that the objects sharing the status line need not know about one another
  or coordinate their activities.  To accomplish this, we define an
  interface, CTadsStatusSource, that each object wishing to provide status
  messages must implement.

  At initialization time, each object that will share the status line must
  call register_source() in the status line, passing a pointer to its
  CTadsStatusSource interface.  It must also keep a pointer to the status
  line.  The status line object keeps a list of the registered sources.
  Whenever the status line needs to update its display, it goes through its
  list of sources, asking each source for the current message; if a source
  provides a message, that message is used, otherwise the status line asks
  the next source, continuing until a source provides a message or all
  sources have been asked.  Initially, the sources are ordered in reverse of
  the registration order -- the last source registered is the first source
  called.  However, at any time a source can bring itself to the front of the
  list.

  When a source begins an operation that requires a temporary status line
  message (such as to note that a time-consuming operation is in progress),
  it should bring itself to the front of the list, and note internally the
  operation.  The status line will then run through the list to find the new
  message; the source at the front of the list, because the operation is in
  progress, will report its message.  When the operation is finished, the
  source notes that the operation is finished, and tells the status line to
  update its message; the status line once again looks through the sources,
  but this time the first item (since it's done with its operation) doesn't
  provide a message, hence the message reverts to the one being displayed
  previously.

  When the status of an operation changes (for example, the operation is
  completed, or moves on to a new stage of the same operation), the source
  should simply call the status line's update() routine; this will simply
  cause the status line to ask the sources for a new message.
Notes
  
Modified
  10/26/97 MJRoberts  - Creation
*/

#ifndef TADSSTAT_H
#define TADSSTAT_H

#include <windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

class CTadsStatusline
{
public:
    /* 
     *   Create the status line within a given parent window.  If sizebox
     *   is true, we'll create the status line with a sizing box at the
     *   right corner.  id specifies the control ID for the status line
     *   control within the parent window. 
     */
    CTadsStatusline(class CTadsWin *parent, int sizebox, int id);

    /* delete the status line */
    ~CTadsStatusline();

    /* get the handle of the status line's control window */
    HWND get_handle() { return handle_; }

    /* register a new status source */
    void register_source(class CTadsStatusSource *source);

    /* unregister a source */
    void unregister_source(class CTadsStatusSource *source);

    /* ----------------------------------------------------------------- */
    /*
     *   Update operations.  When a source has a new status message to
     *   display, it should call one of these routines.  When one of these
     *   routines is called, we'll go through the list of status sources
     *   to get a message, and display the first message we find.  
     */

    /* 
     *   Update the status line, using existing source list ordering.  A
     *   source should call this when the state of an existing operation
     *   changes (for example, the operation is finished, or a new stage
     *   of the operation begins). 
     */
    void update();

    /* 
     *   Bring a source to the front of the status line, and update the
     *   message.  A source should call this when it begins a new
     *   "foreground" operation, which will run for a while and eventually
     *   complete.  
     */
    void source_to_front(class CTadsStatusSource *source);
    

    /* ----------------------------------------------------------------- */
    /*
     *   Notifications - the parent window must call these routines to
     *   notify the status line object of certain events. 
     */

    /* notify status line that the parent window was resized */
    void notify_parent_resize();

private:
    /* 
     *   Remove a source from the list, returning a pointer to the list
     *   item container for the source item.  This doesn't delete
     *   anything; it just unlinks it from the list.  
     */
    class CTadsStatusSourceListitem *unlink(class CTadsStatusSource *source);

    /* statusline control window handle */
    HWND handle_;

    /* status source list */
    class CTadsStatusSourceListitem *sources_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Status source.  Objects that provide status messages must implement
 *   this interface. 
 */
class CTadsStatusSource
{
public:
    virtual ~CTadsStatusSource() { }

    /*
     *   Get the current message for this source.  If the source does not
     *   currently have anything to report, it should return null.  If the
     *   character array returned was allocated, this routine must set
     *   *caller_deletes to true; in this case, the caller (i.e., the
     *   status line object) will use "delete []" to free the memory
     *   returned.  
     */
    virtual textchar_t *get_status_message(int *caller_deletes) = 0;
};

/* ------------------------------------------------------------------------ */
/*
 *   Status source container.  The status line uses this class to build a
 *   list of status source objects. 
 */
class CTadsStatusSourceListitem
{
public:
    CTadsStatusSourceListitem(CTadsStatusSource *item)
    {
        item_ = item;
        nxt_ = 0;
    }

    /* source at this item */
    CTadsStatusSource *item_;
    
    /* next item in the list */
    CTadsStatusSourceListitem *nxt_;
};

#endif /* TADSSTAT_H */

