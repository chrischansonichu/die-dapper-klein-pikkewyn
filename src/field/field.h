#ifndef FIELD_H
#define FIELD_H

#include <stdbool.h>
#include "raylib.h"
#include "tilemap.h"
#include "player.h"
#include "npc.h"
#include "enemy.h"
#include "../battle/party.h"
#include "../systems/camera_system.h"
#include "../systems/dialogue.h"
#include "inventory_ui.h"

//----------------------------------------------------------------------------------
// FieldState - the complete tile-walking field subsystem (hub, dungeons, etc.)
//----------------------------------------------------------------------------------

#define FIELD_MAX_NPCS    16
#define FIELD_MAX_ENEMIES 16
#define FIELD_MAX_PENDING 4   // battle supports up to 4 enemies at once

typedef struct FieldState {
    TileMap       map;
    Player        player;
    Camera2D      camera;

    Npc           npcs[FIELD_MAX_NPCS];
    int           npcCount;

    FieldEnemy    enemies[FIELD_MAX_ENEMIES];
    int           enemyCount;
    // Indices of every enemy that should enter the next battle together.
    // Filled when one or more enemies aggro simultaneously.
    int           pendingEnemyIdxs[FIELD_MAX_PENDING];
    int           pendingEnemyCount;

    Party         party;

    // Dialogue
    DialogueBox   dialogue;

    // Inventory overlay
    InventoryUI   invUi;

    // Pending battle (set when an enemy reaches the player, or when Jan
    // initiates a surprise strike on an unaware enemy)
    bool          pendingBattle;
    bool          preemptiveAttack; // Jan struck first — grant a free Tackle
} FieldState;

void FieldInit(FieldState *f);
// Reload GPU resources (textures) without touching game state.
// Call this instead of FieldInit when returning from battle.
void FieldReloadResources(FieldState *f);
void FieldUpdate(FieldState *f, float dt);
void FieldDraw(const FieldState *f);
void FieldUnload(FieldState *f);

// True if tile (x, y) is occupied by the player, an NPC, or any active
// enemy other than `ignoreEnemyIdx` (pass -1 to check all enemies). An
// enemy mid-step claims both its current tile and its destination.
bool FieldIsTileOccupied(const FieldState *f, int x, int y, int ignoreEnemyIdx);

#endif // FIELD_H
