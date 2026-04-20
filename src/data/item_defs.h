#ifndef ITEM_DEFS_H
#define ITEM_DEFS_H

#include "move_defs.h"

//----------------------------------------------------------------------------------
// Item definitions - consumables Jan can eat to restore HP
//----------------------------------------------------------------------------------

#define ITEM_NAME_LEN 32
#define ITEM_DESC_LEN 64

typedef enum ItemEffect {
    ITEM_EFFECT_HEAL,       // restores HP by `amount`
    ITEM_EFFECT_HEAL_FULL,  // restores HP to maxHp
} ItemEffect;

typedef struct ItemDef {
    int            id;
    char           name[ITEM_NAME_LEN];
    char           desc[ITEM_DESC_LEN];
    ItemEffect     effect;
    int            amount;
    int            minLevel;    // minimum level required to use; 1 = no gate
    MoveDamageType damageType;  // for items used offensively; DMG_NONE for pure heals
} ItemDef;

// Item IDs
#define ITEM_KRILL_SNACK     0
#define ITEM_FRESH_FISH      1
#define ITEM_SARDINE         2
#define ITEM_ANTARCTIC_ICE   3
#define ITEM_COUNT           4

extern const ItemDef gItemDefs[ITEM_COUNT];

const ItemDef *GetItemDef(int id);

#endif // ITEM_DEFS_H
