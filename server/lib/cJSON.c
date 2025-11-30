/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
  Simplified version for QuizNet
*/

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include "cJSON.h"

static void *(*cJSON_malloc)(size_t sz) = malloc;
static void (*cJSON_free)(void *ptr) = free;

static char* cJSON_strdup(const char* str) {
    size_t len = strlen(str) + 1;
    char* copy = (char*)cJSON_malloc(len);
    if (!copy) return NULL;
    memcpy(copy, str, len);
    return copy;
}

void cJSON_InitHooks(cJSON_Hooks* hooks) {
    if (!hooks) {
        cJSON_malloc = malloc;
        cJSON_free = free;
        return;
    }
    cJSON_malloc = hooks->malloc_fn ? hooks->malloc_fn : malloc;
    cJSON_free = hooks->free_fn ? hooks->free_fn : free;
}

static cJSON *cJSON_New_Item(void) {
    cJSON* node = (cJSON*)cJSON_malloc(sizeof(cJSON));
    if (node) memset(node, 0, sizeof(cJSON));
    return node;
}

void cJSON_Delete(cJSON *c) {
    cJSON *next;
    while (c) {
        next = c->next;
        if (!(c->type & cJSON_IsReference) && c->child) cJSON_Delete(c->child);
        if (!(c->type & cJSON_IsReference) && c->valuestring) cJSON_free(c->valuestring);
        if (!(c->type & cJSON_StringIsConst) && c->string) cJSON_free(c->string);
        cJSON_free(c);
        c = next;
    }
}

static const char *parse_number(cJSON *item, const char *num) {
    double n = 0, sign = 1, scale = 0;
    int subscale = 0, signsubscale = 1;
    
    if (*num == '-') { sign = -1; num++; }
    if (*num == '0') num++;
    if (*num >= '1' && *num <= '9') {
        do { n = (n * 10.0) + (*num++ - '0'); } while (*num >= '0' && *num <= '9');
    }
    if (*num == '.' && num[1] >= '0' && num[1] <= '9') {
        num++;
        do { n = (n * 10.0) + (*num++ - '0'); scale--; } while (*num >= '0' && *num <= '9');
    }
    if (*num == 'e' || *num == 'E') {
        num++;
        if (*num == '+') num++; else if (*num == '-') { signsubscale = -1; num++; }
        while (*num >= '0' && *num <= '9') subscale = (subscale * 10) + (*num++ - '0');
    }
    n = sign * n * pow(10.0, (scale + subscale * signsubscale));
    item->valuedouble = n;
    item->valueint = (int)n;
    item->type = cJSON_Number;
    return num;
}

static size_t pow2gt(size_t x) { --x; x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16; return x + 1; }

typedef struct { char *buffer; size_t length; size_t offset; int noalloc; } printbuffer;

static char* ensure(printbuffer *p, size_t needed) {
    char *newbuffer;
    size_t newsize;
    if (!p || !p->buffer) return NULL;
    if ((p->length > 0) && (p->offset + needed <= p->length)) return p->buffer + p->offset;
    if (p->noalloc) return NULL;
    newsize = pow2gt(p->offset + needed);
    newbuffer = (char*)cJSON_malloc(newsize);
    if (!newbuffer) return NULL;
    memcpy(newbuffer, p->buffer, p->offset + 1);
    cJSON_free(p->buffer);
    p->length = newsize;
    p->buffer = newbuffer;
    return newbuffer + p->offset;
}

static char *print_number(const cJSON *item, printbuffer *p) {
    char *str = NULL;
    double d = item->valuedouble;
    
    if (p) str = ensure(p, 64);
    else str = (char*)cJSON_malloc(64);
    if (!str) return NULL;
    
    if (d == (double)item->valueint) {
        sprintf(str, "%d", item->valueint);
    } else {
        sprintf(str, "%g", d);
    }
    if (p) p->offset += strlen(str);
    return str;
}

static unsigned parse_hex4(const char *str) {
    unsigned h = 0;
    for (int i = 0; i < 4; i++) {
        if (*str >= '0' && *str <= '9') h += (*str) - '0';
        else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A';
        else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a';
        else return 0;
        if (i < 3) h = h << 4;
        str++;
    }
    return h;
}

