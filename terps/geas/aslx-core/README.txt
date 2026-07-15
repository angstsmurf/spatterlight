Quest 5 Core libraries (bundled runtime resources)
==================================================

These .aslx files are the non-editor Core libraries shipped with Quest 5,
copied verbatim from the QuestViva engine
(https://github.com/textadventures/quest, MIT licensed), which are in turn
the same libraries as textadventures/quest branch v5,
WorldModel/WorldModel/Core/.

The Geas native Quest 5 engine resolves a game's <include ref="..."/>
directives against this directory (root first, then Languages/), matching
QuestViva's GetLibraryStream() search order (game-adjacent, then Core/, then
Core/Languages/). See terps/geas/aslx.cc.

Only the runtime libraries are bundled; every CoreEditor*.aslx and
Editor*.aslx (editor-only UI, referenced but skipped at load) is omitted, as
are the .template files (an editor artifact). One version is enough -- Core
itself branches on the game's declared <asl version=>.

To refresh: re-copy the non-editor *.aslx from a current QuestViva checkout's
src/Engine/Core/ and src/Engine/Core/Languages/.
