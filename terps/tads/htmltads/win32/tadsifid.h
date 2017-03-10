/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadsifid.h - TADS IFID generator
Function
  Generates IFIDs for TADS games.  An IFID is a 128-bit UUID; we just pick
  random numbers using a cryptographically secure PRNG, seeded with some
  randomness from the current machine (time of day, memory size, etc).
Notes
  
Modified
  04/09/06 MJRoberts  - Creation
*/

#ifndef TADSIFID_H
#define TADSIFID_H

#ifdef __cplusplus
#define externC extern "C"
#else
#define externC
#endif

/*
 *   Generate a new IFID.  This is just a 128-bit random number, formatted in
 *   standard UUID format (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx).  The seed
 *   string is an optional string that we'll use as an additional source of
 *   randomness in seeding the random number generator; it's recommended that
 *   this simply be the filename of the game for which we're generating an
 *   IFID.  
 */
externC int tads_generate_ifid(char *buf, size_t buflen,
                               const char *seed_string);

#endif /* TADSIFID_H */

