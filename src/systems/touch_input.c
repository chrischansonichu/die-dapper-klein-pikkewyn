#include "touch_input.h"
#include <math.h>

#define DEADZONE_PX      14.0f
#define TAP_MAX_DIST_PX  18.0f
#define TAP_MAX_DUR_S    0.35f
// Distance the finger must travel (from the re-anchored start) to switch
// locked direction mid-gesture. Small enough that you can steer around a
// corner without releasing, big enough that trembling doesn't flip it.
#define REDIRECT_PX      26.0f

static struct {
    bool    active;
    Vector2 startPos;       // re-anchored on each direction lock
    Vector2 gestureStart;   // original touchdown position (for TouchGestureStartedIn)
    Vector2 curPos;
    Vector2 prevPos;
    double  startTime;
    float   totalDist;
    int     lockedDir;
    bool    directionLocked;
    bool    consumed;

    // One-shot outputs (cleared at top of next frame).
    bool    tapReady;
    Vector2 tapPos;
    bool    pressedDirReady;
    int     pressedDir;
} g;

static bool GetPrimaryPoint(Vector2 *out)
{
    if (GetTouchPointCount() > 0) { *out = GetTouchPosition(0); return true; }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { *out = GetMousePosition(); return true; }
    return false;
}

void TouchInputUpdate(void)
{
    g.tapReady        = false;
    g.pressedDirReady = false;

    Vector2 p;
    bool down = GetPrimaryPoint(&p);

    if (down && !g.active) {
        g.active          = true;
        g.startPos        = p;
        g.gestureStart    = p;
        g.curPos          = p;
        g.prevPos         = p;
        g.startTime       = GetTime();
        g.totalDist       = 0.0f;
        g.lockedDir       = -1;
        g.directionLocked = false;
        g.consumed        = false;
        return;
    }

    if (down && g.active) {
        float stepDx = p.x - g.prevPos.x;
        float stepDy = p.y - g.prevPos.y;
        g.totalDist += sqrtf(stepDx * stepDx + stepDy * stepDy);
        g.prevPos    = p;
        g.curPos     = p;

        if (g.consumed) return;

        float dx  = p.x - g.startPos.x;
        float dy  = p.y - g.startPos.y;
        float adx = fabsf(dx), ady = fabsf(dy);
        int   newDir = -1;
        if (adx > DEADZONE_PX || ady > DEADZONE_PX) {
            newDir = (adx > ady) ? (dx > 0 ? 2 : 1)
                                 : (dy > 0 ? 0 : 3);
        }
        if (newDir == -1) return;

        if (!g.directionLocked) {
            g.lockedDir       = newDir;
            g.directionLocked = true;
            g.pressedDir      = newDir;
            g.pressedDirReady = true;
            g.startPos        = p;
        } else if (newDir != g.lockedDir) {
            float rx = p.x - g.startPos.x;
            float ry = p.y - g.startPos.y;
            float rm = sqrtf(rx * rx + ry * ry);
            if (rm > REDIRECT_PX) {
                g.lockedDir       = newDir;
                g.pressedDir      = newDir;
                g.pressedDirReady = true;
                g.startPos        = p;
            }
        }
        return;
    }

    if (!down && g.active) {
        double dur = GetTime() - g.startTime;
        // Consumed gestures still emit the terminal tap — consume blocks the
        // direction stream (so swipes don't leak into field walk) but the tap
        // is what actually reports "the finger went up on this button."
        // Previously gating tap on !consumed meant any UI that pre-claimed a
        // rect on the down-frame silently killed its own tap event.
        if (!g.directionLocked &&
            dur <= TAP_MAX_DUR_S && g.totalDist <= TAP_MAX_DIST_PX) {
            g.tapReady = true;
            g.tapPos   = g.curPos;
        }
        g.active          = false;
        g.directionLocked = false;
        g.lockedDir       = -1;
    }
}

bool TouchGestureActive(void)      { return g.active; }
Vector2 TouchGestureStartPos(void) { return g.active ? g.gestureStart : (Vector2){-1, -1}; }

bool TouchGestureStartedIn(Rectangle r)
{
    if (!g.active) return false;
    Vector2 s = g.gestureStart;
    return (s.x >= r.x && s.x < r.x + r.width &&
            s.y >= r.y && s.y < r.y + r.height);
}

void TouchConsumeGesture(void)
{
    // Claims the direction stream (so a swipe inside a UI rect doesn't walk
    // the player), but leaves the terminal tap alive — the caller that claimed
    // the rect usually *wants* the tap and checks for it with TouchTapInRect.
    g.consumed        = true;
    g.lockedDir       = -1;
    g.directionLocked = false;
    g.pressedDirReady = false;
}

int TouchHeldDir(void)
{
    return (g.active && !g.consumed) ? g.lockedDir : -1;
}

int TouchPressedDir(void)
{
    if (g.pressedDirReady && !g.consumed) {
        g.pressedDirReady = false;
        return g.pressedDir;
    }
    return -1;
}

bool TouchTapOccurred(Vector2 *outPos)
{
    // `consumed` applies to the direction stream only — a UI that claimed a
    // rect on mousedown still wants to receive the terminal tap.
    if (g.tapReady) {
        g.tapReady = false;
        if (outPos) *outPos = g.tapPos;
        return true;
    }
    return false;
}

bool TouchTapPeek(Vector2 *outPos)
{
    if (!g.tapReady) return false;
    if (outPos) *outPos = g.tapPos;
    return true;
}

bool TouchTapInRect(Rectangle r)
{
    if (!g.tapReady) return false;
    if (g.tapPos.x >= r.x && g.tapPos.x < r.x + r.width &&
        g.tapPos.y >= r.y && g.tapPos.y < r.y + r.height) {
        g.tapReady = false;
        return true;
    }
    return false;
}
