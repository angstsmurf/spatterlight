/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  vector_selftest.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Helpers for the tests of the Apple II and Atari 8-bit vector renderers:
 *  draw a test picture and compare the screen memory with a reference file.
 *  Used by test/pictures/filltest.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scott.h"

#include "apple2_vector_draw.h"
#include "scott_apple2detect.h"
#include "apple2draw.h"
#include "atari_8bit_vector_draw.h"
#include "ciderpress.h"
#include "common_file_utils.h"
#include "sagagraphics.h"
#include "vector_selftest.h"

uint8_t *ReadTestDataFromFile(const char *filename, const char *supportpath, size_t *size) {
    size_t pathlength = strlen(supportpath) + strlen(filename) + 1;
    char *pathname = MemAlloc(pathlength);
    snprintf(pathname, pathlength, "%s%s", supportpath, filename);
    uint8_t *result = ReadFileIfExists(pathname, size);
    if (!result)
        fprintf(stderr, "ReadTestDataFromFile: Failed to read file at \"%s\"\n", pathname);
    free(pathname);
    return result;
}

static char *TestFileName(const char *prefix, const char *name, const char *suffix) {
    size_t length = strlen(prefix) + strlen(name) + strlen(suffix) + 1;
    char *filename = MemAlloc(length);
    snprintf(filename, length, "%s%s%s", prefix, name, suffix);
    return filename;
}

USImage *LoadTestImage(const char *prefix, const char *name, const char *supportpath) {
    char *filename = TestFileName(prefix, name, ".dat");
    size_t size;
    uint8_t *data = ReadTestDataFromFile(filename, supportpath, &size);
    free(filename);
    if (!data) {
        fprintf(stderr, "Failed to read image data\n");
        return NULL;
    }
    USImage *image = NewImage();
    image->imagedata = data;
    image->datasize = size;
    return image;
}

void FreeTestImage(USImage *image) {
    free(image->imagedata);
    free(image);
}

int CompareWithTestFile(const char *prefix, const char *name, const char *suffix, const char *supportpath, const uint8_t *actual, size_t actuallen) {
    char *filename = TestFileName(prefix, name, suffix);
    fprintf(stderr, "CompareWithTestFile: Comparison with file %s\n", filename);
    size_t size;
    uint8_t *expected = ReadTestDataFromFile(filename, supportpath, &size);
    free(filename);
    if (expected == NULL || size == 0) {
        fprintf(stderr, "Bad file!\n");
        free(expected);
        return 0;
    }
    if (size != actuallen)
        fprintf(stderr, "Size mismatch: file has 0x%zx bytes, screen memory 0x%zx. Comparing the first 0x%zx\n", size, actuallen, MIN(size, actuallen));
    size = MIN(size, actuallen);
    int result = 1;
    for (size_t i = 0; i < size; i++) {
        if (expected[i] != actual[i]) {
            fprintf(stderr, "Mismatch at 0x%04zx: expected 0x%02x, got 0x%02x\n", i, expected[i], actual[i]);
            result = 0;
            break;
        }
    }
    free(expected);
    return result;
}

/* Apple II */

int TestApple2ImageWithName(const char *name, const char *supportpath) {
    USImage *image = LoadTestImage("apple2", name, supportpath);
    if (!image)
        return 0;
    image->usage = IMG_ROOM;
    DrawApple2VectorImage(image);
    FreeTestImage(image);
    return CompareWithTestFile("apple2", name, ".result", supportpath, screenmem, A2_SCREEN_MEM_SIZE);
}

/* Atari 8-bit */

static int ReadAndDrawImageWithName(const char *name, const char *supportpath, USImageType usage) {
    USImage *image = LoadTestImage("atari8", name, supportpath);
    if (!image)
        return 0;
    image->usage = usage;
    DrawAtari8BitVectorImage(image);
    FreeTestImage(image);
    return 1;
}

int TestAtari8ImageWithName(const char *name, const char *supportpath) {
    if (!ReadAndDrawImageWithName(name, supportpath, IMG_ROOM))
        return 0;
    const uint8_t *screen1_plane = GetAtari8Screen1Plane();
    const uint8_t *screen2_plane = GetAtari8Screen2Plane();
    if (!CompareWithTestFile("atari8", name, "90.result", supportpath, screen1_plane, A8_IMAGE_SIZE))
        return 0;
    return CompareWithTestFile("atari8", name, "A0.result", supportpath, screen2_plane, A8_IMAGE_SIZE);
}

int TestAtari8TintOverlay(const char *supportpath) {
    if (!ReadAndDrawImageWithName("so_hexagonal", supportpath, IMG_ROOM))
        return 0;
    if (!ReadAndDrawImageWithName("so_rod", supportpath, IMG_INV_AND_ROOM_OBJ))
        return 0;
    if (!ReadAndDrawImageWithName("so_tint", supportpath, IMG_INV_AND_ROOM_OBJ))
        return 0;

    const uint8_t *screen1_plane = GetAtari8Screen1Plane();
    const uint8_t *screen2_plane = GetAtari8Screen2Plane();
    if (!CompareWithTestFile("tint", "90", ".result", supportpath, screen1_plane, A8_IMAGE_SIZE))
        return 0;
    return CompareWithTestFile("tint", "a0", ".result", supportpath, screen2_plane, A8_IMAGE_SIZE);
}
