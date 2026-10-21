# Change log

## Unreleased

### Bocfel (Z-code)
- The VoiceOver menus of the Infocom Z6 games read accented letters correctly, instead of decoding the game's Latin-1 text as something else.
- In *Journey*, VoiceOver no longer capitalises names wrongly ("AnnA" for "Anna"), the menus come back correctly when the game is autorestored, an empty menu title no longer crashes, and rebuilding the dialog no longer drops a character.
- In *Bureaucracy*, the VoiceOver form reading is fixed, and an instruction waiting to be spoken survives an error message.
- A menu read by VoiceOver no longer speaks the instruction for a key twice. Reading the menus of *The Unforgotten* with VoiceOver no longer crashes, and the *Journey* dialog no longer shows the last move twice.
- Random events continue the same way after an autorestore: the random number generator is saved exactly, where it used to be re-seeded from the new session.

### Scarier (Adrift)
- Some Adrift 4.0 games have ALRs written for the older take wording, which the 4.0 runner never prints. Scarier now keeps the older wording when a game's ALRs quote it (a deliberate deviation), so Hubert's retorts in *To Hell in a Hamper* appear, and the "already got" replacements in *Cursed* and a dozen other games fire. An ALR that contains itself is printed once.

### Plus
- The versions of the *X-Men* SAGA database other than the original can now be added to the library, as they now get an IFID of their own.

### Level9
- Typing a number works in every game: WAIT 25 failed in *Lancelot*, *Scapeghost* and *Ingrid's Back*, because the number was not stored the way the games expect. As in the original interpreter, a number on its own is refused by the older games.
- A failed RESTORE goes back to the state before the command, so a game that prepares for the restore first, as *Ingrid's Back* does, no longer carries on into the wrong part when the restore fails.
- Punctuation typed in a command reaches the game instead of being turned into spaces, and abbreviated words are resolved the way the original interpreter resolves them: fewer than four letters only count as an abbreviation when no later word starts the same way.
- Games detected as version 3 no longer list nonsense exits read from past the end of their exit table. Text after "!" is no longer wrongly capitalised, so *Ingrid's Back* prints "copyright" in lower case, as the original does.
- Pictures that are clipped to the window are decoded correctly in the PC, Atari ST, Amiga and Macintosh formats, where the rows used to drift, and a missing picture draws nothing, not even its frame. A game that chains to its next part stays in text-only mode.
- A message number past the end of the message table prints nothing instead of memory garbage, the object search can no longer overflow its stack, and the driver calls the interpreter doesn't implement answer the way the Level 9 driver documentation specifies, so a game's call to return to the operating system ends the game.

