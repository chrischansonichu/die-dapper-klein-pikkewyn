#include "ui_button.h"
#include "touch_input.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"

bool DrawChunkyButton(Rectangle r, const char *label, int fontSize,
                      bool primary, bool enabled)
{
    // "Held" — finger/cursor is currently inside the rect AND the primary
    // pointer is down. Used purely for the visual sink; the actual tap
    // semantics are handled by TouchTapInRect below.
    bool held = enabled &&
                CheckCollisionPointRec(GetMousePosition(), r) &&
                IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    Color plateBase = primary ? gPH.roof  : gPH.panel;
    Color border    = enabled ? gPH.ink   : gPH.inkLight;
    Color labelCol  = primary ? RAYWHITE  : gPH.ink;
    if (!enabled) { labelCol = gPH.inkLight; plateBase = gPH.panel; }

    // Soft drop shadow for depth — skipped on press so the button "sinks".
    if (!held) {
        DrawRectangleRounded((Rectangle){r.x + 2, r.y + 3, r.width, r.height},
                             0.30f, 8, (Color){0, 0, 0, 70});
    }

    Rectangle plate = held ? (Rectangle){r.x + 1, r.y + 2, r.width, r.height} : r;
    DrawRectangleRounded(plate, 0.30f, 8, plateBase);
    DrawRectangleRoundedLinesEx(plate, 0.30f, 8, 2.5f, border);

    int tw = MeasureText(label, fontSize);
    int tx = (int)plate.x + ((int)plate.width  - tw)        / 2;
    int ty = (int)plate.y + ((int)plate.height - fontSize)  / 2;
    DrawText(label, tx, ty, fontSize, labelCol);

    return enabled && TouchTapInRect(r);
}

bool DrawBackIconButton(Rectangle r)
{
    // Re-skinned to match the rest of the UI's parchment-and-ink language —
    // the cherry-red round chip stuck out as the only saturated colour on
    // screen. This now reads as "another chunky button, but with a chevron
    // glyph instead of a label," which is what every other button looks
    // like.
    bool held = CheckCollisionPointRec(GetMousePosition(), r) &&
                IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (!held) {
        DrawRectangleRounded((Rectangle){r.x + 2, r.y + 3, r.width, r.height},
                             0.30f, 8, (Color){0, 0, 0, 70});
    }
    Rectangle pr = held ? (Rectangle){r.x + 1, r.y + 2, r.width, r.height} : r;
    DrawRectangleRounded(pr, 0.30f, 8, gPH.panel);
    DrawRectangleRoundedLinesEx(pr, 0.30f, 8, 2.5f, gPH.ink);

    // Inked left-pointing chevron drawn as two line strokes for a hand-drawn
    // feel — matches the rest of the parchment chrome.
    float cx = pr.x + pr.width  * 0.5f;
    float cy = pr.y + pr.height * 0.5f;
    float reach = pr.width * 0.22f;
    Vector2 tip   = { cx - reach * 0.9f, cy };
    Vector2 upper = { cx + reach * 0.5f, cy - reach * 0.85f };
    Vector2 lower = { cx + reach * 0.5f, cy + reach * 0.85f };
    DrawLineEx(tip, upper, 4.0f, gPH.ink);
    DrawLineEx(tip, lower, 4.0f, gPH.ink);

    return TouchTapInRect(r);
}
