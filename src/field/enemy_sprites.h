#ifndef ENEMY_SPRITES_H
#define ENEMY_SPRITES_H

#include "raylib.h"

// Procedural rounded sailor drawing — matches the Elder Penguin / Seal
// visual style from the field (DrawRectangleRounded + DrawCircle +
// DrawTriangle primitives). No texture atlases, no caching.
//
//   dir:        0=down, 1=left, 2=right, 3=up
//   frame:      0 or 1 (walk animation wobble)
//   alpha:      0..1 fade (battle only; field passes 1.0f)
//   flashWhite: tint everything white for the hit-frame flicker (battle)
void EnemySpritesDrawSailor(int creatureId, Rectangle r, int dir, int frame,
                            float alpha, bool flashWhite);

// Kept as no-ops so the field lifecycle hooks (ReloadResources / Unload)
// continue to compile without special-casing the switch to procedural art.
void EnemySpritesReload(void);
void EnemySpritesUnload(void);

#endif // ENEMY_SPRITES_H
