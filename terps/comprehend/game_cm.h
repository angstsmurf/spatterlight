/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef GLK_COMPREHEND_GAME_CM_H
#define GLK_COMPREHEND_GAME_CM_H

#include "game_opcodes.h"

namespace Glk {
namespace Comprehend {

/**
 * The Coveted Mirror (Polarware, 1985). A version 2 Comprehend game built like
 * OO-Topos and Talisman: a single packed "G0" main data file, with room (RA-RG)
 * and object (OA-OG) pictures and the title in a separate vector image (T0).
 *
 * This is a first-pass port: detection and the data/graphic file wiring are in
 * place, but the game-specific special opcodes and extra string banks (MA-ML on
 * the Apple II disks) still need to be mapped from play-testing.
 */
class CovetedMirrorGame : public ComprehendGameV2 {
public:
	CovetedMirrorGame();
	~CovetedMirrorGame() override {}

	void beforeGame() override;
	void handleAction(Sentence *sentence) override;
	void beforeTurn() override;
	void handleSpecialOpcode() override;
	bool handle_restart() override;

private:
	// Last room for which the engine's wandering-NPC spawn was rolled (the
	// original keeps this in $4037 and only re-rolls on entering a new room).
	uint8 _lastSpawnRoom = 0;

	void spawnWanderingNPCs();
	void moveCarriedItemsTo(uint8 room);
};

} // namespace Comprehend
} // namespace Glk

#endif
