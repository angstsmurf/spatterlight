/*
 *  iff.h
 *  Spatterlight
 *
 *  Based on code from Yazmin by David Schweinsberg
 *  https://github.com/yazmin
 *
 *  Original code created by David Schweinsberg on 30/11/07.
 *  Copyright 2007 David Schweinsberg. All rights reserved.
 *
 *  Read-only helpers for IFF containers (Blorb, Quetzal). Every function
 *  that takes a length checks it; the ones that do not read a fixed number
 *  of bytes noted in their comment.
 */
#ifndef IFF_H__
#define IFF_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

// Builds a big-endian four-character code, e.g. IFFID('R', 'I', 'd', 'x').
uint32_t IFFID(char c1, char c2, char c3, char c4);

// Read a big-endian 32-bit / 16-bit value. Reads 4 / 2 bytes.
uint32_t unpackLong(const void *data);
uint16_t unpackShort(const void *data);

// True if the 4 bytes at data are c1 c2 c3 c4.
bool isID(const void *data, char c1, char c2, char c3, char c4);

// True if data starts with a "FORM" header of the given type.
// Returns false when len is shorter than the 12-byte FORM header.
bool isForm(const void *data, size_t len, char c1, char c2, char c3, char c4);

// Reads the 8-byte chunk header at offset into the buffer. Returns false
// if the header does not fit. On success *chunkID and *payloadLength are
// set; payloadLength is the length the file claims and may still run past
// the end of the buffer, so check it before reading the payload.
bool chunkHeaderAt(const void *data, size_t len, size_t offset,
                   uint32_t *chunkID, uint32_t *payloadLength);

// A chunk's payload is padded to an even length. Saturates instead of
// wrapping, so that offset arithmetic on hostile lengths stays in range.
size_t paddedLength(size_t len);

// The fields of a Z-code game identifier chunk (IFhd). Quetzal saves and
// Blorb files share the layout: release (2 bytes), serial (6), checksum (2),
// followed in Quetzal by the 3-byte PC, which we do not need.
typedef struct {
  uint16_t release;
  char serial[6];
  uint16_t checksum;
} IFhdInfo;

// Parses an IFhd payload. Returns false if it is shorter than 10 bytes.
bool parseIFhd(const void *payload, size_t len, IFhdInfo *out);

#ifdef __cplusplus
} // extern "C"
#endif //__cplusplus

#endif // IFF_H__
