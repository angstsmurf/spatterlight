# spondre (Jay Nabonne, 2014) — hypertext ResponseLib game, ASL 550, QuestViva oracle
#
# ENDING REACHED: the genuine GOOD ending ("success" path):
#   EndPage_Success lab notes — "The immersion was a complete success today. Patient X
#   eventually opened up..." — followed by Day 1 / Day 2 lore pages, "end game",
#   and the FINALTEXT credits page ("This has been 'spondre' by Jay Nabonne. Thank
#   you for playing!", sets GAMEOVER).
#
# GATES HIT (verified via assert: in a diagnostic run at the repent decision):
#   trusting = 100 (gate: >= 8), cooperative = 35 (gate: >= 2), ignored = 0 (gate: < 4)
#   -> player_repent_crime_case_success, then menu option "Repent" sets `success`,
#      which selects EndPage_Success (game.aslx ~3641-3645 switch; page ~2397).
# Trust economy taken: ask-himself +5t, tell real name +10t, Colintha general +5c,
#   reply-yes +5t, Colintha query +5c, ask-why-here +5t, wife +10t/+5c, child +10t/+5c,
#   accept cigarette +10t, crime truth +10t, sabotage truth +10t/+10c,
#   family truth +10t/-5c, death reveal +10t/+5c, thank him +5t, eat bug +5c.
#
# FINAL STATE: Running (scriptExhausted), NOT Finished — this is the game's own
#   terminal state and Finished is UNREACHABLE in spondre: the game never calls
#   `finish` or request(Quit). Its inlined-Core HandleCommand routes ALL typed
#   input to ResponseLib_Call, so the `quit` command (pattern ^quit$, the only
#   request(Quit) in the file) is dead code; the end page's only affordance is a
#   "Restart" JS reload. The transcript above [state=Running] ends on the full
#   credits page with GAMEOVER set — that IS the win.
#
# BENIGN ERRORS: exactly 1 — "Property 'longtermtopics' not found on ''" at startup
#   (faithful legacy null-pov init error; also present in real QuestViva).
#
# MECHANICS NOTES:
#   - event:TitleDone;0 dismisses the JS title fade; the next command (any word)
#     triggers TitleScreen_Any and moves the player into the cell.
#   - event:FinishLeadin;ROOMFIRST_REST / event:FinishLeadin;FEEDINGTIME stand in
#     for the JS FadeInTitle completion callbacks (headless JS never fires them).
#   - Topic keywords with several possibilities print a bullet menu (game-inlined
#     ShowMenu); answer with the bare 1-based option NUMBER. Any non-number
#     cancels the menu into the silent "ignore" response (ignored:+1) — never
#     send a topic while a menu is open.
#   - Spon only initiates conversation steps (cigarette/crime/family/feeding time)
#     after 4-6 consecutive REFLECT-class turns (yellow topics: window, scream,
#     walls, ceiling...); interact-class responses reset his reflect counter.
#   - event:EndPage_FadeDone;0 stands in for the "end game" fadeOut JS callback
#     and prints the FINALTEXT credits.
event:TitleDone;0
look
event:FinishLeadin;ROOMFIRST_REST
room
walls
beds
left bed
figure
1
old man
2
old man
2
1
Spon
2
1
Colintha
Spon
2
wife
child
window
door
scream
holes
ceiling
child
webs
window
scream
room
1
right bed
2
writing
window
scream
decay
doubt
1
1
window
scream
walls
1
1
2
window
scream
beds
event:FinishLeadin;FEEDINGTIME
ceiling
walls
bugs
2
bugs
2
bugs
2
spiders
spiders
walls
holes
window
ceiling
walls
holes
window
ceiling
repent
sabotage
1
day 1
day 2
today
end game
event:EndPage_FadeDone;0
