#include "fab_menu.h"
#include "touch_input.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include <stddef.h>

#if SCREEN_PORTRAIT
    #define FAB_SIZE      64
    #define FAB_MARGIN    12
    #define MENU_W        220
    #define MENU_ROW_H    56
    #define MENU_FONT     22
    #define TOAST_FONT    22
    #define DOT_R          4.5f
    #define DOT_STEP      12
#else
    #define FAB_SIZE      44
    #define FAB_MARGIN    10
    #define MENU_W        170
    #define MENU_ROW_H    38
    #define MENU_FONT     16
    #define TOAST_FONT    16
    #define DOT_R          3.0f
    #define DOT_STEP       9
#endif

#ifdef DEV_BUILD
    #define MENU_ITEMS 5
#else
    #define MENU_ITEMS 4
#endif

static const char *kLabels[MENU_ITEMS] = {
#ifdef DEV_BUILD
    "Character", "Inventory", "Save", "Dev Warp", "Close"
#else
    "Character", "Inventory", "Save", "Close"
#endif
};

Rectangle FabButtonRect(void)
{
    int sw = GetScreenWidth();
    return (Rectangle){ (float)(sw - FAB_SIZE - FAB_MARGIN),
                        (float)FAB_MARGIN, (float)FAB_SIZE, (float)FAB_SIZE };
}

Rectangle FabMenuRect(void)
{
    int sw = GetScreenWidth();
    int y = FAB_MARGIN + FAB_SIZE + 8;
    int h = MENU_ROW_H * MENU_ITEMS + 12;
    return (Rectangle){ (float)(sw - MENU_W - FAB_MARGIN),
                        (float)y, (float)MENU_W, (float)h };
}

static Rectangle ItemRect(int i)
{
    Rectangle m = FabMenuRect();
    return (Rectangle){ m.x + 6, m.y + 6 + i * MENU_ROW_H,
                        m.width - 12, (float)(MENU_ROW_H - 2) };
}

void FabMenuInit(FabMenu *f)
{
    f->open        = false;
    f->toastFrames = 0;
    f->toastOk     = true;
}

bool FabMenuIsOpen(const FabMenu *f) { return f->open; }
void FabMenuClose(FabMenu *f)        { f->open = false; }

void FabMenuShowSavedToast(FabMenu *f, bool ok)
{
    f->toastFrames = 90;   // ~1.5s at 60fps
    f->toastOk     = ok;
}

FabAction FabMenuUpdate(FabMenu *f)
{
    if (f->toastFrames > 0) f->toastFrames--;

    Rectangle btn = FabButtonRect();

    // Claim any gesture that started on the button or inside the open menu so
    // the player doesn't walk from that swipe/tap.
    if (TouchGestureStartedIn(btn)) TouchConsumeGesture();
    if (f->open && TouchGestureStartedIn(FabMenuRect())) TouchConsumeGesture();

    if (TouchTapInRect(btn)) {
        f->open = !f->open;
        return FAB_ACTION_NONE;
    }

    if (!f->open) return FAB_ACTION_NONE;

    for (int i = 0; i < MENU_ITEMS; i++) {
        if (TouchTapInRect(ItemRect(i))) {
            f->open = false;
            switch (i) {
                case 0: return FAB_ACTION_STATS;
                case 1: return FAB_ACTION_INVENTORY;
                case 2: return FAB_ACTION_SAVE;
#ifdef DEV_BUILD
                case 3: return FAB_ACTION_DEV_WARP;
                case 4: return FAB_ACTION_NONE;  // Close
#else
                case 3: return FAB_ACTION_NONE;  // Close
#endif
            }
        }
    }

    // Tap outside the menu closes it.
    if (TouchTapOccurred(NULL)) f->open = false;

    return FAB_ACTION_NONE;
}

void FabMenuDraw(const FabMenu *f)
{
    Rectangle btn = FabButtonRect();
    PHDrawPanel(btn, 0x7A01);

    float cx = btn.x + btn.width  * 0.5f;
    float cy = btn.y + btn.height * 0.5f;
    DrawCircle((int)cx, (int)(cy - DOT_STEP), DOT_R, gPH.ink);
    DrawCircle((int)cx, (int)cy,              DOT_R, gPH.ink);
    DrawCircle((int)cx, (int)(cy + DOT_STEP), DOT_R, gPH.ink);

    if (f->open) {
        Rectangle m = FabMenuRect();
        // Dim the rest of the screen a touch so the menu reads as modal.
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), gPH.dimmer);
        PHDrawPanel(m, 0x7A02);
        for (int i = 0; i < MENU_ITEMS; i++) {
            Rectangle r = ItemRect(i);
            int textY = (int)(r.y + (r.height - MENU_FONT) * 0.5f);
            DrawText(kLabels[i], (int)(r.x + 14), textY, MENU_FONT, gPH.ink);
            if (i < MENU_ITEMS - 1) {
                DrawLine((int)(r.x + 8),               (int)(r.y + r.height),
                         (int)(r.x + r.width - 8),     (int)(r.y + r.height),
                         gPH.inkLight);
            }
        }
    }

    if (f->toastFrames > 0) {
        int sw = GetScreenWidth();
        int w  = SCREEN_PORTRAIT ? 220 : 160;
        int h  = SCREEN_PORTRAIT ?  48 :  34;
        Rectangle tr = { (float)(sw - w) * 0.5f,
                         (float)(FAB_MARGIN + FAB_SIZE + 8),
                         (float)w, (float)h };
        PHDrawPanel(tr, 0x7A03);
        const char *msg = f->toastOk ? "Saved" : "Save failed";
        int tw = MeasureText(msg, TOAST_FONT);
        DrawText(msg,
                 (int)(tr.x + (tr.width - tw) * 0.5f),
                 (int)(tr.y + (tr.height - TOAST_FONT) * 0.5f),
                 TOAST_FONT, gPH.ink);
    }
}
