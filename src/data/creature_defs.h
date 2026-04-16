#ifndef CREATURE_DEFS_H
#define CREATURE_DEFS_H

//----------------------------------------------------------------------------------
// Creature definitions - static data table for all combatants
//----------------------------------------------------------------------------------

#define CREATURE_NAME_LEN 32
#define CREATURE_MAX_MOVES 4
#define CREATURE_DEF_COUNT 5

typedef struct CreatureDef {
    int  id;
    char name[CREATURE_NAME_LEN];
    int  baseHp;
    int  baseAtk;
    int  baseDef;
    int  baseSpd;
    int  moveIds[CREATURE_MAX_MOVES];
    int  moveCount;
} CreatureDef;

extern const CreatureDef gCreatureDefs[CREATURE_DEF_COUNT];

const CreatureDef *GetCreatureDef(int id);

// Creature IDs
#define CREATURE_JAN      0
#define CREATURE_DECKHAND 1
#define CREATURE_BOSUN    2
#define CREATURE_CAPTAIN  3
#define CREATURE_SEAL     4

#endif // CREATURE_DEFS_H
