#ifndef FIELD_H
#define FIELD_H

#include <stdbool.h>
#include "raylib.h"
#include "tilemap.h"
#include "player.h"
#include "npc.h"
#include "enemy.h"
#include "field_object.h"
#include "map_source.h"
#include "../systems/camera_system.h"
#include "../systems/dialogue.h"
#include "../battle/battle.h"
#include "inventory_ui.h"
#include "stats_ui.h"
#include "donation_ui.h"
#include "salvager_ui.h"
#include "blacksmith_ui.h"
#include "discard_ui.h"
#include "dev_warp_ui.h"
#include "../dev/style_preview.h"
#include "../systems/fab_menu.h"

// Forward declaration — field.c reads/writes the persistent party + inventory
// through this pointer; ownership lives in screen_gameplay.c.
struct GameState;

//----------------------------------------------------------------------------------
// FieldState - the complete tile-walking field subsystem (hub, dungeons, etc.)
// Combat runs inline on the same tilemap — `mode` gates between free movement
// and turn-based battle.
//----------------------------------------------------------------------------------

#define FIELD_MAX_NPCS    16
#define FIELD_MAX_ENEMIES 16
#define FIELD_MAX_WARPS   8
#define FIELD_MAX_OBJECTS 8

typedef enum FieldMode {
    FIELD_FREE = 0,
    FIELD_BATTLE,
} FieldMode;

typedef struct FieldState {
    TileMap       map;
    Player        player;
    Camera2D      camera;

    Npc           npcs[FIELD_MAX_NPCS];
    int           npcCount;

    FieldEnemy    enemies[FIELD_MAX_ENEMIES];
    int           enemyCount;

    FieldWarp     warps[FIELD_MAX_WARPS];
    int           warpCount;

    FieldObject   objects[FIELD_MAX_OBJECTS];
    int           objectCount;

    // Borrowed pointer to the persistent game state (party, inventory, ...).
    struct GameState *gs;

    // Dialogue
    DialogueBox   dialogue;

    // Inventory overlay
    InventoryUI   invUi;

    // Stats/Layout overlay
    StatsUI       statsUi;

    // Food-bank donation picker (opened by NPC_FOOD_BANK interaction).
    DonationUI    donationUi;

    // Salvager trade picker (opened by NPC_SALVAGER interaction) — hands over
    // broken weapons in exchange for fish.
    SalvagerUI    salvagerUi;

    // Forge/repair modal (opened by NPC_BLACKSMITH when captainDefeated).
    BlacksmithUI  blacksmithUi;

    // Bag-full discard prompt — opened whenever a weapon is about to enter a
    // full bag (post-battle drops, keeper rewards, inventory unequip). Holds a
    // pending weapon until the player picks one to toss or cancels.
    DiscardUI     discardUi;

    // Developer-only warp cheat — F9 opens a picker listing every destination
    // so we can jump straight to a floor for testing. Gated by DEV_BUILD at
    // the input site in field.c; the struct field compiles unconditionally.
    DevWarpUI     devWarpUi;

    // Developer-only visual-style preview room — F10 toggles a full-screen
    // overlay that renders the same mini-scene in each candidate style, so we
    // can pick a direction by looking. Does not touch GameState.
    StylePreview  stylePreview;

    // Warp confirmation prompt — -1 when no prompt is open, otherwise an
    // index into warps[]. Set by facing + Z'ing a warp tile; confirmed with
    // Z/Enter (applies the transition) or cancelled with X/Esc.
    int           warpPromptIdx;

    // Inline battle sub-state. FIELD_BATTLE pauses enemy patrol AI and routes
    // input through BattleUpdate; the battle writes back into party combatants
    // and the FieldEnemy array via enemyFieldIdx in BattleContext.
    FieldMode     mode;
    BattleContext battle;

    // Mobile/wasm floating menu — tap-in button in the canvas corner that
    // reaches stats, inventory, and save without a keyboard.
    FabMenu       fab;
} FieldState;

void FieldInit(FieldState *f, struct GameState *gs);
// Reload GPU resources (textures) without touching game state.
void FieldReloadResources(FieldState *f);
void FieldUpdate(FieldState *f, float dt);
void FieldDraw(const FieldState *f);
void FieldUnload(FieldState *f);

// True if tile (x, y) is occupied by the player, an NPC, or any active
// enemy other than `ignoreEnemyIdx` (pass -1 to check all enemies). An
// enemy mid-step claims both its current tile and its destination.
bool FieldIsTileOccupied(const FieldState *f, int x, int y, int ignoreEnemyIdx);

#endif // FIELD_H
