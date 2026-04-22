#include "item_defs.h"

const ItemDef gItemDefs[ITEM_COUNT] = {
    { ITEM_KRILL_SNACK,   "Krill Snack",   "A handful of krill.",           ITEM_EFFECT_HEAL,      15, 1, DMG_NONE },
    { ITEM_FRESH_FISH,    "Fresh Fish",    "A plump silverfish.",           ITEM_EFFECT_HEAL,      30, 1, DMG_NONE },
    { ITEM_SARDINE,       "Sardine",       "An oily, filling sardine.",     ITEM_EFFECT_HEAL,      50, 1, DMG_NONE },
    { ITEM_PERLEMOEN,     "Perlemoen",     "A rare shore-feast from home.", ITEM_EFFECT_HEAL_FULL,  0, 1, DMG_NONE },
};

const ItemDef *GetItemDef(int id)
{
    if (id < 0 || id >= ITEM_COUNT) return &gItemDefs[0];
    return &gItemDefs[id];
}
