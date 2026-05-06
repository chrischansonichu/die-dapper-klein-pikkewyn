#ifndef MOVE_DEFS_H
#define MOVE_DEFS_H

#include <stdbool.h>

//----------------------------------------------------------------------------------
// Move definitions - static data table for all attacks/abilities
//----------------------------------------------------------------------------------

#define MOVE_NAME_LEN  32
#define MOVE_DESC_LEN  64
#define MOVE_COUNT     9

typedef enum MoveRange {
    RANGE_MELEE = 0,    // must be in front column, hits adjacent enemy
    RANGE_RANGED,       // can attack from anywhere, pick any enemy
    RANGE_AOE,          // hits all enemies
    RANGE_SELF,         // targets self / entire party
} MoveRange;

// Damage type: drives rope-cutting (SLASH, PIERCE) and future elemental logic.
// Shared with ItemDef so a "sharpened krill" could also free a captive.
typedef enum MoveDamageType {
    DMG_NONE = 0,   // heal items, status-only moves
    DMG_BLUNT,      // Tackle, ShellThrow
    DMG_SLASH,      // FishingHook — cuts ropes
    DMG_PIERCE,     // SeaUrchinSpike — cuts ropes
    DMG_SPECIAL,    // WaveCall, ColonyRoar (magical/sonic)
} MoveDamageType;

// Sharp attacks cut ropes (STATUS_BOUND).
static inline bool DamageCutsRopes(MoveDamageType t) {
    return t == DMG_SLASH || t == DMG_PIERCE;
}

// Move group drives both UI column placement and equip-slot rules.
// Item Attacks are the only group that accepts weapon drops.
typedef enum MoveGroup {
    MOVE_GROUP_ATTACK = 0,       // innate damaging moves (Tackle, etc.)
    MOVE_GROUP_ITEM_ATTACK,      // thrown/wielded items (FishingHook, ShellThrow)
    MOVE_GROUP_SPECIAL,          // buffs/debuffs (WaveCall, ColonyRoar)
    MOVE_GROUP_COUNT,
} MoveGroup;

typedef struct MoveDef {
    int            id;
    char           name[MOVE_NAME_LEN];
    char           desc[MOVE_DESC_LEN];
    int            power;             // damage base; 0 = status move
    MoveRange      range;
    int            defaultDurability; // -1 = unlimited; >0 = uses before broken
    bool           isWeapon;          // true = equippable weapon (swappable); false = innate move
    int            minLevel;          // minimum level required to equip/use; 1 = no gate
    MoveGroup      group;             // which of the 3 move-slot columns this belongs to
    MoveDamageType damageType;        // physical type — drives rope-cut checks
    // AOE moves hit either all enemies or all friendlies, never a mix. Meaningless
    // outside RANGE_AOE. WaveCall = enemies, a hypothetical party-heal = friendlies.
    bool           aoeTargetsEnemies;
    // Base weapon tier (1, 2, ...) used by the blacksmith's scrap economy.
    // Melting a weapon at runtime yields (baseWeaponLevel + upgradeLevel) scrap;
    // upgrading costs 2 × current effective level. Non-weapon moves use 0.
    int            baseWeaponLevel;
    // Pre-strike dash distance for melee moves. 0 = no dash (default for every
    // existing move). >0 = the actor steps up to N tiles toward the target
    // before the damage step, stopping early at any solid tile or another
    // combatant. Used for the Captain's Boarding Charge so the boss can chase
    // the player into the railing on the F7 ship arena.
    int            dashTiles;
} MoveDef;

// Forward declaration - defined in move_defs.c
extern const MoveDef gMoveDefs[MOVE_COUNT];

const MoveDef *GetMoveDef(int id);

//----------------------------------------------------------------------------------
// Weapon upgrade math — pure functions on (moveId, upgradeLevel). Centralized
// so damage code, the blacksmith UI, and the inventory-UI tooltip all agree
// on what a "+2 FishingHook" costs / lasts / hits for.
//----------------------------------------------------------------------------------

// Effective weapon level: baseWeaponLevel + upgradeLevel. Drives both the
// scrap-yield-on-melt and the next-upgrade cost. Non-weapon moves return 0.
int WeaponEffectiveLevel(int moveId, int upgradeLevel);

// Max durability for a weapon at the given upgrade level. Adds +10% of the
// base durability per upgrade level (additive, not compounded). For unlimited
// moves (defaultDurability < 0) returns -1.
int WeaponMaxDurability(int moveId, int upgradeLevel);

// Power multiplier in percent: 100 at +0, 110 at +1, 120 at +2, 130 at +3.
// Apply with `(damage * pct) / 100`. For non-weapons or upgradeLevel<=0
// returns 100.
int WeaponPowerBonusPct(int upgradeLevel);

// Scrap yielded by melting this weapon: effective level (1 for L1 base, 2
// for Harpoon or any +1 L1 weapon, etc.). Non-weapon moves return 0.
int WeaponMeltScrap(int moveId, int upgradeLevel);

// Scrap cost to bump from the current upgrade level to the next: 2× the
// current effective level. Returns 0 for non-weapons or for weapons already
// at WEAPON_UPGRADE_MAX (caller should gate on the latter).
int WeaponUpgradeCost(int moveId, int upgradeLevel);

#endif // MOVE_DEFS_H
