#ifndef OVERWORLD_H
#define OVERWORLD_H

#include <stdbool.h>
#include "raylib.h"
#include "tilemap.h"
#include "player.h"
#include "npc.h"
#include "enemy.h"
#include "../battle/party.h"
#include "../systems/camera_system.h"
#include "../systems/dialogue.h"

//----------------------------------------------------------------------------------
// OverworldState - the complete overworld subsystem
//----------------------------------------------------------------------------------

#define OVERWORLD_MAX_NPCS    16
#define OVERWORLD_MAX_ENEMIES 16

typedef struct OverworldState {
    TileMap       map;
    Player        player;
    Camera2D      camera;

    Npc           npcs[OVERWORLD_MAX_NPCS];
    int           npcCount;

    OverworldEnemy enemies[OVERWORLD_MAX_ENEMIES];
    int            enemyCount;
    int            pendingEnemyIdx; // index of enemy that triggered current battle (-1=none)

    Party         party;

    // Dialogue
    DialogueBox   dialogue;

    // Pending battle (set when an enemy reaches the player)
    bool          pendingBattle;
} OverworldState;

void OverworldInit(OverworldState *ow);
// Reload GPU resources (textures) without touching game state.
// Call this instead of OverworldInit when returning from battle.
void OverworldReloadResources(OverworldState *ow);
void OverworldUpdate(OverworldState *ow, float dt);
void OverworldDraw(const OverworldState *ow);
void OverworldUnload(OverworldState *ow);

#endif // OVERWORLD_H
