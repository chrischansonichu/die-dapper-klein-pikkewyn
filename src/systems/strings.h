#ifndef STRINGS_H
#define STRINGS_H

#include <stdbool.h>

//----------------------------------------------------------------------------------
// Localized string table. ALL player-facing dialogue lives in
// resources/lang/<code>.lang files — key = value, one line each, UTF-8,
// '#' comments, '\n' escapes. English (en.lang) is the canonical table and
// is always loaded as the fallback; a selected language overlays it, so a
// partially-translated file falls back to English line-by-line instead of
// showing blanks.
//
// Multi-page dialogue uses numbered keys ("tut.ryno.meet.1", ".2", ...) —
// StrPages collects them in order, so a translation may even use a
// different page count than English.
//
// Printf-style lines keep their %s/%d placeholders in the .lang file; call
// sites snprintf with Str(key) as the format. Translators must keep the
// placeholders (order included) intact.
//----------------------------------------------------------------------------------

#define STR_MAX_PAGES 8

// Load resources/lang/en.lang as the base table. Call once at startup,
// after ChangeDirectory(GetApplicationDirectory()).
void StringsInit(void);

// Overlay resources/lang/<code>.lang ("af", "zh", ...). "en" (or NULL)
// clears the overlay. Returns false if the file was missing/unreadable —
// the overlay is cleared to plain English in that case.
bool StringsSetLanguage(const char *code);

// Current overlay code ("en" when none).
const char *StringsLanguage(void);

// Localized string for `key`: overlay, then English, then the key itself
// (a visible "tut.ryno.meet.1" in-game means a missing entry — grep for it).
const char *Str(const char *key);

// Collect "<prefix>.1", "<prefix>.2", ... into out[] until a number is
// missing or maxPages is hit. Returns the page count.
int StrPages(const char *prefix, const char **out, int maxPages);

#endif // STRINGS_H
