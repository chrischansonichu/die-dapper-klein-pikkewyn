#ifndef BATTLE_SPRITES_H
#define BATTLE_SPRITES_H

#include <stdbool.h>
#include "raylib.h"

// Draws a procedurally-constructed sprite for the given creature centered in
// cell `r`. Player-side sprites face right, enemy-side face left.
//
//   slideX / slideY: pixel offsets (lunge + faint slide); applied by this
//                    function so sprite parts stay aligned together.
//   alpha:           0..1 fade factor (used by faint animation).
//   flashWhite:      when true, every filled shape is drawn white (hit flicker).
void DrawCombatantSprite(int creatureId, Rectangle r, bool isEnemy,
                         float alpha, float slideX, float slideY, bool flashWhite);

#endif // BATTLE_SPRITES_H
