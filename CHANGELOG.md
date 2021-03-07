# Change log

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
