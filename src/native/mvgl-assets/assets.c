#include "assets.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int mvgl_normalize(const char *input, char output[MVGL_PATH_MAX]) {
    if (!input || !*input) return 0;
    size_t n = 0, component = 0;
    for (; input[n]; n++) {
        unsigned char c = (unsigned char)input[n];
        if (n >= MVGL_PATH_MAX - 1 || c < 32 || c >= 127 || c == ':' || c == '*' || c == '?' || c == '"') return 0;
        if (c == '/' || c == '\\') {
            if (!component || (component == 1 && output[n-1] == '.') ||
                (component == 2 && output[n-1] == '.' && output[n-2] == '.')) return 0;
            output[n] = '/'; component = 0;
        } else {
            output[n] = c >= 'A' && c <= 'Z' ? (char)(c + 32) : (char)c;
            component++;
        }
    }
    if (!component || (component == 1 && output[n-1] == '.') ||
        (component == 2 && output[n-1] == '.' && output[n-2] == '.')) return 0;
    output[n] = 0; return 1;
}

int mvgl_add(MvglAssets *store, const char *path, const void *bytes, size_t size) {
    char normalized[MVGL_PATH_MAX];
    if (!store || !mvgl_normalize(path, normalized) || !bytes || !size || size > MVGL_MAX_FILE_BYTES ||
        store->count >= MVGL_MAX_ASSETS || store->total > MVGL_MAX_BYTES || size > MVGL_MAX_BYTES - store->total) return 0;
    for (size_t i = 0; i < store->count; i++) if (!strcmp(store->items[i].path, normalized)) return 0;
    unsigned char *copy = malloc(size);
    if (!copy) return 0;
    MvglAsset *items = realloc(store->items, (store->count + 1) * sizeof(*items));
    if (!items) { free(copy); return 0; }
    store->items = items;
    MvglAsset *asset = &items[store->count++];
    memcpy(asset->path, normalized, strlen(normalized) + 1);
    memcpy(copy, bytes, size); asset->bytes = copy; asset->size = size; store->total += size;
    return 1;
}

const MvglAsset *mvgl_find(const MvglAssets *store, const char *path) {
    char normalized[MVGL_PATH_MAX];
    if (!store || !mvgl_normalize(path, normalized)) return NULL;
    for (size_t i = 0; i < store->count; i++) if (!strcmp(store->items[i].path, normalized)) return &store->items[i];
    size_t n = strlen(normalized);
    if (n >= 4 && !strcmp(normalized + n - 4, ".img")) {
        memcpy(normalized + n - 4, ".dds", 5);
        for (size_t i = 0; i < store->count; i++) if (!strcmp(store->items[i].path, normalized)) return &store->items[i];
    }
    return NULL;
}

int mvgl_read(const MvglAssets *store, const char *path, void *buffer, size_t capacity) {
    const MvglAsset *asset = mvgl_find(store, path);
    if (!asset || !buffer || capacity != asset->size) return 0;
    memcpy(buffer, asset->bytes, asset->size); return 1;
}

void mvgl_free(MvglAssets *store) {
    for (size_t i = 0; i < store->count; i++) free(store->items[i].bytes);
    free(store->items); memset(store, 0, sizeof(*store));
}

size_t mvgl_match_count(const unsigned char *data, size_t length, const unsigned char *pattern, const char *mask, size_t size, size_t *offset) {
    size_t count = 0; *offset = SIZE_MAX;
    if (!size || size > length || strlen(mask) != size) return 0;
    for (size_t i = 0; i <= length - size; i++) {
        size_t j = 0;
        for (; j < size; j++) if (mask[j] != '?' && data[i+j] != pattern[j]) break;
        if (j == size) { *offset = i; if (++count == 2) return count; }
    }
    return count;
}
size_t mvgl_unique_match(const unsigned char *data, size_t length, const unsigned char *pattern, const char *mask, size_t size) {
    size_t offset;
    return mvgl_match_count(data, length, pattern, mask, size, &offset) == 1 ? offset : SIZE_MAX;
}
