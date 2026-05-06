#include "lore_text.h"
#include <stddef.h>

//----------------------------------------------------------------------------------
// Logbook entries — short multi-page snippets. Pages are tuned to fit the
// dialogue panel width without manual line-wrapping (the dialogue system
// word-wraps at runtime).
//----------------------------------------------------------------------------------

static const char *kLore_F2_Cave[] = {
    "An etching, scratched into the cave wall.",
    "\"In the time before sails, the colony was the harbour. We dove with the seals, and the seals dove with us.\"",
    "Below the words, a faint outline of a flipper and a fin, side by side.",
};

static const char *kLore_F4_Trader[] = {
    "A clipped page from a scrap-trader's ledger, tacked to a beam.",
    "\"Bring me what the sea coughs up. Hooks, shells, urchin spikes - I will take them off your flippers and call it square.\"",
    "\"Do not bring rust. The forge will not eat rust.\"",
};

static const char *kLore_F5_LanternHint[] = {
    "A tally mark in damp ink, crossed out and re-drawn three times.",
    "\"Three lanterns guide the captain's ship to dock. No light, no plank.\"",
};

static const char *kLore_F6_Log3[] = {
    "A salt-stained page tucked under a crate.",
    "Captain's Log, page 3:",
    "\"The penguins watch us from the rocks. They think we cannot see them counting.\"",
    "\"Let them count. The hold is full and the tide is ours.\"",
};

static const char *kLore_F7_Log4[] = {
    "Captain's Log, final page.",
    "\"All three dock lanterns are burning. I did not light them.\"",
    "\"Something is climbing the gangplank.\"",
    "\"If you are reading this and you are not me - leave my ship.\"",
};

typedef struct LoreEntry {
    const char *title;
    const char *const *pages;
    int                pageCount;
} LoreEntry;

#define LORE_PAGES(arr) (arr), (int)(sizeof(arr) / sizeof((arr)[0]))

static const LoreEntry kLore[LORE_COUNT] = {
    [LORE_F2_CAVE]         = { "Cave Etching",         LORE_PAGES(kLore_F2_Cave) },
    [LORE_F4_TRADER]       = { "Trader's Ledger",      LORE_PAGES(kLore_F4_Trader) },
    [LORE_F5_LANTERN_HINT] = { "Tally Mark",           LORE_PAGES(kLore_F5_LanternHint) },
    [LORE_F6_LOG3]         = { "Captain's Log p.3",    LORE_PAGES(kLore_F6_Log3) },
    [LORE_F7_LOG4]         = { "Captain's Log (Last)", LORE_PAGES(kLore_F7_Log4) },
};

const char *const *GetLoreText(int loreId, int *outPageCount)
{
    if (loreId < 0 || loreId >= LORE_COUNT) {
        if (outPageCount) *outPageCount = 0;
        return NULL;
    }
    if (outPageCount) *outPageCount = kLore[loreId].pageCount;
    return kLore[loreId].pages;
}

const char *GetLoreTitle(int loreId)
{
    if (loreId < 0 || loreId >= LORE_COUNT) return NULL;
    return kLore[loreId].title;
}

//----------------------------------------------------------------------------------
// Chest contents
//----------------------------------------------------------------------------------

// Move ids are stable; see data/move_defs.c. 1 = FishingHook, 2 = ShellThrow,
// 3 = SeaUrchinSpike, 5 = Harpoon. Item ids: 0=Krill, 1=FreshFish, 2=Sardine,
// 3=Perlemoen.
static const ChestContents kChests[CHEST_COUNT] = {
    [CHEST_ALCOVE_F3] = {
        .weaponMoveId = 3,             // SeaUrchinSpike
        .weaponDurabilityFraction = 100,
        .itemId       = -1,
        .itemCount    = 0,
        .flavor       = "Inside the chest, a fresh urchin spike, still salt-bright.",
    },
    [CHEST_ALCOVE_F4] = {
        .weaponMoveId = -1,
        .weaponDurabilityFraction = 0,
        .itemId       = 3,             // Perlemoen
        .itemCount    = 2,
        .flavor       = "Two perlemoen, packed in seaweed. The trader will pay for these.",
    },
};

const ChestContents *GetChestContents(int chestId)
{
    if (chestId < 0 || chestId >= CHEST_COUNT) return NULL;
    return &kChests[chestId];
}
