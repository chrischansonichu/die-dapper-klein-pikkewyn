#include "lore_text.h"
#include "../systems/strings.h"
#include <stddef.h>

//----------------------------------------------------------------------------------
// Logbook entries. The text itself lives in resources/lang/<code>.lang —
// this table only maps lore ids to string-table key prefixes. Pages are the
// numbered keys "<prefix>.1", ".2", ... so a translation may change the
// page count without touching code.
//----------------------------------------------------------------------------------

typedef struct LoreEntry {
    const char *titleKey;
    const char *pagesPrefix;
} LoreEntry;

static const LoreEntry kLore[LORE_COUNT] = {
    [LORE_F2_CAVE]         = { "lore.f2cave.title",   "lore.f2cave"   },
    [LORE_F4_TRADER]       = { "lore.f4trader.title", "lore.f4trader" },
    [LORE_F5_LANTERN_HINT] = { "lore.f5tally.title",  "lore.f5tally"  },
    [LORE_F6_LOG3]         = { "lore.f6log3.title",   "lore.f6log3"   },
    [LORE_F7_LOG4]         = { "lore.f7log4.title",   "lore.f7log4"   },
    [LORE_LOK_SIGN]        = { "lore.loksign.title",  "lore.loksign"  },
    [LORE_LOK_LEDGER]      = { "lore.lokledger.title","lore.lokledger"},
};

const char *const *GetLoreText(int loreId, int *outPageCount)
{
    // Static page buffer — pointers land straight into the string table, so
    // they stay valid; callers copy into the dialogue box immediately anyway.
    static const char *pages[STR_MAX_PAGES];

    if (loreId < 0 || loreId >= LORE_COUNT) {
        if (outPageCount) *outPageCount = 0;
        return NULL;
    }
    int n = StrPages(kLore[loreId].pagesPrefix, pages, STR_MAX_PAGES);
    if (outPageCount) *outPageCount = n;
    return pages;
}

const char *GetLoreTitle(int loreId)
{
    if (loreId < 0 || loreId >= LORE_COUNT) return NULL;
    return Str(kLore[loreId].titleKey);
}

//----------------------------------------------------------------------------------
// Chest contents
//----------------------------------------------------------------------------------

// Move ids are stable; see data/move_defs.c. 1 = FishingHook, 2 = ShellThrow,
// 3 = SeaUrchinSpike, 5 = Harpoon. Item ids: 0=Krill, 1=FreshFish, 2=Sardine,
// 3=Perlemoen. Flavor text is a string-table key.
static const ChestContents kChests[CHEST_COUNT] = {
    [CHEST_ALCOVE_F3] = {
        .weaponMoveId = 3,             // SeaUrchinSpike
        .weaponDurabilityFraction = 100,
        .itemId       = -1,
        .itemCount    = 0,
        .flavorKey    = "chest.alcove_f3.flavor",
    },
    [CHEST_ALCOVE_F4] = {
        .weaponMoveId = -1,
        .weaponDurabilityFraction = 0,
        .itemId       = 3,             // Perlemoen
        .itemCount    = 2,
        .flavorKey    = "chest.alcove_f4.flavor",
    },
    [CHEST_TUTORIAL_CACHE] = {
        .weaponMoveId = 1,             // FishingHook
        .weaponDurabilityFraction = 100,
        .itemId       = 2,             // Sardine
        .itemCount    = 2,
        .flavorKey    = "chest.cache.flavor",
    },
    // Lokasie hidden items — each sits behind an environmental gate that
    // needs a particular kind of weapon (see field.c BeginObjectInteraction).
    [CHEST_LOK_S2_DRUM] = {
        .weaponMoveId = 10,            // Kettie
        .weaponDurabilityFraction = 100,
        .itemId       = 2,             // Sardine
        .itemCount    = 2,
        .flavorKey    = "chest.lok_s2.flavor",
    },
    [CHEST_LOK_S3_WIRE] = {
        .weaponMoveId = -1,
        .weaponDurabilityFraction = 0,
        .itemId       = 4,             // Smoked Snoek
        .itemCount    = 3,
        .flavorKey    = "chest.lok_s3.flavor",
    },
    [CHEST_LOK_S4_LOCK] = {
        .weaponMoveId = -1,
        .weaponDurabilityFraction = 0,
        .itemId       = 3,             // Perlemoen
        .itemCount    = 2,
        .flavorKey    = "chest.lok_s4.flavor",
    },
    [CHEST_LOK_S5_DRUM] = {
        .weaponMoveId = 10,            // Kettie (a spare — the first one wears out)
        .weaponDurabilityFraction = 100,
        .itemId       = 3,             // Perlemoen
        .itemCount    = 1,
        .flavorKey    = "chest.lok_s5.flavor",
    },
};

const ChestContents *GetChestContents(int chestId)
{
    if (chestId < 0 || chestId >= CHEST_COUNT) return NULL;
    return &kChests[chestId];
}
