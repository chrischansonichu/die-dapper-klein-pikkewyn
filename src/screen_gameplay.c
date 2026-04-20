/**********************************************************************************************
*
*   Die Dapper Klein Pikkewyn - Gameplay Screen (Field)
*   Hosts the tile-based field with player movement, NPCs, and encounter triggers.
*
**********************************************************************************************/

#include "raylib.h"
#include "screens.h"
#include "field/field.h"
#include "field/enemy.h"
#include "state/game_state.h"
#include "battle/inventory.h"
#include "battle/combatant.h"
#include "data/item_defs.h"
#include "data/move_defs.h"
#include "data/creature_defs.h"
#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int  finishScreen  = 0;
static bool gInitialized  = false;  // false until first FieldInit
static bool gShowRescueDialogue = false; // set on defeat → triggers dialogue after hub transition
static GameState gGameState = {0};
static FieldState gField = {0};

// Persistent storage for the drop dialogue pages — DialogueBegin keeps pointers
// into these buffers, so they must outlive the call.
#define DROP_MSG_PAGES 2
#define DROP_MSG_LEN   160
static char gDropMsg[DROP_MSG_PAGES][DROP_MSG_LEN];

// Rescue greeting — shown when a captive ally is freed and joins the party.
// Kept alongside drop pages so both can flow into one DialogueBegin.
#define RESCUE_GREET_LEN 160
static char gRescueGreet[RESCUE_GREET_LEN];

// Rescue dialogue storage — same lifetime reasoning as gDropMsg: DialogueBegin
// keeps pointers, so the buffers must outlive the call.
#define RESCUE_MSG_PAGES 2
#define RESCUE_MSG_LEN   160
static const char *gRescueMsg[RESCUE_MSG_PAGES] = {
    "A fisherman pulled you out of the water and carried you back to the village.",
    "You're safe. Rest up, gather your courage, and head back when you're ready.",
};

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------

// Roll drops on the defeated enemy, append to inventory, and prepare a dialogue
// describing what was picked up. Returns number of dialogue pages populated.
static int RollEnemyDrops(FieldEnemy *e, Inventory *inv)
{
    int pages = 0;
    if (e->dropItemId >= 0 && GetRandomValue(1, 100) <= e->dropItemPct) {
        const ItemDef *it = GetItemDef(e->dropItemId);
        if (InventoryAddItem(inv, e->dropItemId, 1)) {
            snprintf(gDropMsg[pages], DROP_MSG_LEN, "Got %s!", it->name);
            pages++;
        }
    }
    if (e->dropWeaponId >= 0 && GetRandomValue(1, 100) <= e->dropWeaponPct && pages < DROP_MSG_PAGES) {
        const MoveDef *mv = GetMoveDef(e->dropWeaponId);
        if (mv->isWeapon && InventoryAddWeapon(inv, e->dropWeaponId, mv->defaultDurability)) {
            snprintf(gDropMsg[pages], DROP_MSG_LEN,
                     "Picked up a %s! (open inventory with I to equip)", mv->name);
            pages++;
        }
    }
    return pages;
}

