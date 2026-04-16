#include "battle_grid.h"
#include "../data/move_defs.h"
#include <string.h>

// Grid cell dimensions on screen (pixels)
#define CELL_W    70
#define CELL_H    60
#define CELL_PAD   6
// Top-left corners of each grid on screen (800x450 window)
#define PLAYER_GRID_X  40
#define PLAYER_GRID_Y  80
#define ENEMY_GRID_X   430
#define ENEMY_GRID_Y   80

void BattleGridInit(BattleGrid *g)
{
    for (int c = 0; c < GRID_COLS; c++)
        for (int r = 0; r < GRID_ROWS; r++) {
            g->playerSlots[c][r] = GRID_EMPTY;
            g->enemySlots[c][r]  = GRID_EMPTY;
        }
}

void BattleGridPlace(BattleGrid *g, bool isEnemy, int idx, int col, int row)
{
    if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS) return;
    if (isEnemy) g->enemySlots[col][row]  = idx;
    else         g->playerSlots[col][row] = idx;
}

void BattleGridRemove(BattleGrid *g, bool isEnemy, int idx)
{
    for (int c = 0; c < GRID_COLS; c++)
        for (int r = 0; r < GRID_ROWS; r++) {
            if (isEnemy && g->enemySlots[c][r]  == idx) g->enemySlots[c][r]  = GRID_EMPTY;
            if (!isEnemy && g->playerSlots[c][r] == idx) g->playerSlots[c][r] = GRID_EMPTY;
        }
}

bool BattleGridFind(const BattleGrid *g, bool isEnemy, int idx, GridPos *out)
{
    for (int c = 0; c < GRID_COLS; c++)
        for (int r = 0; r < GRID_ROWS; r++) {
            int slot = isEnemy ? g->enemySlots[c][r] : g->playerSlots[c][r];
            if (slot == idx) {
                if (out) { out->col = c; out->row = r; }
                return true;
            }
        }
    return false;
}

bool BattleGridCellEmpty(const BattleGrid *g, bool isEnemy, int col, int row)
{
    if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS) return false;
    int slot = isEnemy ? g->enemySlots[col][row] : g->playerSlots[col][row];
    return slot == GRID_EMPTY;
}

bool BattleGridCanHit(const BattleGrid *g, GridPos attacker, bool attackerIsEnemy,
                      GridPos target, bool targetIsEnemy, int moveRange)
{
    (void)g;
    // Must attack the opposite side
    if (attackerIsEnemy == targetIsEnemy) return false;

    if (moveRange == RANGE_RANGED || moveRange == RANGE_AOE) return true;

    // MELEE: attacker must be in their front column, target in their front column
    // Player front col = 2, enemy front col = 0
    int attackerFront = attackerIsEnemy ? 0 : GRID_COLS - 1;
    int targetFront   = targetIsEnemy   ? 0 : GRID_COLS - 1;
    return (attacker.col == attackerFront && target.col == targetFront);
}

bool BattleGridMoveCombatant(BattleGrid *g, bool isEnemy, int idx, int dir)
{
    GridPos pos;
    if (!BattleGridFind(g, isEnemy, idx, &pos)) return false;

    int nc = pos.col, nr = pos.row;
    if (dir == 0) nr--;       // up
    else if (dir == 1) nc++;  // right
    else if (dir == 2) nr++;  // down
    else if (dir == 3) nc--;  // left

    if (nc < 0 || nc >= GRID_COLS || nr < 0 || nr >= GRID_ROWS) return false;
    if (!BattleGridCellEmpty(g, isEnemy, nc, nr)) return false;

    BattleGridRemove(g, isEnemy, idx);
    BattleGridPlace(g, isEnemy, idx, nc, nr);
    return true;
}

Rectangle BattleGridCellRect(bool isEnemy, int col, int row)
{
    float baseX = isEnemy ? ENEMY_GRID_X  : PLAYER_GRID_X;
    float baseY = isEnemy ? ENEMY_GRID_Y  : PLAYER_GRID_Y;
    return (Rectangle){
        baseX + col * (CELL_W + CELL_PAD),
        baseY + row * (CELL_H + CELL_PAD),
        CELL_W,
        CELL_H
    };
}
