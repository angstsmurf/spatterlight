/* $Header: d:/cvsroot/tads/html/htmlplst.h,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlplst.h - property list class
Function
  This class provides a list of properties.  Each property has a name,
  an ID number, and a value.  This mechanism can be used as a simple
  way of storing preferences and other settings.
Notes
  
Modified
  11/01/97 MJRoberts  - Creation
*/

#ifndef HTMLPLST_H
#define HTMLPLST_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Property object.  Each property has a name and a value.
 */

/* property type identifiers */
enum HTML_prop_type_t
{
    HTML_prop_type_null,
    HTML_prop_type_string,
    HTML_prop_type_rect,
    HTML_prop_type_color,
    HTML_prop_type_bool,
    HTML_prop_type_longint
};


class CHtmlProperty
{
public:
    CHtmlProperty(const textchar_t *nm)
    {
        /* store our name */
        name_.set(nm);

        /* we have no value yet */
        type_ = HTML_prop_type_null;

        /* properties are local and synchronized by default */
        is_global_ = FALSE;
        is_synced_ = TRUE;
    }

    ~CHtmlProperty()
    {
        free_value();
    }

    /* get/set the name */
    const textchar_t *get_name() const { return name_.get(); }
    void set_name(const textchar_t *newname) { name_.set(newname); }

    /* 
     *   Get/set the 'global' status.  A property can be marked local or
     *   global; a local property might vary according to different user
     *   profiles, for example, while a global property is the same in all
     *   profiles.  By default, everything is local.  
     */
    int is_global() const { return is_global_; }
    void set_global(int g) { is_global_ = g; }

    /*
     *   Get/set the 'synchronized' status.  By default, properties are all
     *   marked as synchronized, which means that implementations in a
     *   multi-tasking system should always check for changes and reload
     *   properties saved persistently with different settings than in
     *   memory.  This ensures that if two instances of the application are
     *   running, and the user makes changes to the settings in the
     *   foreground instance, the background instance will be brought up to
     *   date with the changes as soon as it's reactivated.
     *   
     *   Some properties are not suitable for synchronizing, and should
     *   instead use a 'last change wins' strategy.  In this approach,
     *   multiple instances shouldn't try to keep in sync with one another on
     *   these properties; they should instead ignore changes made in the
     *   persistent store while the app is running, and just blithely save
     *   their changes over any other changes previously made.  For example,
     *   window placement properties should usually be unsynchronized, so
     *   that the last instance of the app that was closed determines the
     *   placement of the next instance opened.  
     */
    int is_synchronized() const { return is_synced_; }
    void set_synchronized(int f) { is_synced_ = f; }

    /* 
     *   Get the datatype of the value, and the value.  The get-value
     *   routines do not perform type conversions, so the value must be
     *   retrieved using the same datatype that was originally used to
     *   store the value. 
     */
    HTML_prop_type_t get_type() const { return type_; }
    const textchar_t *get_val_str() const { return value_.str_->get(); }
    const CHtmlRect  *get_val_rect() const { return value_.rect_; }
    HTML_color_t get_val_color() const { return value_.color_; }
    int get_val_bool() const { return value_.bool_; }
    long get_val_longint() const { return value_.longint_; }

    /*
     *   Set the value.  The new value need not be of the same type as the
     *   previous value.  
     */
    void set_val_str(const textchar_t *str)
    {
        set_val_str(str, (str != 0 ? get_strlen(str) : 0));
    }
    
    void set_val_str(const textchar_t *str, size_t len)
    {
        change_type(HTML_prop_type_string);
        value_.str_->set(str, len);
    }

    void set_val_color(HTML_color_t color)
    {
        change_type(HTML_prop_type_color);
        value_.color_ = color;
    }
    
    void set_val_rect(const CHtmlRect *rect)
    {
        change_type(HTML_prop_type_rect);
        value_.rect_->set(rect);
    }
    
    void set_val_bool(int val)
    {
        change_type(HTML_prop_type_bool);
        value_.bool_ = val;
    }
    
