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
    NPC_KEEPER,      // hub barter NPC — trades loot for leveled rewards
    NPC_FOOD_BANK,   // hub donation NPC — accepts consumables for reputation
    NPC_SCRIBE,      // hub save NPC — writes the player's progress to disk
} NpcType;

struct FieldEnemy;

typedef struct Npc {
    int     tileX;
    int     tileY;
    int     dir;     // 0=down 1=left 2=right 3=up
    NpcType type;
    bool    active;
    char    dialogue[NPC_MAX_DIALOGUE_PAGES][NPC_DIALOGUE_LEN];
    int     dialogueCount;
    // Captive scene: NPC is tied up while any listed captor is still active.
    // Check via NpcCurrentlyCaptive; overlay (rope + flashing "!") via
    // NpcDrawCaptiveOverlay. captorCount == 0 means "never captive".
    bool    isCaptive;
    int     captorIdxs[2];
    int     captorCount;
} Npc;

void NpcInit(Npc *n, int tileX, int tileY, int dir, NpcType type);
void NpcAddDialogue(Npc *n, const char *text);
// Mark this NPC as a captive held by up to two enemies (by index into
// FieldState.enemies). Pass -1 for unused slots.
void NpcSetCaptors(Npc *n, int enemyIdx0, int enemyIdx1);
// True iff the NPC is flagged captive AND at least one listed captor is
// still active (non-defeated).
bool NpcCurrentlyCaptive(const Npc *n, const struct FieldEnemy *enemies, int enemyCount);
// Returns true if npc is on the tile directly in front of the player.
// NPC facing doesn't matter — the player is the one initiating interaction.
bool NpcIsInteractable(const Npc *n, int playerTileX, int playerTileY, int playerDir);
// Snap NPC to face the given tile (used when the player starts a conversation).
void NpcTurnToFace(Npc *n, int tileX, int tileY);
void NpcDraw(const Npc *n, Camera2D cam);
// Draws rope + flashing "!" on top of the NPC. Caller decides whether to
// invoke based on NpcCurrentlyCaptive.
void NpcDrawCaptiveOverlay(const Npc *n);

#endif // NPC_H
