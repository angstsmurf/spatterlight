#ifndef GLK_H
#define GLK_H

/* glk.h: Header file for Glk API, version 0.7.5.
 Designed by Andrew Plotkin <erkyrath@eblong.com>
 http://eblong.com/zarf/glk/
 
 This file is copyright 1998-2017 by Andrew Plotkin. It is
 distributed under the MIT license; see the "LICENSE" file.
 */

/* If your system does not have <stdint.h>, you'll have to remove this
 include line. Then edit the definition of glui32 to make sure it's
 really a 32-bit unsigned integer type, and glsi32 to make sure
 it's really a 32-bit signed integer type. If they're not, horrible
 things will happen. */
#include <stdint.h>
typedef uint32_t glui32;
typedef int32_t glsi32;

/* These are the compile-time conditionals that reveal various Glk optional
 modules. Note that if GLK_MODULE_SOUND2 is defined, GLK_MODULE_SOUND
 must be also. */
#define GLK_MODULE_LINE_ECHO
#define GLK_MODULE_LINE_TERMINATORS
#define GLK_MODULE_UNICODE
#define GLK_MODULE_UNICODE_NORM
#define GLK_MODULE_IMAGE
#define GLK_MODULE_SOUND
#define GLK_MODULE_SOUND2
#define GLK_MODULE_HYPERLINKS
#define GLK_MODULE_DATETIME
#define GLK_MODULE_RESOURCE_STREAM
#define GLK_MODULE_GARGLKTEXT

/* Define a macro for a function attribute that indicates a function that
 never returns. (E.g., glk_exit().) We try to do this only in C compilers
 that support it. If this is causing you problems, comment all this out
 and simply "#define GLK_ATTRIBUTE_NORETURN". */
#if defined(__GNUC__) || defined(__clang__)
#define GLK_ATTRIBUTE_NORETURN __attribute__((__noreturn__))
#endif /* defined(__GNUC__) || defined(__clang__) */
#ifndef GLK_ATTRIBUTE_NORETURN
#define GLK_ATTRIBUTE_NORETURN
#endif /* GLK_ATTRIBUTE_NORETURN */

#define gestalt_Version (0)
#define gestalt_CharInput (1)
#define gestalt_LineInput (2)
#define gestalt_CharOutput (3)
#define gestalt_CharOutput_CannotPrint (0)
#define gestalt_CharOutput_ApproxPrint (1)
#define gestalt_CharOutput_ExactPrint (2)
#define gestalt_MouseInput (4)
#define gestalt_Timer (5)
#define gestalt_Graphics (6)
#define gestalt_DrawImage (7)
#define gestalt_Sound (8)
#define gestalt_SoundVolume (9)
#define gestalt_SoundNotify (10)
#define gestalt_Hyperlinks (11)
#define gestalt_HyperlinkInput (12)
#define gestalt_SoundMusic (13)
#define gestalt_GraphicsTransparency (14)
#define gestalt_Unicode (15)
#define gestalt_UnicodeNorm (16)
#define gestalt_LineInputEcho (17)
#define gestalt_LineTerminators (18)
#define gestalt_LineTerminatorKey (19)
#define gestalt_DateTime (20)
#define gestalt_Sound2 (21)
#define gestalt_ResourceStream (22)
#define gestalt_GraphicsCharInput (23)
#define gestalt_GarglkText (0x1100)

#define evtype_None (0)
#define evtype_Timer (1)
#define evtype_CharInput (2)
#define evtype_LineInput (3)
#define evtype_MouseInput (4)
#define evtype_Arrange (5)
#define evtype_Redraw (6)
#define evtype_SoundNotify (7)
#define evtype_Hyperlink (8)
#define evtype_VolumeNotify (9)

#define keycode_Unknown  (0xffffffff)
#define keycode_Left     (0xfffffffe)
#define keycode_Right    (0xfffffffd)
#define keycode_Up       (0xfffffffc)
#define keycode_Down     (0xfffffffb)
#define keycode_Return   (0xfffffffa)
#define keycode_Delete   (0xfffffff9)
#define keycode_Escape   (0xfffffff8)
#define keycode_Tab      (0xfffffff7)
#define keycode_PageUp   (0xfffffff6)
#define keycode_PageDown (0xfffffff5)
#define keycode_Home     (0xfffffff4)
#define keycode_End      (0xfffffff3)
#define keycode_Func1    (0xffffffef)
#define keycode_Func2    (0xffffffee)
#define keycode_Func3    (0xffffffed)
#define keycode_Func4    (0xffffffec)
#define keycode_Func5    (0xffffffeb)
#define keycode_Func6    (0xffffffea)
#define keycode_Func7    (0xffffffe9)
#define keycode_Func8    (0xffffffe8)
#define keycode_Func9    (0xffffffe7)
#define keycode_Func10   (0xffffffe6)
#define keycode_Func11   (0xffffffe5)
#define keycode_Func12   (0xffffffe4)
/* The last keycode is always (0x100000000 - keycode_MAXVAL) */
#define keycode_MAXVAL   (28)

#define style_Normal (0)
#define style_Emphasized (1)
#define style_Preformatted (2)
#define style_Header (3)
#define style_Subheader (4)
#define style_Alert (5)
#define style_Note (6)
#define style_BlockQuote (7)
#define style_Input (8)
#define style_User1 (9)
#define style_User2 (10)
#define style_NUMSTYLES (11)

#define wintype_AllTypes (0)
#define wintype_Pair (1)
#define wintype_Blank (2)
#define wintype_TextBuffer (3)
#define wintype_TextGrid (4)
#define wintype_Graphics (5)

#define winmethod_Left  (0x00)
#define winmethod_Right (0x01)
#define winmethod_Above (0x02)
#define winmethod_Below (0x03)
#define winmethod_DirMask (0x0f)

#define winmethod_Fixed (0x10)
#define winmethod_Proportional (0x20)
#define winmethod_DivisionMask (0xf0)

#define winmethod_Border   (0x000)
#define winmethod_NoBorder (0x100)
#define winmethod_BorderMask (0x100)

#define fileusage_Data (0x00)
#define fileusage_SavedGame (0x01)
#define fileusage_Transcript (0x02)
#define fileusage_InputRecord (0x03)
#define fileusage_TypeMask (0x0f)

#define fileusage_TextMode   (0x100)
#define fileusage_BinaryMode (0x000)

#define filemode_Write (0x01)
#define filemode_Read (0x02)
#define filemode_ReadWrite (0x03)
#define filemode_WriteAppend (0x05)

#define seekmode_Start (0)
#define seekmode_Current (1)
#define seekmode_End (2)

#define stylehint_Indentation (0)
#define stylehint_ParaIndentation (1)
#define stylehint_Justification (2)
#define stylehint_Size (3)
#define stylehint_Weight (4)
#define stylehint_Oblique (5)
#define stylehint_Proportional (6)
#define stylehint_TextColor (7)
#define stylehint_BackColor (8)
#define stylehint_ReverseColor (9)
#define stylehint_NUMHINTS (10)

#define   stylehint_just_LeftFlush (0)
#define   stylehint_just_LeftRight (1)
#define   stylehint_just_Centered (2)
#define   stylehint_just_RightFlush (3)

#endif /* GLK_H */
