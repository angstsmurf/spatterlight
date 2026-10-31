//  minmax.h
//  MIN and MAX, unless a system header got there first. As with any macro, an
//  argument may be evaluated twice.

#ifndef minmax_h
#define minmax_h

#if !defined MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#if !defined MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#endif /* minmax_h */
