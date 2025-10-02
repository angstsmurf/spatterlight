<img src="readme_images/Red-Sun-App.png" width="200">

# The Pectoral Skybreak Spatterlight

## Interactive Fiction for macOS

Spatterlight is a native Cocoa application that plays most parser-based interactive fiction game files: AGT, Adrift (except v5), AdvSys, Alan, Glulx, Hugo, Level 9, Magnetic Scrolls, Scott Adams, TADS (text-only), and Z-code. See [credits file][credits] for more details.

[credits]: https://github.com/angstsmurf/spatterlight/blob/master/resources/Credits.txt "Credits.txt: Credits for Spatterlight libraries"

Download the latest release [here](https://github.com/angstsmurf/spatterlight/releases)!

Then report bugs on the [issues tracker][issues].

Fixes for some common problems can be found in [TROUBLESHOOTING.md][troubleshooting].

[issues]: https://github.com/angstsmurf/spatterlight/issues "The issues page of this repository"

[troubleshooting]: https://github.com/angstsmurf/spatterlight/blob/master/ACCESSIBILITY.md "Work-in-progress list of common problems and their possible solutions"

<img src="readme_images/jigsaw.png" width="900">

# Features
- VoiceOver support. See [ACCESSIBILITY.md][accessibility] for documentation.
- Themes
- Per-game settings
- Easy download of game info from Ifdb
- Sounds, images and text colours
- Autosave and autorestore for Glulx and Z-code games

[accessibility]: https://github.com/angstsmurf/spatterlight/blob/master/ACCESSIBILITY.md "Documentation for using VoiceOver with Spatterlight"

#

A promotional video for Spatterlight 0.5.13b: https://youtu.be/KXbk_gpLQ8w

And another one for 0.5.12b: https://youtu.be/Y07jFFvjsnE

<img src="readme_images/library.png" width="1000">
<p align="center">
  <img src="readme_images/preferences_themes_2.png" width="600">
</p>

# Gameplay videos

Beyond Zork: https://youtu.be/GzYmywCm_24

Guilty Bastards: https://youtu.be/vXjZwwlFqyQ

Necrotic Drift: https://youtu.be/Um8z7X91S8U

Cryptozookeeper: https://youtu.be/1MkdVnhZRl8

Hugo Tetris: https://youtu.be/G-tTzXso6AA

Adventure International showcase: https://youtu.be/p5Oelhsmzco

Rebel Planet and Kayleth: https://youtu.be/RygPK26_e5Q

The Sorcerer of Claymorgue Castle (Atari ST): https://youtu.be/vvoFQ49VLgE

Questprobe 2 featuring Spider-Man (Atari ST): https://youtu.be/_wA7gEhTeZE

Questprobe: Featuring Human Torch and the Thing (Atari ST): https://youtu.be/IK_KpdVIOIg

Journey using VoiceOver: https://youtu.be/9jauDyUu6Ro

# Compiling

Building the current version has only been tried on macOS 15 Sequoia and Xcode 26. If you have success on earlier versions, please let me know! The resulting binary will still run on 10.13 High Sierra, though.

Clone or download the source. Open the file `Spatterlight.xcodeproj` in Xcode. Make sure that the target is set to `Spatterlight > My Mac`. Press the Build & Run button.

Note that the `master` branch has the QuickLook plugins disabled. To enable them, you must use the `release` branch, which requires at least setting your Apple Developer Group ID on all targets, and possibly other changes as well.

#

Spatterlight supports the [Treaty of Babel][babel] standard for cataloguing bibliographic information about interactive fiction.

[babel]: http://babel.ifarchive.org "Interactive Fiction Archive: Treaty of Babel"

The Spatterlight application is released under the GNU Public License; the interpreters and libraries it uses are freely redistributable and covered by their own specific licenses.

This is beta software! There is no warranty: use it at your own risk. You will need macOS 10.10 or higher for the latest version, but there are older versions compatible with older systems.

Spatterlight was originally written by Tor Andersson. Copyright 2007-2024 by Tor Andersson and the respective interpreter authors.
