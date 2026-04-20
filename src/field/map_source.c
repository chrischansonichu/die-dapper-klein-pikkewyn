#include "map_source.h"

void BuildHarborFloor1(MapBuildContext *ctx);
void BuildOverworldHub(MapBuildContext *ctx);

void MapBuild(MapId id, MapBuildContext *ctx, unsigned seed)
{
    (void)seed;
    switch (id) {
        case MAP_OVERWORLD_HUB:
            BuildOverworldHub(ctx);
            break;
        case MAP_HARBOR_F1:
            BuildHarborFloor1(ctx);
            break;
        default:
            BuildOverworldHub(ctx);
            break;
    }
}
