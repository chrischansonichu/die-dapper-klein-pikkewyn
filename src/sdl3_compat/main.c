// SDL3 hello-world. Proves the toolchain (SDL3 + SDL3_ttf + CMake) is wired
// up before any game code or compat shim is layered on top. Opens a window
// matching the game's logical resolution, fills it with a recognizable
// color, and waits for close/ESC.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>

#define WIN_W 800
#define WIN_H 450

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (!TTF_Init()) {
        fprintf(stderr, "TTF_Init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    if (!SDL_CreateWindowAndRenderer("Die Dapper Klein Pikkewyn (SDL3)",
                                     WIN_W, WIN_H,
                                     SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &window, &renderer)) {
        fprintf(stderr, "CreateWindowAndRenderer failed: %s\n", SDL_GetError());
        TTF_Quit(); SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) running = false;
        }

        SDL_SetRenderDrawColor(renderer, 32, 24, 18, 255);
        SDL_RenderClear(renderer);

        SDL_FRect r = { 100.0f, 100.0f, 200.0f, 100.0f };
        SDL_SetRenderDrawColor(renderer, 220, 180, 90, 255);
        SDL_RenderFillRect(renderer, &r);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
