#ifndef CAMERA_SYSTEM_H
#define CAMERA_SYSTEM_H

#include "raylib.h"

//----------------------------------------------------------------------------------
// Camera system - smooth Camera2D follow with map-bound clamping
//----------------------------------------------------------------------------------

Camera2D CameraCreate(Vector2 target, int mapPixW, int mapPixH);
void     CameraUpdate(Camera2D *cam, Vector2 target, int mapPixW, int mapPixH);

#endif // CAMERA_SYSTEM_H
