Quest 5 Core libraries (bundled runtime resources)
==================================================

These .aslx files are the non-editor Core libraries shipped with Quest 5,
copied verbatim from the QuestViva engine
(https://github.com/textadventures/quest, MIT licensed), which are in turn
the same libraries as textadventures/quest branch v5,
WorldModel/WorldModel/Core/.

The Question native Quest 5 engine resolves a game's <include ref="..."/>
directives against this directory (root first, then Languages/), matching
QuestViva's GetLibraryStream() search order (game-adjacent, then Core/, then
Core/Languages/). See terps/question/aslx.cc.

The runtime libraries are bundled, plus CoreEditor.aslx: although it is an
"editor" library, QuestViva loads it at runtime too (it defines editor_player
and a couple of runtime types every editor-made game inherits). Its own 18
CoreEditor*.aslx sub-includes are the actual visual-editor UI -- referenced but
not bundled, so the loader skips them (a missing *editor* ref is a no-op). The
other Editor*.aslx files and the .template files (an editor artifact) are also
omitted. One version is enough -- Core itself branches on the game's declared
<asl version=>.

To refresh: re-copy the non-editor *.aslx (plus CoreEditor.aslx) from the
oracle's QuestViva checkout (test/quest5/harness/oracle/questviva_clone.sh's
QV_REV), src/Engine/Core/ and src/Engine/Core/Languages/, so the native engine
and the oracle run the same Core. Last refreshed 2026-10-17 from 290a87df
(v6.0.0 + 69), which added CorePages.aslx.
