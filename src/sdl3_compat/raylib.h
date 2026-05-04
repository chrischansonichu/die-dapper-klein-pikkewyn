// raylib API shim implemented over SDL3 + SDL3_ttf + SDL3_image.
//
// Drop-in replacement for raylib.h on the SDL3 build. The game's screen and
// system code includes "raylib.h" verbatim; we put this directory ahead of
// raylib's own headers on the include path so this file is what they find.
//
// Surface here is intentionally narrow — only what's actually called in the
// game today. Add functions as we extend coverage to more screens.
//
// Types (Color, Vector2, Rectangle, Texture2D, Font, Sound, ...) match
// raylib's field layout where game code touches the fields directly
// (Texture2D.id, .width, .height; Font.texture); private SDL handles live
// in trailing _opaque fields the game code never reads.

#ifndef RAYLIB_SHIM_H
#define RAYLIB_SHIM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// Types
// ----------------------------------------------------------------------------

typedef struct Vector2 { float x; float y; } Vector2;
typedef struct Vector3 { float x, y, z; } Vector3;
typedef struct Vector4 { float x, y, z, w; } Vector4;

typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

typedef struct Rectangle {
    float x;
    float y;
    float width;
    float height;
} Rectangle;

// Field layout up to `format` matches raylib so any code reading
// .id/.width/.height stays correct. _sdl holds the SDL_Texture*; game code
// must not poke at it.
typedef struct Texture2D {
    unsigned int id;       // 0 = unloaded; non-zero = loaded
    int width;
    int height;
    int mipmaps;
    int format;
    void *_sdl;            // SDL_Texture*
} Texture2D;

typedef Texture2D Texture;
typedef Texture2D RenderTexture2D;
typedef Texture2D RenderTexture;

// Image is a CPU-side pixel buffer (used by paper_harbor for the paper grain).
// We mirror raylib's struct shape so existing code compiles; pixel manipulation
// goes through ImageDrawPixel etc. The raw buffer is owned by `data`.
typedef struct Image {
    void *data;            // pixels, RGBA8
    int width;
    int height;
    int mipmaps;
    int format;
} Image;

typedef struct Font {
    int baseSize;
    int glyphCount;
    int glyphPadding;
    Texture2D texture;     // unused by the SDL3 backend; populated stub
    void *_recs;           // unused
    void *_glyphs;         // unused
    void *_ttf;            // TTF_Font*
} Font;

typedef struct Sound {
    unsigned int frameCount;
    unsigned int sampleRate;
    void *_data;           // SDL audio buffer (deferred — currently stub)
} Sound;

typedef struct Music {
    unsigned int frameCount;
    bool looping;
    int ctxType;
    void *_data;
} Music;

typedef struct Camera2D {
    Vector2 offset;
    Vector2 target;
    float rotation;
    float zoom;
} Camera2D;

// ----------------------------------------------------------------------------
// Color constants (raylib palette)
// ----------------------------------------------------------------------------

#define LIGHTGRAY  ((Color){ 200, 200, 200, 255 })
#define GRAY       ((Color){ 130, 130, 130, 255 })
#define DARKGRAY   ((Color){  80,  80,  80, 255 })
#define YELLOW     ((Color){ 253, 249,   0, 255 })
#define GOLD       ((Color){ 255, 203,   0, 255 })
#define ORANGE     ((Color){ 255, 161,   0, 255 })
#define PINK       ((Color){ 255, 109, 194, 255 })
#define RED        ((Color){ 230,  41,  55, 255 })
#define MAROON     ((Color){ 190,  33,  55, 255 })
#define GREEN      ((Color){   0, 228,  48, 255 })
#define LIME       ((Color){   0, 158,  47, 255 })
#define DARKGREEN  ((Color){   0, 117,  44, 255 })
#define SKYBLUE    ((Color){ 102, 191, 255, 255 })
#define BLUE       ((Color){   0, 121, 241, 255 })
#define DARKBLUE   ((Color){   0,  82, 172, 255 })
#define PURPLE     ((Color){ 200, 122, 255, 255 })
#define VIOLET     ((Color){ 135,  60, 190, 255 })
#define DARKPURPLE ((Color){ 112,  31, 126, 255 })
#define BEIGE      ((Color){ 211, 176, 131, 255 })
#define BROWN      ((Color){ 127, 106,  79, 255 })
#define DARKBROWN  ((Color){  76,  63,  47, 255 })
#define WHITE      ((Color){ 255, 255, 255, 255 })
#define BLACK      ((Color){   0,   0,   0, 255 })
#define BLANK      ((Color){   0,   0,   0,   0 })
#define MAGENTA    ((Color){ 255,   0, 255, 255 })
#define RAYWHITE   ((Color){ 245, 245, 245, 255 })

