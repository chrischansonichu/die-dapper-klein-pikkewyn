#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include "raylib.h"
#include <stdbool.h>

// Chunky illustrated touch button. Visual language:
//   - Parchment plate (gPH.panel) for neutral; warm-orange (gPH.roof) for primary
//   - Ink border (gPH.ink), 2.5px stroke, slightly rounded corners
//   - Drop shadow underneath; on press the plate sinks 1–2px into the shadow
//   - Label centered, sized at the caller's request
// Touch-target sizing is the caller's responsibility; aim for ≥ 44×44 logical
// px to honour Apple HIG and stay comfortable for thumbs.
//
// `primary`  paints the accent variant (use sparingly — one CTA per screen)
// `enabled`  false greys the plate and ignores taps; useful for "not enough
//            X" / "nothing selected" states without hiding the affordance
//
// Returns true on tap-up inside the rect (TouchTapInRect under the hood —
// works for both touch and mouse via the systems/touch_input gesture model).
bool DrawChunkyButton(Rectangle r, const char *label, int fontSize,
                      bool primary, bool enabled);

// Round red icon button — used for back/cancel chips. Draws a red plate
// with a left-pointing chevron centered. Returns true on tap.
bool DrawBackIconButton(Rectangle r);

#endif // UI_BUTTON_H
