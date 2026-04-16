#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include "raylib.h"
#include "tilemap.h"

//----------------------------------------------------------------------------------
// Player - grid-locked tile movement with walk animation
//----------------------------------------------------------------------------------

#define PLAYER_MOVE_FRAMES 8  // frames to complete one tile step

typedef struct Player {
    int       tileX;
    int       tileY;
    int       targetTileX;
    int       targetTileY;
    bool      moving;
    int       moveFrames;
    int       dir;        // 0=down 1=left 2=right 3=up
    int       animFrame;
    float     animT;
    float     animFps;
    Texture2D atlas;      // 16x16 x 4dirs x 2frames layout (same as BuildPenguinAtlas)
    int       scale;
    bool      stepCompleted;  // true for one frame when a step finishes
} Player;

// Build the penguin atlas (shared with screen_gameplay.c logic, but local here)
Texture2D PlayerBuildAtlas(void);

void PlayerInit(Player *p, int startTileX, int startTileY);
void PlayerUpdate(Player *p, const TileMap *m);
void PlayerDraw(const Player *p);
void PlayerUnload(Player *p);

// Returns the player's current pixel center position (for camera target)
Vector2 PlayerPixelPos(const Player *p);

#endif // PLAYER_H
