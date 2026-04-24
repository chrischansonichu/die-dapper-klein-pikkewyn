#ifndef FIELD_BUILDINGS_H
#define FIELD_BUILDINGS_H

#include "raylib.h"

// Draws a Muizenberg-style beach hut — three independent colours for the
// body wall, door, and roof, plus a parchment-cream fascia and dark
// stilts. Outlines use PHWobbleLine so the silhouette feels hand-drawn,
// matching the rest of the procedural art. Each hut should pass a
// different `seed` so the wobble pattern doesn't repeat across the row.
//
// `dst` is in world space (call between BeginMode2D / EndMode2D). The
// caller is responsible for collision tiles underneath — Draw* doesn't
// touch the tilemap, it just paints over it.
void DrawBeachHut(Rectangle dst, Color body, Color door, Color roof,
                  int seed);

// Back-compat shim — hands DrawBeachHut a default Muizenberg palette
// so legacy callers keep working.
void DrawCapeDutchHouse(Rectangle dst);

#endif // FIELD_BUILDINGS_H
