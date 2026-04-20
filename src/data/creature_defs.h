#ifndef CREATURE_DEFS_H
#define CREATURE_DEFS_H

//----------------------------------------------------------------------------------
// Creature definitions - static data table for all combatants
//----------------------------------------------------------------------------------

#define CREATURE_NAME_LEN 32
#define CREATURE_MAX_MOVES 4
#define CREATURE_DEF_COUNT 5

typedef enum CreatureClass {
    CLASS_PENGUIN = 0,
    CLASS_HUMAN,
    CLASS_PINNIPED,
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
    int           moveIds[CREATURE_MAX_MOVES];
    int           moveCount;
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

#endif // CREATURE_DEFS_H
