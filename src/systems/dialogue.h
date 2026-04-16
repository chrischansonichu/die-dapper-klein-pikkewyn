#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <stdbool.h>
#include "raylib.h"

//----------------------------------------------------------------------------------
// Dialogue box - typewriter effect, multi-page text display
//----------------------------------------------------------------------------------

#define DIALOGUE_MAX_PAGES  8
#define DIALOGUE_PAGE_LEN   200

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
