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

// Npc.personaId values for the cormorant family — lets field.c script
// individual farewell scenes for NPCs that share NPC_CORMORANT.
#define TUT_PERSONA_MA   1
#define TUT_PERSONA_PA   2
#define TUT_PERSONA_SIB  3

void BuildTutorialIsland(MapBuildContext *ctx);

// Open the story-gated swim zones for whichever STORY_FLAG_TUT_* bits are
// set: the east cove after the swim lesson, the north channel once the
// family goodbyes are done and Ryno gives the word. Called by the builder
// on (re)entry and by field.c the moment a gate flag flips mid-play.
void TutorialApplyZones(TileMap *m, uint64_t storyFlags);

// Append the drying-rack gull mob to `enemies` (latent unless `active`).
// Called by the map builder on rebuilds that land mid-raid, and by field.c
// live when Ryno's ranged lesson lands — the field persists across battles,
// so the mob can't wait for the next map rebuild to exist.
struct FieldEnemy;
void TutorialSpawnRaidGulls(struct FieldEnemy *enemies, int *enemyCount,
                            int enemyMax, bool active);

#endif // MAP_TUTORIAL_H
