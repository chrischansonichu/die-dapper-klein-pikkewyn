#include "field_object.h"
#include "tilemap.h"
#include "../render/paper_harbor.h"
#include <math.h>

void FieldObjectInit(FieldObject *o, int tileX, int tileY,
                     ObjectType type, int dataId)
{
    o->tileX    = tileX;
    o->tileY    = tileY;
    o->type     = type;
    o->dataId   = dataId;
    o->active   = true;
    o->consumed = false;
}

bool FieldObjectIsInteractable(const FieldObject *o,
                               int playerTileX, int playerTileY, int playerDir)
{
    if (!o->active) return false;
    int fx = playerTileX, fy = playerTileY;
    switch (playerDir) {
        case 0: fy += 1; break;
        case 1: fx -= 1; break;
        case 2: fx += 1; break;
        case 3: fy -= 1; break;
    }
    return (o->tileX == fx && o->tileY == fy);
}

// Render a small primitive over the object's tile. The draw is intentionally
// tile-center anchored so it composes with whatever floor tile is underneath
// (sand, dock, rock floor) without needing a per-tile sprite.
void FieldObjectDraw(const FieldObject *o, Camera2D cam)
{
    (void)cam;
    if (!o->active) return;

    float px = (float)(o->tileX * TILE_SIZE * TILE_SCALE);
    float py = (float)(o->tileY * TILE_SIZE * TILE_SCALE);
    float cx = px + (TILE_SIZE * TILE_SCALE) / 2.0f;
    float cy = py + (TILE_SIZE * TILE_SCALE) / 2.0f;

    switch (o->type) {
        case OBJ_LOGBOOK: {
            // Open logbook viewed top-down. Two cream pages flanking a dark
            // spine, with a few inked text lines on each page so the object
            // reads as "you can read this" without a label.
            float w = (TILE_SIZE * TILE_SCALE) * 0.78f;
            float h = (TILE_SIZE * TILE_SCALE) * 0.6f;
            Rectangle book = { cx - w * 0.5f, cy - h * 0.5f, w, h };
            // Cover/back peek under the pages.
            Color cover = (Color){90, 60, 40, 255};
            Rectangle coverR = { book.x - 2, book.y + 2, book.width + 4, book.height };
            DrawRectangleRounded(coverR, 0.12f, 3, cover);
            // The two open pages.
            Color paper = (Color){240, 230, 200, 255};
            DrawRectangleRounded(book, 0.10f, 3, paper);
            DrawRectangleRoundedLines(book, 0.10f, 3, gPH.ink);
            // Spine — dark band down the centre.
            DrawRectangle((int)(cx - 1), (int)book.y, 2, (int)book.height,
                          gPH.ink);
            // Text lines: 3 short strokes on each page.
            float pageMargin = 4.0f;
            float lineLen    = (book.width / 2.0f) - pageMargin * 1.5f;
            for (int i = 0; i < 3; i++) {
                float ly = book.y + book.height * 0.30f + i * 6.0f;
                DrawLineEx((Vector2){book.x + pageMargin, ly},
                           (Vector2){book.x + pageMargin + lineLen, ly},
                           1.2f, gPH.inkLight);
                DrawLineEx((Vector2){cx + pageMargin * 0.5f, ly},
                           (Vector2){cx + pageMargin * 0.5f + lineLen, ly},
                           1.2f, gPH.inkLight);
            }
            break;
        }
        case OBJ_LANTERN: {
            // Lantern post with a glowing top when lit; dark when unlit.
            float postW = 4.0f;
            float postH = (TILE_SIZE * TILE_SCALE) * 0.55f;
            DrawRectangle((int)(cx - postW * 0.5f),
                          (int)(cy - postH * 0.2f),
                          (int)postW, (int)postH, gPH.ink);
            float bulbR = 8.0f;
            Color bulb = o->consumed
                            ? (Color){250, 210, 100, 255}
                            : (Color){60, 60, 70, 255};
            DrawCircle((int)cx, (int)(cy - postH * 0.2f), bulbR, bulb);
            DrawCircleLines((int)cx, (int)(cy - postH * 0.2f),
                            bulbR, gPH.ink);
            if (o->consumed) {
                // Soft halo when lit so the player can see at a glance which
                // ones are still pending.
                DrawCircle((int)cx, (int)(cy - postH * 0.2f),
                           bulbR * 1.8f,
                           (Color){250, 210, 100, 60});
            }
            break;
        }
        case OBJ_CHEST: {
            float w = (TILE_SIZE * TILE_SCALE) * 0.7f;
            float h = (TILE_SIZE * TILE_SCALE) * 0.55f;
            Rectangle box = { cx - w * 0.5f, cy - h * 0.4f, w, h };
            Color body = o->consumed
                            ? (Color){90, 70, 50, 255}
                            : (Color){170, 120, 60, 255};
            DrawRectangleRounded(box, 0.18f, 4, body);
            DrawRectangleRoundedLines(box, 0.18f, 4, gPH.ink);
            // Simple band across the lid.
            DrawRectangle((int)box.x, (int)(box.y + h * 0.35f),
                          (int)box.width, 3, gPH.ink);
            break;
        }
        case OBJ_TIDEPOOL: {
            // Shallow rock pool: dark rim, sea-glass water, a couple of
            // glints. A silver flick marks a stranded sardine while the pool
            // still holds one; consumed pools sit still until the next tide
            // (map rebuild).
            float tp = (float)(TILE_SIZE * TILE_SCALE);
            DrawEllipse((int)cx, (int)cy, tp * 0.40f, tp * 0.28f,
                        (Color){0x6E, 0x62, 0x50, 255});          // wet rock rim
            DrawEllipse((int)cx, (int)cy, tp * 0.33f, tp * 0.22f,
                        (Color){0x4E, 0x8A, 0x96, 255});          // pool water
            DrawEllipse((int)(cx - tp * 0.10f), (int)(cy - tp * 0.06f),
                        tp * 0.10f, 0.05f * tp,
                        (Color){0xBF, 0xE3, 0xE6, 190});          // sky glint
            if (!o->consumed) {
                DrawEllipse((int)(cx + tp * 0.08f), (int)(cy + tp * 0.05f),
                            tp * 0.09f, tp * 0.035f,
                            (Color){0xD8, 0xD8, 0xC8, 255});      // the sardine
            }
            break;
        }
        case OBJ_BUOY: {
            // Listing bell buoy: rust-red float with a pale waterline band
            // and a stubby bell mast. Sits in open water, so it draws a hint
            // of its own ripple.
            float tp = (float)(TILE_SIZE * TILE_SCALE);
            DrawEllipse((int)cx, (int)(cy + tp * 0.18f),
                        tp * 0.34f, tp * 0.10f,
                        (Color){0xEE, 0xF4, 0xEE, 70});           // ripple
            DrawCircle((int)cx, (int)cy, tp * 0.24f,
                       (Color){0xA8, 0x4A, 0x38, 255});           // float
            DrawCircleLines((int)cx, (int)cy, tp * 0.24f, gPH.ink);
            DrawRectangle((int)(cx - tp * 0.24f), (int)(cy + tp * 0.04f),
                          (int)(tp * 0.48f), 3,
                          (Color){0xE8, 0xDC, 0xC4, 255});        // waterline band
            DrawRectangle((int)(cx - 1), (int)(cy - tp * 0.36f), 3,
                          (int)(tp * 0.16f), gPH.ink);            // bell mast
            DrawCircle((int)cx, (int)(cy - tp * 0.38f), 4.0f,
                       (Color){0xC8, 0xA8, 0x50, 255});           // the bell
            break;
        }
        case OBJ_NEST: {
            // Storm-nest on the west point: a weathered ring of driftwood
            // twigs. While the feather is still wedged in the weave (not
            // consumed), a single black-over-white plume leans out of it.
            float tp = (float)(TILE_SIZE * TILE_SCALE);
            const Color twig     = (Color){0xB8, 0xA8, 0x8C, 255};
            const Color twigDark = (Color){0x8A, 0x7A, 0x60, 255};
            DrawEllipse((int)cx, (int)cy, tp * 0.36f, tp * 0.24f, twigDark);
            DrawEllipse((int)cx, (int)cy, tp * 0.26f, tp * 0.16f,
                        (Color){0x6E, 0x62, 0x50, 255});          // hollow
            // Criss-cross twigs around the rim.
            for (int i = 0; i < 6; i++) {
                float a  = (float)i * 1.047f;                     // ~60° steps
                float ex = cx + cosf(a) * tp * 0.34f;
                float ey = cy + sinf(a) * tp * 0.22f;
                DrawLineEx((Vector2){cx + cosf(a + 0.6f) * tp * 0.18f,
                                     cy + sinf(a + 0.6f) * tp * 0.12f},
                           (Vector2){ex, ey}, 2.5f, twig);
            }
            if (!o->consumed) {
                // Annika's feather — black vane over a white base.
                DrawLineEx((Vector2){cx + tp * 0.06f, cy - tp * 0.02f},
                           (Vector2){cx + tp * 0.16f, cy - tp * 0.30f},
                           4.0f, (Color){0xF2, 0xF0, 0xE8, 255});
                DrawLineEx((Vector2){cx + tp * 0.11f, cy - tp * 0.16f},
                           (Vector2){cx + tp * 0.16f, cy - tp * 0.30f},
                           4.0f, (Color){0x30, 0x30, 0x38, 255});
            }
            break;
        }
        case OBJ_CRATE: {
            // Splintered cargo crate, half-buried at an angle, with a scrap
            // of fine netting spilling out. Foreign stamp suggested by two
            // meaningless glyph strokes.
            float tp = (float)(TILE_SIZE * TILE_SCALE);
            Rectangle box = { cx - tp * 0.34f, cy - tp * 0.22f,
                              tp * 0.62f, tp * 0.44f };
            DrawRectangleRounded(box, 0.08f, 3, (Color){0xB0, 0x8A, 0x58, 255});
            DrawRectangleRoundedLines(box, 0.08f, 3, gPH.ink);
            // Plank seams + a splintered corner.
            DrawLineEx((Vector2){box.x, box.y + box.height * 0.5f},
                       (Vector2){box.x + box.width, box.y + box.height * 0.5f},
                       1.5f, gPH.inkLight);
            DrawTriangle((Vector2){box.x + box.width, box.y},
                         (Vector2){box.x + box.width + 7, box.y + 4},
                         (Vector2){box.x + box.width, box.y + 12},
                         (Color){0xB0, 0x8A, 0x58, 255});
            // Foreign stamp: two strokes that read as no alphabet.
            DrawLineEx((Vector2){cx - tp * 0.16f, cy - tp * 0.10f},
                       (Vector2){cx - tp * 0.02f, cy + tp * 0.02f}, 2.0f, gPH.ink);
            DrawLineEx((Vector2){cx - tp * 0.02f, cy - tp * 0.10f},
                       (Vector2){cx - tp * 0.16f, cy + tp * 0.02f}, 2.0f, gPH.ink);
            // Net scrap trailing into the sand.
            for (int i = 0; i < 3; i++) {
                float nx = box.x + box.width * 0.2f + i * 6.0f;
                DrawLineEx((Vector2){nx, box.y + box.height},
                           (Vector2){nx + 4.0f, box.y + box.height + 10.0f},
                           1.2f, (Color){0x88, 0x94, 0x8C, 255});
            }
            break;
        }
        case OBJ_DECOR: {
            float tp = (float)(TILE_SIZE * TILE_SCALE);
            switch (o->dataId) {
                case DECOR_DRIFTWOOD: {
                    // One thick silvered log with a knot.
                    DrawLineEx((Vector2){px + tp * 0.10f, cy + tp * 0.12f},
                               (Vector2){px + tp * 0.92f, cy - tp * 0.08f},
                               7.0f, (Color){0xC4, 0xBE, 0xB0, 255});
                    DrawLineEx((Vector2){px + tp * 0.10f, cy + tp * 0.12f},
                               (Vector2){px + tp * 0.92f, cy - tp * 0.08f},
                               2.0f, (Color){0xA8, 0xA0, 0x90, 255});
                    DrawCircle((int)(cx + tp * 0.10f), (int)cy, 3.0f,
                               (Color){0x8A, 0x7A, 0x60, 255});
                    break;
                }
                case DECOR_SHELLS: {
                    // A loose drift of shell flecks.
                    const Color shell = (Color){0xE8, 0xDC, 0xC4, 255};
                    const Color pink  = (Color){0xD8, 0xB0, 0xA0, 255};
                    DrawEllipse((int)(cx - tp * 0.14f), (int)(cy + tp * 0.06f),
                                5, 3, shell);
                    DrawEllipse((int)(cx + tp * 0.10f), (int)(cy - tp * 0.04f),
                                4, 3, pink);
                    DrawEllipse((int)(cx + tp * 0.02f), (int)(cy + tp * 0.14f),
                                4, 2, shell);
                    DrawEllipse((int)(cx - tp * 0.04f), (int)(cy - tp * 0.12f),
                                3, 2, pink);
                    DrawEllipse((int)(cx + tp * 0.20f), (int)(cy + tp * 0.10f),
                                3, 2, shell);
                    break;
                }
                case DECOR_STONES: {
                    // Vlerkie's five-stone stack, biggest to smallest.
                    const Color stone = (Color){0x9A, 0x96, 0x8C, 255};
                    float w = tp * 0.34f;
                    float y0 = cy + tp * 0.20f;
                    for (int i = 0; i < 5; i++) {
                        DrawEllipse((int)cx, (int)(y0 - i * 5.5f),
                                    w * 0.5f, 3.5f, stone);
                        DrawEllipse((int)cx, (int)(y0 - i * 5.5f - 1.5f),
                                    w * 0.5f, 2.5f,
                                    (Color){0xB2, 0xAE, 0xA4, 255});
                        w *= 0.78f;
                    }
                    break;
                }
            }
            break;
        }
        case OBJ_RACK: {
            // Fish-drying rack: two driftwood posts, a taut line, and a row
            // of small silver fish hung to cure. The thing the gulls raid.
            float tp = (float)(TILE_SIZE * TILE_SCALE);
            const Color post = (Color){0x8A, 0x7A, 0x60, 255};
            const Color fish = (Color){0xC9, 0xCE, 0xD2, 255};
            float lx = px + tp * 0.14f, rx2 = px + tp * 0.86f;
            float topY = cy - tp * 0.22f;
            DrawLineEx((Vector2){lx, cy + tp * 0.30f},
                       (Vector2){lx, topY}, 4.0f, post);
            DrawLineEx((Vector2){rx2, cy + tp * 0.30f},
                       (Vector2){rx2, topY}, 4.0f, post);
            DrawLineEx((Vector2){lx, topY}, (Vector2){rx2, topY},
                       2.0f, gPH.ink);
            for (int i = 0; i < 3; i++) {
                float fx2 = lx + (rx2 - lx) * (0.25f + 0.25f * (float)i);
                DrawLineEx((Vector2){fx2, topY},
                           (Vector2){fx2, topY + 5.0f}, 1.5f, gPH.inkLight);
                DrawEllipse((int)fx2, (int)(topY + 10.0f), 3.5f, 6.0f, fish);
                DrawCircle((int)fx2, (int)(topY + 6.5f), 1.5f,
                           (Color){0x6E, 0x76, 0x7C, 255});
            }
            break;
        }
        case OBJ_BLOCKAGE: {
            // Storm-kelp tangle: a mound of overlapping olive fronds lashed
            // across the tile, with a couple of bleached driftwood spars.
            // Deliberately messy — it should read as "in the way".
            const Color kelp     = (Color){0x5E, 0x74, 0x4A, 255};
            const Color kelpDark = (Color){0x42, 0x54, 0x36, 255};
            const Color wood     = (Color){0xC0, 0xA8, 0x84, 255};
            float tp = (float)(TILE_SIZE * TILE_SCALE);

            DrawEllipse((int)cx, (int)(cy + tp * 0.16f),
                        tp * 0.42f, tp * 0.20f, kelpDark);
            // Driftwood spars poking out at angles.
            DrawLineEx((Vector2){px + tp * 0.10f, cy + tp * 0.24f},
                       (Vector2){px + tp * 0.86f, cy - tp * 0.10f}, 4.0f, wood);
            DrawLineEx((Vector2){px + tp * 0.20f, cy - tp * 0.18f},
                       (Vector2){px + tp * 0.90f, cy + tp * 0.20f}, 3.0f, wood);
            // Frond loops piled over the wood.
            DrawEllipse((int)(cx - tp * 0.16f), (int)cy,
                        tp * 0.22f, tp * 0.14f, kelp);
            DrawEllipse((int)(cx + tp * 0.14f), (int)(cy + tp * 0.06f),
                        tp * 0.24f, tp * 0.15f, kelp);
            DrawEllipse((int)cx, (int)(cy - tp * 0.12f),
                        tp * 0.20f, tp * 0.12f, kelp);
            // Trailing strands down the tile edges.
            DrawLineEx((Vector2){cx - tp * 0.30f, cy + tp * 0.10f},
                       (Vector2){cx - tp * 0.38f, cy + tp * 0.34f}, 3.0f, kelpDark);
            DrawLineEx((Vector2){cx + tp * 0.28f, cy + tp * 0.12f},
                       (Vector2){cx + tp * 0.38f, cy + tp * 0.36f}, 3.0f, kelpDark);
            break;
        }
    }
}
