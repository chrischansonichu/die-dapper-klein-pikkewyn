#ifndef DISCARD_UI_H
#define DISCARD_UI_H

#include <stdbool.h>
#include "../battle/party.h"

//----------------------------------------------------------------------------------
// DiscardUI — reusable bag-full swap modal. Any time a weapon is about to
// enter an already-full weapon bag (boss drop, post-battle loot, keeper
// reward, inventory unequip), the caller opens this modal with the pending
// weapon. The player picks an existing bag weapon to toss into the surf and
// takes the new one, or cancels and loses the incoming weapon.
//
// Two phases: PICK is the selection screen; after commit/cancel the panel
// flips to RESULT to narrate what happened. Any key closes RESULT.
//----------------------------------------------------------------------------------

typedef enum DiscardPhase {
    DISC_PHASE_PICK = 0,
    DISC_PHASE_RESULT,
} DiscardPhase;

typedef struct DiscardUI {
    bool         active;
    DiscardPhase phase;
    int          cursor;
    int          entryCount;        // = party->inventory.weaponCount at open
    int          pendingMoveId;
    int          pendingDurability;
    bool         cancelled;         // RESULT narration: true if the player tossed the incoming
    int          swappedOutMoveId;  // RESULT narration: what they chose to discard
} DiscardUI;

void DiscardUIInit(DiscardUI *d);
bool DiscardUIIsOpen(const DiscardUI *d);

// Open the picker with a pending weapon. Caller must have already checked
// that the bag is full; calling when there's room is harmless (the modal
// opens with a full picker anyway — but the caller should just add it).
void DiscardUIOpen(DiscardUI *d, const Party *party,
                   int incomingMoveId, int incomingDurability);
void DiscardUIClose(DiscardUI *d);

// Drives input and, on confirm, swaps the chosen weapon out and the pending in.
void DiscardUIUpdate(DiscardUI *d, Party *party);
void DiscardUIDraw(const DiscardUI *d, const Party *party);

#endif // DISCARD_UI_H
