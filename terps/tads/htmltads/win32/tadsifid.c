/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadsifid.c - IFID generator
Function
  This module generates IFIDs for TADS games.  IFIDs are standard UUIDs:
  basically, 128-bit random numbers formatted into strings of hex digits.

  This uses an implementation of ISAAC, a cryptographically secure PRNG
  created by Bob Jenkins.  ISAAC is freely usable and is currently
  widely regarded as secure.
Notes
  
Modified
  01/19/06 MJRoberts  - Creation
*/

#include <stdio.h>
#include <Windows.h>

#include "tadsifid.h"

#define ISAAC_RANDSIZL   (8)
#define ISAAC_RANDSIZ    (1<<ISAAC_RANDSIZL)

/* ------------------------------------------------------------------------ */
/*
 *   ISAAC context structure 
 */
struct isaacctx
{
    unsigned long cnt;
    unsigned long rsl[ISAAC_RANDSIZ];
    unsigned long mem[ISAAC_RANDSIZ];
    unsigned long a;
    unsigned long b;
    unsigned long c;
};

/* 
 *   ISAAC service macros 
 */

#define isaac_ind(mm,x)  ((mm)[(x>>2)&(ISAAC_RANDSIZ-1)])
#define isaac_step(mix,a,b,mm,m,m2,r,x) \
{ \
    x = *m;  \
    a = ((a^(mix)) + *(m2++)) & 0xffffffff; \
    *(m++) = y = (isaac_ind(mm,x) + a + b) & 0xffffffff; \
    *(r++) = b = (isaac_ind(mm,y>>ISAAC_RANDSIZL) + x) & 0xffffffff; \
}
#define isaac_next_rand(r) \
    ((r)->cnt-- == 0 ? \
    (isaac_gen_group(r), (r)->cnt=ISAAC_RANDSIZ-1, (r)->rsl[(r)->cnt]) : \
    (r)->rsl[(r)->cnt])

#define isaac_mix(a,b,c,d,e,f,g,h) \
{ \
    a^=b<<11; d+=a; b+=c; \
    b^=c>>2;  e+=b; c+=d; \
    c^=d<<8;  f+=c; d+=e; \
    d^=e>>16; g+=d; e+=f; \
    e^=f<<10; h+=e; f+=g; \
    f^=g>>4;  a+=f; g+=h; \
    g^=h<<8;  b+=g; h+=a; \
    h^=a>>9;  c+=h; a+=b; \
}

/* generate the group of numbers */
static void isaac_gen_group(struct isaacctx *ctx)
{
    unsigned long a;
    unsigned long b;
    unsigned long x;
    unsigned long y;
    unsigned long *m;
    unsigned long *mm;
    unsigned long *m2;
    unsigned long *r;
    unsigned long *mend;
    
    mm = ctx->mem;
    r = ctx->rsl;
    a = ctx->a;
    b = (ctx->b + (++ctx->c)) & 0xffffffff;
    for (m = mm, mend = m2 = m + (ISAAC_RANDSIZ/2) ; m<mend ; )
    {
        isaac_step(a<<13, a, b, mm, m, m2, r, x);
        isaac_step(a>>6,  a, b, mm, m, m2, r, x);
        isaac_step(a<<2,  a, b, mm, m, m2, r, x);
        isaac_step(a>>16, a, b, mm, m, m2, r, x);
    }
    for (m2 = mm; m2<mend; )
    {
        isaac_step(a<<13, a, b, mm, m, m2, r, x);
        isaac_step(a>>6,  a, b, mm, m, m2, r, x);
        isaac_step(a<<2,  a, b, mm, m, m2, r, x);
        isaac_step(a>>16, a, b, mm, m, m2, r, x);
    }
    ctx->b = b;
    ctx->a = a;
}


/* ------------------------------------------------------------------------ */
/* 
 *   Initialize.  If 'flag' is true, then use the contents of ctx->rsl[] to
 *   initialize ctx->mm[]; otherwise, we'll use a fixed starting
 *   configuration.  
 */
