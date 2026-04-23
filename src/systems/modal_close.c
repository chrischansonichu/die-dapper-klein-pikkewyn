#include "modal_close.h"
#include "touch_input.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"

#if SCREEN_PORTRAIT
    #define CLOSE_SIZE    40
    #define CLOSE_INSET    8
    #define CLOSE_XTHICK   3.0f
#else
    #define CLOSE_SIZE    26
    #define CLOSE_INSET    6
    #define CLOSE_XTHICK   2.0f
#endif

Rectangle ModalCloseButtonRect(Rectangle panel)
{
    return (Rectangle){
        panel.x + panel.width - CLOSE_SIZE - CLOSE_INSET,
        panel.y + CLOSE_INSET,
        (float)CLOSE_SIZE,
        (float)CLOSE_SIZE
    };
}

void ModalCloseButtonDraw(Rectangle panel)
{
    Rectangle r = ModalCloseButtonRect(panel);
    float cx = r.x + r.width  * 0.5f;
    float cy = r.y + r.height * 0.5f;
    float rad = r.width * 0.5f - 2.0f;

    DrawCircle((int)cx, (int)cy, rad + 1.0f, gPH.panel);
    DrawCircleLines((int)cx, (int)cy, rad, gPH.ink);

    float xr = rad * 0.48f;
    DrawLineEx((Vector2){cx - xr, cy - xr}, (Vector2){cx + xr, cy + xr},
               CLOSE_XTHICK, gPH.ink);
    DrawLineEx((Vector2){cx + xr, cy - xr}, (Vector2){cx - xr, cy + xr},
               CLOSE_XTHICK, gPH.ink);
}

bool ModalCloseButtonTapped(Rectangle panel)
{
    return TouchTapInRect(ModalCloseButtonRect(panel));
}

bool ModalTappedOutside(Rectangle panel)
{
    Vector2 tp;
    if (!TouchTapOccurred(&tp)) return false;
    bool inside = (tp.x >= panel.x && tp.x < panel.x + panel.width &&
                   tp.y >= panel.y && tp.y < panel.y + panel.height);
    return !inside;
}
