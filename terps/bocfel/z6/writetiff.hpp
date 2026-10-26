//
//  writetiff.hpp
//  bocfel6
//
//  Created by Administrator on 2023-05-22.
//

#ifndef writetiff_hpp
#define writetiff_hpp

#include <cstddef>
#include <cstdint>
#include <string>

// Writes an RGBA buffer of `size` bytes, `width` pixels wide, to a TIFF file
// at `path`. Reports a failure on stderr and leaves it at that.
void write_to_tiff(const std::string &path, const uint8_t *data, size_t size, uint32_t width);

#endif /* writetiff_hpp */
