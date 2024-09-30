// vim: set ft=cpp:

#ifndef ZTERP_DICT_H
#define ZTERP_DICT_H

#include "types.h"

void tokenize(uint16_t text, uint16_t parse, uint16_t dictaddr, bool ignore_unknown);

void ztokenise();
void zencode_text();

#ifdef SPATTERLIGHT
void public_encode_text(uint32_t text, uint16_t len, uint16_t coded);
#endif

#endif
