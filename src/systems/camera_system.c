#include "camera_system.h"

Camera2D CameraCreate(Vector2 target, int mapPixW, int mapPixH)
{
    Camera2D cam = {0};
    cam.zoom = 1.0f;
    CameraUpdate(&cam, target, mapPixW, mapPixH);
    return cam;
}

void CameraUpdate(Camera2D *cam, Vector2 target, int mapPixW, int mapPixH)
{
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    if (screenW < 1) screenW = 1;
    if (screenH < 1) screenH = 1;

    // Integer-only math from here. Using halfW/halfH as floats produced a
    // subtle asymmetry: the right/bottom clamp resolved to mapPix - halfFloat,
    // and with round-half-up snap the final target could be 1 pixel above the
    // clamp, leaving a black bar on the right/bottom but not on the left/top.
    // Pairing halfW (for offset) with (screenW - halfW) (for the max clamp)
    // guarantees visible_right = mapPixW exactly at the clamp, even with odd
    // screen dimensions.
    int halfW    = screenW / 2;
    int halfH    = screenH / 2;
    int rightPad = screenW - halfW;
    int downPad  = screenH - halfH;

    cam->target = target;
    cam->offset = (Vector2){ (float)halfW, (float)halfH };

    // Clamp so camera never shows outside the map
    int minTx = halfW;
    int minTy = halfH;
    int maxTx = mapPixW - rightPad;
    int maxTy = mapPixH - downPad;

    if (cam->target.x < (float)minTx) cam->target.x = (float)minTx;
    if (cam->target.y < (float)minTy) cam->target.y = (float)minTy;
    if (cam->target.x > (float)maxTx) cam->target.x = (float)maxTx;
    if (cam->target.y > (float)maxTy) cam->target.y = (float)maxTy;

    // Guard against tiny maps smaller than the screen
    if (mapPixW < screenW) cam->target.x = mapPixW / 2.0f;
    if (mapPixH < screenH) cam->target.y = mapPixH / 2.0f;

    // Snap target to integer pixels, then re-clamp so rounding can't push the
    // visible window past a map edge. Round-half-up on its own would allow the
    // post-snap target to sit 1 pixel outside the clamp on one side.
    int snapX = (int)(cam->target.x + 0.5f);
    int snapY = (int)(cam->target.y + 0.5f);
    if (mapPixW >= screenW) {
        if (snapX < minTx) snapX = minTx;
        if (snapX > maxTx) snapX = maxTx;
    }
    if (mapPixH >= screenH) {
        if (snapY < minTy) snapY = minTy;
        if (snapY > maxTy) snapY = maxTy;
    }
    cam->target.x = (float)snapX;
    cam->target.y = (float)snapY;
}
