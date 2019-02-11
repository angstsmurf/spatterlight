/*----------------------------------------------------------------------*\

  decode.c

  Arithmetic decoding module in Arun

\*----------------------------------------------------------------------*/

#include <stdio.h>

#include "main.h"
#include "decode.h"


/* Bit output */
static int decodeBuffer;	/* Bits to be input */
static int bitsToGo;		/* Bits still in buffer */
static int garbageBits;		/* Bits past EOF */


#ifdef _PROTOTYPES_
static int inputBit(void)
#else
static int inputBit()
#endif
{
  int bit;

  if (!bitsToGo) {		/* More bits available ? */
    decodeBuffer = getc(txtfil); /* No, so get more */
    if (decodeBuffer == EOF) {
      garbageBits++;
      if (garbageBits > VALUEBITS-2)
	syserr("Error in encoded data file.");
    } else
      bitsToGo = 8;		/* Another Char, 8 new bits */
  }
  bit = decodeBuffer&1;		/* Get next bit */
  decodeBuffer = decodeBuffer>>1; /* and remove it */
  bitsToGo--;
  return bit;
}


/* Current state of decoding */

static CodeValue value;			/* Currently seen code value */
static CodeValue low, high;		/* Current code region */


#ifdef _PROTOTYPES_
void startDecoding(void)
#else
void startDecoding()
#endif
{
  int i;

  bitsToGo = 0;
  garbageBits = 0;

  value = 0;
  for (i = 0; i < VALUEBITS; i++)
    value = 2*value + inputBit();
  low = 0;
  high = TOPVALUE;
}


#ifdef _PROTOTYPES_
int decodeChar(void)
#else
int decodeChar()
#endif
{
  long range;
  int f;
  int symbol;

  range = (long)(high-low) + 1;
  f = (((long)(value-low)+1)*freq[0]-1)/range;

  /* Find the symbol */
  for (symbol = 1; (int)freq[symbol] > f; symbol++);

  high = low + range*freq[symbol-1]/freq[0]-1;
  low = low + range*freq[symbol]/freq[0];

  for (;;) {
    if (high < HALF)
      ;
    else if (low >= HALF) {
      value = value - HALF;
      low = low - HALF;
      high = high - HALF;
    } else if (low >= ONEQUARTER && high < THREEQUARTER) {
      value = value - ONEQUARTER;
      low = low - ONEQUARTER;
      high = high - ONEQUARTER;
    } else
      break;

    /* Scale up the range */
    low = 2*low;
    high = 2*high+1;
    value = 2*value + inputBit();
  }
  return symbol-1;
}



/* Structure for saved decode info */
typedef struct DecodeInfo {
  long fpos;
  int buffer;
  int bits;
  CodeValue value;
  CodeValue high;
  CodeValue low;
} DecodeInfo;


/*======================================================================

  pushDecode()

  Save so much info about the decoding process so it is possible to
  restore and continue later.

 */
#ifdef _PROTOTYPES_
void *pushDecode(void)
#else
void *pushDecode()
#endif
{
  DecodeInfo *info;

  info = (DecodeInfo *) allocate(sizeof(DecodeInfo));
  info->fpos = ftell(txtfil);
  info->buffer = decodeBuffer;
  info->bits = bitsToGo;
  info->value = value;
  info->high = high;
  info->low = low;
  return(info);
}


/*======================================================================

  popDecode()

  Restore enough info about the decoding process so it is possible to
  continue after having decoded something else.

 */
#ifdef _PROTOTYPES_
void popDecode(void *i)
#else
void popDecode(i)
    void *i;
#endif
{
  DecodeInfo *info = (DecodeInfo *) i;

  fseek(txtfil, info->fpos, 0);
  decodeBuffer = info->buffer;
  bitsToGo = info->bits;
  value = info->value;
  high = info->high;
  low = info->low;

  free(info);
}
