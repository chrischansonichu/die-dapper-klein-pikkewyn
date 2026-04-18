/**********************************************************************************************
*
*   Die Dapper Klein Pikkewyn - Gameplay Screen (Overworld)
*   Hosts the tile-based overworld with player movement, NPCs, and encounter triggers.
*
**********************************************************************************************/

#include "raylib.h"
#include "screens.h"
#include "overworld/overworld.h"
#include "overworld/enemy.h"
#include "battle/inventory.h"
#include "data/item_defs.h"
#include "data/move_defs.h"
#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int  finishScreen  = 0;
static bool gInitialized  = false;  // false until first OverworldInit
static OverworldState gOverworld = {0};

// Persistent storage for the drop dialogue pages — DialogueBegin keeps pointers
// into these buffers, so they must outlive the call.
#define DROP_MSG_PAGES 2
#define DROP_MSG_LEN   160
static char gDropMsg[DROP_MSG_PAGES][DROP_MSG_LEN];

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------

// Roll drops on the defeated enemy, append to inventory, and prepare a dialogue
// describing what was picked up. Returns number of dialogue pages populated.
static int RollEnemyDrops(OverworldEnemy *e, Inventory *inv)
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
    // A defeat sends us through ENDING back to TITLE; starting a new game
    // from there must wipe the prior run, not reload its overworld state.
    if (GetLastBattleResult() == BATTLE_DEFEAT) {
        gInitialized = false;
    }
    if (!gInitialized) {
        OverworldInit(&gOverworld);
        gInitialized = true;
    } else {
        // Returning from battle: reload textures, preserve all game state
        OverworldReloadResources(&gOverworld);
        // Deactivate every enemy that was in the encounter + roll drops on each.
        // Drops from the first enemy that produced any get narrated; the rest
        // still land in the inventory silently so the player isn't buried in
        // dialogue pages after a big group fight.
        if (GetLastBattleResult() == BATTLE_VICTORY) {
            int narratedPages = 0;
            const char *ptrs[DROP_MSG_PAGES];
            for (int k = 0; k < gOverworld.pendingEnemyCount; k++) {
                int idx = gOverworld.pendingEnemyIdxs[k];
                if (idx < 0 || idx >= gOverworld.enemyCount) continue;
                OverworldEnemy *e = &gOverworld.enemies[idx];
                int pages = RollEnemyDrops(e, &gOverworld.party.inventory);
                e->active = false;
                if (narratedPages == 0 && pages > 0) {
                    narratedPages = pages;
                    for (int i = 0; i < pages; i++) ptrs[i] = gDropMsg[i];
                }
            }
            if (narratedPages > 0)
                DialogueBegin(&gOverworld.dialogue, ptrs, narratedPages, 40.0f);
        }
        gOverworld.pendingEnemyCount = 0;
    }
}

void UpdateGameplayScreen(void)
{
    OverworldUpdate(&gOverworld, GetFrameTime());

    if (gOverworld.pendingBattle) {
        gOverworld.pendingBattle = false;
        // Wire up every queued enemy so group aggro → one multi-enemy battle.
        int count = gOverworld.pendingEnemyCount;
        if (count > 0) {
            int ids[OVERWORLD_MAX_PENDING];
            int levels[OVERWORLD_MAX_PENDING];
            int n = 0;
            for (int k = 0; k < count; k++) {
                int idx = gOverworld.pendingEnemyIdxs[k];
                if (idx < 0 || idx >= gOverworld.enemyCount) continue;
                OverworldEnemy *e = &gOverworld.enemies[idx];
                ids[n]    = e->creatureId;
                levels[n] = e->level;
                n++;
            }
            BattlePrepareEncounter(&gOverworld.party, ids, levels, n);
            BattleSetPreemptive(gOverworld.preemptiveAttack);
            gOverworld.preemptiveAttack = false;
        }
        finishScreen = 2; // → BATTLE
    }
}

void DrawGameplayScreen(void)
{
    OverworldDraw(&gOverworld);
}

void UnloadGameplayScreen(void)
{
    OverworldUnload(&gOverworld);
}

int FinishGameplayScreen(void)
{
    return finishScreen;
}
