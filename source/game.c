#include "game.h"
#include "render.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define COL(r,gg,b,a) ((SDL_Color){ (r), (gg), (b), (a) })
static const SDL_Color COLOR_BG        = { 10, 20, 60, 255 };
static const SDL_Color COLOR_CELL      = { 20, 40, 110, 255 };
static const SDL_Color COLOR_CELL_DONE = { 12, 22, 55, 255 };
static const SDL_Color COLOR_CURSOR    = { 255, 205, 40, 255 };
static const SDL_Color COLOR_TEXT      = { 255, 255, 255, 255 };
static const SDL_Color COLOR_GOLD      = { 255, 205, 40, 255 };
static const SDL_Color COLOR_DIM       = { 170, 190, 230, 255 };
static const SDL_Color COLOR_GREEN     = { 90, 220, 120, 255 };
static const SDL_Color COLOR_RED       = { 240, 90, 90, 255 };

static int count_available(const Game *g) {
    int n = 0;
    for (int i = 0; i < g->player_count; i++) {
        if (!g->locked_out[i]) n++;
    }
    return n;
}

static int all_questions_answered(const Game *g) {
    for (int c = 0; c < g->data.category_count; c++) {
        const Category *C = &g->data.categories[c];
        for (int q = 0; q < C->question_count; q++) {
            if (!C->questions[q].answered) return 0;
        }
    }
    return 1;
}

static void clamp_cursor(Game *g) {
    if (g->cursor_cat < 0) g->cursor_cat = 0;
    if (g->cursor_cat >= g->data.category_count) g->cursor_cat = g->data.category_count - 1;
    int rows = g->data.categories[g->cursor_cat].question_count;
    if (g->cursor_row < 0) g->cursor_row = 0;
    if (g->cursor_row >= rows) g->cursor_row = rows - 1;
}

static void begin_final_round(Game *g) {
    g->final_order_count = 0;
    for (int i = 0; i < g->player_count; i++) {
        g->final_wager[i] = 0;
        if (g->players[i].score > 0) {
            g->final_order[g->final_order_count++] = i;
        }
    }
    if (g->final_order_count == 0) {
        g->state = STATE_GAME_OVER;
        return;
    }
    g->final_index = 0;
    g->final_wager_player = g->final_order[0];
    g->state = STATE_FINAL_WAGER;
}

void game_init(Game *g, const GameData *data) {
    memset(g, 0, sizeof(*g));
    g->data = *data;
    input_pool_init(&g->pool);
    g->state = STATE_INTRO;
    g->buzzed_player = -1;
    g->picker = 0;
}

void game_reset_round(Game *g) {
    GameData fresh = g->data;
    for (int c = 0; c < fresh.category_count; c++) {
        for (int q = 0; q < fresh.categories[c].question_count; q++) {
            fresh.categories[c].questions[q].answered = 0;
        }
    }
    g->data = fresh;
    for (int i = 0; i < g->player_count; i++) g->players[i].score = 0;
    g->picker = 0;
    g->cursor_cat = 0;
    g->cursor_row = 0;
    g->buzzed_player = -1;
    memset(g->locked_out, 0, sizeof(g->locked_out));
    g->state = g->player_count > 0 ? STATE_BOARD : STATE_JOIN;
}

int game_should_quit(Game *g) { return g->quit; }

static int player_already_joined(const Game *g, int controller_idx) {
    for (int i = 0; i < g->player_count; i++) {
        if (g->players[i].controller_idx == controller_idx) return 1;
    }
    return 0;
}

static void advance_to_next_picker_after_wrong(Game *g) {
    if (count_available(g) > 0) {
        g->state = STATE_QUESTION;
        g->buzzed_player = -1;
    } else {
        g->data.categories[g->sel_cat].questions[g->sel_row].answered = 1;
        g->state = STATE_BOARD;
    }
}

