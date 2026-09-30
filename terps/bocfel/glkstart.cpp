// Copyright 2010-2024 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <filesystem>
#include <initializer_list>
#include <string>

#include "options.h"
#include "screen.h"
#include "types.h"
#include "zterp.h"

extern "C" {
#include <glk.h>
}

#ifdef ZTERP_GLK_BLORB
extern "C" {
#include <gi_blorb.h>
}
#endif

static void load_resources();

#ifdef ZTERP_GLK_UNIX
extern "C" {
#include <glkstart.h>
}

// The “standard” function (provided by remglk as an extension) for
// getting a filename from a fileref is glkunix_fileref_get_filename;
// Gargoyle has always had the function garglk_fileref_get_name for this
// purpose, but has now adopted the remglk name for greater
// compatibility. If building under garglk, and the newer name is not
// available, fall back to the known-to-exist original name. Otherwise,
// use the new name.
#if defined(GARGLK) && !defined(GLKUNIX_FILEREF_GET_FILENAME)
#define glkunix_fileref_get_filename garglk_fileref_get_name
#endif

// Filled in by the Options constructor.
glkunix_argumentlist_t glkunix_arguments[128] = {
    { nullptr, glkunix_arg_End, nullptr }
};

#ifdef ZTERP_GLK_BLORB
static strid_t load_file(const std::string &file, StreamRock rock)
{
    return glkunix_stream_open_pathname(const_cast<char *>(file.c_str()), 0, static_cast<glui32>(rock));
}
#endif

int glkunix_startup_code(glkunix_startup_t *data)
{
#ifdef GARGLK
    garglk_set_program_name("Bocfel");
#endif
    options.process_arguments(data->argc, data->argv);

    if (arg_status.any() || options.show_version || options.show_help) {
        return 1;
    }

    // Not interrupt in the sense of SIGINT; it’s called by RemGlk
    // when doing a -singleturn shutdown. That will involve closing
    // all streams. So we must finalize any IO objects with Type::Glk
    // first.
    glk_set_interrupt_handler(screen_clean_up_glk_streams);

#ifdef GARGLK
    if (!game_file.empty()) {
        auto slash = game_file.find_last_of('/');
        auto story_name = slash == std::string::npos ? game_file : game_file.substr(slash + 1);
        garglk_set_story_name(story_name.c_str());
    } else {
        frefid_t ref = glk_fileref_create_by_prompt(fileusage_Data | fileusage_BinaryMode, filemode_Read, 0);
        if (ref != nullptr) {
            const char *filename = glkunix_fileref_get_filename(ref);
            if (filename != nullptr) {
                game_file = filename;
            }
            glk_fileref_destroy(ref);
        }
    }
#endif

    if (!game_file.empty()) {
#ifndef ZTERP_OS_DOS
        glkunix_set_base_file(game_file.data());
#endif
        load_resources();
    }

    return 1;
}
#elif defined(ZTERP_GLK_WINGLK)
#include <cstdlib>

#include <WinGlk.h>

using namespace std::literals;

extern "C" {
int InitGlk(unsigned int);
}

#ifdef ZTERP_GLK_BLORB
static strid_t load_file(const std::string &file, StreamRock rock)
{
    frefid_t ref = winglk_fileref_create_by_name(fileusage_BinaryMode | fileusage_Data, const_cast<char *>(file.c_str()), static_cast<glui32>(rock), 0);

    if (ref == nullptr) {
        return nullptr;
    }

    strid_t stream = glk_stream_open_file(ref, filemode_Read, 0);
    glk_fileref_destroy(ref);
    return stream;
}
#endif

static void startup()
{
    winglk_app_set_name("Bocfel");
    winglk_set_menu_name("&Bocfel");
    winglk_set_about_text("Windows Bocfel " ZTERP_VERSION);
    winglk_show_game_dialog();

    options.process_arguments(__argc, __argv);

    if (arg_status.any() || options.show_version || options.show_help) {
        return;
    }

    if (game_file.empty()) {
        const std::string patterns = "*.z1;*.z2;*.z3;*.z4;*.z5;*.z6;*.z7;*.z8;*.zblorb;*.zlb;*.blorb;*.blb";
        std::string filter;
        const char *filename;

        filter = "Z-Code Files (" + patterns + ")|" + patterns + "|All Files (*.*)|*.*||";

        filename = winglk_get_initial_filename(nullptr, "Choose a Z-Code Game", filter.c_str());
        if (filename != nullptr) {
            game_file = filename;
        }
    }

    if (!game_file.empty()) {
        auto path = std::filesystem::path(game_file);

        auto game_dir = path.parent_path();
        if (game_dir.empty()) {
            game_dir = ".";
        }
        winglk_set_resource_directory(game_dir.string().c_str());

        if (auto filename = path.filename(); !filename.empty()) {
            sglk_set_basename(filename.replace_extension().string().data());
        }

        load_resources();
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    if (!InitGlk(0x00000700)) {
        std::exit(EXIT_FAILURE);
    }

    startup();

    glk_main();
    glk_exit();

    return 0;
}
#else
#ifdef ZTERP_GLK_BLORB
#define load_file(file, rock) nullptr
#endif
#error Glk on this platform is not supported.
#endif

#ifdef SPATTERLIGHT

// If we find a valid blorb file (which may be external  or the game file itself)
// we keep track of it using this global. (The blorb file stream stays open during tha game,
// but is closed and reopened during autorestore.)
strid_t active_blorb_file_stream = nullptr;
#endif

// A Blorb file can contain the story file, or it can simply be an
// external package of resources. Try loading the main story file first;
// if it contains Blorb resources, use it. Otherwise, try to find a
// Blorb file that goes with the selected story.
static void load_resources()
{
#ifdef ZTERP_GLK_BLORB
    auto set_map = [](const std::string &blorb_file) {
        strid_t file = load_file(blorb_file, StreamRock::BlorbStream);
        if (file != nullptr) {
            if (giblorb_set_resource_map(file) == giblorb_err_None) {
                screen_load_scale_info();
#ifdef SPATTERLIGHT
                active_blorb_file_stream = file;
#endif
                return true;
            }
            glk_stream_close(file, nullptr);
        }

        return false;
    };

    if (set_map(game_file)) {
        return;
    }

    // Replace the extension by hand: std::filesystem::path requires
    // macOS 10.15, and Spatterlight deploys to 10.13.
    auto slash = game_file.find_last_of('/');
    auto base_start = slash == std::string::npos ? 0 : slash + 1;
    auto dot = game_file.find_last_of('.');
    auto stem = (dot == std::string::npos || dot <= base_start) ? game_file : game_file.substr(0, dot);

    for (const auto &ext : {"blb", "blorb"}) {
        if (set_map(stem + "." + ext)) {
            return;
        }
    }
#endif
}
