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

// Guide walking (see NpcWalkUpdate). One tile-step takes NPC_MOVE_FRAMES
// frames — slightly slower than PLAYER_MOVE_FRAMES (8) so a following
// player never falls behind at full waddle. The guide pauses whenever the
// player is more than NPC_GUIDE_FOLLOW_RANGE tiles away (Chebyshev).
#define NPC_MOVE_FRAMES        10
#define NPC_GUIDE_FOLLOW_RANGE 4
#define NPC_GUIDE_PATH_MAX     96

typedef enum NpcType {
    NPC_PENGUIN_ELDER,    // top-hat + cane silhouette — village mayor only
    NPC_PENGUIN_VILLAGER, // plain penguin (no hat) — generic crowd / harbor NPCs
    NPC_SEAL,
    NPC_KEEPER,      // hub barter NPC — trades loot for leveled rewards
    NPC_FOOD_BANK,   // hub donation NPC — accepts consumables for reputation
    NPC_SCRIBE,      // hub save NPC — writes the player's progress to disk
    NPC_SALVAGER,    // roaming scrap-picker; trades broken weapons for fish
    NPC_BLACKSMITH,  // hub forge NPC — repairs/upgrades weapons for Reputation
    NPC_CORMORANT,   // Jan's adoptive family on the tutorial island
    NPC_RYNO,        // traveling penguin — tutorial guide; dialogue scripted in field.c
    NPC_RESIDENT,    // lokasie resident (human, friendly) — personaId picks the look + script
} NpcType;

// Npc.personaId values for NPC_RESIDENT — the lokasie's friendly faces.
#define LOK_PERSONA_KID    1   // Thandi — saw the sack (stage 1)
#define LOK_PERSONA_OUMA   2   // Ouma Nomsa — knows the ditch (stage 3)
#define LOK_PERSONA_SPAZA  3   // Bra Vusi — spaza keeper on the road (stage 5)

struct FieldEnemy;

typedef struct Npc {
    int     tileX;
    int     tileY;
    int     dir;     // 0=down 1=left 2=right 3=up
    NpcType type;
    // Distinguishes individuals that share an NpcType (the cormorant family:
    // Ma/Pa/Vlerkie each get scripted farewells). 0 = anonymous.
    int     personaId;
    bool    active;
    char    dialogue[NPC_MAX_DIALOGUE_PAGES][NPC_DIALOGUE_LEN];
    int     dialogueCount;
    // Captive scene: NPC is tied up while any listed captor is still active.
    // Check via NpcCurrentlyCaptive; overlay (rope + flashing "!") via
    // NpcDrawCaptiveOverlay. captorCount == 0 means "never captive".
    bool    isCaptive;
    int     captorIdxs[2];
    int     captorCount;
    // Tile-step walking — mirrors FieldEnemy's step fields. NPCs that never
    // get a guide path stay static exactly as before.
    bool    moving;
    int     targetTileX;
    int     targetTileY;
    int     moveFrames;
    // Guide route: a queue of adjacent waypoints (from FieldFindLandPath).
    // The NPC walks it one step at a time via NpcWalkUpdate, waiting for
    // the player to keep up; on arrival it faces guideFinalDir.
    int     guidePathX[NPC_GUIDE_PATH_MAX];
    int     guidePathY[NPC_GUIDE_PATH_MAX];
    int     guidePathLen;
    int     guidePathIdx;
    int     guideFinalDir;
    // Current tile is water — guide swimming the channel. Set by
    // NpcWalkUpdate; drawn as a ripple + lowered sprite.
    bool    onWater;
    // False = walk the route regardless of where the player is (used when
    // the guide announces where he'll wait and the player has other errands
    // first). True = pause whenever the player falls behind.
    bool    guideWaitForPlayer;
} Npc;

struct FieldState;

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
// Hand the NPC a waypoint route (adjacent tiles, walked front-to-back).
// finalDir is the facing adopted on arrival. len > NPC_GUIDE_PATH_MAX is
// truncated — pass routes from FieldFindLandPath, which respects the cap.
void NpcSetGuidePath(Npc *n, const int *xs, const int *ys, int len,
                     int finalDir, bool waitForPlayer);
// True while the NPC still has route left to walk (or a step in flight).
bool NpcGuideActive(const Npc *n);
// Advance one frame of guide walking: finish the in-flight step, or start
// the next one — unless the player has fallen more than
// NPC_GUIDE_FOLLOW_RANGE behind, in which case the guide stops and turns
// to face them until they catch up. Blocked tiles just retry next frame.
void NpcWalkUpdate(Npc *n, const TileMap *map, const struct FieldState *f,
                   int playerTileX, int playerTileY);
void NpcDraw(const Npc *n, Camera2D cam);
// Draws rope + flashing "!" on top of the NPC. Caller decides whether to
// invoke based on NpcCurrentlyCaptive.
void NpcDrawCaptiveOverlay(const Npc *n);

#endif // NPC_H
