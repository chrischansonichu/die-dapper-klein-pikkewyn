#ifndef MODAL_CLOSE_H
#define MODAL_CLOSE_H

#include "raylib.h"
#include <stdbool.h>

// Shared close-button hit target that every in-field modal reuses so the
// mobile build can close UIs without a keyboard. The button sits in the
// top-right of the modal's panel, at a touch-friendly size on portrait and
// a tighter size on desktop. Tap-to-close is routed through touch_input so
// it also consumes the tap (nothing downstream misinterprets it as a field
// interaction after the modal closes).

// Rect of the close button relative to the passed-in panel rect. Useful if
// the modal wants to avoid drawing content under it.
Rectangle ModalCloseButtonRect(Rectangle panel);

// Draws an outlined circle with an ink-X inside, styled to match the paper-
// harbor panel it sits on.
void      ModalCloseButtonDraw(Rectangle panel);

// True on the frame the user taps the button; consumes the tap. Mouse-click
// (via touch_input's mouse fallback) works the same way.
bool      ModalCloseButtonTapped(Rectangle panel);

// True on the frame the user taps anywhere *outside* the panel. Some modals
// (read-only overlays, warp prompt) want this as a cancel. Others (the bag,
// the blacksmith) don't, because a stray tap outside would nuke a careful
// selection — they just ignore and rely on the explicit close button.
bool      ModalTappedOutside(Rectangle panel);

#endif // MODAL_CLOSE_H
