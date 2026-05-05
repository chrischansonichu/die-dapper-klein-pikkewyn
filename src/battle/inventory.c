#include "inventory.h"
#include <string.h>

void InventoryInit(Inventory *inv)
{
    memset(inv, 0, sizeof(Inventory));
}

bool InventoryAddItem(Inventory *inv, int itemId, int count)
{
    if (count <= 0) return true;
    // Stack with existing
    for (int i = 0; i < inv->itemCount; i++) {
        if (inv->items[i].itemId == itemId) {
            inv->items[i].count += count;
            return true;
        }
    }
    // New slot
    if (inv->itemCount >= INVENTORY_MAX_ITEMS) return false;
    inv->items[inv->itemCount].itemId = itemId;
    inv->items[inv->itemCount].count  = count;
    inv->itemCount++;
    return true;
}

void InventoryConsumeItem(Inventory *inv, int slotIdx)
{
    if (slotIdx < 0 || slotIdx >= inv->itemCount) return;
    inv->items[slotIdx].count--;
    if (inv->items[slotIdx].count <= 0) {
        // Compact: shift remaining slots down
        for (int i = slotIdx; i < inv->itemCount - 1; i++)
            inv->items[i] = inv->items[i + 1];
        inv->itemCount--;
    }
}

bool InventoryAddWeapon(Inventory *inv, int moveId, int durability)
{
    return InventoryAddWeaponEx(inv, moveId, durability, 0);
}

bool InventoryAddWeaponEx(Inventory *inv, int moveId, int durability,
                          int upgradeLevel)
{
    if (inv->weaponCount >= INVENTORY_MAX_WEAPONS) return false;
    inv->weapons[inv->weaponCount].moveId       = moveId;
    inv->weapons[inv->weaponCount].durability   = durability;
    inv->weapons[inv->weaponCount].upgradeLevel = upgradeLevel;
    inv->weaponCount++;
    return true;
}

bool InventoryTakeWeapon(Inventory *inv, int slotIdx, WeaponStack *out)
{
    if (slotIdx < 0 || slotIdx >= inv->weaponCount) return false;
    *out = inv->weapons[slotIdx];
    // Compact: shift remaining slots down
    for (int i = slotIdx; i < inv->weaponCount - 1; i++)
        inv->weapons[i] = inv->weapons[i + 1];
    inv->weaponCount--;
    return true;
}

bool InventoryAddArmor(Inventory *inv, int armorId)
{
    if (inv->armorCount >= INVENTORY_MAX_ARMORS) return false;
    inv->armors[inv->armorCount].armorId = armorId;
    inv->armorCount++;
    return true;
}

bool InventoryTakeArmor(Inventory *inv, int slotIdx, ArmorStack *out)
{
    if (slotIdx < 0 || slotIdx >= inv->armorCount) return false;
    *out = inv->armors[slotIdx];
    for (int i = slotIdx; i < inv->armorCount - 1; i++)
        inv->armors[i] = inv->armors[i + 1];
    inv->armorCount--;
    return true;
}
