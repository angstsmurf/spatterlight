#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlplst.cpp,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlplst.cpp - property list implementation
Function
  
Notes
  
Modified
  11/01/97 MJRoberts  - Creation
*/

#include <string.h>
#include <stdio.h>

#ifndef HTMLPLST_H
#include "htmlplst.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Property object implementation
 */

/*
 *   release storage used by the current value 
 */
void CHtmlProperty::free_value()
{
    switch(type_)
    {
    case HTML_prop_type_string:
        /* delete the string buffer */
        if (value_.str_ != 0)
        {
            delete value_.str_;
            value_.str_ = 0;
        }
        break;

    case HTML_prop_type_rect:
        /* delete the rectangle object */
        if (value_.rect_ != 0)
        {
            delete value_.rect_;
            value_.rect_ = 0;
        }
        break;

    default:
        /* other types do not use additional storage */
        break;
    }

    /* we have no type stored now */
    type_ = HTML_prop_type_null;
}

/* 
 *   set the datatype, freeing any storage from a previous datatype and
 *   allocating storage for the new datatype as needed 
 */
void CHtmlProperty::change_type(HTML_prop_type_t new_type)
{
    /* if the type is staying the same, there's nothing to do */
    if (type_ == new_type)
        return;

    /* release any storage used by the old value */
    free_value();

    /* allocate storage for the new type */
    switch(new_type)
    {
    case HTML_prop_type_string:
        /* allocate a string buffer to hold the value */
        value_.str_ = new CStringBuf();
        break;

    case HTML_prop_type_rect:
        /* allocate a rectangle object to hold the value */
        value_.rect_ = new CHtmlRect();
        break;

    default:
        /* other types require no additional storage */
        break;
    }

    /* set the new type */
    type_ = new_type;
}

/*
 *   Generate a string representation of the value 
 */
size_t CHtmlProperty::gen_str_rep(textchar_t *buf, size_t bufsiz)
{
    switch(type_)
    {
    case HTML_prop_type_string:
        /* no conversion needed for strings */
        if (value_.str_->get() == 0)
        {
            /* empty string */
            buf[0] = '\0';
            return 0;
        }
        else
        {
            size_t len;
            
            /* make sure we have room */
            len = get_strlen(value_.str_->get());
            if (len + 1 > bufsiz)
            {
                /* not enough room - return required size */
                return len + 1;
            }
            else
            {
                /* return the string */
                memcpy(buf, value_.str_->get(), (len+1) * sizeof(textchar_t));
                return len;
            }
        }

    case HTML_prop_type_rect:
        /* need space for four numbers, 8 digits each, plus separators */
        if (bufsiz < 41)
            return 41;

        /* generate the value */
        sprintf(buf, "%ld, %ld, %ld, %ld",
                value_.rect_->left, value_.rect_->top,
                value_.rect_->right, value_.rect_->bottom);
        return get_strlen(buf);

    case HTML_prop_type_color:
        /* need space for three numbers, 3 digits each, plus separators */
        if (bufsiz < 16)
            return 16;

        /* generate the value */
        sprintf(buf, "%d, %d, %d",
                HTML_color_red(value_.color_),
                HTML_color_green(value_.color_),
                HTML_color_blue(value_.color_));
        return get_strlen(buf);

    case HTML_prop_type_bool:
        {
            size_t len;
            const textchar_t *val;
            
            /* need space for "yes" or "no" */
            if (bufsiz < 4)
                return 4;
            
            /* generate the value */
            val = (value_.bool_ != 0 ? "yes" : "no");
            len = get_strlen(val);
            memcpy(buf, val, (len+1)*sizeof(textchar_t));
            return len;
        }

    case HTML_prop_type_longint:
        /* need space for up to 8 digits */
        if (bufsiz < 9)
            return 9;

        /* generate the value */
        sprintf(buf, "%ld", value_.longint_);
        return get_strlen(buf);

    default:
        /* unknown type */
        return 0;
    }
}

/*
 *   Parse a string representation into a vlaue 
 */
