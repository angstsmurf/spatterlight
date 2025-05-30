# Change log

## Release 1.4.5
- Game windows would sometimes autorestore to minimum size.
- The animation when opening or closing game info panels would trigger the macOS Game Mode, along with an annoying notification.
- Adds full support for all versions of Infocom's *Shogun* and *Arthur: The Quest for Excalibur* with autosave, window resizing and VoiceOver support. All graphic formats work and can be changed on the fly.
- Adds a new option to redirect Bocfel and *Shogun* error window text to the main buffer window.
- Bocfel error windows could sometimes have incorrect colours in games that change colour on the fly, such as *Beyond Zork*.
- Double copies of margin images would be autosaved, one in `MyAttachmentCell`, one in `MarginImage`.
- The VoiceOver image rotor was broken.
- Strips more leftover junk characters when saving scrollback as plain text.
- The `subheader` Glk grid style text in the DOSBox and MS-DOS built-in themes is now white, which makes *Journey* more playable when using them. The subheader buffer style was already white, for some reason.
- The `loadimage` function in `glkimp` was broken and leaked memory when not using blorb files.
- Thanks to the new static analyzer in Xcode 16.3, several file resource leaks (i.e. `fopen()` without a corresponding `fclose()`) were discovered and fixed.
- Resetting a game will work even if the game file has been moved or renamed.
- Reduces the number of temporary files created when playing *Journey*.
- Autorestoring a game during the "Spatterlight wants to access files in the desktop folder" permission dialog could leave a "[Game name] is taking a long time to load" alert that would not go away.
- After making a selection in the *Journey* VoiceOver menu which opens a dialog, VoiceOver would sometimes focus on a non-existent top level element ("Alert, dialog") that could not be properly navigated.
- VoiceOver will now speak text printed between dialogs in *Journey*, that it previously skipped, by adding it to the dialog text. It will also properly speak text printed after closing a dialog.
- Pushing a Settings menu bar button using VoiceOver will now properly select it.
- The Arrow key usage setting does now default to "Replaced by ⌘↑ and ⌘↓" in all built-in themes. You may have to select Rebuild Default Themes for this to take effect.

## Release 1.3
- Fixes the decoding of certain characters in game descriptions downloaded from IFDB.
- Fixes reading files managed by the File Provider API, such as those on Google Drive. Mostly by opening a lot of dialogs asking the user for permission to read files.
- More file access happens on background threads, so if a file is offline (or on a very slow external media or local network) the app won't be unresponsive while it downloads. If the access takes more than a couple of seconds, perhaps because there is no internet connection, a dialog will appear asking if the user wants to cancel the operation.
- A blank window would appear if a file had become inaccessible or deleted between starting a game and resetting, or between closing the app and autorestoring at startup. If a game file can't be found on reset, a dialog will now inform about this before closing the window.
- The single-blorb version of *Journey* now displays graphics again.
- Quote boxes in Z-code games were not properly autorestored. They would also sometimes fade away too soon during play. 
- The game window was allowed to have zero height.
- The "Add games to library" button and menu item stayed greyed out after importing games on Sequoia.

## Release 1.2.7
- Fixes VoiceOver on macOS 15 Sequoia by adding and increasing some delays.
- Adds improved support for *Journey*. It is now possible to resize the window and change graphics mode and interpreter number on-the-fly. All known versions and graphics formats are supported. Also adds elaborate VoiceOver support with menus and dialogs.
- Documents VoiceOver support in the file [ACCESSIBILITY.md][accessibility]
- Fixes mouse support in Bocfel for the v6 games.
- Extends arrow key settings originally written for *Beyond Zork* to the v6 games.
- Makes the Old settings theme possible to delete.
- Duplicating a theme set as designated dark or light theme would make the copy designated dark or light theme as well.
- Fixes a problem where the Z-machine interpreter number could be set to an illegal value.
- Keeps track of moves even when VoiceOver is off.
- The icons (not found, playing, paused, and stopped) in the status column of the games list are centered.
- Turning autosave on and off during play now should work as expected.
- The timer slider in settings is more accessible, with a correct value description.
- Fixes sorting of game titles in the games list. Titles with numbers somewhere in the middle will now sort just like in the Finder. 
- Adds support for image descriptions in external blorb files.

[accessibility]: https://github.com/angstsmurf/spatterlight/blob/master/ACCESSIBILITY.md "Documentation for using VoiceOver with Spatterlight"

