# Using VoiceOver with Spatterlight

## Table of Contents
<ul>
<li><a href="#intro">Introduction</a></li>
<li><a href="#kbd-shortcuts">Keyboard shortcuts</a></li>
<li><a href="#vo-rotors">Custom rotors</a></li>
<li><a href="#menu-detect">Menu detection</a></li>
<li><a href="#vo-settings">Settings</a></li>
<li><a href="#journey">Playing Journey</a></li>
<li><a href="#arthur">Playing Arthur and Shogun</a></li>
<li><a href="#vo-utility">VoiceOver Utility settings</a></li>

</ul>

<a id='intro'></a>
## Introduction
First of all, don't expect a smooth, predictable experience using VoiceOver. In particular, there is a tendency to read all the game text from the very beginning of the game, rather than just the latest move, which is what you would want most of the time. I've tried hard to fix this, but this is the best I can do.

Also, to avoid annoying beeps, you may want to switch off Details > Animate scrolling when using VoiceOver. It is the checkbox at the bottom of the Details tab in the Settings window.

With that out of the way, here are the VoiceOver features of Spatterlight.

# Features

<a id='kbd-shortcuts'></a>
## Keyboard shortcuts

These are mostly identical to those found in the Mac interpreter Zoom, if anyone is familiar with that. Note that they will only speak text currently on screen or in the scrollback buffer. If the user or the game clears the screen, all the previous commands are cleared as well.

### Repat Last Move
(shortcut Alt + Command + left arrow)
This is very useful for interrupting VoiceOver whenever it starts reading the game scrollback buffer all the way from the start of the game. Unfortunately, you will sometimes have to press it more than once before it works.
### Speak Previous Move
(shortcut Alt + Command + up arrow)
Use this keyboard shortcut and the next to navigate back and forth through all visible previous moves.
### Speak Next Move
(shortcut Alt + Command + down arrow) 
### Speak Status
(shortcut Alt + Command + Backspace)
This one looks for a status window and reads its text if it finds one. It is particularly useful for games that print the room description in a separate window, such as the Scott Adams games. Depending on the game layout, it may fail to find the status window of certain games. 

<a id='vo-rotors'></a>
## Custom rotors

Custom VoiceOver rotors are activated by pressing VO + U, and then stepping through them with the left and right arrow keys. Their results can be filtered by typing any text. There is aso a free text search rotor, activated by VO + F.

### Command history rotor
 Presents the last 50 commands as a list, with the most recent one at the top. Selecting a command in the list will select the corresponding text in the scrollback. If the game or the user clears the screen, the command history is cleared as well.
  
### Glk Windows rotor
 The Glk screen model that Spatterlight uses has three different types of windows: scrolling text buffer windows, fixed-letter-width text grid windows, and graphics windows. This rotor lists all Glk buffer and grid windows shown by the game. It skips any graphics windows for the sake of simplicity, as some games add a lot of empty graphics windows for layout purposes.
 
### Hyperlinks rotor
 If the game displays hyperlinks, they will be listed here. If it doesn't, this rotor won't be available. It is similar in functionality to opening the built-in item chooser (VO + I) and filtering on “hyperlink”, but is hopefully a little more convenient. It tries to sort the links in an intuitive order. Of course, there is not a lot of games with hyperlinks supported yet, but at least *Dead Cities* should now be playable using only this rotor.

### Images rotor
 This is probably mostly useful in games that provide image descriptions, and those are few and far between. At least *Anchorhead: the Illustrated Edition* and *Counterfeit Monkey* do. This rotor will not be available if the game has no images, or if the setting VoiceOver > Speak Images is set to None.
 
 If you Mac supports VoiceOver Recognition (and you have switched it on and installed its English language support) you can select an image with this rotor, and then get an autogenerated description with the VoiceOver Recognition shortcut (VO + shift + L), though this tends to not work flawlessly. You will ususally get a description of the on-screen text as well.

<a id='menu-detect'></a>
## Menu detection

