/*
 *  iff.c
 *  Yazmin
 *
 *  Created by David Schweinsberg on 30/11/07.
 *  Copyright 2007 David Schweinsberg. All rights reserved.
 *
 */

#include "iff.h"
#include <memory.h>
#include <stdlib.h>

unsigned int IFFID(char c1, char c2, char c3, char c4) {
  return (c1 << 24) | (c2 << 16) | (c3 << 8) | c4;
}

unsigned int unpackLong(const void *data) {
  return (((const unsigned char *)data)[0] << 24) |
         (((const unsigned char *)data)[1] << 16) |
         (((const unsigned char *)data)[2] << 8) |
         (((const unsigned char *)data)[3]);
}

unsigned int unpackShort(const void *data) {
  return (((const unsigned char *)data)[0] << 8) |
         (((const unsigned char *)data)[1]);
}

void packLong(const void *data, unsigned long value) {
  ((unsigned char *)data)[0] = (unsigned char)(value >> 24);
  ((unsigned char *)data)[1] = (unsigned char)(value >> 16);
  ((unsigned char *)data)[2] = (unsigned char)(value >> 8);
  ((unsigned char *)data)[3] = (unsigned char)value;
}

int isID(const void *data, char c1, char c2, char c3, char c4) {
  if (((const char *)data)[0] == c1 && ((const char *)data)[1] == c2 &&
      ((const char *)data)[2] == c3 && ((const char *)data)[3] == c4)
    return 1;
  else
    return 0;
}

int isForm(const void *data, char c1, char c2, char c3, char c4) {
  if (isID(data, 'F', 'O', 'R', 'M'))
    return isID((const char *)data + 8, c1, c2, c3, c4);
  else
    return 0;
}

unsigned int chunkIDAndLength(const void *data, unsigned int *chunkID) {
  *chunkID = unpackLong(data);
  return unpackLong((const char *)data + 4);
}

unsigned long paddedLength(unsigned long len) { return len + len % 2; }

void IFFCreateBuffer(IFFHandle *handle, unsigned long initialBufferSize) {
  handle->data = malloc(initialBufferSize);
  handle->bufferSize = initialBufferSize;
  handle->pos = 0;
  handle->chunkStart = 0;
  handle->chunkLen = 0;
}

void IFFSetBuffer(IFFHandle *handle, unsigned char *data, unsigned long bufferSize) {
  handle->data = data;
  handle->bufferSize = bufferSize;
  handle->pos = 0;
  handle->chunkStart = 0;
  handle->chunkLen = 0;
}

void IFFCloseBuffer(IFFHandle *handle) { free(handle->data); }

static void expandBufferIfNeeded(IFFHandle *handle,
                                 unsigned long additionalLen) {
  if (handle->pos + additionalLen > handle->bufferSize) {
    // Double the size of the current buffer
    handle->bufferSize *= 2;
    unsigned char *buf = malloc(handle->bufferSize);
    memcpy(buf, handle->data, handle->pos);
    free(handle->data);
    handle->data = buf;
  }
}

void IFFBeginForm(IFFHandle *handle, unsigned long type) {
  expandBufferIfNeeded(handle, 12);

  handle->data[handle->pos] = 'F';
  handle->data[handle->pos + 1] = 'O';
  handle->data[handle->pos + 2] = 'R';
  handle->data[handle->pos + 3] = 'M';

  packLong(handle->data + handle->pos + 4, 0);
  packLong(handle->data + handle->pos + 8, type);

  handle->pos += 12;
}

void IFFEndForm(IFFHandle *handle) {
  handle->chunkStart = 0;
  handle->chunkLen = handle->pos - 8;
  IFFEndChunk(handle);
}

bool IFFNextForm(IFFHandle *handle, unsigned long *size, unsigned long *type) {
  while (handle->pos < handle->bufferSize - 12) {
    if (handle->data[handle->pos] == 'F' &&
        handle->data[handle->pos + 1] == 'O' &&
        handle->data[handle->pos + 2] == 'R' &&
        handle->data[handle->pos + 3] == 'M') {
      *size = unpackLong(handle->data + handle->pos + 4);
      *type = unpackLong(handle->data + handle->pos + 8);
      handle->pos += 12;
      return true;
    }
    handle->pos += 1;
  }
  return false;
}

void IFFBeginChunk(IFFHandle *handle, unsigned long type) {
  expandBufferIfNeeded(handle, 8);

  packLong(handle->data + handle->pos, type);
  packLong(handle->data + handle->pos + 4, 0);
  handle->chunkStart = handle->pos;
  handle->chunkLen = 0;
  handle->pos += 8;
}

void IFFEndChunk(IFFHandle *handle) {
  unsigned long padding = paddedLength(handle->chunkLen) - handle->chunkLen;
  unsigned long i;
  for (i = 0; i < padding; ++i)
    IFFWriteByte(handle, 0);
  handle->chunkLen -= padding;
  packLong(handle->data + handle->chunkStart + 4, handle->chunkLen);
}

void IFFGetChunk(IFFHandle *handle, unsigned long *type, unsigned long *size) {
  *type = unpackLong(handle->data + handle->pos);
  *size = unpackLong(handle->data + handle->pos + 4);
}

bool IFFNextChunk(IFFHandle *handle) {
  unsigned long type;
  unsigned long size;
  IFFGetChunk(handle, &type, &size);
  unsigned long paddedSize = paddedLength(size);
  if (handle->pos + paddedSize + 8 < handle->bufferSize) {
    handle->pos += paddedSize + 8;
    return true;
  }
  return false;
}

void IFFWriteByte(IFFHandle *handle, unsigned char byte) {
  expandBufferIfNeeded(handle, 1);
  handle->data[handle->pos] = byte;
  handle->pos += 1;
  handle->chunkLen += 1;
}

void IFFWrite(IFFHandle *handle, const void *data, unsigned long len) {
  expandBufferIfNeeded(handle, len);
  memcpy(handle->data + handle->pos, data, len);
  handle->pos += len;
  handle->chunkLen += len;
}
