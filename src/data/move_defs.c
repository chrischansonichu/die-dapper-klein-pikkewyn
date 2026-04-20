#include "move_defs.h"

const MoveDef gMoveDefs[MOVE_COUNT] = {
    // id name           desc                             pwr range         dur  wpn lvl  group                    dmgType     aoeEn
    { 0, "Tackle",         "A basic tackle",               40, RANGE_MELEE,  -1, false, 1, MOVE_GROUP_ATTACK,      DMG_BLUNT,   false },
    { 1, "FishingHook",    "Slash with a discarded hook",  85, RANGE_MELEE,  15, true,  3, MOVE_GROUP_ITEM_ATTACK, DMG_SLASH,   false },
    { 2, "ShellThrow",     "Hurl a shell fragment",       105, RANGE_RANGED, 12, true,  5, MOVE_GROUP_ITEM_ATTACK, DMG_BLUNT,   false },
    { 3, "SeaUrchinSpike", "Stab with a sea urchin",      135, RANGE_MELEE,   8, true,  7, MOVE_GROUP_ITEM_ATTACK, DMG_PIERCE,  false },
    { 4, "WaveCall",       "Lower all enemy DEF by 20%",    0, RANGE_AOE,    -1, false, 1, MOVE_GROUP_SPECIAL,     DMG_SPECIAL, true  },
    { 5, "ColonyRoar",     "Boost own ATK by 25%",          0, RANGE_SELF,   -1, false, 1, MOVE_GROUP_SPECIAL,     DMG_SPECIAL, false },
};

const MoveDef *GetMoveDef(int id)
{
    if (id < 0 || id >= MOVE_COUNT) return &gMoveDefs[0];
    return &gMoveDefs[id];
}
