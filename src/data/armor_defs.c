#include "armor_defs.h"

const ArmorDef gArmorDefs[ARMOR_COUNT] = {
    { ARMOR_CAPTAINS_COAT, "Captain's Coat",
      "Heavy navy wool, gold-trimmed. Stops a jab cold.", 8, 1 },
};

const ArmorDef *GetArmorDef(int id)
{
    if (id < 0 || id >= ARMOR_COUNT) return &gArmorDefs[0];
    return &gArmorDefs[id];
}