// ----------------------------------------------------------------------------
// Config / texture-filter / key / mouse constants
// ----------------------------------------------------------------------------

// Window flags — values arbitrary; only consumed by SetConfigFlags.
#define FLAG_VSYNC_HINT          0x00000040
#define FLAG_FULLSCREEN_MODE     0x00000002
#define FLAG_WINDOW_RESIZABLE    0x00000004
#define FLAG_WINDOW_HIGHDPI      0x00002000

// Texture filters
#define TEXTURE_FILTER_POINT       0
#define TEXTURE_FILTER_BILINEAR    1
#define TEXTURE_FILTER_TRILINEAR   2

// Mouse buttons (match raylib's numbering, NOT SDL's)
#define MOUSE_BUTTON_LEFT     0
#define MOUSE_BUTTON_RIGHT    1
#define MOUSE_BUTTON_MIDDLE   2
#define MOUSE_LEFT_BUTTON     MOUSE_BUTTON_LEFT
#define MOUSE_RIGHT_BUTTON    MOUSE_BUTTON_RIGHT
#define MOUSE_MIDDLE_BUTTON   MOUSE_BUTTON_MIDDLE

// Keys — values match raylib's KEY_* (mostly ASCII for letters/digits, plus
// raylib-specific codes for arrows/specials). We translate to SDL_Scancode
// inside the input implementation.
#define KEY_NULL            0
#define KEY_APOSTROPHE      39
#define KEY_COMMA           44
#define KEY_MINUS           45
#define KEY_PERIOD          46
#define KEY_SLASH           47
#define KEY_ZERO            48
#define KEY_ONE             49
#define KEY_TWO             50
#define KEY_THREE           51
#define KEY_FOUR            52
#define KEY_FIVE            53
#define KEY_SIX             54
#define KEY_SEVEN           55
#define KEY_EIGHT           56
#define KEY_NINE            57
#define KEY_SEMICOLON       59
#define KEY_EQUAL           61
#define KEY_A               65
#define KEY_B               66
#define KEY_C               67
#define KEY_D               68
#define KEY_E               69
#define KEY_F               70
#define KEY_G               71
#define KEY_H               72
#define KEY_I               73
#define KEY_J               74
#define KEY_K               75
#define KEY_L               76
#define KEY_M               77
#define KEY_N               78
#define KEY_O               79
#define KEY_P               80
#define KEY_Q               81
#define KEY_R               82
#define KEY_S               83
#define KEY_T               84
#define KEY_U               85
#define KEY_V               86
#define KEY_W               87
#define KEY_X               88
#define KEY_Y               89
#define KEY_Z               90
#define KEY_SPACE           32
#define KEY_ESCAPE          256
#define KEY_ENTER           257
#define KEY_TAB             258
#define KEY_BACKSPACE       259
#define KEY_RIGHT           262
#define KEY_LEFT            263
#define KEY_DOWN            264
#define KEY_UP              265
#define KEY_F1              290
#define KEY_F2              291
#define KEY_F3              292
#define KEY_F4              293
#define KEY_F5              294
#define KEY_F6              295
#define KEY_F7              296
#define KEY_F8              297
#define KEY_F9              298
#define KEY_F10             299
#define KEY_LEFT_SHIFT      340
#define KEY_LEFT_CONTROL    341
#define KEY_LEFT_ALT        342