Infocom's InvisiClues menus set a non-accessible standard for hint menus in parser interactive fiction. Spatterlight tries to detect this kind of menus and describe them using VoiceOver. If it thinks the game is showing a menu, it will speak the currently selected line. That is all it does. You still have to navigate and select items using the keyboard shortcuts of the original games, which tend to vary a lot.

It supports the Infocom InvisiClues-style menus used by many older Infocom and Inform games, the hint menus commonly found in TADS 3 and Hugo games, the menus in *Beyond Zork*, and the *Bureaucracy* forms.

The VoiceOver > "Speak menu lines" setting lets you set the level of verbosity when speaking the individual menu items. If the menu detection misbehaves, it can also be switched off entirely here, with the "Don't detect menus" setting.

<a id='vo-settings'></a>
## VoiceOver settings

These are found on the VoiceOver tab in the Spatterlight Settings window.

### Speak menu lines
The verbosity when reading auto-detected menu lines. Select Don't detect menus to turn off this feature altogether.
### Speak images
Whether to mention inline images in the text. If "All" is selected here, the word "Image" will be spoken when an image without description appears in the text. If "With images" is set, they won't be mentioned. Most games don't have images at all, and those that have very rarely provide image descriptions. *Anchorhead: the Illustrated Edition* and *Counterfeit Monkey* are two exceptions.
 If you Mac supports VoiceOver Recognition (and you have installed it and switched it on) you may get an autogenerated description of a selected image with the VoiceOver Recognition shortcut (VO + shift + L).
### Speak commands
If enabled, typed commands will be spoken after entering them. This also determines whether previous commands are included when navigating the command history.
### Interrupt VoiceOver when appropriate
Whether Spatterlight should try to interrupt VoiceOver's automatic description of the text view. This only triggers when the game window becomes active (that is, when a game starts, when a system dialog or menu closes, or when the user navigates to a different window and back) or when VoiceOver is initially switched on. These are the situations in which VoiceOver by default will start reading the scrollback text all the way from the start. Setting the interruption delay to a longer value means that the interruption is more likely to actually happen, but you might have to listen to up to eight seconds of the beginning of the game again first.

<a id='journey'></a>
## Journey

Any version of *Journey* found at [the Obsessively Complete Infocom Catalog][obsess] should work, except the first one, the Z5 prototype named `journey-dev-r46-s880603.z5`.

[obsess]: https://eblong.com/infocom/#journey "Zarf's the Obsessively Complete Infocom Catalog"

It is recommended to set the interpreter to IBM PC or Apple IIe on the Format settings tab, rather than Amiga or Macintosh. The latter two draws the main screen using graphic characters, which VoiceOver sometimes wants to read individually.

The original game mainly uses two lists for input: a Party list which contains actions taken by the entire group, and an Individual Commands list for actions by individual members. This is now approximated by two new macOS menus at the top of the screen: the ”Journey Party” menu and the ”Individual Commands” menu. These will only be present when VoiceOver is active and there are actions available corresponding to the menus.

For example, at the very start of the game, there is no command menu, so none of the two Journey-specific macOS menus will be available. A few keypresses later, at the start menu, there are still no individual commands, so while the ”Journey Party” menu shows up, the Individual Commands menu is still hidden.

As these menus are at the right end of the menu bar, they can be reached by pressing VO + M followed by the left arrow key.

"Individual Commands" is a menu of submenus: Every character that has available actions, also has a submenu of verbs.

Often these actions will lead to second-order submenus, for example to pick an object for the verb you selected. This is currently handled in Spatterlight by throwing up a modal dialog, which might contain a popup menu if there is more than one choice to select.

I made all this under the assumption that the standard macOS controls would be sufficiently accessible out-of-the-box, but they seem to have a few problems. It is possible that this all has to be re-done in a completely different way.

