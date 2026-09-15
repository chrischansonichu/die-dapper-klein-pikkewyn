#ifndef DISCARD_UI_H
#define DISCARD_UI_H

#include <stdbool.h>
#include "../battle/party.h"
#include "../battle/inventory.h"

//----------------------------------------------------------------------------------
// DiscardUI — reusable bag-full swap modal. Any time a weapon is about to
// enter an already-full weapon bag (boss drop, post-battle loot, keeper
// reward, inventory unequip), the caller opens this modal with the pending
// weapon. The player picks an existing bag weapon to toss into the surf and
// takes the new one, or refuses and loses the incoming weapon.
//
// Two phases: PICK is the selection screen; after commit/refuse the panel
// flips to RESULT to narrate what happened. A tap on the CTA closes RESULT —
// or, if more weapons arrived while the modal was up (two drops in one
// battle), pops the next one straight into a fresh PICK so nothing is lost
// silently and the player is told how many are still waiting.
//
// The pick list is sorted weakest-first (durability ascending, broken at the
// top) so the obvious toss is always the first row.
//----------------------------------------------------------------------------------

typedef enum DiscardPhase {
    DISC_PHASE_PICK = 0,
    DISC_PHASE_RESULT,
} DiscardPhase;

#define DISCARD_QUEUE_MAX 6

typedef struct DiscardUI {
    bool         active;
    DiscardPhase phase;
    int          cursor;            // index into order[], not the bag
    int          entryCount;        // = party->inventory.weaponCount at open
    int          order[INVENTORY_MAX_WEAPONS]; // bag indices, durability ascending
    float        scrollPx;          // list scroll offset (pixels)
    int          pendingMoveId;
    int          pendingDurability;
    int          pendingUpgradeLevel;
    bool         cancelled;         // RESULT narration: true if the player refused the incoming
    int          swappedOutMoveId;  // RESULT narration: what they chose to discard

    // Weapons that arrived while a pick was already up.
    int          queueMoveId[DISCARD_QUEUE_MAX];
    int          queueDurability[DISCARD_QUEUE_MAX];
    int          queueUpgrade[DISCARD_QUEUE_MAX];
    int          queueCount;
} DiscardUI;

void DiscardUIInit(DiscardUI *d);
bool DiscardUIIsOpen(const DiscardUI *d);

// Open the picker with a pending weapon. Caller must have already checked
// that the bag is full. If the modal is already open, the weapon is queued
// and presented after the current one resolves.
void DiscardUIOpen(DiscardUI *d, const Party *party,
                   int incomingMoveId, int incomingDurability,
                   int incomingUpgradeLevel);
void DiscardUIClose(DiscardUI *d);

// Drives input and, on confirm, swaps the chosen weapon out and the pending in.
void DiscardUIUpdate(DiscardUI *d, Party *party);
void DiscardUIDraw(const DiscardUI *d, const Party *party);

#endif // DISCARD_UI_H
