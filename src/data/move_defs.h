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

typedef struct MoveDef {
    int       id;
    char      name[MOVE_NAME_LEN];
    char      desc[MOVE_DESC_LEN];
    int       power;             // damage base; 0 = status move
    MoveRange range;
    int       defaultDurability; // -1 = unlimited; >0 = uses before broken
    bool      isWeapon;          // true = equippable weapon (swappable); false = innate move
    int       minLevel;          // minimum level required to equip/use; 1 = no gate
} MoveDef;

// Forward declaration - defined in move_defs.c
extern const MoveDef gMoveDefs[MOVE_COUNT];

const MoveDef *GetMoveDef(int id);

#endif // MOVE_DEFS_H
