//
//  unzip_in_mem.h
//  scott
//
//  Created by Administrator on 2026-02-18.
//

// This is code from the MiniZip project (by Gilles Vollant and Mathias Svensson),
// modified in a quick-and-dirty way to decompress in memory instead of writing files.
// The original code is currently at https://github.com/madler/zlib/tree/develop/contrib/minizip
// See http://www.info-zip.org/pub/infozip/license.html for the Info-ZIP license.

// It is used here to extract a single file from a Return to Pirate's Isle cartridge image
// and may not work properly for any other purpose.


#ifndef unzip_in_mem_h
#define unzip_in_mem_h

uint8_t *extract_file_from_zip_data(uint8_t *zipdata, size_t zipdatasize, const char *filename_to_extract, size_t *unzipped_size);

#endif /* unzip_in_mem_h */
