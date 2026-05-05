#ifndef DEV_WARP_UI_H
#define DEV_WARP_UI_H

#include <stdbool.h>

//----------------------------------------------------------------------------------
// DevWarpUI — developer-only cheat / teleport modal, gated by DEV_BUILD. Two
// tabs:
//   WARP   - hub + harbor F1..F9 picker. Z warps to the destination.
//   CHEATS - inventory + state shortcuts (give items, scrap, rep, toggle god
//            mode). Tapping a row applies the cheat in place.
//
// The warp uses the same GameState pending-map machinery as normal warps, so
// state (party, inventory, flags like captainDefeated) is untouched — this is
// a pure teleport, not a reset. Cheats mutate GameState directly.
//----------------------------------------------------------------------------------

struct GameState;

typedef enum DevTab {
    DEV_TAB_WARP = 0,
    DEV_TAB_CHEATS,
    DEV_TAB_COUNT,
} DevTab;

typedef struct DevWarpUI {
    bool   active;
    int    cursor;
    DevTab tab;
    // Toast for the most recent cheat action — flashed under the list so the
    // tap has visible feedback (give-item etc. is otherwise silent).
    char   toast[80];
} DevWarpUI;

void DevWarpUIInit(DevWarpUI *d);
bool DevWarpUIIsOpen(const DevWarpUI *d);
void DevWarpUIOpen(DevWarpUI *d);
void DevWarpUIClose(DevWarpUI *d);

// Returns true if a warp was committed (caller should close peer modals /
// clear battle state before the pending map loads). Cheat taps return false.
bool DevWarpUIUpdate(DevWarpUI *d, struct GameState *gs);
void DevWarpUIDraw(const DevWarpUI *d, const struct GameState *gs);

#endif // DEV_WARP_UI_H
