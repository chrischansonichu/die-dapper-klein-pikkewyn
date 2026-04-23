#ifndef STATS_UI_H
#define STATS_UI_H

#include <stdbool.h>
#include "../battle/party.h"

//----------------------------------------------------------------------------------
// StatsUI - overlay for viewing party stats. Opened with C on the field (or
// the "Menu" button on the mobile virtual pad). The old LAYOUT tab was removed
// when the grid-positioning battle system was retired; Party.preferredCell is
// still persisted for save-format stability but is no longer user-editable.
//----------------------------------------------------------------------------------

typedef struct StatsUI {
    bool active;
    int  cursor;   // party index being inspected
} StatsUI;

void StatsUIInit(StatsUI *ui);

bool StatsUIIsOpen(const StatsUI *ui);
void StatsUIOpen(StatsUI *ui);
void StatsUIClose(StatsUI *ui);

// Returns true if still open after update.
bool StatsUIUpdate(StatsUI *ui, Party *party);
void StatsUIDraw(const StatsUI *ui, const Party *party);

#endif // STATS_UI_H
