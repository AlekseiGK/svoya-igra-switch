/* Обёртка над SDL_GameController для до 8 одновременных контроллеров
 * (на Switch: до 8 запаренных Joy-Con/Pro Controller). */
#ifndef SI_INPUT_H
#define SI_INPUT_H

#include <SDL.h>

#define MAX_PLAYERS 8

typedef struct {
    SDL_GameController *handle[MAX_PLAYERS];
    SDL_JoystickID id[MAX_PLAYERS];
    int count;
    /* Состояние кнопок в предыдущем кадре — для определения "нажали только что". */
    Uint8 prev_buttons[MAX_PLAYERS][SDL_CONTROLLER_BUTTON_MAX];
} ControllerPool;

void input_pool_init(ControllerPool *pool);

/* Открывает вновь обнаруженные контроллеры (вызывать регулярно, особенно
 * на экране присоединения игроков). */
void input_pool_refresh(ControllerPool *pool);

void input_pool_quit(ControllerPool *pool);

/* Было ли только что (в этом кадре) нажатие кнопки контроллером с индексом idx
 * в пуле (переход "не нажато" -> "нажато"). Обновляет внутреннее состояние —
 * вызывать ровно один раз за кадр на каждую пару (idx, button). */
int input_pressed(ControllerPool *pool, int idx, SDL_GameControllerButton button);

#endif