static const char *parse_string(cJSON *item, const char *str) {
    const char *ptr = str + 1;
    char *ptr2, *out;
    int len = 0;
    unsigned uc, uc2;
    
    if (*str != '\"') return NULL;
    while (*ptr != '\"' && *ptr && ++len) if (*ptr++ == '\\') ptr++;
    
    out = (char*)cJSON_malloc(len + 1);
    if (!out) return NULL;
    
    ptr = str + 1;
    ptr2 = out;
    while (*ptr != '\"' && *ptr) {
        if (*ptr != '\\') *ptr2++ = *ptr++;
        else {
            ptr++;
            switch (*ptr) {
                case 'b': *ptr2++ = '\b'; break;
                case 'f': *ptr2++ = '\f'; break;
                case 'n': *ptr2++ = '\n'; break;
                case 'r': *ptr2++ = '\r'; break;
                case 't': *ptr2++ = '\t'; break;
                case 'u':
                    uc = parse_hex4(ptr + 1);
                    ptr += 4;
                    if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0) break;
                    if (uc >= 0xD800 && uc <= 0xDBFF) {
                        if (ptr[1] != '\\' || ptr[2] != 'u') break;
                        uc2 = parse_hex4(ptr + 3);
                        ptr += 6;
                        if (uc2 < 0xDC00 || uc2 > 0xDFFF) break;
                        uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
                    }
                    if (uc < 0x80) *ptr2++ = uc;
                    else if (uc < 0x800) { *ptr2++ = 0xC0 | (uc >> 6); *ptr2++ = 0x80 | (uc & 0x3F); }
                    else if (uc < 0x10000) { *ptr2++ = 0xE0 | (uc >> 12); *ptr2++ = 0x80 | ((uc >> 6) & 0x3F); *ptr2++ = 0x80 | (uc & 0x3F); }
                    else { *ptr2++ = 0xF0 | (uc >> 18); *ptr2++ = 0x80 | ((uc >> 12) & 0x3F); *ptr2++ = 0x80 | ((uc >> 6) & 0x3F); *ptr2++ = 0x80 | (uc & 0x3F); }
                    break;
                default: *ptr2++ = *ptr; break;
            }
            ptr++;
        }
    }
    *ptr2 = 0;
    if (*ptr == '\"') ptr++;
    item->valuestring = out;
    item->type = cJSON_String;
    return ptr;
}

static char *print_string_ptr(const char *str, printbuffer *p) {
    const char *ptr;
    char *ptr2, *out;
    size_t len = 0, flag = 0;
    unsigned char token;
    
    if (!str) {
        if (p) out = ensure(p, 3);
        else out = (char*)cJSON_malloc(3);
        if (!out) return NULL;
        strcpy(out, "\"\"");
        return out;
    }
    
    for (ptr = str; *ptr; ptr++) {
        flag |= ((*ptr > 0 && *ptr < 32) || (*ptr == '\"') || (*ptr == '\\')) ? 1 : 0;
    }
    len = ptr - str;
    
    if (!flag) {
        if (p) out = ensure(p, len + 3);
        else out = (char*)cJSON_malloc(len + 3);
        if (!out) return NULL;
        ptr2 = out;
        *ptr2++ = '\"';
        strcpy(ptr2, str);
        ptr2[len] = '\"';
        ptr2[len + 1] = 0;
        return out;
    }
    
    if (p) out = ensure(p, len * 6 + 3);
    else out = (char*)cJSON_malloc(len * 6 + 3);
    if (!out) return NULL;
    
    ptr2 = out;
    ptr = str;
    *ptr2++ = '\"';
    while (*ptr) {
        if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\') *ptr2++ = *ptr++;
        else {
            *ptr2++ = '\\';
            switch (token = *ptr++) {
                case '\\': *ptr2++ = '\\'; break;
                case '\"': *ptr2++ = '\"'; break;
                case '\b': *ptr2++ = 'b'; break;
                case '\f': *ptr2++ = 'f'; break;
                case '\n': *ptr2++ = 'n'; break;
                case '\r': *ptr2++ = 'r'; break;
                case '\t': *ptr2++ = 't'; break;
                default: sprintf(ptr2, "u%04x", token); ptr2 += 5; break;
            }
        }
    }
    *ptr2++ = '\"';
    *ptr2 = 0;
    return out;
}

static char *print_string(const cJSON *item, printbuffer *p) {
    return print_string_ptr(item->valuestring, p);
}

static const char *skip(const char *in) {
    while (in && *in && (unsigned char)*in <= 32) in++;
    return in;
}

static const char *parse_value(cJSON *item, const char *value);
static char *print_value(const cJSON *item, int depth, int fmt, printbuffer *p);

