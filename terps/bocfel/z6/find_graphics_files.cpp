//
//  find_graphics_files.cpp
//  bocfel
//
//  Created by Administrator on 2023-08-20.
//

#include <cstdio>
#include <string>

extern "C" {
#include "glkimp.h"
}

#include "memory_allocation.hpp"

#include "zterp.h"
#include "v6_image.h"
#include "v6_specific.h"
#include "extract_apple_2.h"

#include "draw_image.hpp"
#include "extract_image_data.hpp"
#include "find_graphics_files.hpp"

#include "spatterlight-autosave.h"

std::array<std::string, kGraphicsFileCount> found_graphics_files;

static strid_t load_file(const std::string &file)
{
    return glkunix_stream_open_pathname(const_cast<char *>(file.c_str()), 0, 0);
}

// Frees all per-image data (pixel data, palette, and Huffman trees) and the
// raw_images array itself, leaving image_count at 0.
// An image either owns its Huffman tree (extract_images() allocates one per
// image that has its own) or points at the global default_huffman_tree,
// which is a static buffer and must not be freed.
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
        if (img->huffman_tree && img->huffman_tree != default_huffman_tree)
            free(img->huffman_tree);
        img->huffman_tree = nullptr;
    }
    free(raw_images);
    raw_images = nullptr;
    image_count = 0;
    forget_sent_images();
}

// If nothing is recorded for `slot` yet and a file exists at `path`, record
// the path. Returns true if it did.
static bool try_slot(GraphicsFile slot, const std::string &path)
{
    if (!found_graphics_files.at(slot).empty())
        return false;
    strid_t file = load_file(path);
    if (file == nullptr)
        return false;
    glk_stream_close(file, nullptr);
    found_graphics_files.at(slot) = path;
    return true;
}

// Make the Blorb file at `blorb_file` (or the one already open, whose name is
// then stored in blorb_file) the Glk resource map. Returns false if there is
// no such file or it is not a usable Blorb.
static bool set_blorb_map(std::string &blorb_file)
{
    strid_t file;
    if (active_blorb_file_stream != nullptr) {
        file = active_blorb_file_stream;
        blorb_file = std::string(active_blorb_file_stream->filename);
    } else {
        file = load_file(blorb_file);
    }
    if (file == nullptr)
        return false;
    if (giblorb_set_resource_map(file) == giblorb_err_None) {
        active_blorb_file_stream = file;
        found_graphics_files.at(kGraphicsFileBlorb) = blorb_file;
        return true;
    }
    glk_stream_close(file, nullptr);
    if (file == active_blorb_file_stream)
        active_blorb_file_stream = nullptr;
    return false;
}

// The length of the directory part of `file_name`, including the separator,
// or npos if it has none. Unix, Windows and classic Mac separators are tried
// in that order, and the first kind present wins: this is not the same as
// the last separator of any kind, since a ':' or '\\' may be part of a
// Unix file name.
static size_t directory_length(const std::string &file_name)
{
    for (char delimiter : {'/', '\\', ':'}) {
        auto dirpos = file_name.rfind(delimiter);
        if (dirpos != std::string::npos)
            return dirpos + 1;
    }
    return std::string::npos;
}

// Searches for graphics files associated with the given game file name.
// Probes the game's directory for platform-specific graphics formats:
//   - CPIC.DATA / PIC.DATA (Amiga/Mac image data)
//   - .MG1 / .EG1 / .CG1   (VGA / EGA / CGA image data)
//   - .woz                   (Apple II disk images)
//   - .blb / .blorb          (Blorb resource archives)
// Found paths are stored in the global found_graphics_files array.
static void find_graphics_files(const std::string &file_name)
{
    auto dotpos = file_name.rfind('.');
    auto dirlen = directory_length(file_name);

    if (dirlen != std::string::npos) {
        auto dir = file_name.substr(0, dirlen);
        try_slot(kGraphicsFileCPic, dir + "CPIC.DATA");
        try_slot(kGraphicsFilePic, dir + "PIC.DATA");
    }

    if (dirlen != std::string::npos && dotpos != std::string::npos) {
        auto stem = file_name.substr(0, dotpos);
        try_slot(kGraphicsFileMG1, stem + ".MG1");
        try_slot(kGraphicsFileEG1, stem + ".EG1");
        try_slot(kGraphicsFileCG1, stem + ".CG1");

        // The Zork Zero disk images are named "Zork Zero side N.woz".
        auto woz_stem = stem;
        auto zorkpos = file_name.rfind("ZORK0.");
        if (zorkpos != std::string::npos)
            woz_stem = file_name.substr(0, zorkpos) + "Zork Zero";
        for (const auto &ext : {".woz", " side 1.woz", " - Side 1.woz"}) {
            if (try_slot(kGraphicsFileWoz, woz_stem + ext))
                break;
        }
    }

    if (found_graphics_files.at(kGraphicsFileBlorb).empty()) {
        auto stem = file_name.substr(0, dotpos);
        for (const auto &ext : {".zlb", ".zblorb", ".blb", ".blorb"}) {
            std::string blorb_file = stem + ext;
            if (set_blorb_map(blorb_file)) {
                return;
            }
        }
    }
}

