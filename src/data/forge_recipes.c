#include "forge_recipes.h"
#include "move_defs.h"

const ForgeRecipe gForgeRecipes[] = {
    { /*result*/ 3 /*SeaUrchinSpike*/,
      /*inputs*/ {{1 /*FishingHook*/, 3}, {-1, 0}, {-1, 0}},
      /*repCost*/ 5,
      /*label*/  "Sea Urchin Spike (3x Fishing Hook + 5 Rep)" },
};

const int gForgeRecipeCount = (int)(sizeof(gForgeRecipes) / sizeof(gForgeRecipes[0]));

static int CountWeaponMatches(const Inventory *inv, int moveId)
{
    int n = 0;
    for (int i = 0; i < inv->weaponCount; i++)
        if (inv->weapons[i].moveId == moveId) n++;
    return n;
}

bool ForgeCanAfford(const Inventory *inv, int villageReputation, const ForgeRecipe *r)
{
    if (!inv || !r) return false;
    if (villageReputation < r->repCost) return false;
    for (int i = 0; i < FORGE_RECIPE_INPUTS; i++) {
        int mid = r->inputs[i].moveId;
        if (mid < 0) continue;
        if (CountWeaponMatches(inv, mid) < r->inputs[i].count) return false;
    }
    return true;
}

// Remove `count` weapons of `moveId` from the bag. Walks descending so
// InventoryTakeWeapon's shift-down doesn't invalidate remaining indices.
static void ConsumeWeapons(Inventory *inv, int moveId, int count)
{
    int remaining = count;
    for (int i = inv->weaponCount - 1; i >= 0 && remaining > 0; i--) {
        if (inv->weapons[i].moveId == moveId) {
            WeaponStack out;
            InventoryTakeWeapon(inv, i, &out);
            remaining--;
        }
    }
}

bool ForgeApplyRecipe(Inventory *inv, int *villageReputation, const ForgeRecipe *r)
{
    if (!inv || !villageReputation || !r) return false;
    if (!ForgeCanAfford(inv, *villageReputation, r)) return false;
    for (int i = 0; i < FORGE_RECIPE_INPUTS; i++) {
        int mid = r->inputs[i].moveId;
        if (mid < 0) continue;
        ConsumeWeapons(inv, mid, r->inputs[i].count);
    }
    *villageReputation -= r->repCost;
    const MoveDef *mv = GetMoveDef(r->resultMoveId);
    int dur = (mv && mv->defaultDurability > 0) ? mv->defaultDurability : 1;
    return InventoryAddWeapon(inv, r->resultMoveId, dur);
}