## Release 1.2.5
- A save file with a garbage single-character name would be written to the user directory when a save request was cancelled.
- Limits the duration of smooth scrolling.
- Adds a preference to open the save dialog in the game file directory by default.
- The VoiceOver Command History rotor would be confused by char requests, and by Glk graphic windows.
- Thumbnailer extension would generate a crash report when no Core Data library was found.
- The library view "Like" buttons have more descriptive accessibility labels.
- Improves VoiceOver feedback when stepping through the command history (not to be confused with the Command History custom rotor).
- *Junior Arithmancer* is no longer detected by VoiceOver as showing a menu.
- Menu detection now works even if the game window is large.
- Improves menu detection in *Vespers*.
- Switching off menu detection manually would sometimes not have any effect until restart.
- New settings for when to interrupt VoiceOver. When a game window gets focus, the standard behaviour of VoiceOver is to start speaking the entire scrollback from the very beginning, which can be a lot. We have always tried to interrupt this by speaking just the text of the latest move instead, but there are now controls to set how long to wait before the interruption, and also a way to turn this hack off entirely.
- When selecting the preferences panel, VoiceOver would speak the sample text even if it was hidden.
- Spatterlight's invisible "more prompt" mode was confusing to VoiceOver users, and is now disabled while VoiceOver is active.
- When multiple games were running, VoiceOver would sometimes speak text from backgrounded games.
- Default name of input recording files would be named "Recordning[sic] of [game title]".
- The "Add to library" option when opening a game file was truncated.
- Unrelated error messaged would sometimes be displayed on game crash.
- Adds patches that fix crashes in the Z-code games *Transporter* and *Unforgotten*.
- Turns on abbreviation expansion in Scare, which fixes *The Cellar* and maybe others.
- Updates Glulxe to 0.6.1, with an improved random number generator.
- Updates Bocfel to 2.2. Includes fixes for missing text in *Trinity* and *Beyond Zork*, and improved support for version 6 games.
- Updates Magnetic to 2.3.1. No functional changes.

## Release 1.1
- Themes can be designated for use in Dark and Light mode, overriding all other theme settings. New checkboxes and menu items in the preferences window are added for this.
- Margin settings in the preferences work correctly again.
- Authentic flicker option for the upper window in ScottFree. Some original interpreters would flicker visibly whenever the player was temporarily transported between rooms for programming reasons. There is now a checkbox for this in the Format preferences tab.
- AGT games show the unicode equivalents of MS-DOS symbols.
- Increased undo limit in TADS games.
- Undo and transcript improvements in the ScottFree, TaylorMade and Plus interpreters.
- It is no longer possible to undo into the intro of *Kayleth* ("Do you want a sneak preview?")
- Battles in *Seas of Blood* are printed to the transcript.
- The C64 palette of *Seas of Blood* is now closer to the original.
- One C64 version of *Questprobe featuring The Human Torch and The Thing* was broken, preventing The Human Torch from flying over the tarpit.
- Some images in the Apple 2 version of *Questprobe featuring The Human Torch and The Thing* were mixed up, showing at the wrong moments.
- ZX Spectrum *Buckaroo Banzai* had broken directions.
- One animation in *Kayleth* (the firing android) was missing. Other animations are now closer to how they looked on the original hardware, and the animation code is a little more readable.
- Energy display in *Rebel Planet* said "0" where it should say "00".
- Multiple waiting in the TaylorMade games should now match the originals. It actually waits one extra turn, but won't print a message for the last one.
- The *Questprobe 4* fragment should work as well as in the original interpreter.
- TaylorMade now gives the same error messages as the originals: One of three depending on whether it understood none of the input words ("I can’t compute that... sorry!"), only understood a noun but no verb ("Please try varying that verb"), or found a verb but no matching command ("That’s not possible.")

## Release 1.0
- New preferences panel design with standard toolbar
- The "Cover image" and "Animate scrolling" settings have moved from the Misc tab to the Details tab, because I decided that Details should be "presentation" settings while Misc is "under the hood" settings
- The Format tab no longer has sub-tabs, but now has settings for both Z-machine and Scott Adams games on the same tab
- Shows a "paused" status icon next to autosaved games in the game list, to complement the existing "playing" and "stopped" status icons
- The library window has a subtitle which shows the number of games in the library
- Support for output and input of Unicode glyphs outside the Basic Multilingual Plane
- Output characters could in some situations be randomly erased
- Opening of d$$ format AGT files was broken
- Certain Commodore 64 games were recognised but would not start
- Implements Glk gestalt_CharOutput check for printable characters
- Adds ram save and ram restore to the Plus interpreter
- Playing Plus games without graphics files could make the status window blank
- Transcripts in Plus, Scott and Taylormade no longer have hard line breaks where the lines ended on-screen in room descriptions
- Info windows could be left behind after deleting library
- Fonts could be impossible to select under certain circumstances
- Font panel and colour picker are no longer restored when restarting Spatterlight
- File extensions were missing their last character

## Release 0.9.9
- Now requires macOS 10.13 or later
- New view-based table view with updated design
- Table view is less eager to scroll automatically
- Adds a Like column
- Adds a main Game menu, for things that previously could only be done by right-clicking in the table view
- The Format column was not sorted properly
- The side view was not always updated when the game on display was deleted
- Autorestoring at the first turn could add another layer of text views on top of the existing. This would be autosaved, then another layer would be added if the game was immediately closed and opened again, and so on
- Fixes a memory leak when interfacing with Babel
- Fixes a memory leak when downloading info from IFDB
- Fixes a memory leak when reading Commodore 64 disk images
- The ScottFree, TaylorMade and Plus engines code is a little more cross-platform friendly
- Adds a line delimiter between the upper and lower windows in Plus
- Implements inventory in the upper window in Plus
- Adds support for more ScottFree and TaylorMade game variants found on the Internet Archive
- Adds support for the unfinished SAGA Plus game fragment *Questprobe Number 4: X-Men*
- Fixes picking up the cannon in the TaylorMade version of *Questprobe featuring Human Torch and The Thing*
- Fixes a problem where TADS files might not be openable in some cases if Gargoyle is installed on your system, and a similar problem with Magnetic Scrolls files
- Resizing the window in a ScottFree game when viewing a closeup image will no longer switch to the room image
- Both disk image files of a pair are now usually recognised and will open the correct game in Plus, just like in ScottFree. Previously only side A files worked in Atari 8-bit games and only side B in Apple 2 games, which was kind of confusing
- Some images in the Apple 2 version of *The Sorcerer of Claymorgue Castle* were cut off
- Reading disk images in .woz format is a little faster
- The Quick Look plugin should be more stable and less jittery during resize
- Updates Bocfel to version 2.1
- Works around a crash that could happen when autorestoring a game with open text search box
- Updating the database from much older versions should work more reliably
- Background and text of a couple of icons has been corrected. Quest 4 games had no text and generic icons had no moon

