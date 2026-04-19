#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "../battle/party.h"

//----------------------------------------------------------------------------------
// GameState - persistent state that survives map transitions and battles.
// The FieldState is transient (rebuilt each time the player enters a map), but
// GameState carries the party, inventory, and (later) story flags / gold /
// dismissed-member roster through the whole session.
//----------------------------------------------------------------------------------

typedef struct GameState {
    Party  party;
    // Additional persistent state lands here in later phases:
    //   int       currentMapId;
    //   int       spawnTileX, spawnTileY, spawnDir;
    //   int       gold;
    //   Roster    dismissedMembers;
    //   uint64_t  storyFlags;
} GameState;

// Fresh game: seeds the party with Jan + starter items.
void GameStateInit(GameState *gs);

#endif // GAME_STATE_H
