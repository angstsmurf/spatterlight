# Change log

## Unreleased
- Added standard key equivalent Commando + Delete for delete 
- Fixed grid view column toggle, which I inadvertently broke in the previous release

## Release 0.5.10b

- Added a perfunctory full screen mode
- Added perfunctory text zooming
- Disabled tab bar on 10.12+
- Added a variable padding space surrounding the game view, i. e. just inside the NSWindow borders

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
- Added text search bar to buffer windows and license document window
- Made Find shortcut (Command + F) work in library window as well
- Made .glkdata and .glksave default extensions
- Fixed license document window sometimes mis-calculating line length, causing ugly line breaks

## Release 0.5.8b

- Fixed save file location. Previously saves always ended up in home directory
- Fixed multiple (unlimited number of?) margin images
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



