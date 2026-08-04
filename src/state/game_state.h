#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "../battle/party.h"

// Persistent one-bit story flags. Used for one-shot world events (read a
// logbook, lit a lantern, opened an alcove chest). Bits are stable: never
// renumber — only add. Reserved upper range for save-format growth.
#define STORY_FLAG_LANTERN_DOCK_W   (1ull << 0)
#define STORY_FLAG_LANTERN_DOCK_M   (1ull << 1)
#define STORY_FLAG_LANTERN_DOCK_E   (1ull << 2)
#define STORY_FLAG_LANTERN_ALL      (STORY_FLAG_LANTERN_DOCK_W | \
                                     STORY_FLAG_LANTERN_DOCK_M | \
                                     STORY_FLAG_LANTERN_DOCK_E)

#define STORY_FLAG_LOGBOOK_F2_CAVE   (1ull << 8)
#define STORY_FLAG_LOGBOOK_F4_TRADER (1ull << 9)
#define STORY_FLAG_LOGBOOK_F6_LOG3   (1ull << 10)
#define STORY_FLAG_LOGBOOK_F7_LOG4   (1ull << 11)
#define STORY_FLAG_LOGBOOK_F5_HINT   (1ull << 12)

#define STORY_FLAG_ALCOVE_CHEST_OPENED (1ull << 16)

// Tutorial island progression (Jan's hatch-island, new-game starting map).
// Sequential beats, but stored as independent bits so a mid-tutorial save
// restores exactly. TUT_INTRO gates the auto-playing opening dialogue only.
#define STORY_FLAG_TUT_INTRO         (1ull << 24)  // Ma's opening scene shown
#define STORY_FLAG_TUT_MET_RYNO      (1ull << 25)  // first Ryno talk (the reveal)
#define STORY_FLAG_TUT_SWIM_TAUGHT   (1ull << 26)  // swim lesson — east cove open
#define STORY_FLAG_TUT_KELP_CUT      (1ull << 27)  // fish-trap gap slashed open
#define STORY_FLAG_TUT_CACHE_TAKEN   (1ull << 28)  // FishingHook cache looted
#define STORY_FLAG_TUT_GULL_BEATEN   (1ull << 29)  // practice fight won
#define STORY_FLAG_TUT_COMPLETE      (1ull << 30)  // goodbyes done — north channel open
#define STORY_FLAG_TUT_RANGED_TAUGHT (1ull << 31)  // ranged lesson — gull mob active
#define STORY_FLAG_TUT_RAID_BEATEN   (1ull << 32)  // drying-rack raid repelled
#define STORY_FLAG_TUT_LEAVE_OFFERED (1ull << 33)  // Ryno's offer — farewells unlocked
#define STORY_FLAG_TUT_FAREWELL_MA   (1ull << 34)  // said goodbye to Ma
#define STORY_FLAG_TUT_FAREWELL_PA   (1ull << 35)  // said goodbye to Pa
#define STORY_FLAG_TUT_FAREWELL_SIB  (1ull << 36)  // said goodbye to Vlerkie
#define STORY_FLAG_TUT_FAREWELL_ALL  (STORY_FLAG_TUT_FAREWELL_MA | \
                                      STORY_FLAG_TUT_FAREWELL_PA | \
                                      STORY_FLAG_TUT_FAREWELL_SIB)
#define STORY_FLAG_TUT_NEST_SEEN     (1ull << 37)  // Annika's storm-nest visited
#define STORY_FLAG_TUT_DARE_ACCEPTED (1ull << 38)  // Vlerkie's buoy dare taken
#define STORY_FLAG_TUT_DARE_DONE     (1ull << 39)  // buoy touched
#define STORY_FLAG_TUT_DARE_PAID     (1ull << 40)  // Vlerkie paid out his stash
#define STORY_FLAG_TUT_POOL_HINT     (1ull << 41)  // first tide-pool heal tip shown
#define STORY_FLAG_TUT_GULL_BRIEFED  (1ull << 42)  // cache find shown to Ryno — he heads south
#define STORY_FLAG_TUT_ARRIVED       (1ull << 43)  // hub journey montage shown
#define STORY_FLAG_HUB_RYNO_GREETED  (1ull << 44)  // village Ryno's first-arrival talk

