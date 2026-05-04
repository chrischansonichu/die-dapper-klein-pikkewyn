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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        case KEY_BACKSPACE:    return SDL_SCANCODE_BACKSPACE;
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
        case KEY_LEFT_SHIFT:   return SDL_SCANCODE_LSHIFT;
        case KEY_LEFT_CONTROL: return SDL_SCANCODE_LCTRL;
        case KEY_LEFT_ALT:     return SDL_SCANCODE_LALT;
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
        g_last_frame_ns = SDL_GetTicksNS();
    }
}

void ClearBackground(Color c) {
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    SDL_RenderClear(g_renderer);
}

// ---------------------------------------------------------------------------
// Shapes
// ---------------------------------------------------------------------------

void DrawRectangle(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    SDL_FRect r = { (float)x, (float)y, (float)w, (float)h };
    SDL_RenderFillRect(g_renderer, &r);
}

void DrawRectangleRec(Rectangle r, Color c) {
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    SDL_FRect fr = { r.x, r.y, r.width, r.height };
    SDL_RenderFillRect(g_renderer, &fr);
}

void DrawRectangleLinesEx(Rectangle r, float thickness, Color c) {
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
    // Four filled rectangles forming the border. SDL_RenderRect would only
    // give us 1-pixel outlines.
    SDL_FRect top    = { r.x,                       r.y,                       r.width,    thickness };
    SDL_FRect bottom = { r.x,                       r.y + r.height - thickness, r.width,    thickness };
    SDL_FRect left   = { r.x,                       r.y,                       thickness,  r.height };
    SDL_FRect right  = { r.x + r.width - thickness, r.y,                       thickness,  r.height };
    SDL_RenderFillRect(g_renderer, &top);
    SDL_RenderFillRect(g_renderer, &bottom);
    SDL_RenderFillRect(g_renderer, &left);
    SDL_RenderFillRect(g_renderer, &right);
}

void DrawRectangleGradientV(int x, int y, int w, int h, Color top, Color bottom) {
    // Vertex-colored quad via SDL_RenderGeometry.
    SDL_FColor c_top    = { top.r/255.0f,    top.g/255.0f,    top.b/255.0f,    top.a/255.0f };
    SDL_FColor c_bottom = { bottom.r/255.0f, bottom.g/255.0f, bottom.b/255.0f, bottom.a/255.0f };
    SDL_Vertex v[4] = {
        { { (float)x,       (float)y       }, c_top,    {0,0} },  // tl
        { { (float)(x + w), (float)y       }, c_top,    {0,0} },  // tr
        { { (float)(x + w), (float)(y + h) }, c_bottom, {0,0} },  // br
        { { (float)x,       (float)(y + h) }, c_bottom, {0,0} },  // bl
    };
    int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(g_renderer, NULL, v, 4, idx, 6);
}

void DrawCircle(int cx, int cy, float radius, Color c) {
    // Triangle fan via SDL_RenderGeometry. Segment count scales with radius.
    int segments = (int)(radius * 0.6f); if (segments < 12) segments = 12; if (segments > 64) segments = 64;
    SDL_FColor fc = { c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f };
    SDL_Vertex *verts = (SDL_Vertex*)malloc(sizeof(SDL_Vertex) * (segments + 1));
    int *idx = (int*)malloc(sizeof(int) * segments * 3);
    verts[0].position = (SDL_FPoint){ (float)cx, (float)cy };
    verts[0].color = fc;
    verts[0].tex_coord = (SDL_FPoint){0,0};
    for (int i = 0; i < segments; i++) {
        float a = (float)i / (float)segments * 6.28318530718f;
        verts[i + 1].position = (SDL_FPoint){ cx + cosf(a) * radius, cy + sinf(a) * radius };
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
    // Thick line as a rotated quad via SDL_RenderGeometry. SDL_RenderLine is
    // 1px-only; raylib's DrawLineEx supports arbitrary widths.
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.0001f) return;
    float nx = -dy / len * thickness * 0.5f;
    float ny =  dx / len * thickness * 0.5f;
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
    SDL_FRect d = { dst.x - origin.x, dst.y - origin.y, dst.width, dst.height };

    if (rotation == 0.0f) {
        SDL_RenderTexture(g_renderer, st, &s, &d);
    } else {
        SDL_FPoint center = { origin.x, origin.y };
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

void DrawTextEx(Font font, const char *text, Vector2 position,
                float fontSize, float spacing, Color tint) {
    (void)spacing;
    TTF_Font *tf = (TTF_Font*)font._ttf;
    if (!tf || !text || !*text) return;
    TTF_SetFontSize(tf, fontSize);
    SDL_Color c = { tint.r, tint.g, tint.b, tint.a };
    SDL_Surface *surf = TTF_RenderText_Blended(tf, text, 0, c);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    if (tex) {
        SDL_FRect dst = { position.x, position.y, (float)surf->w, (float)surf->h };
        SDL_RenderTexture(g_renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_DestroySurface(surf);
}

Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing) {
    (void)spacing;
    Vector2 v = {0, 0};
    TTF_Font *tf = (TTF_Font*)font._ttf;
    if (!tf || !text) return v;
    TTF_SetFontSize(tf, fontSize);
    int w = 0, h = 0;
    TTF_GetStringSize(tf, text, 0, &w, &h);
    v.x = (float)w;
    v.y = (float)h;
    return v;
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
