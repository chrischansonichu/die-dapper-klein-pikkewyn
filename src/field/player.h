#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include "raylib.h"
#include "tilemap.h"

// Forward declaration — player.c queries the field for tile occupancy
// (NPCs, other enemies) so it can block the next step without having to know
// the full FieldState layout here.
struct FieldState;

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
    int       scale;
    bool      stepCompleted;  // true for one frame when a step finishes
    bool      onWater;        // last-resolved tile is a water tile — draw as swimming
    int       dryingFrames;   // >0 = paused after stepping from water onto land
    int       turnDelayFrames;// >0 = facing just changed; hold through delay to commit to movement
} Player;

void PlayerInit(Player *p, int startTileX, int startTileY);
void PlayerUpdate(Player *p, const TileMap *m, const struct FieldState *f);
void PlayerDraw(const Player *p);
void PlayerUnload(Player *p);

// Returns the player's current pixel center position (for camera target)
Vector2 PlayerPixelPos(const Player *p);

#endif // PLAYER_H
