#ifndef OVERWORLD_H
#define OVERWORLD_H

#include <stdbool.h>
#include "raylib.h"
#include "tilemap.h"
#include "player.h"
#include "npc.h"
#include "encounter.h"
#include "../battle/party.h"
#include "../systems/camera_system.h"
#include "../systems/dialogue.h"

//----------------------------------------------------------------------------------
// OverworldState - the complete overworld subsystem
//----------------------------------------------------------------------------------

#define OVERWORLD_MAX_NPCS 16

typedef struct OverworldState {
    TileMap       map;
    Player        player;
    Camera2D      camera;

    Npc           npcs[OVERWORLD_MAX_NPCS];
    int           npcCount;

    Party         party;

    // Dialogue
    DialogueBox   dialogue;

    // Pending battle (set when encounter triggers)
    bool          pendingBattle;
    EncounterResult pendingEncounter;
} OverworldState;

void OverworldInit(OverworldState *ow);
void OverworldUpdate(OverworldState *ow, float dt);
void OverworldDraw(const OverworldState *ow);
void OverworldUnload(OverworldState *ow);

#endif // OVERWORLD_H