//----------------------------------------------------------------------------------
// GameState - persistent state that survives map transitions and battles.
// The FieldState is transient (rebuilt each time the player enters a map), but
// GameState carries the party, inventory, and (later) story flags / gold /
// dismissed-member roster through the whole session.
//
// MapId values are `int` here to keep state/ from depending on field/ headers;
// GameStateInit and screen_gameplay include map_source.h to translate.
//----------------------------------------------------------------------------------

typedef struct GameState {
    Party    party;

    // The map the FieldState currently represents, and the seed used to build
    // it (unused for authored maps). FieldInit reads these. `currentFloor` is
    // the dungeon depth (1..9) or 0 when on a non-dungeon map.
    int      currentMapId;
    unsigned currentMapSeed;
    int      currentFloor;

    // Set by the field when the player steps on a warp tile (or later, uses
    // an escape item, or is rescued after defeat). screen_gameplay consumes
    // this in UpdateGameplayScreen and rebuilds the FieldState.
    bool     hasPendingMap;
    int      pendingMapId;
    unsigned pendingMapSeed;
    int      pendingFloor;
    int      pendingSpawnX, pendingSpawnY, pendingSpawnDir;

    // Tracking a temporary ally added for the duration of a captive-rescue
    // battle (e.g., the bound seal). After the battle, field.c either keeps
    // them (if their captors all died) or removes them. -1 when none.
    int      tempAllyPartyIdx;
    int      tempAllyNpcIdx;

    // Set by the battle-defeat path so the next map transition shows the
    // "village patched you up" dialogue. Consumed by screen_gameplay after
    // the hub is rebuilt.
    bool     rescueDialoguePending;

    // Optional trailing page appended after the rescue dialogue — details
    // what inventory was lost / damaged on the swim back. Staged through
    // GameState (not a local in the defeat handler) so it survives the
    // FieldUnload that tears down the battle map before the hub loads.
    bool     rescueLossPending;
    char     rescueLossMsg[160];

    // Village economy — the game uses no abstract currency. The keeper runs a
    // barter loop (bring specific loot, get leveled rewards) and the food
    // bank accepts consumables for reputation. Rep is a one-way milestone
    // meter today; gates on specific thresholds land when content exists.
    int      villageReputation;
    int      keeperQuestIdx;   // which entry in the keeper's rotating ask list is active

    // Harbor boss progression. `captainDefeated` latches true on victory at
    // Harbor F9 and unlocks the return warp; `captainTauntShown` prevents the
    // pre-fight narration page from repeating on every approach.
    bool     captainDefeated;
    bool     captainTauntShown;

    // Difficulty setting. 0 = easy (default — enemies hit at half power), 1 =
    // hard (full enemy damage). Persisted in the save; toggled from screen_options.
    int      difficulty;

    // Easy-mode dungeon resume. When the player is defeated on a dungeon floor
    // in easy mode, this stores the floor they died on. The next hub→harbor
    // warp redirects to that floor instead of F1, then clears this back to 0.
    // Hard mode never sets this; F1 deaths set it to 1 (which is a no-op
    // redirect — they'd land back on F1 anyway).
    int      rescueResumeFloor;

    // Scrap stash held by the blacksmith — currency for weapon upgrades.
    // Scrap is produced by melting weapons and never lives in the player's
    // inventory; the blacksmith holds it on the player's behalf.
    int      blacksmithScrap;

    // Dev-only god-mode toggle. Transient (not saved). When true, party
    // damage taken is zeroed at the battle's hp-decrement step. Gated behind
    // DEV_BUILD at the UI layer; the runtime check is unconditional but the
    // flag has no UI to flip in shipping builds.
    bool     devGodMode;

    // Persistent one-bit world events — see STORY_FLAG_* macros above. Tracks
    // read logbooks, lit lanterns, opened alcove chests. Survives save/load.
    uint64_t storyFlags;

    // Additional persistent state lands here in later phases:
    //   Roster    dismissedMembers;
} GameState;

// Fresh game: seeds the party with Jan + starter items, lands them in the hub.
void GameStateInit(GameState *gs);

#endif // GAME_STATE_H
