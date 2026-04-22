#ifndef CREATURE_DEFS_H
#define CREATURE_DEFS_H

//----------------------------------------------------------------------------------
// Creature definitions - static data table for all combatants
//----------------------------------------------------------------------------------

#define CREATURE_NAME_LEN 32
// Move slots: 3 groups x 2 slots = 6 total.
// Slot order is fixed: [Atk0, Atk1, Item0, Item1, Spec0, Spec1].
// Empty slots hold moveId = -1.
#define MOVE_SLOTS_PER_GROUP 2
#define CREATURE_MAX_MOVES (MOVE_SLOTS_PER_GROUP * 3)
#define MOVE_GROUP_SLOT(group, n) ((group) * MOVE_SLOTS_PER_GROUP + (n))
#define CREATURE_DEF_COUNT 6

typedef enum CreatureClass {
    CLASS_PENGUIN = 0,
    CLASS_HUMAN,
    CLASS_PINNIPED,
    CLASS_DIVER,       // human anatomy trained for water — no water speed penalty
    CLASS_COUNT,
} CreatureClass;

typedef struct CreatureDef {
    int           id;
    char          name[CREATURE_NAME_LEN];
    CreatureClass creatureClass;
    int           baseHp;
    int           baseAtk;
    int           baseDef;
    int           baseSpd;
    int           baseDex;
    // Fixed-layout move slots. -1 = empty. Each group's two slots are
    // at indices [group*2, group*2+1].
    int           moveIds[CREATURE_MAX_MOVES];
} CreatureDef;

// Per-level stat gain. Added to base stats once per level above 1.
typedef struct ClassGrowth {
    int hpPerLevel;
    int atkPerLevel;
    int defPerLevel;
    int spdPerLevel;
    int dexPerLevel;
} ClassGrowth;

extern const CreatureDef gCreatureDefs[CREATURE_DEF_COUNT];

const CreatureDef *GetCreatureDef(int id);
const ClassGrowth *GetClassGrowth(CreatureClass cclass);

// Creature IDs
#define CREATURE_JAN      0
#define CREATURE_DECKHAND 1
#define CREATURE_BOSUN    2
#define CREATURE_CAPTAIN  3
#define CREATURE_SEAL     4
#define CREATURE_POACHER  5

#endif // CREATURE_DEFS_H
