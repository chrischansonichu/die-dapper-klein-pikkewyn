#include "creature_defs.h"

const CreatureDef gCreatureDefs[CREATURE_DEF_COUNT] = {
    {
        CREATURE_JAN, "Jan", 30, 10, 6, 8,
        {0, 1, 4, -1}, 3
    },
    {
        CREATURE_DECKHAND, "Deckhand", 20, 8, 5, 7,
        {0, 1, -1, -1}, 2
    },
    {
        CREATURE_BOSUN, "Bosun", 35, 12, 8, 5,
        {0, 1, 2, -1}, 3
    },
    {
        CREATURE_CAPTAIN, "Captain", 60, 18, 12, 4,
        {0, 2, 3, 5}, 4
    },
    {
        CREATURE_SEAL, "Seal", 40, 15, 10, 9,
        {0, 2, 4, -1}, 3
    },
};

const CreatureDef *GetCreatureDef(int id)
{
    if (id < 0 || id >= CREATURE_DEF_COUNT) return &gCreatureDefs[0];
    return &gCreatureDefs[id];
}
