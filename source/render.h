/* Обёртка над SDL_ttf: загрузка шрифтов фиксированного набора размеров
 * и отрисовка UTF-8 текста (кириллица). */
#ifndef SI_RENDER_H
#define SI_RENDER_H

#include <SDL.h>
#include <SDL_ttf.h>

typedef enum {
    FONT_SMALL = 0,   /* 22px */
    FONT_MEDIUM,      /* 30px */
    FONT_LARGE,       /* 42px */
    FONT_XLARGE,      /* 64px */
    FONT_SIZE_COUNT
} FontSizeId;

int render_fonts_init(const char *regular_path, const char *bold_path);
void render_fonts_quit(void);

TTF_Font *render_font(FontSizeId size, int bold);

/* Рисует текст, левый верхний угол в (x, y). Возвращает ширину/высоту через out (может быть NULL). */
void draw_text(SDL_Renderer *r, TTF_Font *font, int x, int y, const char *utf8, SDL_Color color);

/* Рисует текст, центрированный по точке (cx, cy). */
void draw_text_centered(SDL_Renderer *r, TTF_Font *font, int cx, int cy, const char *utf8, SDL_Color color);

/* Рисует текст с переносом по словам в прямоугольнике box, начиная сверху. */
void draw_text_wrapped(SDL_Renderer *r, TTF_Font *font, SDL_Rect box, const char *utf8, SDL_Color color);

void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color color);
void draw_rect_outline(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color color, int thickness);

#endif