static void isaac_init(struct isaacctx *ctx, int flag)
{
    int i;
    unsigned long a;
    unsigned long b;
    unsigned long c;
    unsigned long d;
    unsigned long e;
    unsigned long f;
    unsigned long g;
    unsigned long h;
    unsigned long *m;
    unsigned long *r;
    
    ctx->a = ctx->b = ctx->c = 0;
    m = ctx->mem;
    r = ctx->rsl;
    a = b = c = d = e = f = g = h = 0x9e3779b9;         /* the golden ratio */
    
    /* scramble the initial settings */
    for (i = 0 ; i < 4 ; ++i)
    {
        isaac_mix(a, b, c, d, e, f, g, h);
    }

    if (flag) 
    {
        /* initialize using the contents of ctx->rsl[] as the seed */
        for (i = 0 ; i < ISAAC_RANDSIZ ; i += 8)
        {
            a += r[i];   b += r[i+1]; c += r[i+2]; d += r[i+3];
            e += r[i+4]; f += r[i+5]; g += r[i+6]; h += r[i+7];
            isaac_mix(a, b, c, d, e, f, g, h);
            m[i] = a;   m[i+1] = b; m[i+2] = c; m[i+3] = d;
            m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
        }

        /* do a second pass to make all of the seed affect all of m */
        for (i = 0 ; i < ISAAC_RANDSIZ ; i += 8)
        {
            a += m[i];   b += m[i+1]; c += m[i+2]; d += m[i+3];
            e += m[i+4]; f += m[i+5]; g += m[i+6]; h += m[i+7];
            isaac_mix(a, b, c, d, e, f, g, h);
            m[i] = a;   m[i+1] = b; m[i+2] = c; m[i+3] = d;
            m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
        }
    }
    else
    {
        /* initialize using fixed initial settings */
        for (i = 0 ; i < ISAAC_RANDSIZ ; i += 8)
        {
            isaac_mix(a, b, c, d, e, f, g, h);
            m[i] = a; m[i+1] = b; m[i+2] = c; m[i+3] = d;
            m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
        }
    }

    /* fill in the first set of results */
    isaac_gen_group(ctx);

    /* prepare to use the first set of results */    
    ctx->cnt = ISAAC_RANDSIZ;
}


/* ------------------------------------------------------------------------ */
/*
 *   seed the generator
 */