static const char *parse_array(cJSON *item, const char *value) {
    cJSON *child;
    if (*value != '[') return NULL;
    item->type = cJSON_Array;
    value = skip(value + 1);
    if (*value == ']') return value + 1;
    
    item->child = child = cJSON_New_Item();
    if (!item->child) return NULL;
    value = skip(parse_value(child, skip(value)));
    if (!value) return NULL;
    
    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_value(child, skip(value + 1)));
        if (!value) return NULL;
    }
    
    if (*value == ']') return value + 1;
    return NULL;
}

static char *print_array(const cJSON *item, int depth, int fmt, printbuffer *p) {
    char **entries;
    char *out = NULL, *ptr, *ret;
    size_t len = 5;
    cJSON *child = item->child;
    int numentries = 0, i = 0, fail = 0;
    
    while (child) { numentries++; child = child->next; }
    if (!numentries) {
        if (p) out = ensure(p, 3);
        else out = (char*)cJSON_malloc(3);
        if (out) strcpy(out, "[]");
        return out;
    }
    
    if (p) {
        size_t needed = 2;
        ptr = ensure(p, needed + 1);
        if (!ptr) return NULL;
        *ptr++ = '[';
        p->offset++;
        child = item->child;
        while (child) {
            print_value(child, depth + 1, fmt, p);
            if (child->next) {
                len = fmt ? 2 : 1;
                ptr = ensure(p, len + 1);
                if (!ptr) return NULL;
                *ptr++ = ',';
                if (fmt) *ptr++ = ' ';
                *ptr = 0;
                p->offset += len;
            }
            child = child->next;
        }
        ptr = ensure(p, 2);
        if (!ptr) return NULL;
        *ptr++ = ']';
        *ptr = 0;
        out = p->buffer;
    } else {
        entries = (char**)cJSON_malloc(numentries * sizeof(char*));
        if (!entries) return NULL;
        memset(entries, 0, numentries * sizeof(char*));
        child = item->child;
        while (child && !fail) {
            ret = print_value(child, depth + 1, fmt, 0);
            entries[i++] = ret;
            if (ret) len += strlen(ret) + 2 + (fmt ? 1 : 0);
            else fail = 1;
            child = child->next;
        }
        if (!fail) out = (char*)cJSON_malloc(len);
        if (!out) fail = 1;
        if (fail) {
            for (i = 0; i < numentries; i++) if (entries[i]) cJSON_free(entries[i]);
            cJSON_free(entries);
            return NULL;
        }
        *out = '[';
        ptr = out + 1;
        *ptr = 0;
        for (i = 0; i < numentries; i++) {
            size_t tmplen = strlen(entries[i]);
            memcpy(ptr, entries[i], tmplen);
            ptr += tmplen;
            if (i != numentries - 1) { *ptr++ = ','; if (fmt) *ptr++ = ' '; *ptr = 0; }
            cJSON_free(entries[i]);
        }
        cJSON_free(entries);
        *ptr++ = ']';
        *ptr++ = 0;
    }
    return out;
}

static const char *parse_object(cJSON *item, const char *value) {
    cJSON *child;
    if (*value != '{') return NULL;
    item->type = cJSON_Object;
    value = skip(value + 1);
    if (*value == '}') return value + 1;
    
    item->child = child = cJSON_New_Item();
    if (!item->child) return NULL;
    value = skip(parse_string(child, skip(value)));
    if (!value) return NULL;
    child->string = child->valuestring;
    child->valuestring = NULL;
    if (*value != ':') return NULL;
    value = skip(parse_value(child, skip(value + 1)));
    if (!value) return NULL;
    
    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_string(child, skip(value + 1)));
        if (!value) return NULL;
        child->string = child->valuestring;
        child->valuestring = NULL;
        if (*value != ':') return NULL;
        value = skip(parse_value(child, skip(value + 1)));
        if (!value) return NULL;
    }
    
    if (*value == '}') return value + 1;
    return NULL;
}