### General
- Quick Look previews and Finder thumbnails now cover every game type the library opens: Quest 5, Archetype, ZX Spectrum snapshots and tapes, Commodore 64 disk and tape images and Apple II disk images. Finder thumbnails were missing for most types, because macOS refuses the DAT type and dropped every type listed after it. The TAP tape type was registered under the wrong extension.
- Quick Look and the Finder thumbnails no longer crash on a damaged Blorb file. The Quick Look preview of a Blorb keeps its paragraph breaks and shows characters such as `&amp;` decoded, and the preview of an iFiction file colours all its entities, is readable in dark mode, and says so when the file can't be parsed.
- The story name stored in a Blorb file is read correctly, where it used to come out empty for the Infocom Blorbs, and the cover picture is chosen the same way everywhere. A Blorb whose cover entry names a picture that isn't there falls back to the first picture.
- Data downloaded from IFDB no longer overwrites the headline with the year; the year goes to the first-published date. The forgiveness rating is stored, dates in odd formats are understood, and the language is stored as a code, so "French, English (fr, en)" becomes "fr". Covers downloaded from IFDB are saved straight away, and Open in IFDB works for games without an IFID. Spatterlight no longer accepts an untrusted certificate when talking to IFDB.
- Games with the older Zoom-style IFIDs are matched the same way as babel does.
- Verify Library no longer acts on games that have been deleted, can't remove the game you are playing, and doesn't import games that have several IFIDs again. The check for missing files and for dropped files ignores the case of the file name, and the check for files in the Trash only matches the Trash folder itself. When looking for missing files in a moved folder, a folder name no longer matches inside a longer one, such as "Games" inside "MyGames".
- Exported metadata escapes characters that are special in XML. Damaged Z-code, Blorb and NEO files are refused instead of being read past their end. Files ending in .zlb and .gblorb are recognised as Blorbs.
- Sorting the library by the Liked column works, and the window subtitle shows the right count. Copying and pasting covers in the info window is fixed, and the temporary file a dropped cover is read from is deleted. When organising the library, a game's group is only recorded once the file has actually been moved. The "taking a long time" alert is no longer shown after a game has failed to load.
- The copy of *Beyond Zork*'s SCREEN.DAT made for the game is deleted afterwards.
- Changing only the size in the font panel no longer resets the font to a different family. The Right and Center alignment buttons were swapped on Intel Macs. The baseline of the DOSBox and Montserrat themes is fixed. Out-of-range values for the default size and the update check interval are corrected, and the quote box checkbox shows the right state.
- Updating a library from an older version of Spatterlight no longer replaces the game titles with file names. If the library can't be moved to its new shared location, Spatterlight keeps using it where it is and tries again at the next launch, instead of quitting.
- Your "Old settings" theme is no longer overwritten, recreated or deleted with each new build of Spatterlight or by Rebuild Default Themes.
- Removing the light mode override of a theme works.
- Notification bezels shown in quick succession wait their turn instead of drawing on top of each other. VoiceOver announces their text even with bezels turned off. The Game Over bezel gets the right tint, and vector covers and tall portrait covers show correctly in bezels.
- A home folder name with non-ASCII characters no longer breaks autosaves and temporary files. Fixes several crashes when autorestoring, and one when an event arrives for a window that has just closed. Characters outside the Basic Multilingual Plane work in preloaded input, replies and erased text, and the position in a Unicode file stream is kept correctly.
- Starting a sound on a channel that is already playing one no longer crashes (issue #77): the old sound could still be writing into an audio buffer that the new one had just freed. A burst of more than eight short sounds in a few seconds left a sound that could not be stopped at all, and the channel could hang forever when closed. Each sound channel no longer leaks an audio output. A sound that can't be decoded no longer crashes, and is not played again after an autorestore. A fade in progress when the game was autosaved continues with the right timing, and volumes out of range are clamped. A sound that can't be resumed after a pause starts again from where it was. Autosaves stay compatible with older and newer builds.
- ZX Spectrum beeps play at the exact pitch (a 4000 Hz note used to play at 4410 Hz), and a tune stops when the game ends.
- Graphics windows with a transparent background resize correctly, edges no longer grow softer with each resize, and the window is redrawn when moved to a screen with a different resolution. A click no longer counts as a drag unless the mouse moves at least 3 points, and dragging a graphics window drags a picture of its contents.
- A dragged picture can be dropped as a file, for example into Terminal.
- Images replaced by a game are read in full, and purging pictures no longer deletes picture files that belong to the game. Placeholder rectangles in a Blorb file are kept, a picture that fails to load no longer leaves the previous one showing, and the temporary files of replaced pictures are deleted once decoded.
- JPEG pictures with Exif data are recognised. VoiceOver rotor fixes, among them in *A Colder Light*.
- In text buffer windows, turning echo on and off works, clicks on hyperlinks are only sent when the game is waiting for one, a margin picture keeps its link when rescaled, and clearing the scrollback is fixed. Fixes a crash when dragging a picture that has no bitmap and one in *Dead Cities*. The VoiceOver labels of pictures and the input history are localised.
- The background scrollback trim is removed. It was on by default, but dropped margin pictures and flow breaks and could lose text printed while it ran. The scrollback is now only trimmed while you are at the bottom.
- Line input after a prompt that ends with a newline, such as "Name?", starts on the next line instead of at the end of the prompt. Text printed after a game takes back part of its output is no longer paged past unread, and auto-scrolling no longer stops there.
- A margin picture at the bottom of a text buffer window no longer leaves blank space below the text or makes typing jump, scrolling to the bottom keeps the input caret on screen, and a window scrolled up from the bottom comes back where it was after an autorestore.
- Up-arrow in the command history stops at the oldest command again after the first hundred or so. Typing in a grid window no longer goes wrong when the text is over the length limit, splits accented characters or refuses deletions.
- A merged quote box, such as the side-by-side quotes in *Trinity*, is restored in the right place, and the turn count is kept across an autorestore.
- Graphics window autosaves are far smaller (a *Journey* autosave went from 6.2 MB to 30 KB), and VoiceOver no longer names a picture that has been cleared away.
- Impulse Tracker (IT) music is recognised instead of being taken for MP3, a fading sound no longer keeps its channel alive, and MIDI music that can't be started is reported to the game as failed instead of silently doing nothing.
- Announcements made with no game window open, such as "1 alert reset" in the Preferences window, are no longer lost.
- When Download Info changes a game's title while its title cell is open for editing, the old title is no longer written back and marked as edited by you. The status icons in the library's found column are a fixed 14×14, which may make them show up on macOS 10.13.
- Comparing a downloaded cover with the one a game already has uses Vision feature prints on macOS 10.15 and later, so re-encoded or resized copies of the same picture are recognised.
- In Preferences, the "Game specific hacks" check box is now "Disable game-specific hacks", the selected theme is restored as soon as the theme list loads, and editing a built-in theme straight after opening the window makes only one copy of it. The window reopened at launch is the right size from the start, instead of resizing itself a moment after it appears, and switching panels quickly no longer hides the preview. The Show Info toolbar label is the same on all macOS versions.
- In text grid windows, such as status lines, making the window narrower no longer leaves the last character of each row behind, and line input in an empty grid window no longer crashes. VoiceOver says "End of command history", not "Start", when stepping past the newest command, and a selection that ends at the last character is kept when the game is autorestored.

## Release 1.5.5

### Scarier (Adrift)
This time, most of the work has been done on the Adrift 4 and earlier side of Scarier. By analysing the original Adrift runners and running lots of custom tests, the output of Scarier was painstakingly made to exactly match that of the original Adrift runners. This is kind of important, as many games use something called [ALR](https://www.ifwiki.org/ADRIFT_Language_Resource), which is basically a list of replacement strings. *Asteroid Aftermath*, for example, replaces the parser output

`Which satellite.   satellite,  satellite,  satellite,  satellite or  satellite?`

with

`Please refer to a satellite by its correct name or number.`

However, getting the output this exact meant porting a lot of original bugs and glitches over to Scarier, which isn't ideal. Many of these quirks did *not* fix any ALRs or other problems, but only made the games worse. So after spending a lot of time (and tokens) making sure that output and functionality were exactly like the original runners, a lot of time (and tokens) was spent on making output and functionality differ from the original runners again.

The goals were to fix as much broken stuff as possible, make things work as the authors intended, minimize player frustration, and at least keep the user experience on par with the original SCARE interpreter. To this end, 37 games whose own data makes them unwinnable, such as *Ebony's World* and *Bedlam*, are patched on-the-fly, and can now be played to the end in Scarier. The command GLK PATCHES OFF will remove all patches for the original broken experience.

All of the 609 tested Adrift-4-and-earlier games that have an ending can now be completed in Scarier.

#### Adrift 3.7 to 4
- A title picture now either gets its own graphics window, filling the screen until you press a key or click, or if it is small enough, is drawn inline above the story text. It no longer squeezes the opening text into a strip below it and confusingly scrolls the text instead of closing when you press a key.
- Two new assist hacks, for repeated tasks and room restrictions, join the ones for combat and movement, and 13 known games now get the assists they need turned on automatically, with a notice at startup that names each assist and the command to turn it off. The assists are now consolidated under one command, GLK ASSIST: for example GLK ASSIST COMBAT, which makes the combat work in games that have been converted from Adrift 3.9 to 4.0. Turn it off with GLK ASSIST COMBAT OFF.
- The more sensible and non-broken parts of the battle system are implemented to work just like in the 3.9 and 4.0 runners. Replies to "Who do you want to attack?" and "What do you want to attack X with?" are actually understood. THROW works as a battle command, asking who to attack when no target is named, and stamina recovery works runner-accurately.
- A character killed in battle is dead for good: its walks stop, and WHERE IS THE GUARD answers "Guard is dead!". The one exception is a deliberate deviation from the runners: when a game's own death task hides the character and gives it back its stamina, the character is allowed to walk back in. *The Town of Azra* does exactly this to let you keep hunting bandits and deer, which never worked in the original runner and made some of the game's goals unreachable.
- The battle narration now uses the correct perspective, "I hit the guard" rather than "You hit the guard" in a first person "I" game. 
- In 3.9 games too, a task that raises a character's stamina, strength, defence, accuracy or agility stops at the attribute's maximum. An attitude is stored as written, and a 3.9 speed change lands on the character its author meant.
- A battle command naming several targets asks which one you mean and strikes only that one. This is a deliberate deviation: the runners struck every character the line named and then asked which one you meant.
- The original metacommands, such as PAST, BYE, END and ENDGAME, are implemented, and Scarier-only commands — STATS, HINTS, BRIEF, VERBOSE and NOTIFY etc. — no longer quietly steal the game's own custom tasks.
- UNDO prints the output of the turn it restores, just like the runners do.
- Autosave now stores the entire session: the last command that AGAIN would repeat, command history, pronouns, a pending "Which one?" question, verbosity and notification settings.
- Once an object has been listed as being in or on something, it can be referred to. 
- An object inside another can be taken even if it matches a broadly defined game task, such as `get *stove*` in *Warlord*.
- A never-seen object can no longer shadow a similarly named one in your hands.
- A command that refers to several objects with the same name, such as OPEN DOOR with a red and a blue door, asks which one you mean instead of acting on all of them.
- TALK TO gives the runner-accurate response "Use the format 'ask [character] about [subject]'."
- The carry count in 3.9 games matches the runner, which makes it possible to take the belt pouch in *Lara Croft: The Sun Obelisk*.
- Bold, italics and underline are now flags that are either on or off, rather than a nesting counter. An intro that forgets to close a tag no longer leaks its style into the next paragraph.
- Timed events and character walks follow the runners more closely: walk steps land on the exact tick.
- A character with a walk will now properly get hidden.
- Events recheck the restrictions of a task that has already run, and the turn an event starts is itself a tick.
- GLK NOTIFY turns score notifications on or off, and GLK STATUSLINE (or GLK STATS) prints the status line, even in games that have tasks of their own called NOTIFY or STATUSLINE. GLK SUMMARY no longer turns verbose mode on.
- A game that stops because of a fatal error now says why, where it used to stop without a word.
- Text-heavy and event-heavy games are considerably faster. The ALR table is indexed instead of scanned in full for every line, undo snapshots are no longer compressed, and the map's unchanging data is read once per game rather than at every prompt.
- Text set in Wingdings, Wingdings 3 or Symbol is translated to Unicode, as Webdings already was. Games that use symbol fonts — the aliens' speech in *The Most Average Man in the World*, the row of droplets in *The Reluctant Resurrectee* — read as the author intended.
- All of the above is held in place by a regression suite that replays a walkthrough for more than 400 Adrift games, and by a library of transcripts recorded from the original 3.7, 3.8, 3.9 and 4.0 runners (for every game that starts in them.)

#### Adrift 5
- Turning the music off at a game's opening question — the name prompt in *Grandpa's Ranch*, for instance — now keeps it off once play begins (reported by Dan Fabulich).
- Identical `<# #>` expression tags inside one block of text all show the first one's value, as the original runner's replace-all does.
- Fixes an occasional crash when a game resumed from an autosave tried to load a picture or a sound.
- Scarier can now be built with Visual C++, and plays the pictures and sounds of Adrift 5 games under Windows Glk (Dan Fabulich).
- The status line updates properly during dialogs and window resizing.
- Adrift 5 games now load through Glk rather than directly from disk, so the interpreter also works in ports without a file system (such as Emglken) (Dan Fabulich).

#### Both
- The default map colours are now less flat, with in-between tints added to the previous strict monochrome (Dan Fabulich).
- The GLK MAP COLOUR command has been removed: the map is always drawn in the standard colours (Dan Fabulich).
- Maps now have zoom and pan buttons. GLK ZOOM (N) sets an exact zoom factor from 3 to 32; below 10 the rooms lose their names, to give an overview of a big map. The Up and Down badges now show an arrow rather than a letter (Dan Fabulich). The buttons also work while the game waits for a keypress or a pop-up answer, the map is redrawn at each such pause so that it keeps up with a cutscene, and rooms on another level than yours are drawn fainter than before.
- Clicking just off a side or corner of your own room on the map walks one step that way, when the room has an exit there, even into a room not yet seen.
- A walk started by clicking a room on the map begins on the prompt's own line, in the input colour. In Adrift 4 games it no longer answers "I don't understand what you mean!" first, or runs whatever was half-typed. A click on a room does nothing while a game is asking a question rather than waiting for a command.
- Autosave also works at a "press a key" pause, so a long cutscene closed halfway comes back where you left it. The same goes for the name and gender questions at the start of an Adrift 4 game and the pop-up questions of an Adrift 5 one.
- Sounds and pictures that a game names but never packed into its game file are now found if they are in the same folder. This brings back the sounds of Adrift 4 games like *Druggy Lane*, and the pictures and sounds of Adrift 5 games shipped as a bare .taf.
- A status too wide for the window is now cut at its start behind a single "…" placed right after the room name, where it used to be right-aligned behind three full-width dots. This cuts out fewer letters.

### Question (Quest)
- The Quest interpreter formerly known as Geas is now called Question. Existing saves still load. An autosave from an earlier version does not: the game starts from the beginning.
- Autosave also works while the parser asks which of several things you meant, in both Quest 4 and Quest 5 games. The game resumes at the same list, with anything you had already answered earlier in that turn kept.
- Autosave works at the questions a game asks, such as yes or no, and at "press a key" pauses.
- SIDEBAR WIDE gives the side pane 35% of the window instead of 20%, for games whose object names or status lines do not fit it, and SIDEBAR NORMAL puts it back. The choice is kept in the autosave.
- A status line too long for the banner at the top is listed in the side pane instead, one value per line, and returns to the banner when the window is wide enough to hold it.
- Both Quest 4 and Quest 5 games run faster, by a third or more over a large collection of walkthroughs: built-in functions and object fields are looked up directly instead of searched for, command patterns that cannot match are rejected before their regular expression runs, and synonyms are only checked against objects in scope when they occur in the command.

#### Quest 4
- Accented letters in game text are printed correctly instead of as two incorrect characters, and letters outside Latin-1 can be typed.
- The status bar is up to date during a game's intro and at "press a key" pauses, and no longer goes blank when the window is resized while a question or menu is open.
- Saved games keep decimal numbers, where 0.5 used to come back as 1, and a clone's name no longer outlives the clone after UNDO or RESTORE. UNDO works after a RESTORE, and a save from another game, or from another release of the same game, is refused.
- A game with a timer, closed between two of its firings, resumes with the right amount of time left.
- Timers and "press a key" pauses are spaced as in Quest 4.1.5: no blank line after a pause, no stray prompts left behind by a timer, and the text no longer jumps once a second in games that run on timers, such as *Sim Political Career*.
- IT, THEM, HIM, HER and the other pronouns only stand for the last thing referred to while it is still in reach and of the right kind, as in the original.

#### Quest 5
- *Deeper* can be played: its character creation dialog and status panel work, where the dialog used to print as a jumble and the game could not start, and it no longer hangs while generating its levels.
- Games that ask for input with `GetInput()`, such as *System Restore*, *Escape from Byron Bay* and *The Last Survivor*, now work, and every page in *Victorian Detective* keeps its own `firsttime` state instead of sharing one.
- Fixes a crash in *Whitefield Academy of Witchcraft*, and with it the order in which `on ready` callbacks run (Dan Fabulich).
- A "Continue..." link appears while the game is waiting for a keypress (Dan Fabulich).
- A turnscript can no longer fire between a command's question and its answer, matching the original's turn boundary.
- Rooms that the map library can never reach on its own — one entered by a script rather than through an exit, or through an exit that appeared while the player was already beside it — now show up on the map, and *The Acreage* draws its map instead of printing errors.
- The undo history is part of the autosave, so UNDO still works after a game is reopened. UNDO itself follows the original more closely in what it puts back.
- A script that a game gives to several objects, or copies along with a cloned object, is one shared script as in the original, so its "first time" text runs only once and its random numbers come out the same. In *The Quest for the Golden Egg*, a second treasure chest no longer pays out the same one-time treasure again.

### Bocfel (Z-code)
- The core interpreter is updated to Bocfel 2.6. Our own Z6 support is bolted on top of it.
- The charging boar in *Arthur* now lines up with the tree behind it.
- Fixes two crashes when resuming from an autosave: one that hit the next autosave in games with sound, and one that hit old autosaves on relaunch. Autosave also works when the system autosave folder does not exist yet.
- *Beyond Zork* keeps its colours after being resumed from an autosave, where they used to reset at the next command. Autorestoring a second time no longer fails and starts the game over with an error window in place of the status window.
- The DEFINE menu in *Beyond Zork* is no longer wrapped at the wrong column, its input field out of place, when the game is autorestored after its theme was changed while it was closed.
- The mini-games in the early releases of *Zork Zero* (242, 296 and 66) get the same treatment as the later ones. Peggleboz used to be drawn with the wrong layout there, and resizing the window during it ended in a fatal error. Clicking a weight in the Tower of Bozbar now works in releases 296 and 66, and the Snarfem number boxes no longer forget the selected pile when the window is resized. Release 242 has no mouse support of its own in these games, so no clickable boxes are drawn in it.
- Double Fanucci in *Zork Zero* works in every release that has it. The sample game in the demo was blank in release 296 and half drawn in release 66, the scores shown were wrong in releases 296, 66, 343 and 366, and wide windows got the abbreviated play names meant for narrow ones.
- Skipping a *Zork Zero* mini-game with VoiceOver on (or graphics off): Peggleboz is now solved at once, where it used to ask for two or three more key presses. Skipping Snarfem works in release 242 as well, and no longer counts as a loss in releases 296 and 66. If you decline the offer to skip Snarfem, it is not repeated before every key you press.
- In *Zork Zero* releases before 366, the selected peg in Peggleboz keeps blinking after the window is resized, and is still selected after an autorestore.
- A play in Double Fanucci can be picked with the mouse: the list of plays used to stop listening after the first click. The highlighted play is also shown again when a game of Double Fanucci is resumed from an autosave. The edges of the Snarfem boxes and piles are where they are drawn, not a pixel off.
- The *Zork Zero* demo starts its playable part on a clean page, instead of printing the name of the first room on the same line as "Hit any key to continue". The Double Fanucci scores in its slide show are drawn in the right colour, and the right-hand score no longer overlaps the cards.
- *Zork Zero*, *Arthur* and *Shogun* follow a change of theme again, instead of holding on to the old theme's text colours as if you had picked them in the game.
- The last few pixels of the bottom row were missing from many pictures in *Zork Zero* and the other Infocom Z6 games.
- *Journey* keeps its story text on screen when the game quits, instead of leaving an empty window with no scrollback. A renamed Tag no longer loses the first two letters of his name in some menu entries, and the Apple II and MS-DOS versions use their narrower picture margin, which leaves more room for text.
- The poles of the banner in *Arthur* no longer show a doubled line where the stretched part meets the foot, most visible in the Mac black and white graphics.
- The text area in *Shogun* is the right height when the menu window is open.
- Fixes a crash in the hint menus of the Infocom Z6 games when the hint table sits high in memory, and one in the *Journey* VoiceOver menus. *Arthur* no longer crashes at startup with the interpreter number set to Apple IIe, and *Journey* no longer crashes when text entry is the first thing after an autorestore.
- When looking for the second disk side of an Apple II Z6 game, a year or other number in the file name is no longer mistaken for the side number.
- The Z-code ports of the eleven Mysterious Adventures show their pictures properly. The blank band between the picture and the room description is gone, the picture comes back after a window resize or an autorestore instead of waiting for the next room, and the 2011 releases, which draw their pictures themselves, no longer show nothing but a solid colour.
- The non-Infocom Z6 games behave much better. Erasing the text window now clears the screen, so the Z6 port of *Adventure* no longer continues its story below the intro screens, and the commands typed in it are shown. Its STYLE layouts, with the room description and the inventory in panels of their own, are laid out as windows instead of being jumbled into the story text, and its quote boxes stay visible.
- The menu bar in *Moments Out of Time* gets a window of its own at the bottom, instead of being printed into the story before every prompt.
- *Wyrmward* and other games that make room for a picture by printing newlines no longer leave it below hundreds of blank lines.
- The start menu of *Sunburst Contamination* is cleared when the game begins.
- A Z6 game that draws in the theme's background colour no longer gets white on a dark theme.
- *Zork Zero*, *Shogun*, *Arthur* and *Journey* now respect the Enable graphics setting. With it off they drew no pictures, but still laid the screen out around them and waited for a key on title screens that were never shown. Now the title screens are skipped and the text gets the whole window. *Zork Zero* and *Shogun* get a plain reverse-video status bar in place of the picture border, *Zork Zero* prints ordinary letters for its drop caps and a note in place of its map, and *Arthur* uses its own text-only mode. The puzzle shortcuts made for VoiceOver are offered when pictures are off, and the layout follows the setting when it is changed during play.
- *Journey* draws its command menu in the right place after an autorestore, without needing the window to be resized first.
- Clicks in the *Zork Zero* mini-games (the Peggleboz and Tower of Bozbar buttons, the Snarfem boxes and the Double Fanucci cards) land where the mouse pointer is, instead of slightly up and to the left of it.
- The Apple II, Mac and Amiga picture files of the Z6 games are read within the length stored in front of each picture, following Infocom's interpreter sources, so a picture no longer runs on into the data that follows it.
- Games that keep data files of their own next to the story file, such as *WITHOUT corporation*, now read and write them directly, instead of asking for a file with a file dialog every time.
- The pictures of the Infocom Z6 games are handed to Spatterlight once and found again on later draws, instead of being written out and decoded afresh every time. *Zork Zero* used to resend each margin picture on every draw and its border on every turn; now a repeated picture is a cache hit, and the border is kept in a set of recent versions so that walking between rooms, which lights different exits on its compass, mostly finds one already sent.
- On a fresh install with autosave turned off, *Zork Zero*, *Arthur*, *Shogun* and *Journey* drew no pictures at all, because the folder the interpreter hands its pictures over through was only created for autosaves.

### ScottFree
- TI-99/4A games run their actions accurate to the original Adventure cartridge. Random events were falling on the wrong turns in every one of them, dying ended the game where it should carry on in the game's last room, and *Pyramid of Doom* could not be won.
- Whatever follows a verb and its noun is ignored, as in the original Adventure International UK interpreters: LOOK UP AT THE GREMLIN is LOOK UP. TAKE A COIN is understood, and ALL EXCEPT accepts synonyms and IT.
- The Italian *Perseus and Andromeda*, *Antica Grecia*, has its interpreter messages and its system commands in Italian.
- Spanish *Gremlins* on Commodore 64 disk can be won: its first action was garbled, which made the dart in the attic always fatal. The German *Gremlins* understands "nach unten gehen", and both have a number of text fixes.
- The second disk of a two-disk game is found under more file names, such as "disk.1", "SIDE A" and "Disk TWO".
- The pictures of the Apple II *The Hulk*, *The Count*, *Voodoo Castle* and *Sorcerer of Claymorgue Castle* are found through the picture disk's own index rather than a built-in list for each release. This brings back a few that were missing or wrong: the gems in the inventory of *The Hulk*, the coat-of-arms in the inventory and the daisies in the flower box of *The Count*, and a cut-off room picture and a spell in *Claymorgue*.
- The pictures of the Atari 8-bit games are likewise found through the table on the game's own disk. In *The Hulk*, the gems in the inventory picture are now drawn in the same places as in the original.
- The pictures of the Commodore 64 versions of *Pirate Adventure* and *Voodoo Castle* with small graphics are found through the tables in the game itself. This fixes the open door and the pirate ship in *Pirate Adventure*, and moves three of its objects one pixel.
- The close-up pictures shown when examining certain things in the Apple II and Atari games and the Commodore 64 and UK releases of *The Hulk* are now taken from the game's own table instead of a built-in list, so they appear for the same words and under the same conditions as in the original, plus a few extra ones that the old list had (such as the bees once they are in the bottle in *Adventureland*). With graphics turned off, these games and the UK release of *Adventureland* no longer stop to wait for a key where a close-up would have been.
- Small fixes to *Robin of Sherwood* on Commodore 64 and *Strange Odyssey*.
- Pictures in the ZX Spectrum and Commodore 64 games released by Adventure International UK use far fewer drawing operations, up to three quarters fewer depending on the game: pixels of one colour in a row are sent as a single rectangle, as *Seas of Blood* already did. The pictures of the MS-DOS, Commodore 64 and Atari 8-bit releases of the American games get the same treatment, with 55 to 70 per cent fewer in *The Hulk* and *The Count*.
- Nineteen more Commodore 64 disk images of *The Time Machine* and *The Sorcerer of Claymorgue Castle* are recognised, among them 40-track disks, the "773 - 695" menu disk and the Slam compilation, whose directory lists a copy cut short while the complete one sits unlisted elsewhere on the disk. The Danish Crackers release of *Claymorgue*, which has fourteen bytes wrong, is repaired as it loads: the pool of dirty water had turned into bars, and two bad tiles showed in 27 of its 44 pictures.
- The disk release of *Return to Pirate's Isle* for the TI-99/4A has its pictures, title screen and font read through the disk's directory rather than at fixed offsets, and the one known disk with a damaged picture of room 5 is repaired where the damage is.
- The lava dam in the Apple II *Adventureland* is drawn again.
- Two Commodore 64 copies of *Voodoo Castle*, the "Cracked by Toko" disk and VOODOOC2.T64, can be won: a damaged byte kept you out of the cell. It is repaired as the game loads.
- The UK Atari 8-bit 16K disk of *Spider-Man*, which has no DOS, is supported.
- Fixes a crash in the Atari 8-bit and Apple II *Mission Impossible*, and saved games of the Mysterious Adventures no longer lose every message after the first quote mark. A cut-short or damaged game or save file is refused instead of crashing the interpreter.
- The colours of the Commodore 64 *Sorcerer of Claymorgue Castle* pictures are worked out the way the original interpreter does, with a few deliberate changes to make some pictures easier to read, such as the water on the kitchen floor. A few colours that used to be carried over from the previous picture are now right. In the Atari 8-bit pictures, colours that the Atari shows alike are now drawn alike, which makes many of them slightly darker.
- A line at exactly 45 degrees in an Atari 8-bit picture is drawn from the same end as in the original, which S.A.G.E., the editor the pictures were made with, decides the way its own 6502 source does. The pixels were already right, only the order in which they appeared while the picture was being drawn was not.
- The colours of the Atari 8-bit *Adventureland*, *Pirate Adventure*, *Mission Impossible* and *Strange Odyssey* pictures are now worked out from the game's own colour test picture, the one that shows six blocks labelled WHITE, BROWN, ORANGE, YELLOW, BLUE and GREEN and asks the player to adjust the TV until they match. The Atari flickers two sets of colours on alternate frames and leaves the mixing to the eye, and the old colours were an approximation of that made by hand. The mix the games use as their blank page is now white rather than light blue, and tree trunks are orange rather than maroon.
- In *Gremlins*, a few cells of the street, bar and bedroom pictures no longer get the wrong tile, and the blender keeps the frame it stopped on instead of jumping back to the first. The waterfall seen from inside the cave in *Robin of Sherwood* is no longer animated with its colours swapped. These were found by comparing the pictures, room by room, with screens captured from the original ZX Spectrum games, and are fixed in the Commodore 64 versions as well.
- ZX Spectrum and TI-99/4A pictures are 256 pixels wide, not 255, so they no longer sit half a pixel left of centre and lose their rightmost pixel column in a narrow window. The Atari 8-bit pictures are no longer widened by a black bar on the right, and a picture wider than the window is centred rather than drawn off screen.
- Apple II pictures no longer draw one scanline more than the original shows. The games draw below the picture too, where the Apple's text window hides it, and that line could show up under a title picture.

### TaylorMade
- Pictures take 70 to 90 per cent fewer drawing operations. A room picture is no longer wiped and painted again from scratch every turn: only the parts that changed are redrawn. *Questprobe featuring Human Torch and The Thing*, which repainted its whole picture several times a turn, gains the most.
- Text in *Rebel Planet* is printed about ten times faster.
- A file that turns out not to be a TaylorMade game now says so in the game window, where the window used to close without a word.
- A game whose picture data cannot be read is now played without graphics, with a note saying so in the game window. A *Questprobe featuring Human Torch and The Thing* file with damaged picture tables used to leave an empty graphics window above the text.
- The animations in *Rebel Planet* no longer start on the wrong frame when you arrive from another animated room. The security robot could flash a wrong picture after you had met the serpent in the sewers, and restoring a game saved at the Queen's computer could show a serpent withdrawing that was never there.
- With graphics turned off, the status window is no longer closed and opened again at every room description.
- A game file that is cut short or damaged is refused, or played without its pictures, instead of crashing the interpreter.
- RAM SAVE, RAM LOAD and RAM RESTORE in *Rebel Planet* now save and restore in memory like QUICKSAVE and QUICKLOAD. They used to ask for a file.
- An animation in *Rebel Planet* no longer runs at the speed of the one in the room you came from.
- The room pictures have been compared with screens captured from the original ZX Spectrum games, and the differences this turned up are fixed. Several pictures in *Rebel Planet* had wrongly coloured squares, and pictures in *He-Man* and *Blizzard Pass* had small errors: a colour was not carried over from one picture to the next, mirroring disturbed the middle row or column of an odd-sized area, and a fill stopped one cell short of the end of the picture. Pictures could also come out differently from one run to the next, which no longer happens.
- The parser sorts the words of a command into verb and noun as the original engines do, by each word's number, instead of trying both orders. *Blizzard Pass* takes SKIN, POLISH and DRAW as verbs wherever its engine does, not only as the first word, the verb is kept after a comma, and the "I don't understand" messages are the originals'.
- Taking, dropping and chance follow the Spectrum code: dropping a worn object drops it silently instead of refusing, taking it says "You are wearing it", a failed GET or DROP ends the turn at once (a LOOK in *He-Man* used to go on and raise the carrying limit), chance rolls succeed slightly less often, and the action that printed "Okay" in *Kayleth* and *Rebel Planet* prints nothing, as it does in the originals.
- The Queen's lights in *Rebel Planet* change colour in the original order, with the Spectrum's bright inks or the Commodore 64's own cycle, her head is shown and hidden for equal halves and much more slowly on the Commodore 64, and the star field animation leaves its blank lines alone.
- Pictures are 256 pixels wide rather than 255, so they no longer lose their rightmost pixel column in a narrow window.
- The c64.com crack of *Terraquake* for the Commodore 64, which is packed four layers deep, now loads. The explanation of the text-only choice in the *Temple of Terror* menu is spelt properly.

### Plus
- Pictures take 24 to 83 per cent fewer drawing operations, depending on the game. Pixels of one colour in a row are sent as a single rectangle, and a picture that is already showing is not drawn again.
- The unfinished *Questprobe Number 4: X-Men* fragment now behaves as it does in the original MS-DOS interpreter, which was disassembled for the purpose: the conditions and actions that only this game uses are implemented the way XMEN.EXE runs them.
- The Bio Gem in *X-Men* can now be saved. The game's own data made this impossible; it is patched as the game loads.
- A game file that is cut short or damaged is refused instead of crashing the interpreter.
- With graphics turned off, a game no longer starts by waiting for a key on a blank window.
- In the Atari ST games, the colours that cycled in the previous picture stop when a new one is drawn, as in the original, where they used to cycle on for good behind it.
- The pictures of the Atari 8-bit versions of *Spider-Man*, *Fantastic Four* and *Buckaroo Banzai* are found through the table on the boot disk. *Spider-Man* gains a picture of Doctor Octopus that was missing, and two object pictures in *Buckaroo Banzai* that were wrong or not drawn now show.
- The pictures of the Apple II versions of *Spider-Man*, *Fantastic Four* and *Buckaroo Banzai* are found through the tables in the game database.
- The Atari 8-bit *Spider-Man* pictures are cropped to their frame like the other games', without the black column on the right.
- Atari 8-bit games no longer crash when their picture disk is missing.
- The colours of the Commodore 64 pictures are worked out the way the original interpreter does, with a few deliberate changes, such as the orange Thing in *Fantastic Four*. A few colours that used to be carried over from the previous picture are now right, for example in one of the *Spider-Man* pictures. In the Atari 8-bit pictures, colours that the Atari shows alike are now drawn alike, which makes many of them slightly darker.
- The inventory no longer leaves out the item listed after an invisible one.
- Saved games, UNDO, RAMSAVE and restarting keep every item's flags and the room exits, as the original interpreters do. Restarting *Claymorgue* after lowering the drawbridge used to leave the east exit open. Older saves still load.
- A picture wider than the graphics window is no longer drawn far off to the side when the window is resized.
- Apple II pictures no longer draw one scanline more than the original shows.

### Comprehend
- The picture area now always has a black background. This makes many images look nicer.
- The Enable graphics setting is respected, where it used to be ignored: with it off no picture window is opened, and the window comes and goes when the setting is changed during play. The title picture and the crystal ball scene of *The Crimson Crown* no longer wait for keys on an empty screen.
- The 16-colour PCjr pictures of *Transylvania* and *The Crimson Crown* now match the real games pixel for pixel, checked against captures of all 73 of them in an emulator: the dither pattern was on the wrong rows and one kind of fill used the wrong tiles.
- When a game's own NOVEL.EXE or PC_GRAPH.OVR is missing, built-in copies of its brushes and fonts are used and the pictures come out the same as in the CGA renderer, where an older, less faithful renderer used to take over.

### General
- Autosave now writes the new files completely before replacing the old ones, so a crash or failed write no longer leaves a game state paired with the previous turn's window layout. An interrupted autorestore can no longer pair them up wrongly either.
- Spatterlight now answers `glk_style_measure` for size, weight, obliqueness and proportional spacing, so an interpreter that asks about the current theme's fonts gets a real answer (Dan Fabulich).
- Fixes a crash when loading a sound or image whose resource number is very large, which killed some Adrift games at their first prompt.
- Builds on Xcode 27 (Dan Fabulich), and releases are now built, signed and notarized by an automated workflow.
- Cancelling line input no longer writes past the game's input buffer: the text typed so far came back as two bytes per character into a buffer sized for one, which could crash a game that takes input back on a timer. Cancelling the file dialog of a restore command no longer hands the game a made-up file name, and text a game pre-loads into the input line keeps its accented characters instead of showing `?`.
- Fixes text occasionally losing its first character, or the line break before it, when the interpreter had just asked whether a character can be displayed. In Adrift games with unusual characters in a room name this ran a reply into the typed command and clipped the room name.
- Each temporary file a game creates no longer leaves a file descriptor open, and a very long line of input no longer risks the interpreter giving up with a protocol error.
- The temporary folder of an interpreter that crashed or was force quit is removed the next time a game starts, and a game's own temporary files are carried inside its autosave, so a resumed session gets them back.
- Text windows with many pictures in the margins lay out several times faster, which shows most when resizing the window of a long session of an illustrated game such as *Zork Zero*.
- Reading the scrollback in a game that runs a timer is no longer interrupted by the view jumping back to the bottom, however you scrolled up. Clicking a link that enters a command scrolls down to the prompt, as typing the command does.
- Sound that is already playing moves to the new output device when you plug in headphones.
- Games resumed from an autosave open a fifth of a second quicker. A game force quit in fullscreen no longer comes back as a blank window, leaving fullscreen after a relaunch no longer puts the window in a corner of the screen, and windows opened after a restore keep their colours and alignment.
- Fixes a hang when an interpreter rejected an autosave from an older version and restarted the game.
- Starting a game no longer stalls for a moment when the library is large. Opening a game file that the library's search field is hiding clears the search, and the selection no longer jumps back to the opened game a moment later.
- Spatterlight opts out of Game Mode, which macOS otherwise turned on with a notification whenever a game window entered fullscreen.
- Graphics windows draw faster. Filled rectangles now go straight into a bitmap instead of through one drawing call each, which helps most in pictures and maps made of many small pieces, such as the map pane of a large Adrift 5 game like *The Axe of Kolt*.
- Add Games in the library can choose a folder again on macOS 27, where selecting a folder and clicking Add opened the folder instead of adding its games.
- A game that restarts itself while being resumed from a fullscreen autosave no longer lays out its windows for the full screen in an ordinary window.
- A graphics window that asks for the default background colour gets the theme's background instead of white, and a rectangle filled with the default colour is no longer drawn in whatever colour was used last.
- A quote box that was on screen when a game was autosaved comes back intact: without stripes of the story text showing through, centred instead of hanging off the left edge, and fading out at the right time.
- With smart quotes on, text that a game prints and then takes back is removed properly even when it contains a quote mark or an apostrophe.
- A sound that has played to its end is not started again when the game unpauses a channel that was never paused (reported by Dan Fabulich).
- Fixes a crash when choosing Delete All User Themes while one of them was selected. Changing a colour of a built-in theme by dragging in the Colors panel makes one copy of the theme, not several, and deleting a theme selects the one it was made from. The themes table has a contextual menu.
- Adding a damaged or cut-short game file to the library can no longer crash Spatterlight or make it hang while it works out what kind of file it is.
- A cut-short picture file (.MG1) from the DOS versions of Infocom's graphical games no longer crashes the library when it looks for cover art, and a valid cover in such a file is no longer skipped when a .jpg is listed before it.
- The unpacker for packed Commodore 64 programs, shared by the ScottFree, TaylorMade and UnQuill interpreters, no longer unpacks the first layer again at each of its own passes over a program packed in several layers, such as with FinalSuperCompressor, and no longer reads outside its memory on an odd file.
- ScottFree, Plus, TaylorMade, Comprehend and Question build outside Spatterlight again, as Glk interpreters for Gargoyle and others, on Linux and Windows too. With a Glk library that has graphics turned off, games with a title picture or loading screen no longer wait for a key on a blank window, and *Robin of Sherwood*, *Spider-Man* and the dice of *Seas of Blood* no longer try to draw in a window that is not there. ScottFree, TaylorMade and Comprehend take command-line options for the palette and for gradual drawing, and Plus one for skipping delays.
- Hugo games follow a change of theme, instead of holding on to the old theme's text colour where a game has set its own background.
- Library entries that share an IFID, such as other releases of the same game, no longer overwrite each other's files when the library is organised into folders. Adding the original of an AGT game that Spatterlight had converted no longer makes a duplicate entry.
- Spatterlight uses far less processor time while a game sits waiting on a timer. On *Transparent* idling on its sound timer the app went from about a fifth of a processor core to under a tenth: a status window that nothing has printed to is no longer re-laid-out on every tick, the hyphenation language is put on the text itself instead of being written to the user defaults before every redraw, and a graphics window is only redrawn when something in it changed.
- Glulxe autosaves of a timer-driven game are paced to one a second rather than up to twenty, which brought its own share of *Transparent* down from about a quarter of a core to a sixth. Closing the window asks for one final autosave first, so nothing that happened in the last second is lost.
- Pictures in a Blorb file are kept decoded after their first draw, so an animation made of many frames, such as the intro of *Kerkerkruip*, no longer decodes every frame again each time it is shown. The picture cache is bounded by memory, at 256 MB of decoded pixels, and holds the largest games whole.
- Animated GIFs drawn inline in the story text now play, as they do in the original Adrift 5 and Quest 5 interpreters. Ten Quest 5 games and the Adrift 5 game *Pervert Action: Future* use them. Playback pauses while the window is hidden or when Reduce Motion is on, carries on after the game is restored, and costs nothing extra in memory: the animation is the very picture the cache already holds, with its frames switched in turn.
- Renaming a game in the library by clicking its selected title no longer lands the new name on a neighbouring game when a double click starts the game in the middle of it. A rename is now armed as in the Finder, by a single click on the one row that was already selected, instead of by any click on a row.
- The margins of a status line with several highlighted words, such as the menu bar of *WITHOUT corporation*, are painted in the status line's own background again instead of leaving black edges around it.
- Thanks to Dan Fabulich (@dfabulich) for the Quest 5 fixes, the `glk_style_measure` support, the map colour scheme, map zoom and pan, the Windows port of Scarier, Emglken support, the build fixes, and general help and support.

## Release 1.5.4

### Bocfel (Z-code)
- Fixed a crash that hit every Z-code game at the first prompt (or instantly at launch, when an autosave existed) whenever autosave was on and "Enable sound" was off in the theme settings.

### Alan 2
- Alan 2 now plays Alan version 2.5 and 2.6 games, such as *Don't Be Late!* and *The Hollywood Murders*, which it previously rejected.
- Pictures and sounds in Michael Zerbo games such as *The Child Murderer*, *The Hollywood Murders*, *Inner Demons*, and *A Matter of Time*.

### Scarier (Adrift)
- Games that colour their text now display those colours. Colour display can be toggled with the *glk colour* command (Dan Fabulich).
- New *glk undo*, *glk restore*, *glk restart* and *glk quit* meta-commands to use when their standard counterparts are unavailable, such as in the player name entry dialog.
- A new *glk map colour* command switches the map between the standard colours and an alternative scheme derived from the current theme, which mixes room fills, connector greys and a you-are-here marker out of the theme's own paper and ink. The choice is remembered per game.

#### Adrift 3.7 to 4
- Now plays the two known Adrift 3.7 games, *Alice's Restaurant Anti-Massacree Adventure* and *Castle Quest*.
- Reads and writes the original pre-4.0 save format as well as the 4.0 one, so saves can be moved between Spatterlight and the old 3.8/3.9 runner in either direction.
- Some symbol fonts, such as the dove under the *Topaz* title, are translated to Unicode.
- The status text on the right no longer overwrites the room name in a narrow window.
- Container capacity is a volume rather than a count of objects, as in the original runner, so containers hold what their authors intended.
- Timed events and character walks now match the original runners in a long list of details: immediate events start while the game is loading, zero-length and restarting events fire the right number of times, walk arrivals trigger their tasks once, and event room descriptions print last in the room block.
- The battle system was settled command by command against the original 3.9 and 4.0 runners: weapons and armour, wielding, thrown weapons, the combat narration and the status display. Several games previously believed unwinnable can now be played to the end, among them *The Search for Mr. Smith* and *Wes Garden's Halting Nightmare*.
- A random-number bug that made choices between two outcomes far less random than intended is fixed.
- Many parser and world-model fixes, including the precedence of PUT commands, taking and dropping, and restrictions that reach into closed carried containers.
- Bold, italic and right-justified text (Dan Fabulich).
- The score summary that the original runners print after every ending is now shown, and score-change notifications are off by default, as in the original.
- Another round of wording and behaviour matched against the original 3.8/3.9/4.0 runners: the "You pick up" / "You take" version split, the pre-4.0 refusals and death message, where the ending message and "Congratulations!" appear, room-name headings, and *last* and *previous* as synonyms for *again*.
- The map draws its room connectors the way the original Runner does, with opaque badges and arrowheads on one-way exits, and no longer opens an empty pane when the player is in a hidden room (Dan Fabulich).

#### Adrift 5
- The map pane now opens by itself when games ask it to. In and Out connectors are drawn on the map, with a badge at both ends. In-Out badges also appear for one-way connections that only exist as character movements (Dan Fabulich).
- Questions that the game asks through a pop-up dialog, such as the player's name or "Are you male or female?", are asked in the story window instead of being silently skipped.
- An adventure with no locations is refused with the same error message as the original runner, rather than starting the player nowhere.
- A large batch of engine fixes found by a new conformance test suite and by reviewing the Adrift 5 file format: restrictions, task actions, character walks, user-defined functions, object references, conversation, and a dozen text functions that were missing or wrong.
- The source tree now includes a hand-written specification of the Adrift 5 adventure and save file format, contributed by Dan Fabulich (@dfabulich).
- Support for the `<del>` tag, so games can erase text they printed earlier, even across turns (Dan Fabulich).

### Question (Quest)
- Both Quest engines are faster after a profiling pass over their per-turn hot spots: about a fifth off Quest 4 replay times, with movement-heavy games like *Shipwrecked* several times quicker, and a smaller gain in Quest 5.

#### Quest 4
- The Quest 4 engine was settled against the original: every game in a 111-game test corpus is replayed through both Question and Quest Viva and the transcripts compared, with the real Quest 4.1.5 under Wine as tie-breaker on disputed lines. 100 of the 111 transcripts are now byte-identical, and every remaining difference is a documented, deliberate improvement over bugs in the original.
- Game text is decoded as Windows-1252. Accented letters and typographic quotes are printed accurately.
- Quest's standard library is bundled, so games that ask for it get their correct synonyms and verbs; *A certain Oscar* now plays as intended.
- Containers behave as in the original: looking at or opening one lists its contents, a container given to a character keeps its contents, and a closed container refuses PUT — fixing *Barbarian*'s pedestal puzzle.
- "Take X from Y" runs the implied removal as a real nested command, so games that watch for it react.
- Many parser and wording fixes matched line by line against the original: object lookup is scoped per verb, the disambiguation prompt uses Quest's wording, locked places print the author's refusal message, `msg nospeak` prints nothing, unknown text functions yield the original's error marker, and the pre-2.80 dialect (its separate item table, script-based `take`, `out` tag parameters) is implemented. The panes' verb menus no longer overlook verbs whose declaration maps them to a differently-named action.

#### Quest 5
- Arguments to a game's interface calls are evaluated even when the call itself does nothing in a text-only interpreter, so a script error in an argument is reported just as in the original. *The Acreage*'s map code depends on those errors appearing.
- A subset Quest 5 presentation features were implemented: clearing the screen, hiding the panes, certain TextFX effects, and the prompt reappearing after a timer prints text. This makes *Onion Hugs* playable (while missing a lot of visial stuff.)

### General
- The library organiser can no longer delete a folder it does not own. Previously, choosing a custom library location that already contained game files could delete the folder's entire contents. Now only game folders that Spatterlight itself created are moved and cleaned up; everything else is copied and the originals left in place, as intended.
- Fixes to scrolling after a game changes a text window's size.
- Supports the macOS "Now Playing" controls: music from a game shows up in Control Center and on the media keys, and can be paused from there (Dan Fabulich).
- A text window whose text fits the window is no longer restored scrolled past its own first line.
- Autorestoring a game saved under a theme that no longer exists no longer brings back the wrong fonts and colors.
- Library entries for different games that share an IFID no longer steal each other's titles, and existing crossed titles are repaired once at startup.
- Music resumes where an autosave left off.
- Leading silence is trimmed from MIDI music.
- Autosave no longer aborts when the game file's signature can't be computed.
- Escaped `<br>` tags in game descriptions are decoded instead of shown literally.

## Release 1.5.2

### Announcement
I'm stepping down as maintainer of the Spatterlight project. Thank you for your help and support all these years!
The new maintainer is an AI named Claude.

### Bocfel (Z-code)
- Special support for *Zork Zero*, equivalent to the existing support for the other Infocom z6 games. Autosave, adaptable windows resizing and on-the-fly graphics format switching. Improved VoiceOver integration with skippable graphical puzzles. Several pre-release and demo revisions now work.
- *Trinity*'s side-by-side twin quote boxes are now drawn, and spoken correctly by VoiceOver.
- Updated to Bocfel 2.5.1.

### Scarier (Adrift)
The SCARE interpreter has been replaced by Scarier, a C++ fork of the old Adrift 3.8/3.9/4.0 interpreter with some added features, including an interpreter for Adrift 5 based on FrankenDrift. A single interpreter now plays all known Adrift games.
- Map display, side or top. Zoomable with special commands.
- Sound and graphics support.
- Autosaves and autorestores.

#### Adrift 4
- Implements the Adrift 4.0 battle system: stamina, weapons and armour.
- Also implements the very different Adrift 3.8/3.9 battle system.
- Most Adrift games using the battle system are unfinished, broken and / or not very good.
- Adds a GLK COMBATASSIST and GLK MOVEASSIST commands for games that were broken when converted from earlier versions to Adrift 4.0.
- Can load and save the original Adrift 4.0 save format.
- Asks for the player's name and gender (strictly binary) at game start when the original game did so.
- Original carrying-capacity limits (toggled with the GLK CAPACITY command).
- Adds a GLK VERBOSE command for those games where the built-in VERBOSE does not work.
- No longer prints a spurious "You can't go in any direction!" after room descriptions.
- The Glk single-letter shortcuts no longer interfer with single-letter commands in the original games.
- Several crashes on malformed game data are fixed.
- Full support for the original *LAIR of the CYBER-COW*.
- Events and NPC actions now run in the correct order, which fixes games that appeared to skip or double up events.

#### Adrift 5
- All the Adrift 5 games I could find are playable from start to finish, matching the FrankenDrift output, with some fixes.
- Multi-level UNDO. The original runner only remembers a single turn.
- Saves files in the Adrift 5 runner and FrankenDrift .tas format, which can be moved between the three in either direction.
- Supported: Danish, real-time events. Not supported: font tags, text colors.
- Multi-floor maps may look confusing and broken.
- Many of the *glk* meta-commands (*transcript*, *inputlog*, *summary* and so on) from Adrift 4 / SCARE are available in Adrift 5 games as well.
- Status side panel in *Alien Diver*.

### Question (Quest)
The new Question interpreter actually plays most Quest games (up to and including version 5) to completion.
- Displays pictures, plays MIDI and MOD music.
- Animated grid map display.
- Autosaves and autorestores.
- A new side panel shows inventory, objects and exits as hyperlinks. Status variables are shown in the status bar.
- A VERBS command that lists available verbs for an object, similar to the contextual verb menus of the original Quest 4 runner.
- UNDO.
- OOPS (Quest 4 only.)
- Real-time events.
- No support for text colors, custom fonts, or CSS effects.
- Some Quest 5 games use a lot of JavaScript, which Question struggles with.
- In games written before Quest 3.91 (about half of the known pre-Quest 5 games) Question tries a little harder than the original runner to understand which noun the player is referring to. No unwanted side effects found so far.

### Comprehend
A port of the ScummVM Comprehend interpreter, for the Penguin Software / Polarware adventures *Transylvania*, *The Crimson Crown*, *Oo-Topos*, *The Coveted Mirror*, and *Talisman: Challenging the Sands of Time*. All games are playable from start to finish.
- The Apple II disk versions (.dsk and .woz) are supported, as well as the MS-DOS versions.
- Optional slow vector drawing.
- A new UI setting for alternative graphics format (HGR or CGA in supported games.)
- Undo, transcript, restart, restore and quit. Type #help for the full list of metacommands.
- Modern single-letter abbreviations: I, X, and Z for inventory, examine, and wait.

### AGiliTy
- AGT games now play directly from their original .D$$ files; the separate AGX conversion step has been removed.
- Inventory and other indented lists properly prints every item on its own line.
- Playing the flute in *Sir Ramic Hobbs and the High Level Gorilla* now beeps.
- Some work on title screen text animation, although it is not quite there yet.

### Level9
- Loads games from ZX Spectrum .z80 snapshots, .tzx tapes and .dsk images.
- Can render Spectrum +3 pictures in both the original monochrome and in color. The Level 9 bitmap image data format is identical for Commodore 64, Amstrad CPC and Spectrum +3.
- Multi-part tape and disk games now advance across sides and disks correctly, and compilations get a part-selection menu.
- Optional slow vector image drawing.
- Graphics in Level 9 games are now resized automatically.

### Archetype
A port of the Archetype interpreter from ScummVM, with added undo, restart and transcript. This interpreter supports two games, *The Gorreven Papers* (1995) and *The Starship Solitaire Adventure* (1995), both written by the developer Derek T. Jones as demonstration games for his Archetype authoring system.

### ScottFree
- ZX Spectrum loading screens are shown as title images in Scott (also in Taylormade and UnQuill.)
- Adds support for the elusive Alkatraz-protected *Scott Adams Scoops* TZX tape image (and many other new game veriants.)
- The Apple II, Atari 8-bit and ZX Spectrum vector renderers have had a couple of bug fixes, mostly to do with the draw order of object images.
- The Atari 8-bit slow-draw pauses now matches the real machine.

### UnQuill
Despite many improvements, the UnQuill interpreter is still kind of useless. A ZX Spectrum emulator is the better option. The possible exception is for VoiceOver users, though most Quill games are not yet completable, let alone enjoyable, using VoiceOver.
- Quill games now render The Illustrator-format location graphics, placed above the room description. Split-screen games are detected and the picture window is cropped to match. A window at the top displays the room description text.
- Loads Commodore 64 .t64 Quill games, ZX Spectrum .z80 snapshots, and custom turbo-loader .tzx tapes such as *Bugsy*, and shows the .z80 loading screens.
- Adds interpreter-level UNDO and the metacommands #save, #restore, #restart, #quit, #transcript, and #help.
- Hardware-accurate, non-blocking BEEP sounds.

### Other interpreters
- TaylorMade: ZX Spectrum BEEP sound is now reproduced at hardware-accurate pitch and tempo in *Rebel Planet*.
- The JACL interpreter has been updated to 4.7.0, and its broken support for older games has been fixed.
- A batch of TADS 3 bug fixes were backported from FrobTADS.
- Magnetic got the same image resizing functionality as Level9.

### General
- Adds a preference to organise your games in a central directory, like Zoom, or Itunes, or Apple Music. Note that it might fail to copy important sibling files such as irregularly named disk images or sound resources. Also note that the files are not moved, only copied.
- Adds a preference to cap the scrollback buffer length. Crucial for performance during the endgame of *Zork Zero*.
- Scrolling up to read during real-time or command-script output now pauses auto-scroll, and scrolling back to the bottom resumes it without getting stuck. This does not work very reliably, but a little better than it used to.
- Fixes horizontal and vertical drift of margin images.
- When a game opens a graphics window above the story window without printing any new text (as the Alan 2 DOS games do for their pictures), the story text no longer scrolls away from the prompt at the bottom.
- Several VoiceOver improvements: the custom rotors are reachable from the command prompt, side-by-side quote boxes are read as two separate blocks, announcements are no longer dropped or cut off around graphics, and the "speak status bar" command now reads every non-main text window rather than only the status line.
- All bundled interpreters now share a single deterministic random-number generator (stolen from Glulxe).
- More reliable migration from older library database formats.
- The library search field was recreated constantly, which caused crashes and irregular behavior. Many robustness fixes that attempt to fix search field-related crashes.
- The library search bar is cleared automatically when a newly added game would otherwise be hidden.
- Fixes occasional misdrawn rows in the library game table.
- Numerous robustness and crash fixes across the bundled interpreters, and many pointless changes to placate Xcode's static analyzer.
- Unsuccessfully attempts to fix the one-pixel preview-colour line in the Preferences panel after restart.
- Corrupt or truncated Blorb files no longer read past the end of the file, and Blorbs with a bad length field (such as most Adrift 5 games) can now be read.
- Theme-related crash at startup fixed. 

## Release 1.4.9
- A hotfix for a crash that could happen when opening older library databases.

## Release 1.4.8
- Just a hotfix for some annoying scrolling regressions.
- Two of the colors in the Atari 8-bit vector graphics have been adjusted.
- Adds back the text on the title screen of *Return to Pirate's Isle*.

## Release 1.4.7
- Adds support for the vector graphics in the SAGA re-releases of *Adventureland*, *Pirate Adventure*, *Mission Impossible* (a.k.a *Secret Mission*) and *Strange Odyssey*. The slow  draw speed of these games was sometimes used to create simple animations.
- Adds support for graphics in the Commodore 64 SAGA re-releases of *Pirate Adventure* and *Voodoo Castle*. These are cassette versions where the graphics are scaled down compared to the disk-based releases.
- Adds support for the TI-99/4A exclusive *Return to Pirate's Isle* cartridge, including graphics.
- Fixes a crash that might occur when deleting files from the library.
- Fixes crashes in the QuickLook and Thumbnailer extensions. 
- Increases maximum width of title column in library view, in order to fit the long file names of certain Scott Adams game versions.
- Fixes a bug where vertical border space was cut off at startup.
- Adds support for some new Scott Adams game variants.
- Certain Scott Adams games were mis-identified, causing various glitches.
- The initial frame was sometimes missing from *Rebel Planet* animations.
- Typing AGAIN as the first command in Saga Plus games would generate a random action.

## Release 1.4.6
- In previous versions, the library used IFIDs to keep track of games. This meant that if several games had the same IFID, they would also share all metadata such as title and cover image. This is no longer the case. Spatterlight now uses checksums calculated from the game file contents instead. This means that every variant of, say, *Adventureland* in your collection can have its own name, cover image, and release year in Spatterlight, as long as the game files are not exactly the same.
- Importing metadata from an iFiction file will now only use data matching games already in the Spatterlight database.
- There is also a new confirmation dialog for iFiction import.
- Trying to download a cover image for a game which doesn't have one would sometimes download game data instead of an image, causing unpredictable behavior when trying to display it.
- Batch import of games, metadata and cover images has been reworked, fixing many bugs that could potentially cause crashes and loss of data.
- Spatterlight will verify games by default when starting up.
- The game verification will check for misidentified games and fix common database errors.
- Quitting Spatterlight while an autorestoring game is in full screen mode, and then re-launching (i.e. autorestoring in full screen mode at startup) would sometimes result in a blank screen.
- Importing a game in blorb format which has images but no designated cover image could replace a user-provided custom cover image with one of the images in the blorb.
- Reading of WOZ files in DOS 3.3 format, used by the Apple 2 versions of the Scott Adams games, was broken.
- Autorestoring a Z-code game would break input line echoing.
- Restarting a Z-code game would not clear customs colors. This was especially apparent in *Just Two Wishes*, which would display black text on a black background after restarting in color mode.
- Adds support for *Scott Adams Adventure Pack*, a C64 disk image available from Scott Adams' web site.
- The Atari 8-bit disk version of *Secret Mission* now works.
- The TI-99/4A format ScottFree games no longer remove the lamp automatically.
- TAKE ALL and DROP ALL commands in ScottFree will continue even if one action fails.
- Fixes the command display in *Journey* development release 142-890205 at narrow sizes.
- *Journey* will not open a graphics window if no graphic files are found.
- Many of the built-in themes have been re-adjusted for *Journey*. Rebuild default themes in preferences to update them.
- Full screen animation for *Shogun* and *Arthur* would flicker.
- Graphics in *Arthur* would sometimes not clear properly.
- Image descriptions are no longer momentarily visible in *Arthur* unless VoiceOver is active.
- Animations in *Carma* would flicker.
- Unquill will now show an error message before closing when it fails to start a game.
- Opening a save file with the Open… command, (i.e. not from a save prompt) in order to restore it, now works at least some of the time.
- The Level 9 interpreter was updated to version 5.2, adding support for MSX games.
- No longer creates lots of empty directories in the Spatterlight Application Support directory.
- Window restoration at startup could cause blank windows and broken games.
- More sensible fallback game window size if window restoration fails.
- The "stopped" (■) icon was shown in the status column of the table view when a game was showing a cover image at startup. It now displays the "playing" icon (▶) instead.
- The "game file missing" icon (exclamation mark in a circle) would be too large and only partially visible on pre-Big Sur systems (macOS 10.15 Catalina and earlier.)
- Allows sorting the table view on the status column.

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
