#ifndef STYLE_PREVIEW_H
#define STYLE_PREVIEW_H

#include <stdbool.h>

// Dev-only full-screen modal that renders the same mini-scene (dock + ocean +
// sand + grass + one NPC + player stand-in) in each of four candidate visual
// styles. Opened with F10 from the field; TAB / SHIFT+TAB cycle styles; ESC
// closes. Pure view overlay — does not touch GameState.

typedef enum StylePreviewKind {
    STYLE_PAPER_HARBOR = 0,
    STYLE_LIVING_DIORAMA,
    STYLE_INK_AND_TIDE,
    STYLE_LANTERN_DUSK,
    STYLE_PREVIEW_COUNT,
} StylePreviewKind;

typedef struct StylePreview {
    bool             active;
    StylePreviewKind kind;
    float            animT;   // seconds since open; drives ripples, bob, dusk cycle
} StylePreview;

void StylePreviewInit(StylePreview *sp);
void StylePreviewOpen(StylePreview *sp);
bool StylePreviewIsOpen(const StylePreview *sp);
void StylePreviewUpdate(StylePreview *sp, float dt);
void StylePreviewClose(StylePreview *sp);
void StylePreviewDraw(const StylePreview *sp);

#endif // STYLE_PREVIEW_H
