/* $Header: d:/cvsroot/tads/html/htmlstk.h,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlstk.h - stack implementation
Function
  
Notes
  
Modified
  08/26/97 MJRoberts  - Creation
*/

#ifndef HTMLSTK_H
#define HTMLSTK_H


/* ------------------------------------------------------------------------ */
/*
 *   Stack implementation 
 */
class CHtmlStackElement
{
    friend class CHtmlStack;
public:
    CHtmlStackElement(void *obj) { obj_ = obj; }

    /* get the item stored at this position */
    void *get_item() const { return obj_; }

    private:
        /* the object that this element contains */
        void *obj_;

        /* the next deeper element in the stack */
        CHtmlStackElement *nxt_;
};

class CHtmlStack
{
public:
    CHtmlStack() { top_ = 0; depth_ = 0; }

    /* push an object onto the stack */
    void push(void *obj)
    {
        push_ele(new CHtmlStackElement(obj));
        ++depth_;
    }

    /* get the item on top of the stack */
    void *get_top() const { return top_ ? top_->get_item() : 0; }

    /* get the depth of the stack */
    int get_depth() const { return depth_; }

    /* get the item N levels down (0 = top of stack) */
    void *get_item_n(int n) const;

    /* pop the top item off the stack */
    void *pop();

private:
    /* push an element onto the stack */
    void push_ele(CHtmlStackElement *ele)
    {
        ele->nxt_ = top_;
        top_ = ele;
    }

    /* top element on the stack */
    CHtmlStackElement *top_;

    /* depth of the stack */
    int depth_;
};


#endif /* HTMLSTK_H */