void InitGameplayScreen(void)
{
    finishScreen = 0;

    if (!gInitialized) {
        GameStateInit(&gGameState);
        FieldInit(&gField, &gGameState);
        gInitialized = true;
        return;
    }

    BattleResult br = GetLastBattleResult();

    if (br == BATTLE_DEFEAT) {
        // A temp captive ally from a rescue battle doesn't survive a loss —
        // by definition the rescue failed, so drop them before the village
        // patch-up so they don't ride home as a free permanent party member.
        // The captive NPC on the source map is untouched and will rebuild
        // when the player re-enters the dungeon.
        if (gGameState.tempAllyPartyIdx >= 0) {
            PartyRemoveMember(&gGameState.party, gGameState.tempAllyPartyIdx);
            gGameState.tempAllyPartyIdx = -1;
            gGameState.tempAllyNpcIdx   = -1;
        }
        // Rescue flow: the village patches the party up and sets a pending
        // transition back to the hub. The actual field swap + rescue dialogue
        // are handled by ApplyPendingMapTransition in UpdateGameplayScreen,
        // so the current field (harbor, proc floor, whatever) gets torn down
        // properly before the hub is rebuilt.
        PartyHealAll(&gGameState.party);
        gGameState.hasPendingMap   = true;
        gGameState.pendingMapId    = MAP_OVERWORLD_HUB;
        gGameState.pendingMapSeed  = 0;
        gGameState.pendingFloor    = 0;
        gGameState.pendingSpawnX   = 11;  // just inside the south gate
        gGameState.pendingSpawnY   = 13;
        gGameState.pendingSpawnDir = 3;   // facing up, into the village
        gShowRescueDialogue = true;
        return;
    }

    // Returning from battle normally — reload textures, preserve all state.
    FieldReloadResources(&gField);

    // Collect dialogue pages from two sources: enemy drops + rescued-ally
    // greeting. Drops are narrated for the first enemy that produced any;
    // rescue greeting appends once the temp-ally resolution is done.
    const char *ptrs[DROP_MSG_PAGES + 1];
    int pageCount = 0;

    // Deactivate every enemy that was in the encounter + roll drops on each.
    // Drops from the first enemy that produced any get narrated; the rest
    // still land in the inventory silently so the player isn't buried in
    // dialogue pages after a big group fight.
    if (br == BATTLE_VICTORY) {
        for (int k = 0; k < gField.pendingEnemyCount; k++) {
            int idx = gField.pendingEnemyIdxs[k];
            if (idx < 0 || idx >= gField.enemyCount) continue;
            FieldEnemy *e = &gField.enemies[idx];
            int pages = RollEnemyDrops(e, &gGameState.party.inventory);
            e->active = false;
            if (pageCount == 0 && pages > 0) {
                for (int i = 0; i < pages; i++) ptrs[pageCount++] = gDropMsg[i];
            }
        }
    }
    gField.pendingEnemyCount = 0;

    // Resolve any temp captive ally from a rescue battle.
    if (gGameState.tempAllyPartyIdx >= 0) {
        int partyIdx = gGameState.tempAllyPartyIdx;
        int npcIdx   = gGameState.tempAllyNpcIdx;
        bool keepAlly = false;
        if (br == BATTLE_VICTORY && npcIdx >= 0 && npcIdx < gField.npcCount) {
            // Captors were all marked inactive above → NpcCurrentlyCaptive is
            // false when the rescue succeeded. Any other result (flee, defeat,
            // victory with captors somehow still active) drops the ally.
            keepAlly = !NpcCurrentlyCaptive(&gField.npcs[npcIdx],
                                            gField.enemies, gField.enemyCount);
        }
        if (keepAlly && partyIdx < gGameState.party.count) {
            Combatant *ally = &gGameState.party.members[partyIdx];
            CombatantClearStatus(ally, STATUS_BOUND);
            gField.npcs[npcIdx].active = false;
            snprintf(gRescueGreet, RESCUE_GREET_LEN,
                     "Arf! Thanks for the rescue! %s joins your party.",
                     ally->name);
            ptrs[pageCount++] = gRescueGreet;
        } else {
            PartyRemoveMember(&gGameState.party, partyIdx);
        }
        gGameState.tempAllyPartyIdx = -1;
        gGameState.tempAllyNpcIdx   = -1;
    }

    if (pageCount > 0)
        DialogueBegin(&gField.dialogue, ptrs, pageCount, 40.0f);
}

