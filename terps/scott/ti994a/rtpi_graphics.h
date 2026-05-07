//
//  rtpi_graphics.h
//  scott
//
//  Created by Administrator on 2026-02-12.
//

#ifndef rtpi_graphics_h
#define rtpi_graphics_h

int DrawRTPIImage(USImage *image);
int DrawFuzzyRTPIImage(USImage *image);
void DrawRTPIFromMem(void);
void UpdateRTPISystemMessages(void);
void FindUSImageDataInDisk(void);
void CompareUSImagesWithDisk(uint8_t *dsk, size_t dsk_size);
void ClearVDP(void);

#endif /* rtpi_graphics_h */
