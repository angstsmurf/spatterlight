//
//  find_graphics_files.cpp
//  bocfel6
//
//  Created by Administrator on 2023-08-20.
//

#include <string>

extern "C" {
#include "glkimp.h"
}

#include "zterp.h"
#include "v6_image.h"
#include "extract_apple_2.h"

#include "extract_image_data.hpp"
#include "find_graphics_files.hpp"

std::array<std::string, 7> found_graphics_files;

// Look for the following files: filename.blb/.blorb, filename.MG1/.EG1/.EG2/.CG1, "pic.data", "cpic.data"

static strid_t load_file(const std::string &file)
{
    return glkunix_stream_open_pathname(const_cast<char *>(file.c_str()), 0, 0);
}

extern uint8_t lookup_table[256];

void free_images(void) {
    for (int i = 0; i < image_count; i++) {
        ImageStruct *img = &raw_images[i];
        if (img->data)
            free(img->data);
        if (img->palette)
            free(img->palette);
        if (img->huffman_tree && img->huffman_tree != lookup_table) {
            for (int j = 0; j < image_count; j++) {
                if (j != i && raw_images[j].huffman_tree == img->huffman_tree && raw_images[j].huffman_tree != lookup_table)
                    raw_images[j].huffman_tree = nullptr;
            }
            free(img->huffman_tree);
        }
    }
}

void find_graphics_files(void)
{
    auto set_map = [](const std::string &blorb_file) {
        strid_t file = load_file(blorb_file);
        if (file != nullptr) {
            if (giblorb_set_resource_map(file) == giblorb_err_None) {
                found_graphics_files.at(kGraphicsFileBlorb) = blorb_file;
                return true;
            }
            glk_stream_close(file, nullptr);
        }

        return false;
    };


    for (const auto &delimiter : {'/', '\\', ':'}) {
        auto dirpos = game_file.rfind(delimiter);
        if (dirpos != std::string::npos) {
            auto dir = game_file.substr(0, dirpos + 1);
            int index = kGraphicsFileCPic;
            for (const auto &filename : {"CPIC.DATA", "PIC.DATA"}) {
                auto path = dir + filename;
                strid_t file = load_file(path);
                if (file != nullptr) {
                    found_graphics_files.at(index) = path;
                    fprintf(stderr, "found graphics file of type \"%s\"\n", filename);
                    glk_stream_close(file, nullptr);
                }
                index++;
            }

            auto dotpos = game_file.rfind('.');
            if (dotpos != std::string::npos) {
                for (const auto &extension : {".MG1", ".EG1", ".CG1"}) {
                    auto path = game_file;
                    path.replace(dotpos, std::string::npos, extension);
                    strid_t file = load_file(path);
                    if (file != nullptr) {
                        found_graphics_files.at(index) = path;
                        fprintf(stderr, "found graphics file of type \"%s\"\n", extension);
                        glk_stream_close(file, nullptr);
                    }
                    index++;
                }

                // Search for Apple 2 woz files
                for (const auto &ext : {".woz", " side 1.woz", " - Side 1.woz"}) {
                    auto path = game_file;
                    auto zorkpos = path.rfind("ZORK0.");
                    if (zorkpos != std::string::npos) {
                        path.replace(zorkpos, std::string::npos, "Zork Zero.");
                        dotpos = path.rfind('.');
                    }
                    path.replace(dotpos, std::string::npos, ext);
                    strid_t file = load_file(path);
                    if (file != nullptr) {
                        found_graphics_files.at(kGraphicsFileWoz) = path;
                        fprintf(stderr, "found .woz file to use for Apple 2 graphics\n");
                        glk_stream_close(file, nullptr);
                        break;
                    }
                }
            }
            break;
        }
    }


    // Next, we look for external blorb files
    for (const auto &ext : {".blb", ".blorb"}) {
        std::string blorb_file = game_file;
        auto dot = blorb_file.rfind('.');
        if (dot != std::string::npos) {
            blorb_file.replace(dot, std::string::npos, ext);
        } else {
            blorb_file += ext;
        }

        if (set_map(blorb_file)) {
            fprintf(stderr, "found graphics file of type blorb.\n");
            found_graphics_files.at(kGraphicsFileBlorb) = game_file;
            return;
        }
    }
}

uint8_t *read_from_file(strid_t file, glui32 *actual_length) {
    glk_stream_set_position(file, 0, seekmode_End);
    auto file_length = glk_stream_get_position(file);
    glk_stream_set_position(file, 0, seekmode_Start);
    char *file_data = (char *)malloc(file_length);
    if (file_data == nullptr)
        exit(1);
    *actual_length = glk_get_buffer_stream(file, file_data, file_length);
    if (*actual_length != file_length)
        fprintf(stderr, "Read error! Result: %u\n", *actual_length);
    return (uint8_t *)file_data;
}

