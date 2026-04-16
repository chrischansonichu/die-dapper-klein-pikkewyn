#include "encounter.h"
#include "raylib.h"
#include "../data/creature_defs.h"
#include <string.h>

// Zone tables: each zone maps to a list of enemy creature IDs
typedef struct ZoneEntry {
    int creatureId;
    int minLevel;
    int maxLevel;
} ZoneEntry;

// Harbor zone (shallow water)
static const ZoneEntry harborZone[] = {
    { CREATURE_DECKHAND, 1, 3 },
    { CREATURE_BOSUN,    2, 4 },
};
static const int harborZoneCount = 2;

// Default zone fallback
static const ZoneEntry defaultZone[] = {
    { CREATURE_DECKHAND, 1, 2 },
};
static const int defaultZoneCount = 1;

static const ZoneEntry *GetZone(const char *mapName, int *count)
{
    if (mapName && TextIsEqual(mapName, "harbor")) {
        *count = harborZoneCount;
        return harborZone;
    }
    *count = defaultZoneCount;
    return defaultZone;
}

EncounterResult EncounterRoll(const char *mapName)
{
    EncounterResult r = {0};

    if (GetRandomValue(0, 255) >= ENCOUNTER_RATE) return r;

    r.triggered = true;

    int zoneCount = 0;
    const ZoneEntry *zone = GetZone(mapName, &zoneCount);

    // Pick 1-2 enemies from the zone table
    int enemyCount = GetRandomValue(1, 2);
    for (int i = 0; i < enemyCount; i++) {
        int pick = GetRandomValue(0, zoneCount - 1);
        r.enemyIds[r.enemyCount]    = zone[pick].creatureId;
        r.enemyLevels[r.enemyCount] = GetRandomValue(zone[pick].minLevel, zone[pick].maxLevel);
        r.enemyCount++;
    }

    return r;
}
