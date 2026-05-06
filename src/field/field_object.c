#include "field_object.h"
#include "tilemap.h"
#include "../render/paper_harbor.h"

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
    }
}
