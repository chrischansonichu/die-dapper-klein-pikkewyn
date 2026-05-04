// SDL3 implementation of the raylib API surface declared in raylib.h.
//
// Single-window single-renderer model — InitWindow creates the SDL_Window +
// SDL_Renderer pair and stashes them in module-level statics. All draw calls
// route through that renderer. Frame pacing happens via SDL's vsync (set on
// the renderer) plus a fallback delay loop when SetTargetFPS is called and
// vsync is disabled.
//
// Input model: WindowShouldClose() runs once per frame and is the only
// place we pump SDL events. It snapshots prev/cur key+mouse state so
// IsKeyPressed/IsMouseButtonPressed can detect this-frame transitions.
//
// Texture handles: SDL_Texture* lives in Texture2D._sdl. The raylib-style
// `id` field is a non-zero sentinel (1) when loaded so existing checks
// like `if (tex.id != 0)` keep working.
//
// Text path: each DrawTextEx call renders to an SDL_Surface via
// TTF_RenderText_Blended, uploads to a one-shot SDL_Texture, blits, and
// destroys. Slow but correct — caching arrives in a later pass.

#include "raylib.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>  // chdir

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static int           g_logical_w = 0;
static int           g_logical_h = 0;
static unsigned int  g_config_flags = 0;
static bool          g_should_quit = false;
static int           g_target_fps = 0;
static Uint64        g_last_frame_ns = 0;
static Uint64        g_init_ns = 0;
static float         g_frame_dt_seconds = 0.0f;
static char          g_app_dir[2048] = {0};
static unsigned int  g_next_tex_id = 1;  // monotonic — only used as a sentinel

// Keyboard state (indexed by SDL_Scancode)
#define MAX_SCANCODES 512
static bool g_key_cur[MAX_SCANCODES];
static bool g_key_prev[MAX_SCANCODES];

// Mouse state (raylib's button numbering: 0=left, 1=right, 2=middle)
static bool  g_mouse_cur[8];
static bool  g_mouse_prev[8];
static float g_mouse_x = 0.0f;
static float g_mouse_y = 0.0f;

// Camera2D transform — applied by BeginMode2D / cleared by EndMode2D.
// world → screen: screen = (world - target) * zoom + offset.
// Rotation is not applied to the renderer (SDL3 has no rotate-without-target),
// but we honor it in the math helpers so game-side projection still works.
static bool  g_view_active = false;
static float g_view_offset_x = 0.0f;
static float g_view_offset_y = 0.0f;
static float g_view_target_x = 0.0f;
static float g_view_target_y = 0.0f;
static float g_view_zoom = 1.0f;

static inline void XformPoint(float *x, float *y) {
    if (!g_view_active) return;
    float wx = *x - g_view_target_x;
    float wy = *y - g_view_target_y;
    *x = wx * g_view_zoom + g_view_offset_x;
    *y = wy * g_view_zoom + g_view_offset_y;
}
static inline float XformLen(float v) { return g_view_active ? v * g_view_zoom : v; }

// ---------------------------------------------------------------------------
// Key translation: raylib KEY_* → SDL_Scancode
// ---------------------------------------------------------------------------

static SDL_Scancode RlKeyToScancode(int key) {
    // ASCII letters map directly to SDL_SCANCODE_A..Z
    if (key >= KEY_A && key <= KEY_Z) {
        return (SDL_Scancode)(SDL_SCANCODE_A + (key - KEY_A));
    }
    if (key >= KEY_ZERO && key <= KEY_NINE) {
        return (SDL_Scancode)(SDL_SCANCODE_1 + ((key - KEY_ZERO + 9) % 10));
    }
    switch (key) {
        case KEY_SPACE:        return SDL_SCANCODE_SPACE;
        case KEY_ENTER:        return SDL_SCANCODE_RETURN;
        case KEY_ESCAPE:       return SDL_SCANCODE_ESCAPE;
        case KEY_TAB:          return SDL_SCANCODE_TAB;
        case KEY_BACKSPACE:     return SDL_SCANCODE_BACKSPACE;
        case KEY_DELETE:        return SDL_SCANCODE_DELETE;
        case KEY_LEFT_BRACKET:  return SDL_SCANCODE_LEFTBRACKET;
        case KEY_RIGHT_BRACKET: return SDL_SCANCODE_RIGHTBRACKET;
        case KEY_LEFT:         return SDL_SCANCODE_LEFT;
        case KEY_RIGHT:        return SDL_SCANCODE_RIGHT;
        case KEY_UP:           return SDL_SCANCODE_UP;
        case KEY_DOWN:         return SDL_SCANCODE_DOWN;
        case KEY_F1:           return SDL_SCANCODE_F1;
        case KEY_F2:           return SDL_SCANCODE_F2;
        case KEY_F3:           return SDL_SCANCODE_F3;
        case KEY_F4:           return SDL_SCANCODE_F4;
        case KEY_F5:           return SDL_SCANCODE_F5;
        case KEY_F6:           return SDL_SCANCODE_F6;
        case KEY_F7:           return SDL_SCANCODE_F7;
        case KEY_F8:           return SDL_SCANCODE_F8;
        case KEY_F9:           return SDL_SCANCODE_F9;
        case KEY_F10:          return SDL_SCANCODE_F10;
        case KEY_LEFT_SHIFT:    return SDL_SCANCODE_LSHIFT;
        case KEY_LEFT_CONTROL:  return SDL_SCANCODE_LCTRL;
        case KEY_LEFT_ALT:      return SDL_SCANCODE_LALT;
        case KEY_RIGHT_SHIFT:   return SDL_SCANCODE_RSHIFT;
        case KEY_RIGHT_CONTROL: return SDL_SCANCODE_RCTRL;
        case KEY_RIGHT_ALT:     return SDL_SCANCODE_RALT;
        case KEY_COMMA:        return SDL_SCANCODE_COMMA;
        case KEY_PERIOD:       return SDL_SCANCODE_PERIOD;
        case KEY_SLASH:        return SDL_SCANCODE_SLASH;
        case KEY_SEMICOLON:    return SDL_SCANCODE_SEMICOLON;
        case KEY_EQUAL:        return SDL_SCANCODE_EQUALS;
        case KEY_MINUS:        return SDL_SCANCODE_MINUS;
        case KEY_APOSTROPHE:   return SDL_SCANCODE_APOSTROPHE;
        default:               return SDL_SCANCODE_UNKNOWN;
    }
}

