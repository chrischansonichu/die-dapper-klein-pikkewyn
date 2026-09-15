#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <stdbool.h>
#include "raylib.h"

//----------------------------------------------------------------------------------
// Dialogue box - typewriter effect, multi-page text display
//----------------------------------------------------------------------------------

// Pages are capped by both count and length. Source strings (STR_VAL_LEN)
// are 400 bytes; PAGE_LEN leaves room for the inserted wrap newlines. Pages
// that wrap to more lines than the panel can show are split across extra
// pages at DialogueBegin, so MAX_PAGES is larger than STR_MAX_PAGES.
#define DIALOGUE_MAX_PAGES  16
#define DIALOGUE_PAGE_LEN   420

typedef struct DialogueBox {
    char  pages[DIALOGUE_MAX_PAGES][DIALOGUE_PAGE_LEN];
    int   pageCount;
    int   currentPage;
    int   visibleChars;
    float charTimer;
    float charSpeed;    // chars per second
    bool  active;
    bool  finished;     // true after last page advanced
} DialogueBox;

void DialogueBegin(DialogueBox *d, const char *pages[], int count, float charSpeed);
void DialogueUpdate(DialogueBox *d, float dt);
void DialogueDraw(const DialogueBox *d);
bool DialogueFinished(const DialogueBox *d);

#endif // DIALOGUE_H
