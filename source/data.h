/* Модель данных вопросов + загрузка из JSON (romfs:/questions.json). */
#ifndef SI_DATA_H
#define SI_DATA_H

#include <stddef.h>

#define MAX_CATEGORIES 6
#define MAX_QUESTIONS_PER_CATEGORY 6
#define MAX_TEXT 512
#define MAX_NAME 128

typedef struct {
    int value;
    char question[MAX_TEXT];
    char answer[MAX_TEXT];
    int answered; /* 0 = ещё не разыгран, 1 = закрыт */
} Question;

typedef struct {
    char name[MAX_NAME];
    Question questions[MAX_QUESTIONS_PER_CATEGORY];
    int question_count;
} Category;

typedef struct {
    char category[MAX_NAME];
    char question[MAX_TEXT];
    char answer[MAX_TEXT];
} FinalQuestion;

typedef struct {
    char title[MAX_NAME];
    Category categories[MAX_CATEGORIES];
    int category_count;
    FinalQuestion final;
    int has_final;
} GameData;

/* Загружает и разбирает JSON-файл с вопросами. Возвращает 1 при успехе,
 * 0 при ошибке (описание пишется в errbuf). */
int data_load(const char *path, GameData *out, char *errbuf, size_t errbuf_size);

#endif
