#include "creature_defs.h"

// Bases are level-1 values. Final stats = base + (level - 1) * classGrowth.
// Numbers are tuned so Jan at the current starter level (5) lands in the same
// ballpark as the old formula (HP 22, ATK 7, DEF 4, SPD 6); enemies are a
// little sturdier than before, which is fine — the game is meant to start
// rewarding combat rather than trivializing it.
const CreatureDef gCreatureDefs[CREATURE_DEF_COUNT] = {
    // Move slots: [Attack0, Item0, Item1, Item2, Special0, Special1].  -1 = empty.
    // id,                   name,              class,         HP  ATK DEF SPD DEX  { Atk0  Item0 Item1 Item2 Sp0   Sp1 }  scale enrage
    { CREATURE_JAN,          "Jan",             CLASS_PENGUIN, 10, 3,  2,  6,  2,   {   0,    1,   -1,   -1,    4,  -1}, 1.0f, false },
    { CREATURE_DECKHAND,     "Deckhand",        CLASS_HUMAN,    8, 2,  2,  3,  1,   {   0,    1,   -1,   -1,   -1,  -1}, 1.0f, false },
    { CREATURE_BOSUN,        "Bosun",           CLASS_HUMAN,   20, 4,  4,  2,  1,   {   0,    1,    2,   -1,   -1,  -1}, 1.0f, false },
    { CREATURE_CAPTAIN,      "Captain",         CLASS_HUMAN,   45, 8,  6,  2,  1,   {   0,    2,    3,   -1,    5,  -1}, 1.0f, false },
    { CREATURE_SEAL,         "Seal",            CLASS_PINNIPED,24, 7,  5,  3,  1,   {   0,    2,   -1,   -1,    4,  -1}, 1.0f, false },
    { CREATURE_POACHER,      "Abalone Poacher", CLASS_DIVER,   14, 3,  3,  4,  2,   {   0,    1,   -1,   -1,   -1,  -1}, 1.0f, false },
    // Boss variant. Bigger baseHp so L14 class growth lands around 124 HP. Holds
    // ShellThrow + the unique Harpoon (move id 6) in item-attack slots, plus
    // ColonyRoar + the boss-only CrashingTide (move id 7) in specials.
    // canEnrage=true unlocks the one-shot phase-2 buff when HP first crosses 50%.
    { CREATURE_CAPTAIN_BOSS, "Captain",         CLASS_HUMAN,   85, 10, 7,  2,  1,   {   0,    2,    6,   -1,    5,   7}, 1.5f, true  },
};

// Penguins grow nimbly (high DEX, modest power). Humans are bulky and hit hard
// but move slowly. Pinnipeds are tanky strikers with good speed in water.
static const ClassGrowth gClassGrowth[CLASS_COUNT] = {
    [CLASS_PENGUIN]  = { .hpPerLevel = 3, .atkPerLevel = 1, .defPerLevel = 1, .spdPerLevel = 2, .dexPerLevel = 2 },
    [CLASS_HUMAN]    = { .hpPerLevel = 3, .atkPerLevel = 2, .defPerLevel = 1, .spdPerLevel = 1, .dexPerLevel = 1 },
    [CLASS_PINNIPED] = { .hpPerLevel = 4, .atkPerLevel = 2, .defPerLevel = 1, .spdPerLevel = 1, .dexPerLevel = 1 },
    [CLASS_DIVER]    = { .hpPerLevel = 3, .atkPerLevel = 2, .defPerLevel = 1, .spdPerLevel = 1, .dexPerLevel = 2 },
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