static void isaac_gen_seed(struct isaacctx *ctx, const char *seed_string)
{
    LARGE_INTEGER pc1;
    LARGE_INTEGER pc2;
    LARGE_INTEGER pc3;
    HWND hwnd;
    POINT pt;
    MEMORYSTATUS mem;
    char buf[MAX_PATH];
    int i;
    DWORD drives;
    int dr;

    /*
     *   We need a total of 256 bytes of initialization vector.  Obtain the
     *   values from several factors.  
     */

    /*
     *   Get the high-precision performance counter.  This changes very
     *   rapidly - typically well sub-millisecond.  Use just the low-order
     *   16.  Since the bottom few bits should change very rapidly, get
     *   another 8 after a brief pause, then another 8.  Note that even
     *   though the 'sleep' calls use a fixed wait interval, the exact amount
     *   of time that passes is unpredictable at high precision, since the
     *   'sleep' calls will yield to other running processes in the system
     *   before returning.  
     */
    QueryPerformanceCounter(&pc1);
    Sleep(100);
    QueryPerformanceCounter(&pc2);
    Sleep(200);
    QueryPerformanceCounter(&pc3);

    /* set the first 32 bits of the seed vector */
    ctx->rsl[0] = (pc1.u.LowPart & 0xffff)
                  | ((pc2.u.LowPart & 0xff) << 16)
                  | ((pc3.u.LowPart & 0xff) << 24);

    /* 
     *   Next, create a dummy window, and use its handle as the next 32 bits.
     *   Window handles seem to vary pretty randomly from run to run; they
     *   appear to be something like system global memory addresses, so the
     *   handle we get will depend on what else has been happening in the
     *   system since startup. 
     */
    hwnd = CreateWindow("STATIC", "test", WS_POPUPWINDOW | WS_CAPTION,
                        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0, 0, 0);
    ctx->rsl[1] = (unsigned long)hwnd;
    DestroyWindow(hwnd);

    /* 
     *   now get another 16 bits of system timer data, since the window
     *   creation did a bunch of system work that took an unpredictable
     *   amount of time 
     */
    QueryPerformanceCounter(&pc1);

    /* get the mouse position, keeping the low-order 8 bits of x and y */
    GetCursorPos(&pt);

    /* build another 32 bits from the timer and mouse position */
    ctx->rsl[2] = (pc1.LowPart & 0xffff)
                  | ((pt.x & 0xff) << 16)
                  | ((pt.y & 0xff) << 24);

    /* get the system memory status, and use some of its statistics */
    GlobalMemoryStatus(&mem);
    ctx->rsl[3] = mem.dwAvailPhys;
    ctx->rsl[4] = mem.dwAvailPageFile;
    ctx->rsl[5] = mem.dwAvailVirtual;

    /* 
     *   use our process and thread IDs for another 32 bits (just the
     *   low-order 16 bits of each) 
     */
    ctx->rsl[6] = (GetCurrentProcessId() & 0xffff)
                  | ((GetCurrentThreadId() & 0xffff) << 16);

    /* get information on the available volumes */
    i = 7;
    drives = GetLogicalDrives();
    for (dr = 0 ; dr < 32 ; ++dr)
    {
        /* build the name of the drive */
        buf[0] = 'A' + dr;
        buf[1] = ':';
        buf[2] = '\\';
        
        /* 
         *   get this drive's space if we have a drive here and it's not
         *   removable - skip removables, since we don't want an error
         *   message when no volume is mounted 
         */
        if ((drives & (1 << dr)) != 0 && GetDriveType(buf) == DRIVE_FIXED)
        {
            ULARGE_INTEGER tot_avail, tot_bytes, tot_free;

            /* get the free space - use the free cluster count */
            GetDiskFreeSpaceEx(buf, &tot_avail, &tot_bytes, &tot_free);
            ctx->rsl[i++] = tot_free.u.LowPart;
        }
    }

    /* copy bytes from the seed string */
    if (seed_string != 0 && strlen(seed_string) != 0)
    {
        /* remember the length */
        size_t len = strlen(seed_string);

        /* 
         *   Copy bytes from the end of the string backwards.  The seed
         *   string is probably a filename, in which case the last part of
         *   the string is the most likely to vary from one invocation to the
         *   next, as the first part might be a common local path that's used
         *   for many files (C:\My Documents\My TADS Games\...).  If we reach
         *   the beginning of the string, start over and copy it again.  
         */
        for (len = strlen(seed_string) - 1 ; i < ISAAC_RANDSIZ - 1 ; ++i)
        {
            /* copy a byte */
            ctx->rsl[i++] = seed_string[len - 1];

            /* move to the previous seed string character; wrap at start */
            if (--len == 0)
                len = strlen(seed_string);
        }
    }

    /* get one more performance counter value for the last slot */
    QueryPerformanceCounter(&pc1);
    ctx->rsl[ISAAC_RANDSIZ - 1] = pc1.LowPart;

    /* finally, initialize with this rsl[] array */
    isaac_init(ctx, TRUE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Generate an IFID string 
 */
int tads_generate_ifid(char *buf, size_t buflen, const char *seed_string)
{
    static int isaac_inited = 0;
    static struct isaacctx ctx;
    unsigned short n[8];
    int i;
    
    /* 
     *   we need space for the 32 hex digits, plus 4 hyphens, plus the null
     *   terminator 
     */
    if (buflen < 37)
    {
        buf[0] = '\0';
        return FALSE;
    }

    /* initialize the random number generator if we haven't already */
    if (isaac_inited++ == 0)
        isaac_gen_seed(&ctx, seed_string);

    /* generate our 128 bits as 8 16-bit shorts */
    for (i = 0 ; i < 8 ; ++i)
        n[i] = (short)isaac_next_rand(&ctx);

    /* format them to our output buffer */
    sprintf(buf, "%04lx%04lx-%04lx-%04lx-%04lx-%04lx%04lx%04lx",
            n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7]);

    /* success */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Main command-line version 
 */

#ifdef MAKE_CMDLINE

void main(int argc, char **argv)
{
    char ifid[50];
    int i;

    /* check arguments */
    if (argc != 2)
    {
        printf("usage: tadsifid <filename>\n");
        exit(1);
    }

    for (i = 0 ; i < 25 ; ++i)
    {
        /* generate the IFID */
        tads_generate_ifid(ifid, sizeof(ifid), argv[1]);
        
        /* display it */
        printf("IFID = %s\n", ifid);
    }
}

#endif
