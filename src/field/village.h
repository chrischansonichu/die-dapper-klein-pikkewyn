#ifndef VILLAGE_H
#define VILLAGE_H

#include <stdbool.h>
#include "../state/game_state.h"
#include "npc.h"

//----------------------------------------------------------------------------------
// Village — hub-only economy. The game has no abstract currency; instead:
//   * The Keeper runs a barter loop. A fixed rotation of "bring me X" asks,
//     advancing only when the player fulfils one. Rewards scale by the
//     player's (Jan's) level so the same quest pays a stronger weapon and
//     better food as the run progresses.
//   * The Food Bank accepts any consumable food from the inventory and
//     converts each donated item into +1 village reputation.
//
// These helpers are called from BuildNpcInteraction in field.c. They mutate
// GameState directly (consume inventory, advance quest index, bump rep) and
// return the dialogue pages to show the player.
//----------------------------------------------------------------------------------

// Number of keeper quests in the rotation.
#define KEEPER_QUEST_COUNT 3

// Fill *pages with up to 3 dialogue page pointers into scratch. Returns the
// page count. If the player has enough items to fulfil the current keeper
// quest, it is consumed + reward is granted + keeperQuestIdx advances.
// Otherwise a "still need X more" reminder is shown.
int KeeperInteract(GameState *gs, const char **pages, char scratch[4][NPC_DIALOGUE_LEN]);

// Shared test so the Food Bank donation UI and any future hub code classify
// food the same way.
bool VillageIsFoodItem(int itemId);

#endif // VILLAGE_H
