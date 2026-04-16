#ifndef BATTLE_GRID_H
#define BATTLE_GRID_H

#include <stdbool.h>
#include "raylib.h"

//----------------------------------------------------------------------------------
// Tactical battle grid - 3x3 per side, sides face each other
//
// Layout (screen):
//   Player grid (left)    |  Enemy grid (right)
//   col 0  col 1  col 2   |  col 0  col 1  col 2
//
// Player col 2 is the "front" (closest to enemies).
// Enemy  col 0 is the "front" (closest to players).
// Melee attacks require attacker in their front col, enemy in their front col.
//----------------------------------------------------------------------------------

#define GRID_COLS 3
#define GRID_ROWS 3
#define GRID_EMPTY -1

typedef struct GridPos {
    int col;
    int row;
} GridPos;

typedef struct BattleGrid {
    int playerSlots[GRID_COLS][GRID_ROWS]; // index into party.members[], or GRID_EMPTY
    int enemySlots[GRID_COLS][GRID_ROWS];  // index into enemies[], or GRID_EMPTY
} BattleGrid;

void     BattleGridInit(BattleGrid *g);
// Place a combatant index at a position (isEnemy selects which grid)
void     BattleGridPlace(BattleGrid *g, bool isEnemy, int idx, int col, int row);
// Remove a combatant (on faint)
void     BattleGridRemove(BattleGrid *g, bool isEnemy, int idx);
// Find where a combatant is (returns false if not on grid)
bool     BattleGridFind(const BattleGrid *g, bool isEnemy, int idx, GridPos *out);
// Check if a cell is empty
bool     BattleGridCellEmpty(const BattleGrid *g, bool isEnemy, int col, int row);
// Is a move from src valid to hit target? (checks range rules)
bool     BattleGridCanHit(const BattleGrid *g, GridPos attacker, bool attackerIsEnemy,
                          GridPos target, bool targetIsEnemy, int moveRange);
// Move combatant one step in direction (0=up 1=right 2=down 3=left), returns true if moved
bool     BattleGridMoveCombatant(BattleGrid *g, bool isEnemy, int idx, int dir);

// Draw helpers - returns screen rect for a grid cell
Rectangle BattleGridCellRect(bool isEnemy, int col, int row);

#endif // BATTLE_GRID_H