// ---------------------------------------------------------------------------
// Window / lifecycle
// ---------------------------------------------------------------------------

void SetConfigFlags(unsigned int flags) {
    g_config_flags = flags;
}

void InitWindow(int width, int height, const char *title) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(1);
    }
    if (!TTF_Init()) {
        fprintf(stderr, "TTF_Init failed: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_WindowFlags wf = 0;
    if (g_config_flags & FLAG_WINDOW_HIGHDPI)  wf |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (g_config_flags & FLAG_WINDOW_RESIZABLE) wf |= SDL_WINDOW_RESIZABLE;
    if (g_config_flags & FLAG_FULLSCREEN_MODE)  wf |= SDL_WINDOW_FULLSCREEN;

    if (!SDL_CreateWindowAndRenderer(title, width, height, wf, &g_window, &g_renderer)) {
        fprintf(stderr, "CreateWindowAndRenderer failed: %s\n", SDL_GetError());
        exit(1);
    }

    if (g_config_flags & FLAG_VSYNC_HINT) {
        SDL_SetRenderVSync(g_renderer, 1);
    }
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    // Logical presentation size lets us use raylib-style coordinates while
    // SDL handles the high-DPI backbuffer scaling.
    SDL_SetRenderLogicalPresentation(g_renderer, width, height,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    g_logical_w = width;
    g_logical_h = height;
    g_init_ns = SDL_GetTicksNS();
    g_last_frame_ns = g_init_ns;

    // Cache the binary's directory for GetApplicationDirectory.
    const char *base = SDL_GetBasePath();
    if (base) {
        SDL_strlcpy(g_app_dir, base, sizeof(g_app_dir));
    }
}

void CloseWindow(void) {
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window)   { SDL_DestroyWindow(g_window);     g_window   = NULL; }
    TTF_Quit();
    SDL_Quit();
}

bool WindowShouldClose(void) {
    // Snapshot for edge detection.
    memcpy(g_key_prev,   g_key_cur,   sizeof(g_key_cur));
    memcpy(g_mouse_prev, g_mouse_cur, sizeof(g_mouse_cur));

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                g_should_quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.scancode < MAX_SCANCODES) g_key_cur[e.key.scancode] = true;
                if (e.key.scancode == SDL_SCANCODE_ESCAPE) g_should_quit = true;
                break;
            case SDL_EVENT_KEY_UP:
                if (e.key.scancode < MAX_SCANCODES) g_key_cur[e.key.scancode] = false;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                // SDL: 1=left,2=middle,3=right. raylib: 0=left,1=right,2=middle.
                switch (e.button.button) {
                    case SDL_BUTTON_LEFT:   g_mouse_cur[MOUSE_BUTTON_LEFT]   = true; break;
                    case SDL_BUTTON_RIGHT:  g_mouse_cur[MOUSE_BUTTON_RIGHT]  = true; break;
                    case SDL_BUTTON_MIDDLE: g_mouse_cur[MOUSE_BUTTON_MIDDLE] = true; break;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                switch (e.button.button) {
                    case SDL_BUTTON_LEFT:   g_mouse_cur[MOUSE_BUTTON_LEFT]   = false; break;
                    case SDL_BUTTON_RIGHT:  g_mouse_cur[MOUSE_BUTTON_RIGHT]  = false; break;
                    case SDL_BUTTON_MIDDLE: g_mouse_cur[MOUSE_BUTTON_MIDDLE] = false; break;
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                g_mouse_x = e.motion.x;
                g_mouse_y = e.motion.y;
                break;
        }
    }
    return g_should_quit;
}

int GetScreenWidth(void)  { return g_logical_w; }
int GetScreenHeight(void) { return g_logical_h; }

void SetTargetFPS(int fps) {
    g_target_fps = fps;
    g_last_frame_ns = SDL_GetTicksNS();
}

int ChangeDirectory(const char *dir) {
    return chdir(dir);
}

