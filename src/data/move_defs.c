#include "move_defs.h"

const MoveDef gMoveDefs[MOVE_COUNT] = {
    // id name           desc                             pwr range         dur  wpn lvl  group                    dmgType     aoeEn  baseWpnLvl
    { 0, "Tackle",         "A basic tackle",               40, RANGE_MELEE,  -1, false, 1, MOVE_GROUP_ATTACK,      DMG_BLUNT,   false, 0 },
    { 1, "FishingHook",    "Slash with a discarded hook",  85, RANGE_MELEE,  15, true,  3, MOVE_GROUP_ITEM_ATTACK, DMG_SLASH,   false, 1 },
    { 2, "ShellThrow",     "Hurl a shell fragment",       105, RANGE_RANGED, 12, true,  5, MOVE_GROUP_ITEM_ATTACK, DMG_BLUNT,   false, 1 },
    { 3, "SeaUrchinSpike", "Stab with a sea urchin",      135, RANGE_MELEE,   8, true,  7, MOVE_GROUP_ITEM_ATTACK, DMG_PIERCE,  false, 1 },
    // Slot 4 was WaveCall (AOE DEF debuff). Removed 2026-05-04 — it never
    // pulled its weight relative to a basic Tackle. Subsequent IDs shifted
    // down by one; creature_defs and combatant.c follow the new numbering.
    { 4, "ColonyRoar",     "Boost own ATK by 25%",          0, RANGE_SELF,   -1, false, 1, MOVE_GROUP_SPECIAL,     DMG_SPECIAL, false, 0 },
    { 5, "Harpoon",        "A heavy iron harpoon",        150, RANGE_RANGED, 10, true,  1, MOVE_GROUP_ITEM_ATTACK, DMG_PIERCE,  false, 2 },
    { 6, "CrashingTide",   "A furious wave floods the deck", 90, RANGE_AOE, -1, false, 1, MOVE_GROUP_SPECIAL,     DMG_BLUNT,   true,  0 },
};

const MoveDef *GetMoveDef(int id)
{
    if (id < 0 || id >= MOVE_COUNT) return &gMoveDefs[0];
    return &gMoveDefs[id];
}

int WeaponEffectiveLevel(int moveId, int upgradeLevel)
{
    const MoveDef *mv = GetMoveDef(moveId);
    if (!mv->isWeapon) return 0;
    if (upgradeLevel < 0) upgradeLevel = 0;
    return mv->baseWeaponLevel + upgradeLevel;
}

int WeaponMaxDurability(int moveId, int upgradeLevel)
{
    const MoveDef *mv = GetMoveDef(moveId);
    if (mv->defaultDurability < 0) return -1;
    if (upgradeLevel < 0) upgradeLevel = 0;
    // Additive +10% per upgrade — easier to reason about than compounding,
    // and at the +3 cap a Harpoon ends up at dur 13 instead of 13.31.
    return mv->defaultDurability + (mv->defaultDurability * upgradeLevel) / 10;
}

int WeaponPowerBonusPct(int upgradeLevel)
{
    if (upgradeLevel < 0) upgradeLevel = 0;
    return 100 + 10 * upgradeLevel;
}

int WeaponMeltScrap(int moveId, int upgradeLevel)
{
    return WeaponEffectiveLevel(moveId, upgradeLevel);
}

int WeaponUpgradeCost(int moveId, int upgradeLevel)
{
    int effLvl = WeaponEffectiveLevel(moveId, upgradeLevel);
    if (effLvl <= 0) return 0;
    return 2 * effLvl;
}
