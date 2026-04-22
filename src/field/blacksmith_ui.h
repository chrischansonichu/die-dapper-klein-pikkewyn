#ifndef BLACKSMITH_UI_H
#define BLACKSMITH_UI_H

#include <stdbool.h>
#include "../battle/party.h"
#include "discard_ui.h"

//----------------------------------------------------------------------------------
// BlacksmithUI — post-harbor forge modal. Two modes toggled via TAB:
//   REPAIR:  pick a target weapon, sacrifice other weapons as fuel, spend
//            2 Reputation flat to restore durability (capped at max).
//   UPGRADE: pick a recipe from forge_recipes.h; consume its inputs + rep,
//            add the result weapon at full durability. Bag-full spills into
//            DiscardUI, same as the boss Harpoon drop.
//----------------------------------------------------------------------------------

#define BLACKSMITH_MAX_ENTRIES 16
#define BLACKSMITH_REPAIR_REP  2
// Minimum durability contributed per sacrificed weapon. A broken weapon still
// gives this much so the player always has something to throw in the forge.
#define BLACKSMITH_MIN_FUEL    3

typedef enum BlacksmithMode {
    SMITH_MODE_REPAIR = 0,
    SMITH_MODE_UPGRADE,
} BlacksmithMode;

typedef enum BlacksmithPhase {
    SMITH_PHASE_PICK_TARGET = 0,
    SMITH_PHASE_PICK_FUEL,
    SMITH_PHASE_PICK_RECIPE,
    SMITH_PHASE_CONFIRM,
    SMITH_PHASE_RESULT,
} BlacksmithPhase;

typedef struct BlacksmithUI {
    bool            active;
    BlacksmithMode  mode;
    BlacksmithPhase phase;

    // Cursor over whichever list the current phase is showing.
    int             cursor;

    // Snapshot of inv->weapons at open / when re-entering PICK.
    int             entryCount;
    int             weaponIdx[BLACKSMITH_MAX_ENTRIES];
    int             startDur [BLACKSMITH_MAX_ENTRIES];
    int             maxDur   [BLACKSMITH_MAX_ENTRIES];

    // REPAIR state
    int             targetEntry;                       // index into weaponIdx[]
    bool            fuel[BLACKSMITH_MAX_ENTRIES];      // per-entry sacrifice flag
    int             pendingDurGain;                    // previewed durability restored

    // UPGRADE state
    int             recipeIdx;
    bool            recipeAffordable;

    // RESULT narration (two lines)
    char            resultLine1[96];
    char            resultLine2[96];
} BlacksmithUI;

void BlacksmithUIInit(BlacksmithUI *b);
bool BlacksmithUIIsOpen(const BlacksmithUI *b);

void BlacksmithUIOpen(BlacksmithUI *b, const Party *party);
void BlacksmithUIClose(BlacksmithUI *b);

// `discard` may be NULL — if a recipe result would overflow the bag and
// DiscardUI isn't wired, the operation narrates a "nowhere to stow it" page
// and reverts nothing (inputs are already consumed). Caller should pass it.
void BlacksmithUIUpdate(BlacksmithUI *b, Party *party, int *villageReputation,
                        DiscardUI *discard);

void BlacksmithUIDraw(const BlacksmithUI *b, const Party *party,
                      int villageReputation);

#endif // BLACKSMITH_UI_H
