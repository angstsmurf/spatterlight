# Fixes for common problems

## Weird text in status bar

If text is cut off or line spacing looks wrong in the status bar or other grid text windows, it might be fixed by changing the paragraph settings of the Grid Normal style.

You do this on the Glk Styles tab of the settings window, by setting the pop-up menus below the line (set to Buffer Input by default) to Grid Normal (the first one to Grid, the second to Normal) and then opening the paragraph settings pop-up sheet for this style by clicking the "ellipsis in a circle" button to the right.

Baseline offset and Max line height are the most likely culprits.

## Getting rid of game information

When a game is deleted on the Interactive Fiction library window, Spatterlight will remember all metadata such as title and cover image, so that they will still be there if you open the deleted game again. To completely get rid of all metadata, select the "Prune Library…" command by holding down the option key (⌥) while opening the File menu. As a nuclear option, you may also "Delete Library…", which will delete all metadata along with all your games. 

## Slowdowns

If you have played a game for a long time and it has printed a lot of text to the scrolling main window without ever erasing it, things may eventually become slow and unresponsive. You may be able to fix this with the File > Clear scrollback command, which will delete all the text printed so far.
