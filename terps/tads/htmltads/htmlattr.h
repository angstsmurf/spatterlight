/* $Header: d:/cvsroot/tads/html/htmlattr.h,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlattr.h - HTML attributes
Function
  
Notes
  
Modified
  09/01/97 MJRoberts  - Creation
*/

#ifndef HTMLATTR_H
#define HTMLATTR_H

/*
 *   Attribute ID's.  Each valid attribute name has an entry in this
 *   table.  Note that the attribute ID's are global, so this table
 *   contains the union of all of the valid attributes from all of the
 *   tags.  In addition, this table contains the valid value names for
 *   those attribute values that use enumerated value names; for example,
 *   the ALIGN attribute can be set to values such as LEFT, RIGHT, and
 *   CENTER, so these names use these ID's as well.  It is always clear
 *   from context what a particular identifier means, so we put all of the
 *   identifier names into one big table for simplicity.  
 */
enum HTML_Attrib_id_t
{
    /* special value for invalid or unknown attribute */
    HTML_Attrib_invalid,

    HTML_Attrib_background,
    HTML_Attrib_bgcolor,
    HTML_Attrib_text,
    HTML_Attrib_link,
    HTML_Attrib_vlink,
    HTML_Attrib_alink,
    HTML_Attrib_align,
    HTML_Attrib_compact,
    HTML_Attrib_type,
    HTML_Attrib_start,
    HTML_Attrib_value,
    HTML_Attrib_width,
    HTML_Attrib_noshade,
    HTML_Attrib_size,
    HTML_Attrib_valign,
    HTML_Attrib_border,
    HTML_Attrib_cellspacing,
    HTML_Attrib_cellpadding,
    HTML_Attrib_nowrap,
    HTML_Attrib_rowspan,
    HTML_Attrib_colspan,
    HTML_Attrib_height,
    HTML_Attrib_name,
    HTML_Attrib_checked,
    HTML_Attrib_maxlength,
    HTML_Attrib_src,
    HTML_Attrib_multiple,
    HTML_Attrib_selected,
    HTML_Attrib_rows,
    HTML_Attrib_cols,
    HTML_Attrib_href,
    HTML_Attrib_rel,
    HTML_Attrib_rev,
    HTML_Attrib_title,
    HTML_Attrib_alt,
    HTML_Attrib_hspace,
    HTML_Attrib_vspace,
    HTML_Attrib_usemap,
    HTML_Attrib_ismap,
    HTML_Attrib_codebase,
    HTML_Attrib_code,
    HTML_Attrib_face,
    HTML_Attrib_color,
    HTML_Attrib_clear,
    HTML_Attrib_shape,
    HTML_Attrib_coords,
    HTML_Attrib_nohref,
    HTML_Attrib_black,
    HTML_Attrib_silver,
    HTML_Attrib_gray,
    HTML_Attrib_white,
    HTML_Attrib_maroon,
    HTML_Attrib_red,
    HTML_Attrib_purple,
    HTML_Attrib_fuchsia,
    HTML_Attrib_green,
    HTML_Attrib_lime,
    HTML_Attrib_olive,
    HTML_Attrib_yellow,
    HTML_Attrib_navy,
    HTML_Attrib_blue,
    HTML_Attrib_teal,
    HTML_Attrib_aqua,
    HTML_Attrib_left,
    HTML_Attrib_right,
    HTML_Attrib_center,
    HTML_Attrib_disc,
    HTML_Attrib_square,
    HTML_Attrib_circle,
    HTML_Attrib_top,
    HTML_Attrib_middle,
    HTML_Attrib_bottom,
    HTML_Attrib_password,
    HTML_Attrib_checkbox,
    HTML_Attrib_radio,
    HTML_Attrib_submit,
    HTML_Attrib_reset,
    HTML_Attrib_file,
    HTML_Attrib_hidden,
    HTML_Attrib_image,
    HTML_Attrib_all,
    HTML_Attrib_rect,
    HTML_Attrib_poly,
    HTML_Attrib_justify,
    HTML_Attrib_id,
    HTML_Attrib_previous,
    HTML_Attrib_remove,
    HTML_Attrib_to,
    HTML_Attrib_indent,
    HTML_Attrib_dp,
    HTML_Attrib_decimal,
    HTML_Attrib_plain,
    HTML_Attrib_continue,
    HTML_Attrib_seqnum,
    HTML_Attrib_layer,
    HTML_Attrib_foreground,
    HTML_Attrib_ambient,
    HTML_Attrib_cancel,
    HTML_Attrib_repeat,
    HTML_Attrib_loop,
    HTML_Attrib_random,
    HTML_Attrib_fadein,
    HTML_Attrib_fadeout,
    HTML_Attrib_interrupt,
    HTML_Attrib_sequence,
    HTML_Attrib_replace,
    HTML_Attrib_cycle,
    HTML_Attrib_bgambient,
    HTML_Attrib_statusbg,
    HTML_Attrib_statustext,
    HTML_Attrib_removeall,
    HTML_Attrib_noenter,
    HTML_Attrib_append,
    HTML_Attrib_asrc,
    HTML_Attrib_hsrc,
    HTML_Attrib_hlink,
    HTML_Attrib_char,
    HTML_Attrib_word,
    HTML_Attrib_forced,
    HTML_Attrib_hover
};

#endif /* HTMLATTR_H */
