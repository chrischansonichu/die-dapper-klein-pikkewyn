#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdbool.h>
#include "../battle/party.h"

//----------------------------------------------------------------------------------
// GameState - persistent state that survives map transitions and battles.
// The FieldState is transient (rebuilt each time the player enters a map), but
// GameState carries the party, inventory, and (later) story flags / gold /
// dismissed-member roster through the whole session.
//
// MapId values are `int` here to keep state/ from depending on field/ headers;
// GameStateInit and screen_gameplay include map_source.h to translate.
//----------------------------------------------------------------------------------

typedef struct GameState {
    Party    party;

    // The map the FieldState currently represents, and the seed used to build
    // it (unused for authored maps). FieldInit reads these. `currentFloor` is
    // the dungeon depth (1..9) or 0 when on a non-dungeon map.
    int      currentMapId;
    unsigned currentMapSeed;
    int      currentFloor;

    // Set by the field when the player steps on a warp tile (or later, uses
    // an escape item, or is rescued after defeat). screen_gameplay consumes
    // this in UpdateGameplayScreen and rebuilds the FieldState.
    bool     hasPendingMap;
    int      pendingMapId;
    unsigned pendingMapSeed;
    int      pendingFloor;
    int      pendingSpawnX, pendingSpawnY, pendingSpawnDir;

    // Tracking a temporary ally added for the duration of a captive-rescue
    // battle (e.g., the bound seal). After the battle, field.c either keeps
    // them (if their captors all died) or removes them. -1 when none.
    int      tempAllyPartyIdx;
    int      tempAllyNpcIdx;

    // Set by the battle-defeat path so the next map transition shows the
    // "village patched you up" dialogue. Consumed by screen_gameplay after
    // the hub is rebuilt.
    bool     rescueDialoguePending;

    // Additional persistent state lands here in later phases:
    //   int       gold;
    //   Roster    dismissedMembers;
    //   uint64_t  storyFlags;
} GameState;

// Fresh game: seeds the party with Jan + starter items, lands them in the hub.
void GameStateInit(GameState *gs);

#endif // GAME_STATE_H
