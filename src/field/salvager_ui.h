#ifndef SALVAGER_UI_H
#define SALVAGER_UI_H

#include <stdbool.h>
#include "../battle/party.h"

//----------------------------------------------------------------------------------
// SalvagerUI — overlay for the Village Salvager NPC. Lists the player's bag
// weapons, marks the broken ones (durability == 0), and hands out fish for
// each broken weapon turned in. Non-broken weapons are shown but can't be
// selected — the salvager won't take serviceable gear, and the UI calls that
// out rather than silently hiding those entries (so the player knows why).
//
// Two phases: SAL_PHASE_PICK is the selection screen; after confirmation the
// panel flips to SAL_PHASE_RESULT to show the fish gained. Any key closes
// the result page.
//----------------------------------------------------------------------------------

#define SALVAGER_MAX_ENTRIES 16

typedef enum SalvagerPhase {
    SAL_PHASE_PICK = 0,
    SAL_PHASE_RESULT,
} SalvagerPhase;

typedef struct SalvagerUI {
    bool          active;
    SalvagerPhase phase;
    int           cursor;
    int           entryCount;
    int           weaponIdx[SALVAGER_MAX_ENTRIES]; // index into party->inventory.weapons
    bool          broken[SALVAGER_MAX_ENTRIES];    // true when durability == 0
    bool          give[SALVAGER_MAX_ENTRIES];      // player's selection
    int           handedTotal;                     // filled at commit
    int           fishGained;                      // filled at commit
    float         scrollX;                         // horizontal strip scroll
} SalvagerUI;

void SalvagerUIInit(SalvagerUI *s);
bool SalvagerUIIsOpen(const SalvagerUI *s);

// Scans the party inventory for weapons and seeds the picker.
void SalvagerUIOpen(SalvagerUI *s, const Party *party);
void SalvagerUIClose(SalvagerUI *s);

// Drives input and, on confirm, consumes the selected weapons and adds fish.
void SalvagerUIUpdate(SalvagerUI *s, Party *party);

void SalvagerUIDraw(const SalvagerUI *s, const Party *party);

#endif // SALVAGER_UI_H
