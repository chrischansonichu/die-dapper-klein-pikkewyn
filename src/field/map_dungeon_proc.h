#ifndef MAP_DUNGEON_PROC_H
#define MAP_DUNGEON_PROC_H

#include "map_source.h"

//----------------------------------------------------------------------------------
// Procedural dungeon builder. Stitches room templates into a grid, carves doors
// at shared edges, and seeds enemies deterministically from `seed`. Pass a
// non-zero seed to randomize; seed==0 is coerced to a stable constant so
// buggy callers don't get an all-zero RNG state.
//----------------------------------------------------------------------------------

void BuildHarborProcFloor(MapBuildContext *ctx, unsigned seed);

#endif // MAP_DUNGEON_PROC_H
