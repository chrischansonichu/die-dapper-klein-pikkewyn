#ifndef MOVE_DEFS_H
#define MOVE_DEFS_H

#include <stdbool.h>

//----------------------------------------------------------------------------------
// Move definitions - static data table for all attacks/abilities
//----------------------------------------------------------------------------------

#define MOVE_NAME_LEN  32
#define MOVE_DESC_LEN  64
#define MOVE_COUNT     6

typedef enum MoveRange {
    RANGE_MELEE = 0,    // must be in front column, hits adjacent enemy
    RANGE_RANGED,       // can attack from anywhere, pick any enemy
    RANGE_AOE,          // hits all enemies
    RANGE_SELF,         // targets self / entire party
} MoveRange;

// Move group drives both UI column placement and equip-slot rules.
// Item Attacks are the only group that accepts weapon drops.
typedef enum MoveGroup {
    MOVE_GROUP_ATTACK = 0,       // innate damaging moves (Tackle, etc.)
    MOVE_GROUP_ITEM_ATTACK,      // thrown/wielded items (FishingHook, ShellThrow)
    MOVE_GROUP_SPECIAL,          // buffs/debuffs (WaveCall, ColonyRoar)
    MOVE_GROUP_COUNT,
} MoveGroup;

typedef struct MoveDef {
    int       id;
    char      name[MOVE_NAME_LEN];
    char      desc[MOVE_DESC_LEN];
    int       power;             // damage base; 0 = status move
    MoveRange range;
    int       defaultDurability; // -1 = unlimited; >0 = uses before broken
    bool      isWeapon;          // true = equippable weapon (swappable); false = innate move
    int       minLevel;          // minimum level required to equip/use; 1 = no gate
    MoveGroup group;             // which of the 3 move-slot columns this belongs to
} MoveDef;

// Forward declaration - defined in move_defs.c
extern const MoveDef gMoveDefs[MOVE_COUNT];

const MoveDef *GetMoveDef(int id);

#endif // MOVE_DEFS_H
