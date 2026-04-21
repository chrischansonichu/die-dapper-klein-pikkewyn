#ifndef INVENTORY_UI_H
#define INVENTORY_UI_H

#include <stdbool.h>
#include "../battle/party.h"

//----------------------------------------------------------------------------------
// InventoryUI - overlay for viewing and managing the party inventory outside battle
//----------------------------------------------------------------------------------

typedef enum InventoryTab {
    INV_TAB_ITEMS = 0,
    INV_TAB_WEAPONS,
    INV_TAB_COUNT,
} InventoryTab;

typedef struct InventoryUI {
    bool         active;
    InventoryTab tab;
    int          cursor;        // row within the current tab
    // For weapons tab, the cursor also navigates between equipped slots of the active member.
    // equippedFocus == true means cursor is on an equipped slot (0..CREATURE_MAX_MOVES-1);
    // false means cursor is in the bag list.
    bool         equippedFocus;
    // Which party member is the target of use/equip actions. The inventory is
    // shared across the party, but consumables and weapons always land on one
    // specific combatant — cycled with [ / ] keys.
    int          memberCursor;
    char         status[128];   // last action message, e.g. "Ate Fresh Fish +30 HP"
} InventoryUI;

void InventoryUIInit(InventoryUI *ui);

// Returns true if UI just opened or was already open. Should be checked each frame
// before processing field input.
bool InventoryUIIsOpen(const InventoryUI *ui);

void InventoryUIOpen(InventoryUI *ui);
void InventoryUIClose(InventoryUI *ui);

// Update overlay. Consumes input when active. Returns true if still open after update.
bool InventoryUIUpdate(InventoryUI *ui, Party *party);

// Draw overlay (screen space, call after world draw).
void InventoryUIDraw(const InventoryUI *ui, const Party *party, int villageReputation);

#endif // INVENTORY_UI_H
