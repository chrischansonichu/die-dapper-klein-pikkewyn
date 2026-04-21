#ifndef DONATION_UI_H
#define DONATION_UI_H

#include <stdbool.h>
#include "../battle/party.h"

//----------------------------------------------------------------------------------
// DonationUI — overlay for the Food Bank NPC. Filters the inventory down to
// food consumables and lets Jan pick a per-item donation count. On confirm,
// the selected items are consumed and villageReputation is bumped by the
// number of items given. Cancel aborts with no changes.
//
// Two phases: DON_PHASE_PICK is the quantity picker; after confirmation the
// panel flips to DON_PHASE_RESULT to show the thank-you message + new rep
// total. Any key closes the result page.
//----------------------------------------------------------------------------------

#define DONATION_MAX_ENTRIES 16

typedef enum DonationPhase {
    DON_PHASE_PICK = 0,
    DON_PHASE_RESULT,
} DonationPhase;

typedef struct DonationUI {
    bool          active;
    DonationPhase phase;
    int           cursor;
    int           entryCount;
    int           itemIdx[DONATION_MAX_ENTRIES];   // index into party->inventory.items
    int           maxCount[DONATION_MAX_ENTRIES];  // stack size at open (clamp ceiling)
    int           donate[DONATION_MAX_ENTRIES];    // chosen give-count per entry
    int           donatedTotal;                    // filled at commit
    int           repAfter;                        // filled at commit
} DonationUI;

void DonationUIInit(DonationUI *d);
bool DonationUIIsOpen(const DonationUI *d);

// Scans the party inventory for food items and seeds the picker.
void DonationUIOpen(DonationUI *d, const Party *party);
void DonationUIClose(DonationUI *d);

// Drives input and, on confirm, consumes the chosen items and bumps *rep.
void DonationUIUpdate(DonationUI *d, Party *party, int *villageReputation);

void DonationUIDraw(const DonationUI *d, const Party *party, int villageReputation);

#endif // DONATION_UI_H
