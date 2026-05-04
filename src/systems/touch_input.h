#ifndef TOUCH_INPUT_H
#define TOUCH_INPUT_H

#include "raylib.h"
#include <stdbool.h>

// One-finger touch/mouse abstraction used by the wasm portrait build so the
// game can be played without the on-screen D-pad. Desktop/keyboard input is
// untouched — this runs in parallel and only emits events when a pointer is
// active. Mouse-left is treated as a touch so desktop devtools and mouse
// testing both work.
//
// Event model:
//   * Swipe — finger moves past the deadzone in a dominant axis. Direction
//     locks on first commit and is reported via TouchHeldDir() until release.
//     TouchPressedDir() fires one-shot on the lock (used by the player's
//     "fresh press turns the sprite" code).
//   * Tap — touch released under TAP_MAX_DUR with total travel under
//     TAP_MAX_DIST. TouchTapOccurred() / TouchTapInRect() consume it.
//   * Consume — UI that owns a tile of screen (FAB button, open menu) calls
//     TouchConsumeGesture() when the gesture started inside its rect so the
//     player doesn't also walk from that swipe.

void    TouchInputUpdate(void);

bool    TouchGestureActive(void);
Vector2 TouchGestureStartPos(void);
bool    TouchGestureStartedIn(Rectangle r);
void    TouchConsumeGesture(void);

// Held swipe direction (0=down, 1=left, 2=right, 3=up), or -1 if none.
int     TouchHeldDir(void);

// One-shot on the frame a swipe direction first locks (or re-locks).
int     TouchPressedDir(void);

// One-shot tap event. Returns true at most once per gesture; consumes the
// event so subsequent callers see false.
bool    TouchTapOccurred(Vector2 *outPos);
bool    TouchTapInRect(Rectangle r);

// Non-consuming peek at a pending tap. Used when a caller wants to inspect
// the tap position (e.g. to decide whether to turn to face an NPC) before
// letting a later consumer claim it. Returns false if no tap is pending.
bool    TouchTapPeek(Vector2 *outPos);

// Per-frame finger Y motion (positive = moved down) for a gesture that
// started inside r AND has committed to a vertical swipe. Returns 0 if no
// such gesture this frame. Touch-released-as-tap stays a tap (since direction
// never locked), so list rows still receive their TouchTapInRect events.
float   TouchScrollDeltaY(Rectangle r);

// Returns true while the active gesture has been held inside `r` for at
// least `secs` without significant motion (≤ TAP_MAX_DIST_PX of drift).
// Use for press-and-hold affordances (long-press tooltips). Lifting the
// finger after `secs` won't fire a tap because the duration exceeds the
// tap window — caller doesn't have to suppress the trailing tap.
bool    TouchHeldInRect(Rectangle r, float secs);

#endif // TOUCH_INPUT_H
