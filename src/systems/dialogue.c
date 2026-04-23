#include "dialogue.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "touch_input.h"
#include <string.h>

#if SCREEN_PORTRAIT
    #define PANEL_H    230
    #define PANEL_PAD  16
    #define TEXT_SIZE  30
#else
    #define PANEL_H    100
    #define PANEL_PAD  10
    #define TEXT_SIZE  18
#endif
#define PANEL_X    20
#define PANEL_W    (SCREEN_W - 40)
#define PANEL_Y    (SCREEN_H - PANEL_H - 20)

// Word-wrap src into dst at maxPx by inserting '\n' in place of the last space
// that fits. Words longer than maxPx are allowed to overflow. DrawText respects
// embedded newlines, so the typewriter substring logic works unchanged.
static void WrapText(const char *src, char *dst, int dstCap, int maxPx, int fontSize)
{
    int di = 0;
    int lineStart = 0;
    for (int i = 0; src[i] != '\0' && di < dstCap - 2; i++) {
        char c = src[i];
        if (c == '\n') {
            dst[di++] = '\n';
            lineStart = di;
            continue;
        }
        dst[di++] = c;
        dst[di] = '\0';
        int lineW = MeasureText(dst + lineStart, fontSize);
        if (lineW > maxPx) {
            int lastSpace = -1;
            for (int k = di - 1; k > lineStart; k--) {
                if (dst[k] == ' ') { lastSpace = k; break; }
            }
            if (lastSpace >= 0) {
                dst[lastSpace] = '\n';
                lineStart = lastSpace + 1;
            }
        }
    }
    dst[di] = '\0';
}

void DialogueBegin(DialogueBox *d, const char *pages[], int count, float charSpeed)
{
    d->pageCount    = count < DIALOGUE_MAX_PAGES ? count : DIALOGUE_MAX_PAGES;
    d->currentPage  = 0;
    d->visibleChars = 0;
    d->charTimer    = 0.0f;
    d->charSpeed    = charSpeed > 0 ? charSpeed : 30.0f;
    d->active       = true;
    d->finished     = false;

    int wrapPx = PANEL_W - 2 * PANEL_PAD;
    for (int i = 0; i < d->pageCount; i++) {
        WrapText(pages[i], d->pages[i], DIALOGUE_PAGE_LEN, wrapPx, TEXT_SIZE);
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

    // Z/Enter/tap to advance. Tap-anywhere mirrors Z so the mobile build
    // doesn't need a virtual A button to read text.
    bool advance = IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)
                   || TouchTapOccurred(NULL);
    if (advance) {
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
