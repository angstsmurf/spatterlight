// vim: set ft=cpp:

#ifndef ZTERP_DICT_H
#define ZTERP_DICT_H

#include "types.h"

void tokenize(uint16_t text, uint16_t parse, uint16_t dictaddr, bool flag);

void ztokenise();
void zencode_text();

uint16_t find_string_in_dictionary(const std::vector<uint8_t> str);

#endif