// Reads an entire Glk stream into a malloc'd buffer and returns it.
// The caller is responsible for freeing the returned buffer.
static uint8_t *read_from_file(strid_t file, glui32 *actual_length) {
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

// The EGA pictures are split over two files. Append the images of the .EG2
// companion of the .EG1 file at `eg1_path`, if there is one, to raw_images.
static void append_eg2_images(const std::string &eg1_path) {
    auto dotpos = eg1_path.rfind('.');
    if (dotpos == std::string::npos)
        return;
    strid_t eg2_file = load_file(eg1_path.substr(0, dotpos) + ".EG2");
    if (eg2_file == nullptr)
        return;

    glui32 eg2_length = 0;
    uint8_t *eg2_data = read_from_file(eg2_file, &eg2_length);
    glk_stream_close(eg2_file, nullptr);
    if (eg2_data == nullptr)
        return;

    ImageStruct *more_images;
    int additional_count = extract_images(eg2_data, eg2_length, 2, 0, &more_images, &pixversion, &graphics_type);
    free(eg2_data);
    if (additional_count <= 0)
        return;

    int total_count = image_count + additional_count;
    ImageStruct *all_images = (ImageStruct *)MemAlloc(sizeof(ImageStruct) * total_count);
    memcpy(all_images, raw_images, image_count * sizeof(ImageStruct));
    memcpy(all_images + image_count, more_images, additional_count * sizeof(ImageStruct));
    free(raw_images);
    free(more_images);
    raw_images = all_images;
    image_count = total_count;
    forget_sent_images();
}

// Opens a graphics file and extracts images from it into the global raw_images
// array. Handles three extraction paths:
//   - Apple II: delegates to extract_apple2_disk_images (reads from disk image)
//   - Blorb: delegates to extract_images_from_blorb (uses the Glk resource map)
//   - Raw formats (VGA/EGA/CGA/Amiga/Mac): reads file into memory and parses
// For EGA, also loads supplementary images from a companion .EG2 file.
// Sets the screen geometry for the detected graphics type.
static void extract_from_file(const std::string &path, GraphicsType type) {
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
    forget_sent_images();

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

    set_hw_screen_geometry(graphics_type);

    if (graphics_type == kGraphicsTypeEGA)
        append_eg2_images(path);
}

// Loads the Apple II pictures from the disk images found earlier. Returns
// false, leaving the graphics type alone, if there are none.
bool load_apple2_graphics(void) {
    const std::string &path = found_graphics_files.at(kGraphicsFileWoz);
    if (path.empty())
        return false;
    image_count = extract_apple2_disk_images(path.c_str(), &raw_images, &pixversion);
    forget_sent_images();
    if (image_count == 0)
        return false;
    graphics_type = kGraphicsTypeApple2;
    set_hw_screen_geometry(graphics_type);
    return true;
}

// Loads graphics using the user's preferred format (gli_z6_graphics). If the
// preferred format isn't available, falls back to the first available format
// in found_graphics_files.
static void load_best_graphics(void) {

    // In fallback order.
    static const struct { GraphicsFile slot; GraphicsType type; } sources[] = {
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

    if (gli_z6_graphics == kGraphicsTypeMacBW) {
        // The Mac black and white pictures are in a PIC.DATA, the same name
        // as one of the two Amiga files has.
        found = try_load(kGraphicsFilePic, kGraphicsTypeMacBW);
    } else {
        for (const auto &entry : sources) {
            if (entry.type == gli_z6_graphics && try_load(entry.slot, entry.type)) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        for (const auto &entry : sources) {
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
    const char *base_name = is_spatterlight_journey ? "/journey" :
                            is_spatterlight_arthur  ? "/arthur" :
                            is_spatterlight_shogun  ? "/shogun" :
                            is_spatterlight_zork0   ? "/zork0" : nullptr;
    if (delimiterpos != std::string::npos && base_name != nullptr) {
        // Replace the name up to its extension, or all of it if it has none
        // (a dot before the last slash belongs to a directory).
        size_t name_length = std::string::npos;
        if (dotpos != std::string::npos && dotpos > delimiterpos)
            name_length = dotpos - delimiterpos;
        file_name.replace(delimiterpos, name_length, base_name);
        if (file_name != game_file) {
            find_graphics_files(file_name);
        }
    }
    load_best_graphics();
}
