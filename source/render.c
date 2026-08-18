#include "render.h"
#include <stdio.h>

static const int kSizes[FONT_SIZE_COUNT] = { 22, 30, 42, 64 };
static TTF_Font *g_regular[FONT_SIZE_COUNT];
static TTF_Font *g_bold[FONT_SIZE_COUNT];

int render_fonts_init(const char *regular_path, const char *bold_path) {
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return 0;
    }
    for (int i = 0; i < FONT_SIZE_COUNT; i++) {
        g_regular[i] = TTF_OpenFont(regular_path, kSizes[i]);
        g_bold[i] = TTF_OpenFont(bold_path, kSizes[i]);
        if (!g_regular[i] || !g_bold[i]) {
            fprintf(stderr, "TTF_OpenFont failed for size %d: %s\n", kSizes[i], TTF_GetError());
            return 0;
        }
    }
    return 1;
}

void render_fonts_quit(void) {
    for (int i = 0; i < FONT_SIZE_COUNT; i++) {
        if (g_regular[i]) TTF_CloseFont(g_regular[i]);
        if (g_bold[i]) TTF_CloseFont(g_bold[i]);
    }
    TTF_Quit();
}

TTF_Font *render_font(FontSizeId size, int bold) {
    if (size < 0 || size >= FONT_SIZE_COUNT) size = FONT_MEDIUM;
    return bold ? g_bold[size] : g_regular[size];
}

void draw_text(SDL_Renderer *r, TTF_Font *font, int x, int y, const char *utf8, SDL_Color color) {
    if (!utf8 || !utf8[0]) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, utf8, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void draw_text_centered(SDL_Renderer *r, TTF_Font *font, int cx, int cy, const char *utf8, SDL_Color color) {
    if (!utf8 || !utf8[0]) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, utf8, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { cx - surf->w / 2, cy - surf->h / 2, surf->w, surf->h };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void draw_text_wrapped(SDL_Renderer *r, TTF_Font *font, SDL_Rect box, const char *utf8, SDL_Color color) {
    if (!utf8 || !utf8[0]) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended_Wrapped(font, utf8, color, (Uint32)box.w);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { box.x, box.y, surf->w, surf->h };
    if (dst.w > box.w) dst.w = box.w;
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

void draw_rect_outline(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color color, int thickness) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    for (int t = 0; t < thickness; t++) {
        SDL_Rect rect = { x + t, y + t, w - 2 * t, h - 2 * t };
        SDL_RenderDrawRect(r, &rect);
    }
}
