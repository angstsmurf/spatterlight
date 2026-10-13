//
//  find_graphics_files.hpp
//  bocfel
//
//  Created by Administrator on 2023-08-20.
//

#ifndef find_graphics_files_hpp
#define find_graphics_files_hpp

#include <array>
#include <string>

void free_images(void);
void find_and_load_z6_graphics(void);
bool load_apple2_graphics(void);

enum GraphicsFile {
    kGraphicsFileCPic = 0,
    kGraphicsFilePic = 1,
    kGraphicsFileMG1 = 2,
    kGraphicsFileEG1 = 3,
    kGraphicsFileCG1 = 4,
    kGraphicsFileBlorb = 5,
    kGraphicsFileWoz = 6,
    kGraphicsFileCount
};

// Path of each kind of graphics file found next to the game file, indexed by
// GraphicsFile. Empty if there is no such file.
extern std::array<std::string, kGraphicsFileCount> found_graphics_files;

#endif /* find_graphics_files_hpp */
