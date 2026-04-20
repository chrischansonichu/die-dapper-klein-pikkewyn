#include "move_defs.h"

const MoveDef gMoveDefs[MOVE_COUNT] = {
    { 0, "Tackle",         "A basic tackle",               40, RANGE_MELEE,  -1, false, 1 },
    { 1, "FishingHook",    "Slash with a discarded hook",  85, RANGE_MELEE,  15, true,  3 },
    { 2, "ShellThrow",     "Hurl a shell fragment",       105, RANGE_RANGED, 12, true,  5 },
    { 3, "SeaUrchinSpike", "Stab with a sea urchin",      135, RANGE_MELEE,   8, true,  7 },
    { 4, "WaveCall",       "Lower all enemy DEF by 20%",    0, RANGE_AOE,    -1, false, 1 },
    { 5, "ColonyRoar",     "Boost own ATK by 25%",          0, RANGE_SELF,   -1, false, 1 },
};

const MoveDef *GetMoveDef(int id)
{
    if (id < 0 || id >= MOVE_COUNT) return &gMoveDefs[0];
    return &gMoveDefs[id];
}
