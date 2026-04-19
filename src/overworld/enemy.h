#ifndef ENEMY_H
#define ENEMY_H

#include <stdbool.h>
#include "raylib.h"
#include "tilemap.h"

// Forward declaration — enemy.c queries the overworld for tile occupancy so
// enemies don't walk onto the player, NPCs, or other enemies.
struct OverworldState;

//----------------------------------------------------------------------------------
// OverworldEnemy - visible sailor enemies that patrol/stand/wander and engage
// the player via line-of-sight (Pokemon trainer style).
//----------------------------------------------------------------------------------

#define ENEMY_MOVE_FRAMES  8     // tile-step duration (matches PLAYER_MOVE_FRAMES)
#define ENEMY_ALERT_TIME   0.5f  // seconds of "!" freeze before chasing

typedef enum EnemyBehavior {
    BEHAVIOR_STAND,    // stationary, faces one direction
    BEHAVIOR_WANDER,   // random steps on a timer
    BEHAVIOR_PATROL,   // walks between two waypoints
} EnemyBehavior;

typedef enum EnemyAiState {
    ENEMY_IDLE,
    ENEMY_ALERTED,   // spotted player — show "!" and freeze briefly
    ENEMY_CHASING,   // walking toward player tile-by-tile
} EnemyAiState;

typedef struct OverworldEnemy {
    int           tileX, tileY;
    int           dir;          // 0=down 1=left 2=right 3=up
    EnemyBehavior behavior;
    EnemyAiState  aiState;
    bool          active;       // false = defeated, skip draw/update

    int           losRange;     // LoS distance in tiles
    int           creatureId;   // into creature_defs table
    int           level;
    Color         color;        // placeholder sprite color

    // Wander state
    int           wanderTimer;
    int           wanderInterval;

    // Patrol state
    int           patrolX[2], patrolY[2];
    int           patrolTarget; // 0 or 1

    // Grid-locked movement (same pattern as player)
    bool          moving;
    int           moveFrames;
    int           targetTileX, targetTileY;

    // Walk-cycle animation frame (0 or 1). Advances while moving.
    int           animFrame;
    float         animT;

    // Alert countdown
    float         alertTimer;

    // Drops on defeat. Both are optional; set via EnemySetDrops.
    int           dropItemId;      // -1 = no item drop
    int           dropItemPct;     // 0..100
    int           dropWeaponId;    // -1 = no weapon drop
    int           dropWeaponPct;   // 0..100

    bool          onWater;         // current tile is water — draw as swimming
    int           dryingFrames;    // >0 = paused after stepping from water onto land
} OverworldEnemy;

// Initialize a standing/wandering enemy.
// For patrol enemies, call EnemySetPatrol afterward.
void EnemyInit(OverworldEnemy *e, int tileX, int tileY, int dir,
               EnemyBehavior behavior, int creatureId, int level,
               int losRange, Color color);

// Set patrol waypoints (only meaningful for BEHAVIOR_PATROL).
void EnemySetPatrol(OverworldEnemy *e, int x0, int y0, int x1, int y1);

// Set drops for this enemy. Pass -1 for either ID to disable that drop.
void EnemySetDrops(OverworldEnemy *e, int itemId, int itemPct,
                   int weaponId, int weaponPct);

// Update one enemy for this frame (dt in seconds).
// Returns true if the enemy has just reached the player and a battle should start.
// `selfIdx` is this enemy's index into ow->enemies so collision checks can
// exclude its own tile.
bool EnemyUpdate(OverworldEnemy *e, const TileMap *map,
                 int playerTileX, int playerTileY, float dt,
                 const struct OverworldState *ow, int selfIdx);

// Draw inside BeginMode2D.
void EnemyDraw(const OverworldEnemy *e);

#endif // ENEMY_H