void game_update(Game *g, double dt_seconds) {
    (void)dt_seconds;
    ControllerPool *pool = &g->pool;

    switch (g->state) {
        case STATE_INTRO: {
            input_pool_refresh(pool);
            for (int i = 0; i < pool->count; i++) {
                if (input_pressed(pool, i, SDL_CONTROLLER_BUTTON_A)) {
                    g->state = STATE_JOIN;
                    break;
                }
            }
            break;
        }

        case STATE_JOIN: {
            input_pool_refresh(pool);
            for (int i = 0; i < pool->count; i++) {
                if (input_pressed(pool, i, SDL_CONTROLLER_BUTTON_A)) {
                    if (!player_already_joined(g, i) && g->player_count < MAX_PLAYERS) {
                        g->players[g->player_count].controller_idx = i;
                        g->players[g->player_count].score = 0;
                        g->player_count++;
                    }
                }
                if (input_pressed(pool, i, SDL_CONTROLLER_BUTTON_START) && g->player_count >= 1) {
                    g->picker = 0;
                    g->cursor_cat = 0;
                    g->cursor_row = 0;
                    g->state = STATE_BOARD;
                }
            }
            break;
        }

        case STATE_BOARD: {
            input_pool_refresh(pool);
            int ci = g->players[g->picker].controller_idx;
            if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
                g->cursor_row--; clamp_cursor(g);
            }
            if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
                g->cursor_row++; clamp_cursor(g);
            }
            if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
                g->cursor_cat--; clamp_cursor(g);
            }
            if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
                g->cursor_cat++; clamp_cursor(g);
            }
            if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_A)) {
                Question *q = &g->data.categories[g->cursor_cat].questions[g->cursor_row];
                if (!q->answered) {
                    g->sel_cat = g->cursor_cat;
                    g->sel_row = g->cursor_row;
                    g->state = STATE_REVEAL_CATEGORY;
                }
            }
            break;
        }

        case STATE_REVEAL_CATEGORY: {
            input_pool_refresh(pool);
            int ci = g->players[g->picker].controller_idx;
            if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_A)) {
                memset(g->locked_out, 0, sizeof(g->locked_out));
                if (g->player_count > 1) g->locked_out[g->picker] = 1;
                g->buzzed_player = -1;
                g->state = STATE_QUESTION;
            }
            break;
        }

        case STATE_QUESTION: {
            input_pool_refresh(pool);
            /* Ведущий (выбравший вопрос) может закрыть его без ответа. */
            int pci = g->players[g->picker].controller_idx;
            if (input_pressed(pool, pci, SDL_CONTROLLER_BUTTON_Y)) {
                g->buzzed_player = -1;
                g->state = STATE_ANSWERED;
                break;
            }
            for (int p = 0; p < g->player_count; p++) {
                if (g->locked_out[p]) continue;
                int ci = g->players[p].controller_idx;
                if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_A)) {
                    g->buzzed_player = p;
                    g->state = STATE_ANSWERED;
                    break;
                }
            }
            break;
        }

        case STATE_ANSWERED: {
            input_pool_refresh(pool);
            int pci = g->players[g->picker].controller_idx;
            Question *q = &g->data.categories[g->sel_cat].questions[g->sel_row];
            if (g->buzzed_player < 0) {
                if (input_pressed(pool, pci, SDL_CONTROLLER_BUTTON_A)) {
                    q->answered = 1;
                    g->state = all_questions_answered(g)
                        ? (g->data.has_final ? STATE_FINAL_WAGER : STATE_GAME_OVER)
                        : STATE_BOARD;
                    if (g->state == STATE_FINAL_WAGER) begin_final_round(g);
                }
            } else {
                if (input_pressed(pool, pci, SDL_CONTROLLER_BUTTON_A)) {
                    g->players[g->buzzed_player].score += q->value;
                    q->answered = 1;
                    g->picker = g->buzzed_player;
                    g->state = all_questions_answered(g)
                        ? (g->data.has_final ? STATE_FINAL_WAGER : STATE_GAME_OVER)
                        : STATE_BOARD;
                    if (g->state == STATE_FINAL_WAGER) begin_final_round(g);
                } else if (input_pressed(pool, pci, SDL_CONTROLLER_BUTTON_B)) {
                    g->players[g->buzzed_player].score -= q->value;
                    g->locked_out[g->buzzed_player] = 1;
                    advance_to_next_picker_after_wrong(g);
                }
            }
            break;
        }

        case STATE_FINAL_WAGER: {
            input_pool_refresh(pool);
            if (g->final_wager_player < 0) break;
            int p = g->final_wager_player;
            int ci = g->players[p].controller_idx;
            int maxw = g->players[p].score > 0 ? g->players[p].score : 0;
            if (maxw == 0) {
                g->final_wager[p] = 0;
            } else {
                if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
                    g->final_wager[p] += 10; if (g->final_wager[p] > maxw) g->final_wager[p] = maxw;
                }
                if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
                    g->final_wager[p] -= 10; if (g->final_wager[p] < 0) g->final_wager[p] = 0;
                }
                if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
                    g->final_wager[p] += 100; if (g->final_wager[p] > maxw) g->final_wager[p] = maxw;
                }
                if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
                    g->final_wager[p] -= 100; if (g->final_wager[p] < 0) g->final_wager[p] = 0;
                }
            }
            if (input_pressed(pool, ci, SDL_CONTROLLER_BUTTON_A)) {
                g->final_index++;
                if (g->final_index >= g->final_order_count) {
                    g->final_wager_player = -1;
                    g->state = STATE_FINAL_QUESTION;
                } else {
                    g->final_wager_player = g->final_order[g->final_index];
                }
            }
            break;
        }

        case STATE_FINAL_QUESTION: {
            input_pool_refresh(pool);
            int pci = g->players[g->picker].controller_idx;
            if (input_pressed(pool, pci, SDL_CONTROLLER_BUTTON_A)) {
                g->final_index = 0;
                g->state = STATE_FINAL_JUDGE;
            }
            break;
        }

        case STATE_FINAL_JUDGE: {
            input_pool_refresh(pool);
            int pci = g->players[g->picker].controller_idx;
            if (g->final_index >= g->final_order_count) {
                g->state = STATE_GAME_OVER;
                break;
            }
            int p = g->final_order[g->final_index];
            if (input_pressed(pool, pci, SDL_CONTROLLER_BUTTON_A)) {
                g->players[p].score += g->final_wager[p];
                g->final_index++;
            } else if (input_pressed(pool, pci, SDL_CONTROLLER_BUTTON_B)) {
                g->players[p].score -= g->final_wager[p];
                g->final_index++;
            }
            if (g->final_index >= g->final_order_count) {
                g->state = STATE_GAME_OVER;
            }
            break;
        }

        case STATE_GAME_OVER: {
            input_pool_refresh(pool);
            for (int i = 0; i < pool->count; i++) {
                if (input_pressed(pool, i, SDL_CONTROLLER_BUTTON_A)) {
                    game_reset_round(g);
                    break;
                }
                if (input_pressed(pool, i, SDL_CONTROLLER_BUTTON_B)) {
                    g->player_count = 0;
                    g->state = STATE_JOIN;
                    break;
                }
            }
            break;
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Отрисовка                                                              */
/* ---------------------------------------------------------------------- */

static void draw_scoreboard(Game *g, SDL_Renderer *r, int screen_w, int screen_h) {
    int bar_h = 90;
    int y = screen_h - bar_h;
    fill_rect(r, 0, y, screen_w, bar_h, COL(6, 12, 36, 255));
    if (g->player_count == 0) return;
    int cell_w = screen_w / g->player_count;
    for (int i = 0; i < g->player_count; i++) {
        int x = i * cell_w;
        SDL_Color box = (i == g->picker) ? COLOR_GOLD : COL(30, 50, 100, 255);
        draw_rect_outline(r, x + 6, y + 6, cell_w - 12, bar_h - 12, box, 3);
        char label[64];
        snprintf(label, sizeof(label), "Игрок %d", i + 1);
        draw_text_centered(r, render_font(FONT_SMALL, 1), x + cell_w / 2, y + 26, label, COLOR_TEXT);
        char score[32];
        snprintf(score, sizeof(score), "%d", g->players[i].score);
        SDL_Color scol = g->players[i].score < 0 ? COLOR_RED : COLOR_GOLD;
        draw_text_centered(r, render_font(FONT_MEDIUM, 1), x + cell_w / 2, y + 60, score, scol);
    }
}

static void render_board(Game *g, SDL_Renderer *r, int screen_w, int screen_h) {
    int top = 30;
    int bottom_margin = 100;
    int cols = g->data.category_count;
    int col_w = screen_w / cols;
    int header_h = 90;

    for (int c = 0; c < cols; c++) {
        int x = c * col_w;
        fill_rect(r, x + 4, top, col_w - 8, header_h, COL(30, 55, 130, 255));
        SDL_Rect box = { x + 10, top + 10, col_w - 20, header_h - 20 };
        draw_text_wrapped(r, render_font(FONT_SMALL, 1), box, g->data.categories[c].name, COLOR_TEXT);
    }

    int rows = 0;
    for (int c = 0; c < cols; c++) {
        if (g->data.categories[c].question_count > rows) rows = g->data.categories[c].question_count;
    }
    int grid_top = top + header_h + 6;
    int grid_h = screen_h - bottom_margin - grid_top;
    int row_h = rows > 0 ? grid_h / rows : 0;

    for (int c = 0; c < cols; c++) {
        Category *C = &g->data.categories[c];
        for (int qi = 0; qi < C->question_count; qi++) {
            int x = c * col_w;
            int y = grid_top + qi * row_h;
            Question *Q = &C->questions[qi];
            SDL_Color cell = Q->answered ? COLOR_CELL_DONE : COLOR_CELL;
            fill_rect(r, x + 4, y + 4, col_w - 8, row_h - 8, cell);
            if (!Q->answered) {
                char val[16];
                snprintf(val, sizeof(val), "%d", Q->value);
                draw_text_centered(r, render_font(FONT_LARGE, 1), x + col_w / 2, y + row_h / 2, val, COLOR_GOLD);
            }
            if (g->state == STATE_BOARD && c == g->cursor_cat && qi == g->cursor_row) {
                draw_rect_outline(r, x + 4, y + 4, col_w - 8, row_h - 8, COLOR_CURSOR, 5);
            }
        }
    }
}

void game_render(Game *g, SDL_Renderer *r, int screen_w, int screen_h) {
    fill_rect(r, 0, 0, screen_w, screen_h, COLOR_BG);

    switch (g->state) {
        case STATE_INTRO: {
            draw_text_centered(r, render_font(FONT_XLARGE, 1), screen_w / 2, screen_h / 2 - 60,
                                g->data.title, COLOR_GOLD);
            draw_text_centered(r, render_font(FONT_MEDIUM, 0), screen_w / 2, screen_h / 2 + 30,
                                "Нажмите A на любом контроллере, чтобы начать", COLOR_DIM);
            break;
        }

        case STATE_JOIN: {
            draw_text_centered(r, render_font(FONT_LARGE, 1), screen_w / 2, 80,
                                "Присоединение игроков", COLOR_GOLD);
            draw_text_centered(r, render_font(FONT_SMALL, 0), screen_w / 2, 140,
                                "Каждый жмёт A на своём контроллере. Когда все готовы — START (+) у любого игрока.",
                                COLOR_DIM);
            for (int i = 0; i < g->player_count; i++) {
                char label[32];
                snprintf(label, sizeof(label), "Игрок %d готов", i + 1);
                draw_text_centered(r, render_font(FONT_MEDIUM, 1), screen_w / 2, 220 + i * 50, label, COLOR_GREEN);
            }
            break;
        }

        case STATE_BOARD: {
            render_board(g, r, screen_w, screen_h);
            draw_scoreboard(g, r, screen_w, screen_h);
            break;
        }

        case STATE_REVEAL_CATEGORY: {
            Category *C = &g->data.categories[g->sel_cat];
            Question *Q = &C->questions[g->sel_row];
            draw_text_centered(r, render_font(FONT_LARGE, 1), screen_w / 2, screen_h / 2 - 60, C->name, COLOR_GOLD);
            char val[16];
            snprintf(val, sizeof(val), "%d", Q->value);
            draw_text_centered(r, render_font(FONT_XLARGE, 1), screen_w / 2, screen_h / 2 + 30, val, COLOR_TEXT);
            draw_scoreboard(g, r, screen_w, screen_h);
            break;
        }

        case STATE_QUESTION: {
            Question *Q = &g->data.categories[g->sel_cat].questions[g->sel_row];
            SDL_Rect box = { screen_w / 8, screen_h / 2 - 220, screen_w * 3 / 4, 320 };
            draw_text_wrapped(r, render_font(FONT_LARGE, 1), box, Q->question, COLOR_TEXT);
            draw_text_centered(r, render_font(FONT_SMALL, 0), screen_w / 2, screen_h - 130,
                                "Кто знает — жмите A на своём контроллере!", COLOR_DIM);
            draw_scoreboard(g, r, screen_w, screen_h);
            break;
        }

        case STATE_ANSWERED: {
            Question *Q = &g->data.categories[g->sel_cat].questions[g->sel_row];
            SDL_Rect qbox = { screen_w / 8, 60, screen_w * 3 / 4, 160 };
            draw_text_wrapped(r, render_font(FONT_MEDIUM, 0), qbox, Q->question, COLOR_DIM);
            SDL_Rect abox = { screen_w / 8, 260, screen_w * 3 / 4, 200 };
            draw_text_wrapped(r, render_font(FONT_LARGE, 1), abox, Q->answer, COLOR_GOLD);

            if (g->buzzed_player < 0) {
                draw_text_centered(r, render_font(FONT_MEDIUM, 1), screen_w / 2, screen_h - 150,
                                    "Никто не ответил. Ведущий: продолжить (A)", COLOR_DIM);
            } else {
                char msg[64];
                snprintf(msg, sizeof(msg), "Отвечает игрок %d", g->buzzed_player + 1);
                draw_text_centered(r, render_font(FONT_MEDIUM, 1), screen_w / 2, screen_h - 190, msg, COLOR_TEXT);
                draw_text_centered(r, render_font(FONT_MEDIUM, 1), screen_w / 2, screen_h - 140,
                                    "Ведущий: верно (A) / неверно (B)", COLOR_DIM);
            }
            draw_scoreboard(g, r, screen_w, screen_h);
            break;
        }

        case STATE_FINAL_WAGER: {
            draw_text_centered(r, render_font(FONT_LARGE, 1), screen_w / 2, 100, "Финальный раунд — ставки", COLOR_GOLD);
            draw_text_centered(r, render_font(FONT_MEDIUM, 0), screen_w / 2, 160, g->data.final.category, COLOR_DIM);
            if (g->final_wager_player >= 0) {
                int p = g->final_wager_player;
                char msg[64];
                snprintf(msg, sizeof(msg), "Игрок %d, ваш счёт: %d", p + 1, g->players[p].score);
                draw_text_centered(r, render_font(FONT_MEDIUM, 1), screen_w / 2, screen_h / 2 - 60, msg, COLOR_TEXT);
                char wager[64];
                snprintf(wager, sizeof(wager), "Ставка: %d", g->final_wager[p]);
                draw_text_centered(r, render_font(FONT_XLARGE, 1), screen_w / 2, screen_h / 2 + 20, wager, COLOR_GOLD);
                draw_text_centered(r, render_font(FONT_SMALL, 0), screen_w / 2, screen_h / 2 + 90,
                                    "Вверх/вниз ±10, влево/вправо ±100, A — подтвердить", COLOR_DIM);
            }
            draw_scoreboard(g, r, screen_w, screen_h);
            break;
        }

        case STATE_FINAL_QUESTION: {
            draw_text_centered(r, render_font(FONT_LARGE, 1), screen_w / 2, 80, g->data.final.category, COLOR_GOLD);
            SDL_Rect box = { screen_w / 8, screen_h / 2 - 160, screen_w * 3 / 4, 300 };
            draw_text_wrapped(r, render_font(FONT_LARGE, 1), box, g->data.final.question, COLOR_TEXT);
            draw_text_centered(r, render_font(FONT_SMALL, 0), screen_w / 2, screen_h - 130,
                                "Игроки отвечают на бумаге/вслух. Ведущий: показать ответ (A)", COLOR_DIM);
            draw_scoreboard(g, r, screen_w, screen_h);
            break;
        }

        case STATE_FINAL_JUDGE: {
            draw_text_centered(r, render_font(FONT_MEDIUM, 1), screen_w / 2, 80, "Ответ:", COLOR_DIM);
            SDL_Rect abox = { screen_w / 8, 120, screen_w * 3 / 4, 160 };
            draw_text_wrapped(r, render_font(FONT_LARGE, 1), abox, g->data.final.answer, COLOR_GOLD);
            if (g->final_index < g->final_order_count) {
                int p = g->final_order[g->final_index];
                char msg[96];
                snprintf(msg, sizeof(msg), "Игрок %d поставил %d", p + 1, g->final_wager[p]);
                draw_text_centered(r, render_font(FONT_MEDIUM, 1), screen_w / 2, screen_h / 2 + 60, msg, COLOR_TEXT);
                draw_text_centered(r, render_font(FONT_MEDIUM, 1), screen_w / 2, screen_h / 2 + 120,
                                    "Ведущий: верно (A) / неверно (B)", COLOR_DIM);
            }
            draw_scoreboard(g, r, screen_w, screen_h);
            break;
        }

        case STATE_GAME_OVER: {
            draw_text_centered(r, render_font(FONT_XLARGE, 1), screen_w / 2, 100, "Итоги игры", COLOR_GOLD);
            /* Простая сортировка пузырьком по убыванию очков (игроков мало, до 8). */
            int order[MAX_PLAYERS];
            for (int i = 0; i < g->player_count; i++) order[i] = i;
            for (int i = 0; i < g->player_count; i++) {
                for (int j = i + 1; j < g->player_count; j++) {
                    if (g->players[order[j]].score > g->players[order[i]].score) {
                        int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
                    }
                }
            }
            for (int i = 0; i < g->player_count; i++) {
                int p = order[i];
                char line[64];
                snprintf(line, sizeof(line), "%d. Игрок %d — %d", i + 1, p + 1, g->players[p].score);
                SDL_Color c = (i == 0) ? COLOR_GOLD : COLOR_TEXT;
                draw_text_centered(r, render_font(FONT_LARGE, i == 0), screen_w / 2, 200 + i * 60, line, c);
            }
            draw_text_centered(r, render_font(FONT_SMALL, 0), screen_w / 2, screen_h - 80,
                                "A — сыграть ещё раз с этими вопросами / B — новый набор игроков", COLOR_DIM);
            break;
        }
    }
}
