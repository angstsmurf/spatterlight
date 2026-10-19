/*
 *  iff.c
 *  Spatterlight
 *
 *  Based on code from Yazmin by David Schweinsberg
 *  https://github.com/yazmin
 *
 *  Original code created by David Schweinsberg on 30/11/07.
 *  Copyright 2007 David Schweinsberg. All rights reserved.
 *
 */

#include "iff.h"
#include <string.h>

uint32_t IFFID(char c1, char c2, char c3, char c4) {
  return ((uint32_t)(unsigned char)c1 << 24) |
         ((uint32_t)(unsigned char)c2 << 16) |
         ((uint32_t)(unsigned char)c3 << 8) |
         (uint32_t)(unsigned char)c4;
}

uint32_t unpackLong(const void *data) {
  const unsigned char *p = data;
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

uint16_t unpackShort(const void *data) {
  const unsigned char *p = data;
  return (uint16_t)((p[0] << 8) | p[1]);
}

bool isID(const void *data, char c1, char c2, char c3, char c4) {
  const char *p = data;
  return p[0] == c1 && p[1] == c2 && p[2] == c3 && p[3] == c4;
}

bool isForm(const void *data, size_t len, char c1, char c2, char c3, char c4) {
  if (!data || len < 12)
    return false;
  return isID(data, 'F', 'O', 'R', 'M') &&
         isID((const char *)data + 8, c1, c2, c3, c4);
}

bool chunkHeaderAt(const void *data, size_t len, size_t offset,
                   uint32_t *chunkID, uint32_t *payloadLength) {
  if (!data || offset > len || len - offset < 8)
    return false;
  const unsigned char *p = (const unsigned char *)data + offset;
  *chunkID = unpackLong(p);
  *payloadLength = unpackLong(p + 4);
  return true;
}

size_t paddedLength(size_t len) {
  if (len == SIZE_MAX)
    return SIZE_MAX;
  return len + (len & 1);
}

bool parseIFhd(const void *payload, size_t len, IFhdInfo *out) {
  if (!payload || len < 10)
    return false;
  const unsigned char *p = payload;
  out->release = unpackShort(p);
  memcpy(out->serial, p + 2, 6);
  out->checksum = unpackShort(p + 8);
  return true;
}
