#include "battle_grid.h"
#include "../field/tilemap.h"
#include "../data/move_defs.h"
#include <stdlib.h>

int TileChebyshev(TilePos a, TilePos b)
{
    int dx = abs(a.x - b.x);
    int dy = abs(a.y - b.y);
    return dx > dy ? dx : dy;
}

bool TileHasLOS(const TileMap *m, TilePos a, TilePos b)
{
    // Standard Bresenham — one step per iteration. Neither endpoint is tested
    // for blocking, so the attacker's own tile and the target's tile are
    // always transparent. Water tiles do not block LOS (only solids do).
    int x = a.x, y = a.y;
    int dx = abs(b.x - a.x);
    int dy = abs(b.y - a.y);
    int sx = a.x < b.x ? 1 : -1;
    int sy = a.y < b.y ? 1 : -1;
    int err = dx - dy;
    while (true) {
        if (x == b.x && y == b.y) return true;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
        if (x == b.x && y == b.y) return true;
        if (TileMapIsSolid(m, x, y)) return false;
    }
}

bool TileMoveReaches(const TileMap *m, TilePos attacker, TilePos target,
                     int moveRange)
{
    if (moveRange == RANGE_AOE || moveRange == RANGE_SELF) return true;
    int d = TileChebyshev(attacker, target);
    if (moveRange == RANGE_MELEE)  return d <= 1;
    if (moveRange == RANGE_RANGED) return d <= 3 && TileHasLOS(m, attacker, target);
    return false;
}
