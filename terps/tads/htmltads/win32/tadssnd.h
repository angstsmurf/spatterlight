/* $Header: d:/cvsroot/tads/html/win32/tadssnd.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadssnd.h - TADS sound object
Function
  
Notes
  
Modified
  01/10/98 MJRoberts  - Creation
*/

#ifndef TADSSND_H
#define TADSSND_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif


/*------------------------------------------------------------------------ */
/*
 *   Audio Controller - provides global control over certain aspects of sound
 *   playback.  This is an abstract interface that must be provided to sound
 *   objects when they're created.  
 */
class CTadsAudioControl
{
public:
    /* reference control */
    virtual void audioctl_add_ref() = 0;
    virtual void audioctl_release() = 0;

    /* get the current muting status */
    virtual int get_mute_sound() = 0;

    /* 
     *   Register/unregister a playing sound.  When the controller needs to
     *   change a global playback parameter globally (such as the muting
     *   status), it'll inform each registered sound object of the change.  
     */
    virtual void register_active_sound(class CTadsAudioPlayer *sound) = 0;
    virtual void unregister_active_sound(class CTadsAudioPlayer *sound) = 0;
};

/*
 *   The generic sound player interface.  System sound player objects can
 *   register themselves with the audio controller to receive notifications
 *   of changes to global system sound settings.  
 */
class CTadsAudioPlayer
{
public:
    /* receive notification of a change in muting status */
    virtual void on_mute_change(int mute) = 0;
};

/* ------------------------------------------------------------------------ */
/*
 *   Base windows-specific sound object 
 */
class CTadsSound
{
public:
    CTadsSound()
    {
        seek_pos_ = 0;
        data_size_ = 0;
    }
    ~CTadsSound();

    /* load from a sound resource loader */
    int load_from_res(class CHtmlSound *loader);

protected:
    /* file information */
    CStringBuf fname_;
    unsigned long seek_pos_;
    unsigned long data_size_;
};


#endif /* TADSSND_H */

