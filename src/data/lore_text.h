#ifndef LORE_TEXT_H
#define LORE_TEXT_H

//----------------------------------------------------------------------------------
// Logbook & chest pickup text. Static, indexed by `dataId` on the
// FieldObject. New entries land at the bottom; ids are stable so saved
// `storyFlags` keep meaning across builds.
//----------------------------------------------------------------------------------

#define LORE_F2_CAVE       0
#define LORE_F4_TRADER     1
#define LORE_F5_LANTERN_HINT 2
#define LORE_F6_LOG3       3
#define LORE_F7_LOG4       4
#define LORE_COUNT         5

// Returns NULL for unknown ids. `outPageCount` receives the page count for
// the entry; pages are NUL-terminated strings owned by the static table.
const char *const *GetLoreText(int loreId, int *outPageCount);

// Per-logbook title shown above the page text. NULL if unknown id.
const char *GetLoreTitle(int loreId);

//----------------------------------------------------------------------------------
// Chest contents — fixed per dataId. dataId is independent of LORE_*; chests
// don't use the lore table.
//----------------------------------------------------------------------------------

#define CHEST_ALCOVE_F3  0
#define CHEST_ALCOVE_F4  1
#define CHEST_COUNT      2

typedef struct ChestContents {
    // Weapon dropped (move id), or -1 for none.
    int weaponMoveId;
    int weaponDurabilityFraction; // 100 = full, 50 = half, etc.
    // Item dropped (item id), or -1 for none.
    int itemId;
    int itemCount;
    // Optional one-shot text shown after pickup. NULL for silent.
    const char *flavor;
} ChestContents;

const ChestContents *GetChestContents(int chestId);

#endif // LORE_TEXT_H
