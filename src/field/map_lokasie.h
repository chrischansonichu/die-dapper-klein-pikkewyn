#ifndef MAP_LOKASIE_H
#define MAP_LOKASIE_H

#include <stdint.h>
#include "map_source.h"

//----------------------------------------------------------------------------------
// Level 2 — the Sinkbaai lokasie. Six TMX-authored stages (MAP_LOKASIE with
// floor = 1..6), loaded from resources/maps/lokasie_s<N>.tmx. The maps are
// drafted by tools/gen_lokasie_maps.py and stay editable in Tiled; object
// names in the tmx are the contract with this builder, and every one has a
// hard-coded fallback so a renamed object degrades to the shipped layout.
//
//   S1 Strandkant   — the beach below the shacks; poachers in the surf
//   S2 Die Stege    — alleys; a paraffin drum against a compound wall
//   S3 Die Sloot    — the storm-water ditch; fences force the swim
//   S4 Die Werf     — the runners' scrap yard; padlocked store shed
//   S5 Die Hoofpad  — the tar road to the gate; the whole crew
//   S6 Sangoma se Erf — the walled compound and the shut rondavel door
//
// Story scripting (residents' dialogue, gate interactions, arrival scenes)
// lives in field.c keyed off STORY_FLAG_LOK_* bits.
//----------------------------------------------------------------------------------

#define LOKASIE_STAGE_COUNT 6

// Drum ids (FieldObject.dataId on OBJ_DRUM) — one per stage that has one.
#define LOK_DRUM_S2 0
#define LOK_DRUM_S5 1
#define LOK_DRUM_COUNT 2

void BuildLokasieStage(MapBuildContext *ctx, int stage);

// Player spawn for a stage — used by the previous stage's exit warp, the
// hub gate, the dev warp menu, and easy-mode resume. Mirrors the "Spawn"
// point in each tmx (the C table is authoritative for gameplay).
void LokasieStageSpawn(int stage, int *outX, int *outY, int *outDir);

// Blow the wall behind drum `drumId`: repaint the drum tile and its hole
// zone as scorched ground and clear their SOLID flags. The zone comes from
// the "Hole" rect object recorded at build time, so this only works on the
// stage that placed the drum. Called by the builder (restoring from the
// story flag) and by field.c the moment the drum is hit.
void LokasieApplyBlast(TileMap *m, int drumId);

// Story-flag bit for a drum / chest id, so field.c and the builder agree.
uint64_t LokasieDrumFlag(int drumId);

#endif // MAP_LOKASIE_H
