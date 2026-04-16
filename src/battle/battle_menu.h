#ifndef BATTLE_MENU_H
#define BATTLE_MENU_H

#include <stdbool.h>
#include "raylib.h"
#include "combatant.h"

//----------------------------------------------------------------------------------
// Battle menu panels - root action menu, move select, move-phase cursor
//----------------------------------------------------------------------------------

typedef enum BattleMenuAction {
    BMENU_FIGHT  = 0,
    BMENU_ITEM,
    BMENU_SWITCH,
    BMENU_PASS,
    BMENU_ACTION_COUNT,
} BattleMenuAction;

typedef struct BattleMenuState {
    int  rootCursor;    // 0..3 (FIGHT/ITEM/SWITCH/PASS)
    int  moveCursor;    // 0..3 (which move slot)
    int  targetCursor;  // which enemy idx (for ranged target select)
} BattleMenuState;

void BattleMenuInit(BattleMenuState *m);

// Update root action menu cursor; returns selected action or -1 if none yet
int  BattleMenuUpdateRoot(BattleMenuState *m);
// Update move selection cursor; returns selected move slot or -1 if none yet
int  BattleMenuUpdateMoveSelect(BattleMenuState *m, int moveCount);
// Update target cursor; returns enemy idx or -1 if none yet
int  BattleMenuUpdateTarget(BattleMenuState *m, int enemyCount);

// Draw the bottom panel with 2x2 action grid
void BattleMenuDrawRoot(const BattleMenuState *m);
// Draw the move select panel (4 move slots in 2x2 grid)
void BattleMenuDrawMoveSelect(const BattleMenuState *m, const Combatant *actor);
// Draw move cursor on the grid (highlights reachable cells)
void BattleMenuDrawMoveCursor(int col, int row, bool isEnemy);
// Draw narration text box at bottom
void BattleMenuDrawNarration(const char *text);

#endif // BATTLE_MENU_H
