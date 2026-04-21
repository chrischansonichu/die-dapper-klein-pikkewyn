#ifndef BATTLE_GRID_H
#define BATTLE_GRID_H

#include <stdbool.h>
#include "raylib.h"

//----------------------------------------------------------------------------------
// Tile-space combat helpers. Combat now runs directly on the dungeon map — range
// checks are Chebyshev distance, not 3x3 cell membership.
//
//   MELEE  = Chebyshev ≤ 1 (8 neighbours)
//   RANGED = Chebyshev ≤ 3 + line of sight (walls block; water passes)
//   AOE    = every living target on the "other" side (filtered at execute time)
//   SELF   = actor only
//
// GridPos stays as a small (col, row) pair for the party-layout preference in
// stats_ui. It no longer participates in combat.
//----------------------------------------------------------------------------------

struct TileMap; // forward decl — battle_grid.c includes tilemap.h

typedef struct TilePos {
    int x;
    int y;
} TilePos;

typedef struct GridPos {
    int col;
    int row;
} GridPos;

// Vestigial: used only by the LAYOUT tab in stats_ui for cosmetic party ordering.
#define GRID_COLS 3
#define GRID_ROWS 3

// Chebyshev tile distance: max(|dx|, |dy|). Diagonals count as 1.
int  TileChebyshev(TilePos a, TilePos b);

// True if `attacker` can reach `target` with a move of the given MoveRange.
// AOE / SELF always return true; filtering happens at execute time.
bool TileMoveReaches(const struct TileMap *m, TilePos attacker, TilePos target,
                     int moveRange);

// 8-directional Bresenham walk from a to b. Interior tiles are tested for
// TileMapIsSolid; the endpoints themselves are not checked (so a combatant
// standing on `b` doesn't block their own cell). Water is transparent to LOS.
bool TileHasLOS(const struct TileMap *m, TilePos a, TilePos b);

#endif // BATTLE_GRID_H