// ----------------------------------------------------------------------------
// Window / lifecycle
// ----------------------------------------------------------------------------

void SetConfigFlags(unsigned int flags);
void InitWindow(int width, int height, const char *title);
void CloseWindow(void);
bool WindowShouldClose(void);
int  GetScreenWidth(void);
int  GetScreenHeight(void);
void SetTargetFPS(int fps);
int  ChangeDirectory(const char *dir);
const char *GetApplicationDirectory(void);

// ----------------------------------------------------------------------------
// Drawing frame
// ----------------------------------------------------------------------------

void BeginDrawing(void);
void EndDrawing(void);
void ClearBackground(Color c);

// ----------------------------------------------------------------------------
// Shapes
// ----------------------------------------------------------------------------

void DrawRectangle(int x, int y, int w, int h, Color c);
void DrawRectangleRec(Rectangle r, Color c);
void DrawRectangleLinesEx(Rectangle r, float thickness, Color c);
void DrawRectangleGradientV(int x, int y, int w, int h, Color top, Color bottom);
void DrawCircle(int cx, int cy, float radius, Color c);
void DrawLineEx(Vector2 a, Vector2 b, float thickness, Color c);

// ----------------------------------------------------------------------------
// Textures
// ----------------------------------------------------------------------------

Texture2D LoadTexture(const char *path);
Texture2D LoadTextureFromImage(Image img);
void      UnloadTexture(Texture2D tex);
void      DrawTexturePro(Texture2D tex, Rectangle src, Rectangle dst,
                         Vector2 origin, float rotation, Color tint);
void      GenTextureMipmaps(Texture2D *tex);
void      SetTextureFilter(Texture2D tex, int filter);

// ----------------------------------------------------------------------------
// Images (CPU-side pixel buffers)
// ----------------------------------------------------------------------------

Image GenImageColor(int w, int h, Color c);
void  ImageDrawPixel(Image *img, int x, int y, Color c);
void  UnloadImage(Image img);

// ----------------------------------------------------------------------------
// Fonts / text
// ----------------------------------------------------------------------------

Font    LoadFontEx(const char *path, int baseSize, int *codepoints, int codepointCount);
void    UnloadFont(Font font);
void    DrawTextEx(Font font, const char *text, Vector2 position,
                   float fontSize, float spacing, Color tint);
Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing);

// Default-font wrappers — route through the global `font` (defined in main.c).
// Some screens (e.g. screen_logo.c) call these directly; screen_layout.h's
// macros redirect them to DrawTextEx for screens that include that header.
void DrawText(const char *text, int posX, int posY, int fontSize, Color color);
int  MeasureText(const char *text, int fontSize);
const char *TextSubtext(const char *text, int position, int length);

// ----------------------------------------------------------------------------
// Audio (stub for now — real impl arrives with mixer wiring)
// ----------------------------------------------------------------------------

void  InitAudioDevice(void);
void  CloseAudioDevice(void);
Sound LoadSound(const char *path);
void  UnloadSound(Sound s);
void  PlaySound(Sound s);

// ----------------------------------------------------------------------------
// Input
// ----------------------------------------------------------------------------

bool    IsKeyPressed(int key);
bool    IsKeyDown(int key);
bool    IsMouseButtonPressed(int button);
bool    IsMouseButtonDown(int button);
Vector2 GetMousePosition(void);

// ----------------------------------------------------------------------------
// Collision
// ----------------------------------------------------------------------------

bool CheckCollisionPointRec(Vector2 p, Rectangle r);

// ----------------------------------------------------------------------------
// Color helpers
// ----------------------------------------------------------------------------

Color Fade(Color c, float alpha);

#ifdef __cplusplus
}
#endif

#endif // RAYLIB_SHIM_H
