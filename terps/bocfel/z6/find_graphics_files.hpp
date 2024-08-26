//
//  find_graphics_files.hpp
//  bocfel6
//
//  Created by Administrator on 2023-08-20.
//

#ifndef find_graphics_files_hpp
#define find_graphics_files_hpp

#include <stdio.h>

extern std::array<std::string, 7> found_graphics_files;

void find_graphics_files(void);
void load_best_graphics(void);
void free_images(void);

enum GraphicsFile {
    kGraphicsFileCPic = 0,
    kGraphicsFilePic = 1,
    kGraphicsFileMG1 = 2,
    kGraphicsFileEG1 = 3,
    kGraphicsFileCG1 = 4,
    kGraphicsFileBlorb = 5,
    kGraphicsFileWoz = 6
};

#endif /* find_graphics_files_hpp */
