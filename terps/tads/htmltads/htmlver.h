/* $Header$ */

/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  htmlver.h - HTML TADS version number
Function
  HTML TADS has its own release numbering, separate from that of the
  underlying TADS VMs.  TADS 2, TADS 3, and HTML TADS are conceptually
  separate components, and in practice each component is on its own
  independent cycle.  Up until release HT-11 in September, 2006, it was our
  practice to identify the HTML TADS version by the versions of the
  underlying VMs linked into the build, but the quasi-independent release
  cycles made this quite confusing.  To rememdy this, we've introduced the
  new practice of giving each HTML TADS release its own independent version
  number.  The underlying VM versions should still be identified in the
  "about" box as well, but the new "HT-n" release number should be given the
  primary position.  Something like this:

     HTML TADS Release HT-11 (TADS 2.5.10/3.0.11)

  The HTML TADS release number will henceforth be in the format "HT-n",
  where n is simply an integer we'll increment on each release.  Ports can
  add a local patch version if needed, preferably with a "build" number
  that's simply incremented on each release (we recommend *not* resetting the
  build numger when the HT-n numbers changes - that could create confusion
  about whether "HT-11 build 5" is more or less recent than "HT-12 build 2";
  it's easier to simply never reuse a build number so there's no ambiguity).

  The first version using this convention was numbered HT-11, since it was
  the 11th public release overall (as far as we can tell from our records).
Notes
  
Modified
  09/08/06 MJRoberts  - Creation
*/

#ifndef HTMLVER_H
#define HTMLVER_H

/*
 *   The HTML TADS HT-n version ID string
 */
#define HTMLTADS_VERSION   "HT-11"


#endif /* HTMLVER_H */
