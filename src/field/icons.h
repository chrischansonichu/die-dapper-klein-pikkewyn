#ifndef FIELD_ICONS_H
#define FIELD_ICONS_H

#include "raylib.h"

// Procedural item / move / armor icons. Each draws into the supplied rect
// using rounded primitives (circles, ellipses, polygons, wobble lines)
// consistent with the rest of the Paper Harbor visual language. No atlas,
// no pixel art — every glyph is computed at draw time.
//
// The rect is the *icon area*; callers typically draw a tile plate behind
// it and compose name / qty / dur overlays on top.

void DrawItemIcon(Rectangle r, int itemId);
void DrawMoveIcon(Rectangle r, int moveId);
void DrawArmorIcon(Rectangle r, int armorId);

#endif // FIELD_ICONS_H
