/* cmextract.cpp -- list and extract files from a Coveted Mirror .woz disk,
 * so the G-files (and any other) can be inspected for the <D> DHGR bitmaps.
 *
 *   cmextract <disk.woz> --list
 *   cmextract <disk.woz> <FILE> <out.bin>
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

extern "C" {
#include "../../readwoz/ciderpress.h"
#include "../../readwoz/woz2nib.h"
}

#define DSK_IMAGE_SIZE (35 * 16 * 256)
#define NIB_IMAGE_SIZE (35 * 6656)

static uint8_t *readFile(const char *path, size_t *sz) {
	FILE *f = fopen(path, "rb");
	if (!f) return nullptr;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	uint8_t *d = (uint8_t *)malloc(n);
	if (fread(d, 1, n, f) != (size_t)n) { free(d); fclose(f); return nullptr; }
	fclose(f); *sz = n; return d;
}
static uint8_t *toDsk(uint8_t *data, size_t size, size_t *outSize) {
	if (size >= 4 && data[0]=='W' && data[1]=='O' && data[2]=='Z') {
		size_t nibSize = size;
		uint8_t *nib = woz2nib(data, &nibSize);
		if (!nib) return nullptr;
		uint8_t *dsk = ReadFullImageFromNib(nib, nibSize);
		free(nib);
		*outSize = DSK_IMAGE_SIZE;
		return dsk;
	}
	if (size == NIB_IMAGE_SIZE) { *outSize = DSK_IMAGE_SIZE; return ReadFullImageFromNib(data, size); }
	if (size >= DSK_IMAGE_SIZE) { uint8_t *d=(uint8_t*)malloc(DSK_IMAGE_SIZE); memcpy(d,data,DSK_IMAGE_SIZE); *outSize=DSK_IMAGE_SIZE; return d; }
	return nullptr;
}

int main(int argc, char **argv) {
	if (argc < 3) { fprintf(stderr, "usage: %s <disk.woz> --list | <FILE> <out.bin>\n", argv[0]); return 2; }
	size_t rawSize = 0;
	uint8_t *raw = readFile(argv[1], &rawSize);
	if (!raw) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
	size_t dskSize = 0;
	uint8_t *dsk = toDsk(raw, rawSize, &dskSize);
	if (!dsk) { fprintf(stderr, "cannot decode disk\n"); return 1; }
	size_t numFiles = 0;
	A2FileRec *files = GetAllProDOSFiles(dsk, dskSize, &numFiles);
	if (!files) { fprintf(stderr, "no files\n"); return 1; }
	if (strcmp(argv[2], "--list") == 0) {
		for (size_t i = 0; i < numFiles; i++)
			printf("%-16s %zu\n", files[i].filename, files[i].datasize);
		return 0;
	}
	if (argc < 4) { fprintf(stderr, "need <out.bin>\n"); return 2; }
	for (size_t i = 0; i < numFiles; i++) {
		if (strcasecmp(files[i].filename, argv[2]) == 0) {
			FILE *o = fopen(argv[3], "wb");
			fwrite(files[i].data, 1, files[i].datasize, o);
			fclose(o);
			fprintf(stderr, "wrote %s (%zu bytes)\n", argv[3], files[i].datasize);
			return 0;
		}
	}
	fprintf(stderr, "file '%s' not found. have:", argv[2]);
	for (size_t i = 0; i < numFiles; i++) fprintf(stderr, " %s", files[i].filename);
	fprintf(stderr, "\n");
	return 1;
}
