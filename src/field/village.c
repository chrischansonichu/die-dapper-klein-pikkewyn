#include "village.h"
#include "../battle/combatant.h"
#include "../battle/inventory.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include <stdio.h>
#include <string.h>

// Keeper quest rotation — asks cycle Q0→Q1→Q2→Q0. Each quest is a fixed
// item-or-weapon demand. Reward *types* are fixed by quest too (always a
// weapon + a food item), but the specific reward tier scales with Jan's
// level so the Keeper stays useful from level 1 through the end-game.
typedef struct KeeperQuest {
    // What the player has to hand over. `wantMoveId` is a weapon move id
    // (FishingHook etc.); the player's inventory weapon stash is searched.
    int  wantMoveId;
    int  wantCount;
    // Short phrase for narration ("fishing hooks", "shell throws").
    const char *plural;
} KeeperQuest;

// Move IDs match gMoveDefs indices. Hook=1, ShellThrow=2, UrchinSpike=3.
static const KeeperQuest gKeeperQuests[KEEPER_QUEST_COUNT] = {
    { 1, 3, "FishingHooks"    },
    { 2, 2, "ShellThrows"     },
    { 3, 1, "SeaUrchinSpikes" },
};

// How many weapons of `moveId` the inventory holds (sum of weaponCount
// across all stacks of that id).
static int CountWeapon(const Inventory *inv, int moveId)
{
    int n = 0;
    for (int i = 0; i < inv->weaponCount; i++)
        if (inv->weapons[i].moveId == moveId) n++;
    return n;
}

// Remove `count` weapons of `moveId` from the inventory (lowest durability
// first — player keeps the fresher ones). Returns the number actually
// removed, which is <= count if the inventory was short.
static int TakeWeapons(Inventory *inv, int moveId, int count)
{
    int taken = 0;
    while (taken < count) {
        int pickSlot = -1;
        int pickDur  = 99999;
        for (int i = 0; i < inv->weaponCount; i++) {
            if (inv->weapons[i].moveId != moveId) continue;
            if (inv->weapons[i].durability < pickDur) {
                pickDur  = inv->weapons[i].durability;
                pickSlot = i;
            }
        }
        if (pickSlot < 0) break;
        WeaponStack out;
        if (!InventoryTakeWeapon(inv, pickSlot, &out)) break;
        taken++;
    }
    return taken;
}

// Reward scales by Jan's level:
//   level <= 3: FishingHook   + 1 Krill Snack
//   level <= 6: ShellThrow    + 2 Fresh Fish
//   level <= 9: SeaUrchinSpike + 1 Sardine
//   level >=10: SeaUrchinSpike + 1 Perlemoen
// Weapon durability comes from its MoveDef default.
typedef struct KeeperReward {
    int weaponMoveId;
    int fishItemId;
    int fishCount;
} KeeperReward;

static KeeperReward ComputeReward(int janLevel)
{
    KeeperReward r = {0};
    if (janLevel <= 3)      { r.weaponMoveId = 1; r.fishItemId = ITEM_KRILL_SNACK;   r.fishCount = 1; }
    else if (janLevel <= 6) { r.weaponMoveId = 2; r.fishItemId = ITEM_FRESH_FISH;    r.fishCount = 2; }
    else if (janLevel <= 9) { r.weaponMoveId = 3; r.fishItemId = ITEM_SARDINE;       r.fishCount = 1; }
    else                    { r.weaponMoveId = 3; r.fishItemId = ITEM_PERLEMOEN;     r.fishCount = 1; }
    return r;
}

int KeeperInteract(GameState *gs, DiscardUI *discard,
                   const char **pages, char scratch[4][NPC_DIALOGUE_LEN])
{
    int q = gs->keeperQuestIdx;
    if (q < 0 || q >= KEEPER_QUEST_COUNT) q = 0;
    const KeeperQuest *Q = &gKeeperQuests[q];

    Inventory *inv     = &gs->party.inventory;
    int        have    = CountWeapon(inv, Q->wantMoveId);
    int        janLvl  = (gs->party.count > 0) ? gs->party.members[0].level : 1;

    if (have < Q->wantCount) {
        snprintf(scratch[0], NPC_DIALOGUE_LEN,
                 "I'm collecting %s - bring me %d and I'll trade you something nice.",
                 Q->plural, Q->wantCount);
        snprintf(scratch[1], NPC_DIALOGUE_LEN,
                 "(You have %d of %d.)", have, Q->wantCount);
        pages[0] = scratch[0];
        pages[1] = scratch[1];
        return 2;
    }

    // Consume and pay out.
    TakeWeapons(inv, Q->wantMoveId, Q->wantCount);
    KeeperReward r = ComputeReward(janLvl);
    const MoveDef *rwMv = GetMoveDef(r.weaponMoveId);
    bool gotWeapon = InventoryAddWeapon(inv, r.weaponMoveId, rwMv->defaultDurability);
    bool gotFish   = InventoryAddItem(inv, r.fishItemId, r.fishCount);

    gs->keeperQuestIdx = (q + 1) % KEEPER_QUEST_COUNT;

    snprintf(scratch[0], NPC_DIALOGUE_LEN,
             "Excellent! That's just what I needed.");
    const ItemDef *rfIt = GetItemDef(r.fishItemId);
    snprintf(scratch[1], NPC_DIALOGUE_LEN,
             "Take this %s and %d %s for your trouble.",
             rwMv->name, r.fishCount, rfIt->name);

    // Weapon bag full: hand the swap decision to the player via DiscardUI
    // instead of silently dropping the reward.
    if (!gotWeapon && discard) {
        DiscardUIOpen(discard, &gs->party, r.weaponMoveId,
                      rwMv->defaultDurability, 0);
        if (!gotFish) {
            snprintf(scratch[2], NPC_DIALOGUE_LEN,
                     "(Your food bag is full too - some of the reward couldn't fit.)");
            pages[0] = scratch[0]; pages[1] = scratch[1]; pages[2] = scratch[2];
            return 3;
        }
        pages[0] = scratch[0]; pages[1] = scratch[1];
        return 2;
    }

    if (!gotWeapon || !gotFish) {
        snprintf(scratch[2], NPC_DIALOGUE_LEN,
                 "(Your bag was full - some of the reward couldn't fit.)");
        pages[0] = scratch[0];
        pages[1] = scratch[1];
        pages[2] = scratch[2];
        return 3;
    }
    pages[0] = scratch[0];
    pages[1] = scratch[1];
    return 2;
}

bool VillageIsFoodItem(int itemId)
{
    return itemId == ITEM_KRILL_SNACK
        || itemId == ITEM_FRESH_FISH
        || itemId == ITEM_SARDINE
        || itemId == ITEM_PERLEMOEN;
}
