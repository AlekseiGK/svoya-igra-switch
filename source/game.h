/* Игровая логика "Своей игры": состояния, игроки, счёт, финальный раунд. */
#ifndef SI_GAME_H
#define SI_GAME_H

#include <SDL.h>
#include "data.h"
#include "input.h"

typedef enum {
    STATE_INTRO,
    STATE_JOIN,
    STATE_BOARD,
    STATE_REVEAL_CATEGORY,
    STATE_QUESTION,
    STATE_ANSWERED,
    STATE_FINAL_WAGER,
    STATE_FINAL_QUESTION,
    STATE_FINAL_JUDGE,
    STATE_GAME_OVER
} GameStateId;

typedef struct {
    int controller_idx; /* индекс в ControllerPool */
    int score;
} Player;

typedef struct {
    GameData data;
    ControllerPool pool;

    Player players[MAX_PLAYERS];
    int player_count;

    GameStateId state;

    int picker;                 /* индекс игрока, который сейчас выбирает/судит */
    int cursor_cat, cursor_row; /* курсор на игровом поле */
    int sel_cat, sel_row;       /* выбранный вопрос */

    int buzzed_player;          /* -1, если ещё никто не нажал */
    int locked_out[MAX_PLAYERS];

    int final_order[MAX_PLAYERS];
    int final_order_count;
    int final_index;
    int final_wager_player;     /* кого сейчас спрашиваем про ставку, -1 если этап завершён */
    int final_wager[MAX_PLAYERS];

    char message[256];          /* короткое сообщение в статус-бар (необязательно) */
    int quit;                   /* запрошен выход из приложения */
} Game;

void game_init(Game *g, const GameData *data);
void game_reset_round(Game *g); /* сбрасывает очки и поле, оставляя присоединившихся игроков */
void game_update(Game *g, double dt_seconds);
void game_render(Game *g, SDL_Renderer *renderer, int screen_w, int screen_h);
int game_should_quit(Game *g);

#endif
