#ifndef MAP_DUNGEON_PROC_H
#define MAP_DUNGEON_PROC_H

#include "map_source.h"

//----------------------------------------------------------------------------------
// Procedural dungeon builder. Stitches room templates into a grid, carves doors
// at shared edges, and seeds enemies deterministically from `seed`. Pass a
// non-zero seed to randomize; seed==0 is coerced to a stable constant so
// buggy callers don't get an all-zero RNG state. `floor` is the dungeon depth
// (2..8); it seeds level-scaling and decides whether the stairs-down warp
// targets the next proc floor or the authored final floor (F9).
//----------------------------------------------------------------------------------

void BuildHarborProcFloor(MapBuildContext *ctx, int floor, unsigned seed);

#endif // MAP_DUNGEON_PROC_H
