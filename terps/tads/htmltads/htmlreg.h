/* $Header: d:/cvsroot/tads/html/htmlreg.h,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlreg.h - HTML class registry
Function
  Defines a "registry" of classes used by the HTML parser.  A class is
  registered here by mentioning the class in an appropriate key macro.
  Functions that wish to enumerate classes registered under a particular
  key define the macro for that key then include this file.
Notes
  This file can be safely included multiple times, because each inclusion
  merely provides an enumeration of classes registered under the key or
  keys selected immediately prior to the inclusion.
Modified
  08/26/97 MJRoberts  - Creation
*/

#ifndef HTMLREG_H
#define HTMLREG_H


#endif /* HTMLREG_H */

/*
 *   This portion of the file is intentionally unprotected against
 *   multiple inclusion, because multiple inclusion of this section is
 *   valid. 
 */

/* ------------------------------------------------------------------------ */
/*
 *   Tag classes.  Each class that implements a named tag should be
 *   registered here. 
 */

/* if the key macro isn't defined by the includer, don't generate these keys */
#ifndef HTML_REG_TAG
# define HTML_REG_TAG(cls)
#endif

/* block structuring elements */
HTML_REG_TAG(CHtmlTagBODY)
HTML_REG_TAG(CHtmlTagTITLE)
HTML_REG_TAG(CHtmlTagH1)
HTML_REG_TAG(CHtmlTagH2)
HTML_REG_TAG(CHtmlTagH3)
HTML_REG_TAG(CHtmlTagH4)
HTML_REG_TAG(CHtmlTagH5)
HTML_REG_TAG(CHtmlTagH6)
HTML_REG_TAG(CHtmlTagP)
HTML_REG_TAG(CHtmlTagPRE)
HTML_REG_TAG(CHtmlTagLISTING)
HTML_REG_TAG(CHtmlTagXMP)
HTML_REG_TAG(CHtmlTagADDRESS)
HTML_REG_TAG(CHtmlTagBLOCKQUOTE)
HTML_REG_TAG(CHtmlTagBQ)
HTML_REG_TAG(CHtmlTagCREDIT)
HTML_REG_TAG(CHtmlTagDIV)
HTML_REG_TAG(CHtmlTagCENTER)

/* list elements */
HTML_REG_TAG(CHtmlTagUL)
HTML_REG_TAG(CHtmlTagOL)
HTML_REG_TAG(CHtmlTagDIR)
HTML_REG_TAG(CHtmlTagMENU)
HTML_REG_TAG(CHtmlTagLI)
HTML_REG_TAG(CHtmlTagDL)
HTML_REG_TAG(CHtmlTagDT)
HTML_REG_TAG(CHtmlTagDD)
HTML_REG_TAG(CHtmlTagLH)

/* phrase markup */
HTML_REG_TAG(CHtmlTagCITE)
HTML_REG_TAG(CHtmlTagCODE)
HTML_REG_TAG(CHtmlTagEM)
HTML_REG_TAG(CHtmlTagKBD)
HTML_REG_TAG(CHtmlTagSAMP)
HTML_REG_TAG(CHtmlTagSTRONG)
HTML_REG_TAG(CHtmlTagVAR)
HTML_REG_TAG(CHtmlTagB)
HTML_REG_TAG(CHtmlTagI)
HTML_REG_TAG(CHtmlTagU)
HTML_REG_TAG(CHtmlTagSTRIKE)
HTML_REG_TAG(CHtmlTagS)
HTML_REG_TAG(CHtmlTagTT)
HTML_REG_TAG(CHtmlTagA)
HTML_REG_TAG(CHtmlTagBR)
HTML_REG_TAG(CHtmlTagHR)
HTML_REG_TAG(CHtmlTagIMG)
HTML_REG_TAG(CHtmlTagFONT)
HTML_REG_TAG(CHtmlTagBASEFONT)
HTML_REG_TAG(CHtmlTagBIG)
HTML_REG_TAG(CHtmlTagSMALL)
HTML_REG_TAG(CHtmlTagDFN)
HTML_REG_TAG(CHtmlTagSUP)
HTML_REG_TAG(CHtmlTagSUB)
HTML_REG_TAG(CHtmlTagNOBR)
HTML_REG_TAG(CHtmlTagWRAP)
HTML_REG_TAG(CHtmlTagBANNER)
HTML_REG_TAG(CHtmlTagABOUTBOX)
HTML_REG_TAG(CHtmlTagTAB)
HTML_REG_TAG(CHtmlTagMAP)
HTML_REG_TAG(CHtmlTagAREA)
HTML_REG_TAG(CHtmlTagSOUND)
HTML_REG_TAG(CHtmlTagQ)
HTML_REG_TAG(CHtmlTagNOLOG)
HTML_REG_TAG(CHtmlTagLOG)

/* table elements */
HTML_REG_TAG(CHtmlTagTABLE)
HTML_REG_TAG(CHtmlTagCAPTION)
HTML_REG_TAG(CHtmlTagTR)
HTML_REG_TAG(CHtmlTagTH)
HTML_REG_TAG(CHtmlTagTD)

/* internal in-line attribute tags */
HTML_REG_TAG(CHtmlTagHILITEON)
HTML_REG_TAG(CHtmlTagHILITEOFF)

/* internal processing directives */
HTML_REG_TAG(CHtmlTagQT2)
HTML_REG_TAG(CHtmlTagQT3)


/* unimplemented HTML 3.2 tags */
/*
  HEAD
  BASE
  ISINDEX
  SCRIPT
  STYLE
  LINK
  META
  NXTID
  FORM
  INPUT
  SELECT
  OPTION
  TEXTAREA
  APPLET
  PARAM
*/

/*
 *   done with key macros for this pass - undefine them so that they can
 *   be redefined differently, if necessary, on a subsequent inclusion of
 *   this header file 
 */
#undef HTML_REG_TAG

