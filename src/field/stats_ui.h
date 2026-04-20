#ifndef STATS_UI_H
#define STATS_UI_H

#include <stdbool.h>
#include "../battle/party.h"
#include "../battle/battle_grid.h"

//----------------------------------------------------------------------------------
// StatsUI - overlay for viewing party stats and setting the default battle layout.
// Opened with C on the field. TAB switches between tabs.
//----------------------------------------------------------------------------------

typedef enum StatsTab {
    STATS_TAB_STATS = 0,
    STATS_TAB_LAYOUT,
    STATS_TAB_COUNT,
} StatsTab;

typedef struct StatsUI {
    bool     active;
    StatsTab tab;
    int      cursor;         // STATS tab: party index being inspected
    GridPos  layoutCursor;   // LAYOUT tab: currently highlighted cell
    int      layoutHeld;     // LAYOUT tab: party idx picked up, or -1
} StatsUI;

void StatsUIInit(StatsUI *ui);

bool StatsUIIsOpen(const StatsUI *ui);
void StatsUIOpen(StatsUI *ui);
void StatsUIClose(StatsUI *ui);

// Returns true if still open after update.
bool StatsUIUpdate(StatsUI *ui, Party *party);
void StatsUIDraw(const StatsUI *ui, const Party *party);

#endif // STATS_UI_H
