#include "camera_system.h"

Camera2D CameraCreate(Vector2 target, int mapPixW, int mapPixH)
{
    Camera2D cam = {0};
    cam.zoom   = 1.0f;
    cam.target = target;
    cam.offset = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    CameraUpdate(&cam, target, mapPixW, mapPixH);
    return cam;
}

void CameraUpdate(Camera2D *cam, Vector2 target, int mapPixW, int mapPixH)
{
    cam->target = target;
    cam->offset = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };

    // Clamp so camera never shows outside the map
    float halfW = GetScreenWidth()  / 2.0f;
    float halfH = GetScreenHeight() / 2.0f;

    if (cam->target.x < halfW)            cam->target.x = halfW;
    if (cam->target.y < halfH)            cam->target.y = halfH;
    if (cam->target.x > mapPixW - halfW)  cam->target.x = mapPixW - halfW;
    if (cam->target.y > mapPixH - halfH)  cam->target.y = mapPixH - halfH;

    // Guard against tiny maps smaller than the screen
    if (mapPixW < GetScreenWidth())  cam->target.x = mapPixW  / 2.0f;
    if (mapPixH < GetScreenHeight()) cam->target.y = mapPixH / 2.0f;
}
