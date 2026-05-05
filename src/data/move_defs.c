#include "move_defs.h"

const MoveDef gMoveDefs[MOVE_COUNT] = {
    // id name           desc                             pwr range         dur  wpn lvl  group                    dmgType     aoeEn
    { 0, "Tackle",         "A basic tackle",               40, RANGE_MELEE,  -1, false, 1, MOVE_GROUP_ATTACK,      DMG_BLUNT,   false },
    { 1, "FishingHook",    "Slash with a discarded hook",  85, RANGE_MELEE,  15, true,  3, MOVE_GROUP_ITEM_ATTACK, DMG_SLASH,   false },
    { 2, "ShellThrow",     "Hurl a shell fragment",       105, RANGE_RANGED, 12, true,  5, MOVE_GROUP_ITEM_ATTACK, DMG_BLUNT,   false },
    { 3, "SeaUrchinSpike", "Stab with a sea urchin",      135, RANGE_MELEE,   8, true,  7, MOVE_GROUP_ITEM_ATTACK, DMG_PIERCE,  false },
    // Slot 4 was WaveCall (AOE DEF debuff). Removed 2026-05-04 — it never
    // pulled its weight relative to a basic Tackle. Subsequent IDs shifted
    // down by one; creature_defs and combatant.c follow the new numbering.
    { 4, "ColonyRoar",     "Boost own ATK by 25%",          0, RANGE_SELF,   -1, false, 1, MOVE_GROUP_SPECIAL,     DMG_SPECIAL, false },
    { 5, "Harpoon",        "A heavy iron harpoon",        150, RANGE_RANGED, 10, true,  1, MOVE_GROUP_ITEM_ATTACK, DMG_PIERCE,  false },
    { 6, "CrashingTide",   "A furious wave floods the deck", 90, RANGE_AOE, -1, false, 1, MOVE_GROUP_SPECIAL,     DMG_BLUNT,   true  },
};

const MoveDef *GetMoveDef(int id)
{
    if (id < 0 || id >= MOVE_COUNT) return &gMoveDefs[0];
    return &gMoveDefs[id];
}
