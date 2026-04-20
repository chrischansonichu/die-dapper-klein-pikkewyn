#include "game_state.h"
#include "../data/creature_defs.h"
#include "../data/item_defs.h"
#include "../field/map_source.h"
#include <string.h>

void GameStateInit(GameState *gs)
{
    memset(gs, 0, sizeof(GameState));

    gs->currentMapId     = MAP_OVERWORLD_HUB;
    gs->currentMapSeed   = 0;
    gs->currentFloor     = 0;
    gs->hasPendingMap    = false;
    gs->pendingFloor     = 0;
    gs->tempAllyPartyIdx = -1;
    gs->tempAllyNpcIdx   = -1;

    PartyInit(&gs->party);
    PartyAddMember(&gs->party, CREATURE_JAN, 5);

    InventoryAddItem(&gs->party.inventory, ITEM_KRILL_SNACK, 2);
    InventoryAddItem(&gs->party.inventory, ITEM_FRESH_FISH, 1);
}
