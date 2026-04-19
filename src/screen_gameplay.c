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
#include "battle/inventory.h"
#include "data/item_defs.h"
#include "data/move_defs.h"
#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int  finishScreen  = 0;
static bool gInitialized  = false;  // false until first FieldInit
static FieldState gField = {0};

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
    // A defeat sends us through ENDING back to TITLE; starting a new game
    // from there must wipe the prior run, not reload its field state.
    if (GetLastBattleResult() == BATTLE_DEFEAT) {
        gInitialized = false;
    }
    if (!gInitialized) {
        FieldInit(&gField);
        gInitialized = true;
    } else {
        // Returning from battle: reload textures, preserve all game state
        FieldReloadResources(&gField);
        // Deactivate every enemy that was in the encounter + roll drops on each.
        // Drops from the first enemy that produced any get narrated; the rest
        // still land in the inventory silently so the player isn't buried in
        // dialogue pages after a big group fight.
        if (GetLastBattleResult() == BATTLE_VICTORY) {
            int narratedPages = 0;
            const char *ptrs[DROP_MSG_PAGES];
            for (int k = 0; k < gField.pendingEnemyCount; k++) {
                int idx = gField.pendingEnemyIdxs[k];
                if (idx < 0 || idx >= gField.enemyCount) continue;
                FieldEnemy *e = &gField.enemies[idx];
                int pages = RollEnemyDrops(e, &gField.party.inventory);
                e->active = false;
                if (narratedPages == 0 && pages > 0) {
                    narratedPages = pages;
                    for (int i = 0; i < pages; i++) ptrs[i] = gDropMsg[i];
                }
            }
            if (narratedPages > 0)
                DialogueBegin(&gField.dialogue, ptrs, narratedPages, 40.0f);
        }
        gField.pendingEnemyCount = 0;
    }
}

void UpdateGameplayScreen(void)
{
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
            BattlePrepareEncounter(&gField.party, ids, levels, n);
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
