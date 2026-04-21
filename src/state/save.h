#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>
#include "game_state.h"

//----------------------------------------------------------------------------------
// Save/Load — flat binary snapshot of the durable parts of GameState.
//
// Only the GameState fields that carry meaning across a run are persisted:
// current map/floor/seed, party roster (stats, XP, moves, durability, status),
// and inventory. Transient fields (pendingMap*, tempAlly*, rescueDialogue) and
// runtime-only combatant state (tile position, CreatureDef pointer) are
// reconstructed on load.
//
// Saves live in `savegame.dat` relative to the binary's working directory
// (set by main's ChangeDirectory to GetApplicationDirectory).
//----------------------------------------------------------------------------------

// Save uses the current GameState plus the player's live field position
// (passed in separately since tile/dir live on FieldState, not GameState).
bool SaveGame(const GameState *gs, int playerTileX, int playerTileY, int playerDir);
// Load restores GameState. If outPlayer{X,Y,Dir} are non-NULL they receive
// the saved player tile/dir so the caller can re-seat the FieldState player
// after FieldInit lands them at the map's default spawn.
bool LoadGame(GameState *gs, int *outPlayerX, int *outPlayerY, int *outPlayerDir);
bool SaveGameExists(void);

#endif // SAVE_H
