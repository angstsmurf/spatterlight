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
    auto set_map = [](std::string &blorb_file) {
        strid_t file;
        if (active_blorb_file_stream != nullptr) {
            file = active_blorb_file_stream;
            blorb_file = std::string(active_blorb_file_stream->filename);
        } else {
            file = load_file(blorb_file);
        }
        if (file != nullptr) {
            if (giblorb_set_resource_map(file) == giblorb_err_None) {
                active_blorb_file_stream = file;
                found_graphics_files.at(kGraphicsFileBlorb) = blorb_file;
                return true;
            }
            glk_stream_close(file, nullptr);
            if (file == active_blorb_file_stream)
                active_blorb_file_stream = nullptr;
        }
        return false;
    };

    // Try Unix, Windows, and classic Mac path separators to extract the directory
    for (const auto &delimiter : {'/', '\\', ':'}) {
        auto dirpos = file_name.rfind(delimiter);
        if (dirpos != std::string::npos) {
            auto dir = file_name.substr(0, dirpos + 1);

            struct { const char *filename; GraphicsFile slot; } data_files[] = {
                {"CPIC.DATA", kGraphicsFileCPic},
                {"PIC.DATA",  kGraphicsFilePic},
            };
            for (const auto &entry : data_files) {
                if (found_graphics_files.at(entry.slot).empty()) {
                    auto path = dir + entry.filename;
                    strid_t file = load_file(path);
                    if (file != nullptr) {
                        found_graphics_files.at(entry.slot) = path;
                        glk_stream_close(file, nullptr);
                    }
                }
            }

            auto dotpos = file_name.rfind('.');
            if (dotpos != std::string::npos) {
                struct { const char *extension; GraphicsFile slot; } ext_files[] = {
                    {".MG1", kGraphicsFileMG1},
                    {".EG1", kGraphicsFileEG1},
                    {".CG1", kGraphicsFileCG1},
                };
                for (const auto &entry : ext_files) {
                    if (found_graphics_files.at(entry.slot).empty()) {
                        auto path = file_name;
                        path.replace(dotpos, std::string::npos, entry.extension);
                        strid_t file = load_file(path);
                        if (file != nullptr) {
                            found_graphics_files.at(entry.slot) = path;
                            glk_stream_close(file, nullptr);
                        }
                    }
                }

                if (found_graphics_files.at(kGraphicsFileWoz).empty()) {
                    for (const auto &ext : {".woz", " side 1.woz", " - Side 1.woz"}) {
                        auto path = file_name;
                        auto woz_dotpos = dotpos;
                        auto zorkpos = path.rfind("ZORK0.");
                        if (zorkpos != std::string::npos) {
                            path.replace(zorkpos, std::string::npos, "Zork Zero.");
                            woz_dotpos = path.rfind('.');
                        }
                        path.replace(woz_dotpos, std::string::npos, ext);
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
    if (file == nullptr || actual_length == nullptr)
        return nullptr;

    glk_stream_set_position(file, 0, seekmode_End);
    glui32 file_length = glk_stream_get_position(file);
    if (file_length == 0)
        return nullptr;

    glk_stream_set_position(file, 0, seekmode_Start);
    uint8_t *file_data = (uint8_t *)MemAlloc(file_length);
    *actual_length = glk_get_buffer_stream(file, (char *)file_data, file_length);
    if (*actual_length != file_length) {
        fprintf(stderr, "Read error! Expected: %u, got: %u\n", file_length, *actual_length);
        free(file_data);
        return nullptr;
    }
    return file_data;
}

// Opens a graphics file and extracts images from it into the global raw_images
// array. Handles three extraction paths:
//   - Apple II: delegates to extract_apple2_disk_images (reads from disk image)
//   - Blorb: delegates to extract_images_from_blorb (uses the Glk resource map)
//   - Raw formats (VGA/EGA/CGA/Amiga/Mac): reads file into memory and parses
// For EGA, also loads supplementary images from a companion .EG2 file.
// Sets hw_screenwidth and pixelwidth based on the detected graphics type.
void extract_from_file(const std::string &path, GraphicsType type) {
    strid_t file = nullptr;
    if (type == kGraphicsTypeBlorb) {
        file = active_blorb_file_stream;
    }
    if (file == nullptr) {
        file = load_file(path);
    }
    if (file == nullptr)
        return;

    graphics_type = type;

    if (type == kGraphicsTypeApple2) {
        glk_stream_close(file, nullptr);
        image_count = extract_apple2_disk_images(path.c_str(), &raw_images, &pixversion);
    } else if (type == kGraphicsTypeBlorb) {
        image_count = extract_images_from_blorb(&raw_images);
    } else {
        glui32 file_length = 0;
        uint8_t *file_data = read_from_file(file, &file_length);
        glk_stream_close(file, nullptr);
        if (file_data == nullptr)
            return;
        image_count = extract_images(file_data, file_length, 1, 0, &raw_images, &pixversion, &graphics_type);
        free(file_data);
    }

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
        auto dotpos = path.rfind('.');
        if (dotpos != std::string::npos) {
            std::string eg2_path = path;
            eg2_path.replace(dotpos, std::string::npos, ".EG2");
            strid_t eg2_file = load_file(eg2_path);
            if (eg2_file != nullptr) {
                glui32 eg2_length = 0;
                uint8_t *eg2_data = read_from_file(eg2_file, &eg2_length);
                glk_stream_close(eg2_file, nullptr);
                if (eg2_data != nullptr) {
                    ImageStruct *more_images;
                    int additional_count = extract_images(eg2_data, eg2_length, 2, 0, &more_images, &pixversion, &graphics_type);
                    free(eg2_data);
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

    struct { GraphicsFile slot; GraphicsType type; } fallback_table[] = {
        {kGraphicsFileCPic,  kGraphicsTypeAmiga},
        {kGraphicsFilePic,   kGraphicsTypeAmiga},
        {kGraphicsFileMG1,   kGraphicsTypeVGA},
        {kGraphicsFileEG1,   kGraphicsTypeEGA},
        {kGraphicsFileCG1,   kGraphicsTypeCGA},
        {kGraphicsFileBlorb, kGraphicsTypeBlorb},
        {kGraphicsFileWoz,   kGraphicsTypeApple2},
    };

    auto try_load = [](GraphicsFile slot, GraphicsType type) {
        if (!found_graphics_files.at(slot).empty()) {
            extract_from_file(found_graphics_files.at(slot), type);
            return image_count != 0;
        }
        return false;
    };

    bool found = false;

    switch(gli_z6_graphics) {
        case kGraphicsTypeAmiga:
            found = try_load(kGraphicsFileCPic, kGraphicsTypeAmiga)
                 || try_load(kGraphicsFilePic, kGraphicsTypeAmiga);
            break;
        case kGraphicsTypeMacBW:
            found = try_load(kGraphicsFilePic, kGraphicsTypeMacBW);
            break;
        case kGraphicsTypeApple2:
            found = try_load(kGraphicsFileWoz, kGraphicsTypeApple2);
            break;
        case kGraphicsTypeBlorb:
            found = try_load(kGraphicsFileBlorb, kGraphicsTypeBlorb);
            break;
        case kGraphicsTypeVGA:
            found = try_load(kGraphicsFileMG1, kGraphicsTypeVGA);
            break;
        case kGraphicsTypeEGA:
            found = try_load(kGraphicsFileEG1, kGraphicsTypeEGA);
            break;
        case kGraphicsTypeCGA:
            found = try_load(kGraphicsFileCG1, kGraphicsTypeCGA);
            break;
        default:
            break;
    }

    if (!found) {
        for (const auto &entry : fallback_table) {
            if (try_load(entry.slot, entry.type)) {
                found = true;
                break;
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
// See load_best_graphics() above.
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
