#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2001 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32nogch.cpp - No Game Chest option
Function
  Provides dummy definitions for the required Game Chest entrypoints,
  allowing a version without the game chest to be linked.
Notes
  
Modified
  09/15/01 MJRoberts  - Creation
*/

#include <Windows.h>

#include "htmlw32.h"

/*
 *   Detect whether or not Game Chest is included in this build. 
 */
int CHtmlSys_mainwin::is_game_chest_present()
{
    /* game chest is not present in this build */
    return FALSE;
}

void CHtmlSys_mainwin::do_activate_game_chest(int)
{
}

void CHtmlSys_mainwin::owner_maybe_reload_game_chest()
{
}

void CHtmlSys_mainwin::owner_write_game_chest_db(const textchar_t *)
{
}

const textchar_t *CHtmlSys_mainwin::look_up_fav_title_by_filename(
    const textchar_t *)
{
    return 0;
}

void CHtmlSys_mainwin::show_game_chest(int, int, int)
{
    /* game chest isn't used in this build, so there's nothing to show */
}

void CHtmlSys_mainwin::offer_game_chest()
{
    /* game chest isn't used in this build, so don't even offer it */
}

void CHtmlSys_mainwin::save_game_chest_file()
{
}

void CHtmlSys_mainwin::write_game_chest_file(const textchar_t *)
{
}

void CHtmlSys_mainwin::delete_game_chest_info()
{
}

void CHtmlSys_mainwin::process_game_chest_cmd(const textchar_t *, size_t)
{
}

int CHtmlSys_mainwin::game_chest_add_fav(const char *, int, int)
{
    return FALSE;
}

int CHtmlSys_mainwin::game_chest_add_fav_dir(const char *)
{
    return FALSE;
}

void CHtmlSys_mainwin::add_download(CHtmlGameChestDownloadDlg *)
{
}

/*
 *   Remove an active download dialog 
 */
void CHtmlSys_mainwin::remove_download(CHtmlGameChestDownloadDlg *)
{
}

int CHtmlSys_mainwin::do_close_check_downloads()
{
    /* 
     *   downloads are not possible in a non-game-chest version, so we can
     *   definitely proceed with the close as far as downloads are concerned 
     */
    return TRUE;
}

void CHtmlSys_mainwin::cancel_downloads()
{
}

int CHtmlSysWin_win32_Input::DragEnter_game_chest(
    IDataObject __RPC_FAR *, DWORD, POINTL, DWORD __RPC_FAR *)
{
    /* game chest is inactive, so this isn't our event */
    return FALSE;
}

/*
 *   continue dragging over this window 
 */
int CHtmlSysWin_win32_Input::DragOver_game_chest(
    DWORD, POINTL, DWORD __RPC_FAR *)
{
    return FALSE;
}

int CHtmlSysWin_win32_Input::DragLeave_game_chest()
{
    return FALSE;
}

/*
 *   Check for and process a game chest drop 
 */
int CHtmlSysWin_win32_Input::Drop_game_chest(
    IDataObject __RPC_FAR *, DWORD, POINTL, DWORD __RPC_FAR *)
{
    return FALSE;
}