static char *print_object(const cJSON *item, int depth, int fmt, printbuffer *p) {
    char **entries = NULL, **names = NULL;
    char *out = NULL, *ptr, *ret, *str;
    size_t len = 7;
    int i = 0, j, numentries = 0, fail = 0;
    cJSON *child = item->child;
    
    while (child) { numentries++; child = child->next; }
    if (!numentries) {
        if (p) out = ensure(p, fmt ? depth + 4 : 3);
        else out = (char*)cJSON_malloc(fmt ? depth + 4 : 3);
        if (!out) return NULL;
        ptr = out;
        *ptr++ = '{';
        if (fmt) { *ptr++ = '\n'; for (i = 0; i < depth; i++) *ptr++ = '\t'; }
        *ptr++ = '}';
        *ptr = 0;
        return out;
    }
    
    if (p) {
        ptr = ensure(p, fmt ? 2 : 1);
        if (!ptr) return NULL;
        *ptr++ = '{';
        if (fmt) *ptr++ = '\n';
        p->offset += fmt ? 2 : 1;
        child = item->child;
        depth++;
        while (child) {
            if (fmt) {
                ptr = ensure(p, depth);
                if (!ptr) return NULL;
                for (j = 0; j < depth; j++) *ptr++ = '\t';
                p->offset += depth;
            }
            print_string_ptr(child->string, p);
            p->offset += strlen(p->buffer + p->offset);
            ptr = ensure(p, fmt ? 2 : 1);
            if (!ptr) return NULL;
            *ptr++ = ':';
            if (fmt) *ptr++ = '\t';
            p->offset += fmt ? 2 : 1;
            print_value(child, depth, fmt, p);
            p->offset += strlen(p->buffer + p->offset);
            len = (fmt ? 1 : 0) + (child->next ? 1 : 0);
            ptr = ensure(p, len + 1);
            if (!ptr) return NULL;
            if (child->next) *ptr++ = ',';
            if (fmt) *ptr++ = '\n';
            *ptr = 0;
            p->offset += len;
            child = child->next;
        }
        ptr = ensure(p, fmt ? (depth + 1) : 2);
        if (!ptr) return NULL;
        if (fmt) for (i = 0; i < depth - 1; i++) *ptr++ = '\t';
        *ptr++ = '}';
        *ptr = 0;
        out = p->buffer;
    } else {
        entries = (char**)cJSON_malloc(numentries * sizeof(char*));
        if (!entries) return NULL;
        names = (char**)cJSON_malloc(numentries * sizeof(char*));
        if (!names) { cJSON_free(entries); return NULL; }
        memset(entries, 0, sizeof(char*) * numentries);
        memset(names, 0, sizeof(char*) * numentries);
        child = item->child;
        depth++;
        if (fmt) len += depth;
        while (child && !fail) {
            names[i] = str = print_string_ptr(child->string, 0);
            entries[i++] = ret = print_value(child, depth, fmt, 0);
            if (str && ret) len += strlen(ret) + strlen(str) + 2 + (fmt ? 2 + depth : 0);
            else fail = 1;
            child = child->next;
        }
        if (!fail) out = (char*)cJSON_malloc(len);
        if (!out) fail = 1;
        if (fail) {
            for (i = 0; i < numentries; i++) { if (names[i]) cJSON_free(names[i]); if (entries[i]) cJSON_free(entries[i]); }
            cJSON_free(names);
            cJSON_free(entries);
            return NULL;
        }
        *out = '{';
        ptr = out + 1;
        if (fmt) *ptr++ = '\n';
        *ptr = 0;
        for (i = 0; i < numentries; i++) {
            if (fmt) for (j = 0; j < depth; j++) *ptr++ = '\t';
            size_t tmplen = strlen(names[i]);
            memcpy(ptr, names[i], tmplen);
            ptr += tmplen;
            *ptr++ = ':';
            if (fmt) *ptr++ = '\t';
            tmplen = strlen(entries[i]);
            memcpy(ptr, entries[i], tmplen);
            ptr += tmplen;
            if (i != numentries - 1) *ptr++ = ',';
            if (fmt) *ptr++ = '\n';
            *ptr = 0;
            cJSON_free(names[i]);
            cJSON_free(entries[i]);
        }
        cJSON_free(names);
        cJSON_free(entries);
        if (fmt) for (i = 0; i < depth - 1; i++) *ptr++ = '\t';
        *ptr++ = '}';
        *ptr = 0;
    }
    return out;
}

static const char *parse_value(cJSON *item, const char *value) {
    if (!value) return NULL;
    if (!strncmp(value, "null", 4)) { item->type = cJSON_NULL; return value + 4; }
    if (!strncmp(value, "false", 5)) { item->type = cJSON_False; return value + 5; }
    if (!strncmp(value, "true", 4)) { item->type = cJSON_True; item->valueint = 1; return value + 4; }
    if (*value == '\"') return parse_string(item, value);
    if (*value == '-' || (*value >= '0' && *value <= '9')) return parse_number(item, value);
    if (*value == '[') return parse_array(item, value);
    if (*value == '{') return parse_object(item, value);
    return NULL;
}

