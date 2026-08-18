/* Своя игра для Nintendo Switch (homebrew, .nro)
 * Собирается либо через devkitPro (libnx + switch-sdl2 + switch-sdl2_ttf) в .nro,
 * либо как обычное SDL2-приложение на ПК/Linux для тестирования логики
 * (см. Makefile: `make` для Switch, `make pc` для десктоп-сборки).
 */
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#ifdef __SWITCH__
#include <switch.h>
#define ASSET_PREFIX "romfs:/"
#else
#define ASSET_PREFIX "romfs/"
#endif

#include "data.h"
#include "game.h"
#include "render.h"

#define SCREEN_W 1280
#define SCREEN_H 720

/* Пытается загрузить вопросы сначала из редактируемого файла на SD-карте
 * (sdmc:/switch/svoya-igra/questions.json на Switch, ./questions.json рядом
 * с исполняемым файлом при тестовой ПК-сборке), а если его нет — из набора,
 * вшитого в саму программу (romfs). Это позволяет менять вопросы, просто
 * отредактировав файл на SD-карте, без пересборки .nro.
 */
static int try_load_questions(GameData *data, char *err, size_t errsz, char *used_path, size_t used_path_sz) {
#ifdef __SWITCH__
    const char *user_path = "sdmc:/switch/svoya-igra/questions.json";
#else
    const char *user_path = "questions.json";
#endif

    /* Если пользователь положил свой файл — используем его. Если в нём
     * ошибка JSON, НЕ откатываемся молча на встроенные вопросы: иначе
     * человек отредактирует файл, опечатается, а игра тихо покажет
     * старые вопросы и он не поймёт, в чём дело. Лучше явно показать
     * ошибку разбора, чтобы файл можно было починить. */
    FILE *probe = fopen(user_path, "rb");
    if (probe) {
        fclose(probe);
        if (data_load(user_path, data, err, errsz)) {
            snprintf(used_path, used_path_sz, "%s", user_path);
            return 1;
        }
        return 0;
    }

    char bundled[256];
    snprintf(bundled, sizeof(bundled), "%squestions.json", ASSET_PREFIX);
    if (data_load(bundled, data, err, errsz)) {
        snprintf(used_path, used_path_sz, "%s", bundled);
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

#ifdef __SWITCH__
    romfsInit();
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Своя игра",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Программный рендерер — рисуем только простые прямоугольники и текст,
     * аппаратное ускорение не нужно, а на некоторых прошивках путь через
     * EGL/GPU-рендерер приводит к падению при старте. */
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
#ifdef __SWITCH__
        romfsExit();
#endif
        return 1;
    }

    char font_regular[256], font_bold[256];
    snprintf(font_regular, sizeof(font_regular), "%sfonts/DejaVuSans.ttf", ASSET_PREFIX);
    snprintf(font_bold, sizeof(font_bold), "%sfonts/DejaVuSans-Bold.ttf", ASSET_PREFIX);

    if (!render_fonts_init(font_regular, font_bold)) {
        fprintf(stderr, "Не удалось загрузить шрифты\n");
        return 1;
    }

    GameData data;
    char err[256];
    char used_path[256];
    if (!try_load_questions(&data, err, sizeof(err), used_path, sizeof(used_path))) {
        fprintf(stderr, "Ошибка загрузки вопросов: %s\n", err);
        /* Показываем ошибку на экране и ждём нажатия A или закрытия окна —
         * чтобы на реальной консоли можно было успеть прочитать сообщение. */
        ControllerPool err_pool;
        input_pool_init(&err_pool);
        int wait = 1;
        while (wait) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) wait = 0;
            }
            input_pool_refresh(&err_pool);
            for (int i = 0; i < err_pool.count; i++) {
                if (input_pressed(&err_pool, i, SDL_CONTROLLER_BUTTON_A)) wait = 0;
            }
            SDL_SetRenderDrawColor(renderer, 40, 10, 10, 255);
            SDL_RenderClear(renderer);
            SDL_Rect box = { 60, SCREEN_H / 2 - 150, SCREEN_W - 120, 300 };
            draw_text_wrapped(renderer, render_font(FONT_MEDIUM, 1), box, err, (SDL_Color){255,255,255,255});
            draw_text_centered(renderer, render_font(FONT_SMALL, 0), SCREEN_W / 2, SCREEN_H - 60,
                                "Проверьте questions.json. Нажмите A, чтобы выйти.", (SDL_Color){220,220,220,255});
            SDL_RenderPresent(renderer);
            SDL_Delay(16);
        }
        input_pool_quit(&err_pool);
        return 1;
    }
    fprintf(stderr, "Вопросы загружены из: %s\n", used_path);

    Game game;
    game_init(&game, &data);

    int running = 1;
    Uint64 prev_ticks = SDL_GetPerformanceCounter();
    const Uint64 freq = SDL_GetPerformanceFrequency();

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
        }

        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - prev_ticks) / (double)freq;
        prev_ticks = now;

        game_update(&game, dt);
        if (game_should_quit(&game)) running = 0;

        game_render(&game, renderer, SCREEN_W, SCREEN_H);
        SDL_RenderPresent(renderer);

        SDL_Delay(16); /* ~60 fps */
    }

    input_pool_quit(&game.pool);
    render_fonts_quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

#ifdef __SWITCH__
    romfsExit();
#endif

    return 0;
}
