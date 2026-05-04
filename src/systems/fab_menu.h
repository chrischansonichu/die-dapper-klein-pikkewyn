#ifndef FAB_MENU_H
#define FAB_MENU_H

#include "raylib.h"
#include <stdbool.h>

// Floating-action-button menu for the mobile/wasm build. A small ⋮-button
// sits in the top-right of the canvas; tapping it opens a short list that
// reaches Character (stats), Inventory, and Save — the only global actions
// the field used to hide behind hardware keys (C / I / scribe NPC).
//
// Desktop keyboard shortcuts still work; this is additive, not a replacement.

typedef enum {
    FAB_ACTION_NONE = 0,
    FAB_ACTION_INVENTORY,
    FAB_ACTION_STATS,
    FAB_ACTION_SAVE,
#ifdef DEV_BUILD
    FAB_ACTION_DEV_WARP,
#endif
} FabAction;

typedef struct {
    bool open;
    int  toastFrames;  // countdown for the post-Save confirmation toast
    bool toastOk;      // last save succeeded?
    char toastMsg[32]; // optional explicit message (overrides Saved/Save failed)
} FabMenu;

void      FabMenuInit(FabMenu *f);
bool      FabMenuIsOpen(const FabMenu *f);
void      FabMenuClose(FabMenu *f);

// Returns an action when the user taps an item, otherwise FAB_ACTION_NONE.
// Also claims the current gesture via TouchConsumeGesture when it started on
// the button or inside the open menu, so the player doesn't walk from a tap
// that was meant for the UI.
FabAction FabMenuUpdate(FabMenu *f);
void      FabMenuDraw(const FabMenu *f);

void      FabMenuShowSavedToast(FabMenu *f, bool ok);

// Screen-space rects — exposed so field.c can decide when to draw the grain
// overlay on top without poking at internal layout constants.
Rectangle FabButtonRect(void);
Rectangle FabMenuRect(void);

#endif // FAB_MENU_H
