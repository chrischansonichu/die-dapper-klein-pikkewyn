#ifndef NPC_H
#define NPC_H

#include <stdbool.h>
#include "raylib.h"
#include "tilemap.h"

//----------------------------------------------------------------------------------
// NPC - non-player character with tile position, facing, and dialogue
//----------------------------------------------------------------------------------

#define NPC_MAX_DIALOGUE_PAGES 8
#define NPC_DIALOGUE_LEN       200
#define NPC_SPRITE_SIZE        16

typedef enum NpcType {
    NPC_PENGUIN_ELDER,
    NPC_SEAL,
} NpcType;

typedef struct Npc {
    int     tileX;
    int     tileY;
    int     dir;     // 0=down 1=left 2=right 3=up
    NpcType type;
    bool    active;
    char    dialogue[NPC_MAX_DIALOGUE_PAGES][NPC_DIALOGUE_LEN];
    int     dialogueCount;
} Npc;

void NpcInit(Npc *n, int tileX, int tileY, int dir, NpcType type);
void NpcAddDialogue(Npc *n, const char *text);
// Returns true if npc is adjacent to player and facing player
bool NpcIsInteractable(const Npc *n, int playerTileX, int playerTileY);
void NpcDraw(const Npc *n, Camera2D cam);

#endif // NPC_H
