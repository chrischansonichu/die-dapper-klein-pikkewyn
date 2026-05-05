#ifndef BLACKSMITH_UI_H
#define BLACKSMITH_UI_H

#include <stdbool.h>
#include "../battle/party.h"

//----------------------------------------------------------------------------------
// BlacksmithUI — village forge modal. Three tabs:
//   UPGRADE: pick a weapon (bag or equipped), spend scrap to bump its
//            upgrade level by 1 (+10% damage, +10% max durability per bump).
//            Cost: 2 × current effective level scrap. Capped at +3.
//   MELT:    pick a weapon, destroy it for scrap equal to its current
//            effective level (base + upgrade).
//   REPAIR:  pick a weapon, pay 1 reputation per durability point restored.
//            Restores to the weapon's max durability at its current upgrade
//            level. Confirm screen shows the cost before charging.
//
// Scrap is held by the blacksmith on the player's behalf (GameState slot),
// not in the player's inventory.
//----------------------------------------------------------------------------------

#define BLACKSMITH_MAX_ENTRIES  40   // bag (16) + party_max(4) * 6 slots

typedef enum BlacksmithTab {
    BS_TAB_UPGRADE = 0,
    BS_TAB_MELT,
    BS_TAB_REPAIR,
    BS_TAB_COUNT,
} BlacksmithTab;

typedef enum BSPhase {
    BS_PHASE_PICK = 0,
    BS_PHASE_CONFIRM,
    BS_PHASE_RESULT,
} BSPhase;

// Source of a listed weapon — either the shared bag or a specific combatant
// move slot. Equipped weapons can be operated on directly without forcing the
// player to unequip first.
typedef struct BSEntry {
    int kind;       // 0 = bag, 1 = equipped
    int bagIdx;     // valid when kind == 0
    int memberIdx;  // valid when kind == 1 (party slot 0..count-1)
    int slot;       // valid when kind == 1 (combatant move slot 0..CREATURE_MAX_MOVES-1)
} BSEntry;

typedef struct BlacksmithUI {
    bool          active;
    BlacksmithTab tab;
    BSPhase       phase;

    int           entryCount;
    BSEntry       entries[BLACKSMITH_MAX_ENTRIES];

    int           selectedEntry;  // index into entries[], valid in CONFIRM/RESULT

    // Last-known indices used to refresh entries[] when the inventory mutates.
    // The picker rebuilds on every update tick; CONFIRM / RESULT freeze the
    // entry until the player exits the action.

    char          resultLine1[120];
    char          resultLine2[120];

    float         scrollX;        // horizontal scroll for the weapon strip
} BlacksmithUI;

void BlacksmithUIInit(BlacksmithUI *b);
bool BlacksmithUIIsOpen(const BlacksmithUI *b);

void BlacksmithUIOpen(BlacksmithUI *b, const Party *party);
void BlacksmithUIClose(BlacksmithUI *b);

void BlacksmithUIUpdate(BlacksmithUI *b, Party *party,
                        int *villageReputation, int *blacksmithScrap);

void BlacksmithUIDraw(const BlacksmithUI *b, const Party *party,
                      int villageReputation, int blacksmithScrap);

#endif // BLACKSMITH_UI_H