## Release 0.9.8
- Adds support for some of the graphics used in the S.A.G.A. (Scott Adams Graphic Adventures) releases of the Scott Adams games. The bitmap formats used by Apple 2, Atari 8-bit, IBM PC and Commodore 64 are supported. Not yet supported are the line drawing graphics of the earlier releases or the tile-based graphics of *Return to Pirate's Isle*
- Adds support for reading ScottFree games from Atari .atr disk images and Apple 2 .dsk and .woz images
- Fixes a bug that made *Questprobe featuring The Hulk* impossible to win
- *The Hulk* would print the wax image at the wrong position
- Adds many new ScottFree and Plus variants, including a Spanish Commodore 64 *Gremlins*
- ScottFree: Patching of broken ZX Spectrum images is functional again
- ScottFree: TAKE ALL wouldn't print object names in dark rooms with a light source
- ScottFree: TAKE ALL and DROP ALL would in some cases not print a line break between objects
- ScottFree: It is now possible to use multiple words to refer to a noun even when one of the words can also be used as a verb, such as TAKE NAIL FILE when FILE is a verb
- Fixes a potential out-of-bounds read that could randomly cause TI-99/4A Scott Adams games to only display a blank screen
- "Parameterized" TADS 3 text colours would be displayed as red
- Changing the "Slow vector drawing" option could cause games to crash
- Capped the speed of animated scrolling a bit. It is less dramatic when scrolling longer texts

## Release 0.9.7
- Adds support for Apple 2, Atari 8-bit, Commodore 64, and Atari ST versions of Saga Plus games
– Line spacing and error messages in Plus are closer to the original interpreters
- Saga Plus *Buckaroo Banzai* would not restore saved games
- Undoing after death in Saga Plus would sometimes undo two turns
- Adds TaylorMade support for Commodore 64 versions
- TaylorMade now understands chained commands, such as DROP FUSE AND GLOVES THEN GO WEST, just like the original interpreter
- TaylorMade support for inventory in the upper window

## Release 0.9.6
- New experimental engine: Plus. This is an interpreter for the Saga Plus game format of the later Scott Adams games. Currently supports the MS-DOS versions of *Questprobe: Featuring Spider-Man*, *The Sorcerer of Claymorgue Castle*, *The Adventures of Buckaroo Banzai*, and *Questprobe: Featuring Human Torch and the Thing*
- Updates Glulxe to 0.6.0
- Changed cover images in Blorb format games will now be detected and added to the Spatterlight library, unless Don't replace images is set in the preferences
- The QuickLook plugin will show any cover images inside Blorb files rather than those in the Spatterlight library
- Fixes out-of-bounds reads in ScottFree, TaylorMade and Hugo
- Quickly browsing through info windows using the arrow keys could leave stray info windows behind

