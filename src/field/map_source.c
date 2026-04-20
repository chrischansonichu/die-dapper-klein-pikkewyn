#include "map_source.h"
#include "map_dungeon_proc.h"

void BuildHarborFloor1(MapBuildContext *ctx);
void BuildOverworldHub(MapBuildContext *ctx);

void MapBuild(MapId id, MapBuildContext *ctx, unsigned seed)
{
    switch (id) {
        case MAP_OVERWORLD_HUB:
            BuildOverworldHub(ctx);
            break;
        case MAP_HARBOR_F1:
            BuildHarborFloor1(ctx);
            break;
        case MAP_HARBOR_PROC_F2:
            BuildHarborProcFloor(ctx, seed);
            break;
        default:
            BuildOverworldHub(ctx);
            break;
    }
}
