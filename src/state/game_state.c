#include "game_state.h"
#include "../data/creature_defs.h"
#include "../data/item_defs.h"
#include <string.h>

void GameStateInit(GameState *gs)
{
    memset(gs, 0, sizeof(GameState));

    PartyInit(&gs->party);
    PartyAddMember(&gs->party, CREATURE_JAN, 5);

    InventoryAddItem(&gs->party.inventory, ITEM_KRILL_SNACK, 2);
    InventoryAddItem(&gs->party.inventory, ITEM_FRESH_FISH, 1);
}
