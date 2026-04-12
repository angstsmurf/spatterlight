//
//  find_graphics_files.cpp
//  bocfel
//
//  Created by Administrator on 2023-08-20.
//

#include <string>

extern "C" {
#include "glkimp.h"
}

#include "memory_allocation.hpp"

#include "zterp.h"
#include "v6_image.h"
#include "v6_specific.h"
#include "extract_apple_2.h"

#include "extract_image_data.hpp"
#include "find_graphics_files.hpp"

extern strid_t active_blorb_file_stream;

std::array<std::string, 7> found_graphics_files;

// Look for the following files: filename.blb/.blorb, filename.MG1/.EG1/.EG2/.CG1, "pic.data", "cpic.data"

static strid_t load_file(const std::string &file)
{
    return glkunix_stream_open_pathname(const_cast<char *>(file.c_str()), 0, 0);
}

// Frees all per-image data (pixel data, palette, and Huffman trees).
// Huffman trees require special handling: multiple images can share the same
// allocated tree, so before freeing one we null out all other references to it.
// The global default_huffman_tree is a static buffer and must not be freed.
void free_images(void) {
    for (int i = 0; i < image_count; i++) {
        ImageStruct *img = &raw_images[i];
        if (img->data) {
            free(img->data);
            img->data = nullptr;
        }
        if (img->palette) {
            free(img->palette);
            img->palette = nullptr;
        }
        if (img->huffman_tree && img->huffman_tree != default_huffman_tree) {
            // Null out any other images sharing this tree to prevent double-free
            for (int j = 0; j < image_count; j++) {
                if (j != i && raw_images[j].huffman_tree == img->huffman_tree && raw_images[j].huffman_tree != default_huffman_tree)
                    raw_images[j].huffman_tree = nullptr;
            }
            free(img->huffman_tree);
        }
        img->huffman_tree = nullptr;
    }
}

// Searches for graphics files associated with the given game file name.
// Probes the game's directory for platform-specific graphics formats:
//   - CPIC.DATA / PIC.DATA (Amiga/Mac image data)
//   - .MG1 / .EG1 / .CG1   (VGA / EGA / CGA image data)
//   - .woz                   (Apple II disk images)
//   - .blb / .blorb          (Blorb resource archives)
// Found paths are stored in the global found_graphics_files array.
void find_graphics_files(const std::string &file_name)
{
    // Tries to open a Blorb file and register it as the active resource map.
    // Uses an existing stream if one is already open, otherwise opens a new one.
    auto set_map = [](std::string &blorb_file) {

        strid_t file;
        if (active_blorb_file_stream != nullptr) {
            file = active_blorb_file_stream;
            blorb_file = std::string(active_blorb_file_stream->filename);
        } else {
            file = load_file(blorb_file);
            active_blorb_file_stream = file;
        }
        if (file != nullptr) {
            if (giblorb_set_resource_map(file) == giblorb_err_None) {
                found_graphics_files.at(kGraphicsFileBlorb) = blorb_file;
                return true;
            }
            glk_stream_close(file, nullptr);
        }

        return false;
    };


    // Try Unix, Windows, and classic Mac path separators to extract the directory
    for (const auto &delimiter : {'/', '\\', ':'}) {
        auto dirpos = file_name.rfind(delimiter);
        if (dirpos != std::string::npos) {
            auto dir = file_name.substr(0, dirpos + 1);
            int index = kGraphicsFileCPic;
            for (const auto &filename : {"CPIC.DATA", "PIC.DATA"}) {
                if (found_graphics_files.at(index).empty()) {
                    auto path = dir + filename;
                    strid_t file = load_file(path);
                    if (file != nullptr) {
                        found_graphics_files.at(index) = path;
                        glk_stream_close(file, nullptr);
                    }
                }
                index++;
            }

            auto dotpos = file_name.rfind('.');
            if (dotpos != std::string::npos) {
                for (const auto &extension : {".MG1", ".EG1", ".CG1"}) {
                    if (found_graphics_files.at(index).empty()) {
                        auto path = file_name;
                        path.replace(dotpos, std::string::npos, extension);
                        strid_t file = load_file(path);
                        if (file != nullptr) {
                            found_graphics_files.at(index) = path;
                            glk_stream_close(file, nullptr);
                        }
                    }
                    index++;
                }

                if (found_graphics_files.at(kGraphicsFileWoz).empty()) {
                    // Search for Apple 2 woz files
                    for (const auto &ext : {".woz", " side 1.woz", " - Side 1.woz"}) {
                        auto path = file_name;
                        auto zorkpos = path.rfind("ZORK0.");
                        if (zorkpos != std::string::npos) {
                            path.replace(zorkpos, std::string::npos, "Zork Zero.");
                            dotpos = path.rfind('.');
                        }
                        path.replace(dotpos, std::string::npos, ext);
                        strid_t file = load_file(path);
                        if (file != nullptr) {
                            found_graphics_files.at(kGraphicsFileWoz) = path;
                            glk_stream_close(file, nullptr);
                            break;
                        }
                    }
                }
            }
            break;
        }
    }


    if (found_graphics_files.at(kGraphicsFileBlorb).empty()) {
        // Next, we look for external blorb files
        for (const auto &ext : {".zlb", ".zblorb", ".blb", ".blorb"}) {
            std::string blorb_file = file_name;
            auto dot = blorb_file.rfind('.');
            if (dot != std::string::npos) {
                blorb_file.replace(dot, std::string::npos, ext);
            } else {
                blorb_file += ext;
            }

            if (set_map(blorb_file)) {
                return;
            }
        }
    }
}