static char *print_value(const cJSON *item, int depth, int fmt, printbuffer *p) {
    char *out = NULL;
    if (!item) return NULL;
    if (p) {
        switch ((item->type) & 0xFF) {
            case cJSON_NULL: out = ensure(p, 5); if (out) strcpy(out, "null"); break;
            case cJSON_False: out = ensure(p, 6); if (out) strcpy(out, "false"); break;
            case cJSON_True: out = ensure(p, 5); if (out) strcpy(out, "true"); break;
            case cJSON_Number: out = print_number(item, p); break;
            case cJSON_String: out = print_string(item, p); break;
            case cJSON_Array: out = print_array(item, depth, fmt, p); break;
            case cJSON_Object: out = print_object(item, depth, fmt, p); break;
        }
    } else {
        switch ((item->type) & 0xFF) {
            case cJSON_NULL: out = cJSON_strdup("null"); break;
            case cJSON_False: out = cJSON_strdup("false"); break;
            case cJSON_True: out = cJSON_strdup("true"); break;
            case cJSON_Number: out = print_number(item, 0); break;
            case cJSON_String: out = print_string(item, 0); break;
            case cJSON_Array: out = print_array(item, depth, fmt, 0); break;
            case cJSON_Object: out = print_object(item, depth, fmt, 0); break;
        }
    }
    return out;
}

cJSON *cJSON_Parse(const char *value) {
    cJSON *c = cJSON_New_Item();
    if (!c) return NULL;
    if (!parse_value(c, skip(value))) { cJSON_Delete(c); return NULL; }
    return c;
}

char *cJSON_Print(const cJSON *item) { return print_value(item, 0, 1, 0); }
char *cJSON_PrintUnformatted(const cJSON *item) { return print_value(item, 0, 0, 0); }

int cJSON_GetArraySize(const cJSON *array) {
    cJSON *c = array ? array->child : NULL;
    int i = 0;
    while (c) { i++; c = c->next; }
    return i;
}

cJSON *cJSON_GetArrayItem(const cJSON *array, int index) {
    cJSON *c = array ? array->child : NULL;
    while (c && index > 0) { index--; c = c->next; }
    return c;
}

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string) {
    cJSON *c = object ? object->child : NULL;
    while (c && strcasecmp(c->string, string)) c = c->next;
    return c;
}

cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object, const char *string) {
    cJSON *c = object ? object->child : NULL;
    while (c && strcmp(c->string, string)) c = c->next;
    return c;
}

int cJSON_HasObjectItem(const cJSON *object, const char *string) {
    return cJSON_GetObjectItem(object, string) ? 1 : 0;
}

char *cJSON_GetStringValue(cJSON *item) {
    if (!cJSON_IsString(item)) return NULL;
    return item->valuestring;
}

int cJSON_IsInvalid(const cJSON * const item) { return (item == NULL) || ((item->type & 0xFF) == cJSON_Invalid); }
int cJSON_IsFalse(const cJSON * const item) { return (item != NULL) && ((item->type & 0xFF) == cJSON_False); }
int cJSON_IsTrue(const cJSON * const item) { return (item != NULL) && ((item->type & 0xFF) == cJSON_True); }
int cJSON_IsBool(const cJSON * const item) { return (item != NULL) && (((item->type & 0xFF) == cJSON_True) || ((item->type & 0xFF) == cJSON_False)); }
int cJSON_IsNull(const cJSON * const item) { return (item != NULL) && ((item->type & 0xFF) == cJSON_NULL); }
int cJSON_IsNumber(const cJSON * const item) { return (item != NULL) && ((item->type & 0xFF) == cJSON_Number); }
int cJSON_IsString(const cJSON * const item) { return (item != NULL) && ((item->type & 0xFF) == cJSON_String); }
int cJSON_IsArray(const cJSON * const item) { return (item != NULL) && ((item->type & 0xFF) == cJSON_Array); }
int cJSON_IsObject(const cJSON * const item) { return (item != NULL) && ((item->type & 0xFF) == cJSON_Object); }
int cJSON_IsRaw(const cJSON * const item) { return (item != NULL) && ((item->type & 0xFF) == cJSON_Raw); }

