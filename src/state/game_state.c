#include "game_state.h"
#include "../data/creature_defs.h"
#include "../data/item_defs.h"
#include "../field/map_source.h"
#include <string.h>

void GameStateInit(GameState *gs)
{
    memset(gs, 0, sizeof(GameState));

    // New games open on the tutorial island (Jan's hatch-island); the hub
    // is reached by finishing the tutorial and taking the mainland exit.
    gs->currentMapId     = MAP_TUTORIAL_ISLAND;
    gs->currentMapSeed   = 0;
    gs->currentFloor     = 0;
    gs->hasPendingMap    = false;
    gs->pendingFloor     = 0;
    gs->tempAllyPartyIdx = -1;
    gs->tempAllyNpcIdx   = -1;
    gs->rescueResumeMapId = -1;
    gs->rescueSourceMapId = -1;

    PartyInit(&gs->party);
    PartyAddMember(&gs->party, CREATURE_JAN, 5);

    // Jan starts with nothing but his beak (Tackle). Everything else is
    // earned on the tutorial island: Ryno hands over a FishingHook at the
    // swim lesson, and the ruin cache holds the ShellThrow + sardines.
}

bool GameStateCompleteDungeon(GameState *gs, uint32_t dungeonBit)
{
    if (gs->dungeonsCompleted & dungeonBit) return false;
    gs->dungeonsCompleted |= dungeonBit;
    for (int i = 0; i < gs->party.count; i++) {
        gs->party.members[i].skillPoints++;
    }
    return true;
}

int GameStateAddMember(GameState *gs, int creatureId, int level)
{
    int idx = gs->party.count;
    PartyAddMember(&gs->party, creatureId, level);
    if (idx >= gs->party.count) return -1;
    int points = 0;
    for (uint32_t bits = gs->dungeonsCompleted; bits != 0; bits >>= 1) {
        points += (int)(bits & 1u);
    }
    gs->party.members[idx].skillPoints = points;
    return idx;
}
