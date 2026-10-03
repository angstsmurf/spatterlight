//
//  messagenames.h
//  Spatterlight
//
//  Created by Administrator on 2026-04-04.
//

#ifndef messagenames_h
#define messagenames_h

#include <stddef.h>

/* msgnames[cmd] is the name of protocol message `cmd`, or NULL for a value
   with no name; cmd must be below msgnames_count. */
extern const char *msgnames[];
extern const size_t msgnames_count;

#endif /* messagenames_h */
