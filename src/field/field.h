#ifndef FIELD_H
#define FIELD_H

#include <stdbool.h>
#include "raylib.h"
#include "tilemap.h"
#include "player.h"
#include "npc.h"
#include "enemy.h"
#include "map_source.h"
#include "../systems/camera_system.h"
#include "../systems/dialogue.h"
#include "inventory_ui.h"
#include "stats_ui.h"

// Forward declaration — field.c reads/writes the persistent party + inventory
// through this pointer; ownership lives in screen_gameplay.c.
struct GameState;

//----------------------------------------------------------------------------------
// FieldState - the complete tile-walking field subsystem (hub, dungeons, etc.)
//----------------------------------------------------------------------------------

#define FIELD_MAX_NPCS    16
#define FIELD_MAX_ENEMIES 16
#define FIELD_MAX_WARPS   8
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

    FieldWarp     warps[FIELD_MAX_WARPS];
    int           warpCount;

    // Borrowed pointer to the persistent game state (party, inventory, ...).
    struct GameState *gs;

    // Dialogue
    DialogueBox   dialogue;

    // Inventory overlay
    InventoryUI   invUi;

    // Stats/Layout overlay
    StatsUI       statsUi;

    // Pending battle (set when an enemy reaches the player, or when Jan
    // initiates a surprise strike on an unaware enemy)
    bool          pendingBattle;
    bool          preemptiveAttack; // Jan struck first — grant a free Tackle
} FieldState;

void FieldInit(FieldState *f, struct GameState *gs);
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