// Rebuild the FieldState against the pending map. Called when the player
// steps on a warp tile (step 7) — later also for the escape item and the
// rescue-after-defeat flow (step 9). The new map's builder emits its default
// spawn; we overwrite it with the warp's target so the player always arrives
// at the door they came in by.
static void ApplyPendingMapTransition(void)
{
    FieldUnload(&gField);

    gGameState.currentMapId    = gGameState.pendingMapId;
    gGameState.currentMapSeed  = gGameState.pendingMapSeed;
    gGameState.currentFloor    = gGameState.pendingFloor;
    int sx   = gGameState.pendingSpawnX;
    int sy   = gGameState.pendingSpawnY;
    int sdir = gGameState.pendingSpawnDir;
    gGameState.hasPendingMap = false;

    FieldInit(&gField, &gGameState);

    gField.player.tileX         = sx;
    gField.player.tileY         = sy;
    gField.player.targetTileX   = sx;
    gField.player.targetTileY   = sy;
    gField.player.dir           = sdir;
    gField.player.moving        = false;
    gField.player.moveFrames    = 0;
    gField.player.stepCompleted = false;

    int mapPixW = gField.map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = gField.map.height * TILE_SIZE * TILE_SCALE;
    gField.camera = CameraCreate(PlayerPixelPos(&gField.player), mapPixW, mapPixH);

    if (gShowRescueDialogue) {
        gShowRescueDialogue = false;
        DialogueBegin(&gField.dialogue, gRescueMsg, RESCUE_MSG_PAGES, 40.0f);
    }
}

void UpdateGameplayScreen(void)
{
    if (gGameState.hasPendingMap) {
        ApplyPendingMapTransition();
        return; // Skip field update this frame — next frame handles input.
    }

    FieldUpdate(&gField, GetFrameTime());

    if (gField.pendingBattle) {
        gField.pendingBattle = false;
        // Wire up every queued enemy so group aggro → one multi-enemy battle.
        int count = gField.pendingEnemyCount;
        if (count > 0) {
            int ids[FIELD_MAX_PENDING];
            int levels[FIELD_MAX_PENDING];
            int n = 0;
            for (int k = 0; k < count; k++) {
                int idx = gField.pendingEnemyIdxs[k];
                if (idx < 0 || idx >= gField.enemyCount) continue;
                FieldEnemy *e = &gField.enemies[idx];
                ids[n]    = e->creatureId;
                levels[n] = e->level;
                n++;
            }

            // Captive rescue: add the captive NPC to the party as a bound
            // temp ally for the duration of this battle. screen_gameplay
            // resolves whether they stay or go based on the battle outcome.
            gGameState.tempAllyPartyIdx = -1;
            gGameState.tempAllyNpcIdx   = -1;
            if (gField.pendingAllyNpcIdx >= 0 &&
                gField.pendingAllyNpcIdx < gField.npcCount &&
                gGameState.party.count < PARTY_MAX) {
                int npcIdx = gField.pendingAllyNpcIdx;
                int janLevel = gGameState.party.count > 0
                                 ? gGameState.party.members[0].level : 1;
                // Map NPC type → creature id. Today only SEAL is captive.
                int allyCreatureId = CREATURE_SEAL;
                int newIdx = gGameState.party.count;
                PartyAddMember(&gGameState.party, allyCreatureId, janLevel);
                if (newIdx < gGameState.party.count) {
                    CombatantAddStatus(&gGameState.party.members[newIdx], STATUS_BOUND);
                    gGameState.tempAllyPartyIdx = newIdx;
                    gGameState.tempAllyNpcIdx   = npcIdx;
                }
            }
            gField.pendingAllyNpcIdx = -1;

            BattlePrepareEncounter(&gGameState.party, ids, levels, n);
            BattleSetPreemptive(gField.preemptiveAttack);
            gField.preemptiveAttack = false;
        }
        finishScreen = 2; // → BATTLE
    }
}

void DrawGameplayScreen(void)
{
    FieldDraw(&gField);
}

void UnloadGameplayScreen(void)
{
    FieldUnload(&gField);
}

int FinishGameplayScreen(void)
{
    return finishScreen;
}
