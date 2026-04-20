#include "creature_defs.h"

// Bases are level-1 values. Final stats = base + (level - 1) * classGrowth.
// Numbers are tuned so Jan at the current starter level (5) lands in the same
// ballpark as the old formula (HP 22, ATK 7, DEF 4, SPD 6); enemies are a
// little sturdier than before, which is fine — the game is meant to start
// rewarding combat rather than trivializing it.
const CreatureDef gCreatureDefs[CREATURE_DEF_COUNT] = {
    // Move slots: [Attack0, Attack1, Item0, Item1, Special0, Special1].  -1 = empty.
    // id,               name,        class,         HP  ATK DEF SPD DEX  { Atk0  Atk1  Item0 Item1 Sp0   Sp1 }
    { CREATURE_JAN,      "Jan",       CLASS_PENGUIN, 10, 3,  2,  4,  2,   {   0,   -1,    1,   -1,    4,  -1} },
    { CREATURE_DECKHAND, "Deckhand",  CLASS_HUMAN,    8, 2,  2,  3,  1,   {   0,   -1,    1,   -1,   -1,  -1} },
    { CREATURE_BOSUN,    "Bosun",     CLASS_HUMAN,   20, 4,  4,  2,  1,   {   0,   -1,    1,    2,   -1,  -1} },
    { CREATURE_CAPTAIN,  "Captain",   CLASS_HUMAN,   45, 8,  6,  2,  1,   {   0,   -1,    2,    3,    5,  -1} },
    { CREATURE_SEAL,     "Seal",      CLASS_PINNIPED,24, 7,  5,  5,  1,   {   0,   -1,    2,   -1,    4,  -1} },
};

// Penguins grow nimbly (high DEX, modest power). Humans are bulky and hit hard
// but move slowly. Pinnipeds are tanky strikers with good speed in water.
static const ClassGrowth gClassGrowth[CLASS_COUNT] = {
    [CLASS_PENGUIN]  = { .hpPerLevel = 3, .atkPerLevel = 1, .defPerLevel = 1, .spdPerLevel = 1, .dexPerLevel = 2 },
    [CLASS_HUMAN]    = { .hpPerLevel = 3, .atkPerLevel = 2, .defPerLevel = 1, .spdPerLevel = 1, .dexPerLevel = 1 },
    [CLASS_PINNIPED] = { .hpPerLevel = 4, .atkPerLevel = 2, .defPerLevel = 1, .spdPerLevel = 2, .dexPerLevel = 1 },
};

const CreatureDef *GetCreatureDef(int id)
{
    if (id < 0 || id >= CREATURE_DEF_COUNT) return &gCreatureDefs[0];
    return &gCreatureDefs[id];
}

const ClassGrowth *GetClassGrowth(CreatureClass class)
{
    if (class < 0 || class >= CLASS_COUNT) return &gClassGrowth[CLASS_PENGUIN];
    return &gClassGrowth[class];
}
