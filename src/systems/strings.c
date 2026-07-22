#include "strings.h"
#include "raylib.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR_MAX_ENTRIES 512
#define STR_KEY_LEN      48
#define STR_VAL_LEN     240

typedef struct StrEntry {
    uint32_t hash;
    char     key[STR_KEY_LEN];
    char     val[STR_VAL_LEN];
} StrEntry;

typedef struct StrTable {
    StrEntry entries[STR_MAX_ENTRIES];
    int      count;
} StrTable;

static StrTable gBase;     // English — always loaded
static StrTable gOverlay;  // selected language — may be empty
static char     gLang[8] = "en";

static uint32_t Fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static const char *TableGet(const StrTable *t, const char *key, uint32_t hash)
{
    for (int i = 0; i < t->count; i++) {
        if (t->entries[i].hash == hash && strcmp(t->entries[i].key, key) == 0)
            return t->entries[i].val;
    }
    return NULL;
}

static void TableSet(StrTable *t, const char *key, const char *val)
{
    uint32_t h = Fnv1a(key);
    for (int i = 0; i < t->count; i++) {
        if (t->entries[i].hash == h && strcmp(t->entries[i].key, key) == 0) {
            snprintf(t->entries[i].val, STR_VAL_LEN, "%s", val);
            return;
        }
    }
    if (t->count >= STR_MAX_ENTRIES) {
        fprintf(stderr, "strings: table full, dropping '%s'\n", key);
        return;
    }
    StrEntry *e = &t->entries[t->count++];
    e->hash = h;
    snprintf(e->key, STR_KEY_LEN, "%s", key);
    snprintf(e->val, STR_VAL_LEN, "%s", val);
}

// Trim leading/trailing spaces + tabs + CR in place; returns start pointer.
static char *Trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        *--end = '\0';
    return s;
}

// Convert "\n" escapes to real newlines in place.
static void Unescape(char *s)
{
    char *w = s;
    for (char *r = s; *r; r++) {
        if (r[0] == '\\' && r[1] == 'n') { *w++ = '\n'; r++; }
        else                             { *w++ = *r; }
    }
    *w = '\0';
}

// Parse a .lang file into `t`. Format per line: key = value. '#' starts a
// comment. Blank lines ignored. Returns false if the file couldn't be read.
static bool TableLoadFile(StrTable *t, const char *path)
{
    int n = 0;
    unsigned char *raw = LoadFileData(path, &n);
    if (!raw || n <= 0) {
        fprintf(stderr, "strings: could not read %s\n", path);
        return false;
    }
    char *text = (char *)malloc((size_t)n + 1);
    if (!text) { UnloadFileData(raw); return false; }
    memcpy(text, raw, (size_t)n);
    text[n] = '\0';
    UnloadFileData(raw);

    char *line = text;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) *next++ = '\0';

        char *s = Trim(line);
        if (*s != '\0' && *s != '#') {
            char *eq = strchr(s, '=');
            if (eq) {
                *eq = '\0';
                char *key = Trim(s);
                char *val = Trim(eq + 1);
                Unescape(val);
                if (*key) TableSet(t, key, val);
            }
        }
        line = next;
    }
    free(text);
    return true;
}

void StringsInit(void)
{
    gBase.count    = 0;
    gOverlay.count = 0;
    snprintf(gLang, sizeof(gLang), "en");
    TableLoadFile(&gBase, "resources/lang/en.lang");
}

bool StringsSetLanguage(const char *code)
{
    gOverlay.count = 0;
    if (!code || strcmp(code, "en") == 0) {
        snprintf(gLang, sizeof(gLang), "en");
        return true;
    }
    char path[64];
    snprintf(path, sizeof(path), "resources/lang/%s.lang", code);
    if (!TableLoadFile(&gOverlay, path)) {
        snprintf(gLang, sizeof(gLang), "en");
        return false;
    }
    snprintf(gLang, sizeof(gLang), "%s", code);
    return true;
}

const char *StringsLanguage(void) { return gLang; }

const char *Str(const char *key)
{
    if (!key) return "";
    uint32_t h = Fnv1a(key);
    const char *v;
    if (gOverlay.count > 0 && (v = TableGet(&gOverlay, key, h)) != NULL) return v;
    if ((v = TableGet(&gBase, key, h)) != NULL) return v;
    return key;  // visible fallback — a raw key on screen means a missing line
}

int StrPages(const char *prefix, const char **out, int maxPages)
{
    int count = 0;
    for (int i = 1; count < maxPages; i++) {
        char key[STR_KEY_LEN];
        snprintf(key, sizeof(key), "%s.%d", prefix, i);
        uint32_t h = Fnv1a(key);
        const char *v = NULL;
        if (gOverlay.count > 0) v = TableGet(&gOverlay, key, h);
        if (!v) v = TableGet(&gBase, key, h);
        if (!v) break;
        out[count++] = v;
    }
    return count;
}
