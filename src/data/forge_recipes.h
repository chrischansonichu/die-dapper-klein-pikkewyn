#ifndef FORGE_RECIPES_H
#define FORGE_RECIPES_H

#include <stdbool.h>
#include "../battle/inventory.h"

//----------------------------------------------------------------------------------
// ForgeRecipe — static upgrade paths consumed by the blacksmith. Each recipe
// turns N weapons + rep into one result weapon at full durability. Extend the
// table in forge_recipes.c; no code changes needed for new paths.
//----------------------------------------------------------------------------------

#define FORGE_RECIPE_INPUTS 3

typedef struct ForgeRecipeInput {
    int moveId;   // -1 = unused slot
    int count;
} ForgeRecipeInput;

typedef struct ForgeRecipe {
    int              resultMoveId;
    ForgeRecipeInput inputs[FORGE_RECIPE_INPUTS];
    int              repCost;
    const char      *label;
} ForgeRecipe;

extern const ForgeRecipe gForgeRecipes[];
extern const int         gForgeRecipeCount;

bool ForgeCanAfford(const Inventory *inv, int villageReputation, const ForgeRecipe *r);
bool ForgeApplyRecipe(Inventory *inv, int *villageReputation, const ForgeRecipe *r);

#endif // FORGE_RECIPES_H