    void set_val_longint(long val)
    {
        change_type(HTML_prop_type_longint);
        value_.longint_ = val;
    }

    /*
     *   Generate a string representation of the value.  Converts the
     *   value to a string representation suitable for conversion back to
     *   the original type with a parse() routine.  This routine can be
     *   used to serialize the value.  The resulting string is
     *   null-terminated.  Returns the length of the string, not including
     *   the null terminator.  If the result won't fit in the buffer,
     *   returns the length required, but does not put the result in the
     *   buffer.  
     */
    size_t gen_str_rep(textchar_t *buf, size_t bufsiz);

    /*
     *   Parse the value from a string.  Parses a string previously
     *   generated by gen_str_rep() and stores it as the appropriate type. 
     */
    void parse_str_rep(const textchar_t *str, size_t len,
                       HTML_prop_type_t datatype);

    /*
     *   Parse the value from a string, saving the value with the same
     *   datatype as the current property value 
     */
    void parse_str_rep(const textchar_t *str, size_t len)
        { parse_str_rep(str, len, get_type()); }


private:
    /* free any storage used by the current value */
    void free_value();

    /* 
     *   set the datatype, freeing any storage from a previous datatype
     *   and allocating storage for the new datatype as needed 
     */
    void change_type(HTML_prop_type_t new_type);

    /* service routine to parse a long int value */
    long parse_longint(const textchar_t **str, size_t *len);
    
    /* name of the property - this can be used for serialization purposes */
    CStringBuf name_;

    /* datatype of the value */
    HTML_prop_type_t type_;

    /* value */
    union
    {
        CStringBuf   *str_;
        CHtmlRect    *rect_;
        HTML_color_t  color_;
        int           bool_;
        long          longint_;
    } value_;

    /* flag: property is global */
    unsigned int is_global_ : 1;

    /* flag: property is synchronized */
    unsigned int is_synced_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   Property list.  The property list is simply an array of properties.
 *   The client identifies properties by index into the property list
 *   array.  
 */

class CHtmlPropList
{
public:
    CHtmlPropList()
    {
        /* nothing allocated initially */
        props_ = 0;
        props_alloced_ = 0;
    }
    
    ~CHtmlPropList();

    /* 
     *   Get the property at the given index.  If no property exists at
     *   this index yet, we'll create a new one. 
     */
    CHtmlProperty *get_prop(int index);

    /*
     *   Initialize a property to a given name 
     */
    void set_prop_name(int index, const textchar_t *nm)
        { get_prop(index)->set_name(nm); }

    /*
     *   Get the value of a property 
     */
    const textchar_t *get_val_str(int id)
        { return get_prop(id)->get_val_str(); }
    const CHtmlRect *get_val_rect(int id)
        { return get_prop(id)->get_val_rect(); }
    HTML_color_t get_val_color(int id)
        { return get_prop(id)->get_val_color(); }
    long get_val_longint(int id)
        { return get_prop(id)->get_val_longint(); }
    int get_val_bool(int id)
        { return get_prop(id)->get_val_bool(); }

    /*
     *   Set the value of a property 
     */
    void set_val_str(int id, const textchar_t *str)
        { get_prop(id)->set_val_str(str); }
    void set_val_str(int id, const textchar_t *str, size_t len)
        { get_prop(id)->set_val_str(str, len); }
    void set_val_rect(int id, const CHtmlRect *rect)
        { get_prop(id)->set_val_rect(rect); }
    void set_val_color(int id, HTML_color_t color)
        { get_prop(id)->set_val_color(color); }
    void set_val_longint(int id, long val)
        { get_prop(id)->set_val_longint(val); }
    void set_val_bool(int id, int val)
        { get_prop(id)->set_val_bool(val); }


private:
    /* property array */
    CHtmlProperty **props_;

    /* size of array allocated */
    size_t props_alloced_;
};


#endif /* HTMLPLST_H */

