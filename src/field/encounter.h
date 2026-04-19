#ifndef ENCOUNTER_H
#define ENCOUNTER_H

#include <stdbool.h>

//----------------------------------------------------------------------------------
// Encounter system - random encounter rolls on encounter tiles
//----------------------------------------------------------------------------------

#define ENCOUNTER_RATE      30    // out of 255 per step on encounter tile
#define ENCOUNTER_MAX_LEVEL  5    // enemy level for first area

// Result of a trigger check
typedef struct EncounterResult {
    bool triggered;
    int  enemyIds[4];
    int  enemyLevels[4];
    int  enemyCount;
} EncounterResult;

// Roll for an encounter after stepping on an encounter tile.
// mapName is used to select the zone's encounter table.
EncounterResult EncounterRoll(const char *mapName);

#endif // ENCOUNTER_H
