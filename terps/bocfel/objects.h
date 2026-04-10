// vim: set ft=cpp:

#ifndef ZTERP_OBJECTS_H
#define ZTERP_OBJECTS_H

#include "types.h"

void print_object(uint16_t obj, void (*outc)(uint8_t));

void zget_sibling();
void zget_child();
void zget_parent();
void zremove_obj();
void ztest_attr();
void zset_attr();
void zclear_attr();
void zinsert_obj();
void zget_prop_len();
void zget_prop_addr();
void zget_next_prop();
void zput_prop();
void zget_prop();
void zjin();
void zprint_obj();

#ifdef SPATTERLIGHT
void internal_put_prop(uint16_t object, uint16_t property, uint16_t value);
bool internal_test_attr(uint16_t object, uint16_t attribute);
void internal_set_attr(uint16_t object, uint16_t attribute);
void internal_clear_attr(uint16_t object, uint16_t attribute);
void internal_insert(uint16_t obj1, uint16_t obj2);
int16_t internal_get_parent(int16_t obj);
uint16_t internal_get_prop(int obj, int prop);
uint16_t property_address(uint16_t n);
#endif

#endif
