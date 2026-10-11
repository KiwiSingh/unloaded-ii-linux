#ifndef BEPIS_MVGL_ASSETS_H
#define BEPIS_MVGL_ASSETS_H
#include <stddef.h>
#define MVGL_PATH_MAX 1024
#define MVGL_MAX_ASSETS 4096
#define MVGL_MAX_BYTES (256u * 1024u * 1024u)
typedef struct { char path[MVGL_PATH_MAX]; unsigned char *bytes; size_t size; } MvglAsset;
typedef struct { MvglAsset *items; size_t count, total; } MvglAssets;
int mvgl_normalize(const char *input, char output[MVGL_PATH_MAX]);
int mvgl_add(MvglAssets *store, const char *path, const void *bytes, size_t size);
const MvglAsset *mvgl_find(const MvglAssets *store, const char *path);
int mvgl_read(const MvglAssets *store, const char *path, void *buffer, size_t capacity);
void mvgl_free(MvglAssets *store);
size_t mvgl_match_count(const unsigned char *data, size_t length, const unsigned char *pattern, const char *mask, size_t pattern_size, size_t *offset);
size_t mvgl_unique_match(const unsigned char *data, size_t length, const unsigned char *pattern, const char *mask, size_t pattern_size);
#endif
