/*
 *  iff.h
 *  Yazmin
 *
 *  Created by David Schweinsberg on 30/11/07.
 *  Copyright 2007 David Schweinsberg. All rights reserved.
 *
 */
#ifndef IFF_H__
#define IFF_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

typedef struct {
  unsigned char *data;
  unsigned long bufferSize;
  unsigned long pos;
  unsigned long chunkStart;
  unsigned long chunkLen;
} IFFHandle;

unsigned int IFFID(char c1, char c2, char c3, char c4);

unsigned int unpackLong(const void *data);

void packLong(const void *data, unsigned long value);

int isID(const void *data, char c1, char c2, char c3, char c4);
int isForm(const void *data, char c1, char c2, char c3, char c4);
unsigned int chunkIDAndLength(const void *data, unsigned int *chunkID);
unsigned long paddedLength(unsigned long len);

void IFFCreateBuffer(IFFHandle *handle, unsigned long initialBufferSize);

void IFFSetBuffer(IFFHandle *handle, unsigned char *data,
                  unsigned long bufferSize);

void IFFCloseBuffer(IFFHandle *handle);

void IFFBeginForm(IFFHandle *handle, unsigned long type);

void IFFEndForm(IFFHandle *handle);

bool IFFNextForm(IFFHandle *handle, unsigned long *size, unsigned long *type);

void IFFBeginChunk(IFFHandle *handle, unsigned long type);

void IFFEndChunk(IFFHandle *handle);

void IFFGetChunk(IFFHandle *handle, unsigned long *type, unsigned long *size);

bool IFFNextChunk(IFFHandle *handle);

void IFFWriteByte(IFFHandle *handle, unsigned char byte);

void IFFWrite(IFFHandle *handle, const void *data, unsigned long len);

#ifdef __cplusplus
} // extern "C"
#endif //__cplusplus

#endif // IFF_H__
