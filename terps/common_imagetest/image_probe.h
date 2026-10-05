// What an interpreter's image probe (plus/test/image_probe.c,
// taylor/test/image_probe.c) and the fake window layer (image_glk.c) ask of
// each other.

#ifndef IMAGE_PROBE_H
#define IMAGE_PROBE_H

#include <stddef.h>
#include <stdint.h>

/* ---- Supplied by the interpreter's probe ---- */

/* The size the graphics window should report. The probe answers with the
   native size of the picture, so that the interpreter settles on a pixel size
   of 1 and no margins: canvas coordinates are then picture pixels. */
void ImageProbeGraphicsSize(int *width, int *height);

/* Called once, when the game first waits for a command: draw every picture
   and report each one with ImageProbeReport(). */
void ImageProbeDump(void);

/* ---- Supplied by image_glk.c ---- */

/* Forget everything painted so far. */
void ImageProbeClear(void);

/* Print one fingerprint line for what has been painted since the last
   ImageProbeClear(): the name, the detail (may be NULL), the number of painted
   pixels, their bounding box and a CRC32 of the colours. With -d, also writes
   <name>.png. */
void ImageProbeReport(const char *name, const char *detail);

/* Print a line of the fingerprint that is not a picture (counts, a CRC of the
   packed picture data). */
void ImageProbeNote(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

uint32_t ImageProbeCRC(const void *data, size_t length);

#endif