void extract_from_file(std::string path, GraphicsType type) {
    strid_t file = load_file(path);
    if (file != nullptr) {
        glui32 file_length = 0;
        uint8_t *file_data;

        if (type == kGraphicsTypeApple2) {
            glk_stream_close(file, nullptr);
            image_count = extract_apple_2_images(path.c_str(), &raw_images, &pixversion);
        } else {
            file_data = read_from_file(file, &file_length);
            if (file_data == nullptr)
                return;
        }

        graphics_type = type;
        if (type == kGraphicsTypeBlorb) {
            image_count = extract_images_from_blorb(&raw_images);
        } else if (type != kGraphicsTypeApple2) {
            image_count = extract_images(file_data, file_length, 1, 0, &raw_images, &pixversion, &graphics_type);
        }

        if (type != kGraphicsTypeApple2)
            glk_stream_close(file, nullptr);

        if (image_count == 0)
            return;

        switch(graphics_type) {
            case kGraphicsTypeBlorb:
            case kGraphicsTypeVGA:
            case kGraphicsTypeAmiga:
            case kGraphicsTypeNoGraphics:
                hw_screenwidth = 320;
                pixelwidth = 1.0;
                break;
            case kGraphicsTypeMacBW:
                hw_screenwidth = 480;
                pixelwidth = 1.0;
                break;
            case kGraphicsTypeEGA:
            case kGraphicsTypeCGA:
                hw_screenwidth = 640;
                pixelwidth = 0.5;
                break;
            case kGraphicsTypeApple2:
                hw_screenwidth = 140;
                pixelwidth = 2.0;
                break;
        }

        if (graphics_type == kGraphicsTypeEGA) {
            // look for .EG2 file and load additional pictures from it
            auto dotpos = game_file.rfind('.');
            if (dotpos != std::string::npos) {
                path.replace(dotpos, std::string::npos, ".EG2");
                file = load_file(path);
                if (file != nullptr) {
                    file_data = read_from_file(file, &file_length);
                    if (file_data == nullptr)
                        return;
                    ImageStruct *more_images;
                    int additional_count = extract_images((uint8_t *)file_data, file_length, 2, 0, &more_images, &pixversion, &graphics_type);
                    glk_stream_close(file, nullptr);
                    if (additional_count > 0) {
                        int total_count = image_count + additional_count;
                        ImageStruct *all_images = (ImageStruct *)malloc(sizeof(ImageStruct) * total_count);
                        memcpy(all_images, raw_images, image_count * sizeof(ImageStruct));
                        memcpy(all_images + image_count, more_images, additional_count * sizeof(ImageStruct));
                        free(raw_images);
                        free(more_images);
                        raw_images = all_images;
                        image_count = total_count;
                    }
                }
            }
        }
    }
}

void load_best_graphics(void) {
    bool found = true;
    switch(gli_z6_graphics) {
        case kGraphicsTypeAmiga:
            // Amiga format graphics data is found in "pic.data" on Amiga disks
            // and in "Cpic.data" on Mac disks.
            if (found_graphics_files.at(kGraphicsFileCPic).size() != 0) {
                extract_from_file(found_graphics_files.at(kGraphicsFileCPic), kGraphicsTypeAmiga);
            } else if (found_graphics_files.at(kGraphicsFilePic).size() != 0) {
                extract_from_file(found_graphics_files.at(kGraphicsFilePic), kGraphicsTypeAmiga);
            } else {
                found = false;
            }
            break;
        case kGraphicsTypeMacBW:
            if (found_graphics_files.at(kGraphicsFilePic).size() != 0) {
                extract_from_file(found_graphics_files.at(kGraphicsFilePic), kGraphicsTypeMacBW);
            } else {
                found = false;
            }
            break;
        case kGraphicsTypeApple2:
            if (found_graphics_files.at(kGraphicsFileWoz).size() != 0) {
                extract_from_file(found_graphics_files.at(kGraphicsFileWoz), kGraphicsTypeApple2);
            } else {
                found = false;
            }
            break;
        case kGraphicsTypeBlorb:
            if (found_graphics_files.at(kGraphicsFileBlorb).size() != 0) {
                extract_from_file(found_graphics_files.at(kGraphicsFileBlorb), kGraphicsTypeBlorb);
            } else {
                found = false;
            }
            break;
        case kGraphicsTypeVGA:
            if (found_graphics_files.at(kGraphicsFileMG1).size() != 0) {
                extract_from_file(found_graphics_files.at(kGraphicsFileMG1), kGraphicsTypeVGA);
            } else {
                found = false;
            }
            break;
        case kGraphicsTypeEGA:
            if (found_graphics_files.at(kGraphicsFileEG1).size() != 0) {
                extract_from_file(found_graphics_files.at(kGraphicsFileEG1), kGraphicsTypeEGA);
            } else {
                found = false;
            }

            break;
        case kGraphicsTypeCGA:
            if (found_graphics_files.at(kGraphicsFileCG1).size() != 0) {
                extract_from_file(found_graphics_files.at(kGraphicsFileCG1), kGraphicsTypeCGA);
            } else {
                found = false;
            }
            break;
        default:
            found = false;
            break;
    }

    std::array<GraphicsType, 7> translation_table = {
        kGraphicsTypeAmiga,
        kGraphicsTypeAmiga,
        kGraphicsTypeVGA,
        kGraphicsTypeEGA,
        kGraphicsTypeCGA,
        kGraphicsTypeBlorb,
        kGraphicsTypeApple2
    };


    if (!found) {
        for (int i = 0; i < 7; i++) {
            const auto &filename = found_graphics_files.at(i);
            if (filename.size() != 0) {
                extract_from_file(filename, translation_table.at(i));
                if (image_count != 0) {
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found) {
        fprintf(stderr, "No graphics found!\n");
        graphics_type = kGraphicsTypeNoGraphics;
    }

    return;
}
