#ifndef ROOM_TEMPLATES_H
#define ROOM_TEMPLATES_H

//----------------------------------------------------------------------------------
// Room templates — fixed-size tile patterns that the procedural dungeon builder
// stitches into a grid. Each template is a square of floor tiles wrapped in
// rock walls; the proc builder carves doorways at shared edges after placement.
// Enemy anchor coords are room-local and may or may not be filled per seed.
//----------------------------------------------------------------------------------

#define ROOM_W              10
#define ROOM_H              10
#define ROOM_MAX_ENEMIES    3
#define ROOM_TEMPLATE_COUNT 3

typedef struct RoomTemplate {
    int tiles[ROOM_H][ROOM_W]; // TILE_* ids, row-major
    int enemyX[ROOM_MAX_ENEMIES];
    int enemyY[ROOM_MAX_ENEMIES];
    int enemyCount;
} RoomTemplate;

// Returns NULL for out-of-range indices.
const RoomTemplate *GetRoomTemplate(int idx);

#endif // ROOM_TEMPLATES_H
