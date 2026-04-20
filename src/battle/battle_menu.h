#ifndef BATTLE_MENU_H
#define BATTLE_MENU_H

#include <stdbool.h>
#include "raylib.h"
#include "combatant.h"
#include "inventory.h"

//----------------------------------------------------------------------------------
// Battle menu panels - root action menu, move select, move-phase cursor
//----------------------------------------------------------------------------------

typedef enum BattleMenuAction {
    BMENU_FIGHT  = 0,
    BMENU_ITEM,
    BMENU_MOVE,
    BMENU_PASS,
    BMENU_ACTION_COUNT,
} BattleMenuAction;

typedef struct BattleMenuState {
    int  rootCursor;    // 0..3 (FIGHT/ITEM/MOVE/PASS)
    int  moveCursor;    // 0..CREATURE_MAX_MOVES-1 (slot index in 3x2 layout)
    int  itemCursor;    // which item slot in inventory
} BattleMenuState;

void BattleMenuInit(BattleMenuState *m);

// Update root action menu cursor; returns selected action or -1 if none yet
int  BattleMenuUpdateRoot(BattleMenuState *m);
// Update move selection cursor; returns selected slot [0..5] or -1 if none yet,
// -2 if the player pressed back. Empty slots (moveIds[slot] == -1) are skipped
// during nav and reject confirm.
int  BattleMenuUpdateMoveSelect(BattleMenuState *m, const Combatant *actor);
// Update item cursor; returns item slot or -1 / -2 (back) if none yet
int  BattleMenuUpdateItemSelect(BattleMenuState *m, int itemCount);

// Draw the bottom panel with 2x2 action grid
void BattleMenuDrawRoot(const BattleMenuState *m);
// Draw the move select panel (4 move slots in 2x2 grid).
// actorInFront: true if the actor is currently in their melee front column,
// which decides whether MELEE moves are shown as usable.
void BattleMenuDrawMoveSelect(const BattleMenuState *m, const Combatant *actor, bool actorInFront);
// Draw the item select panel listing inventory consumables
void BattleMenuDrawItemSelect(const BattleMenuState *m, const Inventory *inv);
// Draw move cursor on the grid (highlights reachable cells)
void BattleMenuDrawMoveCursor(int col, int row, bool isEnemy);
// Draw narration text box at bottom
void BattleMenuDrawNarration(const char *text);

#endif // BATTLE_MENU_H
