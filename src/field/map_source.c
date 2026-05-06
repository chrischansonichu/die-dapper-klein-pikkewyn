#include "map_source.h"
#include "map_dungeon_proc.h"

void BuildHarborFloor1(MapBuildContext *ctx);
void BuildHarborFloor6(MapBuildContext *ctx);
void BuildHarborFloor7(MapBuildContext *ctx);
void BuildOverworldHub(MapBuildContext *ctx);

void MapBuild(MapId id, int floor, MapBuildContext *ctx, unsigned seed)
{
    switch (id) {
        case MAP_OVERWORLD_HUB:
            BuildOverworldHub(ctx);
            break;
        case MAP_HARBOR_F1:
            BuildHarborFloor1(ctx);
            break;
        case MAP_HARBOR_PROC:
            BuildHarborProcFloor(ctx, floor, seed);
            break;
        case MAP_HARBOR_F6:
            BuildHarborFloor6(ctx);
            break;
        case MAP_HARBOR_F7:
            BuildHarborFloor7(ctx);
            break;
        default:
            BuildOverworldHub(ctx);
            break;
    }
}
