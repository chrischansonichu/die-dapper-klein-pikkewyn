#ifndef MAP_TUTORIAL_H
#define MAP_TUTORIAL_H

#include <stdint.h>
#include "map_source.h"

//----------------------------------------------------------------------------------
// Tutorial island — Jan's hatch-island, the new-game starting map. The tile
// layer + object placements come from the Tiled asset
// resources/maps/tutorial.tmx (authored in maps/tutorial.tmx); the story
// scripting lives in field.c keyed off STORY_FLAG_TUT_* bits.
//----------------------------------------------------------------------------------

void BuildTutorialIsland(MapBuildContext *ctx);

// Open the story-gated swim zones for whichever STORY_FLAG_TUT_* bits are
// set: the east cove after the swim lesson, the north channel after the
// finale. Called by the builder on (re)entry and by field.c the moment a
// gate flag flips mid-play.
void TutorialApplyZones(TileMap *m, uint64_t storyFlags);

#endif // MAP_TUTORIAL_H
