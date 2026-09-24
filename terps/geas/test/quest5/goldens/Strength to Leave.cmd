# Strength to Leave — Mikayla Mitchell, 2021, ASL 580; derived from game
# source (game.aslx). No published walkthrough; the game's download page is
# textadventures.co.uk/games/view/sulmz88ba0w-e1gbjwqxka/strength-to-leave
# (see games.manifest.tsv) but it hosts no walkthrough. Sole finish() is Security's
# selfuseon["front door"] in the Front Room.
# Puzzle chain traced from source: Connection <- USB+Computer (in the
# Bedroom; the bare noun "computer" first resolves to the *container*
# "computer desk" until it's been examined once, which reveals/disambiguates
# the nested Computer object — "x computer desk" before "use usb on
# computer" is required); Understanding <- Connection + ingredients + give
# macarons to friend (Kitchen); Community <- Understanding + game cartridge
# (Dresser, Bedroom) + game console (Living Room); Confidence <- any of
# {Connection, Community, Understanding, Support} + mirror (Bathroom);
# Security <- Confidence + examining the framed pictures on the mantle
# (Living Room) auto-grants it, no separate "use" needed; win <- Security +
# "use security on front door" (Front Room). errors=0.
south
x coffee table
take usb
west
x computer desk
use usb on computer
use computer
open dresser
x dresser
take game cartridge
east
south
x ingredients
use ingredients
give macarons to friend
north
use game cartridge on game console
use game console
west
south
x mirror
use community on mirror
north
east
x mantle
x framed pictures
north
use security on front door
