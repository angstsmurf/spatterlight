# disneys time machine — by riley pomeroy, 2018 (ASL 520, standard parser style,
# not gamebook). No published walkthrough exists — derived from game source
# (game.aslx). The title/subtitle ("some kid travels through time") and the
# "Educational" category are misleading: this is an unfinished/joke first
# game with no time-travel content and no implemented win path.
#
# There is exactly ONE finish() that is reachable through ordinary play: the
# <timer name="get to school"> (interval 120, message "you will be late to
# school... hint: find the garage remote"). Quest 5 <timer> elements fire on
# REAL elapsed wall-clock time (game.timeelapsed, advanced by the UI clock),
# not on turn count — confirmed by running 130+ turns/commands here without
# it firing. A headless, instantaneous command-driven session such as this
# oracle can never make wall-clock time pass, so that timer is structurally
# unreachable in this harness (and the "garage remote" it hints at isn't even
# a takeable object — see below), leaving state=Running as the only outcome.
#
# The other 5 finish() sites are all avoidable joke-deaths, not endings to
# aim for: onhealthzero (only reachable by eating the poster or the clothes,
# both harmless flavour text unless eaten), dropping the kitchen fork
# (ondrop -> finish, "you drop it in the toaster"), "put fork in toaster"
# (putintoaster -> finish), any object's toaster.addscript (finish + a
# gruesome electrocution message), and a hidden joke cheat command
# ("awheckthischeatcodewontwork" -> finish) that isn't part of real play.
#
# Author bugs / incompleteness found via full source parse: the school-side
# rooms (main hall way, algebra, english, band, lunch room, janitors closet)
# are defined but have NO exit anywhere in the source that targets them —
# only "school enterance" is reachable (via buena vista street), and it is a
# dead end. Several described objects have no <take/> and are pure scenery
# despite reading as takeable (screwdriver, body spray, lighter, bbq, bike,
# tv controller, garage remote) — the rec room's "door to the garage is to
# the west" is also wrong; the actual exit is east. The front yard (unlocked
# by wearing/taking the shoes) is an optional dead-end branch with no
# further content.
#
# This script is a full-coverage tour: unlocks the bedroom door (wear
# clothes) and the hole behind the poster (take poster), collects every
# actually-takeable item (clothes, poster, wallet — opened for the $90
# inside, knife, backpack, shoes, phone, fork), visits every reachable room
# including the front yard and bathroom side branches, and stops at school
# enterance — the end of the reachable map — without ever touching the fork
# after taking it (dropping/toasting it is an instant joke-death) or eating
# anything. errors=0.
take clothes
wear clothes
take poster
take wallet
open wallet
in
take knife
out
south
take backpack
east
take shoes
wear shoes
south
north
north
south
east
take phone
take fork
north
north
south
east
out
east
north
west
wait
wait