## Release 0.9.5
- New experimental engine: TaylorMade. This is an updated version of Alan Cox's [command-line interpreter](https://github.com/EtchedPixels/TaylorMade). It supports the ZX Spectrum versions of the six non-Scott Adams engine Adventure International UK games: *Questprobe: Featuring Human Torch and the Thing*, *Rebel Planet*, *Blizzard Pass*, *Temple of Terror*, *Masters of the Universe: The Super Adventure*, and *Kayleth*. All games are completable with working graphics, and a couple of them have animations and beep a little
- Updates Bocfel to 2.0
- Updates AGiliTy with changes from 1.1.2 that fixes some synonyms and hides an error message in *Shades of Gray*
- ScottFree would sometimes try to write an external debug file, causing crashes
- Automatically fixes lamp behaviour in DAT versions of the Mysterious Adventures
- All interpreter-added commands in ScottFree, such as TRANSCRIPT and RAM SAVE, now have synonyms starting with a hash sign (#TRANSCRIPT, etc) to avoid conflicts with original game commands
- Adds FIAD file handling to the QuickLook plugins

## Release 0.9.4
- Fixes a bug where closing a window with a pending error message would make it immediately reopen
- Fixes hairline artifacts when drawing rectangles in graphics windows, apparent in the ScottFree images
- Support for vector graphics in Brian Howarth's 11 Mysterious Adventures
- Support for Commodore 64 versions of the Mysterious Adventures
- Implements an option to draw vector images slowly, similar to the originals (but non-blocking)
- Fixes repeated undo in ScottFree games
- Adds file types for the recently supported formats (.dsk and .fiad)
- The title screens of TI-99/4A games are properly centered (if the window is large enough) and some broken characters in them are fixed

## Release 0.9.3
- This is an Apple App Store-only hotfix release that fixes a crash that only occurred in the App Store version of Spatterlight

## Release 0.9.2
- ScottFree support for Commodore 64 and TI-99/4A Scott Adams formats, including homebrew TI-99/4A games. Supported file extensions: .d64, .t64, .dsk, .fiad.
- Fixes a bug where game windows would reopen if they were closed too soon after opening
- Fixes a bug where a game window would move to the front if you switched to another window too soon after opening it
- Fixes a bug where multiple simultaneous file picker windows could open when the "Show library window automatically" setting was off
- Works around a bug where ScottFree games could sometimes show graphics and room description in the top window and accept input, but never print any text in the bottom window
- Fixes many bugs related to undoing and restarting ScottFree games
- Reduces the number of repeated room descriptions in ScottFree transcripts
- Attempts to fix a bug where the end of transcripts were sometimes missing after closing a game
- New settings for graphics palette, delays and inventory in ScottFree games
- Implements determinism in JACL
- Z-machine games would ignore user-defined background colour
- The ZX Spectrum version of *Arrow of Death part 1* now works

## Release 0.9.1
- Preliminary support for ZX Spectrum format ScottFree games with graphics
- Improves animation of game info windows
- Adds a side bar toggle button to the library window
- The animations shown when entering fullscreen should flicker less
- When starting in fullscreen with a cover image in "show and wait for key" mode, the game text view could be too narrow
- When Spatterlight was force-quit or crashed, and then restarted, some autorestored games would have a too-narrow text view
- Improvements to automatic scrolling. Now less likely to scroll in the wrong direction
- Buffer windows could open with the wrong background colour
- Switching off "Games can set colors and styles" could leave some text behind with the old background colour
- Lots of AGiliTy fixes from David Kinder. *The Multi-Dimensional Thief* is now completable

## Release 0.9.0
- Two-line menus in TADS 3 were no longer detected by the VoiceOver code since 0.8.6
- Core Data concurrency fixes should improve speed and stability when importing games and downloading metadata in the background
- Downloading metadata for multiple files is now multi-threaded and a little faster 
- Fixes sandboxing issue that could break reloading images from Blorb file
- The *Beyond Zork* graphic font got lost in the last update. Now it is back
- Spatterlight should be slightly less eager to show a file dialog at inappropriate times
- Updates Bocfel to 1.4
- Adds support for Quest 4 games, using the Geas interpreter
- Adds support for Scott Adams games, using the ScottFree interpreter
- Adds a JACL interpreter. Spatterlight should now be able to play anything that Gargoyle can
- Fixes a bug where games would open to a blank window after resetting, as their autosave files were not properly deleted
- Detects all UUID format IFIDs in Hugo games correctly
- The menu in *Halloween Horror* was broken
- The library window spinning progress indicator would disappear on Yosemite
- Fixes a sandboxing problem where the "Reload from Blorb" image context menu item would stop working
- *Unforgotten* would send garbage strings as preloaded input
- Switching off fancy quote boxes had no effect
- Spatterlight metadata is now searchable in Spotlight
- Improves QuickLook extension. More information shown in Spotlight search results. Displays Gargoyle icons with crisp, large pixels

## Release 0.8.9
- Keypresses could be lost between commands
- Adds a preference to not add games to library when opening them through the Open menu or by double clicking a file associated with Spatterlight. This option is added both to the Open file dialog and to the preferences panel
- Adds a preference to re-check the library periodically for game files that have been deleted. This may be annoying if your game library is spread across external disks and network volumes, so it is off by default
- Adds a Verify Library menu item, shortcut ⌘L, to immediately check for and optionally delete library entries with missing files
- In previous versions, the library window would open by default if no other window were open and the Spatterlight dock icon was clicked. This behaviour is now off by default, but can be re-enabled in the preferences panel
- The preferences panel has a new tab, Global, to accommodate all these new settings. All settings here are independent of the currently selected theme
- The Found table column, the leftmost column that shows an exclamation mark if a game file is missing, can now be hidden in the header contextual menu
- If no windows are shown at startup, a file dialog will open
- Fixes a color issue in *Codex Sadistica*

## Release 0.8.8
- Security scoped bookmarks were stored with write-only access. This broke some TADS games, which try to to write external files
- When starting Spatterlight by double clicking on a game file, a blank game window would sometimes obscure the file access dialog

## Release 0.8.7
- TADS games work on Apple Silicon
- Improves graphics resizing on window resize in many Glulx games
- Navigation with numeric pad works again in *Beyond Zork*
- Navigation with mouse now works in all versions of *Beyond Zork*
- Images in *Waldo's Pie* are now shown if the image files are in the game folder
- No longer beachballs after importing a lot of metadata from iFiction files
- Autosaved games would break if the game file was moved to a different disk drive
- Autorestoring when preloaded input is shown works properly
- The project can be compiled on other computers than mine again
- Replaces the text buttons (Add, Info, Play) in the library window with symbols
- The center and right paragraph justification buttons were mixed up
- The DOSBox theme background blue color differed between text and images

## Release 0.8.6
- Replaces SDL audio with a modified version of SFBAudioEngine
- Beachballing at the beginning of *Renegade Brainwave* fixed
- Spatterlight is now a universal app, with native Apple Silicon support
- Some paragraph settings were not being properly updated in the Preferences panel
- Adjusts the limits of many paragraph settings
- The paragraph settings scale along with text size when zooming in or out
- Adjusts the Automap theme. This may require rebuilding default themes after upgrading
- Border adjustments in some of the built-in themes
- Hyperlink underline setting was not working in buffer windows
- Hyperlink underline setting in grid windows was not working when starting a new game
- Improves calculation of character height and status window height
- The top of the main window was covered by the status bar in *Curses* when showing a quote box. This was visible when the main window had a Find bar
- Determinism now works in Tads 3 and Magnetic Scrolls
- Improves performance in Alan 3. *The Wyldkynd Project* now has playable speed and can be resized without beachballing
- The SCRIPT command works as intended in Alan 3
- All Alan 3 games should be recognized and playable
- Tads 3 status bars have correct width and height
- Characters in Tads 3 menus are now invisible when they should be
- When targeting multiple games with the Apply Theme context menu item, all currently used themes have a checkmark
- Spatterlight now tries to use the normal style color for the text cursor, which tends to look better than the input style color
- When changing any setting, reverse style text color in buffer windows would switch from buffer background color to *grid* background color
- Changes default fixed-width font from Monaco to Source Code Pro. This may require rebuilding themes after upgrading
- Hacks around Hugo *Tetris* and *Cryptozookeeper* breaking at small window sizes
- *A Crimson Spring* did not erase old pictures after restore
- Changing border color manually was not working in some Hugo games without buffer text windows
- Fixes dollar signs in *Shades of Gray* death message

## Release 0.8.5
- New [Automap](https://intfiction.org/t/unicode-characters-in-textgrid-windows/51825) theme.
- Setting to switch off hyperlink underline
- Plain text option when saving scrollback
- Find bar would only appear half the time when pressing Command + F
- Disables Change orientation in text view
- Exporting metadata for games in library only was broken
- Select same theme action would crash when only one game was visible
- More sensible limits to text margins in preferences panel

## Release 0.8.4
- Fixes a bug that could add an extra file extension to downloaded Z-machine story files
- Reinstates Bocfel 1.3.2
- Fixes the autorestore bug that made Bocfel 1.3.2 unstable
- Fixes determinism after autorestore in Bocfel and Glulxe
- Fixes command script handling in Bocfel and Glulxe
- Determinism support in all interpreters
- Improved look for bezels displaying two-figure numbers
- Status bar in Stationfall and Planetfall correctly shows "Time" instead of "Moves"
- Reset should work more reliably after a game crashes
- Support for IFID-less Zoom iFiction files
- In "Apply theme" contextual menu, currently selected theme now has a checkmark
- Double clicking table view header no longer starts selected game

## Release 0.8.3
- QuickLook plugin blorb support was broken
- Drag support for all image views, including inline images
- Mouse cursor changes appropriately over margin images
- Drag, drop and contextual menu for cover image in "Show and wait for key" mode
- "Show and wait for key" background color changes when border color does
- Huge images are shrunk when imported
- Improved look for Game Over bezel
- Turning off automatic border color keeps the last automatic color
- An option to switch off bezel notifications (Cover image bezels will still show)
- Newlines can be entered normally when editing game description in info window
- Fixes erratic selection behaviour when clicking between fields in info window

## Release 0.8.2
- Adds a contextual menu to the cover image views in info windows and the library sidebar
- Using this contextual menu, a new image can be downloaded or loaded from disk
- Image filering can be toggled on or off
- Cover image descriptions can be added and edited
- Cover image descriptions are included when exporting metadata 
- Image views are once again accessibility elements and will speak their description
- New option for setting the border color manually
- New bezel-style notifications for feedback when downloads fail, when games end, and when resetting alerts and custom styles
- Alternative modes for showing the cover image at game start: show and fade, and show as bezel notification
- Improved support for split screen mode
- Cover images in the library sidebar would sometimes lose their aspect ratio after restart
- The Preferences panel would erroneously show part of the theme preview after restart
- Cover images will retain their aspect ratio in "Show and wait for key" mode when resizing the window on 10.10
- Placeholder text now works as expected in info window text fields
- The description field in info windows no longer scrolls, but will show full text when editing
- The Narcolepsy window mask would sometimes break when exiting fullscreen
- Mass adds would be cancelled after a single parallel download
- Multiple "Unknown format" alerts would sometimes be shown when looking for missing files

## Release 0.8.1
- Many bugfixes related to changing Glk style attributes using the font and color panels
- Typography panel now works even if the preferences panel is not at the front
- The preferences panel has been reorganized and enlarged
- The Styles preference tab has been split into two. The new one is titled "Details".
- "Overwrite styles" will now overwrite custom input styles
- "Overwrite styles" is now a button titled "Un-customize styles"
- After zooming, an incorrect font name was sometimes displayed in the preferences panel
- The 10.10 Yosemite version would not start if there was no Spatterlight directory in Application Support
- Fixes a crash that would occur when pasting during a character request
- When using the contextual menu on a buffer window, text input could stop working
- The initial quote box in An Act of Murder would reappear erroneously after autorestoring at turn 0
- Spatterlight will offer to download metadata from IFDB at start if the user has no downloaded metadata
- Games with no metadata will have a download button in the side view
- When adding games, there are options to look for cover images and download metadata. When the former is active, Spatterlight will look for image files in the game directory, and in subdirectories named "Cover."
- There is a new option for policy when downloading cover images with three settings: always replace existing images (the old behaviour), never replace, or ask the user each time
- Cover images can be added to games by dragging image files to the side view or info window, or by pasting into an image view
- Cover images can be dragged from Spatterlight to the desktop or other applications
- Cover images can be copied and pasted
- There is an option to show the cover image before a game starts
- Support for MG1, the MCGA image format used by the MS-DOS Z-machine version 6 games
- Support for NEOchrome, the original Atari ST Beyond Zork title image format (only the original Atari ST release had a title picture)
- The original windowed size is now retained when starting Spatterlight in fullscreen
- When starting in fullscreen, Spatterlight is less prone to immediately switch to the library window non-fullscreen space
- The toggle sidebar action was broken
- "Prune library" was only available when the library window had focus
- Pruning the library will also prune orphaned images
- Glulxe will quit on fatal errors instead of crashing when the "Ignore errors" option is active
- An appropriate error dialog is shown when trying to start a game that has been moved to Trash
- Library search will only match if every word in the search string is present. Searching for "The Pawn" will no longer list every game that contains the word "The."

## Release 0.8.0
- All Glk styles can be customized
- Many new character and paragraph settings. Some of these may be overridden by style hints issued by certain games, unless the Colors and styles option is off
- Most settings on the system font panel (underline, strikethrough, shadow) should be functional
- Most setting on the typography panel (available through the font panel) should be functional. These are font-specific OpenType settings which many fonts don't support at all
- Separate settings for horizontal and vertical margin spacing
- Setting to turn off quote box special handling in Z-code games
- Setting for reproducible random numbers, implemented in Bocfel, Glulxe, and Level 9
- Setting to hide error messages or quit on errors, implemented for errors in the common Glk API, Bocfel, and Glulxe
- Support for command scripts, text files of newline-separated commands, which will be entered at the game prompt sequentially if copied or dragged to the game window
- Support for *Narcolepsy* "thought balloon" window mask
- Lengthy import and metadata download operations for multiple games can be cancelled
- Fixes margin image y position regression
- VoiceOver support for *Unforgotten* main menu
- VoiceOver no longer counts the instruction line in the *Vespers* help menu as a menu item
- Size and position of window content are preserved when border size is changed, if the Adjust window size option is active
- Preserves window position when resetting

## Release 0.7.9
- Bocfel 1.3 made autorestoring unstable, so we revert to 1.2 for now. This breaks autosave backward compatibility again
- Backports sound and other fixes from Bocfel 1.3 to 1.2.

## Release 0.7.8
- Breaks autosave backward compatibility
- VoiceOver describes images
- Reads image descriptions from blorb files
- Running games can be deleted from library
- Info windows are closed when their game is deleted
- Bocfel updated to 1.3.2
- Improved The Lurking Horror sound support
- Babel updated to 0.5
- Fixes a potential crash when importing games
- Fixes hard-to-read preferences text in dark mode
- Smooth scrolling
- Menu action to reset built-in themes
- Zooming zooms all styles in theme
- Bottom line in grid window was sometimes cut off while zooming
- Maintains window aspect ratio when zooming
- Fixes smart quotes and smart dashes
- Fixes map in Vain Empires
- Fixes Mad Bomber glitch

## Release 0.7.7
- Fixes starting up when no old library is present.
- Fix regression with text background colors, apparent in at least Bronze.
- Animate opening and closing info windows.
- Open and close info windows using space bar, in an attempt to approximate the behavior of QuickLook in Finder.
- Fix buggy mouse behavior in grid (status) windows.
- Make Core Data value transformers secure.
- Don't leak two windows every time we enter and exit full screen.

## Release 0.7.6
- Fixes crash when autorestoring saves made when there was no current Glk stream.
- QuickLook extension for many related file types. Shows useful information and cover images. For save files and data, it tries to guess which game created them. Available on macOS 10.15 Catalina and later only.
- QuickLookThumbnailing extension. Creates icons for game files from cover images. Catalina and later only.
- Accepts Z-code files with .z1 and .z2 extensions. These have always worked, but previously had to be renamed.
- Reads cover image descriptions from Blorb resources and adds them as accessibility labels.
- Fixes to file detection and identification. No longer misidentifies the serial-number-less *Suspended* as *Zork I*.
- Once again possible to build source without modification on other computers than mine.
- Use Big Sur alert sound names in preferences when running on Big Sur.
- Updated URLs to support image downloads from new IFDB site.
- High resolution icons.

## Release 0.7.5
- Fixes to Bureaucracy forms.
- Some invalid files could slip through the detection code.
- Added games could get a blank title.
- Always scroll down when a game quits so that the final text is visible.

## Release 0.7.4

- Autosave and autorestore for Z-code games. (Previously this was only implemented for Glulx games.)
- Quote boxes in Z-code games are now nicer-looking separate text views, floating over the main scroll view.
- Setting to turn off autosave.
- Setting to turn off autosave on timer events.
- Setting for maximum timer frequency.
- Preferences button to reset alert suppression.
- New preferences tab with Z-code related settings.
- Setting to select sound effects for the Z-code "high" and "low" beeps. Only the standard macOS system sounds are available for now.
- A setting for the interpreter version (number and letter.)
- A setting for whether the arrow keys in Beyond Zork will navigate windows (as in the original interpreter) or step through previously entered commands (as per usual in Spatterlight.)
- Setting for vertical adjustment of the graphic font in Beyond Zork. This can be useful to avoid gaps and overlap. No horizontal adjustment is possible yet.
- New preferences tab with VoiceOver-specific settings.
- Speak menu lines. On by default. Detect menus and speak currently selected line when VoiceOver is active. The default setting is Text only: only the actual text of the line will be spoken, not its index in the menu or the total number of lines. When a menu is first detected, these numbers will still be read once, along with the title of the menu and instructions for navigation. All of these except the title will also be read when the line is repeated with the Repeat last move action. Menu detection can be switched off entirely here as well.
- Speak commands. On by default. When this is switched off, VoiceOver will not speak entered commands during normal play. Also, keypresses and words entered in the *Bureaucracy* form will not be spoken. If the "While typing speak:" setting in VoiceOver Utility is set to Nothing, there will be no VoiceOver feedback at all while typing commands, except that the first character is still spoken sometimes in a randomly annoying way.
- VoiceOver will now look for new text to speak in all Glk windows.
- The Repeat Last Move, Speak Previous and Speak Next commands work in all windows.
- Custom VoiceOver rotors (VO + U) for previous commands, Glk windows, links and free text search (VO + F).
- Long lines of underscore characters, as seen in *1893: A World’s Fair Mystery*, are trimmed from spoken text (rather than spoken character by character.)
- The Save Scrollback action (to save window text as RTF files) now works in every window. If there is no buffer window with text, the contents of the grid window will be saved instead.
- Fixes a crash that would occur on startup when there are no previous settings present.
- Fixes a crash that would sometimes occur when closing *Pytho's Mask*.
- The bottom menu in *City of Secrets* would become invisible after exiting full screen.
- All sound code is now using SDL Mixer. SDL Sound is no longer used. Thanks to mstoeckl who wrote this code for Gargoyle.
- Reworked sound code to improve performance and fix a bug that could cause crashes or leave CPU-heavy orphan processes running when a game window was closed.
- Includes Bocfel patches by Chris Spiegel for *Beyond Zork* and *Robot Finds Kitten* (which would previously crash), and fixes for status line display of time and negative numbers.
- *The Meteor, the Stone and a Long Glass of Sherbet* and *Tristam Island* are now playable (by ignoring errors.)
- Adds command history to line input in the status window.
- Improved pasting into line input. ⌘C + ⌘V will now directly paste selected text (such as a previous command) into the input line.
- Updates Alan 3 to beta 8.
- Updates Bocfel to version 1.2
- Updates Glulxe to version 0.5.4.147
- Updates Babel to version 0.3

## Release 0.6b

- Ignore Beyond Zork object out of range error
- Fixed line input in grid text windows for real this time
- Numeric keypad navigation in Beyond Zork
- Input would not be read if Enter was pressed after a dead key (such as ~ or ^)

## Release 0.5.5

- New theme based on the Montserrat font
- Mouse clicks in Beyond Zork now still work after restarting
- Fixes to line input in grid text windows, mainly used by My Angel and Beyond Zork. Input length limit is now respected
- Adjusted maximum width of some table headers in the game list view. All headers should be readable while selected
- Less flicker in status window when changing settings
- Graphics windows in Level 9 and Magnetic Scrolls games now adjust their height to the image
- The images in Level 9 games are less tiny
- Fixed a crash that could occur when editing game metadata while mass-downloading or mass-adding games in the background
- Games could crash when trying to play Infocom beeps
- Many broken menu items were enabled when showing the theme preview
- The warning alert about games being autorestored was broken
- Font license window is less wide

## Release 0.5.14b

- Added color swap buttons to the preferences panel
- Future Boy window resizing is less broken, and it has less other glitches
- Changes to the UI, such as window resizing, text entry, and scroll position, are now autosaved even if they happen after the interpreter autosaves, or on the first turn, before the first actual autosave of the game
- More robust autosaving with stricter version control. In principle, only one UI autosave file can be used with one particular interpreter autosave, and  that is now enforced properly
- Entering and exiting fullscreen should be less error-prone
- Images are now cached, although there appears to be no performance difference
- Finished games will still respond to some preference changes, such as fonts and colors
- Eleminated flicker in TADS 3 status bar
- Partial text color support in TADS 3
- Less flicker in games that add and remove windows a lot, noticeable in Cryptozookeeper
- Game detection is now case-insensitive
- It is again possible to set status window height to zero. This fixes gntests.z5
- Prevented side view text from changing color when selected in dark mode
- Made Save scrollback and Clear scrollback work more reliably. Save scrollback now saves readable text also in dark mode
- Added the font license to the Help menu

## Release 0.5.13b

- Improved Hugo support, with working images, sound and colours
- Bocfel is now the Z-machine interpreter
- Sound support for The Lurking Horror and Sherlock: The Riddle of the Crown Jewels
- Support for Gargoyle Glk extensions for text colour and reverse video
- Mouse support in Beyond Zork
- New graphic font for Beyond Zork borrowed from Frotz
- Support for preloaded input
- All TerpEtude tests pass
- Time in Cutthroats shows 99:99 where intended
- Improved form entry in Bureaucracy
- Full support for Robot Finds Kitten and other Z-machine abuses
- Alan 3 was updated to beta 6, and more games now work
- Image support in Alan 3
- Improved performance when scrollback is large
- Improved performance when displaying large images
- Correctly identifies more Level 9 games
- Play Game menu entry and shortcut now work
- Transcripts can now be turned off and autorestored without crashing Glulxe
- Renamed "Transcript" to "Scrollback" in menu items to avoid confusion
- Fixed the zoom button functionality of the preferences window
- Fixed sorting on Last modified column in library
- Annoying beeps in Infocom games supported
- Sound is silenced immediately when switched off in preferences
- Fixed a crash that would sometimes occur when resetting with the Alt+Command+R shortcut
- Some games (such as Hugo Tetris and Hugo Zork) would stop working after adding their IFDB metadata 
- Themes would sometimes be cloned twice when editing a non-editable theme
- Images dragged to the info window are now stored properly as a new cover image

## Release 0.5.12b

- Library converted to use Core Data
- Game presentation in library window side bar
- Metadata download from ifdb.tads.org
- Themes
- Per-game settings
- Themes preview in preferences window
- Game list view now indicates known missing game files
- User can search for and update missing game files
- Option to keep window size in chars constant when changing themes
- Fixed bug that could cause blank status bar on first turn
- Once again it is possible to scroll in the Adrian Mole games
- Hyperlinks for commands work in Dead Cities
- Improved status bar line input, as seen in My Angel
- Improved playability of A Colder Light, though some problems remain
- Status bars in TADS and other interpreters that use style User 1 now has correct background colour
- No longer spams empty folders in Application Support
- Waiting for key press to scroll (our more prompt-equivalent) is less buggy now
- Game titles sent by TADS games are used
- Games in .ulx format now autosave properly
- Undo support in the GUI interface
- Forgiveness setting is a pop-up menu
- Hyperlinks are no longer always blue
- Support for multiple releases of the same game with separate Ifids, though they will just look like duplicates in the library view.
- Cascading of newly opened (not autosaved) game windows, so that new game windows don't completely obscure previous ones
- Games are imported on background thread
- App is built on macOS 10.15 Catalina, notarized and runs all the way back to 10.9

## Release 0.5.11b

- Autosave and autorestore support for Glulx games, based on code from IosGlulxe
- Window restoration (except for non-Glulx game windows)
- Reset command to delete any autosave files and restart game (Alt+Command+R)
- Automatic conversion of curly quotes and non-standard dashes in input
- Improved retention of scroll position during resize
- Added standard key equivalent Command + Delete for deleting games from game library
- Fixed library view column toggle, which I inadvertently broke in the previous release

## Release 0.5.10b

- Added a perfunctory full screen mode
- Added perfunctory text zooming
- Disabled tab bar on 10.12+
- Added a variable padding space surrounding the game view, i. e. a border just inside the NSWindow borders

## Release 0.5.9b

- Added Speak Previous Move ( Alt+Command+Up), Speak Next Move (Alt+Command+Down) and Speak Status (Alt+Command+Backspace). These will only have effect when VoiceOver is active, and only on 10.9+
- Made mouse and hyperlink events in text grid windows accurate
- Fixed duplicate Edit menu
- Added fixes to make it compile and run on 10.7 as well as more recent systems

## Release 0.5.9a

- Turned text grid window into an NSTextView. Now grid window text can be selected, copied, and spoken just like the buffer window text
- Added "Speak Last Move" action. Only functional when VoiceOver is active
- New accessibility functionality, improved VoiceOver support
- Cleaned up project settings and added a Headers folder with symlinks to make the project compile out-of-the-box on most systems (not on Xcode 10 and later yet)
- Margin images are now inserted into saved rtf files
- Added a text search bar to buffer windows and license document window
- Made Find shortcut (Command + F) work in library window as well
- Made .glkdata and .glksave the default file extensions for external data and save files
- Fixed license document window sometimes mis-calculating line length, causing ugly line breaks

## Release 0.5.8b

- Fixed save file location. Previously saves always ended up in home directory
- Improved handling of multiple (unlimited number of?) margin images
- Now extends text buffer view downward when images stretch below text bottom
- Game-generated external files are saved in an appropriate Application Support folder
- Fixed unicode character input
- Image Test should be a little harder to crash
- Interpreter name is no longer in window title

## Release 0.5.7b

- Worked around buggy display of images

## Release 0.5.6b

- Fixed text buffer scrolling which was broken on newer systems

## Release 0.5.5b

- Implemented and improved many parts of the Glk library: advanced sound functions with working sound and volume notifications, hyperlinks, flow breaks, alternate line terminator keys
- Updated all interpreter cores.
- Implemented sorting by clicking menu column headers
- Implemented toggling menu columns on or off
- Added a contextual menu for library actions
- Now uses color values entered in the font panel
- Added a reader window for license documents under the Help menu
- Fixed text input in grid windows. Previously the y coordinate was flipped
- Fixed unicode input length. Previously the input was cut after half the requested length
- Open game on double click in table view, edit name on click-and-hold
- Added Open Recent menu
- Fixed black "scanlines" on images in Magnetic Scrolls games
