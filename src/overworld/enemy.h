#ifndef ENEMY_H
#define ENEMY_H

#include <stdbool.h>
#include "raylib.h"
#include "tilemap.h"

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

    // Alert countdown
    float         alertTimer;
} OverworldEnemy;

// Initialize a standing/wandering enemy.
// For patrol enemies, call EnemySetPatrol afterward.
void EnemyInit(OverworldEnemy *e, int tileX, int tileY, int dir,
               EnemyBehavior behavior, int creatureId, int level,
               int losRange, Color color);

// Set patrol waypoints (only meaningful for BEHAVIOR_PATROL).
void EnemySetPatrol(OverworldEnemy *e, int x0, int y0, int x1, int y1);

// Update one enemy for this frame (dt in seconds).
// Returns true if the enemy has just reached the player and a battle should start.
bool EnemyUpdate(OverworldEnemy *e, const TileMap *map,
                 int playerTileX, int playerTileY, float dt);

// Draw inside BeginMode2D.
void EnemyDraw(const OverworldEnemy *e);

#endif // ENEMY_H
