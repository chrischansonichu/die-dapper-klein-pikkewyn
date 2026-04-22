#include "dialogue.h"
#include "../render/paper_harbor.h"
#include <string.h>

#define PANEL_X    20
#define PANEL_Y    340
#define PANEL_W    760
#define PANEL_H    100
#define PANEL_PAD  10
#define TEXT_SIZE  18

void DialogueBegin(DialogueBox *d, const char *pages[], int count, float charSpeed)
{
    d->pageCount    = count < DIALOGUE_MAX_PAGES ? count : DIALOGUE_MAX_PAGES;
    d->currentPage  = 0;
    d->visibleChars = 0;
    d->charTimer    = 0.0f;
    d->charSpeed    = charSpeed > 0 ? charSpeed : 30.0f;
    d->active       = true;
    d->finished     = false;

    for (int i = 0; i < d->pageCount; i++) {
        strncpy(d->pages[i], pages[i], DIALOGUE_PAGE_LEN - 1);
        d->pages[i][DIALOGUE_PAGE_LEN - 1] = '\0';
    }
}

void DialogueUpdate(DialogueBox *d, float dt)
{
    if (!d->active || d->finished) return;

    const char *curPage = d->pages[d->currentPage];
    int fullLen = (int)TextLength(curPage);

    // Advance typewriter
    if (d->visibleChars < fullLen) {
        d->charTimer += dt * d->charSpeed;
        int newChars = (int)d->charTimer;
        d->charTimer -= newChars;
        d->visibleChars += newChars;
        if (d->visibleChars > fullLen) d->visibleChars = fullLen;
    }

    // Z/Enter to advance
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
        if (d->visibleChars < fullLen) {
            // Skip to end of page
            d->visibleChars = fullLen;
        } else if (d->currentPage + 1 < d->pageCount) {
            d->currentPage++;
            d->visibleChars = 0;
            d->charTimer    = 0.0f;
        } else {
            d->active   = false;
            d->finished = true;
        }
    }
}

void DialogueDraw(const DialogueBox *d)
{
    if (!d->active) return;

    PHDrawPanel((Rectangle){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, 0x101);

    // Draw visible portion of current page using substring
    const char *text = d->pages[d->currentPage];
    char buf[DIALOGUE_PAGE_LEN];
    int len = d->visibleChars < (int)TextLength(text) ? d->visibleChars : (int)TextLength(text);
    strncpy(buf, text, len);
    buf[len] = '\0';

    DrawText(buf, PANEL_X + PANEL_PAD, PANEL_Y + PANEL_PAD, TEXT_SIZE, gPH.ink);

    // Prompt indicator
    int fullLen = (int)TextLength(text);
    if (d->visibleChars >= fullLen) {
        bool showArrow = ((int)(GetTime() * 3.0) % 2 == 0);
        if (showArrow)
            DrawText("v", PANEL_X + PANEL_W - 24, PANEL_Y + PANEL_H - 22, 16, gPH.inkLight);
    }
}

bool DialogueFinished(const DialogueBox *d)
{
    return d->finished;
}
