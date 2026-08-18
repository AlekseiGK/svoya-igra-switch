#include "data.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_str(char *dst, size_t dstsz, const char *src) {
    if (!src) { dst[0] = '\0'; return; }
    snprintf(dst, dstsz, "%s", src);
}

int data_load(const char *path, GameData *out, char *errbuf, size_t errbuf_size) {
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "rb");
    if (!f) {
        if (errbuf) snprintf(errbuf, errbuf_size, "не удалось открыть файл: %s", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        if (errbuf) snprintf(errbuf, errbuf_size, "файл вопросов пуст: %s", path);
        return 0;
    }
    char *buf = (char *)malloc((size_t)size + 1);
    size_t rd = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[rd] = '\0';

    char jerr[256];
    JsonValue *root = json_parse(buf, jerr, sizeof(jerr));
    free(buf);
    if (!root) {
        if (errbuf) snprintf(errbuf, errbuf_size, "ошибка разбора JSON: %s", jerr);
        return 0;
    }

    copy_str(out->title, sizeof(out->title),
             json_as_string(json_object_get(root, "title"), "Своя игра"));

    JsonValue *cats = json_object_get(root, "categories");
    int cat_count = json_array_count(cats);
    if (cat_count > MAX_CATEGORIES) cat_count = MAX_CATEGORIES;

    for (int ci = 0; ci < cat_count; ci++) {
        JsonValue *cat = json_array_get(cats, ci);
        Category *C = &out->categories[out->category_count];
        copy_str(C->name, sizeof(C->name),
                 json_as_string(json_object_get(cat, "name"), "Категория"));

        JsonValue *qs = json_object_get(cat, "questions");
        int qcount = json_array_count(qs);
        if (qcount > MAX_QUESTIONS_PER_CATEGORY) qcount = MAX_QUESTIONS_PER_CATEGORY;

        for (int qi = 0; qi < qcount; qi++) {
            JsonValue *q = json_array_get(qs, qi);
            Question *Q = &C->questions[C->question_count];
            Q->value = (int)json_as_number(json_object_get(q, "value"), 100);
            copy_str(Q->question, sizeof(Q->question),
                     json_as_string(json_object_get(q, "question"), "(вопрос не задан)"));
            copy_str(Q->answer, sizeof(Q->answer),
                     json_as_string(json_object_get(q, "answer"), "(ответ не задан)"));
            Q->answered = 0;
            C->question_count++;
        }
        out->category_count++;
    }

    JsonValue *final = json_object_get(root, "final_round");
    if (final) {
        out->has_final = 1;
        copy_str(out->final.category, sizeof(out->final.category),
                 json_as_string(json_object_get(final, "category"), "Финал"));
        copy_str(out->final.question, sizeof(out->final.question),
                 json_as_string(json_object_get(final, "question"), ""));
        copy_str(out->final.answer, sizeof(out->final.answer),
                 json_as_string(json_object_get(final, "answer"), ""));
    }

    json_free(root);

    if (out->category_count == 0) {
        if (errbuf) snprintf(errbuf, errbuf_size, "в JSON нет ни одной категории");
        return 0;
    }
    return 1;
}
