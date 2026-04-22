#ifndef DEV_WARP_UI_H
#define DEV_WARP_UI_H

#include <stdbool.h>

//----------------------------------------------------------------------------------
// DevWarpUI — developer-only teleport modal, gated by DEV_BUILD. Lists every
// registered destination (hub + harbor F1..F9) as a scrollable picker. Z
// warps the party to the selected destination's spawn tile; X closes.
//
// The warp uses the same GameState pending-map machinery as normal warps, so
// state (party, inventory, flags like captainDefeated) is untouched — this is
// a pure teleport, not a reset.
//----------------------------------------------------------------------------------

struct GameState;

typedef struct DevWarpUI {
    bool active;
    int  cursor;
} DevWarpUI;

void DevWarpUIInit(DevWarpUI *d);
bool DevWarpUIIsOpen(const DevWarpUI *d);
void DevWarpUIOpen(DevWarpUI *d);
void DevWarpUIClose(DevWarpUI *d);

// Returns true if a warp was committed (caller should close peer modals /
// clear battle state before the pending map loads).
bool DevWarpUIUpdate(DevWarpUI *d, struct GameState *gs);
void DevWarpUIDraw(const DevWarpUI *d);

#endif // DEV_WARP_UI_H