cJSON *cJSON_CreateNull(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_NULL; return item; }
cJSON *cJSON_CreateTrue(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_True; return item; }
cJSON *cJSON_CreateFalse(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_False; return item; }
cJSON *cJSON_CreateBool(int b) { cJSON *item = cJSON_New_Item(); if (item) item->type = b ? cJSON_True : cJSON_False; return item; }
cJSON *cJSON_CreateNumber(double num) {
    cJSON *item = cJSON_New_Item();
    if (item) { item->type = cJSON_Number; item->valuedouble = num; item->valueint = (int)num; }
    return item;
}
cJSON *cJSON_CreateString(const char *string) {
    cJSON *item = cJSON_New_Item();
    if (item) { item->type = cJSON_String; item->valuestring = cJSON_strdup(string); if (!item->valuestring) { cJSON_Delete(item); return NULL; } }
    return item;
}
cJSON *cJSON_CreateArray(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_Array; return item; }
cJSON *cJSON_CreateObject(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_Object; return item; }

static void suffix_object(cJSON *prev, cJSON *item) { prev->next = item; item->prev = prev; }

void cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    cJSON *c;
    if (!item || !array) return;
    c = array->child;
    if (!c) { array->child = item; } else { while (c->next) c = c->next; suffix_object(c, item); }
}

void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    if (!item) return;
    if (item->string) cJSON_free(item->string);
    item->string = cJSON_strdup(string);
    cJSON_AddItemToArray(object, item);
}

cJSON *cJSON_AddNullToObject(cJSON * const object, const char * const name) {
    cJSON *null_item = cJSON_CreateNull();
    if (null_item) cJSON_AddItemToObject(object, name, null_item);
    return null_item;
}
cJSON *cJSON_AddTrueToObject(cJSON * const object, const char * const name) {
    cJSON *true_item = cJSON_CreateTrue();
    if (true_item) cJSON_AddItemToObject(object, name, true_item);
    return true_item;
}
cJSON *cJSON_AddFalseToObject(cJSON * const object, const char * const name) {
    cJSON *false_item = cJSON_CreateFalse();
    if (false_item) cJSON_AddItemToObject(object, name, false_item);
    return false_item;
}
cJSON *cJSON_AddBoolToObject(cJSON * const object, const char * const name, const int boolean) {
    cJSON *bool_item = cJSON_CreateBool(boolean);
    if (bool_item) cJSON_AddItemToObject(object, name, bool_item);
    return bool_item;
}
cJSON *cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number) {
    cJSON *number_item = cJSON_CreateNumber(number);
    if (number_item) cJSON_AddItemToObject(object, name, number_item);
    return number_item;
}
cJSON *cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string) {
    cJSON *string_item = cJSON_CreateString(string);
    if (string_item) cJSON_AddItemToObject(object, name, string_item);
    return string_item;
}
cJSON *cJSON_AddObjectToObject(cJSON * const object, const char * const name) {
    cJSON *object_item = cJSON_CreateObject();
    if (object_item) cJSON_AddItemToObject(object, name, object_item);
    return object_item;
}
cJSON *cJSON_AddArrayToObject(cJSON * const object, const char * const name) {
    cJSON *array_item = cJSON_CreateArray();
    if (array_item) cJSON_AddItemToObject(object, name, array_item);
    return array_item;
}

cJSON *cJSON_CreateIntArray(const int *numbers, int count) {
    cJSON *n = NULL, *p = NULL, *a = cJSON_CreateArray();
    for (int i = 0; a && i < count; i++) {
        n = cJSON_CreateNumber(numbers[i]);
        if (!n) { cJSON_Delete(a); return NULL; }
        if (!i) a->child = n; else suffix_object(p, n);
        p = n;
    }
    return a;
}

double cJSON_SetNumberHelper(cJSON *object, double number) {
    if (object) { object->valueint = (int)number; object->valuedouble = number; }
    return number;
}

void cJSON_Minify(char *json) {
    char *into = json;
    while (*json) {
        if (*json == ' ' || *json == '\t' || *json == '\r' || *json == '\n') json++;
        else if (*json == '/' && json[1] == '/') while (*json && *json != '\n') json++;
        else if (*json == '/' && json[1] == '*') { while (*json && !(*json == '*' && json[1] == '/')) json++; json += 2; }
        else if (*json == '\"') { *into++ = *json++; while (*json && *json != '\"') { if (*json == '\\') *into++ = *json++; *into++ = *json++; } *into++ = *json++; }
        else *into++ = *json++;
    }
    *into = 0;
}
