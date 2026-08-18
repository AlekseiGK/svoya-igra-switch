#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct {
    const char *start;
    const char *p;
    const char *end;
    char *err;
    size_t errsz;
    int failed;
} Parser;

static void set_error(Parser *ps, const char *msg) {
    if (ps->failed) return;
    ps->failed = 1;
    if (ps->err && ps->errsz) {
        snprintf(ps->err, ps->errsz, "JSON error at offset %ld: %s",
                 (long)(ps->p - ps->start), msg);
    }
}

static void skip_ws(Parser *ps) {
    while (ps->p < ps->end &&
           (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r')) {
        ps->p++;
    }
}

static JsonValue *new_value(JsonType t) {
    JsonValue *v = (JsonValue *)calloc(1, sizeof(JsonValue));
    v->type = t;
    return v;
}

static JsonValue *parse_value(Parser *ps);

/* Кодирует кодовую точку unicode в UTF-8 и дописывает в динамический буфер. */
static void append_utf8(char **buf, size_t *len, size_t *cap, unsigned int cp) {
    char tmp[4];
    int n = 0;
    if (cp <= 0x7F) {
        tmp[0] = (char)cp;
        n = 1;
    } else if (cp <= 0x7FF) {
        tmp[0] = (char)(0xC0 | (cp >> 6));
        tmp[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp <= 0xFFFF) {
        tmp[0] = (char)(0xE0 | (cp >> 12));
        tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        tmp[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else {
        tmp[0] = (char)(0xF0 | (cp >> 18));
        tmp[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        tmp[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        tmp[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }
    if (*len + (size_t)n + 1 > *cap) {
        *cap = (*cap + n + 1) * 2;
        *buf = (char *)realloc(*buf, *cap);
    }
    memcpy(*buf + *len, tmp, n);
    *len += n;
    (*buf)[*len] = '\0';
}

static unsigned int hex4(Parser *ps) {
    unsigned int v = 0;
    for (int i = 0; i < 4; i++) {
        if (ps->p >= ps->end) { set_error(ps, "unexpected end in \\u escape"); return 0; }
        char c = *ps->p++;
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned int)(c - 'A' + 10);
        else { set_error(ps, "invalid hex digit in \\u escape"); return 0; }
    }
    return v;
}

static char *parse_string_raw(Parser *ps) {
    if (ps->p >= ps->end || *ps->p != '"') { set_error(ps, "expected '\"'"); return NULL; }
    ps->p++; /* skip opening quote */

    size_t cap = 16, len = 0;
    char *buf = (char *)malloc(cap);
    buf[0] = '\0';

    while (1) {
        if (ps->p >= ps->end) { set_error(ps, "unterminated string"); free(buf); return NULL; }
        unsigned char c = (unsigned char)*ps->p;
        if (c == '"') { ps->p++; break; }
        if (c == '\\') {
            ps->p++;
            if (ps->p >= ps->end) { set_error(ps, "unterminated escape"); free(buf); return NULL; }
            char e = *ps->p++;
            char lit = 0;
            switch (e) {
                case '"': lit = '"'; break;
                case '\\': lit = '\\'; break;
                case '/': lit = '/'; break;
                case 'b': lit = '\b'; break;
                case 'f': lit = '\f'; break;
                case 'n': lit = '\n'; break;
                case 'r': lit = '\r'; break;
                case 't': lit = '\t'; break;
                case 'u': {
                    unsigned int cp = hex4(ps);
                    if (ps->failed) { free(buf); return NULL; }
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        /* surrogate pair */
                        if (ps->p + 1 < ps->end && ps->p[0] == '\\' && ps->p[1] == 'u') {
                            ps->p += 2;
                            unsigned int lo = hex4(ps);
                            if (ps->failed) { free(buf); return NULL; }
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            }
                        }
                    }
                    append_utf8(&buf, &len, &cap, cp);
                    continue;
                }
                default:
                    set_error(ps, "invalid escape character");
                    free(buf);
                    return NULL;
            }
            if (len + 2 > cap) { cap *= 2; buf = (char *)realloc(buf, cap); }
            buf[len++] = lit;
            buf[len] = '\0';
        } else {
            if (len + 2 > cap) { cap *= 2; buf = (char *)realloc(buf, cap); }
            buf[len++] = (char)c;
            buf[len] = '\0';
            ps->p++;
        }
    }
    return buf;
}

static JsonValue *parse_string(Parser *ps) {
    char *s = parse_string_raw(ps);
    if (!s) return NULL;
    JsonValue *v = new_value(JSON_STRING);
    v->as.string = s;
    return v;
}

static JsonValue *parse_number(Parser *ps) {
    const char *start = ps->p;
    if (ps->p < ps->end && *ps->p == '-') ps->p++;
    while (ps->p < ps->end && isdigit((unsigned char)*ps->p)) ps->p++;
    if (ps->p < ps->end && *ps->p == '.') {
        ps->p++;
        while (ps->p < ps->end && isdigit((unsigned char)*ps->p)) ps->p++;
    }
    if (ps->p < ps->end && (*ps->p == 'e' || *ps->p == 'E')) {
        ps->p++;
        if (ps->p < ps->end && (*ps->p == '+' || *ps->p == '-')) ps->p++;
        while (ps->p < ps->end && isdigit((unsigned char)*ps->p)) ps->p++;
    }
    if (ps->p == start) { set_error(ps, "invalid number"); return NULL; }
    char tmp[64];
    size_t n = (size_t)(ps->p - start);
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    memcpy(tmp, start, n);
    tmp[n] = '\0';
    JsonValue *v = new_value(JSON_NUMBER);
    v->as.number = atof(tmp);
    return v;
}

static int match_literal(Parser *ps, const char *lit) {
    size_t n = strlen(lit);
    if ((size_t)(ps->end - ps->p) < n) return 0;
    if (strncmp(ps->p, lit, n) != 0) return 0;
    ps->p += n;
    return 1;
}

static JsonValue *parse_array(Parser *ps) {
    ps->p++; /* skip [ */
    JsonValue *v = new_value(JSON_ARRAY);
    size_t cap = 4;
    v->as.array.items = (JsonValue **)malloc(cap * sizeof(JsonValue *));
    v->as.array.count = 0;

    skip_ws(ps);
    if (ps->p < ps->end && *ps->p == ']') { ps->p++; return v; }

    while (1) {
        skip_ws(ps);
        JsonValue *item = parse_value(ps);
        if (ps->failed) { json_free(v); return NULL; }
        if ((size_t)v->as.array.count >= cap) {
            cap *= 2;
            v->as.array.items = (JsonValue **)realloc(v->as.array.items, cap * sizeof(JsonValue *));
        }
        v->as.array.items[v->as.array.count++] = item;
        skip_ws(ps);
        if (ps->p < ps->end && *ps->p == ',') { ps->p++; continue; }
        if (ps->p < ps->end && *ps->p == ']') { ps->p++; break; }
        set_error(ps, "expected ',' or ']' in array");
        json_free(v);
        return NULL;
    }
    return v;
}

static JsonValue *parse_object(Parser *ps) {
    ps->p++; /* skip { */
    JsonValue *v = new_value(JSON_OBJECT);
    size_t cap = 4;
    v->as.object.keys = (char **)malloc(cap * sizeof(char *));
    v->as.object.values = (JsonValue **)malloc(cap * sizeof(JsonValue *));
    v->as.object.count = 0;

    skip_ws(ps);
    if (ps->p < ps->end && *ps->p == '}') { ps->p++; return v; }

    while (1) {
        skip_ws(ps);
        char *key = parse_string_raw(ps);
        if (ps->failed) { json_free(v); return NULL; }
        skip_ws(ps);
        if (ps->p >= ps->end || *ps->p != ':') {
            set_error(ps, "expected ':' after key");
            free(key);
            json_free(v);
            return NULL;
        }
        ps->p++;
        skip_ws(ps);
        JsonValue *val = parse_value(ps);
        if (ps->failed) { free(key); json_free(v); return NULL; }

        if ((size_t)v->as.object.count >= cap) {
            cap *= 2;
            v->as.object.keys = (char **)realloc(v->as.object.keys, cap * sizeof(char *));
            v->as.object.values = (JsonValue **)realloc(v->as.object.values, cap * sizeof(JsonValue *));
        }
        v->as.object.keys[v->as.object.count] = key;
        v->as.object.values[v->as.object.count] = val;
        v->as.object.count++;

        skip_ws(ps);
        if (ps->p < ps->end && *ps->p == ',') { ps->p++; continue; }
        if (ps->p < ps->end && *ps->p == '}') { ps->p++; break; }
        set_error(ps, "expected ',' or '}' in object");
        json_free(v);
        return NULL;
    }
    return v;
}

static JsonValue *parse_value(Parser *ps) {
    skip_ws(ps);
    if (ps->p >= ps->end) { set_error(ps, "unexpected end of input"); return NULL; }
    char c = *ps->p;
    if (c == '{') return parse_object(ps);
    if (c == '[') return parse_array(ps);
    if (c == '"') return parse_string(ps);
    if (c == '-' || isdigit((unsigned char)c)) return parse_number(ps);
    if (match_literal(ps, "true")) { JsonValue *v = new_value(JSON_BOOL); v->as.boolean = 1; return v; }
    if (match_literal(ps, "false")) { JsonValue *v = new_value(JSON_BOOL); v->as.boolean = 0; return v; }
    if (match_literal(ps, "null")) { return new_value(JSON_NULL); }
    set_error(ps, "unexpected character");
    return NULL;
}

JsonValue *json_parse(const char *text, char *errbuf, size_t errbuf_size) {
    Parser ps;
    ps.start = text;
    ps.p = text;
    ps.end = text + strlen(text);
    ps.err = errbuf;
    ps.errsz = errbuf_size;
    ps.failed = 0;
    if (errbuf && errbuf_size) errbuf[0] = '\0';

    JsonValue *v = parse_value(&ps);
    if (ps.failed) {
        if (v) json_free(v);
        return NULL;
    }
    skip_ws(&ps);
    if (ps.p != ps.end) {
        set_error(&ps, "trailing data after JSON value");
        json_free(v);
        return NULL;
    }
    return v;
}

void json_free(JsonValue *v) {
    if (!v) return;
    switch (v->type) {
        case JSON_STRING:
            free(v->as.string);
            break;
        case JSON_ARRAY:
            for (int i = 0; i < v->as.array.count; i++) json_free(v->as.array.items[i]);
            free(v->as.array.items);
            break;
        case JSON_OBJECT:
            for (int i = 0; i < v->as.object.count; i++) {
                free(v->as.object.keys[i]);
                json_free(v->as.object.values[i]);
            }
            free(v->as.object.keys);
            free(v->as.object.values);
            break;
        default:
            break;
    }
    free(v);
}

JsonValue *json_object_get(const JsonValue *v, const char *key) {
    if (!v || v->type != JSON_OBJECT) return NULL;
    for (int i = 0; i < v->as.object.count; i++) {
        if (strcmp(v->as.object.keys[i], key) == 0) return v->as.object.values[i];
    }
    return NULL;
}

int json_array_count(const JsonValue *v) {
    if (!v || v->type != JSON_ARRAY) return 0;
    return v->as.array.count;
}

JsonValue *json_array_get(const JsonValue *v, int index) {
    if (!v || v->type != JSON_ARRAY) return NULL;
    if (index < 0 || index >= v->as.array.count) return NULL;
    return v->as.array.items[index];
}

double json_as_number(const JsonValue *v, double def) {
    if (!v || v->type != JSON_NUMBER) return def;
    return v->as.number;
}

const char *json_as_string(const JsonValue *v, const char *def) {
    if (!v || v->type != JSON_STRING) return def;
    return v->as.string;
}
