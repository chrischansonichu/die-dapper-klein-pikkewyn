#ifndef CAMERA_SYSTEM_H
#define CAMERA_SYSTEM_H

#include "raylib.h"

//----------------------------------------------------------------------------------
// Camera system - smooth Camera2D follow with map-bound clamping
//----------------------------------------------------------------------------------

Camera2D CameraCreate(Vector2 target, int mapPixW, int mapPixH);
void     CameraUpdate(Camera2D *cam, Vector2 target, int mapPixW, int mapPixH);
// Eased follow. Interpolates cam->target toward `target` with time-constant
// `smoothness` (seconds to ~63% of the way) via alpha = 1 - exp(-dt/smoothness),
// then applies the same clamp + pixel-snap as CameraUpdate. Use during battle
// to pan between actors instead of snapping.
void     CameraUpdateSmoothed(Camera2D *cam, Vector2 target,
                              int mapPixW, int mapPixH,
                              float smoothness, float dt);

#endif // CAMERA_SYSTEM_H
