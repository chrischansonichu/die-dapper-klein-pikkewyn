#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdbool.h>

//----------------------------------------------------------------------------------
// Inventory - party-wide bag of consumables and unequipped weapons
//----------------------------------------------------------------------------------

#define INVENTORY_MAX_ITEMS   16
#define INVENTORY_MAX_WEAPONS 16
#define INVENTORY_MAX_ARMORS  8

typedef struct ItemStack {
    int itemId;
    int count;
} ItemStack;

typedef struct WeaponStack {
    int moveId;     // weapon's MoveDef id (must have isWeapon == true)
    int durability; // remaining uses
} WeaponStack;

typedef struct ArmorStack {
    int armorId;    // ArmorDef id
} ArmorStack;

typedef struct Inventory {
    ItemStack   items[INVENTORY_MAX_ITEMS];
    int         itemCount;          // number of occupied item slots
    WeaponStack weapons[INVENTORY_MAX_WEAPONS];
    int         weaponCount;        // number of occupied weapon slots
    ArmorStack  armors[INVENTORY_MAX_ARMORS];
    int         armorCount;         // number of occupied armor slots
} Inventory;

void InventoryInit(Inventory *inv);

// Add `count` of itemId to inventory. Stacks with existing entry if present,
// otherwise creates a new slot. Returns true on success, false if full.
bool InventoryAddItem(Inventory *inv, int itemId, int count);

// Consume one of the item in slot `slotIdx`. Removes the slot if it empties.
void InventoryConsumeItem(Inventory *inv, int slotIdx);

// Add a weapon (moveId, durability) to the bag. Returns false if full.
bool InventoryAddWeapon(Inventory *inv, int moveId, int durability);

// Remove weapon at slotIdx, writing its data to *out. Returns false on bad index.
bool InventoryTakeWeapon(Inventory *inv, int slotIdx, WeaponStack *out);

// Add an armor piece to the bag. Returns false if full.
bool InventoryAddArmor(Inventory *inv, int armorId);

// Remove armor at slotIdx, writing its data to *out. Returns false on bad index.
bool InventoryTakeArmor(Inventory *inv, int slotIdx, ArmorStack *out);

#endif // INVENTORY_H
