#include "input.h"
#include <string.h>

void input_pool_init(ControllerPool *pool) {
    memset(pool, 0, sizeof(*pool));
}

void input_pool_refresh(ControllerPool *pool) {
    int n = SDL_NumJoysticks();
    for (int i = 0; i < n && pool->count < MAX_PLAYERS; i++) {
        if (!SDL_IsGameController(i)) continue;
        SDL_JoystickID jid = SDL_JoystickGetDeviceInstanceID(i);
        int already = 0;
        for (int k = 0; k < pool->count; k++) {
            if (pool->id[k] == jid) { already = 1; break; }
        }
        if (already) continue;
        SDL_GameController *gc = SDL_GameControllerOpen(i);
        if (gc) {
            pool->handle[pool->count] = gc;
            pool->id[pool->count] = jid;
            memset(pool->prev_buttons[pool->count], 0, sizeof(pool->prev_buttons[pool->count]));
            pool->count++;
        }
    }
}

void input_pool_quit(ControllerPool *pool) {
    for (int i = 0; i < pool->count; i++) {
        if (pool->handle[i]) SDL_GameControllerClose(pool->handle[i]);
    }
    memset(pool, 0, sizeof(*pool));
}

int input_pressed(ControllerPool *pool, int idx, SDL_GameControllerButton button) {
    if (idx < 0 || idx >= pool->count) return 0;
    Uint8 cur = SDL_GameControllerGetButton(pool->handle[idx], button);
    Uint8 was = pool->prev_buttons[idx][button];
    pool->prev_buttons[idx][button] = cur;
    return cur && !was;
}
