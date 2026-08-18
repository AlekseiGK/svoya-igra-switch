/* Минимальный JSON-парсер только для чтения (без вендоринга сторонних либ).
 * Поддерживает объекты, массивы, строки (с escape-последовательностями,
 * включая \uXXXX), числа, true/false/null.
 */
#ifndef SI_JSON_H
#define SI_JSON_H

#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue {
    JsonType type;
    union {
        int boolean;
        double number;
        char *string;
        struct {
            struct JsonValue **items;
            int count;
        } array;
        struct {
            char **keys;
            struct JsonValue **values;
            int count;
        } object;
    } as;
} JsonValue;

/* Разбирает JSON-текст. При ошибке возвращает NULL и пишет описание в errbuf. */
JsonValue *json_parse(const char *text, char *errbuf, size_t errbuf_size);

/* Освобождает значение и всех его потомков. */
void json_free(JsonValue *v);

/* Возвращает значение по ключу в объекте или NULL, если ключа нет / v не объект. */
JsonValue *json_object_get(const JsonValue *v, const char *key);

/* Число элементов массива (0, если v не массив или NULL). */
int json_array_count(const JsonValue *v);

/* Элемент массива по индексу (NULL, если вне диапазона). */
JsonValue *json_array_get(const JsonValue *v, int index);

/* Достаёт число/строку с значением по умолчанию, если поле отсутствует
 * или имеет не тот тип. */
double json_as_number(const JsonValue *v, double def);
const char *json_as_string(const JsonValue *v, const char *def);

#endif
