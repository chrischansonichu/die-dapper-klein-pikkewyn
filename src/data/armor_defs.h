#ifndef ARMOR_DEFS_H
#define ARMOR_DEFS_H

//----------------------------------------------------------------------------------
// Armor definitions - equippable defense gear. First entry is the Captain's
// Coat dropped by the F9 boss. Armor is its own category (parallel to items
// and weapons): one armor slot per Combatant, defBonus adds to base defense
// before the defMod percentage is applied.
//----------------------------------------------------------------------------------

#define ARMOR_NAME_LEN 32
#define ARMOR_DESC_LEN 64

typedef struct ArmorDef {
    int  id;
    char name[ARMOR_NAME_LEN];
    char desc[ARMOR_DESC_LEN];
    int  defBonus;
    int  minLevel;
} ArmorDef;

// Armor IDs
#define ARMOR_CAPTAINS_COAT 0
#define ARMOR_COUNT         1

extern const ArmorDef gArmorDefs[ARMOR_COUNT];

const ArmorDef *GetArmorDef(int id);

#endif // ARMOR_DEFS_H