const char *GetApplicationDirectory(void) {
    return g_app_dir;
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

void BeginDrawing(void) {
    // SDL3 doesn't need an explicit begin — RenderClear/draw calls just go.
    // We model BeginDrawing as a no-op; ClearBackground does the work.
}

void EndDrawing(void) {
    SDL_RenderPresent(g_renderer);

    // Soft FPS cap when vsync is off.
    if (g_target_fps > 0 && !(g_config_flags & FLAG_VSYNC_HINT)) {
        const Uint64 frame_ns = 1000000000ULL / (Uint64)g_target_fps;
        const Uint64 now = SDL_GetTicksNS();
        const Uint64 elapsed = now - g_last_frame_ns;
        if (elapsed < frame_ns) SDL_DelayNS(frame_ns - elapsed);
    }

    // Frame-time bookkeeping for GetFrameTime.
    const Uint64 now = SDL_GetTicksNS();
    g_frame_dt_seconds = (float)((now - g_last_frame_ns) / 1.0e9);
    g_last_frame_ns = now;
}

float GetFrameTime(void) { return g_frame_dt_seconds; }
double GetTime(void)     { return (double)(SDL_GetTicksNS() - g_init_ns) / 1.0e9; }

void ClearBackground(Color c) {
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    SDL_RenderClear(g_renderer);
}

// ---------------------------------------------------------------------------
// Shapes
// ---------------------------------------------------------------------------

void DrawRectangle(int x, int y, int w, int h, Color c) {
    float fx = (float)x, fy = (float)y;
    XformPoint(&fx, &fy);
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    SDL_FRect r = { fx, fy, XformLen((float)w), XformLen((float)h) };
    SDL_RenderFillRect(g_renderer, &r);
}

void DrawRectangleRec(Rectangle r, Color c) {
    float fx = r.x, fy = r.y;
    XformPoint(&fx, &fy);
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    SDL_FRect fr = { fx, fy, XformLen(r.width), XformLen(r.height) };
    SDL_RenderFillRect(g_renderer, &fr);
}

void DrawRectangleLinesEx(Rectangle r, float thickness, Color c) {
    float fx = r.x, fy = r.y;
    XformPoint(&fx, &fy);
    float fw = XformLen(r.width);
    float fh = XformLen(r.height);
    float ft = XformLen(thickness);
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    SDL_FRect top    = { fx,           fy,           fw, ft };
    SDL_FRect bottom = { fx,           fy + fh - ft, fw, ft };
    SDL_FRect left   = { fx,           fy,           ft, fh };
    SDL_FRect right  = { fx + fw - ft, fy,           ft, fh };
    SDL_RenderFillRect(g_renderer, &top);
    SDL_RenderFillRect(g_renderer, &bottom);
    SDL_RenderFillRect(g_renderer, &left);
    SDL_RenderFillRect(g_renderer, &right);
}

void DrawRectangleGradientV(int x, int y, int w, int h, Color top, Color bottom) {
    float fx = (float)x, fy = (float)y;
    XformPoint(&fx, &fy);
    float fw = XformLen((float)w), fh = XformLen((float)h);
    SDL_FColor c_top    = { top.r/255.0f,    top.g/255.0f,    top.b/255.0f,    top.a/255.0f };
    SDL_FColor c_bottom = { bottom.r/255.0f, bottom.g/255.0f, bottom.b/255.0f, bottom.a/255.0f };
    SDL_Vertex v[4] = {
        { { fx,      fy      }, c_top,    {0,0} },
        { { fx + fw, fy      }, c_top,    {0,0} },
        { { fx + fw, fy + fh }, c_bottom, {0,0} },
        { { fx,      fy + fh }, c_bottom, {0,0} },
    };
    int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(g_renderer, NULL, v, 4, idx, 6);
}

void DrawCircle(int cx, int cy, float radius, Color c) {
    float fcx = (float)cx, fcy = (float)cy;
    XformPoint(&fcx, &fcy);
    float r = XformLen(radius);
    int segments = (int)(r * 0.6f); if (segments < 12) segments = 12; if (segments > 64) segments = 64;
    SDL_FColor fc = { c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f };
    SDL_Vertex *verts = (SDL_Vertex*)malloc(sizeof(SDL_Vertex) * (segments + 1));
    int *idx = (int*)malloc(sizeof(int) * segments * 3);
    verts[0].position = (SDL_FPoint){ fcx, fcy };
    verts[0].color = fc;
    verts[0].tex_coord = (SDL_FPoint){0,0};
    for (int i = 0; i < segments; i++) {
        float a = (float)i / (float)segments * 6.28318530718f;
        verts[i + 1].position = (SDL_FPoint){ fcx + cosf(a) * r, fcy + sinf(a) * r };
        verts[i + 1].color = fc;
        verts[i + 1].tex_coord = (SDL_FPoint){0,0};
        idx[i*3 + 0] = 0;
        idx[i*3 + 1] = i + 1;
        idx[i*3 + 2] = (i + 1) % segments + 1;
    }
    SDL_RenderGeometry(g_renderer, NULL, verts, segments + 1, idx, segments * 3);
    free(verts);
    free(idx);
}

void DrawLineEx(Vector2 a, Vector2 b, float thickness, Color c) {
    XformPoint(&a.x, &a.y);
    XformPoint(&b.x, &b.y);
    float t = XformLen(thickness);
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.0001f) return;
    float nx = -dy / len * t * 0.5f;
    float ny =  dx / len * t * 0.5f;
    SDL_FColor fc = { c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f };
    SDL_Vertex v[4] = {
        { { a.x - nx, a.y - ny }, fc, {0,0} },
        { { a.x + nx, a.y + ny }, fc, {0,0} },
        { { b.x + nx, b.y + ny }, fc, {0,0} },
        { { b.x - nx, b.y - ny }, fc, {0,0} },
    };
    int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(g_renderer, NULL, v, 4, idx, 6);
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------

Texture2D LoadTexture(const char *path) {
    Texture2D t = {0};
    SDL_Texture *st = IMG_LoadTexture(g_renderer, path);
    if (!st) {
        fprintf(stderr, "LoadTexture(%s) failed: %s\n", path, SDL_GetError());
        return t;
    }
    SDL_SetTextureScaleMode(st, SDL_SCALEMODE_LINEAR);
    float w = 0, h = 0;
    SDL_GetTextureSize(st, &w, &h);
    t.id     = g_next_tex_id++;
    t.width  = (int)w;
    t.height = (int)h;
    t._sdl   = st;
    return t;
}

Texture2D LoadTextureFromImage(Image img) {
    Texture2D t = {0};
    if (!img.data || img.width <= 0 || img.height <= 0) return t;
    // Image data is RGBA8 per GenImageColor / ImageDrawPixel.
    SDL_Surface *surf = SDL_CreateSurfaceFrom(img.width, img.height,
                                              SDL_PIXELFORMAT_RGBA32,
                                              img.data, img.width * 4);
    if (!surf) return t;
    SDL_Texture *st = SDL_CreateTextureFromSurface(g_renderer, surf);
    SDL_DestroySurface(surf);
    if (!st) return t;
    SDL_SetTextureScaleMode(st, SDL_SCALEMODE_LINEAR);
    t.id     = g_next_tex_id++;
    t.width  = img.width;
    t.height = img.height;
    t._sdl   = st;
    return t;
}

void UnloadTexture(Texture2D tex) {
    if (tex._sdl) SDL_DestroyTexture((SDL_Texture*)tex._sdl);
}

void DrawTexturePro(Texture2D tex, Rectangle src, Rectangle dst,
                    Vector2 origin, float rotation, Color tint) {
    SDL_Texture *st = (SDL_Texture*)tex._sdl;
    if (!st) return;
    SDL_SetTextureColorMod(st, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(st, tint.a);

    SDL_FRect s = { src.x, src.y, src.width, src.height };
    float dx = dst.x - origin.x, dy = dst.y - origin.y;
    XformPoint(&dx, &dy);
    SDL_FRect d = { dx, dy, XformLen(dst.width), XformLen(dst.height) };

    if (rotation == 0.0f) {
        SDL_RenderTexture(g_renderer, st, &s, &d);
    } else {
        SDL_FPoint center = { XformLen(origin.x), XformLen(origin.y) };
        SDL_RenderTextureRotated(g_renderer, st, &s, &d, rotation, &center, SDL_FLIP_NONE);
    }
}

void GenTextureMipmaps(Texture2D *tex) {
    (void)tex;  // SDL3 doesn't expose mipmap generation through SDL_Renderer
                // — linear filtering is enough for the UI text use case.
}

void SetTextureFilter(Texture2D tex, int filter) {
    SDL_Texture *st = (SDL_Texture*)tex._sdl;
    if (!st) return;
    SDL_SetTextureScaleMode(st,
        filter == TEXTURE_FILTER_POINT ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

Image GenImageColor(int w, int h, Color c) {
    Image img = {0};
    img.width = w;
    img.height = h;
    img.mipmaps = 1;
    unsigned char *buf = (unsigned char*)malloc((size_t)w * h * 4);
    if (!buf) return img;
    for (int i = 0; i < w * h; i++) {
        buf[i*4 + 0] = c.r;
        buf[i*4 + 1] = c.g;
        buf[i*4 + 2] = c.b;
        buf[i*4 + 3] = c.a;
    }
    img.data = buf;
    return img;
}

void ImageDrawPixel(Image *img, int x, int y, Color c) {
    if (!img || !img->data) return;
    if (x < 0 || y < 0 || x >= img->width || y >= img->height) return;
    unsigned char *p = (unsigned char*)img->data + (y * img->width + x) * 4;
    p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a;
}

void UnloadImage(Image img) {
    if (img.data) free(img.data);
}

// ---------------------------------------------------------------------------
// Fonts / text
// ---------------------------------------------------------------------------

Font LoadFontEx(const char *path, int baseSize, int *codepoints, int codepointCount) {
    (void)codepoints; (void)codepointCount;
    Font f = {0};
    TTF_Font *tf = TTF_OpenFont(path, (float)baseSize);
    if (!tf) {
        fprintf(stderr, "LoadFontEx(%s) failed: %s\n", path, SDL_GetError());
        return f;
    }
    f.baseSize = baseSize;
    f._ttf = tf;
    // Populate texture stub so any caller that pokes font.texture fields
    // gets sane zeros instead of garbage.
    f.texture.id = g_next_tex_id++;
    return f;
}

void UnloadFont(Font font) {
    if (font._ttf) TTF_CloseFont((TTF_Font*)font._ttf);
}

// Render a single line (no embedded newlines). Caller handles the line-by-line
// layout — SDL3_ttf otherwise renders '\n' as a .notdef glyph (visible squares).
static void DrawTextLineSDL(TTF_Font *tf, const char *line, size_t length,
                            float screenX, float screenY, SDL_Color c) {
    if (length == 0) return;
    SDL_Surface *surf = TTF_RenderText_Blended(tf, line, length, c);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    if (tex) {
        SDL_FRect dst = { screenX, screenY, (float)surf->w, (float)surf->h };
        SDL_RenderTexture(g_renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_DestroySurface(surf);
}

void DrawTextEx(Font font, const char *text, Vector2 position,
                float fontSize, float spacing, Color tint) {
    (void)spacing;
    TTF_Font *tf = (TTF_Font*)font._ttf;
    if (!tf || !text || !*text) return;
    // Inside BeginMode2D, scale font size by zoom and place at transformed origin.
    float renderSize = XformLen(fontSize);
    if (renderSize < 1.0f) return;
    TTF_SetFontSize(tf, renderSize);
    SDL_Color c = { tint.r, tint.g, tint.b, tint.a };
    float px = position.x, py = position.y;
    XformPoint(&px, &py);

    // Walk the string, drawing each \n-delimited line at an incrementing Y.
    // TTF_GetFontLineSkip gives the recommended vertical advance.
    int lineSkip = TTF_GetFontLineSkip(tf);
    if (lineSkip <= 0) lineSkip = (int)renderSize;
    const char *seg = text;
    float y = py;
    for (;;) {
        const char *nl = strchr(seg, '\n');
        size_t segLen = nl ? (size_t)(nl - seg) : strlen(seg);
        DrawTextLineSDL(tf, seg, segLen, px, y, c);
        if (!nl) break;
        seg = nl + 1;
        y += (float)lineSkip;
    }
}

Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing) {
    (void)spacing;
    Vector2 v = {0, 0};
    TTF_Font *tf = (TTF_Font*)font._ttf;
    if (!tf || !text) return v;
    TTF_SetFontSize(tf, fontSize);
    // Per-line max width + cumulative height. Skip newline characters so they
    // don't get measured as glyphs.
    int lineSkip = TTF_GetFontLineSkip(tf);
    if (lineSkip <= 0) lineSkip = (int)fontSize;
    const char *seg = text;
    int maxW = 0;
    int lines = 0;
    for (;;) {
        const char *nl = strchr(seg, '\n');
        size_t segLen = nl ? (size_t)(nl - seg) : strlen(seg);
        int w = 0, h = 0;
        if (segLen > 0) TTF_GetStringSize(tf, seg, segLen, &w, &h);
        if (w > maxW) maxW = w;
        lines++;
        if (!nl) break;
        seg = nl + 1;
    }
    v.x = (float)maxW;
    v.y = (float)(lines * lineSkip);
    return v;
}

// Default-font helpers. These route through the game's global `font`, which
// is defined in main.c and loaded via LoadFontEx — there is no separate
// "raylib default font" in our build.
extern Font font;

void DrawText(const char *text, int posX, int posY, int fontSize, Color color) {
    Vector2 pos = { (float)posX, (float)posY };
    DrawTextEx(font, text, pos, (float)fontSize, 0.0f, color);
}

int MeasureText(const char *text, int fontSize) {
    Vector2 v = MeasureTextEx(font, text, (float)fontSize, 0.0f);
    return (int)v.x;
}

// Returns a substring view into a static buffer. Mirrors raylib's
// TextSubtext semantics — caller must use the result before the next call.
const char *TextSubtext(const char *text, int position, int length) {
    static char buf[1024];
    if (!text) { buf[0] = '\0'; return buf; }
    int total = (int)strlen(text);
    if (position < 0) position = 0;
    if (position >= total) { buf[0] = '\0'; return buf; }
    if (length < 0) length = 0;
    if (position + length > total) length = total - position;
    if (length >= (int)sizeof(buf)) length = (int)sizeof(buf) - 1;
    memcpy(buf, text + position, length);
    buf[length] = '\0';
    return buf;
}

// ---------------------------------------------------------------------------
// Audio (stub)
// ---------------------------------------------------------------------------

void  InitAudioDevice(void) { /* deferred — wire SDL_AudioStream + mixer later */ }
void  CloseAudioDevice(void) {}
Sound LoadSound(const char *path) { (void)path; Sound s = {0}; return s; }
void  UnloadSound(Sound s) { (void)s; }
void  PlaySound(Sound s) { (void)s; }

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

bool IsKeyPressed(int key) {
    SDL_Scancode sc = RlKeyToScancode(key);
    if (sc == SDL_SCANCODE_UNKNOWN) return false;
    return g_key_cur[sc] && !g_key_prev[sc];
}

bool IsKeyDown(int key) {
    SDL_Scancode sc = RlKeyToScancode(key);
    if (sc == SDL_SCANCODE_UNKNOWN) return false;
    return g_key_cur[sc];
}

bool IsMouseButtonPressed(int button) {
    if (button < 0 || button > 2) return false;
    return g_mouse_cur[button] && !g_mouse_prev[button];
}

bool IsMouseButtonDown(int button) {
    if (button < 0 || button > 2) return false;
    return g_mouse_cur[button];
}

Vector2 GetMousePosition(void) {
    Vector2 v = { g_mouse_x, g_mouse_y };
    return v;
}

// ---------------------------------------------------------------------------
// Collision
// ---------------------------------------------------------------------------

bool CheckCollisionPointRec(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width &&
           p.y >= r.y && p.y <= r.y + r.height;
}

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------

Color Fade(Color c, float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    Color out = c;
    out.a = (unsigned char)((float)c.a * alpha);
    return out;
}

// ---------------------------------------------------------------------------
// Extended drawing primitives — added in the broad-coverage pass.
// ---------------------------------------------------------------------------

void DrawPixel(int x, int y, Color c) {
    float fx = (float)x, fy = (float)y;
    XformPoint(&fx, &fy);
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    SDL_RenderPoint(g_renderer, fx, fy);
}

void DrawLine(int sx, int sy, int ex, int ey, Color c) {
    float fsx = (float)sx, fsy = (float)sy, fex = (float)ex, fey = (float)ey;
    XformPoint(&fsx, &fsy);
    XformPoint(&fex, &fey);
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    SDL_RenderLine(g_renderer, fsx, fsy, fex, fey);
}

void DrawRectangleLines(int x, int y, int w, int h, Color c) {
    float fx = (float)x, fy = (float)y;
    XformPoint(&fx, &fy);
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    SDL_FRect r = { fx, fy, XformLen((float)w), XformLen((float)h) };
    SDL_RenderRect(g_renderer, &r);
}

// Triangle-fan / capped-rect builder used by the rounded-rect helpers.
// roundness ∈ [0,1]; segments controls per-corner detail. Inputs are in
// the active drawing space (world inside BeginMode2D, else screen);
// transform is applied once up front so every emitted vertex is in screen
// space — matches DrawRectangleRec / DrawRectangleLinesEx semantics.
static void EmitRoundedRectFilled(Rectangle r, float roundness, int segments, Color c) {
    if (roundness <= 0.0f || segments <= 0) {
        DrawRectangleRec(r, c);
        return;
    }
    if (roundness > 1.0f) roundness = 1.0f;

    // Transform rect origin + dimensions to screen space.
    float rx = r.x, ry = r.y;
    XformPoint(&rx, &ry);
    float rw = XformLen(r.width);
    float rh = XformLen(r.height);
    float radius = roundness * 0.5f * (rw < rh ? rw : rh);
    if (segments < 2) segments = 2;

    SDL_FColor fc = { c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f };
    // Center vertex + 4 corners × (segments+1) vertices each. Build a fan.
    int total = 1 + 4 * (segments + 1);
    SDL_Vertex *v = (SDL_Vertex*)malloc(sizeof(SDL_Vertex) * total);
    int *idx = (int*)malloc(sizeof(int) * (total - 1) * 3);

    float cx = rx + rw * 0.5f;
    float cy = ry + rh * 0.5f;
    v[0].position = (SDL_FPoint){ cx, cy };
    v[0].color = fc;
    v[0].tex_coord = (SDL_FPoint){0,0};

    // Corner centers, in order: TR, BR, BL, TL — sweep angles run continuously.
    const float ccx[4] = { rx + rw - radius, rx + rw - radius, rx + radius,      rx + radius      };
    const float ccy[4] = { ry + radius,      ry + rh - radius, ry + rh - radius, ry + radius      };
    int vi = 1;
    for (int corner = 0; corner < 4; corner++) {
        float a0 = -1.5707963f + (float)corner * 1.5707963f;  // -π/2 + corner·π/2
        for (int s = 0; s <= segments; s++) {
            float a = a0 + (1.5707963f * (float)s / (float)segments);
            v[vi].position = (SDL_FPoint){ ccx[corner] + cosf(a) * radius,
                                            ccy[corner] + sinf(a) * radius };
            v[vi].color = fc;
            v[vi].tex_coord = (SDL_FPoint){0,0};
            vi++;
        }
    }
    for (int i = 1; i < total - 1; i++) {
        idx[(i-1)*3 + 0] = 0;
        idx[(i-1)*3 + 1] = i;
        idx[(i-1)*3 + 2] = i + 1;
    }
    // Close the loop back to the first perimeter vertex.
    idx[(total - 2)*3 + 0] = 0;
    idx[(total - 2)*3 + 1] = total - 1;
    idx[(total - 2)*3 + 2] = 1;
    SDL_RenderGeometry(g_renderer, NULL, v, total, idx, (total - 1) * 3);
    free(v);
    free(idx);
}

void DrawRectangleRounded(Rectangle r, float roundness, int segments, Color c) {
    EmitRoundedRectFilled(r, roundness, segments, c);
}

void DrawRectangleRoundedLinesEx(Rectangle r, float roundness, int segments,
                                 float thickness, Color c) {
    // Approximation: stroke each rounded-corner arc as small line segments and
    // connect the four straight edges. Good enough for UI; not a perfect
    // miter. Falls back to rectangle outline when roundness is 0.
    if (roundness <= 0.0f) {
        DrawRectangleLinesEx(r, thickness, c);
        return;
    }
    if (roundness > 1.0f) roundness = 1.0f;
    float radius = roundness * 0.5f * (r.width < r.height ? r.width : r.height);
    if (segments < 2) segments = 2;

    const float ccx[4] = { r.x + r.width  - radius, r.x + r.width  - radius, r.x + radius,            r.x + radius            };
    const float ccy[4] = { r.y + radius,            r.y + r.height - radius, r.y + r.height - radius, r.y + radius            };

    // Arcs.
    Vector2 arcEnds[4][2];
    for (int corner = 0; corner < 4; corner++) {
        float a0 = -1.5707963f + (float)corner * 1.5707963f;
        Vector2 prev = { ccx[corner] + cosf(a0) * radius, ccy[corner] + sinf(a0) * radius };
        arcEnds[corner][0] = prev;
        for (int s = 1; s <= segments; s++) {
            float a = a0 + (1.5707963f * (float)s / (float)segments);
            Vector2 p = { ccx[corner] + cosf(a) * radius, ccy[corner] + sinf(a) * radius };
            DrawLineEx(prev, p, thickness, c);
            prev = p;
        }
        arcEnds[corner][1] = prev;
    }
    // Straight edges between adjacent arc endpoints.
    DrawLineEx(arcEnds[0][1], arcEnds[1][0], thickness, c); // right
    DrawLineEx(arcEnds[1][1], arcEnds[2][0], thickness, c); // bottom
    DrawLineEx(arcEnds[2][1], arcEnds[3][0], thickness, c); // left
    DrawLineEx(arcEnds[3][1], arcEnds[0][0], thickness, c); // top
}

void DrawRectangleRoundedLines(Rectangle r, float roundness, int segments, Color c) {
    DrawRectangleRoundedLinesEx(r, roundness, segments, 1.0f, c);
}

void DrawCircleV(Vector2 center, float radius, Color c) {
    DrawCircle((int)center.x, (int)center.y, radius, c);
}

void DrawCircleLines(int cx, int cy, float radius, Color c) {
    float fcx = (float)cx, fcy = (float)cy;
    XformPoint(&fcx, &fcy);
    float r = XformLen(radius);
    int segments = (int)(r * 0.6f);
    if (segments < 12) segments = 12;
    if (segments > 64) segments = 64;
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    float prevX = fcx + r, prevY = fcy;
    for (int i = 1; i <= segments; i++) {
        float a = (float)i / (float)segments * 6.28318530718f;
        float x = fcx + cosf(a) * r;
        float y = fcy + sinf(a) * r;
        SDL_RenderLine(g_renderer, prevX, prevY, x, y);
        prevX = x; prevY = y;
    }
}

void DrawEllipse(int cx, int cy, float rx, float ry, Color c) {
    float fcx = (float)cx, fcy = (float)cy;
    XformPoint(&fcx, &fcy);
    float frx = XformLen(rx), fry = XformLen(ry);
    int segments = (int)((frx + fry) * 0.4f);
    if (segments < 16) segments = 16;
    if (segments > 64) segments = 64;
    SDL_FColor fc = { c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f };
    SDL_Vertex *v = (SDL_Vertex*)malloc(sizeof(SDL_Vertex) * (segments + 1));
    int *idx = (int*)malloc(sizeof(int) * segments * 3);
    v[0].position = (SDL_FPoint){ fcx, fcy };
    v[0].color = fc;
    v[0].tex_coord = (SDL_FPoint){0,0};
    for (int i = 0; i < segments; i++) {
        float a = (float)i / (float)segments * 6.28318530718f;
        v[i+1].position = (SDL_FPoint){ fcx + cosf(a) * frx, fcy + sinf(a) * fry };
        v[i+1].color = fc;
        v[i+1].tex_coord = (SDL_FPoint){0,0};
        idx[i*3 + 0] = 0;
        idx[i*3 + 1] = i + 1;
        idx[i*3 + 2] = (i + 1) % segments + 1;
    }
    SDL_RenderGeometry(g_renderer, NULL, v, segments + 1, idx, segments * 3);
    free(v);
    free(idx);
}

void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color c) {
    XformPoint(&v1.x, &v1.y);
    XformPoint(&v2.x, &v2.y);
    XformPoint(&v3.x, &v3.y);
    SDL_FColor fc = { c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f };
    SDL_Vertex v[3] = {
        { { v1.x, v1.y }, fc, {0,0} },
        { { v2.x, v2.y }, fc, {0,0} },
        { { v3.x, v3.y }, fc, {0,0} },
    };
    int idx[3] = { 0, 1, 2 };
    SDL_RenderGeometry(g_renderer, NULL, v, 3, idx, 3);
}

// ---------------------------------------------------------------------------
// Camera2D
// ---------------------------------------------------------------------------

void BeginMode2D(Camera2D camera) {
    g_view_active   = true;
    g_view_offset_x = camera.offset.x;
    g_view_offset_y = camera.offset.y;
    g_view_target_x = camera.target.x;
    g_view_target_y = camera.target.y;
    g_view_zoom     = camera.zoom == 0.0f ? 1.0f : camera.zoom;
}

void EndMode2D(void) {
    g_view_active = false;
    g_view_offset_x = 0.0f; g_view_offset_y = 0.0f;
    g_view_target_x = 0.0f; g_view_target_y = 0.0f;
    g_view_zoom = 1.0f;
}

Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera) {
    float zoom = camera.zoom == 0.0f ? 1.0f : camera.zoom;
    Vector2 r = {
        (position.x - camera.offset.x) / zoom + camera.target.x,
        (position.y - camera.offset.y) / zoom + camera.target.y,
    };
    return r;
}

Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera) {
    float zoom = camera.zoom == 0.0f ? 1.0f : camera.zoom;
    Vector2 r = {
        (position.x - camera.target.x) * zoom + camera.offset.x,
        (position.y - camera.target.y) * zoom + camera.offset.y,
    };
    return r;
}

void DrawFPS(int posX, int posY) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d FPS", g_frame_dt_seconds > 0.0f ? (int)(1.0f / g_frame_dt_seconds) : 0);
    DrawText(buf, posX, posY, 20, GREEN);
}

// ---------------------------------------------------------------------------
// Blend / scissor
// ---------------------------------------------------------------------------

void BeginBlendMode(int mode) {
    SDL_BlendMode bm;
    switch (mode) {
        case BLEND_ADDITIVE:           bm = SDL_BLENDMODE_ADD; break;
        case BLEND_MULTIPLIED:         bm = SDL_BLENDMODE_MUL; break;
        case BLEND_ALPHA_PREMULTIPLY:  bm = SDL_BLENDMODE_BLEND_PREMULTIPLIED; break;
        default:                       bm = SDL_BLENDMODE_BLEND; break;
    }
    SDL_SetRenderDrawBlendMode(g_renderer, bm);
}

void EndBlendMode(void) {
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
}

void BeginScissorMode(int x, int y, int width, int height) {
    SDL_Rect r = { x, y, width, height };
    SDL_SetRenderClipRect(g_renderer, &r);
}

void EndScissorMode(void) {
    SDL_SetRenderClipRect(g_renderer, NULL);
}

// ---------------------------------------------------------------------------
// Audio music stream (stubs — same shape as Sound for now)
// ---------------------------------------------------------------------------

Music LoadMusicStream(const char *path) { (void)path; Music m = {0}; return m; }
void  UnloadMusicStream(Music m) { (void)m; }
void  UpdateMusicStream(Music m) { (void)m; }
void  PlayMusicStream(Music m)   { (void)m; }
void  StopMusicStream(Music m)   { (void)m; }

// ---------------------------------------------------------------------------
// File IO
// ---------------------------------------------------------------------------

unsigned char *LoadFileData(const char *fileName, int *bytesRead) {
    if (bytesRead) *bytesRead = 0;
    FILE *f = fopen(fileName, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }
    unsigned char *buf = (unsigned char*)malloc((size_t)size);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (bytesRead) *bytesRead = (int)got;
    return buf;
}

void UnloadFileData(unsigned char *data) {
    if (data) free(data);
}

bool SaveFileData(const char *fileName, void *data, int bytesToWrite) {
    FILE *f = fopen(fileName, "wb");
    if (!f) return false;
    size_t wrote = fwrite(data, 1, (size_t)bytesToWrite, f);
    fclose(f);
    return (int)wrote == bytesToWrite;
}

bool FileExists(const char *fileName) {
    if (!fileName) return false;
    FILE *f = fopen(fileName, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

// ---------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------

const char *TextFormat(const char *text, ...) {
    // Rotating buffer set so chained calls (e.g. DrawText(TextFormat(...), ...)
    // intermixed with another TextFormat(...) on the same line) don't clobber
    // each other before render. raylib uses 4 buffers by default.
    static char buffers[4][512];
    static int idx = 0;
    char *out = buffers[idx];
    idx = (idx + 1) & 3;
    va_list ap;
    va_start(ap, text);
    vsnprintf(out, sizeof(buffers[0]), text, ap);
    va_end(ap);
    return out;
}

unsigned int TextLength(const char *text) {
    return text ? (unsigned int)strlen(text) : 0u;
}

// ---------------------------------------------------------------------------
// Random
// ---------------------------------------------------------------------------

int GetRandomValue(int min, int max) {
    static bool seeded = false;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = true; }
    if (max < min) { int t = min; min = max; max = t; }
    return min + rand() % (max - min + 1);
}

// ---------------------------------------------------------------------------
// Touch (desktop fallback — mirror mouse so code paths gated on touch
// presence still work when developing on Mac)
// ---------------------------------------------------------------------------

int GetTouchPointCount(void) {
    return g_mouse_cur[MOUSE_BUTTON_LEFT] ? 1 : 0;
}

Vector2 GetTouchPosition(int index) {
    (void)index;
    Vector2 v = { g_mouse_x, g_mouse_y };
    return v;
}

bool IsGestureDetected(unsigned int gesture) {
    // Desktop fallback: a left-mouse this-frame transition counts as a tap.
    // Mobile builds will get a real gesture recognizer once we have device-side testing.
    if (gesture & GESTURE_TAP) {
        return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }
    return false;
}