The main problem is that VoiceOver really wants to read the start of the text buffer ("Our journey started on a day bright and clear …") instead of the results of the latest action. What seems to be happening is that, after selecting an action from a menu or a dialog, focus is returned to the game window, and this notifies VoiceOver that focus has moved to a new accessibility element, which has to be described. I haven't found a reliable way to prevent this.

Therefore, it is recommended to use the Speak Last Move shortcut (Option + Command + left arrow) to interrupt VoiceOver if it starts misbehaving. Sometimes it has to be pressed repeatedly in order to work.

The Command History Rotor (VO + U, see above), which lists all previous moves in the game, can also be useful here.

You can also try experimenting with the setting VoiceOver > Interruption delay. A longer delay is more likely to be successful, but you might have to listen to the beginning of the game narration for eight seconds every time.

For more tips, see <a href="#vo-utility">VoiceOver Utility settings</a> below.

Here is a YouTube video with audio that demonstrates playing the start of the game with VoiceOver. although I'm not sure how helpful it is.

https://youtu.be/9jauDyUu6Ro

You can't tell from the video which buttons are pressed, but you can hear what results (and problems) to expect. It may not be apparent, but at many places in the video I use the Speak Last Move shortcut, sometimes repeatedly, to force VoiceOver to start speaking the results of the latest action.

As an added bonus I created [a blorb file][blorb] where the placeholder image descriptions from the `dev-r46` prototype are added to every image, so that when you navigate the VoiceOver cursor to the graphics window, the image description will be read. These descriptions are usually not helpful and may not accurately describe the images, but some people might still enjoy them. Note that the blorb file has to be renamed to match the game file name, in order for the descriptions to work.

[blorb]: https://www.ifarchive.org/if-archive/infocom/media/blorb/Journey%20%28with%20descriptions%29.blb "Blorb file containing all the images for Journey with descriptions. Should be renamed to match the game file."

<a id='arthur'></a>
## Arthur and Shogun

Just like with *Journey*, most versions of [*Arthur: The Quest for Excalibur*][arthurocic] or [*Shogun*][shogunocic] found at [the Obsessively Complete Infocom Catalog][ocic] should run, but the later ones tend to have less problems. There are some devious game-breaking bugs in early versions of *Shogun*.

In both of these games, the arrow up and arrow down keys may not work with command history unless "Settings > Format > Arrow key usage" is set to "Replaced by ⌘↑ and ⌘↓". This setting also applies to *Beyond Zork*.

When playing *Arthur*, the setting "Format > Redirect text to main window" should also be checked, in order to make sure that error messages are spoken properly.

There is a graphical maze in *Shogun* that is difficult to navigate with a screen reader, but Spatterlight will offer to simplify it before you enter it for the first time.

[arthurocic]: https://eblong.com/infocom/#arthur "Zarf's the Obsessively Complete Infocom Catalog"
[shogunocic]: https://eblong.com/infocom/#shogun "Zarf's the Obsessively Complete Infocom Catalog"
[ocic]: https://eblong.com/infocom/ "Zarf's the Obsessively Complete Infocom Catalog"

<a id='vo-utility'></a>
## VoiceOver Utility settings

After some experimentation with the settings in the VoiceOver Utility, it seems that setting Verbosity > Speech > Text Area to “Custom” and unchecking “Content” might  prevent VoiceOver from reading the entire scrollback from the very start. Of course, this is not an ideal solution, as there are other situations where you *want* selected text areas to describe their contents when selected, such as when navigating the help windows and alert dialogs. Not to mention when using other apps. So you probably at least want to create a custom VoiceOver activity for Spatterlight with this verbosity setting. This can be done in VoiceOver Utility > Activities. (Select Spatterlight in "Automatically switch to this activitiy for:" > "Apps and Websites".)

You might also want to disable some other hints and announcements settings in the VoiceOver utility, such as "Speak instructions for using the item in the VoiceOver cursor" and set "Verbosity > Hints > When an item has custom actions" to "Do Nothing" (to get rid of "Actions available" announcements.)