// Reads an entire Glk stream into a malloc'd buffer and returns it.
// The caller is responsible for freeing the returned buffer.
uint8_t *read_from_file(strid_t file, glui32 *actual_length) {
    glk_stream_set_position(file, 0, seekmode_End);
    auto file_length = glk_stream_get_position(file);
    glk_stream_set_position(file, 0, seekmode_Start);
    char *file_data = (char *)MemAlloc(file_length);
    *actual_length = glk_get_buffer_stream(file, file_data, file_length);
    if (*actual_length != file_length)
        fprintf(stderr, "Read error! Result: %u\n", *actual_length);
    return (uint8_t *)file_data;
}

// Opens a graphics file and extracts images from it into the global raw_images
// array. Handles three extraction paths:
//   - Apple II: delegates to extract_apple2_disk_images (reads from disk image)
//   - Blorb: delegates to extract_images_from_blorb (uses the Glk resource map)
//   - Raw formats (VGA/EGA/CGA/Amiga/Mac): reads file into memory and parses
// For EGA, also loads supplementary images from a companion .EG2 file.
// Sets hw_screenwidth and pixelwidth based on the detected graphics type.
void extract_from_file(std::string path, GraphicsType type) {
    strid_t file = nullptr;
    if (type == kGraphicsTypeBlorb) {
        file = active_blorb_file_stream;
    }
    if (file == nullptr) {
        file = load_file(path);
    }
    if (file != nullptr) {
        glui32 file_length = 0;
        uint8_t *file_data;

        graphics_type = type;

        if (type == kGraphicsTypeApple2) {
            glk_stream_close(file, nullptr);
            image_count = extract_apple2_disk_images(path.c_str(), &raw_images, &pixversion);
        } else if (type == kGraphicsTypeBlorb) {
            image_count = extract_images_from_blorb(&raw_images);
        } else {
            file_data = read_from_file(file, &file_length);
            if (file_data == nullptr)
                return;
            image_count = extract_images(file_data, file_length, 1, 0, &raw_images, &pixversion, &graphics_type);
            free(file_data);
        }

        if (type != kGraphicsTypeApple2 && type != kGraphicsTypeBlorb)
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
            auto dotpos = path.rfind('.');
            if (dotpos != std::string::npos) {
                path.replace(dotpos, std::string::npos, ".EG2");
                file = load_file(path);
                if (file != nullptr) {
                    file_data = read_from_file(file, &file_length);
                    if (file_data == nullptr)
                        return;
                    ImageStruct *more_images;
                    int additional_count = extract_images((uint8_t *)file_data, file_length, 2, 0, &more_images, &pixversion, &graphics_type);
                    free(file_data);
                    glk_stream_close(file, nullptr);
                    if (additional_count > 0) {
                        int total_count = image_count + additional_count;
                        ImageStruct *all_images = (ImageStruct *)MemAlloc(sizeof(ImageStruct) * total_count);
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

// Loads graphics using the user's preferred format (gli_z6_graphics). If the
// preferred format isn't available, falls back to the first available format
// in found_graphics_files.
void load_best_graphics(void) {
    bool found = true;
    switch(gli_z6_graphics) {
        case kGraphicsTypeAmiga:
            // Amiga format graphics data is found in "pic.data" on Amiga disks
            // and in "Cpic.data" on Mac disks.
            if (!found_graphics_files.at(kGraphicsFileCPic).empty()) {
                extract_from_file(found_graphics_files.at(kGraphicsFileCPic), kGraphicsTypeAmiga);
            } else if (!found_graphics_files.at(kGraphicsFilePic).empty()) {
                extract_from_file(found_graphics_files.at(kGraphicsFilePic), kGraphicsTypeAmiga);
            } else {
                found = false;
            }
            break;
        case kGraphicsTypeMacBW:
            if (!found_graphics_files.at(kGraphicsFilePic).empty()) {
                extract_from_file(found_graphics_files.at(kGraphicsFilePic), kGraphicsTypeMacBW);
            } else {
                found = false;
            }
            break;
        case kGraphicsTypeApple2:
            if (!found_graphics_files.at(kGraphicsFileWoz).empty()) {
                extract_from_file(found_graphics_files.at(kGraphicsFileWoz), kGraphicsTypeApple2);
            } else {
                found = false;
            }
            break;
        case kGraphicsTypeBlorb:
            if (!found_graphics_files.at(kGraphicsFileBlorb).empty()) {
                extract_from_file(found_graphics_files.at(kGraphicsFileBlorb), kGraphicsTypeBlorb);
            } else {
                found = false;
            }
            break;
        case kGraphicsTypeVGA:
            if (!found_graphics_files.at(kGraphicsFileMG1).empty()) {
                extract_from_file(found_graphics_files.at(kGraphicsFileMG1), kGraphicsTypeVGA);
            } else {
                found = false;
            }
            break;
        case kGraphicsTypeEGA:
            if (!found_graphics_files.at(kGraphicsFileEG1).empty()) {
                extract_from_file(found_graphics_files.at(kGraphicsFileEG1), kGraphicsTypeEGA);
            } else {
                found = false;
            }

            break;
        case kGraphicsTypeCGA:
            if (!found_graphics_files.at(kGraphicsFileCG1).empty()) {
                extract_from_file(found_graphics_files.at(kGraphicsFileCG1), kGraphicsTypeCGA);
            } else {
                found = false;
            }
            break;
        default:
            found = false;
            break;
    }

    // Maps each found_graphics_files index to its corresponding GraphicsType.
    // Used when falling back to any available format.
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
        for (size_t i = 0; i < found_graphics_files.size(); i++) {
            const auto &filename = found_graphics_files.at(i);
            if (!filename.empty()) {
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
}

// Main entry point for Z6 graphics loading. First searches for graphics files
// named according the game file (e.g. journey-r83-s890706.blb if the game file is
// journey-r83-s890706.z6), then looks for a "base" file name (journey.blb for
// journey-r83-s890706.z6), and finally loads the best available format.
// (See load_best_graphics() above.)
void find_and_load_z6_graphics(void) {
    find_graphics_files(game_file);
    std::string file_name = game_file;
    size_t delimiterpos = file_name.rfind('/');
    size_t dotpos = file_name.rfind('.');

    // Look for a "base" file name which doesn't exactly correspond to the game file name.
    // arthur-r74-s890714.z6 -> arthur.MG1, arthur.EG1, arthur side 1.woz, arthur.blb and so on.
    if (delimiterpos != std::string::npos) {
        if (is_spatterlight_journey) {
            file_name.replace(delimiterpos, dotpos - delimiterpos, "/journey");
        } else if (is_spatterlight_arthur) {
            file_name.replace(delimiterpos, dotpos - delimiterpos, "/arthur");
        } else if (is_spatterlight_shogun) {
            file_name.replace(delimiterpos, dotpos - delimiterpos, "/shogun");
        } else if (is_spatterlight_zork0) {
            file_name.replace(delimiterpos, dotpos - delimiterpos, "/zork0");
        }
        if (file_name != game_file) {
            find_graphics_files(file_name);
        }
    }
    load_best_graphics();
}