void CHtmlProperty::parse_str_rep(const textchar_t *str, size_t len,
                                  HTML_prop_type_t new_type)
{
    /* set up to store this type */
    change_type(new_type);

    /* see what we have */
    switch(new_type)
    {
    case HTML_prop_type_string:
        /* copy the string value */
        value_.str_->set(str, len);
        break;

    case HTML_prop_type_rect:
        /* parse the four coordinate values */
        value_.rect_->left = parse_longint(&str, &len);
        value_.rect_->top = parse_longint(&str, &len);
        value_.rect_->right = parse_longint(&str, &len);
        value_.rect_->bottom = parse_longint(&str, &len);
        break;

    case HTML_prop_type_color:
        {
            int r, g, b;
            
            /* parse the three color values */
            r = (int)parse_longint(&str, &len);
            g = (int)parse_longint(&str, &len);
            b = (int)parse_longint(&str, &len);
            value_.color_ = HTML_make_color(r, g, b);
        }
        break;

    case HTML_prop_type_bool:
        /* if it starts with 'y' or 'Y', it's true, otherwise false */
        value_.bool_ = (len > 0 && (str[0] == 'y' || str[0] == 'Y'));
        break;

    case HTML_prop_type_longint:
        /* parse the long int value */
        value_.longint_ = parse_longint(&str, &len);
        break;

    case HTML_prop_type_null:
        /* null value - this has no string representation */
        break;
    }
}

/*
 *   Parse a long integer value, leaving the string pointer positioned to
 *   the next value. 
 */
long CHtmlProperty::parse_longint(const textchar_t **str, size_t *len)
{
    const textchar_t *start;
    textchar_t buf[128];
    size_t numlen;
    long val;
    
    /* 
     *   find the end of this number - it ends at the end of the string,
     *   or at a comma 
     */
    for (start = *str ; *len != 0 && **str != ',' ; ++(*str), --(*len)) ;

    /* copy the string into our buffer and null-terminate it */
    numlen = *str - start;
    if (numlen > sizeof(buf) - 1) numlen = sizeof(buf) - 1;
    memcpy(buf, start, numlen * sizeof(textchar_t));
    buf[numlen] = '\0';

    /* parse the number */
    val = get_atol(buf);

    /* skip the comma and any following whitespace */
    if (*len != 0 && **str == ',')
    {
        /* skip the comma */
        ++(*str), --(*len);

        /* skip whitespace following the comma */
        while (*len != 0 && is_space(**str))
            ++(*str), --(*len);
    }

    /* return the value */
    return val;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property list implementation 
 */

CHtmlPropList::~CHtmlPropList()
{
    /* delete the array, if we allocated it */
    if (props_ != 0)
    {
        CHtmlProperty **p;
        size_t i;

        /* go through the array and delete all allocated property objects */
        for (p = props_, i = 0 ; i < props_alloced_ ; ++p, ++i)
        {
            /* if this property is allocated, delete it */
            if (*p != 0)
                delete *p;
        }
        
        /* delete the array of properties */
        th_free(props_);
    }
}


/*
 *   Get the property at a given index 
 */
CHtmlProperty *CHtmlPropList::get_prop(int index)
{
    /* if the array isn't allocated to this point yet, expand it */
    if ((size_t)index >= props_alloced_)
    {
        size_t new_size;
        
        /* allocate to next multiple of 16 */
        new_size = ((index/16) + 1) * 16;

        /* allocate or reallocate the array */
        if (props_ == 0)
            props_ = (CHtmlProperty **)th_malloc(new_size * sizeof(*props_));
        else
            props_ = (CHtmlProperty **)
                     th_realloc(props_, new_size * sizeof(*props_));

        /* clear the newly allocated properties */
        memset(props_ + props_alloced_, 0,
               (new_size - props_alloced_) * sizeof(*props_));

        /* remember the new size */
        props_alloced_ = new_size;
    }

    /* if we don't have a property in this slot yet, create one */
    if (props_[index] == 0)
        props_[index] = new CHtmlProperty("");

    /* return the property in this slot */
    return props_[index];
}

